#include "shell.h"
#include "disk.h"
#include "buffer.h"
#include "fs.h"
#include "viz.h"
#include "log.h"

int main(void)
{
    /* 初始化各子系统 */
    if (initDisk() != 0) {
        logWrite(ERR, "Disk initialization failed");
        goto err;
    }

    initBuffer();

    if (initFS() != 0) {
        logWrite(ERR, "File system initialization failed");
        goto err;
    }

    /* 注册文件系统命令 */
    shellRegister("mkfile", doMkfile,
                  "mkfile <path> <size>\nCreate a file with given size");
    shellRegister("mkdir", doMkdir,
                  "mkdir <path>\nCreate a subdirectory");
    shellRegister("rm", doRm,
                  "rm <path>\nDelete a file");
    shellRegister("rmdir", doRmdir,
                  "rmdir <path>\nDelete a subdirectory (must be empty)");
    shellRegister("ls", doLs,
                  "ls [path]\nList directory contents");
    shellRegister("open", doOpen,
                  "open <path>\nOpen a file, returns fd");
    shellRegister("close", doClose,
                  "close <fd>\nClose a file by fd");
    shellRegister("read", doRead,
                  "read <path> <size>\nRead from file and print");
    shellRegister("write", doWrite,
                  "write <path> <data>\nWrite data to file");
    shellRegister("df", doDf,
                  "df\nShow disk usage statistics");
    shellRegister("viz", doViz,
                  "viz\nToggle visualization on/off");

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

    return 0;

err:
    return -1;
}