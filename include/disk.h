#ifndef DISK_H
#define DISK_H

#include "common.h"

/* 初始化磁盘：构建超级块和空闲盘区链 */
int initDisk(void);

/* 将指定块号的数据读入 buf（不经过缓冲，用于元数据） */
int readDiskBlock(int blockNo, char *buf);

/* 将 buf 写入指定块号（不经过缓冲，用于元数据） */
int writeDiskBlock(int blockNo, const char *buf);

/* 分配 n 个连续块，返回起始块号，失败返回 -1 */
int allocBlocks(int n);

/* 回收从 start 开始的 n 个连续块 */
void freeBlocks(int start, int n);

/* 获取空闲块总数 */
int getFreeBlockCount(void);

#endif /* DISK_H */