.section ".crt0","ax"
.global _start

_start:
    b entrypoint
    .word __nx_mod0 - _start

entrypoint:
	b startup
    .ascii "SALTYNX"

// Position independent: every address is taken PC-relative (directly or through the MOD0 offsets),
// so this code needs no relocation itself. Nothing may use a relocated pointer before __nx_dynamic.
.org _start+0x80
startup:
    mov  r7, lr            // loader return address
    mov  r5, r0            // context ptr
    mov  r4, r1            // main thread handle

    // relocate ourselves: __nx_dynamic(base, _DYNAMIC)
    adr  r0, _start
    adr  r8, __nx_mod0
    ldr  r1, [r8, #4]      // MOD0: _DYNAMIC - __nx_mod0
    add  r1, r8, r1
    bl   __nx_dynamic

    // clear .bss
    ldr  r0, [r8, #8]      // MOD0: __bss_start__ - __nx_mod0
    add  r0, r8, r0
    ldr  r1, [r8, #12]     // MOD0: __bss_end__ - __nx_mod0
    add  r1, r8, r1
    mov  r2, #0
bss_loop:
    cmp  r0, r1
    strlo r2, [r0], #4
    blo  bss_loop

    // store stack pointer
    ldr  r0, .Lstack_top
.Lstack_top_pc:
    add  r0, pc, r0
    str  sp, [r0]

    // initialize system (same argument order as before)
    mov  r0, r4
    mov  r1, r5
    mov  r2, r7
    bl   __libnx_init

    // call entrypoint
    ldr  r0, .Lsystem_argc
.Lsystem_argc_pc:
    add  r0, pc, r0
    ldr  r0, [r0]          // argc
    ldr  r1, .Lsystem_argv
.Lsystem_argv_pc:
    add  r1, pc, r1
    ldr  r1, [r1]          // argv
    adr  lr, .Lexit_thunk
    b    main

.Lexit_thunk:
    b    exit

.global __nx_exit
.type   __nx_exit, %function
__nx_exit:
    // restore stack pointer
    ldr  r8, .Lstack_top_exit
.Lstack_top_exit_pc:
    add  r8, pc, r8
    ldr  sp, [r8]

    // jump back to loader
    bx   r1

// PC-relative offsets (pc reads as the instruction address + 8), resolved by the linker, not at runtime.
.Lstack_top:      .word __stack_top   - (.Lstack_top_pc + 8)
.Lsystem_argc:    .word __system_argc - (.Lsystem_argc_pc + 8)
.Lsystem_argv:    .word __system_argv - (.Lsystem_argv_pc + 8)
.Lstack_top_exit: .word __stack_top   - (.Lstack_top_exit_pc + 8)

.align 2
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
