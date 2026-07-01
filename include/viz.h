#ifndef VIZ_H
#define VIZ_H

#include "common.h"

/* 启动可视化线程 */
int vizStart(void);

/* 停止可视化线程 */
void vizStop(void);

/* 命令处理函数 */
int doViz(int argc, char **argv);

#endif /* VIZ_H */