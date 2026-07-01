#include "buffer.h"
#include "disk.h"
#include "log.h"
#include "queue.h"
#include <string.h>
#include <stdio.h>

/* 全局缓冲池 */
BufferPage bufferPool[BUFFER_PAGES];

/* FIFO 队列：记录缓冲页加载顺序，存的是槽位号 */
static Queue fifoQueue;
/* 记录每个槽位是否被占用 */
static int slotInQueue[BUFFER_PAGES];

/* 初始化缓冲池 */
void initBuffer(void)
{
    for (int i = 0; i < BUFFER_PAGES; i++) {
        bufferPool[i].blockNo = -1;
        bufferPool[i].dirty = 0;
        memset(bufferPool[i].data, 0, BLOCK_SIZE);
    }
    initQueue(&fifoQueue);
    for (int i = 0; i < BUFFER_PAGES; i++) {
        slotInQueue[i] = 0;
    }
    logWrite(INF, "Buffer pool initialized: %d pages", BUFFER_PAGES);
}

/* 在缓冲池中查找块，返回槽位号，未找到返回 -1 */
static int findInBuffer(int blockNo)
{
    for (int i = 0; i < BUFFER_PAGES; i++) {
        if (bufferPool[i].blockNo == blockNo) {
            return i;
        }
    }
    return -1;
}

/* 找一个空闲槽位，返回槽位号，没有空闲返回 -1 */
static int findFreeSlot(void)
{
    for (int i = 0; i < BUFFER_PAGES; i++) {
        if (bufferPool[i].blockNo == -1) {
            return i;
        }
    }
    return -1;
}

/* 获取块号为 blockNo 的缓冲页槽位号，使用 FIFO 置换 */
int getBufferPage(int blockNo)
{
    if (blockNo < 0 || blockNo >= BLOCK_NUM) {
        logWrite(ERR, "getBufferPage: invalid blockNo %d", blockNo);
        return -1;
    }

    /* 1. 检查是否已在缓冲池中 */
    int slot = findInBuffer(blockNo);
    if (slot >= 0) {
        /* 命中，移到队尾（重新入队以实现 LRU-lite 行为更合理，
         * 但规范要求 FIFO，所以命中时不更新队列位置） */
        return slot;
    }

    /* 2. 找空闲槽位 */
    slot = findFreeSlot();
    if (slot >= 0) {
        /* 从磁盘加载到缓冲页 */
        if (readDiskBlock(blockNo, bufferPool[slot].data) != 0) {
            logWrite(ERR, "getBufferPage: readDiskBlock failed for block %d",
                     blockNo);
            return -1;
        }
        bufferPool[slot].blockNo = blockNo;
        bufferPool[slot].dirty = 0;
        /* 入 FIFO 队 */
        enqueue(&fifoQueue, slot);
        slotInQueue[slot] = 1;
        logWrite(INF, "Buffer: loaded block %d into slot %d", blockNo, slot);
        return slot;
    }

    /* 3. 所有槽位已满，FIFO 置换 */
    int evictSlot = dequeue(&fifoQueue);
    if (evictSlot < 0) {
        logWrite(ERR, "getBufferPage: FIFO queue is empty");
        return -1;
    }
    slotInQueue[evictSlot] = 0;

    /* 若脏，写回磁盘 */
    if (bufferPool[evictSlot].dirty) {
        if (writeDiskBlock(bufferPool[evictSlot].blockNo,
                           bufferPool[evictSlot].data) != 0) {
            logWrite(ERR, "getBufferPage: writeDiskBlock failed for evicted block %d",
                     bufferPool[evictSlot].blockNo);
            return -1;
        }
        logWrite(INF, "Buffer: flushed dirty block %d from slot %d",
                 bufferPool[evictSlot].blockNo, evictSlot);
    }

    logWrite(INF, "Buffer: evicted block %d from slot %d, loading block %d",
             bufferPool[evictSlot].blockNo, evictSlot, blockNo);

    /* 加载新块 */
    if (readDiskBlock(blockNo, bufferPool[evictSlot].data) != 0) {
        logWrite(ERR, "getBufferPage: readDiskBlock failed for block %d",
                 blockNo);
        bufferPool[evictSlot].blockNo = -1;
        bufferPool[evictSlot].dirty = 0;
        return -1;
    }
    bufferPool[evictSlot].blockNo = blockNo;
    bufferPool[evictSlot].dirty = 0;

    /* 新页入队 */
    enqueue(&fifoQueue, evictSlot);
    slotInQueue[evictSlot] = 1;

    return evictSlot;
}

/* 标记槽位为脏 */
void markDirty(int slot)
{
    if (slot < 0 || slot >= BUFFER_PAGES) {
        logWrite(ERR, "markDirty: invalid slot %d", slot);
        return;
    }
    bufferPool[slot].dirty = 1;
}

/* 将槽位写回磁盘 */
void flushBuffer(int slot)
{
    if (slot < 0 || slot >= BUFFER_PAGES) {
        logWrite(ERR, "flushBuffer: invalid slot %d", slot);
        return;
    }
    if (bufferPool[slot].blockNo < 0) {
        return;
    }
    if (bufferPool[slot].dirty) {
        if (writeDiskBlock(bufferPool[slot].blockNo,
                           bufferPool[slot].data) != 0) {
            logWrite(ERR, "flushBuffer: writeDiskBlock failed for block %d",
                     bufferPool[slot].blockNo);
            return;
        }
        bufferPool[slot].dirty = 0;
        logWrite(INF, "Buffer: flushed slot %d (block %d)", slot,
                 bufferPool[slot].blockNo);
    }
}

/* 刷新所有脏页 */
void flushAll(void)
{
    for (int i = 0; i < BUFFER_PAGES; i++) {
        flushBuffer(i);
    }
    logWrite(INF, "Buffer: all dirty pages flushed");
}

/* 打印缓冲状态 */
void printBufferState(void)
{
    printf(BOLD "=== Buffer State (FIFO) ===" COLOR_RESET "\n");
    printf("Slot  BlockNo  Dirty\n");
    printf("----  -------  -----\n");
    for (int i = 0; i < BUFFER_PAGES; i++) {
        printf(" %2d   ", i);
        if (bufferPool[i].blockNo >= 0) {
            printf("  %4d   ", bufferPool[i].blockNo);
        } else {
            printf("  free   ");
        }
        printf("  %s\n", bufferPool[i].dirty ? "Y" : "N");
    }

    /* 打印 FIFO 队列 */
    printf("\nFIFO queue (front to rear): ");
    if (queueEmpty(&fifoQueue)) {
        printf("empty\n");
    } else {
        /* 先打印 output_stack 栈顶到栈底 (front) */
        for (int i = fifoQueue.output_stack.top; i >= 0; i--) {
            printf("%d ", fifoQueue.output_stack.data[i]);
        }
        /* 再打印 input_stack 栈底到栈顶 (rear) */
        for (int i = 0; i <= fifoQueue.input_stack.top; i++) {
            printf("%d ", fifoQueue.input_stack.data[i]);
        }
        printf("\n");
    }
}