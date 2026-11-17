#pragma once

#include <stddef.h>

#define MSC_PATTERN_FILE_BYTES (1024u * 1024u)
#define MSC_PATTERN_SHA256 "f395c263d052f99409f2ac4837dfcc5f5c89270e9d347a8306043155dd08ecf7"

typedef struct {
    size_t bytes_written;
    int error;
    int close_error;
    const char *operation;
} msc_file_write_result_t;

// Exclusively create a new file, write all bytes, fsync, then close. Existing
// files are never replaced. Failed/partial new files are retained for inspection.
msc_file_write_result_t msc_create_synced_file(const char *path, const void *data, size_t length);

// Stream the deterministic v1 pattern through a caller-owned scratch buffer.
// Only the scratch buffer is allocated by the caller, never the full file.
msc_file_write_result_t msc_create_pattern_file(const char *path, void *buffer, size_t buffer_size);
