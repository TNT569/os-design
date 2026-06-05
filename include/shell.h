#ifndef SHELL_H
#define SHELL_H
#define BUFFER_SIZE 1024
#define ARGV_SIZE 64
#define PATH_MAX 1024
extern char currentDir[PATH_MAX];
void shellInit();
int shellLoop();
int shellExec(char *excuteable, int argc, char **argv);
#endif // !SHELL_H
