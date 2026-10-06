// Compile the actual callback extracted from the patched source, not a rewritten
// approximation. Mock only the external audio/clock services.
#define main workgroup_core_cases
#include "ohaudio-workgroup.c"
#undef main
#include <time.h>

typedef struct OH_AudioRenderer OH_AudioRenderer;
struct ohaudio_clock_sample { int64_t position, timestamp; };
struct ohaudio_clock_snapshot { int unused; };
struct priv {
    bool suite_active;
    int suite_output_stride, suite_output_rate;
    atomic_uint_fast64_t stream_epoch, clock_epoch;
    uint64_t callback_stream_epoch;
    int64_t frame_position, written_frames, queued_frames, suite_end_time;
    bool timestamp_valid;
    struct ohaudio_clock_snapshot snapshot;
    struct ohaudio_clock_sample callback_snapshot;
    struct ohaudio_workgroup workgroup;
};
struct ao { struct priv *priv; int sstride, samplerate; };
#define MP_TIME_S_TO_NS(s) ((int64_t)(s) * 1000000000LL)
#define MPMIN(a,b) ((a)<(b)?(a):(b))
#define MPCLAMP(a,l,h) ((a)<(l)?(l):((a)>(h)?(h):(a)))
static int64_t fake_mpv_ns = 1000000000LL, read_end_time;
static int no_audio;
static long thread_id = 42;
static int64_t mp_time_ns(void) { return fake_mpv_ns; }
static void ohaudio_clock_read(struct ohaudio_clock_snapshot *s, struct ohaudio_clock_sample *out)
{ (void)s; *out = (struct ohaudio_clock_sample){0}; }
static bool ohaudio_clock_valid(struct ohaudio_clock_sample s, uint64_t epoch, int64_t now)
{ (void)s; (void)epoch; (void)now; return false; }
static int64_t ohaudio_playhead(int64_t p, int64_t e, int r, int64_t w)
{ (void)e; (void)r; (void)w; return p; }
static int ao_read_data_nonblocking(struct ao *ao, void **data, int samples,
    int64_t end, void *eof, bool pad, void *contended)
{
    (void)eof; (void)contended; assert(!pad);
    read_end_time = end;
    int copied = no_audio ? 0 : samples / 2;
    memset(*data, 0x5a, copied * ao->sstride);
    return copied;
}
static int render_audio_suite(struct ao *ao, void *data, int bytes)
{
    read_end_time = ao->priv->suite_end_time;
    int copied = no_audio ? 0 : bytes / 2;
    memset(data, 0x5a, copied);
    return copied;
}
#define SYS_gettid 1
static long mock_gettid(int number) { assert(number==SYS_gettid); return thread_id; }
#define syscall mock_gettid
#include "workgroup-callback.inc"

int main(void)
{
    for (int suite=0; suite<=1; suite++) {
        // Available, missing capability, and failed Start must all produce the
        // exact same PCM bytes/count and the original mpv presentation time.
        for (int failure=0; failure<=2; failure++) {
            reset_mock(); no_audio=0; thread_id=42;
            fail_open = failure==1; fail_start = failure==2;
            struct priv p = { .suite_active=suite, .suite_output_stride=8,
                             .suite_output_rate=96000 };
            struct ao ao = { .priv=&p, .sstride=4, .samplerate=48000 };
            atomic_init(&p.stream_epoch,1); atomic_init(&p.clock_epoch,1);
            bool available = ohaudio_wg_init(&p.workgroup);
            assert(available == (failure!=1));
            unsigned char audio[3840]; memset(audio,0xcc,sizeof(audio));
            int copied=audio_on_write_callback(NULL,&ao,audio,sizeof(audio));
            assert(copied==1920);
            for (int i=0;i<copied;i++) assert(audio[i]==0x5a);
            for (int i=copied;i<(int)sizeof(audio);i++) assert(audio[i]==0xcc);
            int rate=suite?96000:48000, stride=suite?8:4;
            int samples=(int)sizeof(audio)/stride;
            assert(read_end_time==fake_mpv_ns+MP_TIME_S_TO_NS(samples)/rate);
            if (failure!=1) {
                assert(starts==1 && stops==1);
                assert(last_start>1000000000000ULL); // epoch-ms, not mpv's 1s origin
                assert(last_deadline-last_start==(uint64_t)(suite?4:18));
            }
            // Reset the audio epoch without recreating/registering the same
            // callback thread. The WG must not contaminate A/V clock rebasing.
            atomic_fetch_add(&p.stream_epoch,1);
            audio_on_write_callback(NULL,&ao,audio,sizeof(audio));
            assert(p.written_frames==1920/stride);
            assert(adds==(failure==1?0:1));
            no_audio=1;
            assert(audio_on_write_callback(NULL,&ao,audio,sizeof(audio))==0);
            assert(!p.workgroup.in_cycle);
            if (failure==0) {
                thread_id=43;
                audio_on_write_callback(NULL,&ao,audio,sizeof(audio));
                assert(adds==2 && removes==1 && starts==stops);
            }
            int before=starts;
            assert(audio_on_write_callback(NULL,&ao,audio,0)==0 && starts==before);
            assert(ohaudio_wg_uninit(&p.workgroup,true));
        }
    }
    puts("Actual PCM/Suite callback bytes, counts, reset and fallback tests passed");
}
