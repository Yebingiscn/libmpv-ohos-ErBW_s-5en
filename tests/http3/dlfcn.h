/* Windows host tests only; production uses the system dlfcn.h. */
#ifndef HTTP3_TEST_DLFCN_H
#define HTTP3_TEST_DLFCN_H
#define RTLD_NOW 2
#define RTLD_LOCAL 0
void *dlopen(const char *name, int flags);
void *dlsym(void *library, const char *name);
int dlclose(void *library);
#endif
