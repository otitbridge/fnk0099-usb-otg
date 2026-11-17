#include "msc_file_probe.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

static void write_all(int fd, const void *data, size_t length, msc_file_write_result_t *result)
{
    const uint8_t *bytes = data;
    size_t offset = 0;
    result->operation = "write";
    while (offset < length) {
        ssize_t written = write(fd, bytes + offset, length - offset);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) {
            result->error = written < 0 ? errno : EIO;
            break;
        }
        offset += (size_t)written;
        result->bytes_written += (size_t)written;
    }
}

static msc_file_write_result_t sync_and_close(int fd, msc_file_write_result_t result)
{
    if (!result.error) {
        result.operation = "fsync";
        int rc;
        do { rc = fsync(fd); } while (rc < 0 && errno == EINTR);
        if (rc < 0) result.error = errno;
    }
    // Always attempt close, preserving the first error. Retrying close after
    // EINTR can close an unrelated descriptor on some platforms, so do not retry.
    if (close(fd) < 0) {
        result.close_error = errno;
        if (!result.error) {
            result.operation = "close";
            result.error = result.close_error;
        }
    }
    if (!result.error) result.operation = "complete";
    return result;
}

msc_file_write_result_t msc_create_synced_file(const char *path, const void *data, size_t length)
{
    msc_file_write_result_t result = {.operation = "open"};
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd < 0) {
        result.error = errno;
        return result;
    }
    write_all(fd, data, length, &result);
    return sync_and_close(fd, result);
}

msc_file_write_result_t msc_create_pattern_file(const char *path, void *buffer, size_t buffer_size)
{
    msc_file_write_result_t result = {.operation = "arguments"};
    if (!buffer || !buffer_size) {
        result.error = EINVAL;
        return result;
    }
    result.operation = "open";
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd < 0) {
        result.error = errno;
        return result;
    }
    while (result.bytes_written < MSC_PATTERN_FILE_BYTES && !result.error) {
        size_t remaining = MSC_PATTERN_FILE_BYTES - result.bytes_written;
        size_t length = remaining < buffer_size ? remaining : buffer_size;
        uint8_t *bytes = buffer;
        for (size_t j = 0; j < length; ++j) {
            uint32_t i = (uint32_t)(result.bytes_written + j);
            bytes[j] = (uint8_t)(i * 73u + (i >> 8) * 151u + (i >> 16) * 199u + 0x5au);
        }
        write_all(fd, buffer, length, &result);
    }
    return sync_and_close(fd, result);
}
