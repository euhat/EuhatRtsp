#pragma once

#ifdef EUHAT_DBG
void ivrLog(const char *format, ...);
#define DBG(_param) ivrLog _param
#else
#define DBG(_param)
#endif