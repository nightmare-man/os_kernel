//posix线程测试例子 编译需要加上-lpthread

#include <stdio.h>
#include <pthread.h>
void*thread_func(void*_arg){
	unsigned int*arg=_arg;
	printf("NEW THREAD my tid is:%u\n",*arg);
}
void main(){
	pthread_t new_thread_id;
	pthread_create(&new_thread_id,NULL,thread_func,&new_thread_id);
	printf("main thread,my tid is:%u\n",pthread_self());
	usleep(100);
}