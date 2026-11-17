// Composite HID discovery probe. Never log USB strings or vendor input payloads.
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_intr_alloc.h"
#include "usb/usb_host.h"
#include "usb/hid_host.h"

#define MAX_INTERFACES 8
#define REPORT_BYTES 64
#define CAPTURE_US (30LL * 1000000)

static const char *TAG = "HID_PROBE";
static usb_host_client_handle_t observer;
static QueueHandle_t events;

typedef enum { CONNECT, REPORT, DISCONNECT, TRANSFER_ERROR, STATUS, CAPTURE, STOP } event_kind_t;
typedef struct {
    event_kind_t kind;
    hid_host_device_handle_t device;
    hid_host_dev_params_t params;
    size_t length;
    uint8_t data[REPORT_BYTES];
} input_event_t;
typedef struct {
    hid_host_device_handle_t device;
    hid_host_dev_params_t params;
    bool started;
    uint32_t reports;
    uint32_t logged;
    uint32_t errors;
} interface_t;
static interface_t interfaces[MAX_INTERFACES];
static int64_t capture_until;

static void enqueue(const input_event_t *event)
{
    // A dropped release must not silently turn into a passing input trial.
    ESP_ERROR_CHECK(xQueueSend(events, event, 0) == pdTRUE ? ESP_OK : ESP_ERR_NO_MEM);
}

static void interface_event(hid_host_device_handle_t device, hid_host_interface_event_t event, void *arg)
{
    (void)arg;
    input_event_t e = {.device = device};
    if (hid_host_device_get_params(device, &e.params) != ESP_OK) return;
    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
        e.kind = REPORT;
        if (hid_host_device_get_raw_input_report_data(device, e.data, sizeof(e.data), &e.length) != ESP_OK)
            e.kind = TRANSFER_ERROR;
        break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        // Close before returning to the driver's disconnect handler.
        ESP_LOGI(TAG, "DISCONNECT addr=%u iface=%u close=%s", e.params.addr, e.params.iface_num,
                 esp_err_to_name(hid_host_device_close(device)));
        e.kind = DISCONNECT;
        break;
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        e.kind = TRANSFER_ERROR;
        break;
    default: return;
    }
    enqueue(&e);
}

static void hid_event(hid_host_device_handle_t device, hid_host_driver_event_t event, void *arg)
{
    (void)arg;
    if (event == HID_HOST_DRIVER_EVENT_CONNECTED) {
        input_event_t e = {.device = device, .kind = CONNECT};
        enqueue(&e);
    }
}

static void inspect_device(const usb_host_client_event_msg_t *event, void *arg)
{
    (void)arg;
    if (event->event != USB_HOST_CLIENT_EVENT_NEW_DEV) return;
    usb_device_handle_t device;
    esp_err_t err = usb_host_device_open(observer, event->new_dev.address, &device);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "descriptor open: %s", esp_err_to_name(err));
        return;
    }
    const usb_device_desc_t *desc;
    const usb_config_desc_t *config;
    usb_device_info_t info;
    if (usb_host_get_device_descriptor(device, &desc) == ESP_OK)
        ESP_LOGI(TAG, "DEVICE addr=%u VID:PID=%04x:%04x", event->new_dev.address, desc->idVendor, desc->idProduct);
    if (usb_host_device_info(device, &info) == ESP_OK)
        ESP_LOGI(TAG, "USB speed=%s", info.speed == USB_SPEED_FULL ? "FULL (12Mbps)" :
                 info.speed == USB_SPEED_LOW ? "LOW (1.5Mbps)" : "OTHER");
    if (usb_host_get_active_config_descriptor(device, &config) == ESP_OK) {
        const uint8_t *raw = (const uint8_t *)config;
        int iface = -1;
        for (size_t pos = 0; pos + 2 <= config->wTotalLength;) {
            const uint8_t *d = raw + pos;
            size_t n = d[0];
            if (n < 2 || n > config->wTotalLength - pos) break;
            if (d[1] == 4 && n >= 9) {
                iface = d[2];
                ESP_LOGI(TAG, "INTERFACE number=%d alt=%u class=%u subclass=%u protocol=%u",
                         iface, d[3], d[5], d[6], d[7]);
            } else if (d[1] == 5 && n >= 7) {
                ESP_LOGI(TAG, "ENDPOINT iface=%d address=0x%02x attributes=0x%02x mps=%u interval=%u",
                         iface, d[2], d[3], d[4] | (d[5] << 8), d[6]);
            } else if (d[1] == 0x21 && n >= 9 && d[6] == 0x22) {
                ESP_LOGI(TAG, "HID_DESCRIPTOR iface=%d report_bytes=%u", iface, d[7] | (d[8] << 8));
            }
            pos += n;
        }
    }
    ESP_LOGI(TAG, "descriptor observer close: %s", esp_err_to_name(usb_host_device_close(observer, device)));
}

static void descriptor_task(void *arg)
{
    const usb_host_client_config_t cfg = {
        .is_synchronous = false, .max_num_event_msg = 8,
        .async = {.client_event_callback = inspect_device},
    };
    ESP_ERROR_CHECK(usb_host_client_register(&cfg, &observer));
    xTaskNotifyGive((TaskHandle_t)arg);
    for (;;) ESP_ERROR_CHECK(usb_host_client_handle_events(observer, portMAX_DELAY));
}

static void show_status(void)
{
    ESP_LOGI(TAG, "STATUS capture=%u free_heap=%" PRIu32, capture_until != 0, esp_get_free_heap_size());
    for (size_t i = 0; i < MAX_INTERFACES; ++i) {
        interface_t *s = &interfaces[i];
        if (!s->device) continue;
        ESP_LOGI(TAG, "IF_STATUS addr=%u iface=%u subclass=%u protocol=%u started=%u reports=%" PRIu32
                 " logged=%" PRIu32 " errors=%" PRIu32, s->params.addr, s->params.iface_num,
                 s->params.sub_class, s->params.proto, s->started, s->reports, s->logged, s->errors);
    }
}

static void open_interface(hid_host_device_handle_t device)
{
    hid_host_dev_params_t params;
    if (hid_host_device_get_params(device, &params) != ESP_OK) {
        ESP_LOGW(TAG, "Interface disappeared before inspection");
        return;
    }
    interface_t *slot = NULL;
    for (size_t i = 0; i < MAX_INTERFACES; ++i) {
        if (interfaces[i].device == device) return;
        if (!slot && !interfaces[i].device) slot = &interfaces[i];
    }
    if (!slot) {
        ESP_LOGE(TAG, "Interface limit reached; trial incomplete");
        return;
    }
    const hid_host_device_config_t cfg = {.callback = interface_event};
    esp_err_t err = hid_host_device_open(device, &cfg);
    ESP_LOGI(TAG, "HID_OPEN addr=%u iface=%u subclass=%u protocol=%u result=%s",
             params.addr, params.iface_num, params.sub_class, params.proto, esp_err_to_name(err));
    if (err != ESP_OK) return;
    *slot = (interface_t){.device = device, .params = params};
    size_t length = 0;
    const uint8_t *descriptor = hid_host_get_report_descriptor(device, &length);
    if (!descriptor || !length || length > 2048) {
        ESP_LOGE(TAG, "REPORT_DESCRIPTOR_FAILED addr=%u iface=%u", params.addr, params.iface_num);
        return;
    }
    ESP_LOGI(TAG, "REPORT_DESCRIPTOR_BEGIN addr=%u iface=%u bytes=%u", params.addr, params.iface_num, (unsigned)length);
    ESP_LOG_BUFFER_HEX_LEVEL("HID_REPORT_DESC", descriptor, length, ESP_LOG_INFO);
    ESP_LOGI(TAG, "REPORT_DESCRIPTOR_END iface=%u", params.iface_num);
    // Keep Report protocol to retain wheel, extra buttons and report IDs.
    if (params.sub_class == HID_SUBCLASS_BOOT_INTERFACE) {
        err = hid_class_request_set_protocol(device, HID_REPORT_PROTOCOL_REPORT);
        ESP_LOGI(TAG, "SET_PROTOCOL(REPORT) iface=%u result=%s", params.iface_num, esp_err_to_name(err));
        if (err != ESP_OK) return;
    }
    err = hid_host_device_start(device);
    ESP_LOGI(TAG, "HID_START addr=%u iface=%u result=%s", params.addr, params.iface_num, esp_err_to_name(err));
    slot->started = err == ESP_OK;
}

static void input_task(void *arg)
{
    (void)arg;
    input_event_t e;
    for (;;) {
        bool received = xQueueReceive(events, &e, pdMS_TO_TICKS(100)) == pdTRUE;
        if (capture_until && esp_timer_get_time() >= capture_until) {
            capture_until = 0;
            ESP_LOGI(TAG, "CAPTURE_END reason=timeout");
            show_status();
        }
        if (!received) continue;
        if (e.kind == CONNECT) { open_interface(e.device); continue; }
        if (e.kind == STATUS) { show_status(); continue; }
        if (e.kind == STOP) {
            capture_until = 0;
            ESP_LOGI(TAG, "CAPTURE_END reason=command");
            show_status();
            continue;
        }
        if (e.kind == CAPTURE) {
            capture_until = esp_timer_get_time() + CAPTURE_US;
            for (size_t i = 0; i < MAX_INTERFACES; ++i) interfaces[i].logged = 0;
            ESP_LOGI(TAG, "CAPTURE_BEGIN seconds=30; fixed test inputs only; max=40 reports/interface");
            continue;
        }
        for (size_t i = 0; i < MAX_INTERFACES; ++i) {
            interface_t *s = &interfaces[i];
            if (s->device != e.device || s->params.addr != e.params.addr ||
                s->params.iface_num != e.params.iface_num) continue;
            if (e.kind == DISCONNECT) {
                ESP_LOGI(TAG, "STATE_CLEAR addr=%u iface=%u reason=disconnect", s->params.addr, s->params.iface_num);
                memset(s, 0, sizeof(*s));
            } else if (e.kind == TRANSFER_ERROR) {
                ++s->errors;
                ESP_LOGE(TAG, "INPUT_ERROR addr=%u iface=%u", s->params.addr, s->params.iface_num);
            } else if (e.kind == REPORT) {
                ++s->reports;
                // Vendor/HID++ payloads may contain identifiers: do not dump them.
                if (capture_until && s->logged < 40 && s->params.sub_class == HID_SUBCLASS_BOOT_INTERFACE &&
                    (s->params.proto == HID_PROTOCOL_KEYBOARD || s->params.proto == HID_PROTOCOL_MOUSE)) {
                    ++s->logged;
                    ESP_LOGI(TAG, "RAW addr=%u iface=%u bytes=%u sample=%" PRIu32,
                             s->params.addr, s->params.iface_num, (unsigned)e.length, s->logged);
                    ESP_LOG_BUFFER_HEX_LEVEL("HID_INPUT", e.data, e.length, ESP_LOG_INFO);
                }
            }
            break;
        }
    }
}

static void uart_task(void *arg)
{
    (void)arg;
    char line[32];
    size_t used = 0;
    bool overflow = false;
    for (;;) {
        uint8_t c;
        if (uart_read_bytes(UART_NUM_0, &c, 1, pdMS_TO_TICKS(100)) != 1) continue;
        if (c == '\r' || c == '\n') {
            line[used] = 0;
            if (!overflow && used) {
                input_event_t e = {0};
                if (!strcmp(line, "status")) e.kind = STATUS;
                else if (!strcmp(line, "capture")) e.kind = CAPTURE;
                else if (!strcmp(line, "stop")) e.kind = STOP;
                else {
                    ESP_LOGI(TAG, "COMMANDS status | capture (30s) | stop");
                    used = 0;
                    continue;
                }
                enqueue(&e);
            }
            used = 0;
            overflow = false;
        } else if (!overflow && c >= 32 && c <= 126 && used + 1 < sizeof(line)) line[used++] = c;
        else overflow = true;
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
    ESP_LOGI(TAG, "FNK0099 composite HID descriptor probe; IDF %s; input_capture=disabled", esp_get_idf_version());
    events = xQueueCreate(64, sizeof(input_event_t));
    ESP_ERROR_CHECK(events ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0));
    const usb_host_config_t host = {.skip_phy_setup = false, .intr_flags = ESP_INTR_FLAG_LEVEL1};
    ESP_ERROR_CHECK(usb_host_install(&host));
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(host_events, "usb_events", 4096, NULL, 2, NULL, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(descriptor_task, "descriptors", 4096, xTaskGetCurrentTaskHandle(), 3, NULL, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2000)) ? ESP_OK : ESP_ERR_TIMEOUT);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(input_task, "hid_input", 6144, NULL, 4, NULL, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(uart_task, "hid_uart", 3072, NULL, 3, NULL, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    const hid_host_driver_config_t hid = {
        .create_background_task = true, .task_priority = 5, .stack_size = 4096,
        .core_id = 0, .callback = hid_event,
    };
    ESP_ERROR_CHECK(hid_host_install(&hid));
    ESP_LOGI(TAG, "HID_HOST_READY; COMMANDS status | capture (30s) | stop");
    for (;;) {
        ESP_LOGI(TAG, "alive uptime_ms=%" PRId64 " free_heap=%" PRIu32, esp_timer_get_time()/1000, esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
