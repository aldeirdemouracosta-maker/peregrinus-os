#pragma once
#include <stdint.h>

namespace peregrinus::spin {
// Polls done() until it returns true or the budget is exhausted. The budget is checked
// before it is decremented, so it can never wrap around and report a timeout as success.
// Returns true only when done() was observed true.
template<class Done> bool until(Done done,uint32_t budget){
    for(;;){
        if(done())return true;
        if(budget==0)return false;
        --budget;
        __builtin_ia32_pause();
    }
}
}
