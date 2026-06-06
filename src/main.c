#include "shell.h"
int main() {
  if (shellInit("root") != 0) {
    goto err;
  }
  shellLoop();
  return 0;
err: // 使用goto进行错误处理
  return -1;
}
