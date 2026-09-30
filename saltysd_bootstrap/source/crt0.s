.section ".crt0","ax"
.global _start

_start:
    b startup
    .word 0

.org _start+0x8

#ifdef __aarch64__
startup:

    // save lr
    mov  x27, x30

    // get aslr base
    bl   +4
    sub  x28, x30, #0x88

    // context ptr and main thread handle
    mov  x25, x0
    mov  x26, x1

    // clear .bss
    ldr x0, =__bss_start__
    ldr x1, =__bss_end__
    sub  x1, x1, x0  // calculate size
    add  x1, x1, #7  // round up to 8
    bic  x1, x1, #7

bss_loop: 
    str  xzr, [x0], #8
    subs x1, x1, #8
    bne  bss_loop

    // store stack pointer
    mov  x1, sp
    ldr x0, =__stack_top
    str  x1, [x0]

    // initialize system
    mov  x0, x25
    mov  x1, x26
    mov  x2, x27
    bl   __rel_init

    // call entrypoint
    ldr x0, =__system_argc // argc
    ldr  w0, [x0]
    ldr x1, =__system_argv // argv
    ldr  x1, [x1]
    ldr x30, =__rel_exit
    b    main

.global __nx_exit
.type   __nx_exit, %function
__nx_exit:
    // restore stack pointer
    ldr x8, =__stack_top
    ldr  x8, [x8]
    mov  sp, x8

    // jump back to loader
    br   x2
#elif __arm__

// Pure code: no data segment, no relocations, nothing PC-relative outside this segment. The variables live
// in a BootState block (bootstate.h) on the stack, r9 points to it for the whole run.
.equ BOOTSTATE_SIZE, 64        // bootstate.h
.equ BS_STACK_TOP,   0
.equ BS_ARGC,        4
.equ BS_ARGV,        8

startup:
    // save lr
    mov  r7, lr

    // context ptr and main thread handle
    mov  r5, r0
    mov  r4, r1

    // reserve and zero the state block
    mov  r6, sp
    sub  sp, sp, #BOOTSTATE_SIZE
    mov  r9, sp
    mov  r0, #0
    mov  r1, #0
state_clear:
    str  r0, [r9, r1]
    add  r1, r1, #4
    cmp  r1, #BOOTSTATE_SIZE
    blo  state_clear

    // store stack pointer (the one we were entered with)
    str  r6, [r9, #BS_STACK_TOP]

    // initialize system (same argument order as before)
    mov  r0, r4
    mov  r1, r5
    mov  r2, r7
    bl   __rel_init

    // call entrypoint
    ldr  r0, [r9, #BS_ARGC]
    ldr  r1, [r9, #BS_ARGV]
    adr  lr, .Lrel_exit_thunk
    b    main

.Lrel_exit_thunk:
    b    __rel_exit

.global __nx_exit
.type   __nx_exit, %function
__nx_exit:
    // restore stack pointer
    ldr  sp, [r9, #BS_STACK_TOP]

    // jump back to loader
    bx   r2
#endif
.pool