// arch-independent pointer arithmetic. It's an .inc to quasidocument that
// it's for assembly, not c or c++. Public Domain.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef XAX
#ifdef ARCH_386
#define XAX "eax"
#define XBX "ebx"
#define XCX "ecx"
#define XDX "edx"
#define XSI "esi"
#define XDI "edi"
#define XSP "esp"
#define MOVX "movl"
#define LEAX "leal"
#define DECX "decl"
#define PUSHX "pushl"
#define POPX "popl"
#define CMPX "cmpl"
#define ADDX "addl"
#endif
#ifdef ARCH_AMD64
#define XAX "rax"
#define XBX "rbx"
#define XCX "rcx"
#define XDX "rdx"
#define XSI "rsi"
#define XDI "rdi"
#define XSP "rsp"
#define MOVX "movq"
#define LEAX "leaq"
#define DECX "decq"
#define PUSHX "pushq"
#define POPX "popq"
#define CMPX "cmpq"
#define ADDX "addq"
#endif
#endif
