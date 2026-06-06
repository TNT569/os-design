#ifndef LOG_H
#define LOG_H
#include <stdarg.h>
typedef enum { INF, WRN, ERR } logType;
void logWrite(logType type, const char *fmt, ...)
    __attribute__((format(printf, 2, 3))); // 让编译器帮检查格式化字符串安全
#endif
