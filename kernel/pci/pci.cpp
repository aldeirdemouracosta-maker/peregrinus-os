#include "pci.hpp"
#include <peregrinus/io.hpp>
#include "../console/serial.hpp"
#include "../console/format.hpp"
namespace peregrinus::pci {
static Summary g_summary{};
static Device g_devices[128]{};
static uint32_t g_device_count=0;
static uint32_t read32(uint8_t bus,uint8_t slot,uint8_t func,uint16_t off){
    const uint32_t address=(1u<<31)|(uint32_t(bus)<<16)|(uint32_t(slot)<<11)|(uint32_t(func)<<8)|(off&0xFC);
    io::out32(0xCF8,address); return io::in32(0xCFC);
}
static uint16_t read16(uint8_t b,uint8_t s,uint8_t f,uint16_t off){uint32_t v=read32(b,s,f,off);return uint16_t(v>>((off&2)*8));}
static void write32(uint8_t bus,uint8_t slot,uint8_t func,uint16_t off,uint32_t value){
    const uint32_t address=(1u<<31)|(uint32_t(bus)<<16)|(uint32_t(slot)<<11)|(uint32_t(func)<<8)|(off&0xFC);
    io::out32(0xCF8,address); io::out32(0xCFC,value);
}
static bool selected_device(const Device& d){
    return d.class_code==0x01||d.class_code==0x02||d.class_code==0x03||d.vendor==0x1002||d.vendor==0x10de||(d.vendor==0x8086&&d.device==0x1d41);
}
BarInfo bar_info(const Device& d,uint8_t index){
    BarInfo out{};const uint8_t count=(d.header_type&0x7fu)==0?6:((d.header_type&0x7fu)==1?2:0);if(index>=count)return out;
    const uint16_t off=uint16_t(0x10u+index*4u);const uint32_t lo=read32(d.bus,d.slot,d.function,off);if(lo==0||lo==0xffffffffu)return out;
    if(lo&1u){out.address=uint64_t(lo&~3u);out.io=true;return out;}
    out.address=uint64_t(lo&~0x0fu);out.is64=((lo>>1)&3u)==2u;
    if(out.is64&&index+1<count){const uint32_t hi=read32(d.bus,d.slot,d.function,uint16_t(off+4));out.address|=uint64_t(hi)<<32;}
    return out;
}
static void print_device(const Device& d){
    char h[19];serial::write("PCI dev vendor=");format::hex64(d.vendor,h);serial::write(h);serial::write(" device=");format::hex64(d.device,h);serial::write(h);
    serial::write(" class=");format::hex64((uint64_t(d.class_code)<<16)|(uint64_t(d.subclass)<<8)|d.prog_if,h);serial::writeln(h);
}
Summary enumerate_readonly(){
    g_summary={};g_device_count=0;serial::writeln("PCI: compact selected-device catalog; legacy config I/O mode");
    for(uint16_t bus=0;bus<=255;++bus)for(uint8_t slot=0;slot<32;++slot){
        const uint16_t v0=read16(uint8_t(bus),slot,0,0);if(v0==0xffff)continue;
        const uint8_t header=uint8_t(read32(uint8_t(bus),slot,0,0x0c)>>16);const uint8_t maxf=(header&0x80)?8:1;
        for(uint8_t func=0;func<maxf;++func){
            const uint16_t vendor=read16(uint8_t(bus),slot,func,0);if(vendor==0xffff)continue;
            const uint16_t device=read16(uint8_t(bus),slot,func,2);const uint32_t cls=read32(uint8_t(bus),slot,func,0x08);
            Device d{};d.bus=uint8_t(bus);d.slot=slot;d.function=func;d.vendor=vendor;d.device=device;d.class_code=uint8_t(cls>>24);d.subclass=uint8_t(cls>>16);d.prog_if=uint8_t(cls>>8);d.header_type=uint8_t(read32(uint8_t(bus),slot,func,0x0c)>>16);
            ++g_summary.functions;if(vendor==0x1002)++g_summary.amd_devices;if(vendor==0x10de)++g_summary.nvidia_devices;if(d.class_code==0x03)++g_summary.display_devices;if(d.class_code==0x01)++g_summary.storage_devices;
            if(selected_device(d)&&g_device_count<128)g_devices[g_device_count++]=d;
            if(selected_device(d))print_device(d);
        }
        if(bus==255)break;
    }
    g_summary.recorded_devices=g_device_count;char n[24];format::dec64(g_summary.functions,n);serial::write("PCI functions: ");serial::writeln(n);format::dec64(g_device_count,n);serial::write("PCI selected devices retained: ");serial::writeln(n);return g_summary;
}
uint32_t config_read32(const Device& d,uint16_t off){return read32(d.bus,d.slot,d.function,off);}uint16_t config_read16(const Device& d,uint16_t off){return read16(d.bus,d.slot,d.function,off);}
uint16_t command_register(const Device& d){return config_read16(d,0x04);}bool memory_space_enabled(const Device& d){return (command_register(d)&(1u<<1))!=0;}
bool enable_memory_busmaster(const Device& d){uint32_t v=read32(d.bus,d.slot,d.function,0x04);const uint32_t nv=v|(1u<<1)|(1u<<2);if(nv!=v)write32(d.bus,d.slot,d.function,0x04,nv);return (read32(d.bus,d.slot,d.function,0x04)&((1u<<1)|(1u<<2)))==((1u<<1)|(1u<<2));}
const Summary& summary(){return g_summary;}const Device* devices(){return g_devices;}uint32_t device_count(){return g_device_count;}
}
