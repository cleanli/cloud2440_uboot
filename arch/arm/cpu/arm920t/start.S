/*
 *  armboot - Startup Code for ARM920 CPU-core
 *
 *  Copyright (c) 2001	Marius Gröger <mag@sysgo.de>
 *  Copyright (c) 2002	Alex Züpke <azu@sysgo.de>
 *  Copyright (c) 2002	Gary Jennejohn <garyj@denx.de>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <config.h>

/*
 *************************************************************************
 *
 * Jump vector table as in table 3.1 in [1]
 *
 *************************************************************************
 */


.globl _start
_start:	b	start_code
	ldr	pc, _undefined_instruction
	ldr	pc, _software_interrupt
	ldr	pc, _prefetch_abort
	ldr	pc, _data_abort
	ldr	pc, _not_used
	ldr	pc, _irq
	ldr	pc, _fiq

_undefined_instruction:	.word undefined_instruction
_software_interrupt:	.word software_interrupt
_prefetch_abort:	.word prefetch_abort
_data_abort:		.word data_abort
_not_used:		.word not_used
_irq:			.word irq
_fiq:			.word fiq

	.balignl 16,0xdeadbeef


/*
 *************************************************************************
 *
 * Startup Code (called from the ARM reset exception vector)
 *
 * do important init only if we don't start from memory!
 * relocate armboot to ram
 * setup stack
 * jump to second stage
 *
 *************************************************************************
 */

_TEXT_BASE:
	.word	TEXT_BASE

.globl _armboot_start
_armboot_start:
	.word _start

/*
 * These are defined in the board-specific linker script.
 */
.globl _bss_start
_bss_start:
	.word __bss_start

.globl _bss_end
_bss_end:
	.word _end

#ifdef CONFIG_USE_IRQ
/* IRQ stack memory (calculated at run-time) */
.globl IRQ_STACK_START
IRQ_STACK_START:
	.word	0x0badc0de

/* IRQ stack memory (calculated at run-time) */
.globl FIQ_STACK_START
FIQ_STACK_START:
	.word 0x0badc0de
#endif

	.ltorg
/* the literal pools origin */
SMRDATA:
        .long   0x22111110
        .long   0x00000700
        .long   0x00000700
        .long   0x00000700
        .long   0x00000700
        .long   0x00000700
        .long   0x00000700
        .long   0x0001800a
        .long   0x0001800a
        .long   0x00ac03f4
        .long   0x000000b2
        .long   0x00000030
        .long   0x00000030
/*
        .long   0x22119120
        .long   0x00000700
        .long   0x00000700
        .long   0x00000700
        .long   0x00001f4c
        .long   0x00000700
        .long   0x00000700
        .long   0x00018005
        .long   0x00018005
        .long   0x008e0459
        .long   0x00000032
        .long   0x00000030
        .long   0x00000030
*/

/*88888888888888888just for debug888888888888888888888*/
#if 0
test:
	mov	r3, #0
xtest:
@@@@@@@@@@
	ldr	r0, =0x56000054
	add 	r3, r3, #1
	str	r3, [r0]
@@@@@@@@@@
@@@@@@@@@@
@	ldr	r0, =0x50000020
@	mov	r1, #0x35
@	str	r1, [r0]
@@@@@@@@@@
@@@@@@@@@@
	ldr	r0, =0x00000f20
aagg:
	sub	r0, r0, #1
	cmp	r0, #0
	bne	aagg
@@@@@@@@@@
	b	xtest
#endif
/*88888888888888888888888888888888888888*/

/*
 * the actual start code
 */

start_code:
	/*
	 * set the cpu to SVC32 mode
	 */
@make gpio F 4-7 output, for led debug
	ldr	r0, =0x56000050
	mov	r1, #0x5500
	str	r1, [r0]
@set gpio F4 high, the led off
	ldr	r0, =0x56000054
	mov	r1, #0x10
	str	r1, [r0]

	mrs	r0, cpsr
	bic	r0, r0, #0x1f
	orr	r0, r0, #0xd3
	msr	cpsr, r0

	ldr	sp, =0x00000ffc 	/*sp for tmp use*/
	@bl	coloured_LED_init
	@bl	red_LED_on

#if	defined(CONFIG_AT91RM9200DK) || defined(CONFIG_AT91RM9200EK)
	/*
	 * relocate exception table
	 */
	ldr	r0, =_start
	ldr	r1, =0x0
	mov	r2, #16
copyex:
	subs	r2, r2, #1
	ldr	r3, [r0], #4
	str	r3, [r1], #4
	bne	copyex
#endif

@	b test
#ifdef CONFIG_S3C24X0
	/* turn off the watchdog */

# if defined(CONFIG_S3C2400)
#  define pWTCON	0x15300000
#  define INTMSK	0x14400008	/* Interupt-Controller base addresses */
#  define CLKDIVN	0x14800014	/* clock divisor register */
#else
#  define pWTCON	0x53000000
#  define INTMSK	0x4A000008	/* Interupt-Controller base addresses */
#  define INTSUBMSK	0x4A00001C
#  define CLKDIVN	0x4C000014	/* clock divisor register */
# endif

	ldr	r0, =pWTCON
	mov	r1, #0x0
	str	r1, [r0]
@@@@@@@@@@
	ldr	r0, =0x56000054
	mov	r1, #0x30
	str	r1, [r0]

	/*
	 * mask all IRQs by setting all bits in the INTMR - default
	 */
	mov	r1, #0xffffffff
	ldr	r0, =INTMSK
	str	r1, [r0]
# if defined(CONFIG_S3C2410)
	ldr	r1, =0x7ff
	ldr	r0, =INTSUBMSK
	str	r1, [r0]
# endif

#  define GPFCON                0x56000050
#  define GPFDAT                0x56000054      /*  */
#  define LOCKTIME              0x4C000000
#  define MPLLCON               0x4C000004
#  define UPLLCON               0x4C000008

        ldr     r0, =GPFCON
        mov     r1, #0x5500
        str     r1, [r0]

        @initialize clock
        ldr     r0, =LOCKTIME
        ldr     r1, =0x00ffffff
        str     r1, [r0]
        /* FCLK:HCLK:PCLK = 1:4:8 */
        ldr     r0, =CLKDIVN
        mov     r1, #5
        str     r1, [r0]
        @open code cache
        mrc     p15,0,r1,c1,c0,0
        orr     r1,r1,#0xc0000000
        mcr     p15,0,r1,c1,c0,0
        @UPLL setup
        ldr     r0, =UPLLCON
        ldr     r1, =(0x38<<12|0x2<<4|0x2)
        str     r1, [r0]
        nop
        nop
        nop
        nop
        nop
        nop
        nop

        @MPLL setup
        ldr     r0, =MPLLCON
        ldr     r1, =0x7f021
        str     r1, [r0]
        nop
        nop
        nop
        nop
        nop
        nop
        nop
@	b test
@@@@@@@@@@
	ldr	r0, =0x56000054
	mov	r1, #0x40
	str	r1, [r0]
@@@@@@@@@@
	/* FCLK:HCLK:PCLK = 1:4:8 */
	/* FCLK is 405 MHz ! */
#endif	/* CONFIG_S3C24X0 */

	 /* we do sys-critical inits only at reboot,
	 * not when booting from ram!
	 */
@	b test
#define GPHCON  0x56000070
#define GPHUP   0x56000078
#define ULCON0  0x50000000
#define UCCON0  0x50000004
#define UFCON0  0x50000008
#define USCON0  0x50000010
#define UTXH0   0x50000020
#define URXH0   0x50000024
#define UBRDIV0 0x50000028

/*Init serial port 0*/
        ldr     r0, =GPHCON
        ldr     r1, =0xaaa0
        str     r1, [r0]
        ldr     r0, =GPHUP
        ldr     r1, =0x7ff
        str     r1, [r0]
        ldr     r0, =ULCON0
        mov     r1, #0x3
        str     r1, [r0]
        ldr     r0, =UCCON0
        mov     r1, #0x5
        str     r1, [r0]
        ldr     r0, =UBRDIV0
        mov     r1, #0x1b
        str     r1, [r0]        /*set baudrate 115200bps*/
@mov	r0, #'A'
@bl	s3c2440_serial_send_byte

/* init serial end  */
@	b test
#ifndef CONFIG_SKIP_LOWLEVEL_INIT
	bl	cpu_init_crit
#endif

@	b test
	/* Set up the stack						    */
stack_setup:
	ldr	r0, _TEXT_BASE		/* upper 128 KiB: relocated uboot   */
	sub	r0, r0, #CONFIG_SYS_MALLOC_LEN	/* malloc area              */
	sub	r0, r0, #CONFIG_SYS_GBL_DATA_SIZE /* bdinfo                 */
#ifdef CONFIG_USE_IRQ
	sub	r0, r0, #(CONFIG_STACKSIZE_IRQ+CONFIG_STACKSIZE_FIQ)
#endif
	sub	sp, r0, #12		/* leave 3 words for abort-stack    */

#ifndef CONFIG_SKIP_RELOCATE_UBOOT
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@
@ copy code to ram from nand
@
#define NAND_CTL_BASE   0x4e000000
#define oNFCONF         0x0
#define oNFCONT         0x4
#define oNFCMD          0x8
#define oNFSTAT         0x20

        @ reset NAND
        mov     r1, #NAND_CTL_BASE
        ldr     r2, =( (7<<12)|(7<<8)|(7<<4)|(0<<0) )
        str     r2, [r1, #oNFCONF]

        ldr     r2, =( (1<<4)|(0<<1)|(1<<0) ) @ Active low CE Control
        str     r2, [r1, #oNFCONT]
        ldr     r2, [r1, #oNFCONT]

@        ldr     r2, =(0x6)              @ RnB Clear
@        str     r2, [r1, #oNFSTAT]
@        ldr     r2, [r1, #oNFSTAT]

        mov     r2, #0xff               @ RESET command
        strb    r2, [r1, #oNFCMD]
        mov     r3, #0                  @ wait
1:      add     r3, r3, #0x1
        cmp     r3, #0x2a
        blt     1b

@        ldr     r2, [r1, #oNFCONT]
@        orr     r2, r2, #0x2            @ Flash Memory Chip Disable
@        str     r2, [r1, #oNFCONT]
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        @ get ready to call C functions (for nand_read())
        @ldr     sp, DW_STACK_START      @ has been setuped at start

#define TEXT_RAM_BASE   0x33f80000
#define TEXT_NAND_BASE  0x0
#define TEXT_SIZE       0x60000

        @ copy minicb ENV block to ram base
        ldr     r0, =TEXT_RAM_BASE
        ldr     r1, =TEXT_NAND_BASE
        ldr     r2, =TEXT_SIZE
@	b test
        bl      nand_read_ll
@	b test
#endif	/* CONFIG_SKIP_RELOCATE_UBOOT */

clear_bss:
	ldr	r0, _bss_start		/* find start of bss segment        */
	ldr	r1, _bss_end		/* stop here                        */
	mov	r2, #0x00000000		/* clear                            */

clbss_l:str	r2, [r0]		/* clear loop...                    */
	add	r0, r0, #4
	cmp	r0, r1
	ble	clbss_l

@	b test
	ldr	pc, _start_armboot

_start_armboot:	.word start_armboot


/*
 *************************************************************************
 *
 * CPU_init_critical registers
 *
 * setup important registers
 * setup memory timing
 *
 *************************************************************************
 */


#ifndef CONFIG_SKIP_LOWLEVEL_INIT
cpu_init_crit:
	/*
	 * flush v4 I/D caches
	 */
	mov	r0, #0
	mcr	p15, 0, r0, c7, c7, 0	/* flush v3/v4 cache */
	mcr	p15, 0, r0, c8, c7, 0	/* flush v4 TLB */

@	b test
	/*
	 * disable MMU stuff and caches
	 */
	/*
	mrc	p15, 0, r0, c1, c0, 0
	bic	r0, r0, #0x00002300	@ clear bits 13, 9:8 (--V- --RS)
	bic	r0, r0, #0x00000087	@ clear bits 7, 2:0 (B--- -CAM)
	orr	r0, r0, #0x00000002	@ set bit 2 (A) Align
	orr	r0, r0, #0x00001000	@ set bit 12 (I) I-Cache
	mcr	p15, 0, r0, c1, c0, 0
	*/

        mrc     p15, 0, r1, c1, c0, 0           @ read ctrl register
        orr     r1, r1, #0xc0000000             @ Asynchronous
        mcr     p15, 0, r1, c1, c0, 0           @ write ctrl register

@	b test
	/*
	 * before relocating, we have to setup RAM timing
	 * because memory timing is board-dependend, you will
	 * find a lowlevel_init.S in your board directory.
	 */
	mov	ip, lr

@	b test
	bl	lowlevel_init

	mov	lr, ip
	mov	pc, lr
#endif /* CONFIG_SKIP_LOWLEVEL_INIT */

/*
 *************************************************************************
 *
 * Interrupt handling
 *
 *************************************************************************
 */

@
@ IRQ stack frame.
@
#define S_FRAME_SIZE	72

#define S_OLD_R0	68
#define S_PSR		64
#define S_PC		60
#define S_LR		56
#define S_SP		52

#define S_IP		48
#define S_FP		44
#define S_R10		40
#define S_R9		36
#define S_R8		32
#define S_R7		28
#define S_R6		24
#define S_R5		20
#define S_R4		16
#define S_R3		12
#define S_R2		8
#define S_R1		4
#define S_R0		0

#define MODE_SVC	0x13
#define I_BIT		0x80

/*
 * use bad_save_user_regs for abort/prefetch/undef/swi ...
 * use irq_save_user_regs / irq_restore_user_regs for IRQ/FIQ handling
 */

	.macro	bad_save_user_regs
	sub	sp, sp, #S_FRAME_SIZE
	stmia	sp, {r0 - r12}			@ Calling r0-r12
	ldr	r2, _armboot_start
	sub	r2, r2, #(CONFIG_STACKSIZE)
	sub	r2, r2, #(CONFIG_SYS_MALLOC_LEN)
	/* set base 2 words into abort stack */
	sub	r2, r2, #(CONFIG_SYS_GBL_DATA_SIZE+8)
	ldmia	r2, {r2 - r3}			@ get pc, cpsr
	add	r0, sp, #S_FRAME_SIZE		@ restore sp_SVC

	add	r5, sp, #S_SP
	mov	r1, lr
	stmia	r5, {r0 - r3}			@ save sp_SVC, lr_SVC, pc, cpsr
	mov	r0, sp
	.endm

	.macro	irq_save_user_regs
	sub	sp, sp, #S_FRAME_SIZE
	stmia	sp, {r0 - r12}			@ Calling r0-r12
	add	r7, sp, #S_PC
	stmdb	r7, {sp, lr}^			@ Calling SP, LR
	str	lr, [r7, #0]			@ Save calling PC
	mrs	r6, spsr
	str	r6, [r7, #4]			@ Save CPSR
	str	r0, [r7, #8]			@ Save OLD_R0
	mov	r0, sp
	.endm

	.macro	irq_restore_user_regs
	ldmia	sp, {r0 - lr}^			@ Calling r0 - lr
	mov	r0, r0
	ldr	lr, [sp, #S_PC]			@ Get PC
	add	sp, sp, #S_FRAME_SIZE
	/* return & move spsr_svc into cpsr */
	subs	pc, lr, #4
	.endm

	.macro get_bad_stack
	ldr	r13, _armboot_start		@ setup our mode stack
	sub	r13, r13, #(CONFIG_STACKSIZE)
	sub	r13, r13, #(CONFIG_SYS_MALLOC_LEN)
	/* reserve a couple spots in abort stack */
	sub	r13, r13, #(CONFIG_SYS_GBL_DATA_SIZE+8)

	str	lr, [r13]			@ save caller lr / spsr
	mrs	lr, spsr
	str	lr, [r13, #4]

	mov	r13, #MODE_SVC			@ prepare SVC-Mode
	@ msr	spsr_c, r13
	msr	spsr, r13
	mov	lr, pc
	movs	pc, lr
	.endm

	.macro get_irq_stack			@ setup IRQ stack
	ldr	sp, IRQ_STACK_START
	.endm

	.macro get_fiq_stack			@ setup FIQ stack
	ldr	sp, FIQ_STACK_START
	.endm

/*
 * exception handlers
 */
	.align  5
undefined_instruction:
	get_bad_stack
	bad_save_user_regs
	bl	do_undefined_instruction

	.align	5
software_interrupt:
	get_bad_stack
	bad_save_user_regs
	bl	do_software_interrupt

	.align	5
prefetch_abort:
	get_bad_stack
	bad_save_user_regs
	bl	do_prefetch_abort

	.align	5
data_abort:
	get_bad_stack
	bad_save_user_regs
	bl	do_data_abort

	.align	5
not_used:
	get_bad_stack
	bad_save_user_regs
	bl	do_not_used

#ifdef CONFIG_USE_IRQ

	.align	5
irq:
	get_irq_stack
	irq_save_user_regs
	bl	do_irq
	irq_restore_user_regs

	.align	5
fiq:
	get_fiq_stack
	/* someone ought to write a more effiction fiq_save_user_regs */
	irq_save_user_regs
	bl	do_fiq
	irq_restore_user_regs

#else

	.align	5
irq:
	get_bad_stack
	bad_save_user_regs
	bl	do_irq

	.align	5
fiq:
	get_bad_stack
	bad_save_user_regs
	bl	do_fiq

#endif

#define BWSCON	0x48000000
lowlevel_init:
	/* memory control configuration */
        ldr     r0, =SMRDATA
#ifndef CONFIG_SKIP_RELOCATE_UBOOT
	ldr	r1, _TEXT_BASE
	sub	r0, r0, r1
#endif
	ldr	r1, =BWSCON	/* Bus Width Status Controller */
	add     r2, r0, #13*4
0:
	ldr     r3, [r0], #4
	str     r3, [r1], #4
	cmp     r2, r0
	bne     0b

	/* everything is fine now */
	mov	pc, lr

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
s3c2440_serial_send_byte:
mov	r2, #0x50000000
ldr	r3, [r2,#16]
tst	r3, #2
beq	s3c2440_serial_send_byte
str	r0, [r2, #32]
mov	pc, lr

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
ps:
push	{r4, r5, r6, lr}
mov	r5, r0
mov 	r4, #0
b	1f
2:
bl	s3c2440_serial_send_byte
1:
ldrb	r3, [r5, r4]
subs	r0, r3, #0
add	r4, r4, #1
bne	2b
pop	{r4, r5, r6, pc}

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
nand_read_ll:
push	{r4, r5, r6, r7, sl, lr}

mov	r6, r0
mov	r7, r1
mov	sl, r2

@print welcome message
adr     r0, wel_mesg
bl	ps

@nand chip enable
mov	r2, #0x4e000000
ldr	r3, [r2, #4]
bic	r3, r3, #2
str	r3, [r2, #4]

@r7=nand start addr
mov	r4, r7
mov 	r5, #0
b	1f

@nand clear_rb
2:
mov	r2, #0x4e000000
ldrb	r3, [r2, #32]
asr	r1, r4, #9	

@------------following direct from asm file
orr	r3, r3, #4
        @NFCMD = 0;

        @        NFADDR = i & 0xff;
        @        NFADDR = (i >> 9) & 0xff;
        @        NFADDR = (i >> 17) & 0xff;
asr	r0, r4, #17
        @        NFADDR = (i >> 25) & 0xff;
asr	ip, r4, #25
        @NAND_CHIP_ENABLE;
        @        NAND_CLEAR_RB;
strb	r3, [r2, #32]

and	r1, r1, #255
        @        NFADDR = (i >> 17) & 0xff;
and	r0, r0, #255
        @        NFADDR = (i >> 25) & 0xff;
and	ip, ip, #255
        @        NFADDR = i & 0xff;
and	lr, r4, #255
mov	r3, #0
strb	r3, [r2, #8]

        @        NFADDR = i & 0xff;
strb	lr, [r2, #12]
        @        NFADDR = (i >> 9) & 0xff;
strb	r1, [r2, #12]
        @        NFADDR = (i >> 17) & 0xff;
strb	r0, [r2, #12]
        @        NFADDR = (i >> 25) & 0xff;
strb	ip, [r2, #12]

        @        NAND_DETECT_RB;
3:
mov	r3, #0x4e000000
ldrb	r3, [r3, #32]
tst	r3, #4
beq	3b

        @        for(j=0; j < NAND_SECTOR_SIZE; j++, i++) {
mov	r2, #0
4:
mov	r3, #0x4e000000
ldrb	r3, [r3, #16]
strb	r3, [r6, r2]
add	r2, r2, #1
cmp	r2, #512
bne	4b
tst	r5, #15
add	r4, r4, #512
add	r6, r6, #512
moveq	r0, #'>'
bleq	s3c2440_serial_send_byte
1:
add	r5, r5, #1
add	r3, sl, r7
cmp	r4, r3
bcc	2b
mov	r2, #0x4e000000
ldr	r3, [r2, #4]
orr	r3, r3, #2
        @NAND_CHIP_DISABLE;
str	r3, [r2, #4]
	@ps("\r\nMove U-boot code from nand to Ram done.\r\n");
adr	r0, done_msg
bl	ps
        @return 0;
mov	r0, #0
pop	{r4, r5, r6, r7, sl, pc}
wel_mesg:
.string "\r\n\r\nWelcome use U-boot, compiled by Clean Li.\r\nMoving U-boot code from nand to Ram\r\n"
done_msg:
.string "\r\ndone.\r\n"
