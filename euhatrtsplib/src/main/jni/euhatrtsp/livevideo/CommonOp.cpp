#include "CommonOp.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <string>

using namespace std;

#ifdef EUHAT_DBG
void ivrLog(const char *format, ...)
{
	va_list argptr;
	char buf[1024 * 2];

	va_start(argptr, format);
	vsprintf(buf, format, argptr);
	va_end(argptr);

    struct timeval tv;
    int iRet = gettimeofday(&tv, NULL);
	time_t t = tv.tv_sec;
	tm* local = localtime(&t);
	char timeBuf[256];
	strftime(timeBuf, 254, "[%Y-%m-%d %H:%M:%S", local);
	sprintf(timeBuf + strlen(timeBuf), ":%d] ", (int)(tv.tv_usec / 1000));
	string dispStr = timeBuf;
	dispStr += buf;

	printf("%s", dispStr.c_str());

    char *logFileName = (char *)"/sdcard/test/log.txt";

    int fd = open(logFileName, O_CREAT|O_WRONLY|O_APPEND, 0666);
    int err = errno;
    if (fd != -1)
    {
        write(fd, dispStr.c_str(), dispStr.length());
        close(fd);
    }
}
#endif