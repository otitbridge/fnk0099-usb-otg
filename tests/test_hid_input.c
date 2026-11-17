#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/hid_input.h"

static hid_input_layout_t layout;
static hid_input_value_t value;

static void fixture(const char *path)
{
    uint8_t bytes[2048];
    size_t length = 0;
    unsigned n;
    FILE *file = fopen(path, "r");
    assert(file);
    while (fscanf(file, "%2x", &n) == 1) { assert(length < sizeof(bytes)); bytes[length++] = (uint8_t)n; }
    assert(feof(file));
    assert(fclose(file) == 0);
    assert(hid_input_parse(bytes, length, &layout));
}

static void keyboard(void)
{
    fixture("tests/fixtures/hid/bolt-keyboard-report-desc.hex");
    assert(!layout.report_ids && layout.report_count == 1 && layout.reports[0].bits == 64);
    uint8_t data[8] = {0};
    assert(hid_input_decode(&layout, data, 8, &value) == HID_INPUT_OK);
    assert(value.kind == HID_INPUT_KEYBOARD);
    for (unsigned i = 0; i < 256; ++i) assert(!hid_input_key(value.keys, i));
    data[0] = 0x82; data[2] = 4; data[3] = 0x4f;
    assert(hid_input_decode(&layout, data, 8, &value) == HID_INPUT_OK);
    assert(hid_input_key(value.keys, 0xe1) && hid_input_key(value.keys, 0xe7));
    assert(hid_input_key(value.keys, 4) && hid_input_key(value.keys, 0x4f));
    data[2] = 0;
    assert(hid_input_decode(&layout, data, 8, &value) == HID_INPUT_OK);
    assert(!hid_input_key(value.keys, 4) && hid_input_key(value.keys, 0x4f));
    assert(hid_input_key(value.keys, 0xe1));
    hid_input_value_t before = value;
    for (unsigned error = 1; error <= 3; ++error) {
        data[2] = (uint8_t)error;
        assert(hid_input_decode(&layout, data, 8, &value) == HID_INPUT_ROLLOVER);
        assert(memcmp(&before, &value, sizeof(value)) == 0);
    }
    assert(hid_input_decode(&layout, data, 7, &value) == HID_INPUT_INVALID);
    assert(hid_input_decode(&layout, data, 0, &value) == HID_INPUT_INVALID);
    memset(data, 0, sizeof(data));
    assert(hid_input_decode(&layout, data, 8, &value) == HID_INPUT_OK);
    for (unsigned i = 0; i < 256; ++i) assert(!hid_input_key(value.keys, i));
}

static void mouse(void)
{
    fixture("tests/fixtures/hid/bolt-mouse-report-desc.hex");
    assert(layout.report_ids && layout.report_count == 4);
    uint8_t data[10] = {2, 5, 0x80, 0x2c, 1, 0xd4, 0xfe, 0x7f, 0x81};
    assert(hid_input_decode(&layout, data, 9, &value) == HID_INPUT_OK);
    assert(value.kind == HID_INPUT_MOUSE && value.buttons == 0x8005);
    assert(value.x == 300 && value.y == -300 && value.wheel == 127 && value.pan == -127);
    data[3] = 1; data[4] = 0x80; data[5] = 0xff; data[6] = 0x7f;
    assert(hid_input_decode(&layout, data, 9, &value) == HID_INPUT_OK);
    assert(value.x == -32767 && value.y == 32767);
    hid_input_value_t before = value;
    data[3] = 0; // -32768 is below this descriptor's logical minimum.
    assert(hid_input_decode(&layout, data, 9, &value) == HID_INPUT_INVALID);
    assert(memcmp(&before, &value, sizeof(value)) == 0);
    assert(hid_input_decode(&layout, data, 8, &value) == HID_INPUT_INVALID);
    assert(hid_input_decode(&layout, data, 10, &value) == HID_INPUT_INVALID);
    data[0] = 0x10;
    assert(hid_input_decode(&layout, data, 9, &value) == HID_INPUT_IGNORED);
    data[0] = 3; // Consumer Control is framed but does not overwrite mouse state.
    assert(hid_input_decode(&layout, data, 5, &value) == HID_INPUT_IGNORED);
    assert(hid_input_decode(&layout, data, 4, &value) == HID_INPUT_INVALID);
    assert(memcmp(&before, &value, sizeof(value)) == 0);
    data[0] = 2; memset(data + 1, 0, 8);
    assert(hid_input_decode(&layout, data, 9, &value) == HID_INPUT_OK);
    assert(!value.buttons && !value.x && !value.y && !value.wheel && !value.pan);
}

static void framing(void)
{
    // Three-button, signed 8-bit relative mouse, without a Report ID.
    const uint8_t descriptor[] = {
        0x05,1,0x09,2,0xa1,1,0x05,9,0x19,1,0x29,3,0x15,0,0x25,1,
        0x75,1,0x95,3,0x81,2,0x75,5,0x95,1,0x81,3,
        0x05,1,0x09,0x30,0x09,0x31,0x15,0x81,0x25,0x7f,0x75,8,0x95,2,0x81,6,0xc0,
    };
    assert(hid_input_parse(descriptor, sizeof(descriptor), &layout));
    uint8_t data[] = {5,0x81,0x7f};
    assert(hid_input_decode(&layout, data, sizeof(data), &value) == HID_INPUT_OK);
    assert(value.buttons == 5 && value.x == -127 && value.y == 127 && !value.wheel);
    assert(!hid_input_parse(descriptor, sizeof(descriptor) - 1, &layout));
    const uint8_t truncated[] = {0x27,1,2};
    assert(!hid_input_parse(truncated, sizeof(truncated), &layout));
    const uint8_t pop[] = {0xb4};
    assert(!hid_input_parse(pop, sizeof(pop), &layout));
    const uint8_t end[] = {0xc0};
    assert(!hid_input_parse(end, sizeof(end), &layout));
    const uint8_t huge[] = {0x05,1,0x09,6,0xa1,1,0x75,32,0x96,0,2,0x81,0,0xc0};
    assert(!hid_input_parse(huge, sizeof(huge), &layout));
    assert(!hid_input_parse(NULL,0,&layout));
}

static void malformed_bounds(void)
{
    // Exercise truncation and size/count mutations under ASan/UBSan. Accepted
    // layouts are also decoded at their declared size and at one byte short.
    const char *paths[] = {
        "tests/fixtures/hid/bolt-keyboard-report-desc.hex",
        "tests/fixtures/hid/bolt-mouse-report-desc.hex",
    };
    for (unsigned p = 0; p < 2; ++p) {
        uint8_t original[256], mutated[256], report[64] = {0};
        size_t length = 0;
        unsigned n;
        FILE *file = fopen(paths[p], "r");
        assert(file);
        while (fscanf(file, "%2x", &n) == 1) { assert(length < sizeof(original)); original[length++] = (uint8_t)n; }
        assert(fclose(file) == 0);
        for (size_t cut = 0; cut <= length; ++cut) (void)hid_input_parse(original, cut, &layout);
        const uint8_t changes[] = {0,0xff,0xfe,0x80};
        for (size_t i = 0; i < length; ++i) for (unsigned j = 0; j < sizeof(changes); ++j) {
            memcpy(mutated, original, length);
            mutated[i] = changes[j];
            if (!hid_input_parse(mutated, length, &layout)) continue;
            for (unsigned r = 0; r < layout.report_count; ++r) {
                size_t bytes = (layout.reports[r].bits + 7u) / 8u + layout.report_ids;
                assert(bytes <= sizeof(report));
                memset(report, 0, sizeof(report));
                if (layout.report_ids) report[0] = layout.reports[r].id;
                (void)hid_input_decode(&layout, report, bytes, &value);
                if (bytes) (void)hid_input_decode(&layout, report, bytes - 1, &value);
            }
        }
    }
}

int main(void)
{
    keyboard(); mouse(); framing(); malformed_bounds();
    puts("Captured Bolt descriptors, modifiers/partial release/rollover, signed mouse axes/buttons/wheels, report framing and malformed descriptors: PASS");
}
