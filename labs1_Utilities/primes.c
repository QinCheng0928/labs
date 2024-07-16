#include "kernel/types.h"
#include "user/user.h"

#define MAXNUM 36

void primes(int pipe_read, int pipe_write)
{
    int index = 0;
    char buff[MAXNUM];
    read(pipe_read, buff, MAXNUM);

    for (int i = 0; i < MAXNUM; ++i)
    {
        if ('0' != buff[i])
        {
            index = i;
            break;
        }
    }
    if (0 == index)
        return;
        
    printf("prime %d\n", (buff[index]-'0'));

    buff[index] = '0';
    for (int i = 0; i < MAXNUM; ++i)
    {
        if (0 == (int)(buff[i]-'0') % index)
            buff[i] = '0';
    }

    int pid = fork();
    if (pid > 0)
    {
        write(pipe_write, buff, MAXNUM);
        wait(0);
    }
    if (0 == pid)
    {
        primes(pipe_read, pipe_write);
        exit(0);
    }
}

int main()
{
    int fd[2];
    pipe(fd);
    char num[MAXNUM] = {0};

    int pid = fork();

    if (pid > 0) // 父进程
    {
        // 初始化
        for (int i = 0; i < MAXNUM; ++i)
        {
            if (i < 2)
                num[i] = '0';
            else
                num[i] = i+'0';
        }
        write(fd[1], num, MAXNUM);
        wait(0);
    }
    if (0 == pid) // 子进程
    {
        primes(fd[0], fd[1]);
    }

    exit(0);
}