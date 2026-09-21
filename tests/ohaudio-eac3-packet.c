#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "audio/out/ohaudio_eac3_packet.h"

static uint8_t burst[OHAUDIO_EAC3_BURST], output[OHAUDIO_EAC3_BURST];

static void pack(const uint8_t *payload, unsigned size)
{
    memset(burst, 0, sizeof(burst));
    burst[0] = 0x72; burst[1] = 0xf8; burst[2] = 0x1f; burst[3] = 0x4e;
    burst[4] = 0x15; burst[6] = (uint8_t)(size & 255); burst[7] = (uint8_t)(size >> 8);
    for (unsigned n = 0; n < size; n += 2) {
        burst[8 + n] = payload[n + 1]; burst[9 + n] = payload[n];
    }
}

int main(void)
{
    /* Header-only synthetic frames test transport validation, not decoding. */
    const uint8_t six[] = {0x0b, 0x77, 0, 2, 0x34, 0x80};
    pack(six, sizeof(six));
    assert(ohaudio_eac3_unpack(burst, sizeof(burst), output, sizeof(output)) == 6);
    assert(memcmp(six, output, sizeof(six)) == 0);
    assert(ohaudio_eac3_unpack(burst, sizeof(burst) - 1, output, sizeof(output)) < 0);
    assert(ohaudio_eac3_unpack(burst, sizeof(burst), output, 5) < 0);
    burst[4] = 1; // AC-3 must not reach an E-AC3 renderer.
    assert(ohaudio_eac3_unpack(burst, sizeof(burst), output, sizeof(output)) < 0);
    pack(six, sizeof(six)); burst[6] = 7;
    assert(ohaudio_eac3_unpack(burst, sizeof(burst), output, sizeof(output)) < 0);
    pack(six, sizeof(six)); burst[6] = 255; burst[7] = 255;
    assert(ohaudio_eac3_unpack(burst, sizeof(burst), output, sizeof(output)) < 0);
    pack(six, sizeof(six)); burst[9] = 0;
    assert(ohaudio_eac3_unpack(burst, sizeof(burst), output, sizeof(output)) < 0);
    uint8_t changed[sizeof(six)];
    memcpy(changed, six, sizeof(six)); changed[4] |= 0x40; // 44.1 kHz unsupported
    pack(changed, sizeof(changed));
    assert(ohaudio_eac3_unpack(burst, sizeof(burst), output, sizeof(output)) < 0);
    memcpy(changed, six, sizeof(six)); changed[5] = 8 << 3; // AC-3 bsid
    pack(changed, sizeof(changed));
    assert(ohaudio_eac3_unpack(burst, sizeof(burst), output, sizeof(output)) < 0);
    uint8_t multi[36];
    for (unsigned n = 0; n < 6; n++) {
        memcpy(multi + n * 6, six, 6); multi[n * 6 + 4] = 4; // one block
    }
    pack(multi, sizeof(multi));
    assert(ohaudio_eac3_unpack(burst, sizeof(burst), output, sizeof(output)) == 36);
    pack(multi, sizeof(multi) - 6); // incomplete access unit
    assert(ohaudio_eac3_unpack(burst, sizeof(burst), output, sizeof(output)) < 0);
    puts("E-AC3 packet validation passed");
    return 0;
}
