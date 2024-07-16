#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"


#define MAX_LEN 512

int main(int argc, char *argv[]) {
    // 参数数目不对
    if (argc - 1 >= MAXARG || argc < 2) {
        printf("error! xargs: please check the number of arguments.\n");
        exit(1);
    }
    // 保存命令参数
    int i=1;
    char *x_argv[MAXARG];
    for (i = 1; i < argc; ++i) 
        x_argv[i - 1] = argv[i];
    

    // 存储行/定位指针和exec的参数数组
    char line[MAX_LEN];
    char *p = line;


    //记录读取到的数据字节数
    int rsz = sizeof(char);
    while (rsz == sizeof(char)) {
        // 定位参数起始 结束位置和参数次序
        int word_begin = 0;
        int word_end = 0;
        int arg_cnt = i - 1;
        //读取一行
        while (1) {
            rsz = read(0, p, sizeof(char));
            /*
            正数: 表示成功读取的字节数。sizeof(char) 是1，所以返回值通常是 1，表示成功读取了一个字节。
            0: 表示已经到达文件末尾（EOF）。对于标准输入，这通常发生在输入流关闭时，例如按 Ctrl+D（在 UNIX 系统）或 Ctrl+Z（在 Windows 系统）。
            -1: 表示读取操作失败。这时可以检查 errno 以获取错误的详细信息。 
            */
            if (++word_end >= MAX_LEN) {
                printf("xargs: arguments too long.\n");
                exit(1);
            }

            if (*p == '\0' || *p == '\n' || rsz != sizeof(char)) {
                //用字符串结束标志替换'\0'，'\n'和空字符
                *p = 0;
                x_argv[arg_cnt++] = &line[word_begin];
                //表明下一个参数的位置，在当前参数结束符之后
                word_begin = word_end;
                if (arg_cnt >= MAXARG) {
                    printf("xargs: too many arguments.\n");
                    exit(1);
                }
                if (*p == '\n' || rsz != sizeof(char))
                    break;
            }
            ++p;
        }
        if (fork() == 0) {
            exec(argv[1], x_argv);
        }
        wait(0);
    }
    exit(0);
}