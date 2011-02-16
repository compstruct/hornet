#include "rts.h"

int b;
int main() {
	int a = 12;
	b = a * 2;
	print_int(b);
	__H_enable_memory_hierarchy();
	print_int(b);
}


