	.file	"interrupt.c"
	.text
	.type	outb, @function
outb:
.LFB0:
	.cfi_startproc
	pushl	%ebp
	.cfi_def_cfa_offset 8
	.cfi_offset 5, -8
	movl	%esp, %ebp
	.cfi_def_cfa_register 5
	subl	$8, %esp
	call	__x86.get_pc_thunk.ax
	addl	$_GLOBAL_OFFSET_TABLE_, %eax
	movl	8(%ebp), %eax
	movl	12(%ebp), %edx
	movw	%ax, -4(%ebp)
	movl	%edx, %eax
	movb	%al, -8(%ebp)
	movzbl	-8(%ebp), %eax
	movzwl	-4(%ebp), %edx
#APP
# 12 "../lib/kernel/io.h" 1
	outb %al,%dx
# 0 "" 2
#NO_APP
	nop
	leave
	.cfi_restore 5
	.cfi_def_cfa 4, 4
	ret
	.cfi_endproc
.LFE0:
	.size	outb, .-outb
	.local	idt
	.comm	idt,264,32
	.type	make_idt_desc, @function
make_idt_desc:
.LFB4:
	.cfi_startproc
	endbr32
	pushl	%ebp
	.cfi_def_cfa_offset 8
	.cfi_offset 5, -8
	movl	%esp, %ebp
	.cfi_def_cfa_register 5
	subl	$4, %esp
	call	__x86.get_pc_thunk.ax
	addl	$_GLOBAL_OFFSET_TABLE_, %eax
	movl	12(%ebp), %eax
	movb	%al, -4(%ebp)
	movl	16(%ebp), %eax
	movl	%eax, %edx
	movl	8(%ebp), %eax
	movw	%dx, (%eax)
	movl	8(%ebp), %eax
	movw	$8, 2(%eax)
	movl	8(%ebp), %eax
	movb	$0, 4(%eax)
	movl	8(%ebp), %eax
	movzbl	-4(%ebp), %edx
	movb	%dl, 5(%eax)
	movl	16(%ebp), %eax
	shrl	$16, %eax
	movl	%eax, %edx
	movl	8(%ebp), %eax
	movw	%dx, 6(%eax)
	nop
	leave
	.cfi_restore 5
	.cfi_def_cfa 4, 4
	ret
	.cfi_endproc
.LFE4:
	.size	make_idt_desc, .-make_idt_desc
	.section	.rodata
.LC0:
	.string	"    idt_desc_init done!\n"
	.text
	.type	idt_desc_init, @function
idt_desc_init:
.LFB5:
	.cfi_startproc
	endbr32
	pushl	%ebp
	.cfi_def_cfa_offset 8
	.cfi_offset 5, -8
	movl	%esp, %ebp
	.cfi_def_cfa_register 5
	pushl	%ebx
	subl	$20, %esp
	.cfi_offset 3, -12
	call	__x86.get_pc_thunk.bx
	addl	$_GLOBAL_OFFSET_TABLE_, %ebx
	movl	$0, -12(%ebp)
	jmp	.L4
.L5:
	movl	intr_entry_table@GOT(%ebx), %eax
	movl	-12(%ebp), %edx
	movl	(%eax,%edx,4), %eax
	movl	-12(%ebp), %edx
	leal	0(,%edx,8), %ecx
	leal	idt@GOTOFF(%ebx), %edx
	addl	%ecx, %edx
	pushl	%eax
	pushl	$142
	pushl	%edx
	call	make_idt_desc
	addl	$12, %esp
	addl	$1, -12(%ebp)
.L4:
	cmpl	$32, -12(%ebp)
	jle	.L5
	subl	$12, %esp
	leal	.LC0@GOTOFF(%ebx), %eax
	pushl	%eax
	call	put_str@PLT
	addl	$16, %esp
	nop
	movl	-4(%ebp), %ebx
	leave
	.cfi_restore 5
	.cfi_restore 3
	.cfi_def_cfa 4, 4
	ret
	.cfi_endproc
.LFE5:
	.size	idt_desc_init, .-idt_desc_init
	.section	.rodata
.LC1:
	.string	"   pic_init done!\n"
	.text
	.type	pic_init, @function
pic_init:
.LFB6:
	.cfi_startproc
	endbr32
	pushl	%ebp
	.cfi_def_cfa_offset 8
	.cfi_offset 5, -8
	movl	%esp, %ebp
	.cfi_def_cfa_register 5
	pushl	%ebx
	subl	$4, %esp
	.cfi_offset 3, -12
	call	__x86.get_pc_thunk.bx
	addl	$_GLOBAL_OFFSET_TABLE_, %ebx
	pushl	$17
	pushl	$32
	call	outb
	addl	$8, %esp
	pushl	$32
	pushl	$33
	call	outb
	addl	$8, %esp
	pushl	$4
	pushl	$33
	call	outb
	addl	$8, %esp
	pushl	$1
	pushl	$33
	call	outb
	addl	$8, %esp
	pushl	$17
	pushl	$160
	call	outb
	addl	$8, %esp
	pushl	$40
	pushl	$161
	call	outb
	addl	$8, %esp
	pushl	$2
	pushl	$161
	call	outb
	addl	$8, %esp
	pushl	$1
	pushl	$161
	call	outb
	addl	$8, %esp
	pushl	$254
	pushl	$33
	call	outb
	addl	$8, %esp
	pushl	$255
	pushl	$161
	call	outb
	addl	$8, %esp
	subl	$12, %esp
	leal	.LC1@GOTOFF(%ebx), %eax
	pushl	%eax
	call	put_str@PLT
	addl	$16, %esp
	nop
	movl	-4(%ebp), %ebx
	leave
	.cfi_restore 5
	.cfi_restore 3
	.cfi_def_cfa 4, 4
	ret
	.cfi_endproc
.LFE6:
	.size	pic_init, .-pic_init
	.section	.rodata
.LC2:
	.string	"idt_init start\n"
.LC3:
	.string	"idt_init done!\n"
	.text
	.globl	idt_init
	.type	idt_init, @function
idt_init:
.LFB7:
	.cfi_startproc
	endbr32
	pushl	%ebp
	.cfi_def_cfa_offset 8
	.cfi_offset 5, -8
	movl	%esp, %ebp
	.cfi_def_cfa_register 5
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	subl	$28, %esp
	.cfi_offset 7, -12
	.cfi_offset 6, -16
	.cfi_offset 3, -20
	call	__x86.get_pc_thunk.bx
	addl	$_GLOBAL_OFFSET_TABLE_, %ebx
	movl	%gs:20, %eax
	movl	%eax, -28(%ebp)
	xorl	%eax, %eax
	subl	$12, %esp
	leal	.LC2@GOTOFF(%ebx), %eax
	pushl	%eax
	call	put_str@PLT
	addl	$16, %esp
	call	idt_desc_init
	call	pic_init
	leal	idt@GOTOFF(%ebx), %eax
	movl	$0, %edx
	shldl	$16, %eax, %edx
	sall	$16, %eax
	movl	%eax, %ecx
	orl	$263, %ecx
	movl	%ecx, %esi
	movl	%edx, %eax
	orb	$0, %ah
	movl	%eax, %edi
	movl	%esi, -40(%ebp)
	movl	%edi, -36(%ebp)
#APP
# 70 "interrupt.c" 1
	lidt -40(%ebp)
# 0 "" 2
#NO_APP
	subl	$12, %esp
	leal	.LC3@GOTOFF(%ebx), %eax
	pushl	%eax
	call	put_str@PLT
	addl	$16, %esp
	nop
	movl	-28(%ebp), %eax
	xorl	%gs:20, %eax
	je	.L8
	call	__stack_chk_fail_local
.L8:
	leal	-12(%ebp), %esp
	popl	%ebx
	.cfi_restore 3
	popl	%esi
	.cfi_restore 6
	popl	%edi
	.cfi_restore 7
	popl	%ebp
	.cfi_restore 5
	.cfi_def_cfa 4, 4
	ret
	.cfi_endproc
.LFE7:
	.size	idt_init, .-idt_init
	.section	.text.__x86.get_pc_thunk.ax,"axG",@progbits,__x86.get_pc_thunk.ax,comdat
	.globl	__x86.get_pc_thunk.ax
	.hidden	__x86.get_pc_thunk.ax
	.type	__x86.get_pc_thunk.ax, @function
__x86.get_pc_thunk.ax:
.LFB8:
	.cfi_startproc
	movl	(%esp), %eax
	ret
	.cfi_endproc
.LFE8:
	.section	.text.__x86.get_pc_thunk.bx,"axG",@progbits,__x86.get_pc_thunk.bx,comdat
	.globl	__x86.get_pc_thunk.bx
	.hidden	__x86.get_pc_thunk.bx
	.type	__x86.get_pc_thunk.bx, @function
__x86.get_pc_thunk.bx:
.LFB9:
	.cfi_startproc
	movl	(%esp), %ebx
	ret
	.cfi_endproc
.LFE9:
	.hidden	__stack_chk_fail_local
	.ident	"GCC: (Ubuntu 9.3.0-10ubuntu2) 9.3.0"
	.section	.note.GNU-stack,"",@progbits
	.section	.note.gnu.property,"a"
	.align 4
	.long	 1f - 0f
	.long	 4f - 1f
	.long	 5
0:
	.string	 "GNU"
1:
	.align 4
	.long	 0xc0000002
	.long	 3f - 2f
2:
	.long	 0x3
3:
	.align 4
4:
