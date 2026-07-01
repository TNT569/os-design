#ifndef COMMON_H
#define COMMON_H

#include <pthread.h>

/* ===================== 颜色宏 ===================== */

/* 前景色 */
#define COLOR_BLACK "\033[30m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN "\033[36m"
#define COLOR_WHITE "\033[37m"

/* 背景色 */
#define BG_BLACK "\033[40m"
#define BG_RED "\033[41m"
#define BG_GREEN "\033[42m"
#define BG_YELLOW "\033[43m"
#define BG_BLUE "\033[44m"
#define BG_MAGENTA "\033[45m"
#define BG_CYAN "\033[46m"
#define BG_WHITE "\033[47m"

/* 样式 */
#define BOLD "\033[1m"
#define UNDERLINE "\033[4m"
#define BLINK "\033[5m"
#define REVERSE "\033[7m"

#define COLOR_RESET "\033[0m"

/* ===================== 文件系统常量 ===================== */

#define BLOCK_SIZE 64
#define BLOCK_NUM 1024
#define BUFFER_PAGES 8
#define MAX_NAME 28
#define DIR_ENTRY_SIZE 64
#define ROOT_DIR_BLOCKS 8
#define MAX_DIR_ENTRIES \
    ((ROOT_DIR_BLOCKS * BLOCK_SIZE) / DIR_ENTRY_SIZE) /* 8 */
#define MAX_SUBDIR_ENTRIES 16
#define MAX_OPEN_FILES 16
#define MAX_PATH 256

/* 填充大小常量 */
#define SB_PAD_SIZE \
    (BLOCK_SIZE - (4 * (int)sizeof(int))) /* 64 - 16 = 48 */
#define FB_PAD_SIZE \
    (BLOCK_SIZE - (int)sizeof(int))       /* 64 - 4 = 60 */
#define DE_PAD_SIZE 4

/* ===================== 文件类型 ===================== */

#define TYPE_FILE 0
#define TYPE_DIR 1

/* ===================== 数据结构 ===================== */

/* 超级块：磁盘块 0 */
typedef struct {
    int freeBlockCount;  /* 空闲块总数 */
    int freeChainHead;   /* 空闲盘区链头 */
    int rootDirStart;    /* 根目录起始块 */
    int rootDirBlocks;   /* 根目录占用块数 */
    char padding[SB_PAD_SIZE];    /* 填充至 64 字节 */
} SuperBlock;

/* 空闲块链节点（存储在空闲块开头） */
typedef struct {
    int next;            /* 下一空闲块号，-1 表示链尾 */
    char padding[FB_PAD_SIZE];    /* 填充至 64 字节 */
} FreeBlock;

/* 目录项 */
typedef struct {
    char name[MAX_NAME];     /* 文件名或目录名 */
    int type;                /* TYPE_FILE 或 TYPE_DIR */
    int size;                /* 文件大小（字节） */
    int startBlock;          /* 起始块号 */
    int blockCount;          /* 连续块数 */
    int subDirStart;         /* 子目录项起始块（仅目录有效） */
    int subDirBlocks;        /* 子目录项占用块数（仅目录有效） */
    int openCount;           /* 当前打开计数 */
    char padding[DE_PAD_SIZE];         /* 对齐至 64 字节 */
} DirEntry;

/* 缓冲页 */
typedef struct {
    int blockNo;             /* 缓存的块号，-1 表示空槽 */
    char data[BLOCK_SIZE];   /* 块数据 */
    int dirty;               /* 脏位 */
} BufferPage;

/* 打开文件表项 */
typedef struct {
    int used;                /* 是否正在使用 */
    char path[MAX_PATH];     /* 文件路径 */
    int offset;              /* 当前读写偏移 */
    DirEntry *entry;         /* 指向目录项指针 (需加锁访问) */
} OpenFileEntry;

/* ===================== 全局变量声明 ===================== */

extern char disk[BLOCK_SIZE * BLOCK_NUM];
extern pthread_mutex_t globalLock;
extern BufferPage bufferPool[BUFFER_PAGES];
extern OpenFileEntry openFileTable[MAX_OPEN_FILES];
extern volatile int vizEnabled;

/* shell 退出信号（在 shell.c 中定义） */
extern volatile int shellShouldExit;
extern volatile int shellExitCode;

#endif /* COMMON_H */