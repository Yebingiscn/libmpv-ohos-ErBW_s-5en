/* Real nghttp3 framing/QPACK; mocked RCP transport. No network access. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "libavformat/ohos_http3.c"

static nghttp3_conn *server;
static H3Context *client;
static uint64_t next_uni, next_bidi;
static int missing_library, missing_symbol, no_close, connects, cancels, shutdowns;
static void (*on_connected)(Rcp_QuicConn *, void *);
static void (*on_closed)(Rcp_QuicConn *, void *);
static int (*on_cert)(Rcp_QuicConn *, void *, const unsigned char *const *, const size_t *, size_t);
static int received_request;

int ff_check_interrupt(AVIOInterruptCB *cb) { return cb && cb->callback && cb->callback(cb->opaque); }
int ffurl_closep(URLContext **p) {
    if (!*p) return 0;
    (*p)->prot->url_close(*p);
    av_freep(p);
    return 0;
}
static Rcp_QuicSession *mock_session(void) { return (void *)1; }
static void mock_destroy_session(Rcp_QuicSession *s) {}
static Rcp_QuicConn *mock_create(char *alpn, void *s) {
    assert(!strcmp(alpn, "h3")); client = s; next_uni = 2; next_bidi = 0; return (void *)2;
}
static int mock_option(Rcp_QuicConn *c, int key, const void *value, uint32_t len) {
    if (key == RCP_QUIC_CONN_ON_CONNECTED_FUNCTION) { assert(len == 0); on_connected = (void *)value; }
    if (key == RCP_QUIC_CONN_ON_CLOSED_FUNCTION) { assert(len == 0); on_closed = (void *)value; }
    if (key == RCP_QUIC_TLS_CERT_AUTHORITY_FUNCTION) on_cert = (void *)value;
    return 0;
}
static int mock_connect(Rcp_QuicSession *s, Rcp_QuicConn *c, const char *host, uint16_t port) {
    assert(port == 443); connects++; on_connected(c, client); return 0;
}
static int mock_close(Rcp_QuicConn *c) { cancels++; if (!no_close) on_closed(c, client); return 0; }
static int mock_open(Rcp_QuicConn *c, int direction, uint64_t *id, void *s) {
    *id = direction == RCP_QUIC_STREAM_UNI ? next_uni : next_bidi;
    if (direction == RCP_QUIC_STREAM_UNI) next_uni += 4;
    else next_bidi += 4;
    return 0;
}
static int mock_stream_option(Rcp_QuicConn *c, uint64_t id, int option, const void *val, uint32_t len) {
    return 0;
}
static int mock_send(Rcp_QuicConn *c, uint64_t id, const Rcp_QuicIoVec *iov, uint32_t count, bool fin) {
    for (unsigned i = 0; i < count; i++)
        assert(nghttp3_conn_read_stream2(server, id, iov[i].data, iov[i].length,
                                        fin && i + 1 == count, av_gettime_relative() * 1000) >= 0);
    if (!count && fin) assert(nghttp3_conn_read_stream2(server, id, NULL, 0, 1, av_gettime_relative() * 1000) >= 0);
    return 0;
}
static int mock_want(Rcp_QuicConn *c, uint64_t id) { return 0; }
static int mock_shutdown(Rcp_QuicConn *c, uint64_t id, int flags, uint64_t error) {
    assert(flags == ((id & 2) ? ((id & 1) ? 1 : 2) : 3));
    assert(error == 0x100); shutdowns++; return 0;
}
void *dlopen(const char *name, int flags) { return missing_library ? NULL : (void *)3; }
int dlclose(void *p) { return 0; }
void *dlsym(void *library, const char *name) {
    if (missing_symbol && strstr(name, "StreamWantRead")) return NULL;
#define SYMBOL(name_, fn) if (!strcmp(name, name_)) return (void *)(fn)
    SYMBOL("HMS_Rcp_QuicCreateSession", mock_session);
    SYMBOL("HMS_Rcp_QuicDestroySession", mock_destroy_session);
    SYMBOL("HMS_Rcp_QuicConnCreate", mock_create);
    SYMBOL("HMS_Rcp_QuicConnSetOpt", mock_option);
    SYMBOL("HMS_Rcp_QuicConnConnect", mock_connect);
    SYMBOL("HMS_Rcp_QuicConnDestroy", mock_close);
    SYMBOL("HMS_Rcp_QuicConnStreamOpen", mock_open);
    SYMBOL("HMS_Rcp_QuicStreamSetOpt", mock_stream_option);
    SYMBOL("HMS_Rcp_QuicConnStreamSend", mock_send);
    SYMBOL("HMS_Rcp_QuicConnStreamWantRead", mock_want);
    SYMBOL("HMS_Rcp_QuicConnStreamShutdown", mock_shutdown);
#undef SYMBOL
    return NULL;
}
static int server_header(nghttp3_conn *conn, int64_t id, int32_t token,
                         nghttp3_rcbuf *n, nghttp3_rcbuf *v, uint8_t flags, void *p, void *q) {
    nghttp3_vec name = nghttp3_rcbuf_get_buf(n), value = nghttp3_rcbuf_get_buf(v);
    if (name.len == 5 && !memcmp(name.base, "range", 5))
        assert(value.len == 8 && !memcmp(value.base, "bytes=5-", 8));
    return 0;
}
static int server_end_headers(nghttp3_conn *conn, int64_t id, int fin, void *p, void *q) {
    received_request = 1; return 0;
}
static nghttp3_ssize server_data(nghttp3_conn *conn, int64_t id, nghttp3_vec *vec,
                                size_t count, uint32_t *flags, void *p, void *q) {
    static uint8_t body[] = {0x47, 0x00, 0xff, 0x01, 0x02};
    vec[0].base = body; vec[0].len = sizeof(body); *flags = NGHTTP3_DATA_FLAG_EOF; return 1;
}
#define NV(n, v) { (uint8_t *)n, (uint8_t *)v, sizeof(n)-1, sizeof(v)-1, 0 }
static void deliver_response(void) {
    nghttp3_nv fields[] = { NV(":status", "206"), NV("content-length", "5"),
        NV("content-range", "bytes 5-9/10"), NV("content-type", "video/mp2t"), NV("set-cookie", "test=1") };
    const nghttp3_data_reader reader = { .read_data = server_data };
    assert(!nghttp3_conn_submit_response(server, client->request_stream, fields, FF_ARRAY_ELEMS(fields), &reader));
    for (;;) {
        nghttp3_vec vec[16]; Rcp_QuicIoVec iov[16];
        int64_t id; int fin; uint64_t total = 0;
        nghttp3_ssize n = nghttp3_conn_writev_stream(server, &id, &fin, vec, 16);
        assert(n >= 0); if (id < 0) break;
        for (int i = 0; i < n; i++) { iov[i].data = vec[i].base; iov[i].length = vec[i].len; total += vec[i].len; }
        Rcp_QuicStreamData data = { iov, n, fin };
        assert(data_cb(client->conn, NULL, id, &data) == total);
        assert(!nghttp3_conn_add_write_offset(server, id, total));
    }
}
static int interrupt(void *p) { return 1; }
int main(int argc, char **argv) {
    setbuf(stdout, NULL);
    puts("HTTP/3 regression: discovery");
    URLContext parent = {0}, *h = NULL;
    AVDictionary *opts = NULL;
    missing_library = argc > 1 && !strcmp(argv[1], "missing-library");
    missing_symbol = argc > 1 && !strcmp(argv[1], "missing-symbol");
    ff_ohos_http3_observe("http://example.test/a", "h3=\":443\"");
    puts("plaintext observation skipped");
    assert(ff_ohos_http3_open(&h, &parent, "example.test", 443, NULL) == AVERROR(ENOSYS));
    puts("unknown origin skipped");
    ff_ohos_http3_observe("https://example.test/a", "h3=\"elsewhere.test:443\"");
    assert(ff_ohos_http3_open(&h, &parent, "example.test", 443, NULL) == AVERROR(ENOSYS));
    ff_ohos_http3_observe("https://example.test/a", "h3=\":8443\"");
    assert(ff_ohos_http3_open(&h, &parent, "example.test", 443, NULL) == AVERROR(ENOSYS));
    ff_ohos_http3_observe("https://example.test/a", "h3=\":443\"; ma=0");
    assert(ff_ohos_http3_open(&h, &parent, "example.test", 443, NULL) == AVERROR(ENOSYS));
    ff_ohos_http3_observe("https://example.test/a", "h2=\":443\", h3=\":443\"; ma=3600");
    puts("HTTP/3 regression: loader and TLS policy");
    av_dict_set(&opts, "tls_verify", "no", 0);
    if (missing_library || missing_symbol) {
        assert(ff_ohos_http3_open(&h, &parent, "example.test", 443, opts) == AVERROR(ENOSYS));
        assert(!h && !connects && !quic_available); puts("optional loader fallback passed"); return 0;
    }
    parent.protocol_whitelist = "https,tcp";
    assert(ff_ohos_http3_open(&h, &parent, "example.test", 443, opts) == AVERROR(ENOSYS));
    parent.protocol_whitelist = NULL;
    av_dict_set(&opts, "cert_file", "custom", 0);
    assert(ff_ohos_http3_open(&h, &parent, "example.test", 443, opts) == AVERROR(ENOSYS));
    av_dict_set(&opts, "cert_file", NULL, 0);
    av_dict_set(&opts, "tls_verify", "yes", 0);
    assert(ff_ohos_http3_open(&h, &parent, "example.test", 443, opts) == AVERROR(ENOSYS));
    av_dict_set(&opts, "tls_verify", "no", 0);
    nghttp3_settings settings; nghttp3_settings_default(&settings);
    puts("HTTP/3 regression: request and response");
    const nghttp3_callbacks callbacks = {.recv_header = server_header, .end_headers = server_end_headers};
    assert(!nghttp3_conn_server_new(&server, &callbacks, &settings, NULL, NULL));
    assert(!nghttp3_conn_bind_control_stream(server, 3));
    assert(!nghttp3_conn_bind_qpack_streams(server, 7, 11));
    assert(!ff_ohos_http3_open(&h, &parent, "example.test", 443, opts));
    assert(on_cert(NULL, client, NULL, NULL, 0) == 0);
    /* Match FFmpeg's real header order: Host follows regular headers. */
    const char request[] = "GET /stream.ts HTTP/1.1\r\nUser-Agent: test\r\nRange: bytes=5-\r\nConnection: keep-alive\r\nHost: example.test\r\n\r\n";
    assert(h3_write(h, (const unsigned char *)request, strlen(request)) == strlen(request));
    assert(received_request);
    deliver_response();
    unsigned char bytes[1024] = {0}; int used = 0, ret;
    while ((ret = h3_read(h, bytes + used, sizeof(bytes) - used - 1)) != AVERROR_EOF) {
        assert(ret > 0 || ret == AVERROR(EAGAIN)); if (ret > 0) used += ret;
    }
    assert(strstr((char *)bytes, "HTTP/1.1 206\r\n"));
    assert(strstr((char *)bytes, "content-range: bytes 5-9/10\r\n"));
    assert(strstr((char *)bytes, "set-cookie: test=1\r\n"));
    unsigned char *body = (unsigned char *)strstr((char *)bytes, "\r\n\r\n") + 4;
    assert(used - (body - bytes) == 5 && body[0] == 0x47 && body[1] == 0 && body[2] == 0xff);
    assert(strstr((char *)bytes, "Connection: keep-alive\r\n"));
    for (int request_number = 2; request_number <= 8; request_number++) {
        assert(h3_write(h, (const unsigned char *)request, strlen(request)) == strlen(request));
        deliver_response(); used = 0; memset(bytes, 0, sizeof(bytes));
        while ((ret = h3_read(h, bytes + used, sizeof(bytes) - used - 1)) != AVERROR_EOF) {
            assert(ret > 0 || ret == AVERROR(EAGAIN)); if (ret > 0) used += ret;
        }
        assert(strstr((char *)bytes, request_number < 8 ? "Connection: keep-alive\r\n" : "Connection: close\r\n"));
    }
    assert(connects == 1 && client->requests == 8 && client->request_stream == 28);
    assert(h3_write(h, (const unsigned char *)request, strlen(request)) == AVERROR(EIO));
    /* Ring saturation consumes a prefix, never grows memory or loses FIN. */
    unsigned char big[H3_BLOCK * (H3_SLOTS + 1)] = {0};
    Rcp_QuicIoVec iov = {big, sizeof(big)}; Rcp_QuicStreamData data = {&iov, 1, true};
    assert(data_cb(client->conn, NULL, 0, &data) == H3_BLOCK * H3_SLOTS);
    assert(client->count == H3_SLOTS && !client->input[(client->head + H3_SLOTS - 1) % H3_SLOTS].fin);
    client->count = 0; iov.data += H3_BLOCK * H3_SLOTS; iov.length = H3_BLOCK;
    assert(data_cb(client->conn, NULL, 0, &data) == H3_BLOCK && client->input[client->head].fin);
    client->count = 0; client->eof = 0; client->output_len = client->output_pos = 0;
    client->initial_deadline = 0; client->headers_done = 0;
    assert(h3_read(h, bytes, sizeof(bytes)) == AVERROR(ETIMEDOUT));
    ffurl_closep(&h); assert(cancels == 1 && shutdowns >= 4);
    assert(ff_ohos_http3_open(&h, &parent, "example.test", 443, opts) == AVERROR(ENOSYS));
    ff_ohos_http3_observe("https://other.test/a", "h3=\":443\"");
    parent.interrupt_callback.callback = interrupt;
    assert(ff_ohos_http3_open(&h, &parent, "other.test", 443, opts) == AVERROR_EXIT);
    assert(!h); parent.interrupt_callback.callback = NULL;
    /* A runtime without a final close callback cannot trigger a use-after-free
     * or accumulate retained callback states on subsequent channel changes. */
    no_close = 1;
    assert(!ff_ohos_http3_open(&h, &parent, "other.test", 443, opts));
    H3Context *retained = client;
    ffurl_closep(&h); assert(quic_disabled);
    error_cb(NULL, retained, 0, NULL); closed_cb(NULL, retained);
    assert(ff_ohos_http3_open(&h, &parent, "other.test", 443, opts) == AVERROR(ENOSYS));
    nghttp3_conn_del(server); av_dict_free(&opts);
    puts("HTTP/3 framing, binary/Range/cookies, bounded receive, timeout/cancel and close lifetime passed");
    return 0;
}
