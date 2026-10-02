#include "../../panic/panic.hpp"
#include <stdint.h>
extern "C" [[noreturn]] void peregrinus_exception_dispatch(uint64_t vector,uint64_t error,uint64_t rip,uint64_t cs,uint64_t rflags){
    peregrinus::panic::exception(vector,error,rip,cs,rflags);
}
