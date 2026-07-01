#include "shell.h"
#include "log.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char currentUser[USERNAME_MAX];
int cmdCount;
ShellCmdEntry cmdBucket[CMDSIZE_MAX];

/* 线程化命令退出信号 */
volatile int shellShouldExit = 0;
volatile int shellExitCode = 0;

/* 前向声明 */
static int doEcho(int argc, char **argv);
static int doHelp(int argc, char **argv);
static int doExit(int argc, char **argv);

/* 计算命令名哈希，使用开放桶存储 */
static unsigned int sdbmHash(const char *str) {
  unsigned int hash = 0;
  unsigned int index = 0;
  unsigned int length = strlen(str);
  for (index = 0; index < length; str++, index++) {
    /* sdbm hash algorithm: h = c + (h << 6) + (h << 16) - h */
    /* NOLINTBEGIN(readability-magic-numbers) */
    hash = (*str) + (hash << 6) + (hash << 16) - hash;
    /* NOLINTEND(readability-magic-numbers) */
  }
  return hash;
}

/* 注册命令 */
int shellRegister(const char *cmdName, cmdHandler *cmdFunc,
                  const char *helpText) {
  if (cmdName == NULL || cmdFunc == NULL) {
    return -EINVAL;
  }

  unsigned int hash = sdbmHash(cmdName);
  unsigned int index = hash % CMDSIZE_MAX;
  unsigned int probes = 0;

  /* 线性探测：找空槽或同名槽（支持重复注册覆盖） */
  while (probes < CMDSIZE_MAX) {
    if (!cmdBucket[index].occupied) {
      break;
    }
    if (cmdBucket[index].hash_cache == hash &&
        strcmp(cmdBucket[index].name, cmdName) == 0) {
      break;
    }
    index = (index + 1) % CMDSIZE_MAX;
    probes++;
  }

  if (probes >= CMDSIZE_MAX) {
    logWrite(ERR, "Command table full, cannot register: %s", cmdName);
    return -ENOMEM;
  }

  cmdBucket[index].occupied = 1;
  cmdBucket[index].fn = cmdFunc;
  cmdBucket[index].name = cmdName;
  cmdBucket[index].help = helpText;
  cmdBucket[index].hash_cache = hash;
  cmdCount++;

  return 0;
}

/* 通过命令名寻找命令，返回索引 */
int shellCommandFound(const char *cmdName) {
  unsigned int hash = sdbmHash(cmdName);
  unsigned int index = hash % CMDSIZE_MAX;
  int probe = 0;

  while (probe < CMDSIZE_MAX) {
    if (cmdBucket[index].occupied == 0) {
      return -ENOENT;
    }
    if (cmdBucket[index].occupied == 1 && cmdBucket[index].hash_cache == hash &&
        strcmp(cmdBucket[index].name, cmdName) == 0) {
      break;
    }
    probe++;
    index = (index + 1) % CMDSIZE_MAX;
  }
  return (int)index;
}

/* 复制 argv 字符串数组 */
static char **copyArgv(int argc, char **argv) {
  char **copy = (char **)malloc(sizeof(char *) * ((unsigned long)argc + 1));
  if (copy == NULL) {
    return NULL;
  }
  for (int i = 0; i < argc; i++) {
    size_t len = strlen(argv[i]) + 1;
    copy[i] = (char *)malloc(len);
    if (copy[i] == NULL) {
      for (int j = 0; j < i; j++) {
        free((void *)copy[j]);
      }
      free((void *)copy);
      return NULL;
    }
    memcpy(copy[i], argv[i], len);
  }
  copy[argc] = NULL;
  return copy;
}

/* 释放 argv 副本 */
static void freeArgv(int argc, char **argv) {
  if (argv == NULL) {
    return;
  }
  for (int i = 0; i < argc; i++) {
    free((void *)argv[i]);
  }
  free((void *)argv);
}

/* 命令线程包装器 */
static void *commandThreadWrapper(void *arg) {
  CommandArgs *args = (CommandArgs *)arg;
  args->handler(args->argc, args->argv);
  freeArgv(args->argc, args->argv);
  free((void *)args);
  return NULL;
}

/* 统一执行命令：每个命令作为独立线程 */
int shellExec(char *executeable, int argc, char **argv) {
  int index = shellCommandFound(executeable);
  if (index == -ENOENT) {
    logWrite(WRN, "Command not found: %s", executeable);
    return -ENOENT;
  }

  /* 构造线程参数，复制 argv 以避免缓冲区被覆盖 */
  CommandArgs *args = (CommandArgs *)malloc(sizeof(CommandArgs));
  if (args == NULL) {
    logWrite(ERR, "malloc failed for CommandArgs");
    return -ENOMEM;
  }
  args->argc = argc;
  args->argv = copyArgv(argc, argv);
  if (args->argv == NULL) {
    free((void *)args);
    logWrite(ERR, "copyArgv failed");
    return -ENOMEM;
  }
  args->handler = cmdBucket[index].fn;

  pthread_t tid;
  /* NOLINTNEXTLINE(readability-identifier-length) */
  int rc = pthread_create(&tid, NULL, commandThreadWrapper, args);
  if (rc != 0) {
    logWrite(ERR, "pthread_create failed: %d", rc);
    freeArgv(argc, args->argv);
    free((void *)args);
    return -rc;
  }
  pthread_join(tid, NULL);

  return 0;
}

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
    logWrite(ERR, "No user logged in");
    return EXIT_FAILURE;
  }

  shellRegister("echo", doEcho, "echo <text> \nEcho what you input");
  shellRegister("help", doHelp, "help <command> \nShow helptext of command");
  shellRegister("exit", doExit,
                "exit <exitcode>\nExit shell with <code(default=0)>");

  return 0;
}

/* shell的主要输入/输出循环 */
int shellLoop(void) {
  char buffer[BUFFER_SIZE];
  while (1) {
    /* 检查退出信号 */
    if (shellShouldExit) {
      int code = shellExitCode;
      logWrite(INF, "Shell exiting with code %d", code);
      return code;
    }

    printf("[%s@localhost]$ ", currentUser);
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
      printf("\n");
      break;
    }
    buffer[strcspn(buffer, "\n")] = '\0';

    char *argv[ARGV_SIZE];
    int index = 0;

    char *token = strtok(buffer, " \t\n\r");
    while (token != NULL && index < ARGV_SIZE - 1) {
      argv[index++] = token;
      token = strtok(NULL, " \t\n\r");
    }
    argv[index] = NULL;
    int argc = index;

    if (argc == 0 || argv[0] == NULL) {
      continue;
    }

    int ret = shellExec(argv[0], argc, argv);
    if (ret < 0) {
      logWrite(WRN, "shellExec failed for '%s': %d", argv[0], ret);
    }

    /* 给线程一点时间启动并获取锁 */
    /* NOLINTNEXTLINE(readability-magic-numbers) */
    sleep(0);
  }
  return 0;
}

/* 回显你的输入 */
static int doEcho(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
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
  if (argc == 1) {
    for (int idx = 0; idx < CMDSIZE_MAX; idx++) {
      if (cmdBucket[idx].occupied == 1) {
        printf("%s\n", cmdBucket[idx].help);
      }
    }
  }
  if (argc == 2) {
    cmdName = argv[1];
  }

  int idx = shellCommandFound(cmdName);
  if (idx == -ENOENT) {
    goto err;
  }

  printf("%s\n", cmdBucket[idx].help);
  return 0;

err:
  if (argc > 2) {
    logWrite(WRN, "help: Too many arguments");
    return EXIT_FAILURE;
  }
  logWrite(WRN, "help: Command not found: %s", argv[1]);
  return -ENOENT;
}

/* 优雅退出shell */
static int doExit(int argc, char **argv) {
  int code = 0;
  char *endptr;
  /* NOLINTNEXTLINE(readability-magic-numbers) */
  const int base = 10;
  if (argc > 2) {
    logWrite(WRN, "exit: too many arguments");
    return EXIT_FAILURE;
  }
  if (argc == 2) {
    code = (int)strtol(argv[1], &endptr, base);
  }
  shellExitCode = code;
  shellShouldExit = 1;
  return 0;
}
