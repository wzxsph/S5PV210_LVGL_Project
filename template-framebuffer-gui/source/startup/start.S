.equ	USR_MODE,		0x10	/* normal user mode */
.equ 	FIQ_MODE,		0x11	/* fiq fast interrupts mode */
.equ 	IRQ_MODE,		0x12	/* irq standard interrupts mode */
.equ 	SVC_MODE,		0x13	/* supervisor interrupts mode */
.equ 	ABT_MODE,		0x17	/* abort processing memory faults mode */
.equ 	UDF_MODE,		0x1b	/* undefined instructions mode */
.equ 	MODE_MASK,		0x1f	/* system running in priviledged operating mode */
.equ 	NO_INT,			0xc0

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

	/* MMU initialization using __mmu_table_start (following reference approach) */
mmu_init:
	/* Step 1: Fill entire 4GB table with DEVICE (strongly-ordered) */
	movw	r0, #:lower16:__mmu_table_start
	movt	r0, #:upper16:__mmu_table_start
	mov	r1, #0
	/* Load DEVICE attribute: 0xC02 */
	movw	r3, #0x0C02
mmu_fill_device:
	cmp	r1, #4096
	beq	mmu_fill_device_end
	mov	r2, r1, lsl #20
	orr	r2, r2, r3
	str	r2, [r0], #4
	add	r1, r1, #1
	b	mmu_fill_device
mmu_fill_device_end:

	/* Step 2: Override RAM sections (0x200-0x3FF) with NORMAL (cacheable) */
	movw	r0, #:lower16:__mmu_table_start
	movt	r0, #:upper16:__mmu_table_start
	add	r0, r0, #(0x200 * 4)
	mov	r1, #0x200
	/* Load NORMAL attribute: 0xC1E */
	movw	r3, #0x0C1E
mmu_fill_normal:
	cmp	r1, #0x400
	beq	mmu_fill_normal_end
	mov	r2, r1, lsl #20
	orr	r2, r2, r3
	str	r2, [r0], #4
	add	r1, r1, #1
	b	mmu_fill_normal
mmu_fill_normal_end:

	/* Step 3: Set TTB0 */
	movw	r0, #:lower16:__mmu_table_start
	movt	r0, #:upper16:__mmu_table_start
	mcr	p15, 0, r0, c2, c0, 0

	/* Step 4: Set domain access control - ALL domains manager mode */
	movw	r0, #0xFFFF
	movt	r0, #0xFFFF
	mcr	p15, 0, r0, c3, c0, 0

	/* Step 5: Invalidate I-cache, B-pred, TLB */
	mov	r0, #0
	mcr	p15, 0, r0, c7, c5, 0    /* I-cache */
	mcr	p15, 0, r0, c7, c5, 6    /* B-pred */
	mcr	p15, 0, r0, c8, c7, 0    /* TLB */

	/* Step 6: Invalidate D-cache using set/way */
	mrc	p15, 1, r0, c0, c0, 0
	ldr	r3, =0x1ff
	and	r0, r3, r0, lsr #13
	mov	r1, #0
mmu_dcache_way:
	mov	r3, #0
mmu_dcache_set:
	mov	r2, r1, lsl #30
	orr	r2, r3, lsl #5
	mcr	p15, 0, r2, c7, c6, 2
	add	r3, r3, #1
	cmp	r0, r3
	bgt	mmu_dcache_set
	add	r1, r1, #1
	cmp	r1, #4
	bne	mmu_dcache_way

	dsb
	isb

	/* Step 7: Enable MMU, I-cache, D-cache, branch pred, alignment */
	mrc	p15, 0, r0, c1, c0, 0
	bic	r0, r0, #0x3000      /* Clear V, I */
	bic	r0, r0, #0x0087      /* Clear B, C, A, M */
	orr	r0, r0, #0x0002      /* A: enable alignment check */
	orr	r0, r0, #0x0004      /* C: enable D-cache */
	orr	r0, r0, #0x1000      /* I: enable I-cache */
	orr	r0, r0, #0x0800      /* Z: enable branch pred */
	orr	r0, r0, #0x0001      /* M: enable MMU */
	mcr	p15, 0, r0, c1, c0, 0

	dsb
	isb

	mov	pc, lr

	/* Call mmu_init */
	bl	mmu_init

	/* DEBUG NOPs */
	nop
	nop
	nop

	/* clear bss section */
clear_bss:
	ldr	r0, _bss_start
	ldr	r1, _bss_end
	mov r2, #0x00000000
	bl	mem_clear

	/* jump to main fuction */
	mov r0, #0;
	mov r1, #0;
	bl	main			/* call main function */
	b	.

/*
 * MMU initialization - sets up flat translation table and enables caches/MMU
 *
 * Memory map for S5PV210:
 *   0x00000000-0x1FFFFFFF: Reserved (identity-mapped, strongly-ordered)
 *   0x20000000-0x3FFFFFFF: SDRAM (512MB) - normal cached memory
 *   0x40000000-0x7FFFFFFF: Reserved
 *   0x80000000-0x8FFFFFFF: Reserved
 *   0xD0020000-0xD0037FFF: iRAM - normal cached memory
 *   0xE0000000-0xFFFFFFFF: SFR/Peripherals - strongly-ordered device
 *
 * ARMv7 Section Descriptor format (32-bit L1 page table entry):
 *   [31:20] = Section base PA[31:20] (maps 1MB per entry)
 *   [15:12] = Domain (we use domain 0)
 *   [11:10] = AP[1:0] (0b11 = full access r/w in privileged and user)
 *   [4]    = XN (Execute Never: 0 = executable)
 *   [3]    = C (Cacheable)
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
