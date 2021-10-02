#include <windows.h>
#include <errno.h>
#include <io.h>

#include <config.h>

int is_valid_fd(int fd)
{
	HANDLE h = (HANDLE) _get_osfhandle (fd);
	DWORD flags;
	return h != INVALID_HANDLE_VALUE && GetHandleInformation(h, &flags);
}
