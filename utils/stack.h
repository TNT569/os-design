#ifndef STACK_H
#define STACK_H

#define MAX_SIZE 500

typedef struct {
  int data[MAX_SIZE];
  int top;
} Stack;

// 初始化栈
void initStack(Stack *stack);

// 判断栈是否满
int stackFull(Stack *stack);

// 判断栈是否空
int stackEmpty(Stack *stack);

// 入栈
void push(Stack *stack, int value);

// 出栈
int pop(Stack *stack);

// 取栈顶元素（不弹出）
int peek(Stack *stack);

#endif // STACK_H
