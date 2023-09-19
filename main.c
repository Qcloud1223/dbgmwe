#define _GNU_SOURCE
#include <sys/ptrace.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/user.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage:\n");
        return -EINVAL;
    } 
    
    /* argv[1] is pointing to program being debuged */
    const char *binary = strdup(argv[1]);

    /* read and copy the binary */
    FILE *binary_fp = fopen(binary, "r");
    fseek(binary_fp, 0, SEEK_END);
    long sz = ftell(binary_fp);
    void *buf = malloc(sz);
    rewind(binary_fp);
    fread(buf, 1, sz, binary_fp);
    FILE *aug_fp = fopen("/tmp/debug_binary", "w");
    
    /* get the address from hard code */
    uint64_t text_offset = 0x115d;
    /* one-byte interrupt */
    const unsigned char int3 = 0xcc;
    *((unsigned char *)buf + text_offset) = int3;

    /* write the updated binary on disk */
    fwrite(buf, 1, sz, aug_fp);
    fclose(binary_fp);
    fclose(aug_fp);

    pid_t child;
    if ((child = fork()) == 0) {
        /* the child process should call w/ traceme */
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        raise(SIGSTOP);
        /* the child process now use an modified binary */
        int ret = execl("/tmp/debug_binary", "./debug_binary", NULL);
        // int ret = execl("/home/hypermoon/Qcloud/tst-ptrace/simple_trace", "/home/hypermoon/Qcloud/tst-ptrace/simple_trace", NULL);
        if (ret < 0) {
            fprintf(stderr, "child process did not exec correctly, errno: %d\n", errno);
            exit(ret);
        }
    }

    /* catching stopped signals from child */
    siginfo_t sig;
    int status;
    /* first, filter SIGSTOP */
    waitpid(child, &status, 0);
    ptrace(PTRACE_SETOPTIONS, child, 0, PTRACE_O_TRACESYSGOOD);
    ptrace(PTRACE_CONT, child, 0, 0);

    while (1) {
        /* then, deal with actual code in the binary */
        waitpid(child, &status, 0);
        ptrace(PTRACE_GETSIGINFO, child, NULL, &sig);
        struct user_regs_struct regs;
        ptrace(PTRACE_GETREGS, child, NULL, &regs);
        // printf("Child process ip: %p\n", (void *)(regs.rip));
        
        /* the child finally exits */
        if (WIFEXITED(status)) {
            break;
        }
        if (sig.si_signo == SIGTRAP) {
            /* During booting of dynamic linker, we will receive a SIGTRAP too.
             * With PTRACE_O_TRACESYSGOOD enabled, our trap can be distinguished from it.
             * TODO: find out the difference between the two
             */
            if (sig.si_code != 1 << 7) {
                printf("invalid si_code: %d, try again.\n", sig.si_code);
                /* TODO: use switch-case to avoid goto */
                goto cont;
            }
            /* SIGTRAP will set si_addr, too. The header does not mention it
             * upd: si_addr is never set to correct value according to my experiments...
             * only rip is reliable
             */
            /* move one byte ahead, since rip is pointing to the next instruction after int3 */
            void *addr = (void *)(regs.rip - 1);
            // struct user_regs_struct regs;
            // ptrace(PTRACE_GETREGS, child, NULL, &regs);
            // printf("Child process ip: %p\n", (void *)(regs.rip));
            printf("instruction that received the trap: %p\n", addr);
            /* glibc does not really allow peek to have the fourth argument, but instead via return value,
             * according to: https://man7.org/linux/man-pages/man2/ptrace.2.html#:~:text=C%20library/kernel%20differences
             */
            int ret = ptrace(PTRACE_PEEKTEXT, child, addr, NULL);
            /* restore the instruction. Take care of endianess */
            const long orig_insn = 0xc031c031c031c031;
            ptrace(PTRACE_POKEDATA, child, addr, (void *)orig_insn);
            ret = ptrace(PTRACE_PEEKTEXT, child, addr, NULL);
            /* move rip to correct value to try again */
            regs.rip -= 1;
            ptrace(PTRACE_SETREGS, child, 0, &regs);
            /* Everything should be fine now*/
        } else if (sig.si_signo == SIGSEGV) {
            printf("Address causing the segfault: %p\n", sig.si_addr);
        } else if (sig.si_signo == SIGSTOP) {
            printf("bypassing stop\n");
        }
        else {
            fprintf(stderr, "Wrong signal (%d) from the child!\n", sig.si_signo);
        }
        /* don't send the signal to child */
    cont:
        ptrace(PTRACE_CONT, child, 0, 0);
    }

    return 0;
}