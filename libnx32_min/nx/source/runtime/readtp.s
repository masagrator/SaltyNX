	.section .text.__aeabi_read_tp, "ax", %progbits
	.global __aeabi_read_tp
	.type __aeabi_read_tp, %function
	.align 2
	.cfi_startproc
__aeabi_read_tp:
	mrc p15, 0, r0, c13, c0, 3
	bx lr
	.cfi_endproc
