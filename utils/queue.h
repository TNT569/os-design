#ifndef QUEUE_H
#define QUEUE_H

#include "stack.h" // 包含你的栈定义

typedef struct {
  Stack input_stack;  // 用于入队
  Stack output_stack; // 用于出队
} Queue;

/* 初始化队列 */
void init_queue(Queue *q);

/* 检查队列是否为空 */
int queue_empty(Queue *q);

/* 入队操作 */
void enqueue(Queue *q, int value);

/* 出队操作 */
int dequeue(Queue *q);

/* 获取队首元素但不删除 */
int queue_front(Queue *q);

/* 获取队列大小 */
int queue_size(Queue *q);

#endif /* QUEUE_USING_STACKS_H */
