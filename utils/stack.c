#include "stack.h"
#include <stdio.h>

void initStack(Stack *stack) { stack->top = -1; }

int stackFull(Stack *stack) { return stack->top == MAX_SIZE - 1; }

int stackEmpty(Stack *stack) { return stack->top == -1; }

void push(Stack *stack, int value) {
  if (stackFull(stack)) {
    printf("Stack overflow\n");
    return;
  }
  stack->data[++stack->top] = value;
}

int pop(Stack *stack) {
  if (stackEmpty(stack)) {
    printf("Stack underflow\n");
    return -1; // 注意：-1 可能是合法数据
  }
  return stack->data[stack->top--];
}

// 新增：取栈顶元素（不改变栈）
int peek(Stack *stack) {
  if (stackEmpty(stack)) {
    printf("Stack is empty\n");
    return -1;
  }
  return stack->data[stack->top];
}
