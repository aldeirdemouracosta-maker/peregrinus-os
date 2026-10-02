#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <peregrinus/recovery_journal.h>
#include "../bootctl/common/preboot_recovery.h"

static void guid(uint8_t g[16],uint8_t base){for(unsigned i=0;i<16;++i)g[i]=(uint8_t)(base+i);}

int main(void){
    uint8_t dg[16],rg[16];guid(dg,1);guid(rg,33);
    struct peregrinus_recovery_journal a={0},b={0};
    peregrinus_rj_init(&a,dg,rg,10,9,3,3);b=a;
    if(!peregrinus_preboot_profile_matches(&a,10,9,3,3))return 1;
    struct peregrinus_preboot_journal_selection s=peregrinus_preboot_select(&a,1,&b,1);
    if(s.state!=PEREGRINUS_PREBOOT_HEALTHY||s.active_copy!=0)return 2;
    if(!peregrinus_rj_prepare_attempt(&b,PEREGRINUS_RJ_SLOT_CURRENT))return 3;
    s=peregrinus_preboot_select(&a,1,&b,1);
    if(s.state!=PEREGRINUS_PREBOOT_HEALTHY||s.active_copy!=1||s.journal.sequence!=2)return 4;
    s=peregrinus_preboot_select(&a,1,&b,0);
    if(s.state!=PEREGRINUS_PREBOOT_DEGRADED||s.active_copy!=0)return 5;
    b=a;b.current_attempts=1;b.crc32=peregrinus_rj_crc(&b);
    s=peregrinus_preboot_select(&a,1,&b,1);
    if(s.state!=PEREGRINUS_PREBOOT_SPLIT_BRAIN)return 6;
    uint8_t bad[16];guid(bad,90);if(peregrinus_rj_identity(&a,dg,bad))return 7;
    if(peregrinus_preboot_inactive_copy(0)!=1||peregrinus_preboot_inactive_copy(1)!=0)return 8;
    puts("PASS: pre-boot recovery journal selection, degraded repair target, split-brain rejection and profile binding");
    return 0;
}
