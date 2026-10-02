#pragma once
#include <stdint.h>
#include <stddef.h>

#define PEREGRINUS_TPM_NV_INDEX 0x0180F050u

#define TPM_ST_NO_SESSIONS 0x8001u
#define TPM_ST_SESSIONS 0x8002u
#define TPM_CC_NV_READ 0x0000014Eu
#define TPM_CC_NV_READ_PUBLIC 0x00000169u
#define TPM_RS_PW 0x40000009u
#define TPM_RC_SUCCESS 0u
#define TPMA_NV_TPM_NT_MASK 0x000000F0u
#define TPM_NT_COUNTER_BITS 0x00000010u
#define TPMA_NV_OWNERWRITE 0x00000002u
#define TPMA_NV_AUTHWRITE 0x00000004u
#define TPMA_NV_AUTHREAD 0x00040000u
#define TPMA_NV_WRITTEN 0x20000000u

struct peregrinus_tpm_nv_public {
    uint32_t index;
    uint16_t name_alg;
    uint32_t attributes;
    uint16_t data_size;
    int valid;
};

static inline uint16_t pgr_be16(const uint8_t* p){return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);}
static inline uint32_t pgr_be32(const uint8_t* p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static inline uint64_t pgr_be64(const uint8_t* p){return ((uint64_t)pgr_be32(p)<<32)|pgr_be32(p+4);}
static inline void pgr_put16(uint8_t* p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static inline void pgr_put32(uint8_t* p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}

static inline size_t peregrinus_tpm_build_nv_read_public(uint8_t* out,size_t cap,uint32_t index){
    if(!out||cap<14)return 0;
    pgr_put16(out,TPM_ST_NO_SESSIONS);pgr_put32(out+2,14);pgr_put32(out+6,TPM_CC_NV_READ_PUBLIC);pgr_put32(out+10,index);return 14;
}

static inline int peregrinus_tpm_parse_nv_read_public(const uint8_t* in,size_t n,struct peregrinus_tpm_nv_public* out){
    if(!in||!out||n<12)return 0;
    const uint32_t total=pgr_be32(in+2),rc=pgr_be32(in+6);if(rc!=TPM_RC_SUCCESS||total>n||total<12)return 0;
    size_t pos=10;const uint16_t pub_size=pgr_be16(in+pos);pos+=2;if(pub_size<12||pos+pub_size>total)return 0;
    const size_t end=pos+pub_size;
    out->index=pgr_be32(in+pos);pos+=4;out->name_alg=pgr_be16(in+pos);pos+=2;out->attributes=pgr_be32(in+pos);pos+=4;
    const uint16_t policy=pgr_be16(in+pos);pos+=2;if(pos+policy+2>end)return 0;pos+=policy;out->data_size=pgr_be16(in+pos);
    out->valid=(out->index==PEREGRINUS_TPM_NV_INDEX)&&((out->attributes&TPMA_NV_TPM_NT_MASK)==TPM_NT_COUNTER_BITS)&&(out->attributes&TPMA_NV_AUTHREAD)&&(out->attributes&TPMA_NV_OWNERWRITE)&&((out->attributes&TPMA_NV_AUTHWRITE)==0)&&(out->data_size==8);
    return out->valid;
}

static inline size_t peregrinus_tpm_build_nv_read_counter(uint8_t* out,size_t cap,uint32_t index){
    /* AUTHREAD with empty authValue: password session contains empty nonce and HMAC. */
    if(!out||cap<35)return 0;
    pgr_put16(out,TPM_ST_SESSIONS);pgr_put32(out+2,35);pgr_put32(out+6,TPM_CC_NV_READ);
    pgr_put32(out+10,index);pgr_put32(out+14,index);pgr_put32(out+18,9);
    pgr_put32(out+22,TPM_RS_PW);pgr_put16(out+26,0);out[28]=0;pgr_put16(out+29,0);
    pgr_put16(out+31,8);pgr_put16(out+33,0);return 35;
}

static inline int peregrinus_tpm_parse_nv_read_counter(const uint8_t* in,size_t n,uint64_t* value){
    if(!in||!value||n<16)return 0;
    const uint32_t total=pgr_be32(in+2),rc=pgr_be32(in+6);
    if(rc!=TPM_RC_SUCCESS||total>n||total<16)return 0;
    size_t pos=10;if(pgr_be16(in)==TPM_ST_SESSIONS){if(total<16)return 0;const uint32_t param_size=pgr_be32(in+pos);pos+=4;if(param_size<10||pos+param_size>total)return 0;}
    const uint16_t bytes=pgr_be16(in+pos);pos+=2;if(bytes!=8||pos+8>total)return 0;*value=pgr_be64(in+pos);return 1;
}
