/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <assert.h>
#include <stdio.h>
#include "filters/dts_pts_repair.h"

static void packets(struct mp_dts_pts_repair *s, double base)
{
    for (int i = 0; i < 16; i++)
        mp_dts_pts_packet(s, base + i * .04, base + i * .04, .04, 25, true);
}

int main(int argc, char **argv)
{
    struct mp_dts_pts_repair s = {0};
    packets(&s, 0);
    // Actual presentation-order PTS from the reported HEVC file, verified
    // independently with software decoding as well as OHCodec.
    const double raw[] = {0, .16, .12, .20, .08, .28, .24, .32, .04,
                          .48, .44, .52, .40, .60, .56, .64, .36};
    for (unsigned i = 0; i < sizeof(raw) / sizeof(raw[0]); i++) {
        double pts = mp_dts_pts_frame(&s, raw[i], true);
        assert(fabs(pts - (i >= 4 ? i * .04 : raw[i])) < 1e-9);
        assert(s.active == (i >= 4));
    }
    assert(mp_dts_pts_no_drop(&s));
    // A seek must preserve diagnosis but forget the previous time origin.
    mp_dts_pts_reset(&s);
    assert(mp_dts_pts_no_drop(&s));
    packets(&s, 12);
    for (unsigned i = 0; i < sizeof(raw) / sizeof(raw[0]); i++)
        assert(fabs(mp_dts_pts_frame(&s, 12 + raw[i], true) -
                    (12 + i * .04)) < 1e-9);
    // Good monotonic PTS == DTS video is never retimed.
    s = (struct mp_dts_pts_repair){0};
    for (int i = 0; i < 200; i++) {
        mp_dts_pts_packet(&s, i * .04, i * .04, .04, 25, true);
        assert(mp_dts_pts_frame(&s, i * .04, true) == i * .04);
        assert(!s.active);
    }
    assert(!mp_dts_pts_no_drop(&s));
    mp_dts_pts_frame(&s, .2, true);
    mp_dts_pts_frame(&s, .1, true);
    assert(!s.active); // Do not latch on a discontinuity late in good playback.
    // Valid reordered packet PTS must rule out this repair, even if a damaged
    // decoded frame later has a backwards timestamp.
    s = (struct mp_dts_pts_repair){0};
    packets(&s, 0);
    mp_dts_pts_packet(&s, .72, .64, .04, 25, true);
    for (unsigned i = 0; i < sizeof(raw) / sizeof(raw[0]); i++)
        assert(mp_dts_pts_frame(&s, raw[i], true) == raw[i]);
    assert(s.rejected && !s.active);
    mp_dts_pts_reset(&s);
    assert(!s.rejected && !s.active && s.frames == 0);
    packets(&s, 0);
    mp_dts_pts_frame(&s, 0, false);
    assert(s.rejected && !s.active);
    // VFR, discontinuities, missing timestamps, and a single glitch are not
    // enough evidence. Cadence changes also cancel an existing diagnosis.
    for (int kind = 0; kind < 4; kind++) {
        s = (struct mp_dts_pts_repair){.active = true};
        packets(&s, 0);
        mp_dts_pts_packet(&s, kind == 1 ? 3 : .64,
                          kind == 1 ? 3 : .64, kind == 0 ? .08 : .04,
                          kind == 2 ? 30 : 25, kind != 3);
        assert(s.rejected && !s.active && !mp_dts_pts_no_drop(&s));
    }
    s = (struct mp_dts_pts_repair){0};
    packets(&s, 0);
    for (int i = 0; i < 64; i++)
        mp_dts_pts_frame(&s, i == 8 ? .24 : i * .04, true);
    assert(!s.active);
    // Optional long real-file replay: one decoded raw PTS per line, while
    // simulating decoder input lookahead. No permanent startup offset/drift.
    if (argc > 1) {
        FILE *f = fopen(argv[1], "r");
        assert(f);
        s = (struct mp_dts_pts_repair){0};
        double pts;
        unsigned n = 0;
        packets(&s, 0);
        while (fscanf(f, "%lf", &pts) == 1) {
            mp_dts_pts_packet(&s, (n + 16) * .04, (n + 16) * .04,
                              .04, 25, true);
            double result = mp_dts_pts_frame(&s, pts, true);
            if (n >= 4)
                assert(fabs(result - n * .04) < 1e-7);
            n++;
        }
        fclose(f);
        assert(n > 500 && s.active);
        printf("Real-file replay: %u frames passed\n", n);
    }
    puts("DTS-as-PTS compatibility tests passed");
    return 0;
}
