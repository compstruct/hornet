/* -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-  */
/* vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0: */

#include <rts.h>

int main(int argc, char **argv) {
    int a[2];
    a[0] = 5;
    a[1] = 6;
    print_string("hello from cpu ");
    print_int(cpu_id());
    print_string(" ");
    print_int(a[0]);
    print_string(", ");    
    print_int(a[1]);
    print_string("\n");
}

