#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HID_INPUT_MAX_FIELDS 48
#define HID_INPUT_MAX_REPORTS 16
#define HID_INPUT_KEY_BYTES 32

typedef enum { HID_INPUT_NONE, HID_INPUT_KEYBOARD, HID_INPUT_MOUSE } hid_input_kind_t;
typedef enum { HID_FIELD_KEY_BITS, HID_FIELD_KEY_ARRAY, HID_FIELD_BUTTON, HID_FIELD_X,
               HID_FIELD_Y, HID_FIELD_WHEEL, HID_FIELD_PAN } hid_input_field_kind_t;
typedef struct {
    uint16_t offset, count, usage;
    uint8_t size, report_id, kind;
    int32_t minimum, maximum;
} hid_input_field_t;
typedef struct {
    uint16_t bits;
    uint8_t id, kind;
} hid_input_report_t;
typedef struct {
    hid_input_field_t fields[HID_INPUT_MAX_FIELDS];
    hid_input_report_t reports[HID_INPUT_MAX_REPORTS];
    uint8_t field_count, report_count;
    bool report_ids;
} hid_input_layout_t;
typedef struct {
    hid_input_kind_t kind;
    uint8_t keys[HID_INPUT_KEY_BYTES]; // Keyboard usage IDs, including modifiers E0-E7.
    uint32_t buttons;
    int32_t x, y, wheel, pan;
} hid_input_value_t;
typedef enum { HID_INPUT_OK, HID_INPUT_IGNORED, HID_INPUT_INVALID, HID_INPUT_ROLLOVER } hid_input_result_t;

// Bounded parser for keyboard arrays/bitmaps and relative mouse fields.
// Unsupported application collections are counted for framing but not decoded.
bool hid_input_parse(const uint8_t *descriptor, size_t length, hid_input_layout_t *layout);
hid_input_result_t hid_input_decode(const hid_input_layout_t *layout, const uint8_t *data,
                                    size_t length, hid_input_value_t *value);
static inline bool hid_input_key(const uint8_t *keys, unsigned usage)
{
    return usage < 256 && (keys[usage / 8] & (1u << (usage % 8))) != 0;
}
