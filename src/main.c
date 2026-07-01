#include "buffer.h"
#include "disk.h"
#include "fs.h"
#include "log.h"
#include "persistence.h"
#include "shell.h"
#include "viz.h"

int main(void) {
  if (restoreDisk("disk.img") != 0) {
    logWrite(WRN, "Disk image \"disk.img\" may not exist");
    logWrite(WRN, "Inititalizing Disk and FileSystem");
    if (initDisk() != 0) {
      logWrite(ERR, "Disk initialization failed");
      goto err;
    }
    if (initFS() != 0) {
      logWrite(ERR, "File system initialization failed");
      goto err;
    }
  }
  /* 初始化各子系统 */

  initBuffer();

  /* 注册文件系统命令 */
  shellRegister("mkfile", doMkfile,
                UNDERLINE "mkfile <path> <size>\n" COLOR_RESET
                          "Create a file with given size");
  shellRegister("mkdir", doMkdir,
                UNDERLINE "mkdir <path>\n" COLOR_RESET "Create a subdirectory");
  shellRegister("rm", doRm,
                UNDERLINE "rm <path>\n" COLOR_RESET "Delete a file");
  shellRegister("rmdir", doRmdir,
                UNDERLINE "rmdir <path>\n" COLOR_RESET
                          "Delete a subdirectory (must be empty)");
  shellRegister("ls", doLs,
                UNDERLINE "ls [path]\n" COLOR_RESET "List directory contents");
  shellRegister("open", doOpen,
                UNDERLINE "open <path>\n" COLOR_RESET
                          "Open a file, returns fd");
  shellRegister("close", doClose,
                UNDERLINE "close <fd>\n" COLOR_RESET "Close a file by fd");
  shellRegister("read", doRead,
                UNDERLINE "read <path> <size>\n" COLOR_RESET
                          "Read from file and print");
  shellRegister("write", doWrite,
                UNDERLINE "write <path> <data>\n" COLOR_RESET
                          "Write data to file");
  shellRegister("df", doDf,
                UNDERLINE "df\n" COLOR_RESET "Show disk usage statistics");
  shellRegister("viz", doViz,
                UNDERLINE "viz\n" COLOR_RESET "Toggle visualization on/off");

  /* 初始化 Shell */
  if (shellInit("root") != 0) {
    logWrite(ERR, "Shell initialization failed");
    goto err;
  }

  logWrite(INF, "All systems ready. Starting shell...");

  shellLoop();

  /* 清理 */
  fsShutdown();
  vizStop();

  /*磁盘持久化*/
  if (dumpDisk("disk.img") != 0) {
    goto err;
  }

  return 0;

err:
  return -1;
}
