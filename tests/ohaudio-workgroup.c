#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "audio/out/ohaudio_workgroup.h"

struct OH_AudioResourceManager { int unused; } manager;
struct OH_AudioWorkgroup { int unused; } group;
static int opens, closes, adds, removes, starts, stops, releases, resolves;
static int fail_open, fail_resolve, fail_manager, fail_create, fail_add;
static int fail_remove, fail_start, fail_stop, fail_release, partial_create;
static uint64_t last_start, last_deadline;
static void reset_mock(void)
{
    opens = closes = adds = removes = starts = stops = releases = resolves = 0;
    fail_open = fail_resolve = fail_manager = fail_create = fail_add = 0;
    fail_remove = fail_start = fail_stop = fail_release = partial_create = 0;
}
OH_AudioCommon_Result OH_AudioManager_GetAudioResourceManager(OH_AudioResourceManager **m)
{ if (fail_manager) return -1; *m = &manager; return 0; }
OH_AudioCommon_Result OH_AudioResourceManager_CreateWorkgroup(OH_AudioResourceManager *m, const char *n, OH_AudioWorkgroup **g)
{ assert(m == &manager && !strcmp(n, "mpv-ohaudio")); if (!fail_create || partial_create) *g = &group; return fail_create ? -2 : 0; }
OH_AudioCommon_Result OH_AudioResourceManager_ReleaseWorkgroup(OH_AudioResourceManager *m, OH_AudioWorkgroup *g)
{ assert(m == &manager && g == &group); releases++; return fail_release ? -3 : 0; }
OH_AudioCommon_Result OH_AudioWorkgroup_AddCurrentThread(OH_AudioWorkgroup *g, int32_t *token)
{ assert(g == &group); adds++; *token = 0; return fail_add ? -4 : 0; }
OH_AudioCommon_Result OH_AudioWorkgroup_RemoveThread(OH_AudioWorkgroup *g, int32_t token)
{ assert(g == &group && token == 0); removes++; return fail_remove ? -5 : 0; }
OH_AudioCommon_Result OH_AudioWorkgroup_Start(OH_AudioWorkgroup *g, uint64_t s, uint64_t d)
{ assert(g == &group && d > s); starts++; last_start=s; last_deadline=d; return fail_start ? -6 : 0; }
OH_AudioCommon_Result OH_AudioWorkgroup_Stop(OH_AudioWorkgroup *g)
{ assert(g == &group); stops++; return fail_stop ? -7 : 0; }
void *dlopen(const char *n, int flags)
{ assert(!strcmp(n,"libohaudio.so") && flags == (RTLD_NOW | RTLD_LOCAL)); opens++; return fail_open ? NULL : &manager; }
void *dlsym(void *h, const char *name)
{
    assert(h == &manager);
    if (++resolves == fail_resolve) return NULL;
#define SYMBOL(n) if (!strcmp(name,#n)) return (void *)n
    SYMBOL(OH_AudioManager_GetAudioResourceManager);
    SYMBOL(OH_AudioResourceManager_CreateWorkgroup);
    SYMBOL(OH_AudioResourceManager_ReleaseWorkgroup);
    SYMBOL(OH_AudioWorkgroup_AddCurrentThread);
    SYMBOL(OH_AudioWorkgroup_RemoveThread);
    SYMBOL(OH_AudioWorkgroup_Start);
    SYMBOL(OH_AudioWorkgroup_Stop);
#undef SYMBOL
    assert(0); return NULL;
}
int dlclose(void *h) { assert(h == &manager); closes++; return 0; }

static void *callback_thread(void *arg)
{
    struct ohaudio_workgroup *w = arg;
    // Deliberately collide two callback entrants. The state machine must
    // balance cycles, skip overlaps and never wait on a condition/mutex.
    uint64_t owner = (uint64_t)(uintptr_t)pthread_self();
    for (int i = 0; i < 10000; i++)
        if (ohaudio_wg_begin(w,owner,1000,1010)) ohaudio_wg_end(w);
    return NULL;
}

int main(void)
{
    struct ohaudio_workgroup w;
    uint64_t s, d;
    assert(ohaudio_wg_deadline(1700000000,123456789,960,48000,&s,&d));
    assert(s == 1700000000123ULL && d-s == 18);
    assert(ohaudio_wg_deadline(1700000000,0,480,48000,&s,&d) && d-s == 9);
    assert(ohaudio_wg_deadline(1700000000,0,441,44100,&s,&d) && d-s == 9);
    assert(ohaudio_wg_deadline(1700000000,0,48000,48000,&s,&d) && d-s == 90);
    assert(!ohaudio_wg_deadline(1,0,1,48000,&s,&d));
    assert(!ohaudio_wg_deadline(1,0,960,0,&s,&d));
    assert(!ohaudio_wg_deadline(1,-1,960,48000,&s,&d));
    assert(!ohaudio_wg_deadline(-1,0,960,48000,&s,&d));
    assert(!ohaudio_wg_deadline(INT64_MAX,0,960,48000,&s,&d));

    for (int missing = 1; missing <= 7; missing++) {
        reset_mock(); fail_resolve = missing;
        assert(!ohaudio_wg_init(&w) && closes == 1 && !w.group);
        assert(!ohaudio_wg_begin(&w,1,1000,1010));
        assert(ohaudio_wg_uninit(&w,true) && closes == 1);
    }
    reset_mock(); fail_open=1;
    assert(!ohaudio_wg_init(&w)); assert(ohaudio_wg_uninit(&w,true)); assert(closes==0);
    reset_mock(); fail_manager=1;
    assert(!ohaudio_wg_init(&w)); assert(ohaudio_wg_uninit(&w,true) && closes==1);
    reset_mock(); fail_create=1;
    assert(!ohaudio_wg_init(&w)); assert(ohaudio_wg_uninit(&w,true) && closes==1 && releases==0);
    reset_mock(); fail_create=partial_create=1;
    assert(!ohaudio_wg_init(&w)); assert(ohaudio_wg_uninit(&w,true) && releases==1);

    // Both PCM and AudioSuite use the same callback scope; their data handling
    // must proceed whether the scheduling hint succeeds or not.
    for (int suite = 0; suite <= 1; suite++) {
        reset_mock(); assert(ohaudio_wg_init(&w));
        assert(!ohaudio_wg_begin(&w,1,1000,1000));
        assert(ohaudio_wg_begin(&w,1,1000,1010));
        assert(!ohaudio_wg_begin(&w,2,1001,1011)); // no blocking/reentrant Start
        assert(!ohaudio_wg_uninit(&w,false) && closes==0 && releases==0);
        assert(!ohaudio_wg_uninit(&w,true) && closes==0 && releases==0); // gate held
        ohaudio_wg_end(&w);
        assert(ohaudio_wg_begin(&w,1,1010,1020)); ohaudio_wg_end(&w);
        assert(adds==1 && starts==2 && stops==2 && removes==0);
        assert(ohaudio_wg_begin(&w,2,1020,1030)); ohaudio_wg_end(&w);
        assert(adds==2 && removes==1 && w.registrations==2 && w.cycles==3);
        assert(last_start==1020 && last_deadline==1030);
        assert(ohaudio_wg_uninit(&w,true) && closes==1 && removes==2 && releases==1);
        assert(ohaudio_wg_uninit(&w,true) && closes==1 && releases==1);
        (void)suite;
    }
    reset_mock(); fail_add=1; assert(ohaudio_wg_init(&w));
    assert(!ohaudio_wg_begin(&w,1,1000,1010) && adds==1 && starts==0);
    assert(!ohaudio_wg_begin(&w,1,1010,1020) && adds==1);
    assert(ohaudio_wg_uninit(&w,true));

    reset_mock(); fail_start=1; assert(ohaudio_wg_init(&w));
    assert(!ohaudio_wg_begin(&w,1,1000,1010) && stops==1);
    assert(!ohaudio_wg_begin(&w,1,1010,1020) && starts==1);
    assert(ohaudio_wg_uninit(&w,true));

    reset_mock(); assert(ohaudio_wg_init(&w));
    assert(ohaudio_wg_begin(&w,1,1000,1010)); fail_stop=1; ohaudio_wg_end(&w);
    assert(!ohaudio_wg_begin(&w,1,1010,1020) && starts==1);
    assert(!ohaudio_wg_uninit(&w,true) && closes==0 && releases==0);
    fail_stop=0; assert(ohaudio_wg_uninit(&w,true) && closes==1);

    reset_mock(); assert(ohaudio_wg_init(&w));
    assert(ohaudio_wg_begin(&w,1,1000,1010)); ohaudio_wg_end(&w); fail_remove=1;
    assert(!ohaudio_wg_begin(&w,2,1010,1020) && adds==1 && starts==1);
    assert(ohaudio_wg_uninit(&w,true) && releases==1 && closes==1);

    reset_mock(); assert(ohaudio_wg_init(&w)); fail_release=1;
    assert(!ohaudio_wg_uninit(&w,true) && closes==0);
    fail_release=0; assert(ohaudio_wg_uninit(&w,true) && closes==1);

    reset_mock(); assert(ohaudio_wg_init(&w));
    pthread_t first, second;
    assert(!pthread_create(&first,NULL,callback_thread,&w));
    assert(!pthread_create(&second,NULL,callback_thread,&w));
    assert(!pthread_join(first,NULL)); assert(!pthread_join(second,NULL));
    assert(starts > 0 && starts == stops && (uint64_t)starts == w.cycles);
    assert(ohaudio_wg_uninit(&w,true) && closes==1);
    puts("OHAudio workgroup loader, timing, lifecycle and fallback tests passed");
    return 0;
}
