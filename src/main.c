#include "shell.h"
int main() {
  if (shellInit() != 0) {
    goto err;
  }
  while (1) {
    shellLoop();
  }
  return 0;
err: // 使用goto进行错误处理
  return -1;
}
