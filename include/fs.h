#ifndef FS_H
#define FS_H

#include "common.h"

/* 初始化文件系统 */
int initFS(void);

/* 创建文件：分配连续块，插入目录项 */
int createFile(const char *path, int size);

/* 创建子目录 */
int createDir(const char *path);

/* 删除文件：回收块，检查是否被打开 */
int deleteFile(const char *path);

/* 删除子目录（目录必须为空） */
int deleteDir(const char *path);

/* 显示目录内容 */
int listDir(const char *path);

/* 根据路径查找目录项（返回指针指向磁盘数组内，需持有锁） */
DirEntry *findEntry(const char *path);

/* 打开文件，返回文件描述符 */
int openFile(const char *path);

/* 关闭文件 */
int closeFile(int fileDesc);

/* 从文件读取数据（通过缓冲页） */
int readFile(int fileDesc, char *buf, int size);

/* 向文件写入数据（通过缓冲页） */
int writeFile(int fileDesc, const char *buf, int size);

/* 文件系统命令处理函数 */
int doMkfile(int argc, char **argv);
int doMkdir(int argc, char **argv);
int doRm(int argc, char **argv);
int doRmdir(int argc, char **argv);
int doLs(int argc, char **argv);
int doRead(int argc, char **argv);
int doWrite(int argc, char **argv);
int doDf(int argc, char **argv);
int doOpen(int argc, char **argv);
int doClose(int argc, char **argv);

/* 文件系统关闭（刷新缓冲） */
void fsShutdown(void);

#endif /* FS_H */