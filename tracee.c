#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

int main()
{
    printf("The child process is preparing be trapped.\n");
    // raise(SIGSEGV);
    /* long enough instruction sequence to fill up a word */
    asm volatile(
        "xor %eax, %eax\n"
        "xor %eax, %eax\n"
        "xor %eax, %eax\n"
        "xor %eax, %eax\n"
        "xor %eax, %eax\n"
        "xor %eax, %eax\n"
        "xor %eax, %eax\n"
        "xor %eax, %eax"
    );
    // asm volatile ("mov $1, %rax");
    // char *addr = 0x0;
    // *addr = 1;
    printf("If we reach here, the child process has resumed.\n");

    return 0;
}