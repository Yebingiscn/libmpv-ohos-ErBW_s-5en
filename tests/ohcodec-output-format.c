/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Exercise the real patched output-format callback with recording OH/FFmpeg APIs. */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AVERROR(e) (-(e))
#define AV_ERR_UNKNOWN 6
#define AV_LOG_DEBUG 1
#define AV_LOG_INFO 2
#define AV_LOG_WARNING 3
#define AV_LOG_ERROR 4
#define AV_PROFILE_HEVC_MAIN_10 2
#define AV_PROFILE_H264_HIGH_10 110
#define AV_CODEC_ID_H264 27
#define AV_CODEC_ID_HEVC 173
#define AVCOL_RANGE_JPEG 2
#define AVCOL_RANGE_MPEG 1
#define OH_MD_KEY_VIDEO_PIC_WIDTH "video-pic-width"
#define OH_MD_KEY_VIDEO_PIC_HEIGHT "video-pic-height"
#define OH_MD_KEY_VIDEO_STRIDE "video-stride"
#define OH_MD_KEY_VIDEO_SLICE_HEIGHT "video-slice-height"
#define OH_MD_KEY_BITS_PER_CODED_SAMPLE "bits-per-coded-sample"
#define OH_MD_KEY_PIXEL_FORMAT "pixel-format"
#define OH_MD_KEY_MATRIX_COEFFICIENTS "matrix-coefficients"
#define OH_MD_KEY_COLOR_PRIMARIES "color-primaries"
#define OH_MD_KEY_TRANSFER_CHARACTERISTICS "transfer-characteristics"
#define OH_MD_KEY_RANGE_FLAG "range-flag"
#define OH_MD_KEY_VIDEO_SAR "video-sar"

enum AVPixelFormat {
    AV_PIX_FMT_NONE = -1, AV_PIX_FMT_NV12, AV_PIX_FMT_NV21,
    AV_PIX_FMT_YUV420P, AV_PIX_FMT_P010, AV_PIX_FMT_RGBA,
    AV_PIX_FMT_OHCODEC,
};
typedef enum {
    AV_PIXEL_FORMAT_YUVI420 = 1, AV_PIXEL_FORMAT_NV12 = 2,
    AV_PIXEL_FORMAT_NV21 = 3, AV_PIXEL_FORMAT_SURFACE_FORMAT = 4,
    AV_PIXEL_FORMAT_RGBA = 5, AV_PIXEL_FORMAT_RGBA1010102 = 6,
} OH_AVPixelFormat;
enum {
    NATIVEBUFFER_PIXEL_FMT_RGBA_8888 = 12,
    NATIVEBUFFER_PIXEL_FMT_YCBCR_420_SP = 24,
    NATIVEBUFFER_PIXEL_FMT_YCRCB_420_SP = 25,
    NATIVEBUFFER_PIXEL_FMT_YCBCR_420_P = 28,
    NATIVEBUFFER_PIXEL_FMT_YCRCB_420_P = 29,
    NATIVEBUFFER_PIXEL_FMT_YCBCR_P010 = 35,
    NATIVEBUFFER_PIXEL_FMT_YCRCB_P010 = 36,
};

typedef struct { int num, den; } AVRational;
typedef struct {
    int format;
    enum AVPixelFormat sw_format;
    int width, height;
} AVHWFramesContext;
typedef struct AVBufferRef {
    uint8_t *data;
    unsigned *references;
} AVBufferRef;
typedef struct {
    bool output_to_window, direct_surface_output, got_stream_info;
    int width, height, stride, slice_height, decode_status;
    OH_AVPixelFormat pix_fmt;
} OHCodecDecContext;
typedef struct {
    void *priv_data;
    int width, height, profile, bits_per_raw_sample, codec_id;
    enum AVPixelFormat pix_fmt, sw_pix_fmt;
    AVBufferRef *hw_device_ctx, *hw_frames_ctx;
    int colorspace, color_primaries, color_trc, color_range;
    AVRational sample_aspect_ratio;
} AVCodecContext;
typedef struct { int unused; } OH_AVCodec;
typedef struct {
    struct { const char *key; int value; } entries[16];
    unsigned count;
} OH_AVFormat;

static unsigned allocations, initializations, destroyed;
static bool fail_allocation, fail_initialization;

static void av_log(void *ctx, int level, const char *format, ...)
{ (void)ctx; (void)level; (void)format; }
static const char *av_get_pix_fmt_name(enum AVPixelFormat format)
{
    static const char *const names[] = {
        "nv12", "nv21", "yuv420p", "p010le", "rgba", "ohcodec",
    };
    return format >= 0 && format <= AV_PIX_FMT_OHCODEC ? names[format] : "none";
}
static bool OH_AVFormat_GetIntValue(OH_AVFormat *format, const char *key,
                                     int32_t *value)
{
    for (unsigned i = 0; i < format->count; i++)
        if (!strcmp(format->entries[i].key, key)) {
            *value = format->entries[i].value;
            return true;
        }
    return false;
}
static bool OH_AVFormat_GetDoubleValue(OH_AVFormat *format, const char *key,
                                        double *value)
{ (void)format; (void)key; (void)value; return false; }
static const char *OH_AVFormat_DumpInfo(OH_AVFormat *format)
{ (void)format; return "mock output description"; }
static int ff_set_dimensions(AVCodecContext *ctx, int width, int height)
{
    if (width <= 0 || height <= 0)
        return AVERROR(EINVAL);
    ctx->width = width;
    ctx->height = height;
    return 0;
}
static AVRational av_d2q(double value, int max)
{ (void)value; (void)max; return (AVRational){1, 1}; }
static void ff_set_sar(AVCodecContext *ctx, AVRational ratio)
{ ctx->sample_aspect_ratio = ratio; }
static enum AVPixelFormat ff_oh_pix_to_ff_pix(OH_AVPixelFormat format)
{
    switch (format) {
    case AV_PIXEL_FORMAT_NV12: return AV_PIX_FMT_NV12;
    case AV_PIXEL_FORMAT_NV21: return AV_PIX_FMT_NV21;
    case AV_PIXEL_FORMAT_YUVI420: return AV_PIX_FMT_YUV420P;
    case AV_PIXEL_FORMAT_SURFACE_FORMAT: return AV_PIX_FMT_OHCODEC;
    default: return AV_PIX_FMT_NONE;
    }
}
static AVBufferRef *av_hwframe_ctx_alloc(AVBufferRef *device)
{
    (void)device;
    if (fail_allocation)
        return NULL;
    AVBufferRef *ref = calloc(1, sizeof(*ref));
    assert(ref);
    ref->data = calloc(1, sizeof(AVHWFramesContext));
    ref->references = malloc(sizeof(*ref->references));
    assert(ref->data && ref->references);
    *ref->references = 1;
    allocations++;
    return ref;
}
static AVBufferRef *av_buffer_ref(AVBufferRef *source)
{
    assert(source);
    AVBufferRef *ref = malloc(sizeof(*ref));
    assert(ref);
    *ref = *source;
    ++*ref->references;
    return ref;
}
static void av_buffer_unref(AVBufferRef **reference)
{
    AVBufferRef *ref = *reference;
    if (!ref)
        return;
    if (!--*ref->references) {
        free(ref->references);
        free(ref->data);
        destroyed++;
    }
    free(ref);
    *reference = NULL;
}
static int av_hwframe_ctx_init(AVBufferRef *ref)
{ assert(ref); initializations++; return fail_initialization ? AVERROR(EIO) : 0; }
static void oh_decode_on_err(OH_AVCodec *codec, int error, void *userdata)
{
    (void)codec;
    AVCodecContext *ctx = userdata;
    ((OHCodecDecContext *)ctx->priv_data)->decode_status = -error;
}

#include "ohcodec-output-format.inc"

static void put(OH_AVFormat *format, const char *key, int value)
{
    for (unsigned i = 0; i < format->count; i++)
        if (!strcmp(format->entries[i].key, key)) {
            format->entries[i].value = value;
            return;
        }
    assert(format->count < sizeof(format->entries) / sizeof(format->entries[0]));
    format->entries[format->count].key = key;
    format->entries[format->count++].value = value;
}
static OH_AVFormat description(int pixel, int graphic, int bits)
{
    OH_AVFormat format = {0};
    put(&format, OH_MD_KEY_VIDEO_PIC_WIDTH, 1920);
    put(&format, OH_MD_KEY_VIDEO_PIC_HEIGHT, 1080);
    put(&format, OH_MD_KEY_VIDEO_STRIDE, 1920);
    put(&format, OH_MD_KEY_VIDEO_SLICE_HEIGHT, 1088);
    if (pixel >= 0)
        put(&format, OH_MD_KEY_PIXEL_FORMAT, pixel);
    if (graphic >= 0)
        put(&format, "video_graphic_pixel_format", graphic);
    if (bits >= 0)
        put(&format, OH_MD_KEY_BITS_PER_CODED_SAMPLE, bits);
    return format;
}
static AVCodecContext context(OHCodecDecContext *state, bool surface,
                               int codec, int profile, int bits)
{
    *state = (OHCodecDecContext){.output_to_window = surface};
    return (AVCodecContext){
        .priv_data = state, .codec_id = codec, .profile = profile,
        .bits_per_raw_sample = bits, .pix_fmt = AV_PIX_FMT_NONE,
        .sw_pix_fmt = AV_PIX_FMT_NONE,
    };
}
static void check_surface(int codec, int profile, int input_bits,
                          int pixel, int graphic, int output_bits,
                          enum AVPixelFormat expected)
{
    OHCodecDecContext state;
    AVCodecContext ctx = context(&state, true, codec, profile, input_bits);
    OH_AVFormat format = description(pixel, graphic, output_bits);
    oh_decode_on_stream_changed(NULL, &format, &ctx);
    if (expected == AV_PIX_FMT_NONE) {
        assert(state.decode_status < 0);
        assert(!state.got_stream_info);
        assert(!ctx.hw_frames_ctx);
    } else {
        assert(!state.decode_status && state.got_stream_info);
        assert(ctx.pix_fmt == AV_PIX_FMT_OHCODEC && ctx.sw_pix_fmt == expected);
        assert(ctx.hw_frames_ctx);
        AVHWFramesContext *frames = (void *)ctx.hw_frames_ctx->data;
        assert(frames->format == AV_PIX_FMT_OHCODEC && frames->sw_format == expected);
        assert(frames->width == 1920 && frames->height == 1080);
    }
    av_buffer_unref(&ctx.hw_frames_ctx);
}
static void test_actual_output_precedence(void)
{
    const struct {
        int graphic, bits;
        enum AVPixelFormat expected;
    } cases[] = {
        {NATIVEBUFFER_PIXEL_FMT_YCBCR_420_SP, -1, AV_PIX_FMT_NV12},
        {NATIVEBUFFER_PIXEL_FMT_YCBCR_420_SP, 10, AV_PIX_FMT_NV12},
        {NATIVEBUFFER_PIXEL_FMT_YCRCB_420_SP, 10, AV_PIX_FMT_NV21},
        {NATIVEBUFFER_PIXEL_FMT_YCBCR_420_P, 10, AV_PIX_FMT_YUV420P},
        {NATIVEBUFFER_PIXEL_FMT_YCBCR_P010, 8, AV_PIX_FMT_P010},
        {NATIVEBUFFER_PIXEL_FMT_YCRCB_P010, 8, AV_PIX_FMT_P010},
        {NATIVEBUFFER_PIXEL_FMT_RGBA_8888, 8, AV_PIX_FMT_NONE},
        {0x7fff1234, 10, AV_PIX_FMT_NONE},
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        check_surface(AV_CODEC_ID_H264, AV_PROFILE_H264_HIGH_10, 10,
                      AV_PIXEL_FORMAT_NV12, cases[i].graphic, cases[i].bits,
                      cases[i].expected);
    /* P010 can be reported without the legacy codec pixel-format key. */
    check_surface(AV_CODEC_ID_H264, 100, 8, -1,
                  NATIVEBUFFER_PIXEL_FMT_YCBCR_P010, -1, AV_PIX_FMT_P010);
}
static void test_missing_graphic_format(void)
{
    check_surface(AV_CODEC_ID_HEVC, AV_PROFILE_HEVC_MAIN_10, 10,
                  AV_PIXEL_FORMAT_NV12, -1, -1, AV_PIX_FMT_P010);
    check_surface(AV_CODEC_ID_H264, AV_PROFILE_H264_HIGH_10, 10,
                  AV_PIXEL_FORMAT_NV12, -1, 8, AV_PIX_FMT_NV12);
    check_surface(AV_CODEC_ID_H264, 100, 8,
                  AV_PIXEL_FORMAT_NV12, -1, 10, AV_PIX_FMT_P010);
    check_surface(AV_CODEC_ID_H264, 100, 8,
                  AV_PIXEL_FORMAT_NV21, -1, 8, AV_PIX_FMT_NV21);
    check_surface(AV_CODEC_ID_H264, 100, 8,
                  AV_PIXEL_FORMAT_YUVI420, -1, 8, AV_PIX_FMT_YUV420P);
    check_surface(AV_CODEC_ID_H264, 100, 8,
                  AV_PIXEL_FORMAT_YUVI420, -1, 10, AV_PIX_FMT_NONE);
    check_surface(AV_CODEC_ID_H264, AV_PROFILE_H264_HIGH_10, 10,
                  AV_PIXEL_FORMAT_SURFACE_FORMAT, -1, -1, AV_PIX_FMT_P010);
    check_surface(AV_CODEC_ID_H264, 100, 8,
                  AV_PIXEL_FORMAT_SURFACE_FORMAT, -1, 8, AV_PIX_FMT_NV12);
    check_surface(AV_CODEC_ID_H264, 100, 8,
                  -1, -1, 10, AV_PIX_FMT_P010);
    check_surface(AV_CODEC_ID_H264, 100, 8,
                  AV_PIXEL_FORMAT_RGBA, -1, 8, AV_PIX_FMT_NONE);
    check_surface(AV_CODEC_ID_H264, 100, 8,
                  9000, -1, 8, AV_PIX_FMT_NONE);
    check_surface(AV_CODEC_ID_H264, 100, 12,
                  AV_PIXEL_FORMAT_NV12, -1, -1, AV_PIX_FMT_NONE);
    const int invalid_depths[] = {0, 9, 12, 16, 32};
    for (unsigned i = 0; i < sizeof(invalid_depths) / sizeof(invalid_depths[0]); i++)
        check_surface(AV_CODEC_ID_H264, 100, 8,
                      AV_PIXEL_FORMAT_NV12, -1, invalid_depths[i], AV_PIX_FMT_NV12);
}
static void test_cpu_output_is_unchanged(void)
{
    const struct { int pixel; enum AVPixelFormat expected; } cases[] = {
        {AV_PIXEL_FORMAT_NV12, AV_PIX_FMT_NV12},
        {AV_PIXEL_FORMAT_NV21, AV_PIX_FMT_NV21},
        {AV_PIXEL_FORMAT_YUVI420, AV_PIX_FMT_YUV420P},
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        OHCodecDecContext state;
        AVCodecContext ctx = context(&state, false, AV_CODEC_ID_H264,
                                       AV_PROFILE_H264_HIGH_10, 10);
        OH_AVFormat format = description(cases[i].pixel,
                                           NATIVEBUFFER_PIXEL_FMT_YCBCR_P010, 10);
        unsigned before = allocations;
        oh_decode_on_stream_changed(NULL, &format, &ctx);
        assert(!state.decode_status && state.got_stream_info);
        assert(ctx.pix_fmt == cases[i].expected && !ctx.hw_frames_ctx);
        assert(allocations == before);
    }
}
static void test_runtime_reconfiguration(void)
{
    OHCodecDecContext state;
    AVCodecContext ctx = context(&state, true, AV_CODEC_ID_H264,
                                   AV_PROFILE_H264_HIGH_10, 10);
    OH_AVFormat format = description(AV_PIXEL_FORMAT_NV12,
                                       NATIVEBUFFER_PIXEL_FMT_YCBCR_P010, 10);
    unsigned before = allocations;
    oh_decode_on_stream_changed(NULL, &format, &ctx);
    assert(state.got_stream_info && !state.decode_status);
    AVBufferRef *old_frame = av_buffer_ref(ctx.hw_frames_ctx);
    AVHWFramesContext *old_frames = (void *)old_frame->data;
    oh_decode_on_stream_changed(NULL, &format, &ctx);
    assert(allocations == before + 1 && ctx.hw_frames_ctx->data == old_frame->data);

    put(&format, "video_graphic_pixel_format", NATIVEBUFFER_PIXEL_FMT_YCBCR_420_SP);
    oh_decode_on_stream_changed(NULL, &format, &ctx);
    assert(ctx.sw_pix_fmt == AV_PIX_FMT_NV12 && allocations == before + 2);
    assert(ctx.hw_frames_ctx->data != old_frame->data);
    assert(old_frames->sw_format == AV_PIX_FMT_P010 && *old_frame->references == 1);

    put(&format, "video_graphic_pixel_format", NATIVEBUFFER_PIXEL_FMT_YCBCR_P010);
    oh_decode_on_stream_changed(NULL, &format, &ctx);
    assert(ctx.sw_pix_fmt == AV_PIX_FMT_P010 && allocations == before + 3);
    assert(old_frames->sw_format == AV_PIX_FMT_P010);
    av_buffer_unref(&ctx.hw_frames_ctx);
    av_buffer_unref(&old_frame);
}
static void test_hwframes_failures(void)
{
    for (int allocation_error = 0; allocation_error < 2; allocation_error++) {
        OHCodecDecContext state;
        AVCodecContext ctx = context(&state, true, AV_CODEC_ID_H264, 100, 8);
        OH_AVFormat format = description(AV_PIXEL_FORMAT_NV12,
                                           NATIVEBUFFER_PIXEL_FMT_YCBCR_420_SP, 8);
        fail_allocation = allocation_error;
        fail_initialization = !allocation_error;
        oh_decode_on_stream_changed(NULL, &format, &ctx);
        assert(state.decode_status < 0 && !state.got_stream_info);
        assert(!ctx.hw_frames_ctx);
    }
    fail_allocation = fail_initialization = false;
}
int main(void)
{
    test_actual_output_precedence();
    test_missing_graphic_format();
    test_cpu_output_is_unchanged();
    test_runtime_reconfiguration();
    test_hwframes_failures();
    assert(allocations == destroyed);
    assert(initializations > 0);
    puts("OHCodec output-format behavior checks passed");
    return 0;
}
