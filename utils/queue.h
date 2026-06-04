#ifndef QUEUE_H
#define QUEUE_H

#include "stack.h" // 包含你的栈定义

typedef struct {
  Stack input_stack;  // 用于入队
  Stack output_stack; // 用于出队
} Queue;

/* 初始化队列 */
void initQueue(Queue *queue);

/* 检查队列是否为空 */
int queueEmpty(Queue *queue);

/* 检查队列是否已满 */
int queueFull(Queue *queue);

/* 入队操作 */
void enqueue(Queue *queue, int value);

/* 出队操作 */
int dequeue(Queue *queue);

/* 获取队首元素但不删除 */
int queueFront(Queue *queue);

/* 获取队列大小 */
int queueSize(Queue *queue);

#endif /* QUEUE_H */
