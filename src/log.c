#include "log.h"
#include "common.h"
#include <stdio.h>
void logWrite(logType type, const char *logInfo) {
  switch (type) {
  case ERR:
    fprintf(stderr, "[%sERROR%s] %s\n", COLOR_RED, COLOR_RESET, logInfo);
    break;

  case WRN:
    fprintf(stdout, "[%sWARN%s] %s\n", COLOR_YELLOW, COLOR_RESET, logInfo);
    break;

  case INF:
    fprintf(stdout, "[INFO] %s\n", logInfo);
    break;
  }
}
