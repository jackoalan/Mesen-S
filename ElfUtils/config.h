#ifndef MESEN_S_LUBEU
#define MESEN_S_LUBEU

#define PACKAGE_NAME "Mesen-S elfutils"
#define PACKAGE_VERSION ""
#define PACKAGE_URL ""

#define HAVE_ERR_H 1
#define HAVE_DECL_POWEROF2 1
#define HAVE_DECL_REALLOCARRAY 1

extern char *program_invocation_short_name;

#include "lib/eu-config.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/mman.h>

#ifdef _WIN32
#include <sys/types.h>
#include <io.h>
#include <intrin.h>
#define __attribute__(...)
typedef ptrdiff_t ssize_t;
ssize_t pread(int fd, void* buf, size_t count, off_t offset);
ssize_t pwrite(int fd, const void* buf, size_t count, off_t offset);
#define PROT_NONE       0
#define PROT_READ       1
#define PROT_WRITE      2
#define PROT_EXEC       4
#define MAP_FILE        0
#define MAP_SHARED      1
#define MAP_PRIVATE     2
#define MAP_TYPE        0xf
#define MAP_FIXED       0x10
#define MAP_ANONYMOUS   0x20
#define MAP_ANON        MAP_ANONYMOUS
#define MAP_FAILED      ((void *)-1)
#define MS_ASYNC        1
#define MS_SYNC         2
#define MS_INVALIDATE   4
void* mmap(void* addr, size_t len, int prot, int flags, int fildes, off_t off);
int munmap(void* addr, size_t len);
int msync(void* addr, size_t len, int flags);
size_t get_pagesize();
#define __LITTLE_ENDIAN 0
#define __BIG_ENDIAN 1
#define __BYTE_ORDER __LITTLE_ENDIAN
#define LITTLE_ENDIAN __LITTLE_ENDIAN
#define BIG_ENDIAN __BIG_ENDIAN
#define BYTE_ORDER __BYTE_ORDER
#define __alignof__ __alignof
#define __typeof__ typeof
int is_valid_fd(int fd);
#define __thread __declspec( thread )
#define bswap_16 _byteswap_ushort
#define bswap_32 _byteswap_ulong
#define bswap_64 _byteswap_uint64
#define posix_memalign(p, a, s) (((*(p)) = _aligned_malloc((s), (a))), *(p) ?0 :errno)
#define powerof2(x) (__popcnt(x) == 1)
#define htobe64(x) bswap_64(x)
#define be64toh(x) bswap_64(x)
char *stpcpy(char * to, const char * from);
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#else
#define get_pagesize() sysconf(_SC_PAGESIZE)
#define is_valid_fd(fd) (fcntl(fd, F_GETFD) != -1)
#endif

#endif // MESEN_S_LUBEU
