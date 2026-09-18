#include <assert.h>
#include <stdlib.h>
#include "audio/out/ohaudio_vivid_packet.h"

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
    return 0;
}
