/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <assert.h>
#include <stdio.h>
#include "audio/out/usb_audio_protocol.h"

static const uint8_t uac1[] = {
    9,4,0,0,0,1,1,0,0,
    12,0x24,2,1,1,1,0,2,3,0,0,0,
    9,0x24,6,2,1,1,2,0,0,
    9,4,1,1,1,1,2,0,0,
    7,0x24,1,1,0,1,0,
    14,0x24,2,1,2,3,24,2,0x44,0xac,0,0x80,0xbb,0,
    9,5,1,9,0x26,1,1,0,0,
    7,0x25,1,1,0,0,0,
};
static const uint8_t uac2[] = {
    9,4,0,0,0,1,1,0x20,0,
    17,0x24,2,1,1,1,0,3,2,3,0,0,0,0,0,0,0,
    8,0x24,0x0a,3,3,3,0,0,
    10,0x24,6,2,1,0x0c,0,0,0,0,
    9,4,1,1,2,1,2,0x20,0,
    16,0x24,1,1,0,1,1,0,0,0,2,3,0,0,0,0,
    6,0x24,2,1,4,32,
    7,5,1,5,0x00,4,1,
    7,5,0x81,0x11,4,0,4,
};
int main(void)
{
    struct ua_caps c;
    assert(ua_parse(uac1, sizeof(uac1), &c));
    assert(c.count == 1 && c.modes[0].pcm && c.modes[0].bits == 24);
    assert(c.modes[0].feature == 2 && c.modes[0].frequency_control);
    assert(ua_rate_supported(&c.modes[0], 44100));
    assert(ua_rate_supported(&c.modes[0], 48000));
    assert(!ua_rate_supported(&c.modes[0], 96000));
    assert(ua_parse(uac2, sizeof(uac2), &c));
    assert(c.count == 1 && c.modes[0].clock == 3 && c.modes[0].feature == 2);
    assert(c.modes[0].feedback == 0x81 && c.modes[0].packet == 1024);
    assert(c.modes[0].channels == 2 && c.modes[0].bytes == 4);
    /* Every truncation inside a descriptor must fail, without out-of-bounds reads. */
    size_t boundary = 0;
    for (size_t n = 1; n < sizeof(uac2); n++) {
        if (n == boundary + uac2[boundary]) boundary = n;
        if (n != boundary) assert(!ua_parse(uac2, n, &c));
    }
    uint8_t broken[sizeof(uac1)];
    memcpy(broken, uac1, sizeof(broken)); broken[0] = 0;
    assert(!ua_parse(broken, sizeof(broken), &c));
    uint8_t dop[4];
    ua_dop(dop, 0x12, 0x34, false, 0x05);
    assert(dop[0] == 0 && dop[1] == 0x34 && dop[2] == 0x12 && dop[3] == 5);
    ua_dop(dop, 0x48, 0x2c, true, 0xfa);
    assert(dop[0] == 0 && dop[1] == 0x34 && dop[2] == 0x12 && dop[3] == 0xfa);
    for (unsigned n = 0; n < 256; n++) assert(ua_reverse(ua_reverse((uint8_t)n)) == n);
    const unsigned rates[] = {44100, 48000, 88200, 96000, 176400, 192000, 352800, 705600};
    for (unsigned r = 0; r < sizeof(rates) / sizeof(rates[0]); r++) {
        uint64_t phase = 0, total = 0, q = ((uint64_t)rates[r] << 32) / 8000;
        for (unsigned i = 0; i < 80000; i++) {
            unsigned n = ua_next_frames(&phase, q, 1);
            assert(n >= rates[r] / 8000 && n <= rates[r] / 8000 + 1);
            total += n;
        }
        assert(total <= rates[r] * 10 && total + 1 >= rates[r] * 10);
    }
    puts("USB protocol: descriptor bounds, UAC1/2, rates, fractional packets, DoP byte order passed");
    return 0;
}
