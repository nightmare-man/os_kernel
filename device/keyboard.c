#include "../lib/kernel/stdint.h"
#include "../lib/kernel/interrupt.h"
#include "../lib/kernel/print.h"
#include "../lib/kernel/io.h"
#include "./keyboard.h"
#include "./ioqueue.h"

//以下这一部分 书中将其定义在keyboard.c文件中，并设置为static 可能是既不想被其他文件访问
//又不想keyboard.h被修改导致keyboard.c变化 我想放在.h文件里 经过思考还是算了 其他文件
//不需要访问这些 而且也不该访问 特别是static 变量不该放在.h文件里
//以下为ascii码与扫描码的转换表


//第一部分 字符控制字符
#define esc '\033' //8进制 不用16进制是因为有些古老版本的c语言不支持16进制
#define backspace '\b'
#define tab '\t'
#define enter 'r'
#define delete '\177'
//第二部分 操作控制字符

#define invisible_char 0
#define ctrl_l_char invisible_char
#define ctrl_r_char invisible_char
#define shift_l_char invisible_char
#define shift_r_char invisible_char
#define alt_l_char invisible_char
#define alt_r_char invisible_char
#define caps_lock_char invisible_char

//第三部分 定义扫描码和ascii码（包含操作控制字符）的转换表
#define shift_l_make 0x2a
#define shift_r_make 0x36
#define alt_l_make 0x38
#define alt_r_make 0xe038  
//可以看到即使是主键盘中常用的 案件 扫描码也有两字节的 因此我们应该把扫描码定义为uint16_t
#define alt_r_break 0xe0b8
#define ctrl_l_make 0x1d
#define ctrl_r_make 0xe01d
#define ctrl_r_break 0xe09d
#define caps_lock_make 0x3a


//以下用来记录操作控制字符的状态
static bool ctrl_status,shift_status,alt_status,caps_lock_status,ext_scancode;
//ext_scancode 记录是不是扩展码（0xe0开头的 由于我们只记录主键盘区 即使扩展的 扫描码最多也只有两字节）

static char keymap[][2]={
	{0,0},		//00
	{esc,esc},		//01
	{'1','!'},		//02
	{'2','@'},		//03
	{'3','#'},		//04
	{'4','$'},		//05
	{'5','%'},		//06
	{'6','^'},		//07
	{'7','&'},		//08
	{'8','*'},		//09
	{'9','('},		//0a
	{'0',')'},		//0b
	{'-','_'},		//0c
	{'=','+'},		//0d
	{backspace,backspace},		//0e
	{tab,tab},		//0f
	{'q','Q'},		//10
	{'w','W'},		//11
	{'e','E'},		//12
	{'r','R'},		//13
	{'t','T'},		//14
	{'y','Y'},		//15
	{'u','U'},		//16
	{'i','I'},		//17
	{'o','O'},		//18
	{'p','P'},		//19
	{'[','{'},		//1a
	{']','}'},		//1b
	{enter,enter},		//1c
	{ctrl_l_char,ctrl_l_char},		//1d
	{'a','A'},
	{'s','S'},
	{'d','D'},
	{'f','F'},
	{'g','G'},
	{'h','H'},
	{'j','J'},
	{'k','K'},
	{'l','L'},
	{';',':'},
	{'\'','"'},
	{'`','~'},
	{shift_l_char,shift_l_char},
	{'\\','|'},
	{'z','Z'},
	{'x','X'},
	{'c','C'},
	{'v','V'},
	{'b','B'},
	{'n','N'},
	{'m','M'},
	{',','<'},
	{'.','>'},
	{'/','?'},
	{shift_r_char,shift_r_char},
	{'*','*'},
	{alt_l_char,alt_l_char},
	{' ',' '},
	{caps_lock_char,caps_lock_char}
	//其余按键暂时不处理
};
struct ioqueue kbd_buf;
static void intr_keyboard_handler(void){
	bool ctrl_status_last=ctrl_status;
	bool shift_status_last=shift_status;
	bool caps_status_last=caps_lock_status;

	bool break_code;//扫描码是不是断码？释放按键
	uint16_t scancode=inb(KBD_BUF_PORT);
	//用16位来保存 虽然只从缓冲区读了8位 但如果ext_scancode是true的话 就要把0xe0放在高16位 这次读的是低8位
	if(scancode==0xe0){
		ext_scancode=true;
		return;//设置ext_scancode直接返回  表明还有一次中断
	}
	if(ext_scancode){
		scancode=(0xe000|scancode);
		ext_scancode=false;
	}
	break_code=(  (scancode&0x0080)!=0 );//通过判断第7位来看是不是释放键盘
	if(break_code){
		//断码
		//释放键盘基本啥都不用做 除了 几个操作控制字符 shift alt crtl （caps和释放没关系 只和按的次数有关系）
		uint16_t make_code=(scancode&0xff7f);//拿到对应通码
		if(make_code==ctrl_l_make||make_code==ctrl_r_make){
			ctrl_status=false;
		}else if(make_code==shift_l_make||make_code==shift_r_make){
			shift_status=false;
		}else if(make_code==alt_l_make||make_code==alt_r_make){
			alt_status=false;
		}
		return;//释放按键做的事情比较少 
	}else if((scancode>0x00 &&scancode<0x3b)||(scancode==alt_r_make)||(scancode==ctrl_r_make)){//能够处理的按键
		//通码
		bool shift=false;//这里的shift不是说shift键是否被按下 而是是不是要转换 比如按下的a 要转换成A 也就是用keymap里的第0列还是第一列

		//下面对不同按键 的转换条件进行判断
		//非字母 只检查shift_status_last
		if(  (scancode<0x0e)||(scancode==0x29)||(scancode==0x1a)||(scancode==0x1b)||(scancode==0x2b)||(scancode==0x27)||(scancode==0x28)\
		||(scancode==0x33)||(scancode==0x34)||(scancode==0x35) ){
			if(shift_status_last){
				shift=true;//对于非字母 只看shift是不是被按下
			}
		}else{
			//对应字母
			if(shift_status_last&&caps_lock_status){
				shift=false;//caps灯亮 再按shift 再按字母 还是小写
			}else if(shift_status_last||caps_lock_status){
				shift=true;//如果caps灯亮 或者shift+字母 都是大写 惭愧 才知道 shift可以临时大写
			}else{
				shift=false;
			}
		}

		uint8_t index=(scancode&0x00ff);//拿到低8位（因为扩展按键 都是一样的作用一样的低8位 比如右shift的低8位和 一个字节的左shift一样）
		char cur_char=keymap[index][shift];
		if(cur_char){
			if(!ioq_full(&kbd_buf)){
				//put_char(cur_char);
				ioq_putchar(&kbd_buf,cur_char);
			}
			
			return ;
		}
		//能到这里说明对应的ascii 为0 可能是设置 操作控制字符
		if(scancode==ctrl_l_make||scancode==ctrl_r_make){
			ctrl_status=true;
		}else if(scancode==shift_l_make||scancode==shift_r_make){
			shift_status=true;
		}else if(scancode==alt_l_make||scancode==alt_r_make){
			alt_status=true;
		}else if(scancode==caps_lock_make){
			caps_lock_status=!caps_lock_status;//caps_locks比较特别
		}
		return ;
	}else{
		//到这里说明这个按键我们没法处理 
		//put_str("no this key\n");
	}
	return ;
}
void keyboard_init(void){
	put_str("keyboard_init start");
	ioqueue_init(&kbd_buf);
	register_handler(0x21,intr_keyboard_handler);
	put_str("keyboard_init done\n");
}
