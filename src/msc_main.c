// USB MSC file probe. New-file writes require an explicit UART command.
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "mbedtls/sha256.h"
#include "usb/msc_host.h"
#include "usb/msc_host_vfs.h"
#include "usb/usb_host.h"
#include "msc_file_probe.h"

#define MOUNT_PATH "/usb"
#define READ_PATH MOUNT_PATH "/PROBE.TXT"
#define WRITE_PATH MOUNT_PATH "/WRITE.TXT"
#define BULK_PATH MOUNT_PATH "/BULK1M.BIN"
#define BULK_BUFFER_SIZE 4096u

static const char *TAG = "MSC_PROBE";
static const char read_expected[] = "FNK0099 USB MSC read probe v1\n";
static const char write_expected[] = "FNK0099 USB MSC write probe v1\n";
static QueueHandle_t events;
static msc_host_device_handle_t device;
static msc_host_vfs_handle_t volume;
static uint8_t device_address;
static bool cleanup_failed;

typedef enum {
    CMD_HELP, CMD_STATUS, CMD_MOUNT, CMD_LS, CMD_READ, CMD_UNMOUNT,
    CMD_WRITE, CMD_VERIFY_WRITE, CMD_WRITE_BULK, CMD_VERIFY_BULK,
} command_t;
typedef struct {
    enum { CONNECTED, DISCONNECTED, COMMAND } kind;
    union {
        uint8_t address;
        msc_host_device_handle_t handle;
        command_t command;
    } data;
} probe_event_t;

static bool result(const char *operation, esp_err_t err, int64_t start)
{
    int64_t elapsed = (esp_timer_get_time() - start) / 1000;
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "%s: ESP_OK elapsed_ms=%" PRId64, operation, elapsed);
    } else {
        ESP_LOGE(TAG, "%s: %s code=0x%x elapsed_ms=%" PRId64,
                 operation, esp_err_to_name(err), (unsigned)err, elapsed);
    }
    return err == ESP_OK;
}

static void enqueue(const probe_event_t *event)
{
    // A lost disconnect event invalidates all later observations; fail visibly.
    ESP_ERROR_CHECK(xQueueSend(events, event, 0) == pdTRUE ? ESP_OK : ESP_ERR_NO_MEM);
}

static void msc_event(const msc_host_event_t *event, void *arg)
{
    (void)arg;
    probe_event_t queued;
    if (event->event == MSC_DEVICE_CONNECTED) {
        queued.kind = CONNECTED;
        queued.data.address = event->device.address;
    } else if (event->event == MSC_DEVICE_DISCONNECTED) {
        queued.kind = DISCONNECTED;
        queued.data.handle = event->device.handle;
    } else {
        return;
    }
    enqueue(&queued);
}

static void show_status(void)
{
    ESP_LOGI(TAG, "STATUS device=%u mounted=%u cleanup_failed=%u free_heap=%" PRIu32
             " uptime_ms=%" PRId64, device != NULL, volume != NULL, cleanup_failed,
             esp_get_free_heap_size(), esp_timer_get_time() / 1000);
}

static void show_media(void)
{
    msc_host_device_info_t info;
    int64_t start = esp_timer_get_time();
    if (!result("get_device_info", msc_host_get_device_info(device, &info), start)) return;
    // String descriptors include serial numbers and are deliberately not printed.
    ESP_LOGI(TAG, "MEDIA VID:PID=%04x:%04x sectors=%" PRIu32 " sector_bytes=%" PRIu32
             " capacity_bytes=%" PRIu64, info.idVendor, info.idProduct,
             info.sector_count, info.sector_size, (uint64_t)info.sector_count * info.sector_size);
}

static void mount_media(void)
{
    if (!device || volume || cleanup_failed) {
        ESP_LOGW(TAG, "MOUNT rejected; check status");
        return;
    }
    const esp_vfs_fat_mount_config_t config = {
        .format_if_mount_failed = false,
        .max_files = 3,
        .allocation_unit_size = 0,
    };
    int64_t start = esp_timer_get_time();
    if (!result("mount", msc_host_vfs_register(device, MOUNT_PATH, &config, &volume), start)) {
        ESP_LOGW(TAG, "MOUNT_FAILED; no formatting attempted; investigate before retry");
        return;
    }
    uint64_t total, free_bytes;
    start = esp_timer_get_time();
    errno = 0;
    esp_err_t err = esp_vfs_fat_info(MOUNT_PATH, &total, &free_bytes);
    if (result("filesystem_info", err, start)) {
        ESP_LOGI(TAG, "FILESYSTEM total_bytes=%" PRIu64 " free_bytes=%" PRIu64, total, free_bytes);
    } else {
        ESP_LOGE(TAG, "filesystem_info errno=%d", errno);
    }
}

static bool unmount_media(void)
{
    if (!volume) return true;
    int64_t start = esp_timer_get_time();
    if (!result("unmount", msc_host_vfs_unregister(volume), start)) {
        cleanup_failed = true;
        ESP_LOGE(TAG, "CLEANUP_FAILED; stop testing and reset");
        return false;
    }
    volume = NULL;
    return true;
}

static void count_root_entries(void)
{
    if (!volume || cleanup_failed) {
        ESP_LOGW(TAG, "LS rejected; mount required");
        return;
    }
    int64_t start = esp_timer_get_time();
    DIR *directory = opendir(MOUNT_PATH);
    if (!directory) {
        ESP_LOGE(TAG, "opendir failed errno=%d", errno);
        return;
    }
    size_t count = 0;
    int read_errno;
    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(directory);
        if (!entry) { read_errno = errno; break; }
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) ++count;
    }
    int close_result = closedir(directory);
    int close_errno = close_result ? errno : 0;
    if (read_errno || close_result) {
        ESP_LOGE(TAG, "ROOT_READ_FAILED read_errno=%d close_errno=%d", read_errno, close_errno);
        return;
    }
    // Only the count is logged; existing filenames must not enter public evidence.
    ESP_LOGI(TAG, "ROOT_READ_OK entries=%u elapsed_ms=%" PRId64,
             (unsigned)count, (esp_timer_get_time() - start) / 1000);
}

static void verify_file(const char *path, const char *expected, size_t expected_length,
                        const char *label, int64_t start)
{
    if (!volume || cleanup_failed) {
        ESP_LOGW(TAG, "%s rejected; mount required", label);
        return;
    }
    uint8_t data[128];
    if (expected_length >= sizeof(data)) {
        ESP_LOGE(TAG, "%s_FAILED; fixture exceeds verification buffer", label);
        return;
    }
    FILE *file = fopen(path, "rb");
    if (!file) {
        ESP_LOGE(TAG, "%s_NOT_VERIFIED open errno=%d; expected fixture required", label, errno);
        return;
    }
    // One byte beyond the expected length detects a longer file without reading it all.
    errno = 0;
    size_t length = fread(data, 1, expected_length + 1, file);
    bool read_error = ferror(file) != 0;
    int read_errno = errno;
    int close_result = fclose(file);
    int close_errno = close_result ? errno : 0;
    if (read_error || close_result) {
        ESP_LOGE(TAG, "%s_FAILED read_errno=%d close_errno=%d", label, read_errno, close_errno);
        return;
    }
    if (length != expected_length || memcmp(data, expected, length)) {
        ESP_LOGE(TAG, "%s_MISMATCH; expected_bytes=%u; contents suppressed",
                 label, (unsigned)expected_length);
        return;
    }
    uint8_t digest[32];
    int hash_err = mbedtls_sha256(data, length, digest, 0);
    if (hash_err) {
        ESP_LOGE(TAG, "%s_FAILED sha256 code=%d", label, hash_err);
        return;
    }
    char hex[65];
    for (size_t i = 0; i < sizeof(digest); ++i) snprintf(hex + 2 * i, 3, "%02x", digest[i]);
    ESP_LOGI(TAG, "%s_PASS bytes=%u sha256=%s elapsed_ms=%" PRId64,
             label, (unsigned)length, hex, (esp_timer_get_time() - start) / 1000);
}

static void write_fixture(void)
{
    if (!volume || cleanup_failed) {
        ESP_LOGW(TAG, "WRITE rejected; mount required");
        return;
    }
    int64_t start = esp_timer_get_time();
    msc_file_write_result_t written = msc_create_synced_file(WRITE_PATH, write_expected,
                                                          sizeof(write_expected) - 1);
    if (written.error) {
        if (written.error == EEXIST && strcmp(written.operation, "open") == 0) {
            ESP_LOGW(TAG, "WRITE_EXISTS; no file changed; use verify-write to inspect the test file");
        } else {
            ESP_LOGE(TAG, "WRITE_FAILED operation=%s errno=%d close_errno=%d bytes_written=%u elapsed_ms=%" PRId64,
                     written.operation, written.error, written.close_error,
                     (unsigned)written.bytes_written, (esp_timer_get_time() - start) / 1000);
            ESP_LOGE(TAG, "Stop testing; retain any partial file for investigation");
        }
        return;
    }
    ESP_LOGI(TAG, "WRITE_SYNC_OK bytes=%u fsync=OK close=OK", (unsigned)written.bytes_written);
    verify_file(WRITE_PATH, write_expected, sizeof(write_expected) - 1, "WRITE", start);
}

static void show_help(void)
{
    ESP_LOGI(TAG, "COMMANDS help | status | mount | ls | read | unmount | write | verify-write (CR or LF)");
    ESP_LOGI(TAG, "BULK COMMANDS write-1m | verify-1m (CR or LF)");
    ESP_LOGI(TAG, "read verifies PROBE.TXT; write exclusively creates WRITE.TXT and verifies it");
    ESP_LOGI(TAG, "verify-write only reads WRITE.TXT; no automatic writes or formatting");
    ESP_LOGI(TAG, "write-1m exclusively creates BULK1M.BIN; verify-1m only reads it");
}

static void verify_bulk_file(uint8_t *buffer)
{
    int64_t start = esp_timer_get_time();
    FILE *file = fopen(BULK_PATH, "rb");
    if (!file) {
        ESP_LOGE(TAG, "BULK_NOT_VERIFIED open errno=%d", errno);
        return;
    }
    mbedtls_sha256_context hash;
    mbedtls_sha256_init(&hash);
    int hash_error = mbedtls_sha256_starts(&hash, 0);
    int read_error = 0;
    bool size_mismatch = false;
    size_t total = 0;
    while (!hash_error && !read_error && !size_mismatch && total < MSC_PATTERN_FILE_BYTES) {
        size_t remaining = MSC_PATTERN_FILE_BYTES - total;
        size_t length = remaining < BULK_BUFFER_SIZE ? remaining : BULK_BUFFER_SIZE;
        errno = 0;
        size_t count = fread(buffer, 1, length, file);
        total += count;
        if (ferror(file)) {
            read_error = errno ? errno : EIO;
        } else if (!count) {
            size_mismatch = true;
        } else {
            hash_error = mbedtls_sha256_update(&hash, buffer, count);
        }
    }
    if (!hash_error && !read_error && !size_mismatch) {
        errno = 0;
        int extra = fgetc(file);
        if (ferror(file)) read_error = errno ? errno : EIO;
        else if (extra != EOF) size_mismatch = true;
    }
    int close_result = fclose(file);
    int close_error = close_result ? (errno ? errno : EIO) : 0;
    uint8_t digest[32];
    if (!hash_error && !read_error && !size_mismatch && !close_error) {
        hash_error = mbedtls_sha256_finish(&hash, digest);
    }
    mbedtls_sha256_free(&hash);
    if (hash_error || read_error || close_error) {
        ESP_LOGE(TAG, "BULK_FAILED read_errno=%d close_errno=%d hash_code=%d bytes_read=%u",
                 read_error, close_error, hash_error, (unsigned)total);
        return;
    }
    if (size_mismatch) {
        ESP_LOGE(TAG, "BULK_SIZE_MISMATCH expected_bytes=%u bytes_read=%u",
                 MSC_PATTERN_FILE_BYTES, (unsigned)total);
        return;
    }
    char hex[65];
    for (size_t i = 0; i < sizeof(digest); ++i) snprintf(hex + 2 * i, 3, "%02x", digest[i]);
    if (strcmp(hex, MSC_PATTERN_SHA256)) {
        ESP_LOGE(TAG, "BULK_HASH_MISMATCH; contents suppressed");
        return;
    }
    ESP_LOGI(TAG, "BULK_PASS bytes=%u sha256=%s read_ms=%" PRId64,
             (unsigned)total, hex, (esp_timer_get_time() - start) / 1000);
}

static void bulk_file_command(bool create)
{
    if (!volume || cleanup_failed) {
        ESP_LOGW(TAG, "BULK rejected; mount required");
        return;
    }
    uint8_t *buffer = malloc(BULK_BUFFER_SIZE);
    if (!buffer) {
        ESP_LOGE(TAG, "BULK_FAILED operation=allocate buffer_bytes=%u", BULK_BUFFER_SIZE);
        return;
    }
    if (create) {
        int64_t start = esp_timer_get_time();
        ESP_LOGI(TAG, "BULK_WRITE_BEGIN bytes=%u buffer_bytes=%u", MSC_PATTERN_FILE_BYTES, BULK_BUFFER_SIZE);
        msc_file_write_result_t written = msc_create_pattern_file(BULK_PATH, buffer, BULK_BUFFER_SIZE);
        if (written.error) {
            if (written.error == EEXIST && strcmp(written.operation, "open") == 0) {
                ESP_LOGW(TAG, "BULK_EXISTS; no file changed; stop before retrying");
            } else {
                ESP_LOGE(TAG, "BULK_WRITE_FAILED operation=%s errno=%d close_errno=%d bytes_written=%u elapsed_ms=%" PRId64,
                         written.operation, written.error, written.close_error,
                         (unsigned)written.bytes_written, (esp_timer_get_time() - start) / 1000);
                ESP_LOGE(TAG, "Stop testing; retain any partial file for investigation");
            }
            free(buffer);
            return;
        }
        ESP_LOGI(TAG, "BULK_SYNC_OK bytes=%u fsync=OK close=OK write_sync_ms=%" PRId64,
                 (unsigned)written.bytes_written, (esp_timer_get_time() - start) / 1000);
    }
    verify_bulk_file(buffer);
    free(buffer);
}

static void run_command(command_t command)
{
    switch (command) {
    case CMD_HELP: show_help(); break;
    case CMD_STATUS:
        show_status();
        if (device && !cleanup_failed) show_media();
        break;
    case CMD_MOUNT: mount_media(); break;
    case CMD_LS: count_root_entries(); break;
    case CMD_READ:
        verify_file(READ_PATH, read_expected, sizeof(read_expected) - 1, "READ", esp_timer_get_time());
        break;
    case CMD_WRITE: write_fixture(); break;
    case CMD_VERIFY_WRITE:
        verify_file(WRITE_PATH, write_expected, sizeof(write_expected) - 1, "WRITE_VERIFY", esp_timer_get_time());
        break;
    case CMD_WRITE_BULK: bulk_file_command(true); break;
    case CMD_VERIFY_BULK: bulk_file_command(false); break;
    case CMD_UNMOUNT:
        if (cleanup_failed) {
            ESP_LOGE(TAG, "CLEANUP_FAILED; reset required");
        } else if (unmount_media()) {
            ESP_LOGI(TAG, "SAFE_TO_REMOVE; no open application files");
        }
        break;
    }
}

static void command_task(void *arg)
{
    (void)arg;
    const char *names[] = {
        "help", "status", "mount", "ls", "read", "unmount", "write", "verify-write",
        "write-1m", "verify-1m",
    };
    char line[32];
    size_t length = 0;
    bool invalid = false;
    for (;;) {
        uint8_t c;
        if (uart_read_bytes(UART_NUM_0, &c, 1, pdMS_TO_TICKS(100)) != 1) continue;
        if (c == '\r' || c == '\n') {
            if (length || invalid) {
                line[length] = '\0';
                bool recognized = false;
                if (!invalid) {
                    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
                        if (strcmp(line, names[i])) continue;
                        probe_event_t event = {.kind = COMMAND, .data.command = (command_t)i};
                        if (xQueueSend(events, &event, 0) != pdTRUE) {
                            ESP_LOGW(TAG, "COMMAND_BUSY; retry after current operation");
                        }
                        recognized = true;
                        break;
                    }
                }
                if (!recognized) ESP_LOGW(TAG, "Unknown or invalid command; use help");
            }
            length = 0;
            invalid = false;
        } else if (c >= 32 && c <= 126 && length < sizeof(line) - 1 && !invalid) {
            line[length++] = (char)c;
        } else {
            invalid = true; // Discard the entire invalid/overlong line, not a suffix.
        }
    }
}

static void inspect_device(const usb_host_client_event_msg_t *event, void *arg)
{
    if (event->event != USB_HOST_CLIENT_EVENT_NEW_DEV) return;
    usb_host_client_handle_t observer = *(usb_host_client_handle_t *)arg;
    usb_device_handle_t usb_device;
    int64_t start = esp_timer_get_time();
    if (!result("descriptor_open", usb_host_device_open(observer, event->new_dev.address, &usb_device), start)) return;
    usb_device_info_t info;
    const usb_device_desc_t *descriptor;
    if (usb_host_get_device_descriptor(usb_device, &descriptor) == ESP_OK &&
        usb_host_device_info(usb_device, &info) == ESP_OK) {
        const char *speed = info.speed == USB_SPEED_FULL ? "FULL (12Mbps)" :
                            info.speed == USB_SPEED_LOW ? "LOW (1.5Mbps)" : "OTHER";
        ESP_LOGI(TAG, "USB VID:PID=%04x:%04x speed=%s", descriptor->idVendor, descriptor->idProduct, speed);
    }
    start = esp_timer_get_time();
    result("descriptor_close", usb_host_device_close(observer, usb_device), start);
}

static void descriptor_task(void *arg)
{
    usb_host_client_handle_t observer;
    usb_host_client_config_t config = {
        .is_synchronous = false,
        .max_num_event_msg = 8,
        .async = {.client_event_callback = inspect_device},
    };
    // The library copies config; pass a pointer to this task's persistent handle.
    config.async.callback_arg = &observer;
    ESP_ERROR_CHECK(usb_host_client_register(&config, &observer));
    xTaskNotifyGive((TaskHandle_t)arg);
    for (;;) ESP_ERROR_CHECK(usb_host_client_handle_events(observer, portMAX_DELAY));
}

static void host_events(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t flags;
        ESP_ERROR_CHECK(usb_host_lib_handle_events(portMAX_DELAY, &flags));
        if (flags) ESP_LOGI(TAG, "USB library flags=0x%" PRIx32, flags);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "FNK0099 MSC file probe; IDF %s; automatic_format=disabled; automatic_write=disabled", esp_get_idf_version());
    events = xQueueCreate(16, sizeof(probe_event_t));
    ESP_ERROR_CHECK(events ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0));
    uart_vfs_dev_use_driver(UART_NUM_0);
    const usb_host_config_t host = {.skip_phy_setup = false, .intr_flags = ESP_INTR_FLAG_LEVEL1};
    ESP_ERROR_CHECK(usb_host_install(&host));
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(host_events, "usb_events", 4096, NULL, 2, NULL, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(descriptor_task, "descriptors", 3072, xTaskGetCurrentTaskHandle(), 3, NULL, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2000)) ? ESP_OK : ESP_ERR_TIMEOUT);
    const msc_host_driver_config_t msc = {
        .create_backround_task = true, .task_priority = 5,
        .stack_size = 4096, .core_id = 0, .callback = msc_event,
    };
    ESP_ERROR_CHECK(msc_host_install(&msc));
    ESP_ERROR_CHECK(xTaskCreate(command_task, "commands", 3072, NULL, 3, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_LOGI(TAG, "MSC_HOST_READY");
    show_help();
    for (;;) {
        probe_event_t event;
        if (xQueueReceive(events, &event, pdMS_TO_TICKS(5000)) != pdTRUE) {
            show_status();
            continue;
        }
        if (event.kind == COMMAND) {
            run_command(event.data.command);
        } else if (event.kind == CONNECTED) {
            if (device || cleanup_failed) {
                ESP_LOGW(TAG, "CONNECT ignored; single-device probe, check status");
                continue;
            }
            int64_t start = esp_timer_get_time();
            if (!result("install_device", msc_host_install_device(event.data.address, &device), start)) continue;
            device_address = event.data.address;
            show_media();
            mount_media();
        } else if (event.kind == DISCONNECTED && event.data.handle == device) {
            ESP_LOGI(TAG, "DISCONNECTED address=%u", device_address);
            if (!unmount_media()) continue;
            int64_t start = esp_timer_get_time();
            if (result("uninstall_device", msc_host_uninstall_device(device), start)) {
                device = NULL;
                device_address = 0;
            } else {
                cleanup_failed = true;
                ESP_LOGE(TAG, "CLEANUP_FAILED; stop testing and reset");
            }
        }
    }
}
