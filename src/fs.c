#include "fs.h"
#include "disk.h"
#include "buffer.h"
#include "log.h"
#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* 全局打开文件表 */
OpenFileEntry openFileTable[MAX_OPEN_FILES];

/* ========== 内部辅助函数（调用者须持有 globalLock） ========== */

/* 从磁盘读取根目录某个槽位的目录项 */
static int readRootEntry(int slotIndex, DirEntry *entry)
{
    int blockNo = slotIndex * DIR_ENTRY_SIZE / BLOCK_SIZE;
    int offset = (slotIndex * DIR_ENTRY_SIZE) % BLOCK_SIZE;
    int absBlock = 1 + blockNo; /* 根目录从块1开始 */
    if (absBlock >= 1 + ROOT_DIR_BLOCKS) {
        return -1;
    }
    char blockBuf[BLOCK_SIZE];
    if (readDiskBlock(absBlock, blockBuf) != 0) {
        return -1;
    }
    memcpy(entry, &blockBuf[offset], sizeof(DirEntry));
    return 0;
}

/* 将目录项写入根目录某个槽位 */
static int writeRootEntry(int slotIndex, const DirEntry *entry)
{
    int blockNo = slotIndex * DIR_ENTRY_SIZE / BLOCK_SIZE;
    int offset = (slotIndex * DIR_ENTRY_SIZE) % BLOCK_SIZE;
    int absBlock = 1 + blockNo;
    if (absBlock >= 1 + ROOT_DIR_BLOCKS) {
        return -1;
    }
    char blockBuf[BLOCK_SIZE];
    if (readDiskBlock(absBlock, blockBuf) != 0) {
        return -1;
    }
    memcpy(&blockBuf[offset], entry, sizeof(DirEntry));
    if (writeDiskBlock(absBlock, blockBuf) != 0) {
        return -1;
    }
    return 0;
}

/* 从磁盘上的子目录块中读取目录项 */
static int readSubDirEntry(int startBlock, int subBlocks, int slotIndex,
                           DirEntry *entry)
{
    int maxEntries = (subBlocks * BLOCK_SIZE) / DIR_ENTRY_SIZE;
    if (slotIndex < 0 || slotIndex >= maxEntries) {
        return -1;
    }
    int blockNo = startBlock + (slotIndex * DIR_ENTRY_SIZE) / BLOCK_SIZE;
    int offset = (slotIndex * DIR_ENTRY_SIZE) % BLOCK_SIZE;
    char blockBuf[BLOCK_SIZE];
    if (readDiskBlock(blockNo, blockBuf) != 0) {
        return -1;
    }
    memcpy(entry, &blockBuf[offset], sizeof(DirEntry));
    return 0;
}

/* 将目录项写入子目录块 */
static int writeSubDirEntry(int startBlock, int subBlocks, int slotIndex,
                            const DirEntry *entry)
{
    int maxEntries = (subBlocks * BLOCK_SIZE) / DIR_ENTRY_SIZE;
    if (slotIndex < 0 || slotIndex >= maxEntries) {
        return -1;
    }
    int blockNo = startBlock + (slotIndex * DIR_ENTRY_SIZE) / BLOCK_SIZE;
    int offset = (slotIndex * DIR_ENTRY_SIZE) % BLOCK_SIZE;
    char blockBuf[BLOCK_SIZE];
    if (readDiskBlock(blockNo, blockBuf) != 0) {
        return -1;
    }
    memcpy(&blockBuf[offset], entry, sizeof(DirEntry));
    if (writeDiskBlock(blockNo, blockBuf) != 0) {
        return -1;
    }
    return 0;
}

/* 在根目录中找空闲槽位，返回索引，-1 表示满 */
static int findFreeRootSlot(void)
{
    DirEntry entry;
    for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        if (readRootEntry(i, &entry) != 0) {
            return -1;
        }
        if (entry.name[0] == '\0') {
            return i;
        }
    }
    return -1;
}

/* 在子目录中找空闲槽位，返回索引，-1 表示满 */
static int findFreeSubDirSlot(int startBlock, int subBlocks)
{
    DirEntry entry;
    int maxEntries = (subBlocks * BLOCK_SIZE) / DIR_ENTRY_SIZE;
    for (int i = 0; i < maxEntries; i++) {
        if (readSubDirEntry(startBlock, subBlocks, i, &entry) != 0) {
            return -1;
        }
        if (entry.name[0] == '\0') {
            return i;
        }
    }
    return -1;
}

/* 获取父目录入口（调用者释放锁后指针可能失效，仅内部使用） */
static DirEntry *getParentDir(const char *name1)
{
    if (name1 == NULL) {
        return NULL;
    }
    /* 在根目录中查找 */
    DirEntry entry;
    for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        if (readRootEntry(i, &entry) != 0) {
            return NULL;
        }
        if (entry.name[0] != '\0' && strcmp(entry.name, name1) == 0 &&
            entry.type == TYPE_DIR) {
            /* 返回指向磁盘数据的指针（需确保调用者持有锁） */
            /* 用 static 变量暂存 */
            static DirEntry cachedEntry;
            cachedEntry = entry;
            return &cachedEntry;
        }
    }
    return NULL;
}

/* 根据路径查找目录项（需持有锁，返回指向 static 数据的指针） */
DirEntry *findEntry(const char *path)
{
    if (path == NULL || path[0] != '/') {
        return NULL;
    }

    static DirEntry result;
    char pathCopy[MAX_PATH];
    strncpy(pathCopy, path, MAX_PATH - 1);
    pathCopy[MAX_PATH - 1] = '\0';

    /* 解析路径：跳过首个 '/' */
    char *saveptr;
    char *part1 = strtok_r(pathCopy + 1, "/", &saveptr);
    char *part2 = strtok_r(NULL, "/", &saveptr);
    char *part3 = strtok_r(NULL, "/", &saveptr);

    if (part1 == NULL) {
        /* 路径为 "/" 即根目录 */
        return NULL;
    }

    if (part3 != NULL) {
        /* 超过两级 */
        return NULL;
    }

    if (part2 == NULL) {
        /* 一级路径 /name：在根目录中查找 */
        DirEntry entry;
        for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
            if (readRootEntry(i, &entry) != 0) {
                return NULL;
            }
            if (entry.name[0] != '\0' &&
                strcmp(entry.name, part1) == 0) {
                result = entry;
                return &result;
            }
        }
        return NULL;
    }

    /* 二级路径 /dir/file：先在根找 dir，再在 dir 中找 file */
    DirEntry *parent = getParentDir(part1);
    if (parent == NULL || parent->type != TYPE_DIR) {
        return NULL;
    }
    DirEntry entry;
    int maxEntries = (parent->subDirBlocks * BLOCK_SIZE) / DIR_ENTRY_SIZE;
    for (int i = 0; i < maxEntries; i++) {
        if (readSubDirEntry(parent->subDirStart, parent->subDirBlocks,
                            i, &entry) != 0) {
            return NULL;
        }
        if (entry.name[0] != '\0' &&
            strcmp(entry.name, part2) == 0) {
            result = entry;
            return &result;
        }
    }
    return NULL;
}

/* ========== 初始化 ========== */

/* 初始化文件系统 */
int initFS(void)
{
    pthread_mutex_lock(&globalLock);

    /* 清零打开文件表 */
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        openFileTable[i].used = 0;
        openFileTable[i].path[0] = '\0';
        openFileTable[i].offset = 0;
        openFileTable[i].entry = NULL;
    }

    /* 清零根目录区域 */
    DirEntry emptyEntry;
    memset(&emptyEntry, 0, sizeof(DirEntry));
    for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        writeRootEntry(i, &emptyEntry);
    }

    pthread_mutex_unlock(&globalLock);

    logWrite(INF, "File system initialized: root dir %d blocks, max %d entries",
             ROOT_DIR_BLOCKS, MAX_DIR_ENTRIES);
    return 0;
}

/* ========== 文件操作 ========== */

/* 创建文件 */
int createFile(const char *path, int size)
{
    if (path == NULL || size <= 0) {
        logWrite(ERR, "createFile: invalid parameters");
        return -1;
    }

    pthread_mutex_lock(&globalLock);

    /* 检查路径 */
    if (path[0] != '/') {
        logWrite(ERR, "createFile: path must start with '/'");
        pthread_mutex_unlock(&globalLock);
        return -1;
    }

    /* 检查是否已存在 */
    if (findEntry(path) != NULL) {
        logWrite(ERR, "createFile: '%s' already exists", path);
        pthread_mutex_unlock(&globalLock);
        return -1;
    }

    /* 计算所需块数 */
    int blocksNeeded = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (blocksNeeded == 0) {
        blocksNeeded = 1;
    }

    /* 分配连续块 */
    int startBlock = allocBlocks(blocksNeeded);
    if (startBlock < 0) {
        logWrite(ERR, "createFile: failed to allocate %d blocks", blocksNeeded);
        pthread_mutex_unlock(&globalLock);
        return -1;
    }

    /* 解析路径 */
    char pathCopy[MAX_PATH];
    strncpy(pathCopy, path, MAX_PATH - 1);
    pathCopy[MAX_PATH - 1] = '\0';
    char *saveptr;
    char *part1 = strtok_r(pathCopy + 1, "/", &saveptr);
    char *part2 = strtok_r(NULL, "/", &saveptr);

    if (part1 == NULL) {
        freeBlocks(startBlock, blocksNeeded);
        pthread_mutex_unlock(&globalLock);
        return -1;
    }

    DirEntry newEntry;
    memset(&newEntry, 0, sizeof(DirEntry));
    strncpy(newEntry.name,
            (part2 != NULL) ? part2 : part1, MAX_NAME - 1);
    newEntry.name[MAX_NAME - 1] = '\0';
    newEntry.type = TYPE_FILE;
    newEntry.size = size;
    newEntry.startBlock = startBlock;
    newEntry.blockCount = blocksNeeded;
    newEntry.subDirStart = 0;
    newEntry.subDirBlocks = 0;
    newEntry.openCount = 0;

    int ret = -1;
    if (part2 == NULL) {
        /* 一级路径：插入根目录 */
        int slot = findFreeRootSlot();
        if (slot < 0) {
            logWrite(ERR, "createFile: root directory full");
            freeBlocks(startBlock, blocksNeeded);
            pthread_mutex_unlock(&globalLock);
            return -1;
        }
        ret = writeRootEntry(slot, &newEntry);
    } else {
        /* 二级路径：插入子目录 */
        DirEntry *parent = getParentDir(part1);
        if (parent == NULL) {
            logWrite(ERR, "createFile: parent dir '%s' not found", part1);
            freeBlocks(startBlock, blocksNeeded);
            pthread_mutex_unlock(&globalLock);
            return -1;
        }
        int slot = findFreeSubDirSlot(parent->subDirStart,
                                       parent->subDirBlocks);
        if (slot < 0) {
            logWrite(ERR, "createFile: subdirectory '%s' full", part1);
            freeBlocks(startBlock, blocksNeeded);
            pthread_mutex_unlock(&globalLock);
            return -1;
        }
        ret = writeSubDirEntry(parent->subDirStart, parent->subDirBlocks,
                               slot, &newEntry);
    }

    pthread_mutex_unlock(&globalLock);

    /* 模拟创建耗时 */
    sleep(1);

    if (ret == 0) {
        logWrite(INF, "File created: %s, size=%d, blocks=%d at %d",
                 path, size, blocksNeeded, startBlock);
    }
    return ret;
}

/* 创建子目录 */
int createDir(const char *path)
{
    if (path == NULL) {
        logWrite(ERR, "createDir: NULL path");
        return -1;
    }

    pthread_mutex_lock(&globalLock);

    if (path[0] != '/') {
        logWrite(ERR, "createDir: path must start with '/'");
        pthread_mutex_unlock(&globalLock);
        return -1;
    }

    /* 检查是否已存在 */
    if (findEntry(path) != NULL) {
        logWrite(ERR, "createDir: '%s' already exists", path);
        pthread_mutex_unlock(&globalLock);
        return -1;
    }

    /* 解析路径：目录只能是一级路径 /dirname */
    char pathCopy[MAX_PATH];
    strncpy(pathCopy, path, MAX_PATH - 1);
    pathCopy[MAX_PATH - 1] = '\0';
    char *saveptr;
    char *part1 = strtok_r(pathCopy + 1, "/", &saveptr);
    char *part2 = strtok_r(NULL, "/", &saveptr);

    if (part1 == NULL || part2 != NULL) {
        logWrite(ERR, "createDir: only single-level dirs supported: %s",
                 path);
        pthread_mutex_unlock(&globalLock);
        return -1;
    }

    /* 为子目录内容分配块（默认 2 块 = 128 字节 = 2 条目） */
    int subBlocks = 2;
    int subStart = allocBlocks(subBlocks);
    if (subStart < 0) {
        logWrite(ERR, "createDir: failed to allocate %d blocks for dir",
                 subBlocks);
        pthread_mutex_unlock(&globalLock);
        return -1;
    }

    /* 初始化子目录区域 */
    DirEntry emptyEntry;
    memset(&emptyEntry, 0, sizeof(DirEntry));
    for (int i = 0; i < (subBlocks * BLOCK_SIZE) / DIR_ENTRY_SIZE; i++) {
        writeSubDirEntry(subStart, subBlocks, i, &emptyEntry);
    }

    /* 在根目录中创建目录项 */
    DirEntry newEntry;
    memset(&newEntry, 0, sizeof(DirEntry));
    strncpy(newEntry.name, part1, MAX_NAME - 1);
    newEntry.name[MAX_NAME - 1] = '\0';
    newEntry.type = TYPE_DIR;
    newEntry.size = 0;
    newEntry.startBlock = subStart;
    newEntry.blockCount = subBlocks;
    newEntry.subDirStart = subStart;
    newEntry.subDirBlocks = subBlocks;
    newEntry.openCount = 0;

    int slot = findFreeRootSlot();
    if (slot < 0) {
        logWrite(ERR, "createDir: root directory full");
        freeBlocks(subStart, subBlocks);
        pthread_mutex_unlock(&globalLock);
        return -1;
    }

    int ret = writeRootEntry(slot, &newEntry);

    pthread_mutex_unlock(&globalLock);

    sleep(1);

    if (ret == 0) {
        logWrite(INF, "Directory created: %s", path);
    }
    return ret;
}

/* 删除文件 */
int deleteFile(const char *path)
{
    if (path == NULL) {
        return -1;
    }

    pthread_mutex_lock(&globalLock);

    DirEntry *entry = findEntry(path);
    if (entry == NULL) {
        logWrite(ERR, "deleteFile: '%s' not found", path);
        pthread_mutex_unlock(&globalLock);
        return -1;
    }

    if (entry->type != TYPE_FILE) {
        logWrite(ERR, "deleteFile: '%s' is not a file", path);
        pthread_mutex_unlock(&globalLock);
        return -1;
    }

    if (entry->openCount > 0) {
        logWrite(ERR, "deleteFile: '%s' is currently open by %d users",
                 path, entry->openCount);
        pthread_mutex_unlock(&globalLock);
        return -1;
    }

    /* 回收磁盘块 */
    freeBlocks(entry->startBlock, entry->blockCount);

    /* 从目录中删除条目（清零） */
    DirEntry emptyEntry;
    memset(&emptyEntry, 0, sizeof(DirEntry));

    /* 解析路径以确定在哪级目录 */
    char pathCopy[MAX_PATH];
    strncpy(pathCopy, path, MAX_PATH - 1);
    pathCopy[MAX_PATH - 1] = '\0';
    char *saveptr;
    char *part1 = strtok_r(pathCopy + 1, "/", &saveptr);
    char *part2 = strtok_r(NULL, "/", &saveptr);

    if (part2 == NULL) {
        /* 在根目录中找并删除 */
        DirEntry e;
        for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
            if (readRootEntry(i, &e) != 0) {
                continue;
            }
            if (e.name[0] != '\0' && strcmp(e.name, part1) == 0) {
                writeRootEntry(i, &emptyEntry);
                break;
            }
        }
    } else {
        /* 在子目录中找并删除 */
        DirEntry *parent = getParentDir(part1);
        if (parent != NULL) {
            int maxEntries = (parent->subDirBlocks * BLOCK_SIZE) /
                             DIR_ENTRY_SIZE;
            DirEntry e;
            for (int i = 0; i < maxEntries; i++) {
                if (readSubDirEntry(parent->subDirStart,
                                    parent->subDirBlocks, i, &e) != 0) {
                    continue;
                }
                if (e.name[0] != '\0' && strcmp(e.name, part2) == 0) {
                    writeSubDirEntry(parent->subDirStart,
                                     parent->subDirBlocks, i, &emptyEntry);
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&globalLock);

    sleep(1);

    logWrite(INF, "File deleted: %s", path);
    return 0;
}

/* 删除子目录（目录必须为空） */
int deleteDir(const char *path)
{
    if (path == NULL) {
        return -1;
    }

    pthread_mutex_lock(&globalLock);

    DirEntry *entry = findEntry(path);
    if (entry == NULL) {
        logWrite(ERR, "deleteDir: '%s' not found", path);
        pthread_mutex_unlock(&globalLock);
        return -1;
    }

    if (entry->type != TYPE_DIR) {
        logWrite(ERR, "deleteDir: '%s' is not a directory", path);
        pthread_mutex_unlock(&globalLock);
        return -1;
    }

    /* 检查子目录是否为空 */
    int maxEntries = (entry->subDirBlocks * BLOCK_SIZE) / DIR_ENTRY_SIZE;
    DirEntry e;
    for (int i = 0; i < maxEntries; i++) {
        if (readSubDirEntry(entry->subDirStart, entry->subDirBlocks,
                            i, &e) != 0) {
            continue;
        }
        if (e.name[0] != '\0') {
            logWrite(ERR, "deleteDir: directory '%s' is not empty", path);
            pthread_mutex_unlock(&globalLock);
            return -1;
        }
    }

    /* 回收子目录块 */
    freeBlocks(entry->subDirStart, entry->subDirBlocks);

    /* 从根目录删除 */
    char pathCopy[MAX_PATH];
    strncpy(pathCopy, path, MAX_PATH - 1);
    pathCopy[MAX_PATH - 1] = '\0';
    char *saveptr;
    char *part1 = strtok_r(pathCopy + 1, "/", &saveptr);

    DirEntry emptyEntry;
    memset(&emptyEntry, 0, sizeof(DirEntry));

    if (part1 != NULL) {
        DirEntry rootEntry;
        for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
            if (readRootEntry(i, &rootEntry) != 0) {
                continue;
            }
            if (rootEntry.name[0] != '\0' &&
                strcmp(rootEntry.name, part1) == 0) {
                writeRootEntry(i, &emptyEntry);
                break;
            }
        }
    }

    pthread_mutex_unlock(&globalLock);

    sleep(1);

    logWrite(INF, "Directory deleted: %s", path);
    return 0;
}

/* 显示目录内容 */
int listDir(const char *path)
{
    if (path == NULL) {
        return -1;
    }

    pthread_mutex_lock(&globalLock);

    printf(BOLD COLOR_BLUE "\n=== Directory: %s ===" COLOR_RESET "\n", path);
    printf("%-28s %-6s %-8s %-6s %-6s\n",
           "Name", "Type", "Size", "Blocks", "Start");
    printf("----------------------------------------------\n");

    if (strcmp(path, "/") == 0) {
        /* 列出根目录 */
        DirEntry entry;
        for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
            if (readRootEntry(i, &entry) != 0) {
                continue;
            }
            if (entry.name[0] != '\0') {
                const char *typeStr = (entry.type == TYPE_DIR) ? "DIR" : "FILE";
                printf("%-28s %-6s %-8d %-6d %-6d\n",
                       entry.name, typeStr, entry.size,
                       entry.blockCount, entry.startBlock);
            }
        }
    } else {
        /* 列出子目录 */
        DirEntry *parent = findEntry(path);
        if (parent == NULL || parent->type != TYPE_DIR) {
            printf("(not a valid directory)\n");
            pthread_mutex_unlock(&globalLock);
            return -1;
        }
        int maxEntries = (parent->subDirBlocks * BLOCK_SIZE) /
                         DIR_ENTRY_SIZE;
        DirEntry entry;
        for (int i = 0; i < maxEntries; i++) {
            if (readSubDirEntry(parent->subDirStart, parent->subDirBlocks,
                                i, &entry) != 0) {
                continue;
            }
            if (entry.name[0] != '\0') {
                const char *typeStr = (entry.type == TYPE_DIR) ? "DIR" : "FILE";
                printf("%-28s %-6s %-8d %-6d %-6d\n",
                       entry.name, typeStr, entry.size,
                       entry.blockCount, entry.startBlock);
            }
        }
    }

    printf("\n");
    pthread_mutex_unlock(&globalLock);
    return 0;
}

/* ========== 打开/关闭文件 ========== */

int openFile(const char *path)
{
    if (path == NULL) {
        return -1;
    }

    pthread_mutex_lock(&globalLock);

    DirEntry *entry = findEntry(path);
    if (entry == NULL || entry->type != TYPE_FILE) {
        logWrite(ERR, "openFile: '%s' not found or not a file", path);
        pthread_mutex_unlock(&globalLock);
        return -1;
    }

    /* 找空闲 fd */
    int fd = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!openFileTable[i].used) {
            fd = i;
            break;
        }
    }
    if (fd < 0) {
        logWrite(ERR, "openFile: open file table full");
        pthread_mutex_unlock(&globalLock);
        return -1;
    }

    openFileTable[fd].used = 1;
    strncpy(openFileTable[fd].path, path, MAX_PATH - 1);
    openFileTable[fd].path[MAX_PATH - 1] = '\0';
    openFileTable[fd].offset = 0;
    openFileTable[fd].entry = entry;
    entry->openCount++;

    pthread_mutex_unlock(&globalLock);

    logWrite(INF, "File opened: %s, fd=%d", path, fd);
    return fd;
}

int closeFile(int fd)
{
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        logWrite(ERR, "closeFile: invalid fd %d", fd);
        return -1;
    }

    pthread_mutex_lock(&globalLock);

    if (!openFileTable[fd].used) {
        logWrite(ERR, "closeFile: fd %d not in use", fd);
        pthread_mutex_unlock(&globalLock);
        return -1;
    }

    /* 重新查找条目以更新 openCount */
    DirEntry *entry = findEntry(openFileTable[fd].path);
    if (entry != NULL) {
        if (entry->openCount > 0) {
            entry->openCount--;
        }
    }

    openFileTable[fd].used = 0;
    openFileTable[fd].path[0] = '\0';
    openFileTable[fd].offset = 0;
    openFileTable[fd].entry = NULL;

    /* 刷新脏缓冲 */
    flushAll();

    pthread_mutex_unlock(&globalLock);

    logWrite(INF, "File closed: fd=%d", fd);
    return 0;
}

/* ========== 文件读写（经过缓冲页） ========== */

int readFile(int fd, char *buf, int size)
{
    if (fd < 0 || fd >= MAX_OPEN_FILES || buf == NULL || size <= 0) {
        return -1;
    }

    pthread_mutex_lock(&globalLock);

    if (!openFileTable[fd].used) {
        logWrite(ERR, "readFile: fd %d not open", fd);
        pthread_mutex_unlock(&globalLock);
        return -1;
    }

    /* 重新查找以获取最新 entry 数据 */
    DirEntry *entry = findEntry(openFileTable[fd].path);
    if (entry == NULL) {
        logWrite(ERR, "readFile: entry for fd %d disappeared", fd);
        pthread_mutex_unlock(&globalLock);
        return -1;
    }

    int fileSize = entry->size;
    int offset = openFileTable[fd].offset;

    if (offset >= fileSize) {
        pthread_mutex_unlock(&globalLock);
        return 0; /* EOF */
    }

    int bytesToRead = size;
    if (offset + bytesToRead > fileSize) {
        bytesToRead = fileSize - offset;
    }

    int bytesRead = 0;
    while (bytesRead < bytesToRead) {
        int absPos = offset + bytesRead;
        int blockIndex = absPos / BLOCK_SIZE;
        int blockOffset = absPos % BLOCK_SIZE;
        int blockNo = entry->startBlock + blockIndex;

        int slot = getBufferPage(blockNo);
        if (slot < 0) {
            pthread_mutex_unlock(&globalLock);
            return bytesRead > 0 ? bytesRead : -1;
        }

        int chunk = bytesToRead - bytesRead;
        int availInBlock = BLOCK_SIZE - blockOffset;
        if (chunk > availInBlock) {
            chunk = availInBlock;
        }

        memcpy(buf + bytesRead,
               bufferPool[slot].data + blockOffset, chunk);
        bytesRead += chunk;
    }

    openFileTable[fd].offset += bytesRead;

    pthread_mutex_unlock(&globalLock);

    logWrite(INF, "Read %d bytes from fd=%d", bytesRead, fd);
    return bytesRead;
}

int writeFile(int fd, const char *buf, int size)
{
    if (fd < 0 || fd >= MAX_OPEN_FILES || buf == NULL || size <= 0) {
        return -1;
    }

    pthread_mutex_lock(&globalLock);

    if (!openFileTable[fd].used) {
        logWrite(ERR, "writeFile: fd %d not open", fd);
        pthread_mutex_unlock(&globalLock);
        return -1;
    }

    DirEntry *entry = findEntry(openFileTable[fd].path);
    if (entry == NULL) {
        logWrite(ERR, "writeFile: entry for fd %d disappeared", fd);
        pthread_mutex_unlock(&globalLock);
        return -1;
    }

    int fileSize = entry->size;
    int offset = openFileTable[fd].offset;

    if (offset >= fileSize) {
        logWrite(ERR, "writeFile: offset %d beyond file size %d",
                 offset, fileSize);
        pthread_mutex_unlock(&globalLock);
        return -1;
    }

    int bytesToWrite = size;
    if (offset + bytesToWrite > fileSize) {
        bytesToWrite = fileSize - offset;
    }

    int bytesWritten = 0;
    while (bytesWritten < bytesToWrite) {
        int absPos = offset + bytesWritten;
        int blockIndex = absPos / BLOCK_SIZE;
        int blockOffset = absPos % BLOCK_SIZE;
        int blockNo = entry->startBlock + blockIndex;

        int slot = getBufferPage(blockNo);
        if (slot < 0) {
            pthread_mutex_unlock(&globalLock);
            return bytesWritten > 0 ? bytesWritten : -1;
        }

        int chunk = bytesToWrite - bytesWritten;
        int availInBlock = BLOCK_SIZE - blockOffset;
        if (chunk > availInBlock) {
            chunk = availInBlock;
        }

        memcpy(bufferPool[slot].data + blockOffset,
               buf + bytesWritten, chunk);
        markDirty(slot);
        bytesWritten += chunk;
    }

    openFileTable[fd].offset += bytesWritten;

    pthread_mutex_unlock(&globalLock);

    sleep(1);

    logWrite(INF, "Wrote %d bytes to fd=%d", bytesWritten, fd);
    return bytesWritten;
}

/* ========== 命令处理函数 ========== */

static int parsePathAndSize(int argc, char **argv, char *pathBuf,
                            int *sizeOut)
{
    if (argc < 3) {
        return -1;
    }
    strncpy(pathBuf, argv[1], MAX_PATH - 1);
    pathBuf[MAX_PATH - 1] = '\0';
    *sizeOut = atoi(argv[2]);
    if (*sizeOut <= 0) {
        return -1;
    }
    return 0;
}

int doMkfile(int argc, char **argv)
{
    char path[MAX_PATH];
    int size;
    if (parsePathAndSize(argc, argv, path, &size) != 0) {
        printf("Usage: mkfile <path> <size>\\n");
        return -1;
    }
    return createFile(path, size);
}

int doMkdir(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: mkdir <path>\\n");
        return -1;
    }
    return createDir(argv[1]);
}

int doRm(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: rm <path>\\n");
        return -1;
    }
    return deleteFile(argv[1]);
}

int doRmdir(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: rmdir <path>\\n");
        return -1;
    }
    return deleteDir(argv[1]);
}

int doLs(int argc, char **argv)
{
    const char *path = "/";
    if (argc >= 2) {
        path = argv[1];
    }
    return listDir(path);
}

int doRead(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: read <path> <size>\\n");
        return -1;
    }
    int size = atoi(argv[2]);
    if (size <= 0) {
        printf("read: invalid size\\n");
        return -1;
    }

    int fd = openFile(argv[1]);
    if (fd < 0) {
        printf("read: failed to open '%s'\\n", argv[1]);
        return -1;
    }

    char *buf = (char *)malloc(size + 1);
    if (buf == NULL) {
        closeFile(fd);
        return -1;
    }

    int bytesRead = readFile(fd, buf, size);
    if (bytesRead > 0) {
        buf[bytesRead] = '\0';
        printf("Read %d bytes: %s\\n", bytesRead, buf);
    }

    free(buf);
    closeFile(fd);
    return (bytesRead >= 0) ? 0 : -1;
}

int doWrite(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: write <path> <data>\\n");
        return -1;
    }

    int fd = openFile(argv[1]);
    if (fd < 0) {
        printf("write: failed to open '%s'\\n", argv[1]);
        return -1;
    }

    int size = strlen(argv[2]);
    int bytesWritten = writeFile(fd, argv[2], size);

    closeFile(fd);
    return (bytesWritten >= 0) ? 0 : -1;
}

int doDf(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    pthread_mutex_lock(&globalLock);

    int freeBlocks = getFreeBlockCount();
    int usedBlocks = BLOCK_NUM - freeBlocks;
    int totalSize = BLOCK_NUM * BLOCK_SIZE;
    int usedSize = usedBlocks * BLOCK_SIZE;
    int freeSize = freeBlocks * BLOCK_SIZE;

    printf(BOLD COLOR_CYAN "\n=== Disk Usage ===" COLOR_RESET "\n");
    printf("Total blocks: %d (%d bytes)\n", BLOCK_NUM, totalSize);
    printf("Used blocks:  %d (%d bytes)\n", usedBlocks, usedSize);
    printf("Free blocks:  %d (%d bytes)\n", freeBlocks, freeSize);
    printf("Usage:        %.1f%%\n",
           100.0 * usedBlocks / BLOCK_NUM);

    pthread_mutex_unlock(&globalLock);
    return 0;
}

int doOpen(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: open <path>\n");
        return -1;
    }
    int fd = openFile(argv[1]);
    if (fd >= 0) {
        printf("Opened '%s' as fd=%d\n", argv[1], fd);
    }
    return (fd >= 0) ? 0 : -1;
}

int doClose(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: close <fd>\n");
        return -1;
    }
    int fd = atoi(argv[1]);
    int ret = closeFile(fd);
    if (ret == 0) {
        printf("Closed fd=%d\n", fd);
    }
    return ret;
}

/* 文件系统关闭 */
void fsShutdown(void)
{
    pthread_mutex_lock(&globalLock);
    flushAll();

    /* 关闭所有打开的文件 */
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (openFileTable[i].used) {
            logWrite(WRN, "Force closing fd=%d (%s) on shutdown",
                     i, openFileTable[i].path);
            openFileTable[i].used = 0;
        }
    }

    pthread_mutex_unlock(&globalLock);
    logWrite(INF, "File system shut down");
}