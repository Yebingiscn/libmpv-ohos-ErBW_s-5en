// Link the production C++ reader to a mock getter; no HarmonyOS device required.
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <native_buffer/native_buffer.h>
#include "video/out/ohos_vivid.h"

struct OH_NativeBuffer { int identity; };
struct Reply {
    int ret = 0;
    int32_t size = 0;
    bool allocate = true;
};
static Reply replies[3];
static int calls[3];
static int array_allocations, array_deletions;
static int32_t type_value = OH_VIDEO_HDR_VIVID;
static OH_NativeBuffer buffer = {42};

// Exact new[]/delete[] accounting catches replacement of delete[] with free,
// omitted cleanup on getter failure and double deletion.
void *operator new[](std::size_t n)
{
    void *p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    array_allocations++;
    return p;
}
void operator delete[](void *p) noexcept
{
    if (p) array_deletions++;
    std::free(p);
}
void operator delete[](void *p, std::size_t) noexcept { operator delete[](p); }

extern "C" int32_t OH_NativeBuffer_GetMetadataValue(OH_NativeBuffer *b,
    OH_NativeBuffer_MetadataKey key, int32_t *size, uint8_t **data)
{
    assert(b == &buffer && b->identity == 42);
    Reply reply = replies[key];
    calls[key]++;
    *size = reply.size;
    *data = reply.allocate ? new uint8_t[reply.size > 0 ? reply.size : 1] : nullptr;
    if (*data) {
        std::memset(*data, 0x5a + key, reply.size > 0 ? reply.size : 1);
        if (key == OH_HDR_METADATA_TYPE && reply.size == sizeof(type_value))
            std::memcpy(*data, &type_value, sizeof(type_value));
    }
    return reply.ret;
}

static void fixture()
{
    assert(array_allocations == array_deletions);
    std::memset(calls, 0, sizeof(calls));
    type_value = OH_VIDEO_HDR_VIVID;
    replies[OH_HDR_METADATA_TYPE] = {0, sizeof(OH_NativeBuffer_MetadataType), true};
    replies[OH_HDR_DYNAMIC_METADATA] = {0, 27, true};
    replies[OH_HDR_STATIC_METADATA] = {0, sizeof(OH_NativeBuffer_StaticMetadata), true};
}
static void rejected()
{
    ohos_vivid_frame out;
    std::memset(&out, 0xa5, sizeof(out));
    assert(!ohos_vivid_read(&buffer, &out));
    ohos_vivid_frame empty = {};
    assert(!std::memcmp(&out, &empty, sizeof(out)));
    assert(array_allocations == array_deletions);
}
int main()
{
    fixture();
    ohos_vivid_frame out;
    assert(ohos_vivid_read(&buffer, &out));
    assert(out.valid && out.dynamic_size == 27);
    assert(out.static_size == sizeof(OH_NativeBuffer_StaticMetadata));
    for (size_t i = 0; i < out.dynamic_size; ++i) assert(out.dynamic[i] == 0x5c);
    for (size_t i = 0; i < out.static_size; ++i) assert(out.stat[i] == 0x5b);
    assert(!out.token && !out.epoch && out.pts == 0.0);
    assert(array_allocations == array_deletions);

    fixture();
    replies[OH_HDR_STATIC_METADATA] = {-1, 0, false};
    assert(ohos_vivid_read(&buffer, &out) && out.valid && !out.static_size);
    assert(array_allocations == array_deletions);

    fixture();
    replies[OH_HDR_DYNAMIC_METADATA].size = OHOS_VIVID_MAX_METADATA;
    assert(ohos_vivid_read(&buffer, &out) && out.dynamic_size == OHOS_VIVID_MAX_METADATA);
    assert(array_allocations == array_deletions);

    fixture(); type_value = OH_VIDEO_HDR_HLG; rejected();
    assert(!calls[OH_HDR_DYNAMIC_METADATA]);
    fixture(); type_value = OH_VIDEO_HDR_HDR10; rejected();
    fixture(); type_value = 0x7fffffff; rejected();
    fixture(); replies[OH_HDR_METADATA_TYPE].size = 1; rejected();
    fixture(); replies[OH_HDR_METADATA_TYPE].ret = -1; rejected();
    fixture(); replies[OH_HDR_METADATA_TYPE].allocate = false; rejected();
    fixture(); replies[OH_HDR_DYNAMIC_METADATA].ret = -1; rejected();
    fixture(); replies[OH_HDR_DYNAMIC_METADATA].allocate = false; rejected();
    fixture(); replies[OH_HDR_DYNAMIC_METADATA].size = 0; rejected();
    fixture(); replies[OH_HDR_DYNAMIC_METADATA].size = -1; rejected();
    fixture(); replies[OH_HDR_DYNAMIC_METADATA].size = OHOS_VIVID_MAX_METADATA + 1; rejected();
    fixture(); replies[OH_HDR_STATIC_METADATA].size = OHOS_VIVID_MAX_METADATA + 1; rejected();
    fixture(); replies[OH_HDR_STATIC_METADATA].size = 1; rejected();
    fixture(); replies[OH_HDR_STATIC_METADATA].allocate = false; rejected();
    fixture();
    replies[OH_HDR_STATIC_METADATA] = {-1, 12, true};
    assert(ohos_vivid_read(&buffer, &out) && !out.static_size);
    assert(array_allocations == array_deletions);

    fixture();
    std::memset(&out, 0xa5, sizeof(out));
    assert(!ohos_vivid_read(nullptr, &out) && !out.valid && !out.dynamic_size);
    assert(!ohos_vivid_read(&buffer, nullptr));
    assert(!calls[OH_HDR_METADATA_TYPE]);
    assert(array_allocations == array_deletions);
    std::cout << "HDR Vivid native reader byte-copy, ownership and failure checks passed\n";
}
