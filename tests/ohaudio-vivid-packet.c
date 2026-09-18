#include <assert.h>
#include <stdlib.h>
#include "audio/out/ohaudio_vivid_packet.h"
#include "audio/out/ohaudio_vivid_speed.h"

struct speed_mock { float speed; int mode, sets, gets; };
static int mock_set(void *ctx, float speed)
{
    struct speed_mock *m = ctx;
    m->sets++;
    if (m->mode == 1 || (m->mode == 4 && m->sets > 1)) return -2;
    if (m->mode == 2) return 0; // Success response but silently ignored.
    m->speed = ((m->mode == 3 || m->mode == 4) && m->sets == 1) ? 1.5f : speed;
    return 0;
}
static int mock_get(void *ctx, float *speed)
{
    struct speed_mock *m = ctx;
    m->gets++;
    if (m->mode == 5 && m->gets == 1) return -3;
    *speed = m->speed;
    return 0;
}

int main(void)
{
    size_t size = sizeof(AVAudioVividMetadata) + 4;
    AVAudioVividMetadata *v = calloc(1, size);
    assert(v);
    v->channel_layout = 3;
    v->nb_samples = 4;
    v->metadata_size = 4;
    memcpy(v->metadata, "ABCD", 4);
    assert(ohaudio_vivid_metadata(v, size) == v);
    assert(!ohaudio_vivid_metadata(NULL, size));
    assert(!ohaudio_vivid_metadata(v, sizeof(*v) - 1));
    assert(!ohaudio_vivid_metadata(v, size - 1));
    assert(!ohaudio_vivid_metadata(v, size + 1));
    v->channel_layout = 0;
    assert(!ohaudio_vivid_metadata(v, size));
    v->channel_layout = 3;
    unsigned char pcm[16], audio[16], metadata[8];
    memset(pcm, 0x55, sizeof(pcm));
    memset(audio, 0xcc, sizeof(audio));
    memset(metadata, 0xcc, sizeof(metadata));
    assert(!ohaudio_vivid_copy(v, pcm, 3, 2, audio, 12, metadata, 8));
    assert(!ohaudio_vivid_copy(v, pcm, 4, 2, audio, 8, metadata, 8));
    assert(!ohaudio_vivid_copy(v, pcm, 4, 2, audio, 16, metadata, 3));
    assert(!ohaudio_vivid_copy(v, pcm, 4, 0, audio, 16, metadata, 8));
    assert(!ohaudio_vivid_copy(v, pcm, INT_MAX, 32, audio, 16, metadata, 8));
    assert(!ohaudio_vivid_copy(v, NULL, 4, 2, audio, 16, metadata, 8));
    for (int i = 0; i < 16; i++) assert(audio[i] == 0xcc);
    for (int i = 0; i < 8; i++) assert(metadata[i] == 0xcc);
    assert(ohaudio_vivid_copy(v, pcm, 4, 2, audio, 16, metadata, 8));
    assert(!memcmp(audio, pcm, sizeof(pcm)));
    assert(!memcmp(metadata, "ABCD\0\0\0\0", 8));
    free(v);
    struct vivid_speed_result result;
    for (int mode = 0; mode <= 5; mode++) {
        struct speed_mock mock = {.speed = 1, .mode = mode};
        int accepted = vivid_speed_try(&mock, 2, 1, mock_set, mock_get, &result);
        if (mode == 0) {
            assert(accepted == 1 && mock.speed == 2);
        } else if (mode == 4) {
            assert(accepted == -1); // Must not claim the old speed was restored.
        } else {
            assert(accepted == 0 && mock.speed == 1);
        }
        if (mode == 1 || mode == 2) assert(mock.sets == 1);
    }
    return 0;
}
