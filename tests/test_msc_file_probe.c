#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../src/msc_file_probe.h"

static const char payload[] = "FNK0099 USB MSC write probe v1\n";

static void assert_contents(const char *path)
{
    FILE *file = fopen(path, "rb");
    assert(file);
    char data[sizeof(payload)];
    size_t length = fread(data, 1, sizeof(data), file);
    assert(!ferror(file));
    assert(fclose(file) == 0);
    assert(length == sizeof(payload) - 1);
    assert(memcmp(data, payload, length) == 0);
}

int main(void)
{
    char directory[] = ".local/msc-write-test-XXXXXX";
    assert(mkdtemp(directory));
    char path[128], missing[128];
    snprintf(path, sizeof(path), "%s/WRITE.TXT", directory);
    snprintf(missing, sizeof(missing), "%s/missing/WRITE.TXT", directory);

    msc_file_write_result_t result = msc_create_synced_file(path, payload, sizeof(payload) - 1);
    assert(result.error == 0 && result.close_error == 0);
    assert(result.bytes_written == sizeof(payload) - 1);
    assert(strcmp(result.operation, "complete") == 0);
    assert_contents(path);

    const char replacement[] = "Existing file must survive";
    result = msc_create_synced_file(path, replacement, sizeof(replacement) - 1);
    assert(result.error == EEXIST && result.close_error == 0 && result.bytes_written == 0);
    assert(strcmp(result.operation, "open") == 0);
    assert_contents(path);

    result = msc_create_synced_file(missing, payload, sizeof(payload) - 1);
    assert(result.error == ENOENT && result.close_error == 0 && result.bytes_written == 0);
    assert(strcmp(result.operation, "open") == 0);

    assert(unlink(path) == 0);
    assert(rmdir(directory) == 0);
    puts("New file sync/close/reopen, existing-file preservation and missing-parent failure: PASS (host filesystem)");
}
