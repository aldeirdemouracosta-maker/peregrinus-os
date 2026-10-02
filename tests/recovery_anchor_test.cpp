#include "../kernel/storage/recovery_anchor.hpp"
#include <stdio.h>
int main(){
    if(!peregrinus::recovery_anchor::self_test()) return 1;
    puts("PASS: recovery anchor A/B selection host self-test.");
    return 0;
}
