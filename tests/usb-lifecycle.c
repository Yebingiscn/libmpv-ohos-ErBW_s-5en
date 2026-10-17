/* Compile the actual AO functions with a deterministic usbfs completion queue. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define SLOTS 2
#define USBDEVFS_DISCARDURB 1
#define USBDEVFS_REAPURBNDELAY 2
#define MP_TIME_S_TO_NS(x) ((int64_t)(x) * 1000000000)
#define MP_TIME_MS_TO_NS(x) ((int64_t)(x) * 1000000)
#define atomic_store(p, v) (*(p) = (v))
struct usbdevfs_urb { void *usercontext; };
struct slot { struct usbdevfs_urb *urb; bool pending; };
struct priv {
    struct slot slots[SLOTS], feedback;
    bool stop, thread_live;
    int thread, fd;
};
struct driver { const char *name; };
struct ao { const struct driver *driver; bool usb_pcm_gain; float gain; };
static struct usbdevfs_urb *queue[4];
static unsigned queued, reaped, joins, delays;
static bool stall;
static int64_t now;
static int64_t mp_time_ns(void) { return now; }
static void mp_sleep_ns(int64_t duration) { now += duration; }
static int pthread_join(int thread, void *result)
{ (void)thread; (void)result; joins++; return 0; }
static int ioctl(int fd, unsigned request, void *arg)
{
    (void)fd;
    if (request == USBDEVFS_DISCARDURB) {
        assert(queued < 4);
        queue[queued++] = arg;
        return 0;
    }
    assert(request == USBDEVFS_REAPURBNDELAY);
    if (stall || delays) {
        errno = delays % 2 ? EINTR : EAGAIN;
        if (delays) delays--;
        return -1;
    }
    if (reaped == queued) { errno = EAGAIN; return -1; }
    *(struct usbdevfs_urb **)arg = queue[reaped++];
    return 0;
}
#include "usb-functions.inc"
int main(void)
{
    const struct driver usb = {"usb-exclusive"}, ordinary = {"ohaudio"};
    struct ao ao = {.driver = &usb};
    ao_set_gain(&ao, 0.125f); assert(ao.gain == 1.0f);
    ao.usb_pcm_gain = true;
    ao_set_gain(&ao, 0.125f); assert(ao.gain == 0.125f);
    ao_set_gain(&ao, 0); assert(ao.gain == 0);
    ao.usb_pcm_gain = false;
    ao_set_gain(&ao, 0); assert(ao.gain == 1.0f);
    ao.driver = &ordinary;
    ao_set_gain(&ao, 0.5f); assert(ao.gain == 0.5f);

    struct usbdevfs_urb urbs[3] = {0}, old = {0};
    struct priv p = {.thread_live = true};
    for (unsigned i = 0; i < SLOTS; i++) p.slots[i] = (struct slot){&urbs[i], true};
    p.feedback = (struct slot){&urbs[2], true};
    // A completion from a retired AO must not dereference its stale context.
    queue[queued++] = &old;
    delays = 4;
    assert(stop_thread(&p));
    assert(p.stop && !p.thread_live && joins == 1);
    assert(reaped == queued && !p.slots[0].pending && !p.slots[1].pending && !p.feedback.pending);
    assert(now >= MP_TIME_MS_TO_NS(4));
    assert(stop_thread(&p)); // repeated reset
    queued = reaped = 0;
    p.slots[0].pending = true;
    stall = true;
    assert(!stop_thread(&p)); // do not declare pending transfers reusable
    assert(p.slots[0].pending && now >= MP_TIME_S_TO_NS(2));
    puts("PASS USB gain policy, cancellation delays/EINTR, stale completion, repeated reset and timeout");
    return 0;
}
