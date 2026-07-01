#include "disk.h"
#include "log.h"
#include <stdio.h>
#include <string.h>

/* 全局磁盘数组和锁（定义在此处） */
char disk[BLOCK_SIZE * BLOCK_NUM];
pthread_mutex_t globalLock = PTHREAD_MUTEX_INITIALIZER;

/* 初始化磁盘：构建超级块和空闲盘区链 */
int initDisk(void) {
    pthread_mutex_lock(&globalLock);

    /* 清零磁盘 */
    memset(disk, 0, sizeof(disk));

    /* 构建空闲盘区链 */
    /* 块 0: 超级块, 块 1..ROOT_DIR_BLOCKS: 根目录 */
    int reservedBlocks = 1 + ROOT_DIR_BLOCKS; /* 超级块 + 根目录 */
    int freeCount = BLOCK_NUM - reservedBlocks;

    /* 将所有空闲块串成链 */
    for (int i = reservedBlocks; i < BLOCK_NUM; i++) {
        FreeBlock fb;
        fb.next = (i + 1 < BLOCK_NUM) ? (i + 1) : -1;
        memset(fb.padding, 0, sizeof(fb.padding));
        memcpy(&disk[i * BLOCK_SIZE], &fb, sizeof(FreeBlock));
    }

    /* 写入超级块 */
    SuperBlock sb;
    sb.freeBlockCount = freeCount;
    sb.freeChainHead = reservedBlocks;
    sb.rootDirStart = 1;
    sb.rootDirBlocks = ROOT_DIR_BLOCKS;
    memset(sb.padding, 0, sizeof(sb.padding));
    memcpy(&disk[0], &sb, sizeof(SuperBlock));

    pthread_mutex_unlock(&globalLock);

    logWrite(INF, "Disk initialized: %d total blocks, %d free blocks",
             BLOCK_NUM, freeCount);
    return 0;
}

/* 将指定块号的数据读入 buf */
int readDiskBlock(int blockNo, char *buf) {
    if (blockNo < 0 || blockNo >= BLOCK_NUM) {
        logWrite(ERR, "readDiskBlock: invalid blockNo %d", blockNo);
        return -1;
    }
    if (buf == NULL) {
        logWrite(ERR, "readDiskBlock: NULL buffer");
        return -1;
    }
    memcpy(buf, &disk[blockNo * BLOCK_SIZE], BLOCK_SIZE);
    return 0;
}

/* 将 buf 写入指定块号 */
int writeDiskBlock(int blockNo, const char *buf) {
    if (blockNo < 0 || blockNo >= BLOCK_NUM) {
        logWrite(ERR, "writeDiskBlock: invalid blockNo %d", blockNo);
        return -1;
    }
    if (buf == NULL) {
        logWrite(ERR, "writeDiskBlock: NULL buffer");
        return -1;
    }
    memcpy(&disk[blockNo * BLOCK_SIZE], buf, BLOCK_SIZE);
    return 0;
}

/* 分配 n 个连续块，返回起始块号，失败返回 -1
 * 使用首次适应法扫描空闲盘区链 */
int allocBlocks(int n) {
    if (n <= 0) {
        logWrite(ERR, "allocBlocks: invalid count %d", n);
        return -1;
    }

    /* 读取超级块 */
    SuperBlock sb;
    memcpy(&sb, &disk[0], sizeof(SuperBlock));

    if (sb.freeBlockCount < n) {
        logWrite(WRN, "allocBlocks: not enough free blocks (need %d, have %d)",
                 n, sb.freeBlockCount);
        return -1;
    }

    /* 扫描空闲链找 n 个连续块 */
    int prev = -1;
    int curr = sb.freeChainHead;
    int consecutive = 0;
    int start = -1;
    int prevConsecutive = -1;

    while (curr != -1) {
        if (start == -1) {
            /* 开始新的连续段 */
            start = curr;
            prevConsecutive = prev;
            consecutive = 1;
        } else if (curr == start + consecutive) {
            /* 连续 */
            consecutive++;
        } else {
            /* 不连续，重新开始 */
            start = curr;
            prevConsecutive = prev;
            consecutive = 1;
        }

        if (consecutive == n) {
            /* 找到 n 个连续块: start .. start + n - 1 */
            /* 从空闲链中移除这 n 个块 */
            int chainPrev = prevConsecutive;
            /* 找到这 n 个块之后的链节点 */
            FreeBlock lastFb;
            memcpy(&lastFb, &disk[(start + n - 1) * BLOCK_SIZE],
                   sizeof(FreeBlock));
            int nextChain = lastFb.next;

            if (chainPrev == -1) {
                /* 移除的是链头 */
                sb.freeChainHead = nextChain;
            } else {
                /* 修改前驱的 next 指针 */
                FreeBlock prevFb;
                memcpy(&prevFb, &disk[chainPrev * BLOCK_SIZE],
                       sizeof(FreeBlock));
                prevFb.next = nextChain;
                memcpy(&disk[chainPrev * BLOCK_SIZE], &prevFb,
                       sizeof(FreeBlock));
            }

            sb.freeBlockCount -= n;
            memcpy(&disk[0], &sb, sizeof(SuperBlock));

            /* 清零分配的块 */
            for (int i = start; i < start + n; i++) {
                memset(&disk[i * BLOCK_SIZE], 0, BLOCK_SIZE);
            }

            logWrite(INF, "allocBlocks: allocated %d blocks at %d", n, start);
            return start;
        }

        prev = curr;
        FreeBlock fb;
        memcpy(&fb, &disk[curr * BLOCK_SIZE], sizeof(FreeBlock));
        curr = fb.next;
    }

    logWrite(WRN, "allocBlocks: no %d consecutive blocks available", n);
    return -1;
}

/* 回收从 start 开始的 n 个连续块 */
void freeBlocks(int start, int n) {
    if (n <= 0 || start < 0) {
        logWrite(ERR, "freeBlocks: invalid params start=%d n=%d", start, n);
        return;
    }

    SuperBlock sb;
    memcpy(&sb, &disk[0], sizeof(SuperBlock));

    /* 将这 n 个块重新插入空闲链头 */
    for (int i = start; i < start + n; i++) {
        FreeBlock fb;
        memset(fb.padding, 0, sizeof(fb.padding));
        if (i < start + n - 1) {
            fb.next = i + 1;
        } else {
            /* 最后一块指向原链头 */
            fb.next = sb.freeChainHead;
        }
        memcpy(&disk[i * BLOCK_SIZE], &fb, sizeof(FreeBlock));
    }

    sb.freeChainHead = start;
    sb.freeBlockCount += n;
    memcpy(&disk[0], &sb, sizeof(SuperBlock));

    logWrite(INF, "freeBlocks: freed %d blocks from %d", n, start);
}

/* 获取空闲块总数 */
int getFreeBlockCount(void) {
    SuperBlock sb;
    memcpy(&sb, &disk[0], sizeof(SuperBlock));
    return sb.freeBlockCount;
}