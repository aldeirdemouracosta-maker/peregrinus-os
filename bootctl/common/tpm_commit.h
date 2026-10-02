#pragma once
#include <stdint.h>
#include <peregrinus/release_profile.h>

#ifndef PEREGRINUS_TPM_COMMIT_BASE
#define PEREGRINUS_TPM_COMMIT_BASE 0ull
#endif
#ifndef PEREGRINUS_TPM_COMMIT_NEXT
#define PEREGRINUS_TPM_COMMIT_NEXT 0ull
#endif
#ifndef PEREGRINUS_PRECOMMIT_MIN_EPOCH
#define PEREGRINUS_PRECOMMIT_MIN_EPOCH PEREGRINUS_RELEASE_MIN_SECURITY_EPOCH
#endif
#ifndef PEREGRINUS_POSTCOMMIT_MIN_EPOCH
#define PEREGRINUS_POSTCOMMIT_MIN_EPOCH PEREGRINUS_RELEASE_MIN_SECURITY_EPOCH
#endif
#ifndef PEREGRINUS_CURRENT_EPOCH
#define PEREGRINUS_CURRENT_EPOCH PEREGRINUS_RELEASE_CURRENT_EPOCH
#endif
#ifndef PEREGRINUS_LKG_EPOCH
#define PEREGRINUS_LKG_EPOCH PEREGRINUS_RELEASE_LKG_EPOCH
#endif

enum peregrinus_tpm_commit_state {
    PEREGRINUS_TPM_COMMIT_UNPROVISIONED = 0,
    PEREGRINUS_TPM_COMMIT_STALE,
    PEREGRINUS_TPM_COMMIT_STAGED,
    PEREGRINUS_TPM_COMMIT_COMMITTED,
    PEREGRINUS_TPM_COMMIT_FUTURE
};

static inline int peregrinus_tpm_commit_profile_valid(uint64_t base, uint64_t next) {
    return base > 0 && next == base + 1 && next > base;
}

static inline enum peregrinus_tpm_commit_state peregrinus_tpm_commit_classify(uint64_t counter,
                                                                               uint64_t base,
                                                                               uint64_t next) {
    if (!peregrinus_tpm_commit_profile_valid(base, next)) return PEREGRINUS_TPM_COMMIT_UNPROVISIONED;
    if (counter < base) return PEREGRINUS_TPM_COMMIT_STALE;
    if (counter == base) return PEREGRINUS_TPM_COMMIT_STAGED;
    if (counter == next) return PEREGRINUS_TPM_COMMIT_COMMITTED;
    return PEREGRINUS_TPM_COMMIT_FUTURE;
}

static inline uint64_t peregrinus_tpm_active_epoch_floor(enum peregrinus_tpm_commit_state state) {
    if (state == PEREGRINUS_TPM_COMMIT_STAGED) return PEREGRINUS_PRECOMMIT_MIN_EPOCH;
    if (state == PEREGRINUS_TPM_COMMIT_COMMITTED) return PEREGRINUS_POSTCOMMIT_MIN_EPOCH;
    return 0;
}

static inline int peregrinus_tpm_state_allows_lkg(enum peregrinus_tpm_commit_state state,
                                                   uint64_t current_epoch,
                                                   uint64_t lkg_epoch,
                                                   uint64_t requested_epoch) {
    if (state != PEREGRINUS_TPM_COMMIT_STAGED) return 0;
    const uint64_t floor = peregrinus_tpm_active_epoch_floor(state);
    if (!floor || current_epoch < floor || lkg_epoch < floor) return 0;
    return requested_epoch == lkg_epoch;
}
