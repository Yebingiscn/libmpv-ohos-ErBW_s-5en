/* SPDX-License-Identifier: LGPL-2.1-or-later */
// Exercise actual patched mpv functions extracted by the Python verifier.
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define MP_NOPTS_VALUE (-1e20)
#define STREAM_VIDEO 1
#define STREAM_AUDIO 2
#define VDCTRL_SET_DECODE_HINTS 17
#define mp_assert assert
enum { STATUS_SYNCING, STATUS_READY, STATUS_PLAYING, STATUS_DRAINING, STATUS_EOF };

struct priv;
struct mp_filter { void *priv; };
struct mp_decoder_wrapper { struct mp_filter *f; };
struct codec { int type; };
struct decoder {
    struct mp_filter *f;
    int (*control)(struct mp_filter *, int, void *);
};
struct options { bool correct_pts, repair_dts_pts; };
struct repair { bool no_drop; };
struct priv {
    struct codec *codec;
    struct decoder *decoder;
    struct options *opts;
    struct repair dts_pts_repair;
    struct mp_filter *decf;
    void *opt_cache;
    int cache_lock, play_dir;
    double codec_pts, start_pts;
    bool playback_sync;
};
struct track { struct mp_decoder_wrapper *dec; };
struct vo_chain { struct track *track; bool is_sparse; };
struct MPContext {
    struct vo_chain *vo_chain;
    int video_status, audio_status, play_dir;
    double playback_pts;
    bool hrseek_active;
};

static int lock_depth, wakeups, controls, preroll, preserve;
static int process_stage, feeds, reads, queue_updates;
static bool cache_changed;
static void mp_mutex_lock(int *lock)
{ (void)lock; assert(lock_depth == 0); lock_depth++; }
static void mp_mutex_unlock(int *lock)
{ (void)lock; assert(lock_depth == 1); lock_depth--; }
static void mp_filter_wakeup(struct mp_filter *f)
{ assert(f && !lock_depth); wakeups++; }
static bool mp_dts_pts_no_drop(struct repair *r) { return r->no_drop; }
static int record_control(struct mp_filter *f, int command, void *arg)
{
    assert(f && !lock_depth && command == VDCTRL_SET_DECODE_HINTS);
    const int *hints = arg;
    preroll = hints[0]; preserve = hints[1]; controls++;
    process_stage = 1;
    return 1;
}
static bool m_config_cache_update(void *cache)
{ (void)cache; return cache_changed; }
static void update_queue_config(struct priv *p)
{ assert(p && !lock_depth); queue_updates++; }
static void feed_packet(struct priv *p)
{ assert(p && process_stage == 1 && !lock_depth); feeds++; process_stage = 2; }
static void read_frame(struct priv *p)
{ assert(p && process_stage == 2 && !lock_depth); reads++; process_stage = 3; }

#include "mpv-playback-sync.inc"

struct fixture {
    struct mp_filter filter;
    struct mp_decoder_wrapper wrapper;
    struct codec codec;
    struct decoder decoder;
    struct options opts;
    struct priv p;
    struct track track;
    struct vo_chain vo;
    struct MPContext ctx;
};
static void init(struct fixture *f)
{
    *f = (struct fixture){0};
    f->filter.priv = &f->p;
    f->wrapper.f = &f->filter;
    f->codec.type = STREAM_VIDEO;
    f->decoder = (struct decoder){&f->filter, record_control};
    f->opts = (struct options){true, true};
    f->p = (struct priv){.codec=&f->codec, .decoder=&f->decoder,
        .opts=&f->opts, .decf=&f->filter, .play_dir=1,
        .codec_pts=210, .start_pts=MP_NOPTS_VALUE};
    f->track.dec = &f->wrapper;
    f->vo.track = &f->track;
    f->ctx = (struct MPContext){.vo_chain=&f->vo,
        .video_status=STATUS_SYNCING, .audio_status=STATUS_PLAYING,
        .play_dir=1, .playback_pts=214};
    lock_depth=wakeups=controls=preroll=preserve=0;
    process_stage=feeds=reads=queue_updates=0;
    cache_changed=false;
}
static void process(struct fixture *f)
{
    process_stage = 0;
    decf_process(&f->filter);
    assert(process_stage == 3 && !lock_depth);
}

static void test_moving_audio_target(void)
{
    struct fixture f; init(&f);
    // Four seconds behind. Video decodes at 120fps while 60fps audio time
    // advances at 1x, so catching up consumes 480 pictures, not just one.
    for (int picture=0; picture<=480; picture++) {
        f.p.codec_pts = 210 + picture / 60.0;
        f.ctx.playback_pts = 214 + picture / 120.0;
        update_playback_sync(&f.ctx);
        process(&f);
        assert(f.p.playback_sync && preroll && !preserve);
        assert(f.p.start_pts == MP_NOPTS_VALUE);
        if (picture == 1)
            assert(preroll); // Regression: first output must not end demand.
        if (picture == 240)
            assert(f.p.codec_pts >= 214 && f.p.codec_pts < f.ctx.playback_pts);
    }
    assert(wakeups == 1); // Repeated target updates never spam wakeups.
    // A synchronized frame can still need lookahead/VO reconfiguration.
    update_playback_sync(&f.ctx);
    assert(f.p.playback_sync);
    f.ctx.video_status = STATUS_READY;
    update_playback_sync(&f.ctx);
    process(&f);
    assert(!f.p.playback_sync && !preroll && wakeups == 2);
    f.ctx.video_status = STATUS_PLAYING;
    update_playback_sync(&f.ctx);
    assert(wakeups == 2 && f.ctx.playback_pts == 218 && f.p.codec_pts == 218);
}

static void test_exclusions(void)
{
    for (int scenario=0; scenario<9; scenario++) {
        struct fixture f; init(&f);
        update_playback_sync(&f.ctx);
        assert(f.p.playback_sync);
        switch (scenario) {
        case 0: f.ctx.hrseek_active=true; break;
        case 1: f.ctx.play_dir=-1; f.p.play_dir=-1; break;
        case 2: f.vo.is_sparse=true; break;
        case 3: f.ctx.audio_status=STATUS_EOF; break;
        case 4: f.ctx.audio_status=STATUS_SYNCING; break;
        case 5: f.ctx.playback_pts=MP_NOPTS_VALUE; break;
        case 6: f.ctx.video_status=STATUS_READY; break;
        case 7: f.ctx.video_status=STATUS_DRAINING; break;
        case 8: f.ctx.video_status=STATUS_EOF; break;
        }
        update_playback_sync(&f.ctx);
        process(&f);
        assert(!f.p.playback_sync && !preroll && wakeups == 2);
        assert(f.p.start_pts == MP_NOPTS_VALUE && f.p.codec_pts == 210);
    }
    struct fixture f; init(&f);
    f.ctx.vo_chain=NULL; update_playback_sync(&f.ctx);
    f.ctx.vo_chain=&f.vo; f.vo.track=NULL; update_playback_sync(&f.ctx);
    f.vo.track=&f.track; f.track.dec=NULL; update_playback_sync(&f.ctx);
    assert(wakeups == 0 && controls == 0);
}

static void test_queue_full_restoration(void)
{
    struct fixture f; init(&f);
    mp_decoder_wrapper_set_playback_sync(&f.wrapper, true);
    mp_decoder_wrapper_set_playback_sync(&f.wrapper, true);
    process(&f);
    assert(wakeups == 1 && preroll);
    // No new packet and no frame arrives. The woken process must still clear
    // the hint before feed_packet/read_frame, even with an unchanged cache.
    mp_decoder_wrapper_set_playback_sync(&f.wrapper, false);
    mp_decoder_wrapper_set_playback_sync(&f.wrapper, false);
    process(&f);
    assert(wakeups == 2 && !preroll && controls == 2);
    assert(feeds == 2 && reads == 2 && queue_updates == 0);
    cache_changed=true;
    process(&f);
    assert(queue_updates == 1 && !preroll && !lock_depth);
}

static void test_existing_hint_reasons(void)
{
    struct fixture f; init(&f);
    f.p.codec_pts=MP_NOPTS_VALUE; update_decode_hints(&f.p);
    assert(preroll && !f.p.playback_sync);
    f.p.codec_pts=210; f.p.start_pts=214; update_decode_hints(&f.p);
    assert(preroll && f.p.start_pts == 214);
    mp_decoder_wrapper_set_playback_sync(&f.wrapper, false);
    update_decode_hints(&f.p);
    assert(preroll); // Clearing track sync must not cancel explicit seek.
    f.p.start_pts=MP_NOPTS_VALUE; f.p.dts_pts_repair.no_drop=true;
    update_decode_hints(&f.p); assert(!preroll && preserve);
    mp_decoder_wrapper_set_playback_sync(&f.wrapper, true);
    update_decode_hints(&f.p); assert(preroll && preserve);
    f.p.opts->repair_dts_pts=false;
    update_decode_hints(&f.p); assert(preroll && !preserve);
    f.p.play_dir=-1;
    update_decode_hints(&f.p); assert(!preroll && !preserve);
    int before=controls;
    f.codec.type=STREAM_AUDIO; update_decode_hints(&f.p);
    f.codec.type=STREAM_VIDEO; f.p.decoder=NULL; update_decode_hints(&f.p);
    f.p.decoder=&f.decoder; f.decoder.control=NULL; update_decode_hints(&f.p);
    assert(controls == before);
}

int main(void)
{
    test_moving_audio_target();
    test_exclusions();
    test_queue_full_restoration();
    test_existing_hint_reasons();
    puts("mpv playback sync: moving target, lifecycle, hints and wakeups passed");
    return 0;
}
