#include "hid_input.h"
#include <limits.h>
#include <string.h>

#define STACK_DEPTH 8
#define LOCAL_USAGES 32

typedef struct {
    uint32_t page, size, count;
    int32_t minimum, maximum;
    uint8_t id;
} global_t;
typedef struct {
    uint32_t usages[LOCAL_USAGES], minimum, maximum;
    unsigned count;
    bool have_minimum, have_maximum;
} local_t;

static uint32_t unsigned_item(const uint8_t *p, unsigned size)
{
    uint32_t value = 0;
    for (unsigned i = 0; i < size; ++i) value |= (uint32_t)p[i] << (8 * i);
    return value;
}

static int32_t signed_item(uint32_t value, unsigned size)
{
    if (!size) return 0;
    unsigned bits = size * 8;
    if (bits == 32) return (int32_t)value;
    return (int32_t)(value & ((1u << (bits - 1)) - 1)) -
           (int32_t)(value & (1u << (bits - 1)));
}

static uint32_t local_usage(const local_t *local, unsigned index)
{
    if (index < local->count) return local->usages[index];
    if (local->have_minimum && local->have_maximum && local->maximum >= local->minimum &&
        index <= local->maximum - local->minimum) return local->minimum + index;
    return UINT32_MAX;
}

static hid_input_report_t *report_for(hid_input_layout_t *layout, uint8_t id)
{
    for (unsigned i = 0; i < layout->report_count; ++i)
        if (layout->reports[i].id == id) return &layout->reports[i];
    if (layout->report_count == HID_INPUT_MAX_REPORTS) return NULL;
    hid_input_report_t *report = &layout->reports[layout->report_count++];
    *report = (hid_input_report_t){.id = id};
    return report;
}

static bool add_field(hid_input_layout_t *layout, hid_input_report_t *report, const global_t *g,
                      uint16_t offset, uint16_t count, uint16_t usage, hid_input_field_kind_t kind,
                      hid_input_kind_t app)
{
    if (layout->field_count == HID_INPUT_MAX_FIELDS || (report->kind && report->kind != app)) return false;
    layout->fields[layout->field_count++] = (hid_input_field_t){
        .offset = offset, .count = count, .usage = usage, .size = (uint8_t)g->size,
        .report_id = g->id, .kind = kind, .minimum = g->minimum, .maximum = g->maximum,
    };
    report->kind = app;
    return true;
}

static bool input_fields(hid_input_layout_t *layout, const global_t *g, const local_t *local,
                         uint32_t flags, hid_input_kind_t app)
{
    if (!g->size || g->size > 32 || !g->count || g->count > 512 || g->size * g->count > 512) return false;
    hid_input_report_t *report = report_for(layout, g->id);
    if (!report || report->bits + g->size * g->count > 512) return false;
    uint16_t offset = report->bits;
    report->bits += g->size * g->count;
    if ((flags & 1) || app == HID_INPUT_NONE) return true; // Constant padding / unsupported collection.
    bool variable = (flags & 2) != 0;
    bool relative = (flags & 4) != 0;
    if (app == HID_INPUT_KEYBOARD) {
        if (g->page != 7 || relative) return false;
        if (!variable) {
            if (g->size != 8 || g->count > 32 || g->minimum != 0 || g->maximum > 255) return false;
            return add_field(layout, report, g, offset, (uint16_t)g->count, 0, HID_FIELD_KEY_ARRAY, app);
        }
        if (g->size != 1 || g->minimum != 0 || g->maximum != 1 || g->count > 256) return false;
        for (unsigned i = 0; i < g->count; ++i) {
            uint32_t usage = local_usage(local, i);
            if (usage > 255 || !add_field(layout, report, g, offset + i, 1, (uint16_t)usage,
                                          HID_FIELD_KEY_BITS, app)) return false;
        }
        return true;
    }
    if (!variable) return false;
    for (unsigned i = 0; i < g->count; ++i) {
        uint32_t usage = local_usage(local, i);
        hid_input_field_kind_t kind;
        if (g->page == 9 && usage >= 1 && usage <= 32 && g->size == 1 && !relative) kind = HID_FIELD_BUTTON;
        else if (g->page == 1 && usage == 0x30 && relative) kind = HID_FIELD_X;
        else if (g->page == 1 && usage == 0x31 && relative) kind = HID_FIELD_Y;
        else if (g->page == 1 && usage == 0x38 && relative) kind = HID_FIELD_WHEEL;
        else if (g->page == 0x0c && usage == 0x238 && relative) kind = HID_FIELD_PAN;
        else return false; // Never guess an unfamiliar mouse layout.
        if (!add_field(layout, report, g, offset + i * g->size, 1, (uint16_t)usage, kind, app)) return false;
    }
    return true;
}

bool hid_input_parse(const uint8_t *descriptor, size_t length, hid_input_layout_t *layout)
{
    if (!layout) return false;
    memset(layout, 0, sizeof(*layout));
    if (!descriptor || !length || length > 2048) return false;
    global_t g = {0}, stack[STACK_DEPTH];
    local_t local = {0};
    hid_input_kind_t app = HID_INPUT_NONE, collections[STACK_DEPTH];
    unsigned depth = 0, collection_depth = 0;
    for (size_t pos = 0; pos < length;) {
        uint8_t prefix = descriptor[pos++];
        if (prefix == 0xfe) return false; // Long items are outside this parser's supported subset.
        unsigned size = (prefix & 3) == 3 ? 4 : prefix & 3;
        unsigned type = (prefix >> 2) & 3, tag = prefix >> 4;
        if (size > length - pos) return false;
        uint32_t u = unsigned_item(descriptor + pos, size);
        int32_t s = signed_item(u, size);
        pos += size;
        if (type == 1) {
            switch (tag) {
            case 0: g.page = u; break;
            case 1: g.minimum = s; break;
            case 2: if (g.minimum >= 0 && u > INT32_MAX) return false;
                    g.maximum = g.minimum < 0 ? s : (int32_t)u; break;
            case 7: g.size = u; break;
            case 8: if (!u || u > 255) return false;
                    g.id = (uint8_t)u; layout->report_ids = true; break;
            case 9: g.count = u; break;
            case 10: if (depth == STACK_DEPTH) return false; stack[depth++] = g; break;
            case 11: if (!depth) return false; g = stack[--depth]; break;
            default: break; // Physical bounds, unit and exponent do not affect input framing.
            }
        } else if (type == 2) {
            if (size == 4 && (u >> 16) != g.page) return false;
            if (size == 4) u &= 0xffff;
            if (tag == 0) {
                if (local.count == LOCAL_USAGES) return false;
                local.usages[local.count++] = u;
            } else if (tag == 1) { local.minimum = u; local.have_minimum = true; }
            else if (tag == 2) { local.maximum = u; local.have_maximum = true; }
            else return false; // Delimiter/alternate usage sets need explicit support.
        } else if (type == 0) {
            if (tag == 10) {
                if (collection_depth == STACK_DEPTH) return false;
                collections[collection_depth++] = app;
                if (u == 1) { // Application collection.
                    uint32_t usage = local_usage(&local, 0);
                    app = g.page == 1 && usage == 6 ? HID_INPUT_KEYBOARD :
                          g.page == 1 && usage == 2 ? HID_INPUT_MOUSE : HID_INPUT_NONE;
                }
            } else if (tag == 12) {
                if (!collection_depth) return false;
                app = collections[--collection_depth];
            } else if (tag == 8) {
                if (!collection_depth || g.minimum > g.maximum || !input_fields(layout, &g, &local, u, app)) return false;
            } else if (tag != 9 && tag != 11) return false; // Output and Feature have separate offsets.
            memset(&local, 0, sizeof(local));
        } else return false;
    }
    if (depth || collection_depth || !layout->report_count) return false;
    for (unsigned i = 0; i < layout->report_count; ++i) {
        const hid_input_report_t *r = &layout->reports[i];
        if ((layout->report_ids && !r->id) || (r->bits + 7) / 8 + layout->report_ids > 64) return false;
    }
    return true;
}

static uint32_t bits_at(const uint8_t *data, uint16_t offset, unsigned size)
{
    uint32_t value = 0;
    for (unsigned i = 0; i < size; ++i)
        value |= (uint32_t)((data[(offset + i) / 8] >> ((offset + i) % 8)) & 1u) << i;
    return value;
}

hid_input_result_t hid_input_decode(const hid_input_layout_t *layout, const uint8_t *data,
                                    size_t length, hid_input_value_t *value)
{
    if (!layout || !data || !length || !value || !layout->report_count) return HID_INPUT_INVALID;
    uint8_t id = layout->report_ids ? data[0] : 0;
    const hid_input_report_t *report = NULL;
    for (unsigned i = 0; i < layout->report_count; ++i)
        if (layout->reports[i].id == id) { report = &layout->reports[i]; break; }
    if (!report) return HID_INPUT_IGNORED;
    if (length != (report->bits + 7u) / 8u + layout->report_ids) return HID_INPUT_INVALID;
    if (!report->kind) return HID_INPUT_IGNORED;
    hid_input_value_t decoded = {.kind = report->kind};
    const uint8_t *payload = data + layout->report_ids;
    for (unsigned i = 0; i < layout->field_count; ++i) {
        const hid_input_field_t *f = &layout->fields[i];
        if (f->report_id != id) continue;
        for (unsigned j = 0; j < f->count; ++j) {
            uint32_t raw = bits_at(payload, f->offset + j * f->size, f->size);
            int32_t n = f->minimum < 0 ? signed_item(raw, 4) : (int32_t)raw;
            if (f->minimum < 0 && f->size < 32) {
                n = (int32_t)(raw & ((1u << (f->size - 1)) - 1)) -
                    (int32_t)(raw & (1u << (f->size - 1)));
            }
            if (n < f->minimum || n > f->maximum) return HID_INPUT_INVALID;
            switch ((hid_input_field_kind_t)f->kind) {
            case HID_FIELD_KEY_ARRAY:
                if (raw >= 1 && raw <= 3) return HID_INPUT_ROLLOVER;
                if (raw) decoded.keys[raw / 8] |= 1u << (raw % 8);
                break;
            case HID_FIELD_KEY_BITS:
                if (raw) decoded.keys[f->usage / 8] |= 1u << (f->usage % 8);
                break;
            case HID_FIELD_BUTTON: if (raw) decoded.buttons |= 1u << (f->usage - 1); break;
            case HID_FIELD_X: decoded.x = n; break;
            case HID_FIELD_Y: decoded.y = n; break;
            case HID_FIELD_WHEEL: decoded.wheel = n; break;
            case HID_FIELD_PAN: decoded.pan = n; break;
            }
        }
    }
    *value = decoded;
    return HID_INPUT_OK;
}
