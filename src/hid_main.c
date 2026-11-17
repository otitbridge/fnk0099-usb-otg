// Composite keyboard/mouse probe. Never log USB strings or vendor input payloads.
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <stdatomic.h>
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
#include "hid_input.h"

#define MAX_INTERFACES 8
#define REPORT_BYTES 64
#define CAPTURE_US (30LL * 1000000)

static const char *TAG = "HID_PROBE";
static usb_host_client_handle_t observer;
static QueueHandle_t events, control_events;
static atomic_uint dropped_reports;
static uintptr_t generation;

typedef enum { CONNECT, REPORT, DISCONNECT, TRANSFER_ERROR, STATUS, CAPTURE, STOP } event_kind_t;
typedef struct {
    event_kind_t kind;
    uintptr_t generation;
    hid_host_device_handle_t device;
    hid_host_dev_params_t params;
    size_t length;
    uint8_t data[REPORT_BYTES];
} input_event_t;
typedef struct {
    hid_host_device_handle_t device;
    hid_host_dev_params_t params;
    uintptr_t generation;
    bool started, decoded_layout, rollover;
    hid_input_layout_t layout;
    uint8_t keys[HID_INPUT_KEY_BYTES];
    uint32_t buttons, decoded, ignored, invalid, rollovers, mouse_samples;
    int64_t x, y, wheel, pan, last_mouse_log;
    uint32_t reports;
    uint32_t logged;
    uint32_t errors;
} interface_t;
static interface_t interfaces[MAX_INTERFACES];
static int64_t capture_until;

static void enqueue(const input_event_t *event)
{
    if (event->kind == REPORT || event->kind == TRANSFER_ERROR) {
        if (xQueueSend(events, event, 0) != pdTRUE) atomic_fetch_add(&dropped_reports, 1);
    } else {
        // Reserve a separate queue for disconnects and commands, even under mouse load.
        if (xQueueSend(control_events, event, 0) != pdTRUE) {
            if (event->kind == STATUS || event->kind == CAPTURE || event->kind == STOP)
                ESP_LOGW(TAG, "COMMAND_BUSY; retry after current operation");
            else ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
        }
    }
}

static void interface_event(hid_host_device_handle_t device, hid_host_interface_event_t event, void *arg)
{
    input_event_t e = {.device = device, .generation = (uintptr_t)arg};
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
    ESP_LOGI(TAG, "STATUS capture=%u dropped=%u free_heap=%" PRIu32,
             capture_until != 0, atomic_load(&dropped_reports), esp_get_free_heap_size());
    for (size_t i = 0; i < MAX_INTERFACES; ++i) {
        interface_t *s = &interfaces[i];
        if (!s->device) continue;
        ESP_LOGI(TAG, "IF_STATUS addr=%u iface=%u subclass=%u protocol=%u started=%u reports=%" PRIu32
                 " logged=%" PRIu32 " errors=%" PRIu32, s->params.addr, s->params.iface_num,
                 s->params.sub_class, s->params.proto, s->started, s->reports, s->logged, s->errors);
        ESP_LOGI(TAG, "DECODE_STATUS iface=%u supported=%u decoded=%" PRIu32 " ignored=%" PRIu32
                 " invalid=%" PRIu32 " rollover=%" PRIu32 " buttons=0x%08" PRIx32,
                 s->params.iface_num, s->decoded_layout, s->decoded, s->ignored,
                 s->invalid, s->rollovers, s->buttons);
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
    uintptr_t next_generation = ++generation;
    const hid_host_device_config_t cfg = {.callback = interface_event, .callback_arg = (void *)next_generation};
    esp_err_t err = hid_host_device_open(device, &cfg);
    ESP_LOGI(TAG, "HID_OPEN addr=%u iface=%u subclass=%u protocol=%u result=%s",
             params.addr, params.iface_num, params.sub_class, params.proto, esp_err_to_name(err));
    if (err != ESP_OK) return;
    memset(slot, 0, sizeof(*slot));
    slot->device = device;
    slot->params = params;
    slot->generation = next_generation;
    size_t length = 0;
    const uint8_t *descriptor = hid_host_get_report_descriptor(device, &length);
    if (!descriptor || !length || length > 2048) {
        ESP_LOGE(TAG, "REPORT_DESCRIPTOR_FAILED addr=%u iface=%u", params.addr, params.iface_num);
        return;
    }
    ESP_LOGI(TAG, "REPORT_DESCRIPTOR_BEGIN addr=%u iface=%u bytes=%u", params.addr, params.iface_num, (unsigned)length);
    ESP_LOG_BUFFER_HEX_LEVEL("HID_REPORT_DESC", descriptor, length, ESP_LOG_INFO);
    ESP_LOGI(TAG, "REPORT_DESCRIPTOR_END iface=%u", params.iface_num);
    if (params.sub_class == HID_SUBCLASS_BOOT_INTERFACE &&
        (params.proto == HID_PROTOCOL_KEYBOARD || params.proto == HID_PROTOCOL_MOUSE)) {
        slot->decoded_layout = hid_input_parse(descriptor, length, &slot->layout) && slot->layout.field_count != 0;
        ESP_LOGI(TAG, "LAYOUT iface=%u supported=%u input_reports=%u fields=%u", params.iface_num,
                 slot->decoded_layout, slot->layout.report_count, slot->layout.field_count);
        if (!slot->decoded_layout) {
            ESP_LOGW(TAG, "Layout unsupported; input payload remains suppressed");
        }
    }
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

static uint8_t modifiers(const uint8_t *keys)
{
    return keys[0xe0 / 8];
}

static void keyboard_state(interface_t *s, const uint8_t *next, const char *reason)
{
    // Releases first; identical periodic reports produce no duplicate transitions.
    for (unsigned down = 0; down <= 1; ++down) {
        for (unsigned usage = 1; usage < 256; ++usage) {
            bool was = hid_input_key(s->keys, usage), now = hid_input_key(next, usage);
            if (was == now || now != (bool)down) continue;
            if (capture_until) {
                ESP_LOGI(TAG, "KEY addr=%u iface=%u usage=0x%02x down=%u modifiers=0x%02x reason=%s",
                         s->params.addr, s->params.iface_num, usage, now, modifiers(next), reason);
                ++s->logged;
            }
        }
    }
    memcpy(s->keys, next, sizeof(s->keys));
}

static void mouse_flush(interface_t *s, const char *reason, bool force)
{
    if (capture_until && (force || s->x || s->y || s->wheel || s->pan)) {
        ESP_LOGI(TAG, "MOUSE addr=%u iface=%u buttons=0x%08" PRIx32 " dx=%" PRId64 " dy=%" PRId64
                 " wheel=%" PRId64 " pan=%" PRId64 " samples=%" PRIu32 " reason=%s",
                 s->params.addr, s->params.iface_num, s->buttons, s->x, s->y,
                 s->wheel, s->pan, s->mouse_samples, reason);
        ++s->logged;
    }
    s->x = s->y = s->wheel = s->pan = 0;
    s->mouse_samples = 0;
    s->last_mouse_log = esp_timer_get_time();
}

static void clear_state(interface_t *s, const char *reason)
{
    const uint8_t released[HID_INPUT_KEY_BYTES] = {0};
    keyboard_state(s, released, reason);
    mouse_flush(s, reason, false);
    if (s->buttons) {
        s->buttons = 0;
        mouse_flush(s, reason, true);
    }
    s->rollover = false;
}

static void consume_report(interface_t *s, const input_event_t *e)
{
    ++s->reports;
    // Vendor/HID++/digitizer payloads never reach the decoder or UART.
    if (!s->decoded_layout) { ++s->ignored; return; }
    hid_input_value_t value;
    hid_input_result_t result = hid_input_decode(&s->layout, e->data, e->length, &value);
    if (result == HID_INPUT_IGNORED) { ++s->ignored; return; }
    if (result == HID_INPUT_ROLLOVER) {
        ++s->rollovers;
        if (!s->rollover) ESP_LOGW(TAG, "KEYBOARD_ROLLOVER iface=%u; retain last valid state", s->params.iface_num);
        s->rollover = true;
        return;
    }
    if (result != HID_INPUT_OK) {
        ++s->invalid;
        ++s->errors;
        clear_state(s, "invalid_report");
        ESP_LOGE(TAG, "INPUT_INVALID iface=%u bytes=%u", s->params.iface_num, (unsigned)e->length);
        return;
    }
    ++s->decoded;
    s->rollover = false;
    if (value.kind == HID_INPUT_KEYBOARD) keyboard_state(s, value.keys, "report");
    else if (value.kind == HID_INPUT_MOUSE) {
        bool changed = value.buttons != s->buttons;
        if (changed) mouse_flush(s, "before_button", false);
        s->buttons = value.buttons;
        s->x += value.x; s->y += value.y;
        s->wheel += value.wheel; s->pan += value.pan;
        ++s->mouse_samples;
        if (changed) mouse_flush(s, "button", true);
    }
}

static void input_task(void *arg)
{
    (void)arg;
    input_event_t e;
    unsigned last_dropped = 0;
    for (;;) {
        bool received = xQueueReceive(control_events, &e, 0) == pdTRUE;
        if (!received) received = xQueueReceive(events, &e, pdMS_TO_TICKS(10)) == pdTRUE;
        unsigned dropped = atomic_load(&dropped_reports);
        if (dropped != last_dropped) {
            // Discard queued snapshots before clearing state: replaying an old
            // press after a lost release would leave a key/button held.
            xQueueReset(events);
            if (received && (e.kind == REPORT || e.kind == TRANSFER_ERROR)) received = false;
            for (size_t i = 0; i < MAX_INTERFACES; ++i)
                if (interfaces[i].device) clear_state(&interfaces[i], "queue_overflow");
            ESP_LOGE(TAG, "INPUT_DROPPED total=%u; trial invalid", dropped);
            last_dropped = dropped;
        }
        int64_t now = esp_timer_get_time();
        for (size_t i = 0; i < MAX_INTERFACES; ++i)
            if (interfaces[i].device && now - interfaces[i].last_mouse_log >= 20000)
                mouse_flush(&interfaces[i], "interval", false);
        if (capture_until && now >= capture_until) {
            for (size_t i = 0; i < MAX_INTERFACES; ++i)
                if (interfaces[i].device) mouse_flush(&interfaces[i], "timeout", false);
            capture_until = 0;
            ESP_LOGI(TAG, "CAPTURE_END reason=timeout");
            show_status();
        }
        if (!received) continue;
        if (e.kind == CONNECT) { open_interface(e.device); continue; }
        if (e.kind == STATUS) { show_status(); continue; }
        if (e.kind == STOP) {
            for (size_t i = 0; i < MAX_INTERFACES; ++i)
                if (interfaces[i].device) mouse_flush(&interfaces[i], "stop", false);
            capture_until = 0;
            ESP_LOGI(TAG, "CAPTURE_END reason=command");
            show_status();
            continue;
        }
        if (e.kind == CAPTURE) {
            bool renew = capture_until != 0;
            if (!renew) {
                for (size_t i = 0; i < MAX_INTERFACES; ++i) {
                    interface_t *s = &interfaces[i];
                    mouse_flush(s, "before_capture", false);
                    s->logged = 0;
                    if (!s->device) continue;
                    ESP_LOGI(TAG, "STATE_SYNC iface=%u modifiers=0x%02x buttons=0x%08" PRIx32,
                             s->params.iface_num, modifiers(s->keys), s->buttons);
                }
            }
            capture_until = esp_timer_get_time() + CAPTURE_US;
            ESP_LOGI(TAG, "CAPTURE_%s seconds=30; decoded fixed test inputs only", renew ? "RENEW" : "BEGIN");
            continue;
        }
        for (size_t i = 0; i < MAX_INTERFACES; ++i) {
            interface_t *s = &interfaces[i];
            if (s->device != e.device || s->generation != e.generation) continue;
            if (e.kind == DISCONNECT) {
                clear_state(s, "disconnect");
                ESP_LOGI(TAG, "STATE_CLEAR addr=%u iface=%u reason=disconnect", s->params.addr, s->params.iface_num);
                memset(s, 0, sizeof(*s));
            } else if (e.kind == TRANSFER_ERROR) {
                ++s->errors;
                clear_state(s, "transfer_error");
                ESP_LOGE(TAG, "INPUT_ERROR addr=%u iface=%u", s->params.addr, s->params.iface_num);
            } else if (e.kind == REPORT) consume_report(s, &e);
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
    ESP_LOGI(TAG, "FNK0099 composite HID keyboard/mouse probe; IDF %s; input_capture=disabled", esp_get_idf_version());
    events = xQueueCreate(64, sizeof(input_event_t));
    control_events = xQueueCreate(16, sizeof(input_event_t));
    ESP_ERROR_CHECK(events && control_events ? ESP_OK : ESP_ERR_NO_MEM);
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
