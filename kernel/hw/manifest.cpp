#include "manifest.hpp"
#include "../console/serial.hpp"
#include "../console/format.hpp"
#include "../console/framebuffer.hpp"
#include "../mm/memory.hpp"
#include "../pci/pci.hpp"
#include "../mm/mmio.hpp"
#include "../acpi/acpi.hpp"

namespace peregrinus::hw {
static Manifest g{};
static void copy_vendor(char out[13]) {
    unsigned eax=0,ebx=0,ecx=0,edx=0;
    asm volatile("cpuid":"+a"(eax),"=b"(ebx),"=c"(ecx),"=d"(edx));
    const uint32_t w[3]={ebx,edx,ecx};
    for(int i=0;i<12;++i)out[i]=char(w[i/4]>>(8*(i%4)));
    out[12]=0;
}
void init(){
    copy_vendor(g.cpu_vendor);
    g.usable_memory_bytes=memory::summary().usable_bytes;
    const auto& p=pci::summary();
    g.pci_functions=p.functions; g.amd_devices=p.amd_devices; g.nvidia_devices=p.nvidia_devices;
    g.display_devices=p.display_devices; g.storage_devices=p.storage_devices; g.mmio_mappings=mmio::status().mappings;
    g.framebuffer_ready=framebuffer::ready();
    g.rsdp_present=acpi::rsdp_present(); g.mcfg_present=acpi::mcfg_present(); g.madt_present=acpi::madt_present();
    if(g.madt_present){ g.local_apics=acpi::madt().local_apics; g.io_apics=acpi::madt().io_apics; }
}
const Manifest& manifest(){ return g; }
void print(){
    char n[24];
    serial::writeln("Hardware Manifest:");
    serial::write("  CPU vendor: "); serial::writeln(g.cpu_vendor);
    format::dec64(g.usable_memory_bytes/(1024*1024),n); serial::write("  Usable RAM MiB: "); serial::writeln(n);
    format::dec64(g.pci_functions,n); serial::write("  PCI functions: "); serial::writeln(n);
    format::dec64(g.amd_devices,n); serial::write("  AMD devices: "); serial::writeln(n);
    format::dec64(g.nvidia_devices,n); serial::write("  NVIDIA devices: "); serial::writeln(n);
    format::dec64(g.display_devices,n); serial::write("  Display devices: "); serial::writeln(n);
    format::dec64(g.storage_devices,n); serial::write("  Storage devices: "); serial::writeln(n);
    format::dec64(g.mmio_mappings,n); serial::write("  Peregrinus MMIO mappings: "); serial::writeln(n);
    serial::write("  Framebuffer: "); serial::writeln(g.framebuffer_ready?"yes":"no");
    serial::write("  ACPI RSDP: "); serial::writeln(g.rsdp_present?"yes":"no");
    serial::write("  ACPI MCFG: "); serial::writeln(g.mcfg_present?"yes":"no");
    serial::write("  ACPI MADT: "); serial::writeln(g.madt_present?"yes":"no");
    format::dec64(g.local_apics,n); serial::write("  LAPIC CPUs: "); serial::writeln(n);
    format::dec64(g.io_apics,n); serial::write("  IOAPICs: "); serial::writeln(n);
}
}
