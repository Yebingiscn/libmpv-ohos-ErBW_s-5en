/* Host-only ABI fixture. Production compilation uses the actual OHOS header. */
#pragma once
#include <stdint.h>
typedef struct OH_AudioResourceManager OH_AudioResourceManager;
typedef struct OH_AudioWorkgroup OH_AudioWorkgroup;
typedef int OH_AudioCommon_Result;
enum { AUDIOCOMMON_RESULT_SUCCESS = 0 };
OH_AudioCommon_Result OH_AudioManager_GetAudioResourceManager(OH_AudioResourceManager **);
OH_AudioCommon_Result OH_AudioResourceManager_CreateWorkgroup(OH_AudioResourceManager *, const char *, OH_AudioWorkgroup **);
OH_AudioCommon_Result OH_AudioResourceManager_ReleaseWorkgroup(OH_AudioResourceManager *, OH_AudioWorkgroup *);
OH_AudioCommon_Result OH_AudioWorkgroup_AddCurrentThread(OH_AudioWorkgroup *, int32_t *);
OH_AudioCommon_Result OH_AudioWorkgroup_RemoveThread(OH_AudioWorkgroup *, int32_t);
OH_AudioCommon_Result OH_AudioWorkgroup_Start(OH_AudioWorkgroup *, uint64_t, uint64_t);
OH_AudioCommon_Result OH_AudioWorkgroup_Stop(OH_AudioWorkgroup *);
