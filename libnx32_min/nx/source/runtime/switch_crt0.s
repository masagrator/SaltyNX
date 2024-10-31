.section ".crt0","ax"
.global _start

_start:
    b startup
    .word __nx_mod0 - _start
    .ascii "HOMEBREW"

.org _start+0x80
startup:
    // save lr
    mov  r7, lr

    // get aslr base
    bl   0x88
    sub  r6, lr, #0x88

    // context ptr and main thread handle
    mov  r5, r0
    mov  r4, r1

    b bssclr_start

bssclr_start:
    mov  r12, r7
    mov  r11, r5
    mov  r10, r4

    // clear .bss
    ldr  r0, =__bss_start__
    ldr  r1, =__bss_end__
    sub  r1, r1, r0  // calculate size
    add  r1, r1, #7  // round up to 8
    bic  r1, r1, #7

bss_loop:
	mov  r2, #0
    str  r2, [r0]!
    subs r1, r1, #4
    bne  bss_loop

    // store stack pointer
    ldr  r0, =__stack_top
	str  sp, [r0]

    // initialize system
    mov  r0, r10
    mov  r1, r11
    mov  r2, r12
    blx   __libnx_init

    // call entrypoint
	ldr  r0, =__system_argc // argc
    ldr  r0, [r0]
    ldr  r1, =__system_argv // argv
    ldr  r1, [r1]
    ldr  lr, =exit
    b    main

.global __nx_exit
.type   __nx_exit, %function
__nx_exit:
    // restore stack pointer
    ldr  r8, =__stack_top
	ldr  sp, [r8]

    // jump back to loader
    bx   r1

.global __nx_mod0
__nx_mod0:
    .ascii "MOD0"
    .word  _DYNAMIC             - __nx_mod0
    .word  __bss_start__        - __nx_mod0
    .word  __bss_end__          - __nx_mod0
    .word  __eh_frame_hdr_start - __nx_mod0
    .word  __eh_frame_hdr_end   - __nx_mod0
    .word  0 // "offset to runtime-generated module object" (neither needed, used nor supported in homebrew)

    // MOD0 extensions for homebrew
    .ascii "LNY0"
    .word  __got_start__        - __nx_mod0
    .word  __got_end__          - __nx_mod0
