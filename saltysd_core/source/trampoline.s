.global elf_trampoline
.type   elf_trampoline, %function
elf_trampoline:
#if defined(__arm__)
    sub sp, sp, #0x40
	push {lr}
	push {r0-r12}

    blx r2
	
	pop {r0-r12}
	pop {lr}
	add sp, sp, #0x40

    bx lr
#elif defined(__aarch64__)
    sub sp, sp, #0x100
    stp x29, x30, [sp, #0x0]
    stp x27, x28, [sp, #0x10]
    stp x25, x26, [sp, #0x20]
    stp x23, x24, [sp, #0x30]
    stp x21, x22, [sp, #0x40]
    stp x19, x20, [sp, #0x50]
    stp x17, x18, [sp, #0x60]
    stp x15, x16, [sp, #0x70]
    stp x13, x14, [sp, #0x80]
    stp x11, x12, [sp, #0x90]
    stp x9, x10, [sp, #0xA0]
    stp x7, x8, [sp, #0xB0]
    stp x5, x6, [sp, #0xC0]
    stp x3, x4, [sp, #0xD0]
    stp x1, x2, [sp, #0xE0]
    str x0, [sp, #0xF0]

    blr x2

    ldr x0, [sp, #0xF0]
    ldp x1, x2, [sp, #0xE0]
    ldp x3, x4, [sp, #0xD0]
    ldp x5, x6, [sp, #0xC0]
    ldp x7, x8, [sp, #0xB0]
    ldp x9, x10, [sp, #0xA0]
    ldp x11, x12, [sp, #0x90]
    ldp x13, x14, [sp, #0x80]
    ldp x15, x16, [sp, #0x70]
    ldp x17, x18, [sp, #0x60]
    ldp x19, x20, [sp, #0x50]
    ldp x21, x22, [sp, #0x40]
    ldp x23, x24, [sp, #0x30]
    ldp x25, x26, [sp, #0x20]
    ldp x27, x28, [sp, #0x10]
    ldp x29, x30, [sp, #0x0]
    add sp, sp, #0x100
    ret
#endif