// Exercise the exact production pool with a mock GLES driver, without a GPU.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned int GLuint;
typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef int GLsizei;
typedef intptr_t GLintptr;
typedef intptr_t GLsizeiptr;
typedef unsigned char GLboolean;
typedef uint64_t GLuint64;
struct sync { int state; };
typedef struct sync *GLsync;
enum { GL_PIXEL_UNPACK_BUFFER = 1, GL_STREAM_DRAW, GL_MAP_WRITE_BIT = 4,
       GL_MAP_UNSYNCHRONIZED_BIT = 8, GL_TIMEOUT_EXPIRED = 20,
       GL_ALREADY_SIGNALED, GL_CONDITION_SATISFIED, GL_WAIT_FAILED,
       GL_SYNC_GPU_COMMANDS_COMPLETE };
typedef struct {
    GLenum (*ClientWaitSync)(GLsync, GLbitfield, GLuint64);
    void (*DeleteSync)(GLsync);
    void (*GenBuffers)(GLsizei, GLuint *);
    void (*BindBuffer)(GLenum, GLuint);
    void (*BufferData)(GLenum, GLsizeiptr, const void *, GLenum);
    void *(*MapBufferRange)(GLenum, GLintptr, GLsizeiptr, GLbitfield);
    GLboolean (*UnmapBuffer)(GLenum);
    GLsync (*FenceSync)(GLenum, GLbitfield);
    void (*DeleteBuffers)(GLsizei, const GLuint *);
} gl_funcs;
#include "opengl/upload_pool.h"

struct buffer { unsigned char *data; size_t capacity; } buffers[128];
static GLuint bound, next_buffer;
static int allocations, deletes, maps, live_fences;
static bool fail_map, fail_unmap, fail_fence, fail_gen;
static GLenum wait_sync(GLsync s, GLbitfield flags, GLuint64 timeout)
{
    assert(flags == 0 && timeout == 0); // No GPU waits on the render thread.
    return s->state;
}
static void delete_sync(GLsync s) { live_fences--; free(s); }
static void gen_buffers(GLsizei n, GLuint *b)
{
    assert(n == 1);
    *b = fail_gen ? 0 : ++next_buffer;
    assert(next_buffer < 128);
}
static void bind_buffer(GLenum target, GLuint b)
{ assert(target == GL_PIXEL_UNPACK_BUFFER); bound = b; }
static void buffer_data(GLenum target, GLsizeiptr size, const void *ptr, GLenum usage)
{
    assert(target == GL_PIXEL_UNPACK_BUFFER && bound && !ptr && usage == GL_STREAM_DRAW);
    buffers[bound].data = realloc(buffers[bound].data, size);
    assert(buffers[bound].data);
    buffers[bound].capacity = size;
    allocations++;
}
static void *map_buffer(GLenum target, GLintptr offset, GLsizeiptr size, GLbitfield flags)
{
    assert(target == GL_PIXEL_UNPACK_BUFFER && bound && !offset);
    assert((size_t)size <= buffers[bound].capacity);
    assert(flags == (GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT));
    maps++;
    return fail_map ? NULL : buffers[bound].data;
}
static GLboolean unmap_buffer(GLenum target)
{ assert(target == GL_PIXEL_UNPACK_BUFFER && bound); return !fail_unmap; }
static GLsync fence_sync(GLenum condition, GLbitfield flags)
{
    assert(condition == GL_SYNC_GPU_COMMANDS_COMPLETE && !flags);
    if (fail_fence) return NULL;
    GLsync s = malloc(sizeof(*s));
    assert(s);
    s->state = GL_TIMEOUT_EXPIRED;
    live_fences++;
    return s;
}
static void delete_buffers(GLsizei n, const GLuint *b)
{
    assert(n == 1 && *b);
    free(buffers[*b].data);
    buffers[*b] = (struct buffer){0};
    deletes++;
}
static const gl_funcs gl = {
    wait_sync, delete_sync, gen_buffers, bind_buffer, buffer_data,
    map_buffer, unmap_buffer, fence_sync, delete_buffers
};
static void cleanup(struct gles_upload_pool *p)
{
    gles_upload_destroy(p, &gl);
    assert(!p->allocated && !live_fences);
    fail_map = fail_unmap = fail_fence = fail_gen = false;
    bound = 0;
}
int main(void)
{
    unsigned char *pixels = malloc(GLES_UPLOAD_MAX + 1u);
    assert(pixels);
    memset(pixels, 0x5a, GLES_UPLOAD_MAX + 1u);
    struct gles_upload_pool pool = {0};
    assert(gles_upload_prepare(&pool, &gl, NULL, GLES_UPLOAD_MIN) == -1);
    assert(gles_upload_prepare(&pool, &gl, pixels, GLES_UPLOAD_MIN - 1) == -1);
    assert(gles_upload_prepare(&pool, &gl, pixels, GLES_UPLOAD_MAX + 1u) == -1);
    assert(allocations == 0);

    // Fill all slots with in-flight work: no overwrite, wait, or new allocation.
    for (int i = 0; i < GLES_UPLOAD_SLOTS; i++) {
        int slot = gles_upload_prepare(&pool, &gl, pixels, GLES_UPLOAD_MIN);
        assert(slot >= 0 && !memcmp(pixels, buffers[bound].data, GLES_UPLOAD_MIN));
        assert(gles_upload_submit(&pool, &gl, slot));
        bound = 0; // Real uploader unbinds after TexSubImage2D.
    }
    int before = allocations, before_maps = maps;
    assert(gles_upload_prepare(&pool, &gl, pixels, GLES_UPLOAD_MIN) == -1);
    assert(allocations == before && maps == before_maps && !pool.disabled);
    pool.slots[2].fence->state = GL_ALREADY_SIGNALED;
    assert(gles_upload_prepare(&pool, &gl, pixels, GLES_UPLOAD_MIN) == 2);
    assert(allocations == before); // Reuse completed storage.
    assert(gles_upload_submit(&pool, &gl, 2));
    cleanup(&pool);

    // Sustained completed transfers must not allocate a fresh PBO each frame.
    before = allocations;
    for (int frame = 0; frame < 1000; frame++) {
        int reused = gles_upload_prepare(&pool, &gl, pixels, GLES_UPLOAD_MIN);
        assert(reused >= 0);
        assert(gles_upload_submit(&pool, &gl, reused));
        pool.slots[reused].fence->state = GL_CONDITION_SATISFIED;
        bound = 0;
    }
    assert(allocations == before + 1);
    cleanup(&pool);

    // Total allocation remains bounded even for large/high-bit-depth planes.
    for (int i = 0; i < 4; i++) {
        int slot = gles_upload_prepare(&pool, &gl, pixels, GLES_UPLOAD_MAX);
        assert(slot >= 0 && gles_upload_submit(&pool, &gl, slot));
    }
    assert(pool.allocated == GLES_UPLOAD_BUDGET);
    assert(gles_upload_prepare(&pool, &gl, pixels, GLES_UPLOAD_MIN) == -1);
    cleanup(&pool);

    // Resolution/stride growth only reallocates completed storage.
    int slot = gles_upload_prepare(&pool, &gl, pixels, GLES_UPLOAD_MIN);
    assert(slot >= 0 && gles_upload_submit(&pool, &gl, slot));
    pool.slots[slot].fence->state = GL_CONDITION_SATISFIED;
    assert(gles_upload_prepare(&pool, &gl, pixels, GLES_UPLOAD_MIN * 2) == slot);
    assert(pool.allocated == GLES_UPLOAD_MIN * 2);
    cleanup(&pool);

    for (int failure = 0; failure < 4; failure++) {
        fail_gen = failure == 0; fail_map = failure == 1; fail_unmap = failure == 2;
        slot = gles_upload_prepare(&pool, &gl, pixels, GLES_UPLOAD_MIN);
        if (failure == 3) {
            assert(slot >= 0);
            fail_fence = true;
            assert(!gles_upload_submit(&pool, &gl, slot));
        } else {
            assert(slot == -1 && !bound);
        }
        assert(pool.disabled);
        assert(gles_upload_prepare(&pool, &gl, pixels, GLES_UPLOAD_MIN) == -1);
        cleanup(&pool);
    }
    slot = gles_upload_prepare(&pool, &gl, pixels, GLES_UPLOAD_MIN);
    assert(slot >= 0 && gles_upload_submit(&pool, &gl, slot));
    pool.slots[slot].fence->state = GL_WAIT_FAILED;
    assert(gles_upload_prepare(&pool, &gl, pixels, GLES_UPLOAD_MIN) == -1 && pool.disabled);
    cleanup(&pool);
    assert(deletes == (int)next_buffer);
    free(pixels);
    puts("GLES upload pool ownership, reuse, budget and failure tests passed");
}
