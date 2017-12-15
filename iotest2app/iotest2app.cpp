// io_test_app.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <windows.h>
#include <winioctl.h>
#define MYDEVICE L"\\\\.\\MyWDF_LINK" 
#define FILEIO_TYPE 40001
#define IOCTL_TRY \
    CTL_CODE( FILEIO_TYPE, 0x900, METHOD_IN_DIRECT, FILE_ANY_ACCESS  )

int main()
{
	HANDLE hDevice;
	BOOL bRc;
	void*  OutputBuffer;

	DWORD bytesReturned;
	 hDevice = CreateFile(MYDEVICE, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

if (hDevice == INVALID_HANDLE_VALUE) {
	printf("Error: CreatFile Failed : %d\n", GetLastError());
	return 0;
	}

OutputBuffer = malloc(sizeof(double));
bRc = DeviceIoControl(hDevice,
(DWORD)IOCTL_TRY,
0,
0,
OutputBuffer,
sizeof(double),
&bytesReturned,
NULL
);
printf("get from device %f\n",(*(double *) OutputBuffer));
CloseHandle(hDevice);
Sleep(3000);

    return 0;
}

