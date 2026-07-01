#include "viz.h"
#include "disk.h"
#include "buffer.h"
#include "log.h"
#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

volatile int vizEnabled = 0;
static pthread_t vizThread;
static volatile int vizRunning = 0;

/* 清屏 */
static void clearScreen(void)
{
    printf("\033[2J\033[H");
}

/* 绘制磁盘占用图 */
static void drawDiskUsage(void)
{
    printf(BOLD COLOR_MAGENTA "=== Disk Usage Map ===" COLOR_RESET "\n");
    printf("Blocks: 0..%d (each char = 4 blocks)\n", BLOCK_NUM - 1);
    printf("Legend: [" COLOR_GREEN "#" COLOR_RESET "]=Used ["
           COLOR_YELLOW "." COLOR_RESET "]=Free\n");

    /* 读取超级块获取空闲链 */
    SuperBlock sb;
    memcpy(&sb, &disk[0], sizeof(SuperBlock));

    /* 构建占用位图 */
    int used[BLOCK_NUM];
    for (int i = 0; i < BLOCK_NUM; i++) {
        used[i] = 0;
    }

    /* 标记空闲块 */
    FreeBlock fb;
    int curr = sb.freeChainHead;
    while (curr != -1 && curr < BLOCK_NUM) {
        used[curr] = 0; /* 空闲 */
        memcpy(&fb, &disk[curr * BLOCK_SIZE], sizeof(FreeBlock));
        curr = fb.next;
    }

    /* 所有不在空闲链中的标记为已用 */
    for (int i = 0; i < BLOCK_NUM; i++) {
        if (used[i] == 0) {
            /* 可能空闲链不包含它，检查 */
            /* 简单方法: 默认标记所有块为已用，空闲链中的标记为空闲 */
        }
    }

    /* 重建位图：先全标记为已用，再遍历空闲链标记为空闲 */
    for (int i = 0; i < BLOCK_NUM; i++) {
        used[i] = 1;
    }
    curr = sb.freeChainHead;
    while (curr != -1 && curr < BLOCK_NUM) {
        used[curr] = 0;
        memcpy(&fb, &disk[curr * BLOCK_SIZE], sizeof(FreeBlock));
        curr = fb.next;
    }

    /* 打印占用图，每行64个符号（=256块） */
    int charsPerLine = 64;
    int blocksPerChar = 4;
    for (int line = 0; line < BLOCK_NUM / (charsPerLine * blocksPerChar); line++) {
        for (int c = 0; c < charsPerLine; c++) {
            int startBlock = line * charsPerLine * blocksPerChar +
                             c * blocksPerChar;
            int usedCount = 0;
            for (int b = 0; b < blocksPerChar; b++) {
                int blk = startBlock + b;
                if (blk < BLOCK_NUM && used[blk]) {
                    usedCount++;
                }
            }
            if (usedCount == blocksPerChar) {
                printf(COLOR_GREEN "#" COLOR_RESET);
            } else if (usedCount == 0) {
                printf(COLOR_YELLOW "." COLOR_RESET);
            } else {
                printf(COLOR_CYAN "*" COLOR_RESET);
            }
        }
        printf("\n");
    }

    printf("Free: %d / %d blocks\n\n", sb.freeBlockCount, BLOCK_NUM);
}

/* 绘制目录树 */
static void drawDirTree(void)
{
    printf(BOLD COLOR_BLUE "=== Directory Tree ===" COLOR_RESET "\n");
    printf("%-28s %-6s %-8s %-6s\n", "Name", "Type", "Size", "Blocks");
    printf("----------------------------------------------\n");

    DirEntry entry;
    for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        /* 读取根目录项 */
        int blockNo = i * DIR_ENTRY_SIZE / BLOCK_SIZE;
        int offset = (i * DIR_ENTRY_SIZE) % BLOCK_SIZE;
        int absBlock = 1 + blockNo;
        if (absBlock >= 1 + ROOT_DIR_BLOCKS) {
            break;
        }
        char blockBuf[BLOCK_SIZE];
        if (readDiskBlock(absBlock, blockBuf) != 0) {
            continue;
        }
        memcpy(&entry, &blockBuf[offset], sizeof(DirEntry));
        if (entry.name[0] == '\0') {
            continue;
        }

        const char *typeStr = (entry.type == TYPE_DIR) ? "DIR" : "FILE";
        printf("%-28s %-6s %-8d %-6d\n",
               entry.name, typeStr, entry.size, entry.blockCount);

        /* 如果是目录，列出子内容 */
        if (entry.type == TYPE_DIR && entry.subDirBlocks > 0) {
            int maxEntries = (entry.subDirBlocks * BLOCK_SIZE) /
                             DIR_ENTRY_SIZE;
            DirEntry subEntry;
            for (int j = 0; j < maxEntries; j++) {
                char subBlock[BLOCK_SIZE];
                int subBlockNo = entry.subDirStart +
                                 (j * DIR_ENTRY_SIZE) / BLOCK_SIZE;
                int subOffset = (j * DIR_ENTRY_SIZE) % BLOCK_SIZE;
                if (readDiskBlock(subBlockNo, subBlock) != 0) {
                    continue;
                }
                memcpy(&subEntry, &subBlock[subOffset],
                       sizeof(DirEntry));
                if (subEntry.name[0] == '\0') {
                    continue;
                }
                const char *subType = (subEntry.type == TYPE_DIR) ?
                                      "DIR" : "FILE";
                printf("  %-26s %-6s %-8d %-6d\n",
                       subEntry.name, subType, subEntry.size,
                       subEntry.blockCount);
            }
        }
    }
    printf("\n");
}

/* 绘制缓冲区状态 */
static void drawBufferState(void)
{
    printf(BOLD COLOR_CYAN "=== Buffer State (FIFO) ===" COLOR_RESET "\n");
    printf("Slot  Block  Dirty  Data(hex)\n");
    printf("----  -----  -----  --------\n");
    for (int i = 0; i < BUFFER_PAGES; i++) {
        printf(" %2d   ", i);
        if (bufferPool[i].blockNo >= 0) {
            printf(" %3d   ", bufferPool[i].blockNo);
        } else {
            printf(" free   ");
        }
        printf("  %s   ", bufferPool[i].dirty ? COLOR_RED "Y" COLOR_RESET
                                            : "N");
        /* 显示前8字节 */
        for (int j = 0; j < 8 && j < BLOCK_SIZE; j++) {
            printf("%02x ", (unsigned char)bufferPool[i].data[j]);
        }
        printf("\n");
    }
    printf("\n");
}

/* 可视化刷新线程 */
static void *vizThreadFunc(void *arg)
{
    (void)arg;
    int refreshCount = 0;

    while (vizRunning) {
        clearScreen();

        printf(BOLD COLOR_YELLOW "╔══════════════════════════════════╗\n"
               "║   OS File System Visualizer    ║\n"
               "║   Refresh #%-4d               ║\n"
               "╚══════════════════════════════════╝"
               COLOR_RESET "\n\n", refreshCount++);

        pthread_mutex_lock(&globalLock);
        drawDirTree();
        drawDiskUsage();
        drawBufferState();
        pthread_mutex_unlock(&globalLock);

        printf("Type 'viz' to toggle visualization off.\n");

        sleep(2);
    }

    return NULL;
}

/* 启动可视化线程 */
int vizStart(void)
{
    if (vizRunning) {
        logWrite(WRN, "vizStart: already running");
        return -1;
    }

    vizRunning = 1;
    vizEnabled = 1;

    int rc = pthread_create(&vizThread, NULL, vizThreadFunc, NULL);
    if (rc != 0) {
        logWrite(ERR, "vizStart: pthread_create failed: %d", rc);
        vizRunning = 0;
        vizEnabled = 0;
        return -1;
    }

    logWrite(INF, "Visualization started");
    return 0;
}

/* 停止可视化线程 */
void vizStop(void)
{
    if (!vizRunning) {
        return;
    }
    vizRunning = 0;
    vizEnabled = 0;
    pthread_join(vizThread, NULL);
    clearScreen();
    logWrite(INF, "Visualization stopped");
}

/* viz 命令：切换可视化 */
int doViz(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (vizEnabled) {
        vizStop();
        printf("Visualization turned off.\n");
    } else {
        vizStart();
        printf("Visualization turned on.\n");
    }
    return 0;
}