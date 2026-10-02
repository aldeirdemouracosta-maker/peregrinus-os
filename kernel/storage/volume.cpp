#include "volume.hpp"
#include "storage.hpp"
#include "../console/serial.hpp"
#include "../console/format.hpp"

namespace peregrinus::volume {
namespace {
Catalog g{};
static void copy_name(char dst[37],const char src[37]){ unsigned i=0; for(;i<36&&src[i];++i) dst[i]=src[i]; dst[i]=0; }
static Role cvt(gpt::PartitionRole r){
    switch(r){ case gpt::PartitionRole::system:return Role::system; case gpt::PartitionRole::recovery:return Role::recovery; case gpt::PartitionRole::data:return Role::data; default:return Role::unknown; }
}
static int32_t* slot_for(Role r){
    if(r==Role::system) return &g.system_index;
    if(r==Role::recovery) return &g.recovery_index;
    if(r==Role::data) return &g.data_index;
    return nullptr;
}
}

void init(){ g={}; g.system_index=-1; g.recovery_index=-1; g.data_index=-1; g.readonly_policy=true; }

bool bind_gpt(uint32_t device_index,const gpt::TableInfo& table){
    init();
    if(!table.header_ok || !table.entries_crc_ok) return false;
    if(device_index>=storage::inventory().count && storage::inventory().count!=0) return false;
    for(uint32_t i=0;i<table.used_entries && g.count<16;++i){
        const auto& p=table.partitions[i]; if(!p.used || p.last_lba<p.first_lba) continue;
        auto& v=g.volumes[g.count];
        v.present=true; v.readonly=true; v.bound=true; v.role=cvt(p.role); v.device_index=device_index;
        v.first_lba=p.first_lba; v.last_lba=p.last_lba; v.sector_count=(p.last_lba-p.first_lba)+1;
        v.type_guid=p.type_guid; v.unique_guid=p.unique_guid; copy_name(v.name,p.name);
        int32_t* rs=slot_for(v.role);
        if(rs){ if(*rs>=0) g.duplicate_role=true; else *rs=(int32_t)g.count; }
        ++g.count;
    }
    serial::writeln("Peregrinus Storage volumes (logical, read-only):");
    char n[24],guid[37];
    for(uint32_t i=0;i<g.count;++i){
        const auto& v=g.volumes[i];
        serial::write("  "); serial::write(v.name[0]?v.name:"(unnamed)"); serial::write(" role="); serial::write(role_name(v.role));
        serial::write(" sectors="); format::dec64(v.sector_count,n); serial::write(n); serial::write(" guid="); gpt::guid_text(v.unique_guid,guid); serial::writeln(guid);
    }
    if(g.duplicate_role) serial::writeln("Volume catalog warning: duplicate protected role detected; automatic selection disabled for duplicate role.");
    return g.count>0 && !g.duplicate_role;
}

const Catalog& catalog(){ return g; }

const Volume* by_role(Role r){
    int32_t idx=-1; if(r==Role::system) idx=g.system_index; else if(r==Role::recovery) idx=g.recovery_index; else if(r==Role::data) idx=g.data_index;
    if(idx<0 || (uint32_t)idx>=g.count) return nullptr;
    return &g.volumes[idx];
}

bool read_sector(const Volume& v,uint64_t relative_lba,void* out512){
    if(!v.present || !v.bound || !v.readonly || !out512 || relative_lba>=v.sector_count) return false;
    return storage::read_sector(v.device_index,v.first_lba+relative_lba,out512);
}

const char* role_name(Role r){ switch(r){ case Role::system:return "SYSTEM"; case Role::recovery:return "RECOVERY"; case Role::data:return "DATA"; default:return "UNKNOWN"; } }

bool self_test(){
    gpt::TableInfo t{}; t.header_ok=true; t.entries_crc_ok=true; t.used_entries=3;
    const char* names[3]={"IA_SYSTEM","IA_RECOVERY","IA_DATA"};
    const gpt::PartitionRole roles[3]={gpt::PartitionRole::system,gpt::PartitionRole::recovery,gpt::PartitionRole::data};
    for(unsigned i=0;i<3;++i){ auto& p=t.partitions[i]; p.used=true; p.role=roles[i]; p.first_lba=2048u+i*1024u; p.last_lba=p.first_lba+1023u; p.unique_guid.bytes[0]=(uint8_t)(0x10+i); unsigned j=0; for(;names[i][j]&&j<36;++j)p.name[j]=names[i][j]; p.name[j]=0; }
    // Storage inventory is allowed to be empty in a pure synthetic self-test.
    bool ok=bind_gpt(0,t);
    const Volume* s=by_role(Role::system); const Volume* r=by_role(Role::recovery); const Volume* d=by_role(Role::data);
    return ok&&s&&r&&d&&s->readonly&&r->readonly&&d->readonly&&s->sector_count==1024&&s->unique_guid.bytes[0]==0x10;
}
}
