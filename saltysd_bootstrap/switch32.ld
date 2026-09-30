OUTPUT_ARCH(arm)
ENTRY(_start)

/* Bootstrap32: one segment of pure code (see source/bootstate.h). It's written over rtld's code only, so
   the same binary works for every rtld layout (code-to-data distance 0x3000, 0x5000, ...). */
PHDRS
{
	code   PT_LOAD FLAGS(5) /* Read | Execute */;
}

SECTIONS
{
	PROVIDE(__start__ = 0x0);
	. = __start__;

	.text :
	{
		KEEP (*(.crt0))
		. = ALIGN(8);
		*(.text.unlikely .text.*_unlikely .text.unlikely.*)
		*(.text.exit .text.exit.*)
		*(.text.startup .text.startup.*)
		*(.text.hot .text.hot.*)
		*(.text .stub .text.* .gnu.linkonce.t.*)
		. = ALIGN(8);
	} :code

	.rodata :
	{
		*(.rodata .rodata.* .gnu.linkonce.r.*)
		. = ALIGN(8);
	} :code

	.note.gnu.build-id : { *(.note.gnu.build-id) } :code

	/* The 12 byte header -pie always creates. Unused: no PLT calls, no relocations, never written. */
	.got.plt : { *(.got.plt .igot.plt) } :code

	/* No writable data allowed: variables go in BootState (source/bootstate.h). */
	.data :
	{
		*(.data .data.* .gnu.linkonce.d.*)
		*(.bss .bss.* .gnu.linkonce.b.* COMMON .dynbss)
		*(.got .igot)
		*(.tdata .tdata.* .tbss .tbss.*)
		*(.init_array* .fini_array* .preinit_array* .ctors* .dtors*)
	} :code
	ASSERT(SIZEOF(.data) == 0, "Bootstrap32 must not have writable data: put variables in BootState (source/bootstate.h)")

	/* Linked -pie so anything position dependent shows up here instead of being silently resolved. */
	.rel.dyn : { *(.rel.*) } :code
	ASSERT(SIZEOF(.rel.dyn) == 0, "Bootstrap32 must not need relocations: the proc copies it as is")

	/* Discard sections that difficult post-processing */
	/DISCARD/ : { *(.group .comment .note .eh_frame .eh_frame_hdr .ARM.exidx* .ARM.extab* .interp .dynamic .dynsym .dynstr .hash .gnu.hash) }

	/* Stabs debugging sections. */
	.stab          0 : { *(.stab) }
	.stabstr       0 : { *(.stabstr) }
	.stab.excl     0 : { *(.stab.excl) }
	.stab.exclstr  0 : { *(.stab.exclstr) }
	.stab.index    0 : { *(.stab.index) }
	.stab.indexstr 0 : { *(.stab.indexstr) }

	/* DWARF debug sections.
	   Symbols in the DWARF debugging sections are relative to the beginning
	   of the section so we begin them at 0. */

	/* DWARF 1 */
	.debug          0 : { *(.debug) }
	.line           0 : { *(.line) }

	/* GNU DWARF 1 extensions */
	.debug_srcinfo  0 : { *(.debug_srcinfo) }
	.debug_sfnames  0 : { *(.debug_sfnames) }

	/* DWARF 1.1 and DWARF 2 */
	.debug_aranges  0 : { *(.debug_aranges) }
	.debug_pubnames 0 : { *(.debug_pubnames) }

	/* DWARF 2 */
	.debug_info     0 : { *(.debug_info) }
	.debug_abbrev   0 : { *(.debug_abbrev) }
	.debug_line     0 : { *(.debug_line) }
	.debug_frame    0 : { *(.debug_frame) }
	.debug_str      0 : { *(.debug_str) }
	.debug_loc      0 : { *(.debug_loc) }
	.debug_macinfo  0 : { *(.debug_macinfo) }
}
