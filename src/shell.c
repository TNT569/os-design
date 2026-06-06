#include "shell.h"
#include "log.h"
#include <asm-generic/errno-base.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 初始化shell，注册命令，传入登录的用户名 */
int shellInit(char *username) {
  cmdCount = 0;
  strcpy(currentUser, "");

  if (strlen(username) >= USERNAME_MAX) {
    logWrite(ERR, "Username is too long");
    return EXIT_FAILURE;
  }
  strcpy(currentUser, username);
  if (strcmp(currentUser, "") == 0) {
    /* 没有用户登录时退出 */
    logWrite(ERR, "No user logged in");
    return EXIT_FAILURE;
  }

  // 注册命令
  shellRegister("echo", doEcho, "echo <text> \nEcho what you input");
  shellRegister("help", doHelp, "help <command> \nShow helptext of command");
  shellRegister("exit", doExit,
                "exit <exitcode>\nExit shell with <code(defalut=0)>");

  return 0;
}

/* shell的主要输入/输出循环 */
int shellLoop() {
  char buffer[BUFFER_SIZE];
  while (1) {
    printf("[%s@localhost]$ ", currentUser);
    // 处理 EOF (Ctrl+D)
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
      printf("\n"); // 换行美化输出
      break;        // 等同于 exit 0
    }
    buffer[strcspn(buffer, "\n")] = '\0';
    // 去除fgets留在末尾的换行

    char *argv[ARGV_SIZE]; // 命令所需参数
    int index = 0;

    // 使用多种空白字符作为分隔符
    char *token = strtok(buffer, " \t\n\r");
    while (token != NULL && index < ARGV_SIZE - 1) {
      argv[index++] = token;
      token = strtok(NULL, " \t\n\r");
    }

    argv[index] = NULL; // 确保 NULL 结尾
    int argc = index;   // 命令的参数数量

    if (argc == 0 || argv[0] == NULL) {
      continue;
    }

    int ret = shellExec(argv[0], argc, argv);

    // 拦截退出请求
    if (ret <= -SHELL_EXIT_REQUESTED) {
      // 还原用户设置的真实退出码
      int realCode = -(ret + SHELL_EXIT_REQUESTED);
      logWrite(INF, "Shell exiting with code %d", realCode);
      // TODO: 在这里执行所有清理工作
      return realCode;
    }
  }
  return 0;
}

/* 计算命令名哈希，使用开放桶存储 */
static unsigned int sdbmHash(const char *str) {
  unsigned int hash = 0;
  unsigned int index = 0;
  unsigned int length = strlen(str);
  for (index = 0; index < length; str++, index++) {
    // sdbm hash algorithm: h = c + (h << 6) + (h << 16) - h
    // The magic numbers 6 and 16 are part of the sdbm multiplier (65599)
    // derived from: (2^6 + 2^16 - 1)
    // NOLINTNEXTLINE(readability-magic-numbers)
    hash = (*str) + (hash << 6) + (hash << 16) - hash;
  }
  return hash;
}

static int shellRegister(const char *cmdName, cmdHandler cmdFunc,
                         const char *helpText) {
  if (cmdName == NULL || cmdFunc == NULL) {
    return -EINVAL;
  }

  unsigned int hash = sdbmHash(cmdName);
  unsigned int index = hash % CMDSIZE_MAX;
  unsigned int probes = 0;

  // 线性探测：找空槽或同名槽（支持重复注册覆盖）
  while (probes < CMDSIZE_MAX) {
    // 找到空槽，直接注册
    if (!cmdBucket[index].occupied) {
      break;
    }
    // 找到同名命令，允许覆盖更新
    if (cmdBucket[index].hash_cache == hash &&
        strcmp(cmdBucket[index].name, cmdName) == 0) {
      break;
    }

    index = (index + 1) % CMDSIZE_MAX;
    probes++;
  }

  // 表满且未找到同名槽，拒绝注册
  if (probes >= CMDSIZE_MAX) {
    logWrite(ERR, "Command table full, cannot register: %s", cmdName);
    return -ENOMEM;
  }

  cmdBucket[index].occupied = 1;
  cmdBucket[index].fn = cmdFunc;
  cmdBucket[index].name = cmdName;
  cmdBucket[index].help = helpText;
  cmdBucket[index].hash_cache = hash;

  return 0;
}

/* 通过命令名寻找命令，返回索引 */
static int shellCommandFound(const char *cmdName) {
  unsigned int hash = sdbmHash(cmdName);
  unsigned int index = hash % CMDSIZE_MAX;
  int probe = 0;

  // 如果冲突则寻找下一个
  while (probe < CMDSIZE_MAX) {
    if (cmdBucket[index].occupied == 0) {
      return -ENOENT;
    }

    if (cmdBucket[index].occupied == 1 && cmdBucket[index].hash_cache == hash &&
        strcmp(cmdBucket[index].name, cmdName) == 0) {
      // 探测正确后退出循环
      break;
    }

    probe++;
    index = (index + 1) % CMDSIZE_MAX;
  }
  return (int)index;
}

/* 统一执行命令 */
static int shellExec(char *executeable, int argc, char **argv) {
  int index = shellCommandFound(executeable);
  // 命中后执行
  if (index == -ENOENT) {
    goto notFound;
  }
  return cmdBucket[index].fn(argc, argv);

notFound:
  logWrite(WRN, "Command not found: %s", executeable);
  return -ENOENT;
}

/* 回显你的输入 */
static int doEcho(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    // 一共argc-1个输入
    printf("%s ", argv[i]);
  }
  printf("\n");
  return 0;
}

/* 显示对应命令的提示文本 */
static int doHelp(int argc, char **argv) {
  char *cmdName = "help";
  if (argc > 2) {
    goto err;
  }
  if (argc == 2) {
    cmdName = argv[1];
  }

  int index = shellCommandFound(cmdName);
  if (index == -ENOENT) {
    goto err;
  }

  printf("%s\n", cmdBucket[index].help);

  return 0;

err:
  if (argc > 2) {
    logWrite(WRN, "help: Too many arguments");
    return EXIT_FAILURE;
  }
  if (index == -ENOENT) {
    logWrite(WRN, "help: Command not found: %s", argv[1]);
    return -ENOENT;
  }
  return EXIT_FAILURE;
}

static int doExit(int argc, char **argv) {
  int code = 0; // 默认正常退出
  char *endptr;
  const int base = 10;
  if (argc > 2) {
    logWrite(WRN, "exit: too many arguments");
    return EXIT_FAILURE;
  }

  if (argc == 2) {
    code = (int)strtol(argv[1], &endptr, base);
  }

  // 使用位运算或特定偏移量区分“普通错误”和“携带退出码的退出请求”
  // 设负数区间专用于控制流：
  return -(code + SHELL_EXIT_REQUESTED); // 例如 exit 0 -> -100, exit 1 -> -101
}
