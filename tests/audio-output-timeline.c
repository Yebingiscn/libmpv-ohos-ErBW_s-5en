/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "audio/output_timeline.h"

static void near(double actual, double expected) {
    assert(fabs(actual - expected) < 1e-7);
}

int main(void) {
    struct audio_output_timeline t = {0};
    double pts, wait;
    assert(!audio_timeline_pts(&t, 0, &pts));

    // 200ms of old 1x PCM and then 200ms of new 3x PCM. Changing the
    // requested speed must not reinterpret the queued 1x samples as 3x.
    audio_timeline_append(&t, .2, 10, 1);
    assert(audio_timeline_pts(&t, .15, &pts)); near(pts, 10.05);
    audio_timeline_append(&t, .2, 10.2, 3);
    assert(audio_timeline_pts(&t, .35, &pts)); near(pts, 10.05);
    near(audio_timeline_decode_speed(&t, .35, 1), 3);
    near(audio_timeline_decode_speed(&t, .35, 4), 4);
    double offset;
    assert(audio_timeline_offset(&t, .35, .1, &offset)); near(offset, .1);
    near(pts - 10.15 + offset, 0);
    assert(audio_timeline_offset(&t, .35, .2, &offset)); near(offset, .3);
    // A frame 100ms ahead during queued 1x audio is 0.1 media seconds
    // ahead, not 0.3. The old projection falsely asked the decoder to drop.
    assert(audio_timeline_wait(&t, .35, 10.10, &wait)); near(wait, .05);
    assert(audio_timeline_wait(&t, .35, 10.50, &wait)); near(wait, .25);
    assert(audio_timeline_pts(&t, .15, &pts)); near(pts, 10.35);

    // Restore 1x while 3x audio is still queued, without jumping the clock
    // forward or asking video to drop frames to catch a fictitious jump.
    audio_timeline_append(&t, .2, 10.8, 1);
    assert(audio_timeline_pts(&t, .35, &pts)); near(pts, 10.35);
    near(audio_timeline_decode_speed(&t, .35, 1), 3);
    near(audio_timeline_decode_speed(&t, .2, 1), 1);
    near(audio_timeline_decode_speed(&t, 0, 1), 1);
    assert(audio_timeline_offset(&t, .35, .1, &offset)); near(offset, .3);
    assert(audio_timeline_offset(&t, .35, .2, &offset)); near(offset, .5);
    assert(audio_timeline_offset(&t, .1, .2, &offset)); near(offset, .2);
    assert(audio_timeline_wait(&t, .35, 10.38, &wait)); near(wait, .01);
    assert(audio_timeline_wait(&t, .35, 10.90, &wait)); near(wait, .25);
    assert(audio_timeline_pts(&t, .10, &pts)); near(pts, 10.90);
    assert(audio_timeline_wait(&t, .10, 10.85, &wait)); near(wait, -.05);
    assert(audio_timeline_wait(&t, .10, 11.10, &wait)); near(wait, .2);
    assert(audio_timeline_pts(&t, 0, &pts)); near(pts, 11);

    // More toggles than queue capacity: bounded storage and explicit fallback
    // for evicted history; no interpolation across discontinuous file PTS.
    audio_timeline_reset(&t);
    double media = 0;
    for (int i=0; i<1000; ++i) {
        double speed = i % 2 ? 3 : .5;
        audio_timeline_append(&t, .01, media, speed);
        media += .01 * speed;
    }
    assert(t.count == AUDIO_TIMELINE_CAPACITY);
    assert(!audio_timeline_pts(&t, 10, &pts));
    assert(audio_timeline_pts(&t, .005, &pts)); near(pts, media - .015);
    audio_timeline_reset(&t);
    audio_timeline_append(&t, .2, 0, 1);
    audio_timeline_append(&t, .2, 50, 1);
    assert(!audio_timeline_wait(&t, .3, 50.1, &wait));
    assert(!audio_timeline_offset(&t, .3, .2, &offset));
    assert(!audio_timeline_offset(&t, .3, NAN, &offset));
    assert(!audio_timeline_pts(&t, NAN, &pts));
    assert(!audio_timeline_pts(&t, -.1, &pts));
    audio_timeline_append(&t, .2, NAN, 1);
    assert(!audio_timeline_pts(&t, 0, &pts));
    near(audio_timeline_decode_speed(&t, .1, 1), 1);
    audio_timeline_reset(&t);

    // Long steady playback merges into one segment and remains accurate.
    for (int i=0; i<10000; ++i)
        audio_timeline_append(&t, .01, i * .01, 1);
    assert(t.count == 1);
    assert(audio_timeline_pts(&t, .2, &pts)); near(pts, 99.8);
    puts("audio output timeline: passed");
}
