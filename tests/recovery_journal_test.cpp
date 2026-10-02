#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <peregrinus/recovery_journal.h>
static void guid(uint8_t g[16],uint8_t base){for(unsigned i=0;i<16;++i)g[i]=(uint8_t)(base+i);}
int main(){
    uint8_t dg[16],rg[16];guid(dg,1);guid(rg,33);peregrinus_recovery_journal a{},b{};peregrinus_rj_init(&a,dg,rg,8,7,3,3);if(!peregrinus_rj_valid(&a)||!peregrinus_rj_identity(&a,dg,rg))return 1;
    b=a;if(!peregrinus_rj_prepare_attempt(&b,PEREGRINUS_RJ_SLOT_CURRENT)||b.current_attempts!=1||b.sequence!=2)return 2;
    // Simulated torn write: only half of B reaches media, so its CRC must fail and A survives.
    peregrinus_recovery_journal torn{};memcpy(&torn,&b,sizeof(b)/2);if(peregrinus_rj_valid(&torn))return 3;if(!peregrinus_rj_valid(&a))return 4;
    // Full B is durable, then a success record resets CURRENT attempts.
    if(!peregrinus_rj_valid(&b))return 5;if(!peregrinus_rj_mark_success(&b,PEREGRINUS_RJ_SLOT_CURRENT,8,3)||b.current_attempts!=0||(b.flags&PEREGRINUS_RJ_FLAG_BOOT_SUCCESS)==0)return 6;
    // Three failed CURRENT attempts trigger LKG selection.
    for(unsigned i=0;i<3;++i)if(!peregrinus_rj_prepare_attempt(&b,PEREGRINUS_RJ_SLOT_CURRENT))return 7;if(peregrinus_rj_choose_slot(&b)!=PEREGRINUS_RJ_SLOT_LKG)return 8;
    uint8_t badrg[16];guid(badrg,80);if(peregrinus_rj_identity(&b,dg,badrg))return 9;
    puts("PASS: recovery journal A/B record, torn-write survival, success reset and fallback policy");return 0;
}
