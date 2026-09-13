/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <assert.h>
#include <stdio.h>
#include "audio/out/ohaudio_playhead.h"
int main(void)
{
    // After seek, a cached zero-position timestamp may already be 200ms old.
    // None of the submitted PCM may be counted as played from its age alone.
    assert(ohaudio_playhead(0, 200000000, 48000, 9600) == 0);
    assert(ohaudio_playhead(0, 400000000, 48000, 19200) == 0);
    assert(ohaudio_playhead(0, 0, 48000, 480) == 0);
    // Once hardware advances, retain the normal timestamp interpolation.
    assert(ohaudio_playhead(480, 10000000, 48000, 9600) == 960);
    assert(ohaudio_playhead(441, 10000000, 44100, 4410) == 882);
    // Never claim to have played beyond the PCM actually returned by callbacks.
    assert(ohaudio_playhead(480, 500000000, 48000, 9600) == 9600);
    // A second seek must not inherit the first stream's inferred progress.
    assert(ohaudio_playhead(0, 250000000, 48000, 12000) == 0);
    puts("OHAudio startup clock: passed");
}
