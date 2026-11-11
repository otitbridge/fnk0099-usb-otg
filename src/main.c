// USB Host probe: descriptor observation and UIAPduino boot-keyboard input.
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_intr_alloc.h"
#include "usb/usb_host.h"
#include "usb/hid_host.h"
#include "sfc_report.h"

static const char *TAG = "HOST";
static usb_host_client_handle_t observer;
static QueueHandle_t events;
static hid_host_device_handle_t keyboard;
static uint16_t mask;
typedef struct {
    hid_host_device_handle_t device;
    int kind; // 0=connect, 1=report, 2=disconnect, 3=transfer/read error
    size_t length;
    uint8_t data[64];
} input_event_t;

static void set_mask(uint16_t next, const char *reason)
{
    ESP_LOGI(TAG, "SFC mask=0x%03x changed=0x%03x reason=%s", next, mask ^ next, reason);
    mask = next;
}

static void enqueue(const input_event_t *e)
{
    // Losing a release is worse than resetting this diagnostic host.
    ESP_ERROR_CHECK(xQueueSend(events, e, 0) == pdTRUE ? ESP_OK : ESP_ERR_NO_MEM);
}

static void interface_event(hid_host_device_handle_t dev, hid_host_interface_event_t event, void *arg)
{
    (void)arg;
    input_event_t e = {.device = dev};
    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
        e.kind = 1;
        if (hid_host_device_get_raw_input_report_data(dev, e.data, sizeof(e.data), &e.length) != ESP_OK) e.kind = 3;
        break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        // Driver 1.0.1 expects interface removal before this callback returns.
        ESP_LOGI(TAG, "HID disconnect close: %s", esp_err_to_name(hid_host_device_close(dev)));
        e.kind = 2;
        break;
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR: e.kind = 3; break;
    default: return;
    }
    enqueue(&e);
}

static void hid_event(hid_host_device_handle_t dev, hid_host_driver_event_t event, void *arg)
{
    (void)arg;
    if (event == HID_HOST_DRIVER_EVENT_CONNECTED) {
        input_event_t e = {.device = dev, .kind = 0};
        enqueue(&e);
    }
}

static void inspect_device(const usb_host_client_event_msg_t *event, void *arg)
{
    (void)arg;
    if (event->event != USB_HOST_CLIENT_EVENT_NEW_DEV) return;
    usb_device_handle_t dev;
    esp_err_t err = usb_host_device_open(observer, event->new_dev.address, &dev);
    if (err != ESP_OK) { ESP_LOGE(TAG, "descriptor open: %s", esp_err_to_name(err)); return; }
    const usb_device_desc_t *desc;
    const usb_config_desc_t *config;
    usb_device_info_t info;
    if (usb_host_get_device_descriptor(dev, &desc) == ESP_OK) {
        ESP_LOGI(TAG, "DEVICE address=%u VID:PID=%04x:%04x", event->new_dev.address, desc->idVendor, desc->idProduct);
        ESP_LOG_BUFFER_HEX_LEVEL("DEVICE_DESC", desc, desc->bLength, ESP_LOG_INFO);
    }
    if (usb_host_device_info(dev, &info) == ESP_OK)
        ESP_LOGI(TAG, "speed=%s", info.speed == USB_SPEED_LOW ? "LOW (1.5Mbps)" : "non-low");
    if (usb_host_get_active_config_descriptor(dev, &config) == ESP_OK) {
        const uint8_t *raw = (const uint8_t *)config;
        ESP_LOG_BUFFER_HEX_LEVEL("CONFIG_DESC", raw, config->wTotalLength, ESP_LOG_INFO);
        int iface = -1;
        for (size_t pos = 0; pos + 2 <= config->wTotalLength;) {
            uint8_t n = raw[pos];
            if (n < 2 || pos + n > config->wTotalLength) break;
            const uint8_t *d = raw + pos;
            if (d[1] == 4 && n >= 9) {
                iface = d[2];
                ESP_LOGI(TAG, "INTERFACE number=%d alt=%u class=%u subclass=%u protocol=%u", iface, d[3], d[5], d[6], d[7]);
            } else if (d[1] == 5 && n >= 7) {
                ESP_LOGI(TAG, "ENDPOINT interface=%d address=0x%02x attributes=0x%02x mps=%u interval=%u", iface, d[2], d[3], d[4] | (d[5] << 8), d[6]);
            }
            pos += n;
        }
    }
    ESP_LOGI(TAG, "descriptor observer close: %s", esp_err_to_name(usb_host_device_close(observer, dev)));
}

static void descriptor_task(void *arg)
{
    const usb_host_client_config_t cfg = {
        .is_synchronous = false, .max_num_event_msg = 8,
        .async = {.client_event_callback = inspect_device, .callback_arg = NULL},
    };
    ESP_ERROR_CHECK(usb_host_client_register(&cfg, &observer));
    xTaskNotifyGive((TaskHandle_t)arg);
    for (;;) ESP_ERROR_CHECK(usb_host_client_handle_events(observer, portMAX_DELAY));
}

static void input_task(void *arg)
{
    (void)arg;
    input_event_t e;
    uint8_t last[64] = {0};
    size_t last_length = 0;
    bool have_report = false;
    for (;;) {
        if (xQueueReceive(events, &e, portMAX_DELAY) != pdTRUE) continue;
        if (e.kind == 0) {
            hid_host_dev_params_t params;
            hid_host_dev_info_t info;
            if (hid_host_device_get_params(e.device, &params) != ESP_OK ||
                hid_host_get_device_info(e.device, &info) != ESP_OK) {
                ESP_LOGW(TAG, "Device disappeared before inspection"); continue;
            }
            ESP_LOGI(TAG, "HID VID:PID=%04x:%04x interface=%u subclass=%u protocol=%u", info.VID, info.PID, params.iface_num, params.sub_class, params.proto);
            if (info.VID != 0x1209 || info.PID != 0xd004 || params.iface_num != 1 ||
                params.sub_class != HID_SUBCLASS_BOOT_INTERFACE || params.proto != HID_PROTOCOL_KEYBOARD || keyboard) continue;
            const hid_host_device_config_t cfg = {.callback = interface_event};
            esp_err_t err = hid_host_device_open(e.device, &cfg);
            ESP_LOGI(TAG, "keyboard open: %s", esp_err_to_name(err));
            if (err != ESP_OK) continue;
            keyboard = e.device;
            have_report = false;
            set_mask(0, "connect");
            err = hid_class_request_set_protocol(keyboard, HID_REPORT_PROTOCOL_BOOT);
            ESP_LOGI(TAG, "SET_PROTOCOL(BOOT): %s", esp_err_to_name(err));
            if (err != ESP_OK) { ESP_LOGE(TAG, "Input not started; retain log for class-request investigation"); continue; }
            err = hid_class_request_set_idle(keyboard, 0, 0);
            ESP_LOGI(TAG, "SET_IDLE(0): %s", esp_err_to_name(err));
            if (err != ESP_OK) { ESP_LOGE(TAG, "Input not started; retain log for class-request investigation"); continue; }
            err = hid_host_device_start(keyboard);
            ESP_LOGI(TAG, "keyboard start: %s", esp_err_to_name(err));
        } else if (e.kind == 2) {
            if (e.device == keyboard) { set_mask(0, "disconnect"); keyboard = NULL; }
            ESP_LOGI(TAG, "HID disconnected; state cleared");
        } else if (e.device == keyboard && e.kind == 3) {
            have_report = false;
            set_mask(0, "transfer_error");
            ESP_LOGE(TAG, "HID transfer/read error");
        } else if (e.device == keyboard && e.kind == 1) {
            // Avoid saturating 115200-baud UART with identical periodic reports.
            if (have_report && e.length == last_length && memcmp(last, e.data, e.length) == 0) continue;
            memcpy(last, e.data, e.length);
            last_length = e.length;
            have_report = true;
            ESP_LOG_BUFFER_HEX_LEVEL("RAW", e.data, e.length, ESP_LOG_INFO);
            uint16_t next;
            if (!sfc_decode(e.data, e.length, &next)) set_mask(0, "invalid_report");
            else set_mask(next, "report");
        }
    }
}

static void host_events(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t flags = 0;
        ESP_ERROR_CHECK(usb_host_lib_handle_events(portMAX_DELAY, &flags));
        if (flags) ESP_LOGI(TAG, "USB library flags=0x%" PRIx32, flags);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "FNK0099 Rev1.1 N16R8; P3/P4 keyboard probe; IDF %s", esp_get_idf_version());
    events = xQueueCreate(32, sizeof(input_event_t));
    ESP_ERROR_CHECK(events ? ESP_OK : ESP_ERR_NO_MEM);
    const usb_host_config_t host = {.skip_phy_setup = false, .intr_flags = ESP_INTR_FLAG_LEVEL1};
    ESP_ERROR_CHECK(usb_host_install(&host));
    ESP_LOGI(TAG, "usb_host_install: ESP_OK");
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(host_events, "usb_events", 4096, NULL, 2, NULL, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(descriptor_task, "descriptors", 4096, xTaskGetCurrentTaskHandle(), 3, NULL, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2000)) ? ESP_OK : ESP_ERR_TIMEOUT);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(input_task, "input", 4096, NULL, 4, NULL, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    const hid_host_driver_config_t hid = {
        .create_background_task = true, .task_priority = 5, .stack_size = 4096,
        .core_id = 0, .callback = hid_event,
    };
    ESP_ERROR_CHECK(hid_host_install(&hid));
    ESP_LOGI(TAG, "hid_host_install: ESP_OK; HOST_READY");
    for (;;) {
        ESP_LOGI(TAG, "alive uptime_ms=%" PRId64 " free_heap=%" PRIu32, esp_timer_get_time()/1000, esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
