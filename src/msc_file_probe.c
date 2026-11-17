#include "msc_file_probe.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

msc_file_write_result_t msc_create_synced_file(const char *path, const void *data, size_t length)
{
    msc_file_write_result_t result = {.operation = "open"};
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd < 0) {
        result.error = errno;
        return result;
    }
    const uint8_t *bytes = data;
    result.operation = "write";
    while (result.bytes_written < length) {
        ssize_t written = write(fd, bytes + result.bytes_written, length - result.bytes_written);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) {
            result.error = written < 0 ? errno : EIO;
            break;
        }
        result.bytes_written += (size_t)written;
    }
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
