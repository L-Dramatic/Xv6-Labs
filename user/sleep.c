#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) 
{
    // 参数检查：需要 exactly 1 个参数
    if (argc != 2) {
        // 输出到标准错误 (fd=2)
        fprintf(2, "Usage: sleep <ticks>\n");
        exit(1); // 错误退出
    }
    
    // 转换字符串为整数
    int ticks = atoi(argv[1]);
    
    // 调用系统睡眠函数
    sleep(ticks);
    
    exit(0); // 成功退出
}
