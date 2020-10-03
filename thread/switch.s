[bits 32]
section .text
global switch_to
;此函数由中断处理程序调用 中断处理程序已经保存过发生中断之前
;的上下文（称为上下文1）这里还需要保存中断的上下文（称为上下文2） 
;上下文2只需要保存4个寄存器就行了
;实现任务切换的原理是：

;通过切换栈 来ret到另一个线程被换下时保存的地址/（初始时指定的函数地址）
;A线程发生中断 保存上下文1 到它自己
;的栈（位于其TCB内）  然后调用switch_to函数 保存上下文2
;在switch_to中 切换栈到 线程B的栈，线程B的栈有两种可能：
;1:从来没有执行过 ，但是根据线程创建函数thread_start中对两个栈以及栈指针
;的初始化 esp会指向thread_stack的最低地址 刚好对应后面的pop 四个寄存器
;然后执行ret跳转到kernel_thread函数 然后由于thread_create的设置 回到kernel_
;thread中时 栈顶上的三个元素 分别是1调用kernel_thread的函数的返回地址（虽然我们
;知道不存在调用者 是ret过来的 但是处理器会这样看） 2参数二 3参数一 我们在thread_
;create里不仅让switch_to能够ret到kernel_create，而且还给它传入的参数，它会用
;这两个参数，来执行真正的线程函数
;2:之前执行过 但是被换下来了 那么线程B的栈 可以参考线程A的过程 栈里最下面是
;刚刚push的四个寄存器 pop回去 然后再就是ret 这时候ret会回到线程B被换下时的
;中断函数里，然后继续把中断走完 回到线程B被中断之前

;参数二 参数一 retaddr esi edi ebx ebp
;参数一是cur tcb首地址，对应该tcb的self_kstack，用于保存cur 的esp
;参数二是next tcb首地址，
switch_to:
push esi
push edi
push ebx
push ebp
;保存中断函数的寄存器（abi约定其余寄存器由调用者自己负责 这四个则是被调用者保存）

mov eax,[esp+20];
mov [eax],esp;保存栈顶

;----------------------
;以上都是保存被切换的线程的寄存器

mov eax,[esp+24];
mov esp,[eax];切换到新线程的栈顶

;恢复新线程 的中断函数里的四个寄存器
pop ebp
pop ebx
pop edi
pop esi
;ret 会跳转到中断函数里 但是根据新线程的栈 应该是执行完switch_to
;函数的中断（然后中断就会执行恢复寄存器 再返回到新线程）
;或者新线程是第一次被执行 直接ret到kernel_thread 再执行线程函数
ret
