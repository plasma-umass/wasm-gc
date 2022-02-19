#include<stdio.h>
#include<stdlib.h>

int aaa=123;

void* __malloc(size_t sz);

__attribute__((noinline)) int f(int i) {
	int s = 0;
	for (int j = 0; j < i; j++) {
		s += j + aaa;
		aaa = s;
	}
	return s;
}
int main() 
__attribute__((export_name("main"))) ;

int main()
{
	int N = 100; ((rand()%10+9)/10)*10;
	int* mem = (int*)__malloc(N);
	// scanf("%d", &aaa);
	int s = 0;
	for (int i = 0; i < N; i++) {
		s += f(i) + aaa;
		mem[i] = s;
	}
	printf("s %d %p %d %p\n", s, &s, mem[rand()%4], mem);

	return 0;
//printf("First address past:\n");
  //  printf("    program text (etext)      %10p\n", &etext);
    //printf("    initialized data (edata)  %10p\n", &edata);
    //printf("    uninitialized data (end)  %10p\n", &end);
}
