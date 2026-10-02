#pragma once
#include <stdint.h>
#include <stddef.h>
#include <peregrinus/recovery_journal.h>

enum peregrinus_preboot_journal_state {
    PEREGRINUS_PREBOOT_NO_JOURNAL = 0,
    PEREGRINUS_PREBOOT_HEALTHY = 1,
    PEREGRINUS_PREBOOT_DEGRADED = 2,
    PEREGRINUS_PREBOOT_SPLIT_BRAIN = 3
};

struct peregrinus_preboot_journal_selection {
    enum peregrinus_preboot_journal_state state;
    struct peregrinus_recovery_journal journal;
    uint8_t active_copy;
};

static inline int peregrinus_preboot_bytes_equal(const void* a,const void* b,size_t n){
    const uint8_t* x=(const uint8_t*)a;
    const uint8_t* y=(const uint8_t*)b;
    for(size_t i=0;i<n;++i){
        if(x[i]!=y[i])return 0;
    }
    return 1;
}

static inline int peregrinus_preboot_profile_matches(const struct peregrinus_recovery_journal* j,
                                                      uint64_t cg,uint64_t lg,uint64_t ce,uint64_t le){
    return peregrinus_rj_valid(j)&&j->current_generation==cg&&j->lkg_generation==lg&&j->current_epoch==ce&&j->lkg_epoch==le;
}

static inline struct peregrinus_preboot_journal_selection peregrinus_preboot_select(
    const struct peregrinus_recovery_journal* a,int va,
    const struct peregrinus_recovery_journal* b,int vb){
    struct peregrinus_preboot_journal_selection r={0};
    r.state=PEREGRINUS_PREBOOT_NO_JOURNAL;
    if(va&&vb){
        if(a->sequence==b->sequence&&!peregrinus_preboot_bytes_equal(a,b,sizeof(*a))){
            r.state=PEREGRINUS_PREBOOT_SPLIT_BRAIN;
            return r;
        }
        if(a->sequence>=b->sequence){r.journal=*a;r.active_copy=0;}else{r.journal=*b;r.active_copy=1;}
        r.state=PEREGRINUS_PREBOOT_HEALTHY;
        return r;
    }
    if(va){r.state=PEREGRINUS_PREBOOT_DEGRADED;r.journal=*a;r.active_copy=0;return r;}
    if(vb){r.state=PEREGRINUS_PREBOOT_DEGRADED;r.journal=*b;r.active_copy=1;return r;}
    return r;
}

static inline uint8_t peregrinus_preboot_inactive_copy(uint8_t active_copy){return active_copy?0u:1u;}
