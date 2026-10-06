// Compile/link against the pinned nghttp3 archive with --no-undefined.
// Refer to every nghttp3 API imported by the broken libmpv artifact.
#include <nghttp3/nghttp3.h>
const void *const http3_link_probe[] = {
    nghttp3_conn_add_write_offset,
    nghttp3_conn_bind_control_stream,
    nghttp3_conn_bind_qpack_streams,
    nghttp3_conn_client_new_versioned,
    nghttp3_conn_del,
    nghttp3_conn_read_stream2,
    nghttp3_conn_submit_request,
    nghttp3_conn_writev_stream,
    nghttp3_rcbuf_get_buf,
    nghttp3_settings_default_versioned,
};
