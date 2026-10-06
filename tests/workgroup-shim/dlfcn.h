/* Host-only loader fixture; no extra production dependency. */
#pragma once
#define RTLD_NOW 2
#define RTLD_LOCAL 0
void *dlopen(const char *, int);
void *dlsym(void *, const char *);
int dlclose(void *);
