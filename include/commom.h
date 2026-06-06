#ifndef COMMON_H
#define COMMON_H

// 前景色
#define COLOR_BLACK "\033[30m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN "\033[36m"
#define COLOR_WHITE "\033[37m"

// 背景色
#define BG_BLACK "\033[40m"
#define BG_RED "\033[41m"
#define BG_GREEN "\033[42m"
#define BG_YELLOW "\033[43m"
#define BG_BLUE "\033[44m"
#define BG_MAGENTA "\033[45m"
#define BG_CYAN "\033[46m"
#define BG_WHITE "\033[47m"

// 样式
#define BOLD "\033[1m"
#define UNDERLINE "\033[4m"
#define BLINK "\033[5m"   // 部分终端不支持
#define REVERSE "\033[7m" // 交换前景背景色

// 重置所有属性
#define COLOR_RESET "\033[0m"

#endif
