// Test-only SDK surface. Never used for a target build.
#pragma once
#include <stdint.h>
typedef struct OH_NativeBuffer OH_NativeBuffer;
typedef enum OH_NativeBuffer_MetadataType {
    OH_VIDEO_HDR_HLG, OH_VIDEO_HDR_HDR10, OH_VIDEO_HDR_VIVID,
    OH_VIDEO_NONE = -1
} OH_NativeBuffer_MetadataType;
typedef enum OH_NativeBuffer_MetadataKey {
    OH_HDR_METADATA_TYPE, OH_HDR_STATIC_METADATA, OH_HDR_DYNAMIC_METADATA
} OH_NativeBuffer_MetadataKey;
typedef struct OH_NativeBuffer_StaticMetadata {
    float smpte2086[10];
    float cta861[2];
} OH_NativeBuffer_StaticMetadata;
#ifdef __cplusplus
extern "C" {
#endif
int32_t OH_NativeBuffer_GetMetadataValue(OH_NativeBuffer *, OH_NativeBuffer_MetadataKey,
                                        int32_t *, uint8_t **);
#ifdef __cplusplus
}
#endif
