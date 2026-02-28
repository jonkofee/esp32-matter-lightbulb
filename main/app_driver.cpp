/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include <stdlib.h>
#include <string.h>

#include <esp_matter.h>
#include <app_priv.h>

#include <device.h>
#include <led_driver.h>
#include <button_gpio.h>
#include <driver/gpio.h>

using namespace chip::app::Clusters;
using namespace esp_matter;
using namespace esp_matter::attribute;

static const char *TAG = "app_driver";

/* Глобальный endpoint лампы объявлен в app_main.cpp */
extern uint16_t light_endpoint_id;

/* GPIO для нагрузки (реле / лампа) */
static const gpio_num_t LIGHT_GPIO = GPIO_NUM_25;

/* GPIO для кнопки (выключатель). Каждый раз когда меняется состояние этого ввода - нужно переключить состояние лампы */
static const gpio_num_t BUTTON_GPIO = GPIO_NUM_25;

/* Очередь событий от ISR выключателя */
static QueueHandle_t s_switch_evt_queue = nullptr;

/* Простая защита от дребезга */
static TickType_t s_last_switch_tick = 0;
static const TickType_t SWITCH_DEBOUNCE_TICKS = pdMS_TO_TICKS(500);

/* Вперёд объявим вспомогательные функции */
static void app_driver_toggle_light_from_switch();
static void IRAM_ATTR wall_switch_isr(void *arg);
static void wall_switch_task(void *arg);

/* Низкоуровневое управление питанием лампы по GPIO */
static esp_err_t app_driver_light_set_power(bool on)
{
    int level = on ? 1 : 0;
    esp_err_t err = gpio_set_level(LIGHT_GPIO, level);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LIGHT_GPIO=%d level=%d err=%d",
                 (int)LIGHT_GPIO, level, (int)err);
    }
    return err;
}

/* Вызывается из app_attribute_update_cb() при изменении атрибута */
esp_err_t app_driver_attribute_update(app_driver_handle_t /*driver_handle*/,
                                      uint16_t endpoint_id,
                                      uint32_t cluster_id,
                                      uint32_t attribute_id,
                                      esp_matter_attr_val_t *val)
{
    if (endpoint_id != light_endpoint_id) {
        return ESP_OK;
    }

    if (cluster_id == OnOff::Id &&
        attribute_id == OnOff::Attributes::OnOff::Id) {
        bool on = val->val.b; // OnOff — булевый атрибут
        return app_driver_light_set_power(on);
    }

    return ESP_OK;
}

/* Применение текущего значения OnOff к железу при старте */
esp_err_t app_driver_light_set_defaults(uint16_t endpoint_id)
{
    esp_matter_attr_val_t val = esp_matter_invalid(nullptr);

    attribute_t *attribute = attribute::get(endpoint_id,
                                            OnOff::Id,
                                            OnOff::Attributes::OnOff::Id);
    if (!attribute) {
        ESP_LOGE(TAG, "OnOff attribute not found for endpoint %u", endpoint_id);
        return ESP_FAIL;
    }

    esp_err_t err = attribute::get_val(attribute, &val);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get OnOff attribute value, err=%d", (int)err);
        return err;
    }

    bool on = val.val.b;
    return app_driver_light_set_power(on);
}

app_driver_handle_t app_driver_light_init()
{
    gpio_reset_pin(GPIO_NUM_25);
    gpio_set_direction(GPIO_NUM_25, GPIO_MODE_OUTPUT);

    return (app_driver_handle_t)NULL;
}

static void app_driver_button_toggle_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Toggle button pressed");
    uint16_t endpoint_id = light_endpoint_id;
    uint32_t cluster_id = OnOff::Id;
    uint32_t attribute_id = OnOff::Attributes::OnOff::Id;

    attribute_t *attribute = attribute::get(endpoint_id, cluster_id, attribute_id);

    esp_matter_attr_val_t val = esp_matter_invalid(NULL);
    attribute::get_val(attribute, &val);
    val.val.b = !val.val.b;
    attribute::update(endpoint_id, cluster_id, attribute_id, &val);
}

app_driver_handle_t app_driver_button_init()
{
    /* Initialize button */
    button_handle_t handle = NULL;
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = BUTTON_GPIO,
        .active_level = 0,
        .disable_pull = false,
    };

    if (iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create button device");
        return NULL;
    }

    iot_button_register_cb(handle, BUTTON_PRESS_UP, NULL, app_driver_button_toggle_cb, NULL);
    iot_button_register_cb(handle, BUTTON_PRESS_DOWN, NULL, app_driver_button_toggle_cb, NULL);
    return (app_driver_handle_t)handle;
}
