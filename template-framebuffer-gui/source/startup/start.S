.equ	USR_MODE,		0x10	/* normal user mode */
.equ 	FIQ_MODE,		0x11	/* fiq fast interrupts mode */
.equ 	IRQ_MODE,		0x12	/* irq standard interrupts mode */
.equ 	SVC_MODE,		0x13	/* supervisor interrupts mode */
.equ 	ABT_MODE,		0x17	/* abort processing memory faults mode */
.equ 	UDF_MODE,		0x1b	/* undefined instructions mode */
.equ 	MODE_MASK,		0x1f	/* system running in priviledged operating mode */
.equ 	NO_INT,			0xc0

.equ	MMU_SECT_SDRAM_START,	0x300
.equ	MMU_SECT_SDRAM_END,		0x4ff
.equ	MMU_SECT_FB_START,		0x3e0
.equ	MMU_SECT_FB_END,		0x3e5
.equ	MMU_SECT_MMIO_START,	0xe00
.equ	MMU_SECT_MMIO_END,		0xfff

/* First-level section attributes, kept aligned with the known-good S5PV210 style
 * used in the reference code the project is following. */
.equ	MMU_DESC_SDRAM_CACHED,	0x00001c0e	/* RW, executable WB/WA cached section */
.equ	MMU_DESC_FB_NOCACHE,	0x00001c12	/* RW, executable-never normal non-cacheable section */
.equ	MMU_DESC_MMIO_DEVICE,	0x00000c12	/* RW, uncached/device section */

/*
 * exception vector table
 */
.text
	.arm

	.global	_start
_start:

/* 0x00: reset */
	b	reset

/* 0x04: undefined instruction exception */
	ldr	pc, _undefined_instruction

/* 0x08: software interrupt exception */
	ldr	pc, _software_interrupt

/* 0x0c: prefetch abort */
	ldr	pc, _prefetch_abort

/* 0x10: data access memory abort */
	ldr	pc, _data_abort

/* 0x14: not used */
	ldr	pc, _not_used

/* 0x18: interrupt request exception */
	ldr	pc, _irq

/* 0x1c: fast interrupt request exception */
	ldr	pc, _fiq


_undefined_instruction:
	.long undefined_instruction
_software_interrupt:
	.long software_interrupt
_prefetch_abort:
	.long prefetch_abort
_data_abort:
	.long data_abort
_not_used:
	.long not_used
_irq:
	.long irq
_fiq:
	.long fiq

/*
 * actual reset code
 */
reset:
	/* set the cpu to supervisor mode */
	mrs	r0, cpsr
	bic	r0, r0, #0x1f
	orr	r0, r0, #0xd3
	msr	cpsr, r0

	/* Configure CP15 SCTLR to allow unaligned memory accesses.
	 * Cortex-A8 defaults to strict alignment checking. GCC's optimized
	 * lv_memzero may generate instructions that access 64-bit (STRD) or
	 * 128-bit (NEON) quantities at non-64-bit-aligned addresses.
	 * Clearing bit 1 (A-bit) disables alignment fault checking.
	 * NOTE: Bit 22 (U-bit) behavior varies; we clear A-bit only. */
	mrc	p15, 0, r0, c1, c0, 0
	bic	r0, r0, #(1 << 1)   /* Clear A-bit: disable strict alignment check */
	mcr	p15, 0, r0, c1, c0, 0

	/* initialize stacks */
	bl	init_stacks

	/* Enforce 8-byte stack alignment for AAPCS compliance.
	 * GCC may generate STM/NEON instructions in lv_memzero that require
	 * 8-byte aligned stack pointer. Without this, Cortex-A8 triggers
	 * Alignment Fault (Data Abort) on STMDB/STMDA operations. */
	bic	sp, sp, #7

	/* Set VBAR (Vector Base Address Register) to redirect exceptions
	 * to our vector table at _start instead of iROM default (0x00000000).
	 * Without this, any exception falls into iROM's silent deadloop. */
	ldr	r0, =_start
	mcr	p15, 0, r0, c12, c0, 0

	/* clear bss section */
clear_bss:
	ldr	r0, _bss_start
	ldr	r1, _bss_end
	mov r2, #0x00000000
	bl	mem_clear

	/* build first-level page table and enable MMU/cache */
	bl	mmu_init

	/* jump to main fuction */
	mov r0, #0;
	mov r1, #0;
	bl	main			/* call main function */
	b	.

/*
 * initialize stacks
 */
init_stacks:
	mrs	r0, cpsr
	bic	r0, r0, #MODE_MASK | NO_INT
	orr	r1, r0, #UDF_MODE
	msr	cpsr_cxsf, r1
	ldr	sp, _stack_und_end
	bic	sp, sp, #7		/* Enforce 8-byte alignment */

	bic	r0, r0, #MODE_MASK | NO_INT
	orr	r1, r0, #ABT_MODE
	msr	cpsr_cxsf, r1
	ldr	sp, _stack_abt_end
	bic	sp, sp, #7		/* Enforce 8-byte alignment */

	bic	r0, r0, #MODE_MASK | NO_INT
	orr	r1, r0, #IRQ_MODE
	msr	cpsr_cxsf, r1
	ldr	sp, _stack_irq_end
	bic	sp, sp, #7		/* Enforce 8-byte alignment */

	bic	r0, r0, #MODE_MASK | NO_INT
	orr	r1, r0, #FIQ_MODE
	msr	cpsr_cxsf, r1
	ldr	sp, _stack_fiq_end
	bic	sp, sp, #7		/* Enforce 8-byte alignment */

	bic	r0, r0, #MODE_MASK | NO_INT
	orr	r1, r0, #SVC_MODE
	msr	cpsr_cxsf, r1
	ldr	sp, _stack_srv_end
	bic	sp, sp, #7		/* Enforce 8-byte alignment */
	mov	pc, lr

/*
 * map a contiguous range of 1MB sections
 *
 * r0: L1 table base
 * r1: start section index
 * r2: end section index (inclusive)
 * r3: section descriptor attributes
 */
map_sections:
	add	r4, r0, r1, lsl #2
1:
	cmp	r1, r2
	bgt	2f
	mov	r5, r1, lsl #20
	orr	r5, r5, r3
	str	r5, [r4], #4
	add	r1, r1, #1
	b	1b
2:
	mov	pc, lr

/*
 * invalidate L1 data cache by set/way using CCSIDR
 */
invalidate_dcache_all:
	mov	r0, #0
	mcr	p15, 2, r0, c0, c0, 0		/* CSSELR: L1 data/unified cache */
	isb

	mrc	p15, 1, r1, c0, c0, 0		/* CCSIDR */
	and	r2, r1, #7
	add	r2, r2, #4				/* log2(line_len_bytes) */

	ldr	r3, =0x3ff
	and	r3, r3, r1, lsr #3		/* associativity - 1 */
	clz	r4, r3					/* way shift */

	ldr	r5, =0x7fff
	and	r5, r5, r1, lsr #13		/* number of sets - 1 */
3:
	mov	r6, r3
4:
	mov	r7, r6, lsl r4
	orr	r7, r7, r5, lsl r2
	mcr	p15, 0, r7, c7, c6, 2		/* DCISW */
	subs	r6, r6, #1
	bpl	4b
	subs	r5, r5, #1
	bpl	3b

	dsb
	mov	pc, lr

/*
 * build a flat section page table, keeping SDRAM cached, framebuffer uncached
 * and peripheral MMIO strongly ordered
 */
mmu_init:
	stmdb	sp!, {lr}

	/* start from a known CP15 state in case U-Boot left caches enabled */
	mrc	p15, 0, r0, c1, c0, 0
	bic	r0, r0, #(1 << 0)		/* M */
	bic	r0, r0, #(1 << 2)		/* C */
	bic	r0, r0, #(1 << 11)		/* Z */
	bic	r0, r0, #(1 << 12)		/* I */
	bic	r0, r0, #(1 << 1)		/* A = 0, keep unaligned access enabled */
	mcr	p15, 0, r0, c1, c0, 0
	dsb
	isb

	/* clear the 16KB L1 table */
	ldr	r0, =__mmu_table_start
	ldr	r1, =__mmu_table_end
	mov	r2, #0
	bl	mem_clear

	/* 0x30000000 - 0x4fffffff: normal cached SDRAM */
	ldr	r0, =__mmu_table_start
	ldr	r1, =MMU_SECT_SDRAM_START
	ldr	r2, =MMU_SECT_SDRAM_END
	ldr	r3, =MMU_DESC_SDRAM_CACHED
	bl	map_sections

	/* 0x3e000000 - 0x3e5fffff: framebuffer window as non-cacheable normal memory */
	ldr	r0, =__mmu_table_start
	ldr	r1, =MMU_SECT_FB_START
	ldr	r2, =MMU_SECT_FB_END
	ldr	r3, =MMU_DESC_FB_NOCACHE
	bl	map_sections

	/* 0xe0000000 - 0xffffffff: peripheral registers as strongly ordered/device memory */
	ldr	r0, =__mmu_table_start
	ldr	r1, =MMU_SECT_MMIO_START
	ldr	r2, =MMU_SECT_MMIO_END
	ldr	r3, =MMU_DESC_MMIO_DEVICE
	bl	map_sections

	dsb

	/* TTBR0 only, short-descriptor format */
	ldr	r0, =__mmu_table_start
	mcr	p15, 0, r0, c2, c0, 0		/* TTBR0 */
	mov	r0, #0
	mcr	p15, 0, r0, c2, c0, 2		/* TTBCR = 0 */

	ldr	r0, =0xffffffff
	mcr	p15, 0, r0, c3, c0, 0		/* DACR: manager access */

	mov	r0, #0
	mcr	p15, 0, r0, c7, c5, 0		/* invalidate I-cache */
	mcr	p15, 0, r0, c7, c5, 6		/* invalidate branch predictor array */
	mcr	p15, 0, r0, c8, c7, 0		/* invalidate unified TLB */
	bl	invalidate_dcache_all

	/* enable MMU first so we can isolate which later cache step breaks boot */
	mrc	p15, 0, r0, c1, c0, 0
	bic	r0, r0, #(1 << 1)		/* A = 0 */
	orr	r0, r0, #(1 << 0)		/* M */
	mcr	p15, 0, r0, c1, c0, 0
	dsb
	isb

	/* turn on I-cache next */
	mrc	p15, 0, r0, c1, c0, 0
	orr	r0, r0, #(1 << 12)		/* I */
	mcr	p15, 0, r0, c1, c0, 0
	dsb
	isb

	/* then branch prediction */
	mrc	p15, 0, r0, c1, c0, 0
	orr	r0, r0, #(1 << 11)		/* Z */
	mcr	p15, 0, r0, c1, c0, 0
	dsb
	isb

	/* finally enable D-cache */
	mrc	p15, 0, r0, c1, c0, 0
	orr	r0, r0, #(1 << 2)		/* C */
	mcr	p15, 0, r0, c1, c0, 0
	dsb
	isb

	ldmia	sp!, {pc}

/*
 * memory clear zero
 */
mem_clear:
	sub	r1, r1, #32
	cmp	r0, r1
	ble	cp
	mov r3, #0
	mov r4, #0
	mov r5, #0
	mov r6, #0
	mov r7, #0
	mov r8, #0
	mov r9, #0
	mov r10, #0
1:
	stmia r0!, {r3-r10}
	cmp	r0, r1
	ble	1b
cp:
	add	r1, r1, #32
2:
	str	r2, [r0], #4
	cmp	r0, r1
	blt	2b
	mov	pc, lr

/*
 * exception handlers
 */
undefined_instruction:
	/* 保存 LR 到 r0，然后调用 C 异常处理函数 */
	mov r0, lr
	bl my_undef_handler
	b .

software_interrupt:
	b	.

prefetch_abort:
	b	.

data_abort:
	/* 保存 LR 到 r0，然后调用 C 异常处理函数 */
	mov r0, lr
	bl my_data_abort_handler
	b .

not_used:
	b	.

	.global	irq
irq:
	/* get irq's sp */
	ldr	sp, _stack_irq_end

	/* save user regs */
	sub	sp, sp, #72
	stmia sp, {r0 - r12}			/* calling r0-r12 */
	add r8, sp, #60
	stmdb r8, {sp, lr}^				/* calling sp, lr */
	str lr, [r8, #0]				/* save calling pc */
	mrs r6, spsr
	str r6, [r8, #4]				/* save cpsr */
	str r0, [r8, #8]				/* save old_r0 */
	mov	r0, sp

	/* do irqs routlines */
	bl 	do_irqs

	/* restore user regs */
	ldmia sp, {r0 - lr}^			/* calling r0 - lr */
	mov	r0, r0
	ldr	lr, [sp, #60]				/* get pc */
	add	sp, sp, #72
	subs pc, lr, #4					/* return & move spsr_svc into cpsr */

fiq:
	b	.

/*
 * the location of stacks
 */
  	.align 4
_stack_fiq_end:
	.long	__stack_fiq_end
_stack_irq_end:
	.long	__stack_irq_end
_stack_abt_end:
	.long	__stack_abt_end
_stack_und_end:
	.long	__stack_und_end
_stack_srv_end:
	.long	__stack_srv_end

/*
 * the location of section
 */
  	.align 4
_text_start:
	.long	__text_start
_text_end:
	.long	__text_end
_data_start:
	.long	__data_start
_data_end:
	.long	__data_end
_bss_start:
	.long	__bss_start
_bss_end:
	.long	__bss_end

.end
