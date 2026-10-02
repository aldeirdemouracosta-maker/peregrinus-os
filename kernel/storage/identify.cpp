#include "identify.hpp"

namespace peregrinus::identify {
namespace {
static uint16_t word(const uint8_t* p, uint32_t index){ return uint16_t(p[index*2]) | (uint16_t(p[index*2+1])<<8); }
static uint32_t dword_words(const uint8_t* p,uint32_t low){ return uint32_t(word(p,low)) | (uint32_t(word(p,low+1))<<16); }
static uint64_t qword_words(const uint8_t* p,uint32_t low){ return uint64_t(word(p,low)) | (uint64_t(word(p,low+1))<<16) | (uint64_t(word(p,low+2))<<32) | (uint64_t(word(p,low+3))<<48); }
static bool power_of_two(uint32_t v){ return v && ((v&(v-1u))==0); }
static void ata_string(const uint8_t* p,uint32_t first_word,uint32_t word_count,char* out,uint32_t out_bytes){
    if(!out || out_bytes==0) return;
    uint32_t j=0;
    for(uint32_t i=0;i<word_count && j+1<out_bytes;++i){
        const uint8_t hi=p[(first_word+i)*2+1], lo=p[(first_word+i)*2];
        if(j+1<out_bytes) out[j++]=char(hi);
        if(j+1<out_bytes) out[j++]=char(lo);
    }
    while(j && out[j-1]==' ') --j;
    out[j]=0;
}
static bool checksum_ok(const uint8_t* p){ uint32_t s=0; for(uint32_t i=0;i<512;++i)s+=p[i]; return (s&0xFFu)==0; }
static void set_word(uint8_t* p,uint32_t i,uint16_t v){p[i*2]=uint8_t(v);p[i*2+1]=uint8_t(v>>8);}
static void set_qword_words(uint8_t* p,uint32_t low,uint64_t v){for(uint32_t i=0;i<4;++i)set_word(p,low+i,uint16_t(v>>(16*i)));}
static void set_ata_string(uint8_t* p,uint32_t first,uint32_t words,const char* s){
    uint32_t k=0;
    for(uint32_t i=0;i<words;++i){uint8_t a=' ',b=' ';if(s[k])a=uint8_t(s[k++]);if(s[k])b=uint8_t(s[k++]);p[(first+i)*2]=b;p[(first+i)*2+1]=a;}
}
}

DeviceInfo parse(const uint8_t* p,size_t bytes){
    DeviceInfo d{}; if(!p || bytes<512) return d;
    ata_string(p,10,10,d.serial,sizeof(d.serial));
    ata_string(p,23,4,d.firmware,sizeof(d.firmware));
    ata_string(p,27,20,d.model,sizeof(d.model));
    d.lba_supported=(word(p,49)&0x0200u)!=0;
    const uint16_t w83=word(p,83);
    d.lba48_supported=((w83&0xC400u)==0x4400u);
    uint64_t sectors=d.lba48_supported?qword_words(p,100):uint64_t(dword_words(p,60));
    if(d.lba48_supported && sectors==0) sectors=uint64_t(dword_words(p,60));
    d.sector_count=sectors;
    d.last_lba=sectors?sectors-1u:0;
    d.logical_sector_bytes=512;
    const uint16_t w106=word(p,106);
    if((w106&0xD000u)==0x5000u){
        const uint32_t words=dword_words(p,117);
        if(words && words<=32768u) d.logical_sector_bytes=words*2u;
    }
    d.physical_sector_bytes=d.logical_sector_bytes;
    if((w106&0xE000u)==0x6000u){
        const uint32_t shift=w106&0x000Fu;
        if(shift<16u && d.logical_sector_bytes<=(0xFFFFFFFFu>>shift)) d.physical_sector_bytes=d.logical_sector_bytes<<shift;
    }
    d.checksum_present=(p[510]==0xA5u);
    d.checksum_valid=!d.checksum_present || checksum_ok(p);
    const bool sector_sane=d.logical_sector_bytes>=512u && d.logical_sector_bytes<=65536u && power_of_two(d.logical_sector_bytes);
    const bool capacity_sane=d.lba_supported && d.sector_count>0;
    if(capacity_sane && d.sector_count<=UINT64_MAX/uint64_t(d.logical_sector_bytes)) d.capacity_bytes=d.sector_count*uint64_t(d.logical_sector_bytes);
    d.valid=capacity_sane && sector_sane && d.checksum_valid;
    return d;
}

bool self_test(){
    uint8_t p[512]{};
    set_word(p,49,0x0200u);
    set_word(p,83,0x4400u);
    set_qword_words(p,100,131072u);
    set_word(p,106,0x5000u);
    set_word(p,117,2048u); set_word(p,118,0u); // 4096-byte logical sector
    set_ata_string(p,10,10,"PEREGRINUS0001");
    set_ata_string(p,23,4,"0.9");
    set_ata_string(p,27,20,"Peregrinus Synthetic SATA Disk");
    auto d=parse(p,sizeof(p));
    return d.valid && d.lba_supported && d.lba48_supported && d.logical_sector_bytes==4096u && d.sector_count==131072u && d.last_lba==131071u && d.model[0]=='P';
}
}
