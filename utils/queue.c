#include "queue.h"
#include <stdio.h>

void initQueue(Queue *queue) {
  initStack(&queue->input_stack);
  initStack(&queue->output_stack);
}

int queueEmpty(Queue *queue) {
  return stackEmpty(&queue->input_stack) && stackEmpty(&queue->output_stack);
}

void enqueue(Queue *queue, int value) {
  // 直接入栈到input_stack
  push(&queue->input_stack, value);
}

int dequeue(Queue *queue) {
  if (queueEmpty(queue)) {
    printf("Queue is empty\n");
    return -1;
  }

  // 如果output_stack为空，则从input_stack转移元素
  if (stackEmpty(&queue->output_stack)) {
    while (!stackEmpty(&queue->input_stack)) {
      int value = pop(&queue->input_stack);
      push(&queue->output_stack, value);
    }
  }

  // 从output_stack弹出
  return pop(&queue->output_stack);
}

int queueFront(Queue *queue) {
  if (queueEmpty(queue)) {
    printf("Queue is empty\n");
    return -1;
  }

  // 如果output_stack为空，则从input_stack转移所有元素
  if (stackEmpty(&queue->output_stack)) {
    while (!stackEmpty(&queue->input_stack)) {
      int value = pop(&queue->input_stack);
      push(&queue->output_stack, value);
    }
  }

  // 直接查看output_stack的栈顶（即队列前端）
  return peek(&queue->output_stack);
}

int queueSize(Queue *queue) {
  // 栈的top索引+1就是元素数量
  return (queue->input_stack.top + 1) + (queue->output_stack.top + 1);
}

/* 打印队列内容（用于调试） */
void printQueue(Queue *queue) {
  printf("Queue contents (front to rear): ");

  // 先打印output_stack（从栈顶到栈底）
  printf("[Output stack: ");
  for (int i = queue->output_stack.top; i >= 0; i--) {
    printf("%d ", queue->output_stack.data[i]);
  }
  printf("] ");

  // 再打印input_stack（从栈底到栈顶，因为这是入队的顺序）
  printf("[Input stack: ");
  for (int i = 0; i <= queue->input_stack.top; i++) {
    printf("%d ", queue->input_stack.data[i]);
  }
  printf("]\n");
}
