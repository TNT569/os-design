#ifndef STACK_H
#define STACK_H

#define MAX_SIZE 500 

typedef struct {
    int data[MAX_SIZE];
    int top;
} Stack;

// 初始化栈
void init_stack(Stack *s);

// 判断栈是否满
int stack_full(Stack *s);

// 判断栈是否空
int stack_empty(Stack *s);

// 入栈
void push(Stack *s, int value);

// 出栈
int pop(Stack *s);

// 取栈顶元素（不弹出）
int peek(Stack *s);

#endif // STACK_H
