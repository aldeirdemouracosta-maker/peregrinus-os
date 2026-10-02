#include <stdio.h>
#include <string.h>
#include "../bootctl/common/tpm_monotonic.h"
static void be16(unsigned char*p,unsigned v){p[0]=v>>8;p[1]=v;}
static void be32(unsigned char*p,unsigned v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
static void be64(unsigned char*p,unsigned long long v){be32(p,(unsigned)(v>>32));be32(p+4,(unsigned)v);}
int main(void){
    unsigned char cmd[64]={0};
    if(peregrinus_tpm_build_nv_read_public(cmd,sizeof(cmd),PEREGRINUS_TPM_NV_INDEX)!=14)return 1;
    if(pgr_be16(cmd)!=TPM_ST_NO_SESSIONS||pgr_be32(cmd+6)!=TPM_CC_NV_READ_PUBLIC)return 2;
    unsigned char pub[64]={0};be16(pub,TPM_ST_NO_SESSIONS);be32(pub+2,28);be32(pub+6,0);be16(pub+10,14);be32(pub+12,PEREGRINUS_TPM_NV_INDEX);be16(pub+16,0x000B);be32(pub+18,TPM_NT_COUNTER_BITS|TPMA_NV_OWNERWRITE|TPMA_NV_AUTHREAD|TPMA_NV_WRITTEN);be16(pub+22,0);be16(pub+24,8);be16(pub+26,0);
    struct peregrinus_tpm_nv_public p={0};if(!peregrinus_tpm_parse_nv_read_public(pub,sizeof(pub),&p)||!p.valid||p.data_size!=8)return 3;
    if(peregrinus_tpm_build_nv_read_counter(cmd,sizeof(cmd),PEREGRINUS_TPM_NV_INDEX)!=35)return 4;
    if(pgr_be16(cmd)!=TPM_ST_SESSIONS||pgr_be32(cmd+6)!=TPM_CC_NV_READ)return 5;
    unsigned char rsp[64]={0};be16(rsp,TPM_ST_SESSIONS);be32(rsp+2,29);be32(rsp+6,0);be32(rsp+10,10);be16(rsp+14,8);be64(rsp+16,2); /* trailing empty auth response */
    uint64_t value=0;if(!peregrinus_tpm_parse_nv_read_counter(rsp,sizeof(rsp),&value)||value!=2)return 6;
    if(value!=2)return 7;
    pub[21]^=0x02;if(peregrinus_tpm_parse_nv_read_public(pub,sizeof(pub),&p))return 8;
    pub[21]^=0x02; pub[21]|=0x04;if(peregrinus_tpm_parse_nv_read_public(pub,sizeof(pub),&p))return 9;
    puts("PASS: TPM2 NV counter marshalling, parsing, OWNERWRITE-only commit authorization, and read policy");return 0;
}
