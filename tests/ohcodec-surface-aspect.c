/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Exercise extracted production code with recording platform/device APIs. */
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ESTALE
#define ESTALE 116
#endif

static void ignore_log(void *ctx, const char *format, ...)
{ (void)ctx; (void)format; }
#define MP_ERR(...) ignore_log(__VA_ARGS__)
#define MP_WARN(...) ignore_log(__VA_ARGS__)
#define MP_VERBOSE(...) ignore_log(__VA_ARGS__)
#define MP_INFO(...) ignore_log(__VA_ARGS__)
#define MP_DBG(...) ignore_log(__VA_ARGS__)
#define MSGL_DEBUG 5
#define MPMIN(a, b) ((a) < (b) ? (a) : (b))
#define MPMAX(a, b) ((a) > (b) ? (a) : (b))
#define MPCLAMP(a, lo, hi) MPMIN(MPMAX(a, lo), hi)
#define AVERROR(e) (-(e))
#define AV_HWDEVICE_TYPE_OHCODEC 17
#define OH_SURFACE_SOURCE_VIDEO 1
#define VO_TRUE 1
#define VO_FALSE 0
#define VO_NOTIMPL (-3)
#define VO_ERROR (-1)
#define VOCTRL_PREPARE_DECODER 37
#define SET_SOURCE_TYPE 1
#define GET_BUFFER_GEOMETRY 2
#define SET_BUFFER_GEOMETRY 3

typedef enum {
    OH_SCALING_MODE_SCALE_FIT_V2,
    OH_SCALING_MODE_SCALE_TO_WINDOW_V2,
} OHScalingModeV2;
typedef struct OHNativeWindow { int width, height, scaling; } OHNativeWindow;
typedef struct AVOHCodecDeviceContext {
    OHNativeWindow *native_window;
    int direct_surface;
} AVOHCodecDeviceContext;
typedef struct AVBufferRef AVBufferRef;
typedef struct AVHWDeviceContext {
    void *hwctx, *user_opaque;
    void (*free)(struct AVHWDeviceContext *ctx);
} AVHWDeviceContext;
struct owner { unsigned refs; AVHWDeviceContext device; AVOHCodecDeviceContext codec; };
struct AVBufferRef { uint8_t *data; struct owner *owner; };
typedef struct AVHWFramesContext { AVBufferRef *device_ref; } AVHWFramesContext;
typedef struct AVOHCodecBuffer { int renders, discards; } AVOHCodecBuffer;
struct mp_rect { int x0, y0, x1, y1; };
static int mp_rect_w(struct mp_rect rect) { return rect.x1 - rect.x0; }
static int mp_rect_h(struct mp_rect rect) { return rect.y1 - rect.y0; }
struct mp_image_params { int w, h, p_w, p_h, rotate; struct mp_rect crop; };
struct mp_image { AVBufferRef *hwctx; uint8_t *planes[4]; double pts; };
struct mp_hwdec_ctx { AVBufferRef *av_device_ref; const char *driver_name; int hw_imgfmt; };
struct options {
    int64_t WinID;
    bool ohos_super_resolution, ohos_sdr_to_hdr, keepaspect;
    struct { int w, h; } ohos_surface_size;
};
struct priv {
    struct mp_hwdec_ctx hwctx;
    struct mp_image *next_image;
    int64_t next_render_time_ns;
};
struct vo {
    struct priv *priv;
    struct options *opts;
    void *log, *hwdec_devs;
    struct mp_image_params *params;
    int dwidth, dheight;
    bool want_redraw, prepare_supported;
};
struct ohos_vpe {
    OHNativeWindow input;
    int width, height;
    bool stopped;
};

static unsigned vpe_creates, vpe_destroys, vpe_stops;
static unsigned window_creates, window_destroys, device_destroys, images_freed;
static unsigned input_notes, osd_updates;
static unsigned fallback_checks;
static bool fail_device_alloc, fail_device_init, fail_window, fail_vpe, fail_ref, fail_stop;
static struct ohos_vpe *last_vpe;
static unsigned prepare_controls;
typedef struct { int num, den; } AVRational;
typedef struct AVCodecContext {
    int width, height;
    AVRational sample_aspect_ratio;
    AVBufferRef *hw_device_ctx;
} AVCodecContext;
typedef struct AVCodec { const char *wrapper_name; } AVCodec;
struct mp_codec_params { int par_w, par_h; };
struct decoder_context { struct vo *vo; AVBufferRef *hwdec_dev; };
static int vo_control(struct vo *vo, uint32_t request, void *data);

static AVBufferRef *av_buffer_ref(AVBufferRef *source)
{
    if (!source || fail_ref)
        return NULL;
    AVBufferRef *ref = malloc(sizeof(*ref));
    assert(ref);
    *ref = *source;
    ++ref->owner->refs;
    return ref;
}
static void av_buffer_unref(AVBufferRef **reference)
{
    AVBufferRef *ref = *reference;
    if (!ref)
        return;
    if (!--ref->owner->refs) {
        AVHWDeviceContext *ctx = &ref->owner->device;
        if (ctx->free)
            ctx->free(ctx);
        free(ref->owner);
        ++device_destroys;
    }
    free(ref);
    *reference = NULL;
}
static AVBufferRef *av_hwdevice_ctx_alloc(int type)
{
    assert(type == AV_HWDEVICE_TYPE_OHCODEC);
    if (fail_device_alloc)
        return NULL;
    AVBufferRef *ref = calloc(1, sizeof(*ref));
    assert(ref);
    ref->owner = calloc(1, sizeof(*ref->owner));
    assert(ref->owner);
    ref->owner->refs = 1;
    ref->owner->device.hwctx = &ref->owner->codec;
    ref->data = (uint8_t *)&ref->owner->device;
    return ref;
}
static int av_hwdevice_ctx_init(AVBufferRef *ref)
{ assert(ref); return fail_device_init ? -1 : 0; }
static int OH_NativeWindow_CreateNativeWindowFromSurfaceId(uint64_t id, OHNativeWindow **window)
{
    assert(id == 123);
    if (fail_window)
        return -1;
    *window = calloc(1, sizeof(**window));
    assert(*window);
    (*window)->width = 2832;
    (*window)->height = 1278;
    ++window_creates;
    return 0;
}
static void OH_NativeWindow_DestroyNativeWindow(OHNativeWindow *window)
{ assert(window); ++window_destroys; free(window); }
static int OH_NativeWindow_NativeWindowSetScalingModeV2(OHNativeWindow *window, OHScalingModeV2 mode)
{ assert(window); window->scaling = mode; return 0; }
static int OH_NativeWindow_NativeWindowHandleOpt(OHNativeWindow *window, int option, ...)
{
    assert(window);
    /* The fixture supplies explicit display dimensions, so querying is unused. */
    assert(option == SET_SOURCE_TYPE);
    return 0;
}
static struct ohos_vpe *ohos_vpe_create(void *log, OHNativeWindow *output, int width, int height)
{
    (void)log;
    assert(output && width > 0 && height > 0);
    if (fail_vpe)
        return NULL;
    struct ohos_vpe *vpe = calloc(1, sizeof(*vpe));
    assert(vpe);
    vpe->width = width;
    vpe->height = height;
    vpe->input.width = width;
    vpe->input.height = height;
    last_vpe = vpe;
    ++vpe_creates;
    return vpe;
}
static void ohos_vpe_attach(struct ohos_vpe *vpe, struct vo *vo)
{ (void)vpe; assert(vo); }
static OHNativeWindow *ohos_vpe_input(struct ohos_vpe *vpe)
{ assert(vpe); return &vpe->input; }
static bool ohos_vpe_stop(struct ohos_vpe *vpe)
{
    if (fail_stop)
        return false;
    if (vpe && !vpe->stopped) {
        vpe->stopped = true;
        ++vpe_stops;
    }
    return true;
}
static void ohos_vpe_check(struct ohos_vpe *vpe, struct vo *vo)
{ assert(vpe && vo && fail_stop); ++fallback_checks; }
static void ohos_vpe_destroy(struct ohos_vpe *vpe)
{
    if (vpe) {
        ++vpe_destroys;
        if (last_vpe == vpe)
            last_vpe = NULL;
        free(vpe);
    }
}
static void ohos_vpe_note_input(struct ohos_vpe *vpe)
{ if (vpe) { assert(!vpe->stopped); ++input_notes; } }
static bool mp_image_params_equal(const struct mp_image_params *a, const struct mp_image_params *b)
{ return memcmp(a, b, sizeof(*a)) == 0; }
static void update_osd_resolution(struct vo *vo)
{ assert(vo); ++osd_updates; }
static bool mp_msg_test(void *log, int level)
{ (void)log; (void)level; return false; }
static int64_t mp_time_ns(void) { return 123; }
static int64_t get_surface_time_ns(void) { return 456; }
static int av_ohcodec_release_buffer(AVOHCodecBuffer *buffer, int render)
{ assert(buffer); if (render) ++buffer->renders; else ++buffer->discards; return 0; }
static int av_ohcodec_release_buffer_at_time(AVOHCodecBuffer *buffer, int64_t time)
{ assert(time > 0); return av_ohcodec_release_buffer(buffer, 1); }
static void mp_image_unrefp(struct mp_image **reference)
{
    struct mp_image *image = *reference;
    if (!image)
        return;
    /* Frame reference ownership is modeled separately from the device ref. */
    if (image->hwctx) {
        AVHWFramesContext *frames = (void *)image->hwctx->data;
        av_buffer_unref(&frames->device_ref);
        free(frames);
        free(image->hwctx);
    }
    free(image);
    *reference = NULL;
    ++images_freed;
}

#include "ohcodec-surface-aspect.inc"

static int vo_control(struct vo *vo, uint32_t request, void *data)
{
    assert(vo && request == VOCTRL_PREPARE_DECODER);
    ++prepare_controls;
    return vo->prepare_supported ? prepare_decoder(vo, data) : VO_NOTIMPL;
}

struct fixture { struct vo vo; struct options opts; struct priv priv; };
static void init(struct fixture *f, int width, int height)
{
    *f = (struct fixture){0};
    f->opts = (struct options){.WinID=123, .ohos_super_resolution=true,
        .keepaspect=false, .ohos_surface_size={width,height}};
    f->vo = (struct vo){.priv=&f->priv, .opts=&f->opts, .hwdec_devs=f,
        .prepare_supported=true};
    f->priv.hwctx.av_device_ref = create_ohcodec_device_ref(&f->vo);
    assert(f->priv.hwctx.av_device_ref);
}
static void finish(struct fixture *f)
{
    mp_image_unrefp(&f->priv.next_image);
    av_buffer_unref(&f->priv.hwctx.av_device_ref);
}

static struct ohcodec_output *output_of(AVBufferRef *ref)
{ return ((AVHWDeviceContext *)(void *)ref->data)->user_opaque; }

static int prepare(struct fixture *f, const struct mp_image_params *source,
                   AVBufferRef **replacement)
{
    struct voctrl_prepare_decoder request = {
        .device_ref=f->priv.hwctx.av_device_ref, .params=source,
    };
    int result = prepare_decoder(&f->vo, &request);
    if (replacement)
        *replacement = request.replacement;
    else
        av_buffer_unref(&request.replacement);
    return result;
}

static struct mp_image *frame(AVBufferRef *device, AVOHCodecBuffer *buffer)
{
    struct mp_image *image = calloc(1, sizeof(*image));
    assert(image);
    image->hwctx = calloc(1, sizeof(*image->hwctx));
    assert(image->hwctx);
    AVHWFramesContext *frames = calloc(1, sizeof(*frames));
    assert(frames);
    frames->device_ref = av_buffer_ref(device);
    assert(frames->device_ref);
    image->hwctx->data = (uint8_t *)frames;
    image->planes[3] = (uint8_t *)buffer;
    return image;
}

static void assert_dar(const struct ohos_vpe *vpe, double expected)
{
    assert(vpe && vpe->width > 0 && vpe->height > 0);
    assert((vpe->width & 1) == 0 && (vpe->height & 1) == 0);
    /* Two-pixel alignment may change the ratio by at most two source pixels. */
    assert(fabs((double)vpe->width / vpe->height - expected) < 0.004);
}

static void test_reopen_mode(int saved_width)
{
    struct fixture f;
    unsigned before = vpe_creates;
    init(&f, saved_width, 1278);
    assert(!output_of(f.priv.hwctx.av_device_ref)->vpe);
    /* Initial Surface is a saved 4:3/fullscreen container. The actual stream
     * must set the first VPE input before the decoder binds that input. */
    struct mp_image_params source = {.w=1920, .h=1096, .p_w=1, .p_h=1};
    assert(prepare(&f, &source, NULL) == VO_TRUE);
    struct ohcodec_output *output = output_of(f.priv.hwctx.av_device_ref);
    struct ohos_vpe *vpe = output->vpe;
    assert(vpe_creates == before + 1);
    assert_dar(vpe, 1920.0 / 1096);
    int buffer_width = vpe->width, buffer_height = vpe->height;
    assert(buffer_width <= saved_width && buffer_height <= 1278);
    /* Selecting Original changes final fit/stretch and display dimensions,
     * with no hidden bars baked into an intermediate processing buffer. */
    f.opts.keepaspect = true;
    f.opts.ohos_surface_size.w = 2238;
    resize(&f.vo);
    assert(f.vo.dwidth == 2238 && f.vo.dheight == 1278 && f.vo.want_redraw);
    assert(output->window->scaling == OH_SCALING_MODE_SCALE_FIT_V2);
    assert(vpe->width == buffer_width && vpe->height == buffer_height);
    assert(prepare(&f, &source, NULL) == VO_TRUE);
    assert(vpe_creates == before + 1 && output->vpe == vpe);
    f.opts.keepaspect = false;
    f.opts.ohos_surface_size.w = 2832;
    resize(&f.vo);
    assert(output->window->scaling == OH_SCALING_MODE_SCALE_TO_WINDOW_V2);
    assert(vpe->width == buffer_width && vpe->height == buffer_height);
    AVOHCodecBuffer token = {0};
    f.priv.next_image = frame(f.priv.hwctx.av_device_ref, &token);
    f.priv.next_render_time_ns = 1000;
    unsigned notes = input_notes;
    flip_page(&f.vo);
    assert(token.renders == 1 && !token.discards && input_notes == notes + 1);
    assert(!f.priv.next_image);
    finish(&f);
}

static void test_sar_and_missing_viewport(void)
{
    struct fixture f;
    init(&f, 1703, 1278);
    struct mp_image_params source = {.w=720, .h=576, .p_w=16, .p_h=15};
    assert(prepare(&f, &source, NULL) == VO_TRUE);
    assert_dar(output_of(f.priv.hwctx.av_device_ref)->vpe, 4.0 / 3);
    finish(&f);
    init(&f, 0, 0);
    assert(prepare(&f, &source, NULL) == VO_TRUE);
    assert_dar(output_of(f.priv.hwctx.av_device_ref)->vpe, 4.0 / 3);
    assert(last_vpe->width == 768 && last_vpe->height == 576);
    finish(&f);
}

static void test_source_change_and_old_frames(void)
{
    struct fixture f;
    init(&f, 2832, 1278);
    struct mp_image_params first = {.w=1920, .h=1096, .p_w=1, .p_h=1};
    struct mp_image_params next = {.w=1080, .h=1920, .p_w=1, .p_h=1};
    assert(prepare(&f, &first, NULL) == VO_TRUE);
    AVBufferRef *old_decoder = av_buffer_ref(f.priv.hwctx.av_device_ref);
    struct ohcodec_output *old_output = output_of(old_decoder);
    struct ohos_vpe *old_vpe = old_output->vpe;
    AVOHCodecBuffer token = {0};
    f.priv.next_image = frame(old_decoder, &token);
    AVBufferRef *replacement = NULL;
    unsigned destroyed = device_destroys, stopped = vpe_stops, notes = input_notes;
    assert(prepare(&f, &next, &replacement) == VO_TRUE);
    assert(replacement && replacement->data == f.priv.hwctx.av_device_ref->data);
    assert(replacement->data != old_decoder->data);
    assert(old_output->retired && old_vpe->stopped && vpe_stops == stopped + 1);
    assert(device_destroys == destroyed); /* decoder/frame owners still live */
    assert_dar(output_of(replacement)->vpe, 1080.0 / 1920);
    flip_page(&f.vo);
    assert(token.discards == 1 && !token.renders && input_notes == notes);
    assert(device_destroys == destroyed);
    av_buffer_unref(&old_decoder);
    assert(device_destroys == destroyed + 1);
    av_buffer_unref(&replacement);
    finish(&f);
}

static void test_generation_allocation_failures(void)
{
    struct fixture f;
    init(&f, 2832, 1278);
    struct mp_image_params first = {.w=1920, .h=1096, .p_w=1, .p_h=1};
    struct mp_image_params next = {.w=1280, .h=720, .p_w=1, .p_h=1};
    assert(prepare(&f, &first, NULL) == VO_TRUE);
    AVBufferRef *old = f.priv.hwctx.av_device_ref;
    struct ohcodec_output *output = output_of(old);
    bool *faults[] = {&fail_device_alloc, &fail_device_init, &fail_window, &fail_ref};
    for (unsigned i=0; i < sizeof(faults) / sizeof(faults[0]); ++i) {
        *faults[i] = true;
        AVBufferRef *replacement = NULL;
        assert(prepare(&f, &next, &replacement) == VO_ERROR);
        *faults[i] = false;
        assert(!replacement && f.priv.hwctx.av_device_ref == old);
        assert(!output->retired && !output->vpe->stopped);
        assert(prepare(&f, &first, NULL) == VO_TRUE);
    }
    finish(&f);
}

static void test_invalid_and_optional_processing(void)
{
    struct fixture f;
    init(&f, 2832, 1278);
    assert(prepare(&f, NULL, NULL) == VO_ERROR);
    struct mp_image_params invalid = {.w=0, .h=1080};
    assert(prepare(&f, &invalid, NULL) == VO_ERROR);
    struct mp_image_params source = {.w=1920, .h=1096, .p_w=1, .p_h=1};
    unsigned before = vpe_creates;
    fail_vpe = true;
    assert(prepare(&f, &source, NULL) == VO_TRUE);
    fail_vpe = false;
    AVHWDeviceContext *device = (void *)f.priv.hwctx.av_device_ref->data;
    struct ohcodec_output *output = device->user_opaque;
    assert(!output->vpe && ((AVOHCodecDeviceContext *)device->hwctx)->native_window == output->window);
    assert(vpe_creates == before);
    finish(&f);
    for (unsigned mode=0; mode < 2; ++mode) {
        init(&f, 2832, 1278);
        f.opts.ohos_super_resolution = mode != 0;
        f.opts.ohos_sdr_to_hdr = mode != 0;
        assert(prepare(&f, &source, NULL) == VO_TRUE);
        assert(!output_of(f.priv.hwctx.av_device_ref)->vpe && vpe_creates == before);
        finish(&f);
    }
}

static void test_stop_failure_blocks_retired_frames(void)
{
    struct fixture f;
    init(&f, 2832, 1278);
    struct mp_image_params first = {.w=1920, .h=1096, .p_w=1, .p_h=1};
    struct mp_image_params next = {.w=1080, .h=1920, .p_w=1, .p_h=1};
    assert(prepare(&f, &first, NULL) == VO_TRUE);
    AVBufferRef *old = f.priv.hwctx.av_device_ref;
    AVOHCodecBuffer token = {0};
    f.priv.next_image = frame(old, &token);
    unsigned created = vpe_creates, notes = input_notes, checks = fallback_checks;
    AVBufferRef *replacement = NULL;
    fail_stop = true;
    assert(prepare(&f, &next, &replacement) == VO_ERROR);
    assert(!replacement && f.priv.hwctx.av_device_ref == old);
    assert(output_of(old)->retired && vpe_creates == created);
    assert(fallback_checks == checks + 1);
    assert(prepare(&f, &first, NULL) == VO_ERROR);
    flip_page(&f.vo);
    assert(token.discards == 1 && !token.renders && input_notes == notes);
    fail_stop = false;
    finish(&f);
}

static void test_decoder_guards_and_parameter_transfer(void)
{
    struct fixture f;
    init(&f, 1703, 1278);
    AVCodecContext avctx = {.width=720, .height=576, .sample_aspect_ratio={1,1},
        .hw_device_ctx=av_buffer_ref(f.priv.hwctx.av_device_ref)};
    struct decoder_context ctx = {.vo=&f.vo,
        .hwdec_dev=av_buffer_ref(f.priv.hwctx.av_device_ref)};
    const AVCodec ohcodec = {.wrapper_name="ohcodec"};
    const AVCodec software = {.wrapper_name=NULL};
    struct mp_codec_params container = {.par_w=16, .par_h=15};
    unsigned controls = prepare_controls, before = vpe_creates;
    assert(decoder_before_open(&ctx, &avctx, &software, &container) == 1);
    assert(prepare_controls == controls && vpe_creates == before);
    ctx.vo = NULL;
    assert(decoder_before_open(&ctx, &avctx, &ohcodec, &container) == 1);
    ctx.vo = &f.vo;
    AVBufferRef *device = avctx.hw_device_ctx;
    avctx.hw_device_ctx = NULL;
    assert(decoder_before_open(&ctx, &avctx, &ohcodec, &container) == 1);
    avctx.hw_device_ctx = device;
    assert(prepare_controls == controls && vpe_creates == before);
    /* A GPU/Buffer VO declines this request and keeps its original device. */
    f.vo.prepare_supported = false;
    assert(decoder_before_open(&ctx, &avctx, &ohcodec, &container) == 1);
    assert(prepare_controls == controls + 1 && vpe_creates == before);
    assert(avctx.hw_device_ctx->data == f.priv.hwctx.av_device_ref->data);
    f.vo.prepare_supported = true;
    assert(decoder_before_open(&ctx, &avctx, &ohcodec, &container) == 1);
    assert_dar(output_of(avctx.hw_device_ctx)->vpe, 4.0 / 3);
    avctx.width = 1920;
    avctx.height = 1096;
    avctx.sample_aspect_ratio = (AVRational){0,0};
    container = (struct mp_codec_params){0};
    assert(decoder_before_open(&ctx, &avctx, &ohcodec, &container) == 1);
    assert(avctx.hw_device_ctx->data == f.priv.hwctx.av_device_ref->data);
    assert(ctx.hwdec_dev->data == avctx.hw_device_ctx->data);
    assert_dar(output_of(avctx.hw_device_ctx)->vpe, 1920.0 / 1096);
    av_buffer_unref(&avctx.hw_device_ctx);
    av_buffer_unref(&ctx.hwdec_dev);
    finish(&f);
}

int main(void)
{
    test_reopen_mode(1703); /* saved 4:3 */
    test_reopen_mode(2832); /* saved stretch */
    test_sar_and_missing_viewport();
    test_source_change_and_old_frames();
    test_generation_allocation_failures();
    test_invalid_and_optional_processing();
    test_stop_failure_blocks_retired_frames();
    test_decoder_guards_and_parameter_transfer();
    assert(window_creates == window_destroys);
    assert(vpe_creates == vpe_destroys);
    assert(images_freed == 4 && osd_updates == 4);
    puts("OHCodec Surface aspect: saved 4:3/stretch, SAR, resize, generations, guards and failures passed");
    return 0;
}
