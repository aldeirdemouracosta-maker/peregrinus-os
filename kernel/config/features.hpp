#pragma once
// Peregrinus OS safety gates (compile-time profile selection).
// The distributed build keeps all physical journal writes disabled. The only
// admitted persistent write path is the two-sector IA_RECOVERY journal commit,
// enabled explicitly for QEMU testing or hardware qualification.
// The local-LLM profile is experimental: it may not be combined with the live NIC profile or
// with any disk-write profile.
#if (defined(PEREGRINUS_LLM_LOCAL) && PEREGRINUS_LLM_LOCAL == 1) && \
    ((defined(PEREGRINUS_QEMU_E1000_SANDBOX) && PEREGRINUS_QEMU_E1000_SANDBOX == 1) || \
     (defined(PEREGRINUS_QEMU_RECOVERY_JOURNAL_TEST) && PEREGRINUS_QEMU_RECOVERY_JOURNAL_TEST == 1) || \
     (defined(PEREGRINUS_QEMU_RECOVERY_COMMIT_TEST) && PEREGRINUS_QEMU_RECOVERY_COMMIT_TEST == 1) || \
     (defined(PEREGRINUS_RECOVERY_COMMIT_LIVE) && PEREGRINUS_RECOVERY_COMMIT_LIVE == 1))
#error "Peregrinus: the local-LLM profile is exclusive of the e1000 and storage-write profiles"
#endif
#if ((defined(PEREGRINUS_QEMU_E1000_SANDBOX) && PEREGRINUS_QEMU_E1000_SANDBOX == 1) && \
     ((defined(PEREGRINUS_QEMU_DMA_TEST) && PEREGRINUS_QEMU_DMA_TEST == 1) || \
      (defined(PEREGRINUS_QEMU_RECOVERY_JOURNAL_TEST) && PEREGRINUS_QEMU_RECOVERY_JOURNAL_TEST == 1) || \
      (defined(PEREGRINUS_QEMU_RECOVERY_COMMIT_TEST) && PEREGRINUS_QEMU_RECOVERY_COMMIT_TEST == 1) || \
      (defined(PEREGRINUS_RECOVERY_COMMIT_LIVE) && PEREGRINUS_RECOVERY_COMMIT_LIVE == 1)))
#error "Peregrinus: e1000 qualification and storage-write qualification profiles are mutually exclusive"
#endif

namespace peregrinus::features {
#if (defined(PEREGRINUS_QEMU_DMA_TEST) && PEREGRINUS_QEMU_DMA_TEST == 1) || \
    (defined(PEREGRINUS_QEMU_RECOVERY_JOURNAL_TEST) && PEREGRINUS_QEMU_RECOVERY_JOURNAL_TEST == 1) || \
    (defined(PEREGRINUS_QEMU_RECOVERY_COMMIT_TEST) && PEREGRINUS_QEMU_RECOVERY_COMMIT_TEST == 1) || \
    (defined(PEREGRINUS_RECOVERY_COMMIT_LIVE) && PEREGRINUS_RECOVERY_COMMIT_LIVE == 1)
inline constexpr bool ahci_dma_read_live=true;
inline constexpr bool recovery_anchor_read_live=true;
#else
inline constexpr bool ahci_dma_read_live=false;
inline constexpr bool recovery_anchor_read_live=false;
#endif

#if (defined(PEREGRINUS_QEMU_DMA_TEST) && PEREGRINUS_QEMU_DMA_TEST == 1) || \
    (defined(PEREGRINUS_QEMU_RECOVERY_JOURNAL_TEST) && PEREGRINUS_QEMU_RECOVERY_JOURNAL_TEST == 1) || \
    (defined(PEREGRINUS_QEMU_RECOVERY_COMMIT_TEST) && PEREGRINUS_QEMU_RECOVERY_COMMIT_TEST == 1)
inline constexpr bool disposable_qemu_disk_only=true;
#else
inline constexpr bool disposable_qemu_disk_only=false;
#endif

#if defined(PEREGRINUS_QEMU_RECOVERY_JOURNAL_TEST) && PEREGRINUS_QEMU_RECOVERY_JOURNAL_TEST == 1
inline constexpr bool recovery_journal_read_live=true;
inline constexpr bool recovery_journal_write_test_live=true;
inline constexpr bool recovery_journal_commit_live=false;
#elif defined(PEREGRINUS_QEMU_RECOVERY_COMMIT_TEST) && PEREGRINUS_QEMU_RECOVERY_COMMIT_TEST == 1
inline constexpr bool recovery_journal_read_live=true;
inline constexpr bool recovery_journal_write_test_live=false;
inline constexpr bool recovery_journal_commit_live=true;
#elif defined(PEREGRINUS_RECOVERY_COMMIT_LIVE) && PEREGRINUS_RECOVERY_COMMIT_LIVE == 1
inline constexpr bool recovery_journal_read_live=true;
inline constexpr bool recovery_journal_write_test_live=false;
inline constexpr bool recovery_journal_commit_live=true;
#else
inline constexpr bool recovery_journal_read_live=false;
inline constexpr bool recovery_journal_write_test_live=false;
inline constexpr bool recovery_journal_commit_live=false;
#endif
inline constexpr bool recovery_journal_write_live=recovery_journal_write_test_live||recovery_journal_commit_live;
inline constexpr bool recovery_metadata_live=disposable_qemu_disk_only||recovery_journal_commit_live;
#if defined(PEREGRINUS_QEMU_E1000_SANDBOX) && PEREGRINUS_QEMU_E1000_SANDBOX == 1
inline constexpr bool e1000_dma32_required=true;
#else
inline constexpr bool e1000_dma32_required=false;
#endif
inline constexpr bool dma32_required=ahci_dma_read_live||recovery_journal_write_live||e1000_dma32_required;
// A build that can touch disk metadata must evaluate the disk/recovery policy; an
// unevaluated policy then halts. Builds without any storage path boot passively.
inline constexpr bool storage_policy_required=ahci_dma_read_live||recovery_metadata_live;

#if defined(PEREGRINUS_LLM_LOCAL) && PEREGRINUS_LLM_LOCAL == 1
inline constexpr bool llm_local=true;
#else
inline constexpr bool llm_local=false;
#endif

inline constexpr bool itco_watchdog_arm_live=false;
inline constexpr unsigned long long gpt_primary_metadata_max_lba=33ULL;
inline constexpr unsigned long long gpt_backup_metadata_sectors=33ULL;
inline constexpr unsigned ahci_max_sectors_per_command=1U;

// The production/SAFE build is passive. A live NIC datapath exists only in the explicit
// QEMU e1000 qualification profile.
#if defined(PEREGRINUS_QEMU_E1000_SANDBOX) && PEREGRINUS_QEMU_E1000_SANDBOX == 1
inline constexpr bool firewall_packet_hook_live=true;
inline constexpr bool nic_driver_live=true;
#else
inline constexpr bool firewall_packet_hook_live=false;
inline constexpr bool nic_driver_live=false;
#endif
}
