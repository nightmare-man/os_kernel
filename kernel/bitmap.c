#include "../lib/kernel/bitmap.h"
#include "../lib/kernel/stdint.h"
#include "../lib/string.h"
#include "../lib/kernel/debug.h"
#include "../lib/kernel/print.h"
void bitmap_init(struct bitmap* btmp){
	memset((void*)btmp->bits,0,btmp->btmp_bytes_len);
}
bool bitmap_scan_test(struct bitmap*btmp,uint32_t bit_idx){
	uint32_t byte_idx=bit_idx/8;
	uint32_t bit_odd=bit_idx%8;
	return ((btmp->bits[byte_idx] & (BITMAP_MASK<<bit_odd))==0?0:1);
}
int bitmap_scan(struct bitmap*btmp,uint32_t cnt){
	uint32_t byte_idx=0;
	uint32_t bit_idx=0;
	uint32_t start_bit=-1;
	uint32_t last_start_bit=-1;
	uint32_t i=0;
	uint32_t j=0;
	for(i=0;i<btmp->btmp_bytes_len*8;i++){
		if(bitmap_scan_test(btmp,i)==0){
			j=0;
			uint8_t flag=1;
			last_start_bit=i;
			for(j=0;j<cnt;j++){
				if(bitmap_scan_test(btmp,i+j)==1){
					flag=0;
					last_start_bit=i+j-1;
					break;
				}
			}
			if(flag){
				start_bit=i;
				break;
			}else{
				i=last_start_bit;//直接跳过中间已经检查的
			}
		}
	}
	return start_bit;//最后没找到就算了返回-1
}
void bitmap_set(struct bitmap*btmp,uint32_t bit_idx,int8_t value){
	ASSERT(value==0 || value==1);
	uint32_t byte_idx=bit_idx/8;
	uint32_t bit_odd=bit_idx%8;
	if(value){
		btmp->bits[byte_idx]=btmp->bits[byte_idx] | (BITMAP_MASK<<bit_odd);
	}else{
		btmp->bits[byte_idx]=btmp->bits[byte_idx] & (~(BITMAP_MASK<<bit_odd));
	}
}