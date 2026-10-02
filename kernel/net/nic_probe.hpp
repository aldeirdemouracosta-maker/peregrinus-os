#pragma once
#include <stdint.h>
#include "../pci/pci.hpp"

namespace peregrinus::net::nic {

enum class Candidate : uint8_t {
    none = 0,
    qemu_e1000,
    virtio_legacy,
    virtio_modern,
    ethernet_generic,
};

struct Summary {
    uint32_t ethernet_controllers;
    uint32_t qemu_e1000;
    uint32_t virtio;
};

Candidate classify(const pci::Device& device);
Summary probe_readonly();
const char* candidate_name(Candidate candidate);
bool self_test();

}
