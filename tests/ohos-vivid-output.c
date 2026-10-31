// Compile the production output transition functions from a temporary include.
// Only the surrounding platform/context types and native setters are mocked.
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <native_buffer/native_buffer.h>
#include "video/out/ohos_vivid.h"

enum { PL_COLOR_TRC_SRGB, PL_COLOR_TRC_PQ, PL_COLOR_TRC_HLG };
enum { OH_COLORSPACE_DISPLAY_SRGB, OH_COLORSPACE_DISPLAY_BT2020_PQ,
       OH_COLORSPACE_DISPLAY_BT2020_HLG };
typedef int OH_NativeBuffer_ColorSpace;
struct pl_color_space { int transfer, primaries; float negotiated_marker; };
typedef struct window {
    OH_NativeBuffer_MetadataType type;
    OH_NativeBuffer_ColorSpace color;
    uint8_t dynamic[OHOS_VIVID_MAX_METADATA];
    size_t dynamic_size;
} OHNativeWindow;
struct vo_ohos_state {
    OHNativeWindow *native_window;
    void *vpe;
    bool color_space_valid, metadata_type_valid, static_metadata_valid;
    OH_NativeBuffer_ColorSpace color_space;
    OH_NativeBuffer_MetadataType metadata_type;
    uint64_t vivid_epoch;
    bool vivid_ready, vivid_failed, vivid_restore_failed;
    unsigned vivid_audits;
    uint32_t vivid_expected;
    struct pl_color_space output_color;
};
struct vo { struct vo_ohos_state *ohos; };

#define MP_ERR(...) ((void)0)
#define MP_WARN(...) ((void)0)
#define MP_INFO(...) ((void)0)
static bool is_hdr(const struct pl_color_space *c)
{ return c->transfer == PL_COLOR_TRC_PQ || c->transfer == PL_COLOR_TRC_HLG; }
static OH_NativeBuffer_ColorSpace get_color_space(const struct pl_color_space *c)
{
    return c->transfer == PL_COLOR_TRC_PQ ? OH_COLORSPACE_DISPLAY_BT2020_PQ :
           c->transfer == PL_COLOR_TRC_HLG ? OH_COLORSPACE_DISPLAY_BT2020_HLG :
                                           OH_COLORSPACE_DISPLAY_SRGB;
}

static OHNativeWindow window;
static struct vo_ohos_state ctx;
static struct vo vo;
static struct ohos_vivid_frame frame;
static struct pl_color_space source = { PL_COLOR_TRC_PQ, 2020, 0.5f };
static struct pl_color_space negotiated = { PL_COLOR_TRC_HLG, 2020, 0.75f };
static bool fail_dynamic, fail_static, fail_vivid_type, fail_basic_type;
static int fail_color = -1;
static unsigned native_calls, restored;
static char operations[128];

static void record(char operation)
{
    assert(native_calls + 1 < sizeof(operations));
    operations[native_calls++] = operation;
    operations[native_calls] = 0;
}
static void clear_calls(void)
{
    native_calls = restored = 0;
    memset(operations, 0, sizeof(operations));
}
static int OH_NativeWindow_SetMetadataValue(OHNativeWindow *w,
    OH_NativeBuffer_MetadataKey key, int32_t size, uint8_t *data)
{
    assert(w == &window && data && size > 0);
    if (key == OH_HDR_DYNAMIC_METADATA) {
        record('D');
        if (fail_dynamic) return -10;
        assert((size_t)size <= sizeof(w->dynamic));
        memcpy(w->dynamic, data, size);
        w->dynamic_size = size;
    } else if (key == OH_HDR_STATIC_METADATA) {
        record('S');
        if (fail_static) return -11;
        assert((size_t)size == sizeof(OH_NativeBuffer_StaticMetadata));
    } else {
        assert(key == OH_HDR_METADATA_TYPE && (size_t)size == sizeof(w->type));
        OH_NativeBuffer_MetadataType type;
        memcpy(&type, data, sizeof(type));
        record(type == OH_VIDEO_HDR_VIVID ? 'V' : 'B');
        if (type == OH_VIDEO_HDR_VIVID ? fail_vivid_type : fail_basic_type)
            return -12;
        w->type = type;
    }
    return 0;
}
static int OH_NativeWindow_SetColorSpace(OHNativeWindow *w, OH_NativeBuffer_ColorSpace color)
{
    assert(w == &window);
    record('C');
    if (color == fail_color) return -13;
    w->color = color;
    return 0;
}
static void set_output_color(struct vo_ohos_state *s, struct pl_color_space color)
{
    // The surrounding renderer's existing negotiation is external to this
    // state-machine test. Verify that restore retains that negotiated color.
    assert(!memcmp(&color, &negotiated, sizeof(color)));
    s->output_color = color;
    restored++;
}

// Generated from ohos_common.c on each test run, never maintained as a copy.
#include "vivid-output.inc"

static void fixture(void)
{
    window = (OHNativeWindow){ .type = OH_VIDEO_HDR_HLG,
        .color = OH_COLORSPACE_DISPLAY_BT2020_HLG };
    ctx = (struct vo_ohos_state){ .native_window = &window, .vivid_epoch = 7,
        .output_color = negotiated };
    vo = (struct vo){ &ctx };
    frame = (struct ohos_vivid_frame){ .valid = true, .token = 0x1234,
        .epoch = 7, .pts = 1.5, .dynamic_size = 2,
        .static_size = sizeof(OH_NativeBuffer_StaticMetadata) };
    frame.dynamic[0] = 0x26;
    frame.dynamic[1] = 0x04;
    fail_dynamic = fail_static = fail_vivid_type = fail_basic_type = false;
    fail_color = -1;
    clear_calls();
}
static int prepare(uintptr_t token, bool eligible)
{ return vo_ohos_vivid_prepare(&vo, &frame, token, eligible, source); }
static void passed(void)
{
    assert(prepare(frame.token, true) == 1);
    assert(ctx.vivid_ready && !ctx.vivid_restore_failed);
    assert(window.type == OH_VIDEO_HDR_VIVID);
    assert(window.color == OH_COLORSPACE_DISPLAY_BT2020_PQ);
    assert(window.dynamic_size == frame.dynamic_size);
    assert(!memcmp(window.dynamic, frame.dynamic, frame.dynamic_size));
    assert(ctx.vivid_expected == ohos_vivid_hash(frame.dynamic, frame.dynamic_size));
}
static void basic(void)
{
    assert(!ctx.vivid_ready && !ctx.vivid_restore_failed);
    assert(window.type == OH_VIDEO_HDR_HLG);
    assert(window.color == OH_COLORSPACE_DISPLAY_BT2020_HLG);
    assert(!memcmp(&ctx.output_color, &negotiated, sizeof(negotiated)));
}
int main(void)
{
    fixture();
    assert(prepare(frame.token, false) == 0);
    assert(!native_calls && !restored);
    assert(!memcmp(&ctx.output_color, &negotiated, sizeof(negotiated)));
    basic(); // Ineligible frames preserve negotiated output instead of source PQ.
    assert(prepare(frame.token, false) == 0 && !native_calls);

    fixture(); passed();
    assert(!strcmp(operations, "DSCV") && !restored);
    clear_calls(); passed(); // Repeated presentation reuses the same metadata.
    assert(!strcmp(operations, "DSCV"));

    fixture(); assert(prepare(0x5678, true) == 0 && !native_calls);
    fixture(); frame.epoch--; assert(prepare(frame.token, true) == 0 && !native_calls);
    fixture(); passed(); clear_calls();
    assert(prepare(0x5678, true) == 0);
    basic(); assert(!strcmp(operations, "BC") && restored == 1);
    fixture(); passed(); clear_calls(); frame.epoch--;
    assert(prepare(frame.token, true) == 0); basic();

    fixture(); frame.static_size = 0;
    assert(prepare(frame.token, true) == 0 && !native_calls); // PQ needs static metadata.
    source.transfer = PL_COLOR_TRC_HLG;
    assert(prepare(frame.token, true) == 1 && ctx.vivid_ready); // HLG may omit it.
    assert(!strcmp(operations, "DCV"));
    source.transfer = PL_COLOR_TRC_PQ;

    // A device without the dynamic setter must retain ordinary HDR even when
    // its metadata-type setter is also unsupported. No Vivid state was applied.
    fixture(); fail_dynamic = fail_basic_type = true;
    assert(prepare(frame.token, true) == 0 && ctx.vivid_failed);
    assert(!strcmp(operations, "D") && !restored);
    basic(); clear_calls();
    assert(prepare(frame.token, true) == 0 && !native_calls);

    for (int failure = 0; failure < 4; failure++) {
        fixture(); passed(); clear_calls();
        fail_dynamic = failure == 0; fail_static = failure == 1;
        fail_color = failure == 2 ? OH_COLORSPACE_DISPLAY_BT2020_PQ : -1;
        fail_vivid_type = failure == 3;
        assert(prepare(frame.token, true) == 0);
        basic(); assert(ctx.vivid_failed && restored == 1);
        clear_calls(); // A native staging failure disables further passthrough.
        assert(prepare(frame.token, true) == 0 && !native_calls);
    }

    // Restoration failure cannot be treated as a safe basic-HDR fallback.
    fixture(); passed(); clear_calls(); fail_basic_type = true;
    assert(prepare(0x5678, true) == -1);
    assert(ctx.vivid_restore_failed && !ctx.vivid_ready && !restored);
    assert(window.type == OH_VIDEO_HDR_VIVID && !strcmp(operations, "B"));
    fail_basic_type = false; clear_calls();
    assert(prepare(0x5678, true) == 0); basic();

    fixture(); passed(); clear_calls(); fail_color = OH_COLORSPACE_DISPLAY_BT2020_HLG;
    assert(prepare(frame.token, false) == -1);
    assert(ctx.vivid_restore_failed && !restored);
    assert(window.type == OH_VIDEO_HDR_HLG && window.color == OH_COLORSPACE_DISPLAY_BT2020_PQ);
    fail_color = -1; clear_calls();
    assert(prepare(frame.token, false) == 0); basic();

    // Even a partial staging error followed by a failed restore is unsafe.
    fixture(); fail_static = true; fail_basic_type = true;
    assert(prepare(frame.token, true) == -1 && ctx.vivid_restore_failed);
    fail_static = fail_basic_type = false; clear_calls();
    assert(prepare(frame.token, true) == 0); basic(); // Failed staging remains disabled.

    fixture(); passed(); ctx.vivid_audits = 9; clear_calls();
    vo_ohos_vivid_reset(&vo);
    assert(vo_ohos_vivid_epoch(&vo) == 8 && ctx.vivid_audits == 0 && !ctx.vivid_expected);
    basic(); clear_calls();
    assert(prepare(frame.token, true) == 0 && !native_calls); // Old seek epoch rejected.
    frame.epoch = 8; passed();
    fail_basic_type = true;
    vo_ohos_vivid_reset(&vo);
    assert(ctx.vivid_restore_failed && !ctx.vivid_ready);
    assert(prepare(frame.token, false) == -1);
    fail_basic_type = false;
    assert(prepare(frame.token, false) == 0); basic();

    fixture(); vo.ohos = NULL;
    assert(prepare(frame.token, true) == 0 && vo_ohos_vivid_epoch(&vo) == 0);
    vo_ohos_vivid_reset(&vo);
    puts("HDR Vivid production output transition, fallback and reset checks passed");
    return 0;
}
