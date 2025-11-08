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

startup:
    // save lr
    mov  r7, lr

    // context ptr and main thread handle
    mov  r5, r0
    mov  r4, r1

    b bssclr_start

bssclr_start:
    mov r12, r7
    mov r11, r5
    mov r10, r4

    // clear .bss
    ldr r0, =__bss_start__
    ldr r1, =__bss_end__
    sub  r1, r1, r0  // calculate size
    add  r1, r1, #7  // round up to 8
    bic  r1, r1, #7

bss_loop:
	mov r2, #0
    str  r2, [r0], #4
    subs r1, r1, #4
    bne  bss_loop

    // store stack pointer
    ldr  r0, =__stack_top
	str  sp, [r0]

    // initialize system
    mov  r0, r10
    mov  r1, r11
    mov  r2, r12
    blx  __rel_init

    // call entrypoint
	ldr r0, =__system_argc // argc
    ldr  r0, [r0]
    ldr r1, =__system_argv // argv
    ldr  r1, [r1]
    ldr lr, =__rel_exit
    b    main

.global __nx_exit
.type   __nx_exit, %function
__nx_exit:
    // restore stack pointer
    ldr  r8, =__stack_top
	ldr  sp, [r8]

    // jump back to loader
    bx   r2
#endif
.pool