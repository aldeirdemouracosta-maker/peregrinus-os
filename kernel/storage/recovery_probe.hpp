#pragma once
#include <stdint.h>
#include "gpt.hpp"
#include "recovery_anchor.hpp"

namespace peregrinus::recovery_probe {

struct Result {
    bool attempted;
    bool recovery_volume_present;
    bool first_read;
    bool last_read;
    bool first_layout_ok;
    bool last_layout_ok;
    recovery_anchor::Assessment assessment;
};

Result run(const gpt::Guid& disk_guid);

}
