#ifndef BUFFER_H
#define BUFFER_H

#include "common.h"

/* 初始化缓冲池 */
void initBuffer(void);

/* 获取块号为 blockNo 的缓冲页槽位号
 * 若已在缓冲池中则直接返回槽位号
 * 若不在则使用 FIFO 置换加载，返回槽位号 */
int getBufferPage(int blockNo);

/* 标记槽位为脏 */
void markDirty(int slot);

/* 将槽位写回磁盘并标记为干净 */
void flushBuffer(int slot);

/* 刷新所有脏页 */
void flushAll(void);

/* 打印缓冲状态（用于可视化） */
void printBufferState(void);

#endif /* BUFFER_H */