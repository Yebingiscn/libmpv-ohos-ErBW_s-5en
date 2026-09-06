/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <assert.h>
#include <stdio.h>
#include "audio/out/ohaudio_clock.h"
#ifdef _WIN32
#include <windows.h>
#define THREAD_RESULT DWORD WINAPI
#else
#include <pthread.h>
#define THREAD_RESULT void *
#endif

static struct ohaudio_clock_snapshot snapshot;
static atomic_bool finished;

static THREAD_RESULT write_samples(void *unused)
{
    for (int64_t n = 1; n <= 200000; n++) {
        ohaudio_clock_publish(&snapshot, (struct ohaudio_clock_sample) {
            .epoch = n, .position = n * 10, .timestamp = n * 100,
            .published_at = n * 100 + 50,
        });
    }
    atomic_store(&finished, true);
    return 0;
}

int main(void)
{
    int64_t mapped = 0;
    // Large system uptime, small mpv uptime: the old comparison rejected this.
    int64_t mono = INT64_C(7000000000000), now = INT64_C(1000000000);
    assert(ohaudio_clock_translate(mono - 20000000, mono, now, &mapped));
    assert(mapped == now - 20000000);
    // Mapping follows changes in relative clock origins, including RAW drift.
    assert(ohaudio_clock_translate(mono - 20000000, mono, now + 12345, &mapped));
    assert(mapped == now + 12345 - 20000000);
    assert(!ohaudio_clock_translate(0, mono, now, &mapped));
    assert(!ohaudio_clock_translate(mono + 1, mono, now, &mapped));
    assert(!ohaudio_clock_translate(mono - OHAUDIO_CLOCK_MAX_AGE_NS, mono, now, &mapped));
    assert(!ohaudio_clock_translate(mono - 20000000, mono, 100, &mapped));

    ohaudio_clock_init(&snapshot);
    struct ohaudio_clock_sample sample = {0};
    assert(ohaudio_clock_read(&snapshot, &sample));
    assert(!ohaudio_clock_valid(sample, 1, now));
    sample = (struct ohaudio_clock_sample){1, 960, now - 20000000, now};
    ohaudio_clock_publish(&snapshot, sample);
    assert(ohaudio_clock_valid(sample, 1, now + 1000000));
    assert(!ohaudio_clock_valid(sample, 2, now)); // pause/seek invalidates old epoch
    assert(!ohaudio_clock_valid(sample, 1, now + OHAUDIO_CLOCK_MAX_AGE_NS));
    atomic_fetch_add(&snapshot.sequence, 1); // writer deliberately left in progress
    struct ohaudio_clock_sample retained = sample;
    assert(!ohaudio_clock_read(&snapshot, &retained)); // returns immediately
    assert(retained.timestamp == sample.timestamp && retained.position == sample.position);
    atomic_fetch_add(&snapshot.sequence, 1);

    ohaudio_clock_publish(&snapshot, (struct ohaudio_clock_sample){0});
    atomic_init(&finished, false);
#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, write_samples, NULL, 0, NULL);
    assert(thread);
#else
    pthread_t thread;
    assert(!pthread_create(&thread, NULL, write_samples, NULL));
#endif
    while (!atomic_load(&finished)) {
        if (ohaudio_clock_read(&snapshot, &sample) && sample.epoch) {
            assert(sample.position == (int64_t)sample.epoch * 10);
            assert(sample.timestamp == (int64_t)sample.epoch * 100);
            assert(sample.published_at == sample.timestamp + 50);
        }
    }
#ifdef _WIN32
    assert(WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0);
    CloseHandle(thread);
#else
    assert(!pthread_join(thread, NULL));
#endif
    assert(ohaudio_clock_read(&snapshot, &sample));
    assert(sample.epoch == 200000 && sample.position == 2000000);
    puts("OHAudio clock: timebase conversion, invalid/stale samples, epochs, bounded reads and concurrent publication passed");
    return 0;
}
