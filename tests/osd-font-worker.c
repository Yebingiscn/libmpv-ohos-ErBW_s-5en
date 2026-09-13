// Exercise the actual worker/start functions with a blocking fake font provider.
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#include <process.h>
typedef CRITICAL_SECTION mp_mutex;
typedef CONDITION_VARIABLE test_cond;
typedef HANDLE mp_thread;
#define MP_THREAD_VOID unsigned __stdcall
#define MP_THREAD_RETURN() return 0
static void mutex_init(mp_mutex *m) { InitializeCriticalSection(m); }
static void mutex_destroy(mp_mutex *m) { DeleteCriticalSection(m); }
static void mp_mutex_lock(mp_mutex *m) { EnterCriticalSection(m); }
static void mp_mutex_unlock(mp_mutex *m) { LeaveCriticalSection(m); }
static void cond_init(test_cond *c) { InitializeConditionVariable(c); }
static void cond_wait(test_cond *c, mp_mutex *m) { SleepConditionVariableCS(c, m, INFINITE); }
static void cond_signal(test_cond *c) { WakeAllConditionVariable(c); }
static int mp_thread_create(mp_thread *t, unsigned (__stdcall *fn)(void *), void *p) {
    *t = (HANDLE)_beginthreadex(NULL, 0, fn, p, 0, NULL); return !*t;
}
static void mp_thread_join(mp_thread t) { WaitForSingleObject(t, INFINITE); CloseHandle(t); }
#else
#include <pthread.h>
typedef pthread_mutex_t mp_mutex;
typedef pthread_cond_t test_cond;
typedef pthread_t mp_thread;
#define MP_THREAD_VOID void *
#define MP_THREAD_RETURN() return NULL
static void mutex_init(mp_mutex *m) { pthread_mutex_init(m, NULL); }
static void mutex_destroy(mp_mutex *m) { pthread_mutex_destroy(m); }
static void mp_mutex_lock(mp_mutex *m) { pthread_mutex_lock(m); }
static void mp_mutex_unlock(mp_mutex *m) { pthread_mutex_unlock(m); }
static void cond_init(test_cond *c) { pthread_cond_init(c, NULL); }
static void cond_wait(test_cond *c, mp_mutex *m) { pthread_cond_wait(c, m); }
static void cond_signal(test_cond *c) { pthread_cond_broadcast(c); }
static int mp_thread_create(mp_thread *t, void *(*fn)(void *), void *p) {
    return pthread_create(t, NULL, fn, p);
}
static void mp_thread_join(mp_thread t) { pthread_join(t, NULL); }
#endif
#define MP_DBG(...) ((void)0)
#define MP_WARN(...) ((void)0)
enum { OSDTYPE_OSD, OSDTYPE_EXTERNAL };
struct ass_state { void *render; };
struct osd_external { struct ass_state ass; struct { int64_t id; } ov; };
struct osd_object {
    struct ass_state ass; struct osd_external **externals; int num_externals;
    bool osd_changed, changed; int vo_change_id;
};
struct osd_state {
    mp_mutex lock; mp_thread font_thread; bool font_thread_started, fonts_pending;
    bool want_redraw_notification; void *global, *opts_cache;
    struct osd_object *objs[2]; struct ass_state external_preload;
};
static mp_mutex gate;
static test_cond condition;
static bool entered, released;
static int allocated, freed;
static int64_t mp_time_ns(void) { return 1; }
static void m_config_cache_update(void *p) { (void)p; }
static struct osd_state *osd_create(void *global) {
    struct osd_state *s = calloc(1, sizeof(*s)); s->global = global;
    mutex_init(&s->lock);
    for (int i=0; i<2; i++) s->objs[i] = calloc(1, sizeof(*s->objs[i]));
    return s;
}
static void release_renderer(struct ass_state *a) {
    if (a->render) { free(a->render); a->render = NULL; freed++; }
}
static void osd_free(struct osd_state *s) {
    if (s->font_thread_started) mp_thread_join(s->font_thread);
    for (int i=0; i<2; i++) {
        release_renderer(&s->objs[i]->ass);
        for (int j=0; j<s->objs[i]->num_externals; j++)
            release_renderer(&s->objs[i]->externals[j]->ass);
        free(s->objs[i]);
    }
    release_renderer(&s->external_preload);
    mutex_destroy(&s->lock); free(s);
}
static void create_ass_renderer(struct osd_state *s, struct ass_state *a) {
    (void)s;
    mp_mutex_lock(&gate); entered = true; cond_signal(&condition);
    while (!released) cond_wait(&condition, &gate);
    mp_mutex_unlock(&gate);
    a->render = malloc(1); assert(a->render); allocated++;
}
#include "osd-font-worker.inc"
int main(void) {
    mutex_init(&gate); cond_init(&condition);
    for (int pending_overlay=0; pending_overlay<2; pending_overlay++) {
        entered = released = false;
        struct osd_state *s = osd_create(NULL);
        osd_preload_fonts(s); // Must return while font setup is blocked.
        mp_mutex_lock(&gate);
        while (!entered) cond_wait(&condition, &gate);
        mp_mutex_unlock(&gate);
        struct osd_external overlay = {0};
        struct osd_external *entries[] = {&overlay};
        // Playback/overlay updates must still be able to acquire the OSD lock.
        mp_mutex_lock(&s->lock);
        assert(s->fonts_pending);
        if (pending_overlay) {
            s->objs[OSDTYPE_EXTERNAL]->externals = entries;
            s->objs[OSDTYPE_EXTERNAL]->num_externals = 1;
        }
        mp_mutex_unlock(&s->lock);
        osd_preload_fonts(s); // Repeated start must not create a second worker.
        mp_mutex_lock(&gate); released = true; cond_signal(&condition); mp_mutex_unlock(&gate);
        mp_thread_join(s->font_thread); s->font_thread_started = false;
        assert(!s->fonts_pending && s->want_redraw_notification);
        assert(s->objs[OSDTYPE_OSD]->ass.render);
        assert(pending_overlay ? !!overlay.ass.render : !!s->external_preload.render);
        assert(allocated == (pending_overlay + 1) * 2);
        osd_free(s);
        assert(freed == allocated); // Moved renderers are released exactly once.
    }
    puts("OSD font worker: passed");
}
