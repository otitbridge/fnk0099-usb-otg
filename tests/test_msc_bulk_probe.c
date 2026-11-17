#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "../src/msc_file_probe.h"

int main(int argc, char **argv)
{
    assert(argc == 2);
    // Exercise a final short chunk, not just a 4096-byte-aligned file size.
    unsigned char buffer[4093];
    msc_file_write_result_t result = msc_create_pattern_file(argv[1], buffer, 0);
    assert(result.error == EINVAL && result.bytes_written == 0);
    assert(strcmp(result.operation, "arguments") == 0);
    assert(access(argv[1], F_OK) != 0 && errno == ENOENT);

    result = msc_create_pattern_file(argv[1], buffer, sizeof(buffer));
    assert(result.error == 0 && result.close_error == 0);
    assert(result.bytes_written == MSC_PATTERN_FILE_BYTES);
    assert(strcmp(result.operation, "complete") == 0);

    memset(buffer, 0, sizeof(buffer));
    result = msc_create_pattern_file(argv[1], buffer, sizeof(buffer));
    assert(result.error == EEXIST && result.close_error == 0 && result.bytes_written == 0);
    assert(strcmp(result.operation, "open") == 0);
    puts("1 MiB creation with a partial final chunk, collision refusal, invalid-buffer rejection: PASS (host)");
}
