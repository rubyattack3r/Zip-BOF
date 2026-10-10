/*
 * Stack probe routines for Windows builds that do not link a C runtime.
 *
 * Frames larger than a page must touch each page in order so the stack guard
 * page can grow the stack. The compiler calls these routines with the frame
 * size in eax/rax.
 */
#if defined(__x86_64__)
/* Probes from the caller's stack pointer down by rax; preserves registers. */
__asm__(
    ".text\n"
    ".globl ___chkstk_ms\n"
    "___chkstk_ms:\n"
    "    pushq %rcx\n"
    "    pushq %rax\n"
    "    leaq 24(%rsp), %rcx\n"
    "    cmpq $0x1000, %rax\n"
    "    jb 2f\n"
    "1:  subq $0x1000, %rcx\n"
    "    testq %rcx, (%rcx)\n"
    "    subq $0x1000, %rax\n"
    "    cmpq $0x1000, %rax\n"
    "    ja 1b\n"
    "2:  subq %rax, %rcx\n"
    "    testq %rcx, (%rcx)\n"
    "    popq %rax\n"
    "    popq %rcx\n"
    "    ret\n");
#elif defined(__i386__)
/* MinGW _alloca: probes like ___chkstk_ms, then also lowers esp by eax.
 * Preserves ecx and edx; the compiler saves eax around the call if needed. */
__asm__(
    ".text\n"
    ".globl __alloca\n"
    "__alloca:\n"
    "    pushl %ecx\n"
    "    leal 8(%esp), %ecx\n"
    "    cmpl $0x1000, %eax\n"
    "    jb 2f\n"
    "1:  subl $0x1000, %ecx\n"
    "    testl %ecx, (%ecx)\n"
    "    subl $0x1000, %eax\n"
    "    cmpl $0x1000, %eax\n"
    "    ja 1b\n"
    "2:  subl %eax, %ecx\n"
    "    testl %ecx, (%ecx)\n"
    "    movl %esp, %eax\n"
    "    movl %ecx, %esp\n"
    "    movl (%eax), %ecx\n"
    "    pushl 4(%eax)\n"
    "    ret\n");
#endif
