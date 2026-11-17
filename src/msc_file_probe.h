#pragma once

#include <stddef.h>

typedef struct {
    size_t bytes_written;
    int error;
    int close_error;
    const char *operation;
} msc_file_write_result_t;

// Exclusively create a new file, write all bytes, fsync, then close. Existing
// files are never replaced. Failed/partial new files are retained for inspection.
msc_file_write_result_t msc_create_synced_file(const char *path, const void *data, size_t length);
