#include "../kernel/storage/boot_policy.hpp"
#include <stdio.h>
int main(){
    if(!peregrinus::boot_policy::self_test()) return 1;
    puts("PASS: Noe 1.0 boot policy host self-test.");
    return 0;
}
