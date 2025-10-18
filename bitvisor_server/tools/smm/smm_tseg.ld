CPUS = 6;
SECTIONS
{
	/* This is the actual SMM handler.
	 *
	 * We just put code, rodata, data and bss all in a row.
	 */

	.emptytable (.): {
		/* Assembler stub */
		*(.emptytable)
	}
	. = 0x6000;
	.handler : {
		/* Assembler stub */
		*(.handler)
	}

	/* Data used in early SMM TSEG handler. */
	. = 0x7000;
	.earlydata : {
		*(.earlydata)
	}
	/* We are using the TSEG interleaved to stuff the SMM handlers
	 * for all CPU cores in there. The jump table redirects the execution
	 * to the actual SMM handler
	 */
	
	. = 0x8000;
	.jumptable : {
		*(.jumptable)
	}
	
	. = 0x15000;
	.smm_c_handler : {
		_smm_c_handler_start = .;

		/* C code of the SMM handler */
		*(.text);
		*(.text.*);

		/* C read-only data of the SMM handler */
		. = ALIGN(32);
		*(.rodata)
		*(.rodata.*)
		*(.data.rel.ro.*)

		/* C read-write data of the SMM handler */
		 . = ALIGN(4);
		*(.data)

		/* C uninitialized data of the SMM handler */
		 . = ALIGN(4);
		*(.bss)
		/*early_serial_base = ABSOLUTE(0x40000000 + .); */ 
		*(.sbss)
		*(COMMON)
		 . = ALIGN(4);

		_smm_c_handler_end = .;
	}

	. = 0x30000;
	.eh_frame : {
		*(.eh_frame)
	}
	. = 0x40000;
	.pgtable : {
		*(.pgtable)
	}
	/* 16KB for the heap at 64KB */
	/*
	. = 0x80000;
	.heap : {
		_heap = ABSOLUTE(.);
		. = 0x1000000;
		_eheap = ABSOLUTE(.);
	}
	*/

	/DISCARD/ : {
		*(.comment)
		*(.note)
		*(.note.*)
	}
}
