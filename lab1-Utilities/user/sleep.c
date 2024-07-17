#include "kernel/types.h"
#include "user/user.h"

/*
argc：表示命令行参数的数量。
argv：表示指向命令行参数的指针数组,是指向字符数组的指针
*/
int main(int argc, char const *argv[])
{
    // 因为命令为sleep 10，所以argc==2
    if (argc != 2)
    {
        printf("error! usage: sleep <time>\n");
        exit(1);
    }
    // atoi：将字符串转换为整数
    // sleep：使程序暂停执行一段时间
    sleep(atoi(argv[1]));
    exit(0);
}