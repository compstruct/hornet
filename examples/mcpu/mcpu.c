#include "rts.h"

int main() {
	print_int(cpu_id());
	print_string("\n");
	int i = 1 + 2 + 3;
	int j = i * i;
	print_int(j);
}
