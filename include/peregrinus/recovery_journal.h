#pragma once
#include <stdint.h>
#include <stddef.h>

#define PEREGRINUS_RECOVERY_JOURNAL_VERSION 2u
#define PEREGRINUS_RECOVERY_JOURNAL_MAX_ATTEMPTS 3u
#define PEREGRINUS_RJ_SLOT_CURRENT 0u
#define PEREGRINUS_RJ_SLOT_LKG 1u
#define PEREGRINUS_RJ_FLAG_VALID 0x1u
#define PEREGRINUS_RJ_FLAG_BOOT_SUCCESS 0x2u

#pragma pack(push, 1)
struct peregrinus_recovery_journal {
    uint8_t magic[8];
    uint32_t version;
    uint32_t struct_size;
    uint64_t sequence;
    uint8_t disk_guid[16];
    uint8_t recovery_guid[16];
    uint64_t current_generation;
    uint64_t lkg_generation;
    uint64_t current_epoch;
    uint64_t lkg_epoch;
    uint32_t current_attempts;
    uint32_t lkg_attempts;
    uint32_t max_attempts;
    uint8_t last_slot;
    uint8_t flags;
    uint8_t reserved[6];
    uint32_t crc32;
};
#pragma pack(pop)

static inline uint32_t peregrinus_rj_crc32_bytes(const void* data,size_t size){
    const uint8_t* p=(const uint8_t*)data;
    uint32_t c=0xffffffffu;
    for(size_t i=0;i<size;++i){
        c^=p[i];
        for(unsigned b=0;b<8;++b)c=(c>>1)^(0xedb88320u&(0u-(c&1u)));
    }
    return ~c;
}
static inline int peregrinus_rj_magic(const uint8_t m[8]){
    static const uint8_t k[8]={'P','G','R','J','N','L','0','2'};
    for(unsigned i=0;i<8;++i){
        if(m[i]!=k[i])return 0;
    }
    return 1;
}
static inline uint32_t peregrinus_rj_crc(const struct peregrinus_recovery_journal* j){
    struct peregrinus_recovery_journal t=*j;
    t.crc32=0;
    return peregrinus_rj_crc32_bytes(&t,sizeof(t));
}
static inline int peregrinus_rj_valid(const struct peregrinus_recovery_journal* j){
    if(!j||!peregrinus_rj_magic(j->magic))return 0;
    if(j->version!=PEREGRINUS_RECOVERY_JOURNAL_VERSION||j->struct_size!=sizeof(*j))return 0;
    if((j->flags&PEREGRINUS_RJ_FLAG_VALID)==0)return 0;
    if(j->max_attempts==0||j->max_attempts>16||j->last_slot>PEREGRINUS_RJ_SLOT_LKG)return 0;
    if(j->current_generation==0||j->lkg_generation==0||j->current_epoch==0||j->lkg_epoch==0)return 0;
    return peregrinus_rj_crc(j)==j->crc32;
}
static inline void peregrinus_rj_init(struct peregrinus_recovery_journal* j,const uint8_t disk_guid[16],const uint8_t recovery_guid[16],uint64_t cg,uint64_t lg,uint64_t ce,uint64_t le){
    static const uint8_t m[8]={'P','G','R','J','N','L','0','2'};
    for(size_t i=0;i<sizeof(*j);++i)((uint8_t*)j)[i]=0;
    for(unsigned i=0;i<8;++i)j->magic[i]=m[i];
    for(unsigned i=0;i<16;++i){
        j->disk_guid[i]=disk_guid[i];
        j->recovery_guid[i]=recovery_guid[i];
    }
    j->version=PEREGRINUS_RECOVERY_JOURNAL_VERSION;
    j->struct_size=sizeof(*j);
    j->sequence=1;
    j->current_generation=cg;
    j->lkg_generation=lg;
    j->current_epoch=ce;
    j->lkg_epoch=le;
    j->max_attempts=PEREGRINUS_RECOVERY_JOURNAL_MAX_ATTEMPTS;
    j->last_slot=PEREGRINUS_RJ_SLOT_CURRENT;
    j->flags=PEREGRINUS_RJ_FLAG_VALID;
    j->crc32=peregrinus_rj_crc(j);
}
static inline int peregrinus_rj_identity(const struct peregrinus_recovery_journal* j,const uint8_t disk_guid[16],const uint8_t recovery_guid[16]){
    if(!peregrinus_rj_valid(j))return 0;
    for(unsigned i=0;i<16;++i){
        if(j->disk_guid[i]!=disk_guid[i]||j->recovery_guid[i]!=recovery_guid[i])return 0;
    }
    return 1;
}
static inline uint8_t peregrinus_rj_choose_slot(const struct peregrinus_recovery_journal* j){
    if(!peregrinus_rj_valid(j))return PEREGRINUS_RJ_SLOT_CURRENT;
    if(j->current_attempts<j->max_attempts)return PEREGRINUS_RJ_SLOT_CURRENT;
    return PEREGRINUS_RJ_SLOT_LKG;
}
static inline int peregrinus_rj_prepare_attempt(struct peregrinus_recovery_journal* j,uint8_t slot){
    if(!peregrinus_rj_valid(j)||slot>PEREGRINUS_RJ_SLOT_LKG)return 0;
    uint32_t* attempts=slot==PEREGRINUS_RJ_SLOT_CURRENT?&j->current_attempts:&j->lkg_attempts;
    if(*attempts>=j->max_attempts)return 0;
    ++(*attempts);
    ++j->sequence;
    j->last_slot=slot;
    j->flags=(uint8_t)((j->flags|PEREGRINUS_RJ_FLAG_VALID)&~PEREGRINUS_RJ_FLAG_BOOT_SUCCESS);
    j->crc32=0;
    j->crc32=peregrinus_rj_crc(j);
    return 1;
}
static inline int peregrinus_rj_mark_success(struct peregrinus_recovery_journal* j,uint8_t slot,uint64_t generation,uint64_t epoch){
    if(!peregrinus_rj_valid(j)||slot>PEREGRINUS_RJ_SLOT_LKG||slot!=j->last_slot)return 0;
    // A success commit is only legal after pre-boot has recorded a pending attempt.
    // This prevents a kernel started outside the trusted controller path from
    // manufacturing a healthy boot record.
    if((j->flags&PEREGRINUS_RJ_FLAG_BOOT_SUCCESS)!=0)return 0;
    if(slot==PEREGRINUS_RJ_SLOT_CURRENT){
        if(generation!=j->current_generation||epoch!=j->current_epoch||j->current_attempts==0)return 0;
        j->current_attempts=0;
    }else{
        if(generation!=j->lkg_generation||epoch!=j->lkg_epoch||j->lkg_attempts==0)return 0;
        j->lkg_attempts=0;
    }
    ++j->sequence;
    j->flags=(uint8_t)(j->flags|PEREGRINUS_RJ_FLAG_VALID|PEREGRINUS_RJ_FLAG_BOOT_SUCCESS);
    j->crc32=0;
    j->crc32=peregrinus_rj_crc(j);
    return 1;
}
