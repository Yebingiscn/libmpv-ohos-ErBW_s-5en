// Exercise the production VPE adapter with failure injection. Native API
// declarations come from the real SDK; only the vendor implementation is fake.
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdatomic.h>
#include "osdep/threads.h"
#include "osdep/timer.h"
#include <multimedia/video_processing_engine/video_processing.h>
#include <multimedia/player_framework/native_avformat.h>
#include <native_buffer/native_buffer.h>
#include "video/out/ohos_vpe.h"

const int32_t VIDEO_PROCESSING_TYPE_DETAIL_ENHANCER = 2;
const char *VIDEO_DETAIL_ENHANCER_PARAMETER_KEY_QUALITY_LEVEL = "quality";
enum { CREATE = 1, CALLBACK_CREATE, BIND_ERROR, BIND_OUTPUT, REGISTER, FORMAT,
       VALUE, PARAMETER, RESIZE, OUTPUT_FORMAT, OUTPUT_GET_USAGE, OUTPUT_USAGE,
       OUTPUT_COLOR, SURFACE, INPUT_SURFACE, INPUT_FORMAT, INPUT_GET_USAGE,
       INPUT_USAGE, INPUT_COLOR, START };
static int expected_format = NATIVEBUFFER_PIXEL_FMT_RGBA_8888;
static OH_NativeBuffer_ColorSpace expected_color = OH_COLORSPACE_DISPLAY_SRGB;
static int output_format, input_format;
static bool output_color_set, input_color_set;
static int fail_at, processors, formats, callbacks, inputs, running, quality;
static int stage, resize_count, destroy_error, width, height;
static atomic_int rendered, render_error;
static atomic_bool hold_render;
static atomic_int wakeups;
static void wake_vo(void *data) { atomic_fetch_add(&wakeups, 1); }
static atomic_int_fast64_t fake_time = 1000000000;
static _Thread_local bool in_callback;
int64_t mp_time_ns(void) { return atomic_load(&fake_time); }
static OH_VideoProcessingCallback_OnError error_cb;
static OH_VideoProcessingCallback_OnNewOutputBuffer output_cb;
static void *callback_data;
static int processor_token, window_token, callback_token, format_token;
#define PROCESSOR ((OH_VideoProcessing *)&processor_token)
#define WINDOW ((OHNativeWindow *)&window_token)
#define INPUT ((OHNativeWindow *)&inputs)
static int step(int n) { stage = n; return fail_at == n ? 29210004 : 0; }
void mp_warn(struct mp_log *log, const char *format, ...) {}

VideoProcessing_ErrorCode OH_VideoProcessing_Create(OH_VideoProcessing **p, int type)
{
    assert(type == VIDEO_PROCESSING_TYPE_DETAIL_ENHANCER);
    output_format = input_format = 0;
    output_color_set = input_color_set = false;
    int r = step(CREATE); if (!r) { *p = PROCESSOR; processors++; } return r;
}
VideoProcessing_ErrorCode OH_VideoProcessingCallback_Create(VideoProcessing_Callback **p)
{
    int r = step(CALLBACK_CREATE); if (!r) { *p = (void *)&callback_token; callbacks++; } return r;
}
VideoProcessing_ErrorCode OH_VideoProcessingCallback_BindOnError(
    VideoProcessing_Callback *p, OH_VideoProcessingCallback_OnError cb)
{ error_cb = cb; return step(BIND_ERROR); }
VideoProcessing_ErrorCode OH_VideoProcessingCallback_BindOnNewOutputBuffer(
    VideoProcessing_Callback *p, OH_VideoProcessingCallback_OnNewOutputBuffer cb)
{ output_cb = cb; return step(BIND_OUTPUT); }
VideoProcessing_ErrorCode OH_VideoProcessing_RegisterCallback(
    OH_VideoProcessing *p, const VideoProcessing_Callback *c, void *data)
{ callback_data = data; return step(REGISTER); }
VideoProcessing_ErrorCode OH_VideoProcessingCallback_Destroy(VideoProcessing_Callback *c)
{ assert(callbacks == 1); callbacks--; return 0; }
OH_AVFormat *OH_AVFormat_Create(void)
{ if (step(FORMAT)) return NULL; formats++; return (void *)&format_token; }
void OH_AVFormat_Destroy(OH_AVFormat *f) { assert(formats == 1); formats--; }
bool OH_AVFormat_SetIntValue(OH_AVFormat *f, const char *key, int32_t value)
{ quality = value; return !step(VALUE); }
VideoProcessing_ErrorCode OH_VideoProcessing_SetParameter(
    OH_VideoProcessing *p, const OH_AVFormat *f)
{ assert(quality == VIDEO_DETAIL_ENHANCER_QUALITY_LEVEL_HIGH); return step(PARAMETER); }
int32_t OH_NativeWindow_NativeWindowHandleOpt(OHNativeWindow *w, int code, ...)
{
    va_list ap; va_start(ap, code);
    int r = 0;
    if (code == SET_BUFFER_GEOMETRY) {
        width = va_arg(ap, int); height = va_arg(ap, int);
        resize_count++; r = step(RESIZE);
    } else if (code == SET_FORMAT) {
        assert(!running);
        int value = va_arg(ap, int); assert(value == expected_format);
        if (w == WINDOW) output_format = value; else input_format = value;
        r = step(w == WINDOW ? OUTPUT_FORMAT : INPUT_FORMAT);
    } else if (code == GET_USAGE) {
        *va_arg(ap, uint64_t *) = 1;
        r = step(w == WINDOW ? OUTPUT_GET_USAGE : INPUT_GET_USAGE);
    } else if (code == SET_USAGE) {
        assert(va_arg(ap, uint64_t) == (1 | NATIVEBUFFER_USAGE_HW_RENDER | NATIVEBUFFER_USAGE_HW_TEXTURE));
        r = step(w == WINDOW ? OUTPUT_USAGE : INPUT_USAGE);
    } else { assert(!"unexpected window option"); }
    va_end(ap); return r;
}
int32_t OH_NativeWindow_SetColorSpace(OHNativeWindow *w, OH_NativeBuffer_ColorSpace color)
{
    assert(!running && color == expected_color);
    if (w == WINDOW) output_color_set = true; else input_color_set = true;
    return step(w == WINDOW ? OUTPUT_COLOR : INPUT_COLOR);
}
VideoProcessing_ErrorCode OH_VideoProcessing_SetSurface(OH_VideoProcessing *p, const OHNativeWindow *w)
{ assert(w == WINDOW && output_format == expected_format && output_color_set); return step(SURFACE); }
VideoProcessing_ErrorCode OH_VideoProcessing_GetSurface(OH_VideoProcessing *p, OHNativeWindow **w)
{ int r = step(INPUT_SURFACE); if (!r) { *w = INPUT; inputs++; } return r; }
VideoProcessing_ErrorCode OH_VideoProcessing_Start(OH_VideoProcessing *p)
{
    assert(output_format == expected_format && input_format == expected_format);
    assert(output_color_set && input_color_set);
    int r = step(START); if (!r) running++; return r;
}
VideoProcessing_ErrorCode OH_VideoProcessing_Stop(OH_VideoProcessing *p)
{ assert(running == 1); running--; return 0; }
VideoProcessing_ErrorCode OH_VideoProcessing_Destroy(OH_VideoProcessing *p)
{
    if (destroy_error) return VIDEO_PROCESSING_ERROR_OPERATION_NOT_PERMITTED;
    assert(!running && processors == 1); processors--; return 0;
}
void OH_NativeWindow_DestroyNativeWindow(OHNativeWindow *w)
{ assert(!running && !processors && inputs == 1); inputs--; }
VideoProcessing_ErrorCode OH_VideoProcessing_RenderOutputBuffer(OH_VideoProcessing *p, uint32_t index)
{
    assert(!in_callback && index == 7);
    atomic_fetch_add(&rendered, 1);
    while (atomic_load(&hold_render)) test_sleep();
    return atomic_load(&render_error);
}

static void emit_output(void)
{
    in_callback = true;
    output_cb(PROCESSOR, 7, callback_data);
    in_callback = false;
}

static struct ohos_vpe *create_and_start(struct mp_log *log, OHNativeWindow *window,
                                        int w, int h)
{
    struct ohos_vpe *vpe = ohos_vpe_create_configured(log, window, w, h, expected_format, expected_color);
    assert(!running);
    if (vpe && !ohos_vpe_start(vpe)) {
        ohos_vpe_destroy(vpe);
        return NULL;
    }
    return vpe;
}

static int wait_error(struct ohos_vpe *vpe)
{
    for (int n = 0; n < 2000; n++) {
        int err = ohos_vpe_take_error(vpe);
        if (err) return err;
        test_sleep();
    }
    assert(!"Expected fallback");
    return 0;
}

int main(void)
{
    for (int n = CREATE; n <= START; n++) {
        fail_at = n;
        struct ohos_vpe *vpe = create_and_start(NULL, WINDOW, 1920, 1080);
        assert(!vpe && stage == n);
        assert(!processors && !formats && !callbacks && !inputs && !running);
    }
    fail_at = 0;
    struct ohos_vpe *prepared = ohos_vpe_create_configured(NULL, WINDOW, 1920, 1080,
                                                         expected_format, expected_color);
    assert(prepared && !running && processors == 1 && inputs == 1);
    ohos_vpe_destroy(prepared);
    assert(!processors && !inputs && !running);
    struct ohos_vpe *vpe = create_and_start(NULL, WINDOW, 1920, 1080);
    assert(vpe && running == 1 && inputs == 1 && processors == 1);
    assert(!formats && !callbacks && ohos_vpe_input(vpe) == INPUT);
    int old_resizes = resize_count;
    ohos_vpe_resize(vpe, 1920, 1080);
    assert(resize_count == old_resizes);
    ohos_vpe_resize(vpe, 1280, 720);
    assert(width == 1280 && height == 720 && resize_count == old_resizes + 1);
    ohos_vpe_resize(vpe, 0, 0);
    assert(resize_count == old_resizes + 1);
    emit_output();
    for (int n = 0; n < 2000 && !atomic_load(&rendered); n++) test_sleep();
    assert(rendered == 1 && !ohos_vpe_take_error(vpe));
    ohos_vpe_note_input(vpe);
    atomic_fetch_add(&fake_time, MP_TIME_S_TO_NS(10));
    assert(!ohos_vpe_take_error(vpe)); // a displayed first frame disarms timeout
    error_cb(PROCESSOR, VIDEO_PROCESSING_ERROR_PROCESS_FAILED, callback_data);
    assert(ohos_vpe_take_error(vpe) == VIDEO_PROCESSING_ERROR_PROCESS_FAILED);
    assert(!ohos_vpe_take_error(vpe)); // one fallback request per instance
    ohos_vpe_destroy(vpe);
    assert(!processors && !formats && !callbacks && !inputs && !running);

    vpe = create_and_start(NULL, WINDOW, 1920, 1080);
    fail_at = RESIZE;
    ohos_vpe_resize(vpe, 640, 360);
    assert(ohos_vpe_take_error(vpe));
    ohos_vpe_destroy(vpe);
    assert(!processors && !inputs && !running);

    fail_at = 0;
    vpe = create_and_start(NULL, WINDOW, 1920, 1080);
    atomic_fetch_add(&fake_time, MP_TIME_S_TO_NS(10));
    assert(!ohos_vpe_take_error(vpe)); // loading alone never arms timeout
    ohos_vpe_note_input(vpe);
    ohos_vpe_set_wakeup(vpe, wake_vo, NULL);
    atomic_fetch_add(&fake_time, MP_TIME_S_TO_NS(4));
    assert(wait_error(vpe) == VIDEO_PROCESSING_ERROR_PROCESS_FAILED);
    assert(atomic_load(&wakeups) > 0); // wake a sleeping VO to request fallback
    assert(!ohos_vpe_take_error(vpe));
    ohos_vpe_destroy(vpe);

    vpe = create_and_start(NULL, WINDOW, 1920, 1080);
    atomic_store(&render_error, VIDEO_PROCESSING_ERROR_PROCESS_FAILED);
    emit_output();
    assert(wait_error(vpe) == VIDEO_PROCESSING_ERROR_PROCESS_FAILED);
    ohos_vpe_destroy(vpe);
    atomic_store(&render_error, 0);

    vpe = create_and_start(NULL, WINDOW, 1920, 1080);
    atomic_store(&hold_render, true);
    int previous = atomic_load(&rendered);
    emit_output();
    for (int n = 0; n < 2000 && atomic_load(&rendered) == previous; n++) test_sleep();
    assert(atomic_load(&rendered) > previous);
    for (int n = 0; n < 33; n++) emit_output();
    assert(wait_error(vpe) == VIDEO_PROCESSING_ERROR_PROCESS_FAILED); // bounded queue
    atomic_store(&hold_render, false);
    ohos_vpe_stop_output(vpe);
    previous = atomic_load(&rendered);
    emit_output(); // late callbacks after stopping cannot submit more frames
    assert(atomic_load(&rendered) == previous);
    ohos_vpe_destroy(vpe); // stop is idempotent, processor still destroyed once

    vpe = create_and_start(NULL, WINDOW, 1920, 1080);
    ohos_vpe_destroy(vpe);
    expected_format = NATIVEBUFFER_PIXEL_FMT_RGBA_1010102;
    expected_color = OH_COLORSPACE_DISPLAY_BT2020_PQ;
    vpe = create_and_start(NULL, WINDOW, 2236, 1258);
    assert(vpe && running && input_format == output_format);
    ohos_vpe_destroy(vpe);
    expected_color = OH_COLORSPACE_DISPLAY_BT2020_HLG;
    vpe = create_and_start(NULL, WINDOW, 2236, 1258);
    assert(vpe && running && input_format == output_format);
    destroy_error = 1;
    ohos_vpe_destroy(vpe);
    // Failed destruction must retain callback data and the input window.
    assert(processors == 1 && inputs == 1 && !running);
    error_cb(PROCESSOR, VIDEO_PROCESSING_ERROR_PROCESS_FAILED, callback_data);
    assert(ohos_vpe_take_error(vpe));
    puts("VPE lifecycle: SDR/PQ/HLG format negotiation before Start, setup failures, prepared teardown, worker output, timeout, resize and teardown passed");
    return 0;
}
