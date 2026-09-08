#pragma once
#include <stdint.h>
#define MP_TIME_S_TO_NS(s) ((int64_t)(s) * 1000000000)
int64_t mp_time_ns(void);
