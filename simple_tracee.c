#include <stdio.h>
#include <signal.h>

int main()
{
    printf("1\n");
    // raise(SIGSEGV);
    asm volatile("int3");
    printf("2\n");
    // raise(SIGINT);
    asm volatile("int3");
    printf("3\n");
    // raise(SIGSEGV);
    asm volatile("int3");
    printf("4\n");

    return 0;
}