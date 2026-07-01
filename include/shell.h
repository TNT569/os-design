#ifndef SHELL_H
#define SHELL_H
#define BUFFER_SIZE 1024
#define ARGV_SIZE 64
#define USERNAME_MAX 1024
#define CMDSIZE_MAX 128
#define SHELL_EXIT_REQUESTED 100 // 内部约定：请求退出Shell
typedef int(cmdHandler)(int argc, char **argv);

typedef struct {
  const char *name;
  const char *help;
  cmdHandler *fn;          // 函数指针
  unsigned int hash_cache; // ⭐ 缓存哈希值，避免每次查找都重新计算字符串哈希
  int occupied;            // 开放寻址必须有的“已占用”标记
} ShellCmdEntry;

extern char currentUser[USERNAME_MAX];
extern int cmdCount;
extern ShellCmdEntry cmdBucket[CMDSIZE_MAX];

int shellInit(char *username);
int shellLoop();

static int shellCommandFound(const char *cmdName);
static int shellRegister(const char *cmdName, cmdHandler *cmdFunc,
                         const char *helpText);
static int shellExec(char *executeable, int argc, char **argv);
static int doEcho(int argc, char **argv);
static int doHelp(int argc, char **argv);
static int doExit(int argc, char **argv);
#endif // !SHELL_H
