#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "audio/out/usb_audio_pcm.h"
int main(void)
{
    uint8_t out[4] = {0};
    assert(ua_pcm_pack(0x12345600, 3, 24, false, out));
    assert(!memcmp(out, "\x56\x34\x12", 3));
    assert(!ua_pcm_pack(0x12345678, 3, 24, false, out));
    assert(ua_pcm_pack(0x12345678, 3, 24, true, out));
    assert(!memcmp(out, "\x56\x34\x12", 3));
    assert(ua_pcm_pack(0x87654321, 4, 24, true, out));
    assert(!memcmp(out, "\x00\x43\x65\x87", 4));
    assert(ua_pcm_pack(0x80000000, 2, 16, false, out));
    assert(!memcmp(out, "\x00\x80", 2));
    assert(ua_pcm_pack(0x7fffffff, 2, 16, true, out));
    assert(!memcmp(out, "\xff\x7f", 2));
    assert(ua_pcm_pack(0xffffffff, 4, 32, false, out));
    assert(!memcmp(out, "\xff\xff\xff\xff", 4));
    assert(!ua_pcm_pack(0, 1, 8, true, out));
    puts("PASS USB strict/compatible packing, precision loss, sign, full scale and 24-in-32 alignment");
    return 0;
}
