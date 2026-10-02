#include "acpi.hpp"
#include "../console/serial.hpp"
#include "../console/format.hpp"
#include "../../third_party/limine/limine_min.h"

namespace peregrinus::acpi {
struct __attribute__((packed)) Rsdp20 { char sig[8]; uint8_t checksum; char oem[6]; uint8_t revision; uint32_t rsdt; uint32_t length; uint64_t xsdt; uint8_t ext_checksum; uint8_t reserved[3]; };
struct __attribute__((packed)) SdtHeader { char sig[4]; uint32_t length; uint8_t revision; uint8_t checksum; char oemid[6]; char oemtableid[8]; uint32_t oemrev; uint32_t creatorid; uint32_t creatorrev; };
struct __attribute__((packed)) McfgAllocation { uint64_t base; uint16_t segment; uint8_t start_bus; uint8_t end_bus; uint32_t reserved; };
struct __attribute__((packed)) MadtHeader { SdtHeader header; uint32_t lapic_address; uint32_t flags; };
struct __attribute__((packed)) MadtEntryHeader { uint8_t type; uint8_t length; };

static bool g_rsdp=false, g_mcfg=false, g_madt=false;
static uint64_t g_hhdm=0;
static McfgInfo g_mcfg_info{};
static MadtInfo g_madt_info{};

static bool sig4(const char* a,const char* b){ return a[0]==b[0]&&a[1]==b[1]&&a[2]==b[2]&&a[3]==b[3]; }
static bool checksum(const void* p,uint32_t n){ const auto* b=(const uint8_t*)p; uint8_t s=0; for(uint32_t i=0;i<n;++i)s=uint8_t(s+b[i]); return s==0; }
static const SdtHeader* phys_sdt(uint64_t phys){ return reinterpret_cast<const SdtHeader*>(static_cast<uintptr_t>(phys + g_hhdm)); }

static void parse_mcfg(const SdtHeader* h){
    if(h->length < sizeof(SdtHeader)+8+sizeof(McfgAllocation)) return;
    auto* a=reinterpret_cast<const McfgAllocation*>(reinterpret_cast<const uint8_t*>(h)+sizeof(SdtHeader)+8);
    g_mcfg_info={a->base,a->segment,a->start_bus,a->end_bus};
    g_mcfg=true;
    char hbuf[19]; format::hex64(g_mcfg_info.ecam_base,hbuf);
    serial::write("ACPI MCFG ECAM base: "); serial::writeln(hbuf);
}

static void parse_madt(const SdtHeader* h){
    if(h->length < sizeof(MadtHeader)) return;
    auto* m=reinterpret_cast<const MadtHeader*>(h);
    g_madt_info={m->lapic_address,0,0,0,(m->flags&1u)!=0};
    const uint8_t* p=reinterpret_cast<const uint8_t*>(m)+sizeof(MadtHeader);
    const uint8_t* end=reinterpret_cast<const uint8_t*>(m)+m->header.length;
    while(p+sizeof(MadtEntryHeader)<=end){
        auto* e=reinterpret_cast<const MadtEntryHeader*>(p);
        if(e->length < sizeof(MadtEntryHeader) || p+e->length>end) break;
        if(e->type==0 && e->length>=8){ if((p[4]&1u)!=0) ++g_madt_info.local_apics; }
        else if(e->type==1 && e->length>=12) ++g_madt_info.io_apics;
        else if(e->type==2 && e->length>=10) ++g_madt_info.interrupt_overrides;
        else if(e->type==5 && e->length>=12){ uint64_t addr=0; for(int i=0;i<8;++i) addr|=uint64_t(p[4+i])<<(i*8); g_madt_info.lapic_address=addr; }
        p += e->length;
    }
    g_madt=true;
    char n[24], hbuf[19];
    format::hex64(g_madt_info.lapic_address,hbuf); serial::write("ACPI MADT LAPIC: "); serial::writeln(hbuf);
    format::dec64(g_madt_info.local_apics,n); serial::write("MADT enabled LAPICs: "); serial::writeln(n);
    format::dec64(g_madt_info.io_apics,n); serial::write("MADT IOAPICs: "); serial::writeln(n);
}

void init(limine_rsdp_response* rsp, limine_hhdm_response* hhdm){
    g_rsdp=g_mcfg=g_madt=false; g_mcfg_info={}; g_madt_info={}; g_hhdm=hhdm?hhdm->offset:0;
    if(!rsp || !rsp->address){ serial::writeln("ACPI: RSDP unavailable"); return; }
    auto* r=reinterpret_cast<const Rsdp20*>(rsp->address); g_rsdp=true;
    if(r->revision<2 || !r->xsdt){ serial::writeln("ACPI: RSDP found; XSDT unavailable"); return; }
    auto* x=phys_sdt(r->xsdt);
    if(!x || x->length<sizeof(SdtHeader) || !sig4(x->sig,"XSDT") || !checksum(x,x->length)){ serial::writeln("ACPI: invalid XSDT"); return; }
    uint32_t count=(x->length-sizeof(SdtHeader))/8;
    auto* entries=reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(x)+sizeof(SdtHeader));
    for(uint32_t i=0;i<count;++i){
        auto* h=phys_sdt(entries[i]);
        if(!h || h->length<sizeof(SdtHeader) || !checksum(h,h->length)) continue;
        if(sig4(h->sig,"MCFG")) parse_mcfg(h);
        else if(sig4(h->sig,"APIC")) parse_madt(h);
    }
    if(!g_mcfg) serial::writeln("ACPI: MCFG not found; legacy PCI config remains available");
    if(!g_madt) serial::writeln("ACPI: MADT not found; APIC activation deferred");
}
bool rsdp_present(){return g_rsdp;} bool mcfg_present(){return g_mcfg;} bool madt_present(){return g_madt;}
uint64_t hhdm_offset(){return g_hhdm;}
const McfgInfo& mcfg(){return g_mcfg_info;} const MadtInfo& madt(){return g_madt_info;}
}
