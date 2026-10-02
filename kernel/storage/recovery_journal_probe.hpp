#pragma once
#include "recovery_journal.hpp"
namespace peregrinus::recovery_journal_probe {
struct Result{
    bool attempted;
    bool recovery_volume_present;
    bool a_read;
    bool b_read;
    bool write_test_attempted;
    bool write_test_passed;
    recovery_journal::Assessment assessment;
};
enum class CommitResult : unsigned char {
    disabled,
    recovery_unavailable,
    read_failed,
    no_baseline,
    split_brain,
    mark_rejected,
    write_failed,
    flush_failed,
    readback_failed,
    confirmed
};
Result run(const gpt::Guid& disk_guid);
CommitResult confirm_boot_success(const gpt::Guid& disk_guid,unsigned char slot,unsigned long long generation,unsigned long long epoch);
const char* commit_result_name(CommitResult);
}
