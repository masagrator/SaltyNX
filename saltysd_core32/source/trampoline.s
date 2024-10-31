.global elf_trampoline
.type   elf_trampoline, %function
elf_trampoline:
    sub sp, sp, #0x40
	push {lr}
	push {r0-r12}

    blx r2
	
	pop {r0-r12}
	pop {lr}
	add sp, sp, #0x40

    bx lr
