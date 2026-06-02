#include "queue.h"
#include <stdio.h>

void init_queue(Queue *q) {
  init_stack(&q->input_stack);
  init_stack(&q->output_stack);
}

int queue_empty(Queue *q) {
  return stack_empty(&q->input_stack) && stack_empty(&q->output_stack);
}

void enqueue(Queue *q, int value) {
  // 直接入栈到input_stack
  push(&q->input_stack, value);
}

int dequeue(Queue *q) {
  if (queue_empty(q)) {
    printf("Queue is empty\n");
    return -1;
  }

  // 如果output_stack为空，则从input_stack转移元素
  if (stack_empty(&q->output_stack)) {
    while (!stack_empty(&q->input_stack)) {
      int value = pop(&q->input_stack);
      push(&q->output_stack, value);
    }
  }

  // 从output_stack弹出
  return pop(&q->output_stack);
}

int queue_front(Queue *q) {
  if (queue_empty(q)) {
    printf("Queue is empty\n");
    return -1;
  }

  // 如果output_stack为空，则从input_stack转移所有元素
  if (stack_empty(&q->output_stack)) {
    while (!stack_empty(&q->input_stack)) {
      int value = pop(&q->input_stack);
      push(&q->output_stack, value);
    }
  }

  // 直接查看output_stack的栈顶（即队列前端）
  return peek(&q->output_stack);
}

int queue_size(Queue *q) {
  // 栈的top索引+1就是元素数量
  return (q->input_stack.top + 1) + (q->output_stack.top + 1);
}

/* 打印队列内容（用于调试） */
void print_queue(Queue *q) {
  printf("Queue contents (front to rear): ");

  // 先打印output_stack（从栈顶到栈底）
  printf("[Output stack: ");
  for (int i = q->output_stack.top; i >= 0; i--) {
    printf("%d ", q->output_stack.data[i]);
  }
  printf("] ");

  // 再打印input_stack（从栈底到栈顶，因为这是入队的顺序）
  printf("[Input stack: ");
  for (int i = 0; i <= q->input_stack.top; i++) {
    printf("%d ", q->input_stack.data[i]);
  }
  printf("]\n");
}
