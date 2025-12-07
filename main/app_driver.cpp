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

extern uint16_t light1_endpoint_id;
extern uint16_t light2_endpoint_id;

static const gpio_num_t LIGHT1_GPIO = GPIO_NUM_17;
static const gpio_num_t LIGHT2_GPIO = GPIO_NUM_18;

static const gpio_num_t BUTTON1_GPIO = GPIO_NUM_26;
static const gpio_num_t BUTTON2_GPIO = GPIO_NUM_25;

/* Низкоуровневое управление питанием лампы по GPIO */
static esp_err_t app_driver_light1_set_power(bool on)
{
    int level = on ? 1 : 0;
    esp_err_t err = gpio_set_level(LIGHT1_GPIO, level);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LIGHT1_GPIO=%d level=%d err=%d",
                 (int)LIGHT1_GPIO, level, (int)err);
    }
    return err;
}

static esp_err_t app_driver_light2_set_power(bool on)
{
    int level = on ? 1 : 0;
    esp_err_t err = gpio_set_level(LIGHT2_GPIO, level);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LIGHT2_GPIO=%d level=%d err=%d",
                 (int)LIGHT2_GPIO, level, (int)err);
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
    if (cluster_id == OnOff::Id &&
        attribute_id == OnOff::Attributes::OnOff::Id) {
        bool on = val->val.b; // OnOff — булевый атрибут

		if (endpoint_id == light1_endpoint_id) {
			return app_driver_light1_set_power(on);
		} else if (endpoint_id == light2_endpoint_id) {
			return app_driver_light2_set_power(on);
		} else {
			return ESP_OK;
		}
    }

    return ESP_OK;
}

/* Применение текущего значения OnOff к железу при старте */
esp_err_t app_driver_light1_set_defaults(uint16_t endpoint_id)
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
    return app_driver_light1_set_power(on);
}

esp_err_t app_driver_light2_set_defaults(uint16_t endpoint_id)
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
    return app_driver_light2_set_power(on);
}

app_driver_handle_t app_driver_light1_init()
{
    gpio_reset_pin(LIGHT1_GPIO);
    gpio_set_direction(LIGHT1_GPIO, GPIO_MODE_OUTPUT);

    return (app_driver_handle_t)NULL;
}

app_driver_handle_t app_driver_light2_init()
{
    gpio_reset_pin(LIGHT2_GPIO);
    gpio_set_direction(LIGHT2_GPIO, GPIO_MODE_OUTPUT);

    return (app_driver_handle_t)NULL;
}

static void app_driver_button1_toggle_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Toggle button1 pressed");
    uint16_t endpoint_id = light1_endpoint_id;
    uint32_t cluster_id = OnOff::Id;
    uint32_t attribute_id = OnOff::Attributes::OnOff::Id;

    attribute_t *attribute = attribute::get(endpoint_id, cluster_id, attribute_id);

    esp_matter_attr_val_t val = esp_matter_invalid(NULL);
    attribute::get_val(attribute, &val);
    val.val.b = !val.val.b;
    attribute::update(endpoint_id, cluster_id, attribute_id, &val);
}

static void app_driver_button2_toggle_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Toggle button2 pressed");
    uint16_t endpoint_id = light2_endpoint_id;
    uint32_t cluster_id = OnOff::Id;
    uint32_t attribute_id = OnOff::Attributes::OnOff::Id;

    attribute_t *attribute = attribute::get(endpoint_id, cluster_id, attribute_id);

    esp_matter_attr_val_t val = esp_matter_invalid(NULL);
    attribute::get_val(attribute, &val);
    val.val.b = !val.val.b;
    attribute::update(endpoint_id, cluster_id, attribute_id, &val);
}

app_driver_handle_t app_driver_button1_init()
{
    /* Initialize button */
    button_handle_t handle = NULL;
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = BUTTON1_GPIO,
        .active_level = 0,
        .disable_pull = false,
    };

    if (iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create button device");
        return NULL;
    }

    iot_button_register_cb(handle, BUTTON_PRESS_UP, NULL, app_driver_button1_toggle_cb, NULL);
    iot_button_register_cb(handle, BUTTON_PRESS_DOWN, NULL, app_driver_button1_toggle_cb, NULL);
    return (app_driver_handle_t)handle;
}

app_driver_handle_t app_driver_button2_init()
{
    /* Initialize button */
    button_handle_t handle = NULL;
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = BUTTON2_GPIO,
        .active_level = 0,
        .disable_pull = false,
    };

    if (iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create button device");
        return NULL;
    }

    iot_button_register_cb(handle, BUTTON_PRESS_UP, NULL, app_driver_button2_toggle_cb, NULL);
    iot_button_register_cb(handle, BUTTON_PRESS_DOWN, NULL, app_driver_button2_toggle_cb, NULL);
    return (app_driver_handle_t)handle;
}
