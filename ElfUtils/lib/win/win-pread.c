#include <config.h>
#include <windows.h>
#include <stdint.h>
#include <io.h>

ssize_t pread(int fd, void* buf, size_t count, off_t offset)
{
	OVERLAPPED o = { 0,0,0,0,0 };
	HANDLE fh = (HANDLE)_get_osfhandle(fd);
	uint64_t off = offset;
	DWORD bytes;
	BOOL ret;

	if (fh == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}

	o.Offset = off & 0xffffffff;
	o.OffsetHigh = (off >> 32) & 0xffffffff;

	ret = ReadFile(fh, buf, (DWORD)count, &bytes, &o);
	if (!ret) {
		errno = EIO;
		return -1;
	}

	return (ssize_t)bytes;
}

ssize_t pwrite(int fd, const void* buf, size_t count, off_t offset)
{
	OVERLAPPED o = { 0,0,0,0,0 };
	HANDLE fh = (HANDLE)_get_osfhandle(fd);
	uint64_t off = offset;
	DWORD bytes;
	BOOL ret;

	if (fh == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}

	o.Offset = off & 0xffffffff;
	o.OffsetHigh = (off >> 32) & 0xffffffff;

	ret = WriteFile(fh, buf, (DWORD)count, &bytes, &o);
	if (!ret) {
		errno = EIO;
		return -1;
	}

	return (ssize_t)bytes;
}
