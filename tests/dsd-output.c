/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "audio/decode/dsd_packer.h"
#include "audio/out/usb_audio_dsd.h"

static void framing(bool native, bool planar, bool lsb, unsigned channels)
{
    uint8_t source[2][28], interleaved[56], expected[56], output[128];
    for (unsigned i = 0; i < 28; i++) {
        for (unsigned c = 0; c < channels; c++) {
            uint8_t v = (uint8_t)(i * 17 + c * 59);
            source[c][i] = lsb ? ua_reverse(v) : v;
            interleaved[i * channels + c] = source[c][i];
            expected[i * channels + c] = v;
        }
    }
    for (unsigned split = 0; split <= 28; split++) {
        struct dsd_packer p;
        dsd_packer_reset(&p);
        memset(output, 0xcc, sizeof(output));
        const uint8_t *a[2] = {planar ? source[0] : interleaved, source[1]};
        unsigned n = dsd_pack(&p, output, a, split, channels, planar, lsb, native);
        const uint8_t *b[2] = {planar ? source[0] + split : interleaved + split * channels, source[1] + split};
        n += dsd_pack(&p, output + n * channels * 4, b, 28 - split, channels, planar, lsb, native);
        unsigned group = native ? 4 : 2;
        assert(n == 28 / group && p.count == 0);
        for (unsigned f = 0; f < n; f++) {
            for (unsigned c = 0; c < channels; c++) {
                const uint8_t *o = output + (f * channels + c) * 4;
                if (native) {
                    for (unsigned k = 0; k < 4; k++)
                        assert(o[k] == expected[(f * 4 + k) * channels + c]);
                    uint8_t wire[4];
                    ua_dsd_wire(wire, o, UA_DSD_U32_LE);
                    for (unsigned k = 0; k < 4; k++) assert(wire[k] == o[3-k]);
                    ua_dsd_wire(wire, o, UA_DSD_U32_BE);
                    assert(!memcmp(wire, o, 4));
                } else {
                    assert(o[0] == 0 && o[3] == (f % 2 ? 0xfa : 0x05));
                    assert(o[2] == expected[f * 2 * channels + c]);
                    assert(o[1] == expected[(f * 2 + 1) * channels + c]);
                }
            }
        }
        assert(output[n * channels * 4] == 0xcc);
        // Reset after a partial packet / seek must discard carry and restart markers.
        dsd_pack(&p, output, a, 1, channels, planar, lsb, native);
        assert(p.count == 1);
        dsd_packer_reset(&p);
        assert(!p.count && p.marker == 0x05);
    }
}
int main(void)
{
    for (unsigned flags = 0; flags < 16; flags++)
        framing(flags & 1, flags & 2, flags & 4, flags & 8 ? 2 : 1);
    struct ua_mode m = {.alternate=2, .protocol=0x20, .bytes=4, .bits=32};
    assert(ua_dsd_mode(0x16d0,0x071a,0x0199,&m) == UA_DSD_U32_LE);
    assert(ua_dsd_mode(0x16d0,0x071a,0x019b,&m) == UA_DSD_U32_BE);
    assert(ua_dsd_mode(0x16d0,0x071a,0x0203,&m) == UA_DSD_U32_BE);
    assert(!ua_dsd_mode(0x16d0,0x071a,0x0204,&m));
    assert(!ua_dsd_mode(0x20b1,0x0001,0,&m)); // no XMOS wildcard
    assert(ua_dsd_mode(0x2772,0x0230,0,&m) == UA_DSD_U32_BE);
    m.alternate=1; assert(!ua_dsd_mode(0x2772,0x0230,0,&m));
    m.alternate=3; assert(ua_dsd_mode(0x2622,0x0041,0,&m) == UA_DSD_U32_BE);
    m.bits=24; assert(!ua_dsd_mode(0x2622,0x0041,0,&m));
    puts("DSD: split packets, planar/interleaved, bit order, mono/stereo, DoP markers, native endian and device rules passed");
    return 0;
}
