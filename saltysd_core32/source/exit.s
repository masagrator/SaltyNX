.global __nx_exit_clear
.type   __nx_exit_clear, %function
__nx_exit_clear:
    // restore stack pointer
    ldr  r8, =__stack_top
    ldr  sp, [r8]
    
    mov lr, r2
    
    mov r2, #0
    mov r3, #0
    mov r4, #0
    mov r5, #0
    mov r6, #0
    mov r7, #0
    mov r8, #0
    mov r9, #0
    mov r10, #0
    mov r11, #0
    mov r12, #0

    // jump back to loader
    bx lr
