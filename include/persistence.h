#ifndef PERSISTENCE_H
#define PERSISTENCE_H
/* Dump 整个磁盘到文件 */
int dumpDisk(const char *filename);

/* 从文件恢复整个磁盘 */
int restoreDisk(const char *filename);

#endif // !PERSISTENCE_
