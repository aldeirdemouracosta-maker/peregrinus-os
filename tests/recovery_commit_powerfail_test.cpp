#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <peregrinus/recovery_journal.h>

static void guid(uint8_t g[16],uint8_t base){for(unsigned i=0;i<16;++i)g[i]=(uint8_t)(base+i);}
static int same(const peregrinus_recovery_journal& a,const peregrinus_recovery_journal& b){return memcmp(&a,&b,sizeof(a))==0;}
static int select(const peregrinus_recovery_journal& a,const peregrinus_recovery_journal& b,peregrinus_recovery_journal* out){
    const int va=peregrinus_rj_valid(&a),vb=peregrinus_rj_valid(&b);
    if(va&&vb&&a.sequence==b.sequence&&!same(a,b))return -2;
    if(va&&(!vb||a.sequence>=b.sequence)){*out=a;return 0;}
    if(vb){*out=b;return 1;}
    return -1;
}
static bool pending_current(const peregrinus_recovery_journal& j){return peregrinus_rj_valid(&j)&&j.current_attempts==1&&(j.flags&PEREGRINUS_RJ_FLAG_BOOT_SUCCESS)==0;}
static bool successful_current(const peregrinus_recovery_journal& j){return peregrinus_rj_valid(&j)&&j.current_attempts==0&&(j.flags&PEREGRINUS_RJ_FLAG_BOOT_SUCCESS)!=0;}
int main(){
    uint8_t dg[16],rg[16];guid(dg,1);guid(rg,33);
    peregrinus_recovery_journal clean{};peregrinus_rj_init(&clean,dg,rg,10,9,3,3);
    // Build a clean committed baseline, then the controller records a pending attempt.
    if(!peregrinus_rj_prepare_attempt(&clean,PEREGRINUS_RJ_SLOT_CURRENT))return 1;
    if(!peregrinus_rj_mark_success(&clean,PEREGRINUS_RJ_SLOT_CURRENT,10,3))return 2;
    peregrinus_recovery_journal pending=clean;
    if(!peregrinus_rj_prepare_attempt(&pending,PEREGRINUS_RJ_SLOT_CURRENT)||!pending_current(pending))return 3;
    peregrinus_recovery_journal success=pending;
    if(!peregrinus_rj_mark_success(&success,PEREGRINUS_RJ_SLOT_CURRENT,10,3)||!successful_current(success))return 4;

    // No pending attempt means no legal success commit.
    peregrinus_recovery_journal forged=clean;
    if(peregrinus_rj_mark_success(&forged,PEREGRINUS_RJ_SLOT_CURRENT,10,3))return 5;

    // Media starts with A=last clean success, B=pending attempt (B active).
    peregrinus_recovery_journal a=clean,b=pending,chosen{};
    if(select(a,b,&chosen)!=1||!pending_current(chosen))return 6;

    // Crash point 1: after computing success, before any write. Pending B survives.
    if(select(a,b,&chosen)!=1||!pending_current(chosen))return 7;

    // Crash point 2: torn A write. A becomes invalid, pending B still survives.
    peregrinus_recovery_journal torn{};memcpy(&torn,&success,sizeof(success)/2);
    if(peregrinus_rj_valid(&torn))return 8;
    if(select(torn,b,&chosen)!=1||!pending_current(chosen))return 9;

    // Crash point 3: after WriteBlocks/WRITE DMA but before flush. Storage may keep
    // old A or the complete new A; both outcomes are safe and self-consistent.
    peregrinus_recovery_journal old_a=clean,new_a=success;
    if(select(old_a,b,&chosen)!=1||!pending_current(chosen))return 10;
    if(select(new_a,b,&chosen)!=0||!successful_current(chosen))return 11;

    // Crash point 4: after flush, before readback. New A must be a valid winner.
    a=success;if(select(a,b,&chosen)!=0||!successful_current(chosen))return 12;

    // Crash point 5: after readback/validation is identical to completed commit.
    if(select(a,b,&chosen)!=0||!successful_current(chosen)||chosen.sequence!=success.sequence)return 13;

    // A same-sequence divergence is split-brain and must never auto-select.
    peregrinus_recovery_journal split=success;split.current_attempts=1;split.crc32=0;split.crc32=peregrinus_rj_crc(&split);
    if(select(success,split,&chosen)!=-2)return 14;

    puts("PASS: recovery success commit survives all modeled power-loss points and rejects forged/no-attempt success");
    return 0;
}
