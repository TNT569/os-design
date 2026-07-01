#include "common.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Dump 整个磁盘到文件 */
int dumpDisk(const char *filename) {
  FILE *fp = fopen(filename, "wb");
  if (fp == NULL) {
    perror("fopen failed");
    return -1;
  }

  // 直接写入整个磁盘数组
  size_t diskSize = BLOCK_SIZE * BLOCK_NUM;
  size_t written = fwrite(disk, 1, diskSize, fp);

  fclose(fp);

  if (written != diskSize) {
    logWrite(ERR, "Only wrote %zu of %zu bytes", written, diskSize);
    return -1;
  }

  logWrite(INF, "Dumped entire disk (%zu bytes) to %s", diskSize, filename);
  return 0;
}

/* 从文件恢复整个磁盘 */
int restoreDisk(const char *filename) {
  FILE *fp = fopen(filename, "rb");
  if (fp == NULL) {
    perror("fopen failed");
    return -1;
  }

  size_t diskSize = BLOCK_SIZE * BLOCK_NUM;
  size_t read = fread(disk, 1, diskSize, fp);

  fclose(fp);

  if (read != diskSize) {
    logWrite(ERR, "Only read %zu of %zu bytes", read, diskSize);
    return -1;
  }

  logWrite(INF, "Disk image %s found. Skip initlization", filename);
  logWrite(INF, "Restored entire disk (%zu bytes) from %s", diskSize, filename);
  return 0;
}
