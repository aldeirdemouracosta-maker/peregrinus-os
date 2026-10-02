#include <stdio.h>
#include <string.h>
#define PEREGRINUS_MIN_SECURITY_EPOCH 2ull
#include "../bootctl/common/boot_request.h"
static struct peregrinus_boot_request make(uint8_t choice, uint64_t epoch){
    struct peregrinus_boot_request r={0}; const char m[8]={'P','G','R','R','E','Q','0','1'}; memcpy(r.magic,m,8);
    r.version=PEREGRINUS_BOOT_REQUEST_VERSION;r.choice=choice;r.requested_security_epoch=epoch;r.generation_hint=2;r.crc32=peregrinus_request_crc(&r);return r;
}
int main(void){
    struct peregrinus_boot_request a=make(PEREGRINUS_BOOT_LKG,2);
    if(!peregrinus_request_valid(&a)||!peregrinus_request_allows_lkg_floor(&a,3,2,2)) return 1;
    struct peregrinus_boot_request b=a;b.requested_security_epoch=1;b.crc32=peregrinus_request_crc(&b);if(peregrinus_request_valid(&b)) return 2;
    struct peregrinus_boot_request c=a;c.choice=9;c.crc32=peregrinus_request_crc(&c);if(peregrinus_request_valid(&c)) return 3;
    struct peregrinus_boot_request d=a;((unsigned char*)&d)[0]^=1;if(peregrinus_request_valid(&d)) return 4;
    if(!peregrinus_request_allows_lkg_floor(&a,3,2,2)) return 5;
    if(peregrinus_request_allows_lkg_floor(&a,3,2,3)) return 6;
    puts("PASS: trusted boot request validation, active epoch floor, and staged cross-epoch LKG clamp");return 0;
}
