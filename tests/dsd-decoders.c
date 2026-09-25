/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Link against the patched host libavcodec/libavutil. Optional arguments:
 * [DST length-prefixed packets,] WavPack fast, WavPack high, expected MSB DSD,
 * PCM WavPack. Without arguments tests deterministic DST and WavPack copy frames.
 */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libavcodec/avcodec.h"
#include "libavcodec/dsd.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/opt.h"

static AVCodecContext *decoder(const char *name, int raw)
{
    const AVCodec *codec = avcodec_find_decoder_by_name(name);
    assert(codec);
    AVCodecContext *c = avcodec_alloc_context3(codec);
    assert(c);
    c->sample_rate = 352800;
    av_channel_layout_default(&c->ch_layout, 2);
    c->thread_count = 1;
    c->err_recognition = AV_EF_CRCCHECK | AV_EF_EXPLODE;
    assert(av_opt_set_int(c->priv_data, "dsd_raw", raw, 0) == 0);
    assert(avcodec_open2(c, codec, NULL) == 0);
    return c;
}
static AVFrame *decode(AVCodecContext *c, const uint8_t *data, int size)
{
    AVPacket *p = av_packet_alloc();
    AVFrame *f = av_frame_alloc();
    assert(p && f && av_new_packet(p, size) == 0);
    memcpy(p->data, data, size);
    p->pts = 0;
    assert(avcodec_send_packet(c, p) == 0);
    av_packet_free(&p);
    assert(avcodec_receive_frame(c, f) == 0);
    return f;
}
static void raw_tag(AVFrame *f)
{
    AVDictionaryEntry *tag = av_dict_get(f->metadata, "sweetvideo.dsd_raw", NULL, 0);
    assert(tag && !strcmp(tag->value, "msb-u8-v1"));
    assert(f->sample_rate == 352800 && f->ch_layout.nb_channels == 2);
}
static void wavpack_copy(void)
{
    uint8_t packet[100] = {0};
    memcpy(packet, "wvpk", 4); AV_WL32(packet+4, 92); AV_WL16(packet+8, 0x410);
    AV_WL32(packet+12, 32); AV_WL32(packet+20, 32);
    AV_WL32(packet+24, 0x80000000u | (9u<<23) | 0x1800);
    packet[32]=14; packet[33]=33; packet[34]=3; packet[35]=0;
    uint32_t crc=UINT32_MAX;
    for(int i=0;i<64;i++) {packet[36+i]=(uint8_t)(i*53);crc=crc*3+packet[36+i];}
    AV_WL32(packet+28,crc);
    AVCodecContext *c=decoder("wavpack",1);
    AVFrame *f=decode(c,packet,sizeof(packet));raw_tag(f);
    assert(f->format==AV_SAMPLE_FMT_U8P && f->nb_samples==32);
    for(int i=0;i<32;i++)for(int ch=0;ch<2;ch++) assert(f->extended_data[ch][i]==packet[36+i*2+ch]);
    av_frame_free(&f);avcodec_free_context(&c);
}
static void dst_uncoded(void)
{
    const int samples = 4704, bytes = samples * 2;
    uint8_t *data = calloc(1, bytes + 1);
    assert(data);
    for (int i = 0; i < bytes; i++) data[i+1] = (uint8_t)(i * 29);
    AVCodecContext *c = decoder("dst", 1);
    AVFrame *f = decode(c, data, bytes + 1);
    raw_tag(f);
    assert(f->format == AV_SAMPLE_FMT_U8 && f->nb_samples == samples);
    assert(!memcmp(f->data[0], data+1, bytes));
    av_frame_free(&f);
    avcodec_flush_buffers(c);
    AVPacket *bad = av_packet_alloc();
    assert(av_new_packet(bad, bytes) == 0);
    memcpy(bad->data, data, bytes);
    assert(avcodec_send_packet(c, bad) < 0); // truncated uncoded frame
    av_packet_free(&bad);
    avcodec_free_context(&c);
    free(data);
}
static uint8_t *read_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb"); assert(f);
    assert(!fseek(f, 0, SEEK_END)); long len = ftell(f); assert(len > 0);
    assert(!fseek(f, 0, SEEK_SET)); *size = (size_t)len;
    uint8_t *p = malloc(*size); assert(p && fread(p,1,*size,f) == *size);
    fclose(f); return p;
}
static void dst_compressed(const char *path)
{
    size_t size; uint8_t *data = read_file(path, &size);
    AVCodecContext *raw = decoder("dst", 1), *pcm = decoder("dst", 0);
    DSDContext states[2] = {0};
    for (int c=0;c<2;c++) memset(states[c].buf,0x69,sizeof(states[c].buf));
    ff_init_dsd_data();
    unsigned packets=0;
    for (size_t pos=0;pos<size;) {
        assert(size-pos>=4); unsigned len=AV_RL32(data+pos);pos+=4;
        assert(len>0 && len<=size-pos && len<=INT_MAX);
        AVFrame *a=decode(raw,data+pos,(int)len),*b=decode(pcm,data+pos,(int)len);
        raw_tag(a); assert(a->nb_samples == b->nb_samples);
        float *reference=calloc((size_t)a->nb_samples*2,sizeof(float)); assert(reference);
        for(int c=0;c<2;c++) ff_dsd2pcm_translate(&states[c],a->nb_samples,0,a->data[0]+c,2,reference+c,2);
        assert(b->format == AV_SAMPLE_FMT_FLT);
        assert(!memcmp(reference,b->data[0],(size_t)a->nb_samples*2*sizeof(float)));
        free(reference);av_frame_free(&a);av_frame_free(&b);pos+=len;packets++;
    }
    assert(packets);printf("DST compressed packets: %u, raw DSD -> PCM matches default decoder exactly\n",packets);
    free(data);avcodec_free_context(&raw);avcodec_free_context(&pcm);
}
static void wavpack(const char *path, const char *reference)
{
    size_t size,refsize;uint8_t *data=read_file(path,&size),*ref=read_file(reference,&refsize);
    AVCodecContext *c=decoder("wavpack",1);
    size_t offset=0; unsigned blocks=0;
    for(size_t pos=0;pos+32<=size && !memcmp(data+pos,"wvpk",4);) {
        size_t len=(size_t)AV_RL32(data+pos+4)+8;
        assert(len>=32 && len<=size-pos && len<=INT_MAX);
        if(AV_RL32(data+pos+20)) {
            AVFrame *f=decode(c,data+pos,(int)len);raw_tag(f);
            assert(f->format==AV_SAMPLE_FMT_U8P && offset+(size_t)f->nb_samples*2<=refsize);
            for(int i=0;i<f->nb_samples;i++) for(int ch=0;ch<2;ch++)
                assert(f->extended_data[ch][i]==ref[offset+(size_t)i*2+ch]);
            offset+=(size_t)f->nb_samples*2;av_frame_free(&f);blocks++;
        }
        pos+=len;
    }
    assert(offset==refsize && blocks);
    avcodec_flush_buffers(c);
    size_t len=(size_t)AV_RL32(data+4)+8;
    AVPacket *bad=av_packet_alloc();assert(bad && av_new_packet(bad,(int)len)==0);
    memcpy(bad->data,data,len);bad->data[28]^=1;
    int ret=avcodec_send_packet(c,bad);
    if(ret>=0) { AVFrame *f=av_frame_alloc();ret=avcodec_receive_frame(c,f);av_frame_free(&f); }
    assert(ret<0);av_packet_free(&bad);
    printf("WavPack %s: %zu original DSD bytes identical, corrupt CRC rejected\n",path,offset);
    free(data);free(ref);avcodec_free_context(&c);
}
static void wavpack_pcm(const char *path)
{
    size_t size;uint8_t *data=read_file(path,&size);
    for(int raw=0;raw<=1;raw++) {
        AVCodecContext *c=decoder("wavpack",raw);unsigned offset=0;
        for(size_t pos=0;pos+32<=size && !memcmp(data+pos,"wvpk",4);) {
            size_t len=(size_t)AV_RL32(data+pos+4)+8;
            assert(len>=32 && len<=size-pos && len<=INT_MAX);
            if(AV_RL32(data+pos+20)) {
                AVFrame *f=decode(c,data+pos,(int)len);
                assert(!av_dict_get(f->metadata,"sweetvideo.dsd_raw",NULL,0));
                assert(f->format==AV_SAMPLE_FMT_S16P && f->sample_rate==44100 && f->ch_layout.nb_channels==2);
                for(int i=0;i<f->nb_samples;i++)for(int ch=0;ch<2;ch++) {
                    int expected=(((offset+i)*29+ch*101)&65535)-32768;
                    assert(((int16_t *)f->extended_data[ch])[i]==expected);
                }
                offset+=f->nb_samples;av_frame_free(&f);
            }
            pos+=len;
        }
        assert(offset==4096);avcodec_free_context(&c);
    }
    free(data);puts("PCM WavPack: unchanged with dsd_raw off/on");
}
int main(int argc,char **argv)
{
    dst_uncoded();wavpack_copy();
    if(argc==6) {dst_compressed(argv[1]);argv++;argc--;}
    if(argc==5) {wavpack(argv[1],argv[3]);wavpack(argv[2],argv[3]);wavpack_pcm(argv[4]);}
    else assert(argc==1);
    puts("DSD decoder tests passed");return 0;
}
