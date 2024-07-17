#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"


#define MAX_LEN 1024

//echo hello too | xargs echo bye

int main(int argc, char *argv[]) {
    // 缺少参数
    if (argc < 2) {
        printf("Usage: xargs command [args...]\n");
        exit(1);
    }

    // 保存命令
    //x_argv echo bye
    int i=1;
    char *x_argv[MAXARG];
    for (i = 1; i < argc; ++i) 
        x_argv[i - 1] = argv[i];
    

    char buf[MAX_LEN];
    int index = 0;
    char c;

    /*
    正数: 表示成功读取的字节数。sizeof(char) 是1，所以返回值通常是 1，表示成功读取了一个字节。
    0: 表示已经到达文件末尾（EOF）。对于标准输入，这通常发生在输入流关闭时，例如按 Ctrl+D（在 UNIX 系统）或 Ctrl+Z（在 Windows 系统）。
    -1: 表示读取操作失败。这时可以检查 errno 以获取错误的详细信息。 
    
    read读取的是hello too
    */
    while (read(0, &c, 1) > 0) {
        if (c == '\n' || c == '\0') {
            if (index > 0) {
                buf[index] = '\0';
                x_argv[argc - 1] = buf;
                x_argv[argc] = 0;

                if (fork() == 0) //子进程
                {
                    //执行echo bye hello too
                    exec(x_argv[0], x_argv);
                    fprintf(2, "exec %s failed\n", x_argv[0]);
                    exit(1);
                } 
                else 
                {
                    wait(0);
                }

                index = 0;
            }
        } 
        else 
        {
            buf[index++] = c;
        }
    }
    exit(0);
}