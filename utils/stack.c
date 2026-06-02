#include <stdio.h>
#include "stack.h"

void init_stack(Stack *s) {
    s->top = -1;
}

int stack_full(Stack *s) {
    return s->top == MAX_SIZE - 1;
}

int stack_empty(Stack *s) {
    return s->top == -1;
}

void push(Stack *s, int value) {
    if (stack_full(s)) {
        printf("Stack overflow\n");
        return;
    }
    s->data[++s->top] = value;
}

int pop(Stack *s) {
    if (stack_empty(s)) {
        printf("Stack underflow\n");
        return -1; // 注意：-1 可能是合法数据
    }
    return s->data[s->top--];
}

// 新增：取栈顶元素（不改变栈）
int peek(Stack *s) {
    if (stack_empty(s)) {
        printf("Stack is empty\n");
        return -1;
    }
    return s->data[s->top];
}