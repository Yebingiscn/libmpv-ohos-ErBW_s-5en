// Real host synchronization, independent of mpv's platform configuration.
#pragma once
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
typedef CRITICAL_SECTION mp_mutex;
typedef CONDITION_VARIABLE mp_cond;
typedef HANDLE mp_thread;
#define MP_THREAD_VOID DWORD WINAPI
#define MP_THREAD_RETURN() return 0
static inline int mp_mutex_init(mp_mutex *m) { InitializeCriticalSection(m); return 0; }
static inline void mp_mutex_destroy(mp_mutex *m) { DeleteCriticalSection(m); }
static inline void mp_mutex_lock(mp_mutex *m) { EnterCriticalSection(m); }
static inline void mp_mutex_unlock(mp_mutex *m) { LeaveCriticalSection(m); }
static inline int mp_cond_init(mp_cond *c) { InitializeConditionVariable(c); return 0; }
static inline void mp_cond_destroy(mp_cond *c) {}
static inline void mp_cond_signal(mp_cond *c) { WakeConditionVariable(c); }
static inline void mp_cond_wait(mp_cond *c, mp_mutex *m) { SleepConditionVariableCS(c, m, INFINITE); }
static inline void mp_cond_timedwait(mp_cond *c, mp_mutex *m, int64_t ns) { SleepConditionVariableCS(c, m, 1); }
static inline int mp_thread_create(mp_thread *t, DWORD (WINAPI *fn)(void *), void *p)
{ *t = CreateThread(NULL, 0, fn, p, 0, NULL); return !*t; }
static inline void mp_thread_join(mp_thread t) { WaitForSingleObject(t, INFINITE); CloseHandle(t); }
static inline void test_sleep(void) { Sleep(1); }
#else
#include <pthread.h>
#include <time.h>
typedef pthread_mutex_t mp_mutex;
typedef pthread_cond_t mp_cond;
typedef pthread_t mp_thread;
#define MP_THREAD_VOID void *
#define MP_THREAD_RETURN() return NULL
static inline int mp_mutex_init(mp_mutex *m) { return pthread_mutex_init(m, NULL); }
static inline void mp_mutex_destroy(mp_mutex *m) { pthread_mutex_destroy(m); }
static inline void mp_mutex_lock(mp_mutex *m) { pthread_mutex_lock(m); }
static inline void mp_mutex_unlock(mp_mutex *m) { pthread_mutex_unlock(m); }
static inline int mp_cond_init(mp_cond *c) { return pthread_cond_init(c, NULL); }
static inline void mp_cond_destroy(mp_cond *c) { pthread_cond_destroy(c); }
static inline void mp_cond_signal(mp_cond *c) { pthread_cond_signal(c); }
static inline void mp_cond_wait(mp_cond *c, mp_mutex *m) { pthread_cond_wait(c, m); }
static inline void mp_cond_timedwait(mp_cond *c, mp_mutex *m, int64_t ns)
{
    struct timespec t; clock_gettime(CLOCK_REALTIME, &t);
    t.tv_nsec += 1000000;
    if (t.tv_nsec >= 1000000000) { t.tv_sec++; t.tv_nsec -= 1000000000; }
    pthread_cond_timedwait(c, m, &t);
}
static inline int mp_thread_create(mp_thread *t, void *(*fn)(void *), void *p)
{ return pthread_create(t, NULL, fn, p); }
static inline void mp_thread_join(mp_thread t) { pthread_join(t, NULL); }
static inline void test_sleep(void) { struct timespec t = {0, 1000000}; nanosleep(&t, NULL); }
#endif
