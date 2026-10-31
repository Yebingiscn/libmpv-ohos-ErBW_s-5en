// Host-only checks of the exact production matching and diagnostic hash helpers.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "video/out/ohos_vivid.h"

int main(void)
{
    struct ohos_vivid_frame frame = {0};
    frame.dynamic[0] = 0x26;
    frame.dynamic[1] = 0x04;
    frame.dynamic_size = 2;
    frame.valid = true;
    frame.token = 0x1234;
    frame.epoch = 7;
    frame.pts = 12.5;
    assert(ohos_vivid_matches(&frame, 0x1234, 7));
    assert(!ohos_vivid_matches(NULL, 0x1234, 7));
    assert(!ohos_vivid_matches(&frame, 0x5678, 7));
    assert(!ohos_vivid_matches(&frame, 0x1234, 8));
    assert(!ohos_vivid_matches(&frame, 0, 7));
    frame.token = 0;
    assert(!ohos_vivid_matches(&frame, 0, 7));
    frame.token = 0x1234;
    frame.valid = false;
    assert(!ohos_vivid_matches(&frame, 0x1234, 7));
    frame.valid = true;
    frame.dynamic_size = 0;
    assert(!ohos_vivid_matches(&frame, 0x1234, 7));
    frame.dynamic_size = OHOS_VIVID_MAX_METADATA + 1u;
    assert(!ohos_vivid_matches(&frame, 0x1234, 7));
    frame.dynamic_size = OHOS_VIVID_MAX_METADATA;
    frame.static_size = OHOS_VIVID_MAX_METADATA;
    assert(ohos_vivid_matches(&frame, 0x1234, 7));
    frame.static_size++;
    assert(!ohos_vivid_matches(&frame, 0x1234, 7));

    const uint8_t text[] = {'h', 'e', 'l', 'l', 'o'};
    assert(ohos_vivid_hash(text, sizeof(text)) == UINT32_C(0x4f9f2cab));
    assert(ohos_vivid_hash(NULL, 0) == UINT32_C(2166136261));
    uint8_t changed[sizeof(text)];
    memcpy(changed, text, sizeof(text));
    changed[0] ^= 1;
    assert(ohos_vivid_hash(changed, sizeof(changed)) != ohos_vivid_hash(text, sizeof(text)));

    // A seek/reconfiguration invalidates the epoch even if the frame address
    // is reused. A reset frame must never inherit a previous Vivid decision.
    memset(&frame, 0, sizeof(frame));
    assert(!ohos_vivid_matches(&frame, 0x1234, 7));
    puts("HDR Vivid frame identity, epoch, bounds and hash checks passed");
    return 0;
}
