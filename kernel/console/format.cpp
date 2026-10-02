#include "format.hpp"
namespace peregrinus::format {
static constexpr char H[] = "0123456789ABCDEF";
void hex64(uint64_t v, char out[19]) { out[0]='0'; out[1]='x'; for(int i=0;i<16;i++) out[2+i]=H[(v >> ((15-i)*4)) & 0xF]; out[18]=0; }
void hex32(uint32_t v, char out[11]) { out[0]='0'; out[1]='x'; for(int i=0;i<8;i++) out[2+i]=H[(v >> ((7-i)*4)) & 0xF]; out[10]=0; }
void dec64(uint64_t v, char out[24]) { char tmp[24]; int n=0; do { tmp[n++]=char('0'+(v%10)); v/=10; } while(v && n<23); int j=0; while(n) out[j++]=tmp[--n]; out[j]=0; }
}
