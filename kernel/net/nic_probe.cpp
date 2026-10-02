#include "nic_probe.hpp"
#include "../console/serial.hpp"
#include "../console/format.hpp"

namespace peregrinus::net::nic {
namespace {

bool ethernet_class(const pci::Device& d) {
    return d.class_code == 0x02 && d.subclass == 0x00;
}

void print_device(const pci::Device& d, Candidate c) {
    char h[19];
    serial::write("NIC probe vendor=");
    format::hex64(d.vendor, h);
    serial::write(h);
    serial::write(" device=");
    format::hex64(d.device, h);
    serial::write(h);
    serial::write(" candidate=");
    serial::writeln(candidate_name(c));
}

}

Candidate classify(const pci::Device& d) {
    if (!ethernet_class(d)) return Candidate::none;
    if (d.vendor == 0x8086 && d.device == 0x100e) return Candidate::qemu_e1000;
    if (d.vendor == 0x1af4 && d.device == 0x1000) return Candidate::virtio_legacy;
    if (d.vendor == 0x1af4 && d.device == 0x1041) return Candidate::virtio_modern;
    return Candidate::ethernet_generic;
}

Summary probe_readonly() {
    Summary s{};
    const pci::Device* ds = pci::devices();
    for (uint32_t i = 0; i < pci::device_count(); ++i) {
        const Candidate c = classify(ds[i]);
        if (c == Candidate::none) continue;
        ++s.ethernet_controllers;
        if (c == Candidate::qemu_e1000) ++s.qemu_e1000;
        if (c == Candidate::virtio_legacy || c == Candidate::virtio_modern) ++s.virtio;
        print_device(ds[i], c);
    }
    if (s.ethernet_controllers == 0) serial::writeln("NIC probe: no Ethernet controller recorded");
    serial::writeln("NIC probe: read-only; BAR/MMIO/DMA/interrupt setup BLOCKED");
    return s;
}

const char* candidate_name(Candidate candidate) {
    switch (candidate) {
        case Candidate::none: return "NONE";
        case Candidate::qemu_e1000: return "QEMU-E1000-82540EM";
        case Candidate::virtio_legacy: return "VIRTIO-NET-LEGACY";
        case Candidate::virtio_modern: return "VIRTIO-NET-MODERN";
        case Candidate::ethernet_generic: return "ETHERNET-GENERIC";
    }
    return "UNKNOWN";
}

bool self_test() {
    pci::Device d{};
    d.class_code = 0x02;
    d.subclass = 0x00;
    d.vendor = 0x8086;
    d.device = 0x100e;
    if (classify(d) != Candidate::qemu_e1000) return false;
    d.vendor = 0x1af4;
    d.device = 0x1000;
    if (classify(d) != Candidate::virtio_legacy) return false;
    d.device = 0x1041;
    if (classify(d) != Candidate::virtio_modern) return false;
    d.vendor = 0x10ec;
    d.device = 0x8168;
    if (classify(d) != Candidate::ethernet_generic) return false;
    d.class_code = 0x01;
    if (classify(d) != Candidate::none) return false;
    return true;
}

}
