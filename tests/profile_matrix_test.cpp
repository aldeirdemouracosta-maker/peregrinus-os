// Compile-time check of every build profile's capability flags. Each profile is compiled with
// its macros (see tests/profile_matrix.sh) and must satisfy the static_asserts for that profile.
#include "config/features.hpp"
namespace f = peregrinus::features;
static_assert(!f::itco_watchdog_arm_live, "no profile arms the physical iTCO watchdog");
#if defined(EXPECT_SAFE)
static_assert(!f::ahci_dma_read_live && !f::recovery_journal_read_live && !f::recovery_journal_write_live, "SAFE: no disk access");
static_assert(!f::nic_driver_live && !f::firewall_packet_hook_live && !f::dma32_required, "SAFE: no NIC, no DMA32");
static_assert(!f::storage_policy_required, "SAFE boots passively");
#elif defined(EXPECT_E1000)
static_assert(f::nic_driver_live && f::firewall_packet_hook_live && f::dma32_required, "e1000: live NIC");
static_assert(!f::ahci_dma_read_live && !f::recovery_journal_write_live && !f::storage_policy_required, "e1000: never touches storage");
#elif defined(EXPECT_RECOVERY_LIVE)
static_assert(f::ahci_dma_read_live && f::recovery_journal_read_live && f::recovery_journal_commit_live, "recovery-live: reads GPT/journal and commits");
static_assert(!f::recovery_journal_write_test_live && !f::disposable_qemu_disk_only, "recovery-live: no destructive write test");
static_assert(!f::nic_driver_live && f::storage_policy_required, "recovery-live: no NIC, disk policy mandatory");
#elif defined(EXPECT_RECOVERY_COMMIT_TEST)
static_assert(f::ahci_dma_read_live && f::recovery_journal_commit_live && f::disposable_qemu_disk_only && f::storage_policy_required, "QEMU commit test");
#else
#error "no EXPECT_* profile selected"
#endif
int main() { return 0; }
