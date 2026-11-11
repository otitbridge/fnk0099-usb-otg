// P1: host-only startup probe. Leave the OTG connector empty.
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_intr_alloc.h"
#include "usb/usb_host.h"
#include "usb/hid_host.h"

static const char *TAG = "P1";

static void hid_event(hid_host_device_handle_t device,
                      hid_host_driver_event_t event, void *arg)
{
    (void)arg;
    hid_host_dev_params_t params;
    esp_err_t err = hid_host_device_get_params(device, &params);
    if (err == ESP_OK) {
        ESP_LOGW(TAG, "Unexpected device: event=%d address=%u interface=%u protocol=%u; P1 does not start input",
                 event, params.addr, params.iface_num, params.proto);
    } else {
        ESP_LOGE(TAG, "get_params: %s", esp_err_to_name(err));
    }
}

static void host_events(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t flags = 0;
        ESP_ERROR_CHECK(usb_host_lib_handle_events(portMAX_DELAY, &flags));
        if (flags) ESP_LOGI(TAG, "USB library flags=0x%" PRIx32, flags);
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            esp_err_t err = usb_host_device_free_all();
            ESP_LOGI(TAG, "free_all: %s", esp_err_to_name(err));
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "FNK0099 Rev1.1 N16R8 (user-confirmed), IDF %s", esp_get_idf_version());
    ESP_LOGI(TAG, "P1 host-only; UART=115200; keep OTG disconnected; PSRAM unused");
    const usb_host_config_t host = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host));
    ESP_LOGI(TAG, "usb_host_install: ESP_OK");
    BaseType_t created = xTaskCreatePinnedToCore(host_events, "usb_events", 4096,
                                                NULL, 2, NULL, 0);
    ESP_ERROR_CHECK(created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    const hid_host_driver_config_t hid = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = hid_event,
        .callback_arg = NULL,
    };
    ESP_ERROR_CHECK(hid_host_install(&hid));
    ESP_LOGI(TAG, "hid_host_install: ESP_OK; HOST_READY");
    for (;;) {
        ESP_LOGI(TAG, "alive uptime_ms=%" PRId64 " free_heap=%" PRIu32,
                 esp_timer_get_time() / 1000, esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
