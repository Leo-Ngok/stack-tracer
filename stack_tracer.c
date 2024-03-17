#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <unwind.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
extern char **environ;
// endbr64
uint8_t fn_entry[] = {0xf3, 0x0f, 0x1e, 0xfa};
// save callee-saved registers.
// %rsp	Stack pointer, caller-owned	%esp	%sp	%spl
// %rbx	Local variable, caller-owned	%ebx	%bx	%bl
// %rbp	Local variable, caller-owned	%ebp	%bp	%bpl
// %r12	Local variable, caller-owned	%r12d	%r12w	%r12b
// %r13	Local variable, caller-owned	%r13d	%r13w	%r13b
// %r14	Local variable, caller-owned	%r14d	%r14w	%r14b
// %r15	Local variable, caller-owned	%r15d	%r15w	%r15b
uint8_t push_sp[] = {0x54};
uint8_t push_bx[] = {0x53};
uint8_t push_bp[] = {0x55};
uint8_t push_12[] = {0x41, 0x54};
uint8_t push_13[] = {0x41, 0x55};
uint8_t push_14[] = {0x41, 0x56};
uint8_t push_15[] = {0x41, 0x57};

// int __libc_start_main(int (*main) (int, char * *, char * *),
// int argc, char * * ubp_av, void (*init) (void), void (*fini) (void),
// void (*rtld_fini) (void), void (* stack_end));
// 0000000000001210 <_start>:
//     1210:	f3 0f 1e fa          	endbr64
//     1214:	31 ed                	xor    %ebp,%ebp
//     1216:	49 89 d1             	mov    %rdx,%r9          (set #6 rtld+fini)
//     1219:	5e                     	pop    %rsi              (set #2 argc = SI)
//     121a:	48 89 e2             	mov    %rsp,%rdx         (set #3 ubp_av = SP)
//     121d:	48 83 e4 f0          	and    $0xfffffffffffffff0,%rsp
//     1221:	50                   	push   %rax
//     1222:	54                   	push   %rsp
//     1223:	4c 8d 05 06 06 00 00 	lea    0x606(%rip),%r8        # 1830 <__libc_csu_fini> (set #5 = fini)
//     122a:	48 8d 0d 8f 05 00 00 	lea    0x58f(%rip),%rcx        # 17c0 <__libc_csu_init> (set #4 init = libc_csu_init)
//     1231:	48 8d 3d 48 ff ff ff 	lea    -0xb8(%rip),%rdi        # 1180 <main>
//     1238:	ff 15 a2 2d 00 00    	callq  *0x2da2(%rip)        # 3fe0 <__libc_start_main@GLIBC_2.2.5>
//     123e:	f4                   	hlt
//     123f:	90                   	nop
// you shall not see this "prolog" / "epilog" functions, since they are done / not being called yet.
// libc_start_main calls main, so it is _start -> libc_start_main -> main -> ...
struct saved_ctx
{
    char name[8];
    uint64_t val;
    void *pos;
    struct saved_ctx *next;
};
struct fn_stack
{
    void *sp;
    void *bp;
    void *entry;
    void *retn;
    char *msg;
    struct saved_ctx *regs;
};
void print_stackframe(int stack_cnt)
{
    // warning.
    fprintf(stderr, "Be warned that do not use this fn in production environment, or else stack contents would be leaked and security would be compromised.\n");

    // step 1: trace all return addresses.
    void **__buffer = malloc(stack_cnt * sizeof(void *));
    int nptrs = backtrace(__buffer, stack_cnt);
    // malloc.
    char **strings = backtrace_symbols(__buffer, nptrs);
    // printf("backtrace() returned %d addresses\n", nptrs);

    // note that stack_ctx[0] is context for this function.
    struct fn_stack *stack_ctx = malloc(nptrs * sizeof(struct fn_stack));
    for (int ii = 0; ii < nptrs; ++ii)
    {
        stack_ctx[ii].retn = (void *)__buffer[ii];
        stack_ctx[ii].msg = strings[ii];
        // printf("Return address = %p, prompt = %s\n", stack_ctx[ii].retn, stack_ctx[ii].msg);
        stack_ctx[ii].regs = NULL;
    }
    free(__buffer);
    free(strings);

    // step 2. record sp and bp.
    void *rsp;
    asm volatile("mov %%rsp, %0" : "=r"(rsp));
    uint64_t off = 0;
    int jj = 1;
    char *curr;
    // printf("SP = %p\n", rsp);
    // not supposed to derive base pointer for _start.
    stack_ctx[nptrs - 1].bp = NULL;
    while ((curr = rsp + off) != (char *)environ)
    {
        unsigned long long val = *((unsigned long long *)curr);
        void *fnptr = (void *)val;
        // printf("%p = %016llx\n", curr, val);
        if (jj < nptrs && fnptr == stack_ctx[jj].retn)
        {
            stack_ctx[jj].sp = (void *)((uint64_t)curr + 8);
            stack_ctx[jj - 1].bp = (void *)((uint64_t)curr);
            // printf("BP=%p\n", curr);
            jj++;
        }
        off += 8;
    }
    for (int ii = 1; ii < nptrs; ++ii)
    {
        char *ins = stack_ctx[ii].retn;
        while (memcmp(ins, fn_entry, sizeof(fn_entry)) != 0)
        {
            ins--;
        }
        // printf("Fn entry is at %p\n", ins);
        stack_ctx[ii].entry = (void *)ins;
        // we do not record for saved registers of _start
        if (ii + 1 == nptrs)
            break;
        int off = -8; // keep in mind that bp points to return address here.
        for (; ins != stack_ctx[ii].retn; ins++)
        {
            uint64_t *loc = (uint64_t *)((char *)stack_ctx[ii].bp + off);
            if (memcmp(ins, push_sp, sizeof(push_sp)) == 0)
            {
                // ins is push sp instruction.
                // printf("Push SP\n");
                struct saved_ctx *reg = malloc(sizeof(struct saved_ctx));
                strcpy(reg->name, "SP");
                reg->val = *loc;
                reg->pos = loc;
                reg->next = stack_ctx[ii].regs;
                stack_ctx[ii].regs = reg;
                off -= 8;
            }
            else if (memcmp(ins, push_bx, sizeof(push_bx)) == 0)
            {
                struct saved_ctx *reg = malloc(sizeof(struct saved_ctx));
                strcpy(reg->name, "BX");
                reg->val = *loc;
                reg->pos = loc;
                reg->next = stack_ctx[ii].regs;
                stack_ctx[ii].regs = reg;
                off -= 8;
            }
            else if (memcmp(ins, push_bp, sizeof(push_bp)) == 0)
            {
                struct saved_ctx *reg = malloc(sizeof(struct saved_ctx));
                strcpy(reg->name, "BP");
                reg->val = *loc;
                reg->pos = loc;
                reg->next = stack_ctx[ii].regs;
                stack_ctx[ii].regs = reg;
                off -= 8;
            }
            else if (memcmp(ins, push_12, sizeof(push_12)) == 0)
            {
                struct saved_ctx *reg = malloc(sizeof(struct saved_ctx));
                strcpy(reg->name, "R12");
                reg->val = *loc;
                reg->pos = loc;
                reg->next = stack_ctx[ii].regs;
                stack_ctx[ii].regs = reg;
                off -= 8;
            }
            else if (memcmp(ins, push_13, sizeof(push_13)) == 0)
            {
                // printf("Push 13\n");
                struct saved_ctx *reg = malloc(sizeof(struct saved_ctx));
                strcpy(reg->name, "R13");
                reg->val = *loc;
                reg->pos = loc;
                reg->next = stack_ctx[ii].regs;
                stack_ctx[ii].regs = reg;
                off -= 8;
            }
            else if (memcmp(ins, push_14, sizeof(push_14)) == 0)
            {
                // printf("Push 14\n");
                struct saved_ctx *reg = malloc(sizeof(struct saved_ctx));
                strcpy(reg->name, "R14");
                reg->val = *loc;
                reg->pos = loc;
                reg->next = stack_ctx[ii].regs;
                stack_ctx[ii].regs = reg;
                off -= 8;
            }
            else if (memcmp(ins, push_15, sizeof(push_15)) == 0)
            {
                // printf("Push 15\n");
                struct saved_ctx *reg = malloc(sizeof(struct saved_ctx));
                strcpy(reg->name, "R15");
                reg->val = *loc;
                reg->pos = loc;
                reg->next = stack_ctx[ii].regs;
                stack_ctx[ii].regs = reg;
                off -= 8;
            }
        }
    }
    for (int ii = 1; ii < nptrs; ++ii)
    {
        printf("+-----------------------------\n");
        printf("| Entry: %p\n", stack_ctx[ii].entry);
        printf("| Address to return: %p\n", stack_ctx[ii].retn);
        printf("| Stack pointer: %p\n", stack_ctx[ii].sp);
        struct saved_ctx *reg = stack_ctx[ii].regs;
        while (reg != NULL)
        {
            printf("| --> Saved register: [%s] (at %p)= 0x%lx\n", reg->name, reg->pos, reg->val);
            reg = reg->next;
        }
        printf("| Base pointer: %p\n", stack_ctx[ii].bp);
        // this is actually address to return of the adjacent entry.
        // printf("| Dest of retn: %p\n", (void *)*(void **)(stack_ctx[ii].bp));
        printf("| Description: %s\n", stack_ctx[ii].msg);
        printf("+-----------------------------\n");
    }
    // TODO: Recycle garbage.
}

// credit:
// https://zhuanlan.zhihu.com/p/658352901
// https://codebrowser.dev/glibc/glibc/debug/backtrace.c.html
// https://zhuanlan.zhihu.com/p/507138494
// https://www.cnblogs.com/mickole/p/3246702.html

// TODO: Extend implementation that
// parses DWARF contents in .eh_frame and
// search fn address in .eh_frame_hdr
//
// Implement CIE and FDE parsers.
// These entries holds how stack pointers
// move, and where do callee-saved parameter
// saves.

// Note: It is not that possible to
// read function parameters, unless
// you want to implement a disassembler entirely :)
// since input arguments are put in DI, SI, DX, CX, R8, R9
// You are not able to trace these registers
// very easily.

// Reference for doing a DWARF parser.
// https://wiki.osdev.org/DWARF
//