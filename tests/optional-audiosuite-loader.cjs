// Host regression for the real loader logic with simulated platform APIs.
// Usage: node optional-audiosuite-loader.cjs <loader.h> <clang.exe> <output-dir>
const fs = require('fs');
const path = require('path');
const cp = require('child_process');
const [headerPath, clang, output] = process.argv.slice(2);
const header = fs.readFileSync(headerPath, 'utf8');
const names = [...header.matchAll(/LOAD_SUITE_SYMBOL\((OH_\w+)\);/g)].map(m => m[1]);
if (!names.length) throw new Error('No runtime symbols found');
fs.mkdirSync(output, { recursive: true });
const code = `
typedef _Bool bool;
#define true 1
#define false 0
#define RTLD_NOW 2
#define RTLD_LOCAL 0
typedef int pthread_once_t;
#define PTHREAD_ONCE_INIT 0
static int mode, missing, opens, closes, lookups;
static int dummy(void) { return 0; }
static void *dlopen(const char *name, int flags) { opens++; return mode ? (void*)1 : 0; }
static void *dlsym(void *handle, const char *name) { return ++lookups == missing ? 0 : (void*)&dummy; }
static int dlclose(void *handle) { closes++; return 0; }
static int pthread_once(int *once, void (*init)(void)) { if (!*once) { *once=1; init(); } return 0; }
void *memset(void *ptr, int v, unsigned long long n) { unsigned char *p=ptr; while(n--) *p++=v; return ptr; }
void *memcpy(void *dst, const void *src, unsigned long long n) { unsigned char *d=dst; const unsigned char *s=src; while(n--) *d++=*s++; return dst; }
${names.map(n => `int ${n}(void);`).join('\n')}
${header.replace(/^#include.*$/gm, '')}
static void reset(int available, int absent) {
    mode=available; missing=absent; opens=closes=lookups=0;
    suite_load_once=0; suite_available=false;
    memset(&suite_api, 0, sizeof(suite_api));
}
int mainCRTStartup(void) {
    reset(0, 0);
    if (audio_suite_available() || opens!=1 || lookups || closes) return 1;
    if (audio_suite_available() || opens!=1) return 2;
    for (int i=1; i<=${names.length}; i++) {
        reset(1, i);
        if (audio_suite_available() || closes!=1 || suite_api.${names[0]}) return 3;
    }
    reset(1, 0);
    if (!audio_suite_available() || closes || lookups!=${names.length}) return 4;
    if (!audio_suite_available() || opens!=1) return 5;
    return 0;
}
`;
const source = path.join(output, 'loader-test.c');
const exe = path.join(output, 'loader-test.exe');
fs.writeFileSync(source, code);
cp.execFileSync(clang, ['--target=x86_64-pc-windows-msvc', '-fuse-ld=lld', '-nostdlib', '-fno-builtin', source,
  '-o', exe, '-Wl,/entry:mainCRTStartup,/subsystem:console'], { stdio: 'inherit' });
cp.execFileSync(exe, [], { stdio: 'inherit' });
console.log('PASS: absent library, each of ' + names.length + ' absent APIs, complete API, cached initialization');
