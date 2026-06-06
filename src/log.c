#include "log.h"
#include "common.h" // ← 注意：你原代码拼写为 commom.h，请确认实际文件名
#include <stdarg.h>
#include <stdio.h>

void logWrite(logType type, const char *fmt, ...) {
  FILE *out;
  const char *prefix;
  const char *colorStart = "";
  const char *colorEnd = "";

  // 统一处理输出流和前缀，减少重复代码
  switch (type) {
  case ERR:
    out = stderr;
    colorStart = COLOR_RED;
    colorEnd = COLOR_RESET;
    prefix = "ERROR";
    break;
  case WRN:
    out = stdout;
    colorStart = COLOR_YELLOW;
    colorEnd = COLOR_RESET;
    prefix = "WARN";
    break;
  case INF:
  default:
    out = stdout;
    prefix = "INFO";
    break;
  }

  // 用 vfprintf 替代 fprintf，透传格式化能力
  fprintf(out, "[%s%s%s] ", colorStart, prefix, colorEnd);

  va_list args;
  va_start(args, fmt);
  vfprintf(out, fmt, args); // ← 格式化在这里完成
  va_end(args);

  fprintf(out, "\n");
}
