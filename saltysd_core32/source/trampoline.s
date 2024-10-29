.global elf_trampoline
.type   elf_trampoline, %function
elf_trampoline:
    sub sp, sp, #0x40
	push {lr}
	push {r12}
	push {r11}
	push {r10}
	push {r9}
	push {r8}
	push {r7}
	push {r6}
	push {r5}
	push {r4}
	push {r3}
	push {r2}
	push {r1}
	push {r0}

    blx r2
	
	pop {r0}
	pop {r1}
	pop {r2}
	pop {r3}
	pop {r4}
	pop {r5}
	pop {r6}
	pop {r7}
	pop {r8}
	pop {r9}
	pop {r10}
	pop {r11}
	pop {r12}
	pop {lr}
	add sp, sp, #0x40

    bx lr
