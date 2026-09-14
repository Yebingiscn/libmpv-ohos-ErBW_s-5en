/* SPDX-License-Identifier: LGPL-2.1-or-later */
// Compile the actual patched policy/setters against a recording OHCodec API.
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <stdio.h>
#define AVERROR(e) (-(e))
#define AV_LOG_VERBOSE 1
#define AV_LOG_WARNING 2
#define AV_ERR_OK 0
#define AV_ERR_INVALID_VAL 1
typedef int OH_AVErrCode;
typedef enum { OH_FRAME_RETENTION_MODE_FULL,
               OH_FRAME_RETENTION_MODE_ADAPTIVE } OH_FrameRetentionMode;
typedef struct { double fps, speed; int retention; } OH_AVFormat;
typedef struct {
    double source_frame_rate, requested_playback_speed;
    bool playback_policy_pending, preroll, preserve_frames;
    bool smart_fluency_failed, smart_fluency_active, diagnostics_enabled;
    int64_t policy_requested_ns;
    void *dec;
    struct {const char *frame_rate, *retention_mode, *decoder_speed;} keys;
} OHCodecDecContext;
typedef struct {const char *wrapper_name;} AVCodec;
typedef struct {const AVCodec *codec; void *priv_data; int codec_id;} AVCodecContext;
static OH_AVFormat format, applied;
static int calls, result;
static int64_t oh_decode_perf_time_ns(void) { return 1000; }
static bool oh_decode_supports_smart_fluency(AVCodecContext *c) { (void)c; return true; }
static const char *avcodec_get_name(int id) { (void)id; return "hevc"; }
static void av_log(void *c, int level, const char *fmt, ...)
{ (void)c; (void)level; (void)fmt; }
static OH_AVFormat *OH_AVFormat_Create(void)
{ format = (OH_AVFormat){.retention = -1}; return &format; }
static void OH_AVFormat_Destroy(OH_AVFormat *f) { (void)f; }
static bool OH_AVFormat_SetDoubleValue(OH_AVFormat *f, const char *k, double v)
{ if (!strcmp(k,"fps")) f->fps=v; else f->speed=v; return true; }
static bool OH_AVFormat_SetIntValue(OH_AVFormat *f, const char *k, int v)
{ (void)k; f->retention=v; return true; }
static OH_AVErrCode OH_VideoDecoder_SetParameter(void *d, OH_AVFormat *f)
{ (void)d; applied=*f; calls++; return result; }
#include "ohcodec-decode-hints.inc"

int main(void)
{
    const AVCodec codec = {.wrapper_name="ohcodec"};
    OHCodecDecContext s = {.source_frame_rate=25, .requested_playback_speed=1,
        .dec=&format, .keys={"fps","retention","speed"}};
    AVCodecContext c = {.codec=&codec, .priv_data=&s};
    // Long resume: raise decoding demand, retain every frame, keep 1x media.
    assert(avcodec_ohcodec_set_decode_hints(&c, 1, 1) == 0);
    oh_decode_update_playback_policy(&c);
    assert(calls==1 && applied.fps==120 && !s.smart_fluency_active);
    assert(s.requested_playback_speed==1);
    for (int i=0; i<208; i++) {
        avcodec_ohcodec_set_decode_hints(&c, 1, 1);
        oh_decode_update_playback_policy(&c);
    }
    assert(calls==1); // No platform call per preroll frame.
    avcodec_ohcodec_set_decode_hints(&c, 0, 1);
    oh_decode_update_playback_policy(&c);
    assert(applied.fps==25 && calls==2);
    // Repair must not accidentally lose frames when the user holds 2x.
    avcodec_ohcodec_set_playback_speed(&c, 2);
    oh_decode_update_playback_policy(&c);
    assert(applied.fps==50 && !s.smart_fluency_active);
    // Normal media retains the previous adaptive-speed behavior.
    avcodec_ohcodec_set_decode_hints(&c, 0, 0);
    oh_decode_update_playback_policy(&c);
    assert(s.smart_fluency_active && applied.retention==OH_FRAME_RETENTION_MODE_ADAPTIVE);
    avcodec_ohcodec_set_decode_hints(&c, 1, 0);
    oh_decode_update_playback_policy(&c);
    assert(!s.smart_fluency_active && applied.retention==OH_FRAME_RETENTION_MODE_FULL);
    assert(applied.fps==120);
    avcodec_ohcodec_set_decode_hints(&c, 0, 0);
    oh_decode_update_playback_policy(&c);
    assert(s.smart_fluency_active && applied.fps==50);
    // A high source/playback rate must never be reduced by the preroll floor.
    s.source_frame_rate=120;
    avcodec_ohcodec_set_decode_hints(&c, 1, 0);
    oh_decode_update_playback_policy(&c);
    assert(applied.fps==240);
    // Unsupported codecs/keys and rejected hints do not fail decoding.
    assert(avcodec_ohcodec_set_decode_hints(NULL, 1, 0)==AVERROR(ENOSYS));
    const AVCodec software = {0};
    c.codec=&software;
    assert(avcodec_ohcodec_set_decode_hints(&c, 1, 0)==AVERROR(ENOSYS));
    c.codec=&codec;
    s.keys.retention_mode=NULL; s.keys.decoder_speed=NULL;
    result=AV_ERR_INVALID_VAL;
    avcodec_ohcodec_set_decode_hints(&c, 0, 1);
    oh_decode_update_playback_policy(&c);
    assert(!s.playback_policy_pending);
    puts("OHCodec preroll/full-frame policy tests passed");
    return 0;
}
