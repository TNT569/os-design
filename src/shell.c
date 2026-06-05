#include "shell.h"
#include <stdio.h>
#include <string.h>
char currentDir[PATH_MAX];

void shellInit() { strcpy(currentDir, "/"); }

int shellLoop() {
  char buffer[BUFFER_SIZE];
  printf("sh$");
  if (fgets(buffer, sizeof(buffer), stdin)) {
    buffer[strcspn(buffer, "\n")] = '\0';
    // 去除fgets留在末尾的换行
  }

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
  return shellExec(argv[0], argc, argv);
}

int shellExec(char *excuteable, int argc, char **argv) { return 0; }
