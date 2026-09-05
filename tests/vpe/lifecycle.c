// Exercise the production VPE adapter with failure injection. Native API
// declarations come from the real SDK; only the vendor implementation is fake.
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <multimedia/video_processing_engine/video_processing.h>
#include <multimedia/player_framework/native_avformat.h>
#include "video/out/ohos_vpe.h"

const int32_t VIDEO_PROCESSING_TYPE_DETAIL_ENHANCER = 2;
const char *VIDEO_DETAIL_ENHANCER_PARAMETER_KEY_QUALITY_LEVEL = "quality";
enum { CREATE = 1, CALLBACK, BIND_ERROR, BIND_OUTPUT, REGISTER, FORMAT,
       VALUE, PARAMETER, RESIZE, SURFACE, INPUT, START };
static int fail_at, processors, formats, callbacks, inputs, running, quality;
static int stage, resize_count, rendered, destroy_error, width, height;
static OH_VideoProcessingCallback_OnError error_cb;
static OH_VideoProcessingCallback_OnNewOutputBuffer output_cb;
static void *callback_data;
static int processor_token, window_token, callback_token, format_token;
#define PROCESSOR ((OH_VideoProcessing *)&processor_token)
#define WINDOW ((OHNativeWindow *)&window_token)
static int step(int n) { stage = n; return fail_at == n ? 29210004 : 0; }
void mp_warn(struct mp_log *log, const char *format, ...) {}

VideoProcessing_ErrorCode OH_VideoProcessing_Create(OH_VideoProcessing **p, int type)
{
    assert(type == VIDEO_PROCESSING_TYPE_DETAIL_ENHANCER);
    int r = step(CREATE); if (!r) { *p = PROCESSOR; processors++; } return r;
}
VideoProcessing_ErrorCode OH_VideoProcessingCallback_Create(VideoProcessing_Callback **p)
{
    int r = step(CALLBACK); if (!r) { *p = (void *)&callback_token; callbacks++; } return r;
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
    assert(code == SET_BUFFER_GEOMETRY);
    va_list ap; va_start(ap, code); width = va_arg(ap, int); height = va_arg(ap, int); va_end(ap);
    resize_count++; return step(RESIZE);
}
VideoProcessing_ErrorCode OH_VideoProcessing_SetSurface(OH_VideoProcessing *p, const OHNativeWindow *w)
{ assert(w == WINDOW); return step(SURFACE); }
VideoProcessing_ErrorCode OH_VideoProcessing_GetSurface(OH_VideoProcessing *p, OHNativeWindow **w)
{ int r = step(INPUT); if (!r) { *w = WINDOW; inputs++; } return r; }
VideoProcessing_ErrorCode OH_VideoProcessing_Start(OH_VideoProcessing *p)
{ int r = step(START); if (!r) running++; return r; }
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
{ assert(index == 7); rendered++; return 0; }

int main(void)
{
    for (int n = CREATE; n <= START; n++) {
        fail_at = n;
        struct ohos_vpe *vpe = ohos_vpe_create(NULL, WINDOW, 1920, 1080);
        assert(!vpe && stage == n);
        assert(!processors && !formats && !callbacks && !inputs && !running);
    }
    fail_at = 0;
    struct ohos_vpe *vpe = ohos_vpe_create(NULL, WINDOW, 1920, 1080);
    assert(vpe && running == 1 && inputs == 1 && processors == 1);
    assert(!formats && !callbacks && ohos_vpe_input(vpe) == WINDOW);
    int old_resizes = resize_count;
    ohos_vpe_resize(vpe, 1920, 1080);
    assert(resize_count == old_resizes);
    ohos_vpe_resize(vpe, 1280, 720);
    assert(width == 1280 && height == 720 && resize_count == old_resizes + 1);
    ohos_vpe_resize(vpe, 0, 0);
    assert(resize_count == old_resizes + 1);
    output_cb(PROCESSOR, 7, callback_data);
    assert(rendered == 1 && !ohos_vpe_take_error(vpe));
    error_cb(PROCESSOR, VIDEO_PROCESSING_ERROR_PROCESS_FAILED, callback_data);
    assert(ohos_vpe_take_error(vpe) == VIDEO_PROCESSING_ERROR_PROCESS_FAILED);
    assert(!ohos_vpe_take_error(vpe)); // one fallback request per instance
    ohos_vpe_destroy(vpe);
    assert(!processors && !formats && !callbacks && !inputs && !running);

    vpe = ohos_vpe_create(NULL, WINDOW, 1920, 1080);
    fail_at = RESIZE;
    ohos_vpe_resize(vpe, 640, 360);
    assert(ohos_vpe_take_error(vpe));
    ohos_vpe_destroy(vpe);
    assert(!processors && !inputs && !running);

    fail_at = 0;
    vpe = ohos_vpe_create(NULL, WINDOW, 1920, 1080);
    destroy_error = 1;
    ohos_vpe_destroy(vpe);
    // Failed destruction must retain callback data and the input window.
    assert(processors == 1 && inputs == 1 && !running);
    error_cb(PROCESSOR, VIDEO_PROCESSING_ERROR_PROCESS_FAILED, callback_data);
    assert(ohos_vpe_take_error(vpe));
    puts("VPE lifecycle: 12 setup failures, high quality, output, resize, fallback and failed destruction passed");
    return 0;
}
