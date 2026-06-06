#ifndef SHELL_H
#define SHELL_H
#define BUFFER_SIZE 1024
#define ARGV_SIZE 64
#define USERNAME_MAX 1024
extern char currentUser[USERNAME_MAX];
int shellInit();
int shellLoop();
int shellExec(char *excuteable, int argc, char **argv);
#endif // !SHELL_H
