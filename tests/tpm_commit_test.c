#include <stdio.h>
#define PEREGRINUS_TPM_COMMIT_BASE 41ull
#define PEREGRINUS_TPM_COMMIT_NEXT 42ull
#define PEREGRINUS_PRECOMMIT_MIN_EPOCH 2ull
#define PEREGRINUS_POSTCOMMIT_MIN_EPOCH 3ull
#define PEREGRINUS_CURRENT_EPOCH 3ull
#define PEREGRINUS_LKG_EPOCH 2ull
#include "../bootctl/common/tpm_commit.h"
int main(void){
    if(peregrinus_tpm_commit_classify(40,41,42)!=PEREGRINUS_TPM_COMMIT_STALE)return 1;
    if(peregrinus_tpm_commit_classify(41,41,42)!=PEREGRINUS_TPM_COMMIT_STAGED)return 2;
    if(peregrinus_tpm_commit_classify(42,41,42)!=PEREGRINUS_TPM_COMMIT_COMMITTED)return 3;
    if(peregrinus_tpm_commit_classify(43,41,42)!=PEREGRINUS_TPM_COMMIT_FUTURE)return 4;
    if(peregrinus_tpm_commit_classify(1,0,0)!=PEREGRINUS_TPM_COMMIT_UNPROVISIONED)return 5;
    if(peregrinus_tpm_active_epoch_floor(PEREGRINUS_TPM_COMMIT_STAGED)!=2)return 6;
    if(peregrinus_tpm_active_epoch_floor(PEREGRINUS_TPM_COMMIT_COMMITTED)!=3)return 7;
    if(!peregrinus_tpm_state_allows_lkg(PEREGRINUS_TPM_COMMIT_STAGED,3,2,2))return 8;
    if(peregrinus_tpm_state_allows_lkg(PEREGRINUS_TPM_COMMIT_COMMITTED,3,2,2))return 9;
    puts("PASS: TPM absolute-counter staged/committed transition policy");return 0;
}
