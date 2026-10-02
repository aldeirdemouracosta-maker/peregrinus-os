#include "uefi_min.h"
#include "tcg2_min.h"
#include "../common/boot_request.h"
#include "../common/preboot_recovery.h"
#include "../common/tpm_monotonic.h"
#include "../common/tpm_commit.h"
#include <peregrinus/recovery_journal.h>
#include <peregrinus/release_profile.h>

#ifndef PEREGRINUS_RECOVERY_JOURNAL_LIVE
#define PEREGRINUS_RECOVERY_JOURNAL_LIVE 0
#endif
#ifndef PEREGRINUS_CURRENT_GENERATION
#define PEREGRINUS_CURRENT_GENERATION PEREGRINUS_RELEASE_CURRENT_GENERATION
#endif
#ifndef PEREGRINUS_LKG_GENERATION
#define PEREGRINUS_LKG_GENERATION PEREGRINUS_RELEASE_LKG_GENERATION
#endif
#ifndef PEREGRINUS_CURRENT_EPOCH
#define PEREGRINUS_CURRENT_EPOCH PEREGRINUS_RELEASE_CURRENT_EPOCH
#endif
#ifndef PEREGRINUS_LKG_EPOCH
#define PEREGRINUS_LKG_EPOCH PEREGRINUS_RELEASE_LKG_EPOCH
#endif

static EFI_GUID g_loaded_image={0x5b1b31a1,0x9562,0x11d2,{0x8e,0x3f,0x00,0xa0,0xc9,0x69,0x72,0x3b}};
static EFI_GUID g_device_path={0x09576e91,0x6d3f,0x11d2,{0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b}};
static EFI_GUID g_block_io={0x964e5b21,0x6459,0x11d2,{0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b}};
static EFI_GUID g_partition_info={0x8cf2f62c,0xbc9b,0x4821,{0x80,0x8d,0xec,0x9e,0xc4,0x21,0xa1,0xa0}};
static EFI_GUID g_tcg2={0x607f766c,0x7455,0x42be,{0x93,0x0b,0xe4,0xd7,0x6d,0xb2,0x72,0x0f}};
static EFI_GUID g_global={0x8be4df61,0x93ca,0x11d2,{0xaa,0x0d,0x00,0xe0,0x98,0x03,0x2b,0x8c}};
static EFI_GUID g_loader={0x4a67b082,0x0a4c,0x41cf,{0xb6,0xc7,0x44,0x0b,0x29,0xbb,0x8c,0x4f}};
static EFI_GUID g_peregrinus={0x8fd2e5b1,0x0f3e,0x4c51,{0x92,0x81,0xa7,0x16,0x67,0xe5,0x5a,0x61}};
static void* volatile g_force_reloc=&g_peregrinus;

static CHAR16 n_secure_boot[]={'S','e','c','u','r','e','B','o','o','t',0};
static CHAR16 n_setup_mode[]={'S','e','t','u','p','M','o','d','e',0};
static CHAR16 n_request[]={'P','e','r','e','g','r','i','n','u','s','B','o','o','t','R','e','q','u','e','s','t',0};
static CHAR16 n_oneshot[]={'L','o','a','d','e','r','E','n','t','r','y','O','n','e','S','h','o','t',0};
static CHAR16 e_lkg[]={'P','e','r','e','g','r','i','n','u','s','/','L','K','G',0};
static CHAR16 limine_staged_path[]={'\\','E','F','I','\\','P','e','r','e','g','r','i','n','u','s','\\','s','t','a','g','e','d','\\','l','i','m','i','n','e','_','x','6','4','.','e','f','i',0};
static CHAR16 limine_committed_path[]={'\\','E','F','I','\\','P','e','r','e','g','r','i','n','u','s','\\','c','o','m','m','i','t','t','e','d','\\','l','i','m','i','n','e','_','x','6','4','.','e','f','i',0};
static const CHAR16 recovery_name[]={'I','A','_','R','E','C','O','V','E','R','Y',0};

static void out(EFI_SYSTEM_TABLE* st,CHAR16* s){if(st&&st->ConOut&&st->ConOut->OutputString)st->ConOut->OutputString(st->ConOut,s);}
static void out_ascii(EFI_SYSTEM_TABLE* st,const char* s){CHAR16 b[192];unsigned n=0;while(s[n]&&n<191){b[n]=(CHAR16)(uint8_t)s[n];++n;}b[n]=0;out(st,b);}
static void copy_bytes(void* d,const void* s,UINTN n){uint8_t* dd=(uint8_t*)d;const uint8_t* ss=(const uint8_t*)s;for(UINTN i=0;i<n;++i)dd[i]=ss[i];}
static void zero_bytes(void* d,UINTN n){uint8_t* dd=(uint8_t*)d;for(UINTN i=0;i<n;++i)dd[i]=0;}
static int bytes_equal(const void* a,const void* b,UINTN n){const uint8_t* x=(const uint8_t*)a;const uint8_t* y=(const uint8_t*)b;for(UINTN i=0;i<n;++i)if(x[i]!=y[i])return 0;return 1;}
static UINTN node_len(const EFI_DEVICE_PATH_PROTOCOL* p){return (UINTN)p->Length[0]|((UINTN)p->Length[1]<<8);}
static uint32_t journal_attrs(void){return EFI_VARIABLE_NON_VOLATILE|EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS;}

static int secure_boot_active(EFI_SYSTEM_TABLE* st){
    uint8_t sb=0,sm=1;UINTN n=1;uint32_t a=0;
    EFI_STATUS x=st->RuntimeServices->GetVariable(n_secure_boot,&g_global,&a,&n,&sb);
    n=1;EFI_STATUS y=st->RuntimeServices->GetVariable(n_setup_mode,&g_global,&a,&n,&sm);
    return x==EFI_SUCCESS&&y==EFI_SUCCESS&&sb==1&&sm==0;
}
static int read_request(EFI_SYSTEM_TABLE* st,struct peregrinus_boot_request* r){UINTN n=sizeof(*r);uint32_t a=0;EFI_STATUS s=st->RuntimeServices->GetVariable(n_request,&g_peregrinus,&a,&n,r);return s==EFI_SUCCESS&&n==sizeof(*r)&&peregrinus_request_valid(r);}
static int consume_request(EFI_SYSTEM_TABLE* st){EFI_STATUS s=st->RuntimeServices->SetVariable(n_request,&g_peregrinus,0,0,0);return s==EFI_SUCCESS||s==EFI_NOT_FOUND;}
static int clear_oneshot(EFI_SYSTEM_TABLE* st){EFI_STATUS s=st->RuntimeServices->SetVariable(n_oneshot,&g_loader,0,0,0);return s==EFI_SUCCESS||s==EFI_NOT_FOUND;}
static int set_lkg_oneshot(EFI_SYSTEM_TABLE* st){return st->RuntimeServices->SetVariable(n_oneshot,&g_loader,journal_attrs(),sizeof(e_lkg),e_lkg)==EFI_SUCCESS;}

static int name_equals(const CHAR16* a,const CHAR16* b,UINTN max){for(UINTN i=0;i<max;++i){if(a[i]!=b[i])return 0;if(a[i]==0)return 1;}return 0;}
static UINTN device_path_payload_bytes(const EFI_DEVICE_PATH_PROTOCOL* p){
    UINTN total=0;
    if(!p)return 0;
    for(unsigned guard=0;guard<128;++guard){
        const UINTN len=node_len(p);
        if(len<4)return 0;
        if(p->Type==0x7f&&p->SubType==0xff)return total;
        total+=len;
        p=(const EFI_DEVICE_PATH_PROTOCOL*)((const uint8_t*)p+len);
    }
    return 0;
}
static int get_device_path(EFI_SYSTEM_TABLE* st,EFI_HANDLE h,EFI_DEVICE_PATH_PROTOCOL** dp){return !EFI_ERROR(st->BootServices->HandleProtocol(h,&g_device_path,(void**)dp))&&*dp;}
static int device_path_is_prefix(const EFI_DEVICE_PATH_PROTOCOL* parent,const EFI_DEVICE_PATH_PROTOCOL* child){
    const UINTN n=device_path_payload_bytes(parent);
    const UINTN c=device_path_payload_bytes(child);
    return n>0&&c>n&&bytes_equal(parent,child,n);
}

static int alloc_aligned(EFI_SYSTEM_TABLE* st,UINTN bytes,uint32_t align,void** raw,void** aligned){
    UINTN a=align<=1?1u:(UINTN)align;
    if((a&(a-1))!=0)return 0;
    if(bytes>~(UINTN)0-(a-1))return 0;
    uint8_t* r=0;
    if(EFI_ERROR(st->BootServices->AllocatePool(EfiLoaderData,bytes+a-1,(void**)&r))||!r)return 0;
    uintptr_t x=(uintptr_t)r;
    uintptr_t y=(x+(a-1))&~(uintptr_t)(a-1);
    *raw=r;*aligned=(void*)y;return 1;
}

static EFI_HANDLE find_parent_physical_block(EFI_SYSTEM_TABLE* st,EFI_HANDLE partition){
    EFI_DEVICE_PATH_PROTOCOL* pdp=0;
    if(!get_device_path(st,partition,&pdp)||!st->BootServices->LocateHandleBuffer)return 0;
    EFI_HANDLE* hs=0;UINTN count=0;
    if(EFI_ERROR(st->BootServices->LocateHandleBuffer(ByProtocol,&g_block_io,0,&count,&hs))||!hs)return 0;
    EFI_HANDLE best=0;UINTN best_len=0;
    for(UINTN i=0;i<count;++i){
        EFI_BLOCK_IO_PROTOCOL* bio=0;EFI_DEVICE_PATH_PROTOCOL* dp=0;
        if(EFI_ERROR(st->BootServices->HandleProtocol(hs[i],&g_block_io,(void**)&bio))||!bio||!bio->Media)continue;
        if(bio->Media->LogicalPartition||!bio->Media->MediaPresent)continue;
        if(!get_device_path(st,hs[i],&dp)||!device_path_is_prefix(dp,pdp))continue;
        const UINTN n=device_path_payload_bytes(dp);if(n>best_len){best=hs[i];best_len=n;}
    }
    st->BootServices->FreePool(hs);return best;
}

#pragma pack(push,1)
struct gpt_header_min {
    uint8_t signature[8];
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved;
    uint64_t my_lba;
    uint64_t alternate_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    uint8_t disk_guid[16];
    uint64_t partition_entry_lba;
    uint32_t partition_count;
    uint32_t partition_entry_size;
    uint32_t partition_array_crc32;
};
#pragma pack(pop)

static int read_disk_guid(EFI_SYSTEM_TABLE* st,EFI_HANDLE physical,uint8_t out_guid[16]){
    EFI_BLOCK_IO_PROTOCOL* bio=0;
    if(EFI_ERROR(st->BootServices->HandleProtocol(physical,&g_block_io,(void**)&bio))||!bio||!bio->Media||!bio->ReadBlocks)return 0;
    if(!bio->Media->MediaPresent||bio->Media->LogicalPartition||bio->Media->BlockSize<sizeof(struct gpt_header_min)||bio->Media->LastBlock<1)return 0;
    void* raw=0;void* buf=0;const UINTN bs=bio->Media->BlockSize;
    if(!alloc_aligned(st,bs,bio->Media->IoAlign,&raw,&buf))return 0;
    const EFI_STATUS rs=bio->ReadBlocks(bio,bio->Media->MediaId,1,bs,buf);
    if(EFI_ERROR(rs)){st->BootServices->FreePool(raw);return 0;}
    struct gpt_header_min h;copy_bytes(&h,buf,sizeof(h));
    static const uint8_t sig[8]={'E','F','I',' ','P','A','R','T'};
    int ok=bytes_equal(h.signature,sig,8)&&h.header_size>=92&&h.header_size<=bs&&h.my_lba==1&&h.reserved==0;
    if(ok){
        uint8_t* tmp=(uint8_t*)buf;const uint32_t expected=h.header_crc32;
        tmp[16]=tmp[17]=tmp[18]=tmp[19]=0;
        ok=peregrinus_rj_crc32_bytes(tmp,h.header_size)==expected;
    }
    if(ok)copy_bytes(out_guid,h.disk_guid,16);
    st->BootServices->FreePool(raw);return ok;
}

struct recovery_disk_context {
    EFI_HANDLE partition_handle;
    EFI_BLOCK_IO_PROTOCOL* bio;
    uint8_t disk_guid[16];
    uint8_t recovery_guid[16];
};

static int find_recovery_partition(EFI_HANDLE image,EFI_SYSTEM_TABLE* st,struct recovery_disk_context* outctx){
    EFI_LOADED_IMAGE_PROTOCOL* li=0;
    if(EFI_ERROR(st->BootServices->HandleProtocol(image,&g_loaded_image,(void**)&li))||!li||!li->DeviceHandle)return 0;
    EFI_HANDLE boot_disk=find_parent_physical_block(st,li->DeviceHandle);if(!boot_disk)return 0;
    uint8_t disk_guid[16];if(!read_disk_guid(st,boot_disk,disk_guid))return 0;
    EFI_HANDLE* hs=0;UINTN count=0;
    if(!st->BootServices->LocateHandleBuffer||EFI_ERROR(st->BootServices->LocateHandleBuffer(ByProtocol,&g_partition_info,0,&count,&hs))||!hs)return 0;
    int found=0;
    for(UINTN i=0;i<count;++i){
        EFI_PARTITION_INFO_PROTOCOL* pi=0;EFI_BLOCK_IO_PROTOCOL* bio=0;
        if(EFI_ERROR(st->BootServices->HandleProtocol(hs[i],&g_partition_info,(void**)&pi))||!pi||pi->Type!=PARTITION_TYPE_GPT)continue;
        if(!name_equals(pi->Info.Gpt.PartitionName,recovery_name,36))continue;
        if(find_parent_physical_block(st,hs[i])!=boot_disk)continue;
        if(EFI_ERROR(st->BootServices->HandleProtocol(hs[i],&g_block_io,(void**)&bio))||!bio||!bio->Media||!bio->Media->LogicalPartition)continue;
        if(!bio->Media->MediaPresent||bio->Media->ReadOnly||bio->Media->BlockSize<sizeof(struct peregrinus_recovery_journal)||bio->Media->LastBlock<5)continue;
        if(pi->Info.Gpt.EndingLBA<pi->Info.Gpt.StartingLBA||bio->Media->LastBlock!=pi->Info.Gpt.EndingLBA-pi->Info.Gpt.StartingLBA)continue;
        outctx->partition_handle=hs[i];outctx->bio=bio;copy_bytes(outctx->disk_guid,disk_guid,16);copy_bytes(outctx->recovery_guid,&pi->Info.Gpt.UniquePartitionGUID,16);found=1;break;
    }
    st->BootServices->FreePool(hs);return found;
}

static int read_recovery_record(EFI_SYSTEM_TABLE* st,const struct recovery_disk_context* ctx,EFI_LBA lba,struct peregrinus_recovery_journal* outj){
    const UINTN bs=ctx->bio->Media->BlockSize;void* raw=0;void* buf=0;
    if(!alloc_aligned(st,bs,ctx->bio->Media->IoAlign,&raw,&buf))return 0;
    const EFI_STATUS s=ctx->bio->ReadBlocks(ctx->bio,ctx->bio->Media->MediaId,lba,bs,buf);
    if(!EFI_ERROR(s))copy_bytes(outj,buf,sizeof(*outj));
    st->BootServices->FreePool(raw);return !EFI_ERROR(s);
}

static int recovery_profile_valid(const struct peregrinus_recovery_journal* j,const struct recovery_disk_context* ctx){
    return peregrinus_rj_identity(j,ctx->disk_guid,ctx->recovery_guid)&&
           peregrinus_preboot_profile_matches(j,PEREGRINUS_CURRENT_GENERATION,PEREGRINUS_LKG_GENERATION,PEREGRINUS_CURRENT_EPOCH,PEREGRINUS_LKG_EPOCH);
}

static int load_recovery_journal(EFI_SYSTEM_TABLE* st,const struct recovery_disk_context* ctx,struct peregrinus_preboot_journal_selection* sel){
    struct peregrinus_recovery_journal a={0},b={0};
    const int ra=read_recovery_record(st,ctx,2,&a);const int rb=read_recovery_record(st,ctx,ctx->bio->Media->LastBlock-2,&b);
    const int va=ra&&recovery_profile_valid(&a,ctx);const int vb=rb&&recovery_profile_valid(&b,ctx);
    *sel=peregrinus_preboot_select(&a,va,&b,vb);return sel->state!=PEREGRINUS_PREBOOT_NO_JOURNAL&&sel->state!=PEREGRINUS_PREBOOT_SPLIT_BRAIN;
}

static int persist_recovery_journal(EFI_SYSTEM_TABLE* st,const struct recovery_disk_context* ctx,struct peregrinus_preboot_journal_selection* sel){
    const uint8_t target=peregrinus_preboot_inactive_copy(sel->active_copy);
    const EFI_LBA lba=target==0?2:ctx->bio->Media->LastBlock-2;
    const UINTN bs=ctx->bio->Media->BlockSize;void* raw=0;void* buf=0;
    if(!ctx->bio->WriteBlocks||!alloc_aligned(st,bs,ctx->bio->Media->IoAlign,&raw,&buf))return 0;
    EFI_STATUS s=ctx->bio->ReadBlocks(ctx->bio,ctx->bio->Media->MediaId,lba,bs,buf);
    if(EFI_ERROR(s)){st->BootServices->FreePool(raw);return 0;}
    copy_bytes(buf,&sel->journal,sizeof(sel->journal));
    s=ctx->bio->WriteBlocks(ctx->bio,ctx->bio->Media->MediaId,lba,bs,buf);
    if(EFI_ERROR(s)){st->BootServices->FreePool(raw);return 0;}
    if(ctx->bio->FlushBlocks){const EFI_STATUS fs=ctx->bio->FlushBlocks(ctx->bio);if(EFI_ERROR(fs)&&!(fs==EFI_UNSUPPORTED&&!ctx->bio->Media->WriteCaching)){st->BootServices->FreePool(raw);return 0;}}
    else if(ctx->bio->Media->WriteCaching){st->BootServices->FreePool(raw);return 0;}
    zero_bytes(buf,bs);s=ctx->bio->ReadBlocks(ctx->bio,ctx->bio->Media->MediaId,lba,bs,buf);
    if(EFI_ERROR(s)){st->BootServices->FreePool(raw);return 0;}
    struct peregrinus_recovery_journal verify={0};copy_bytes(&verify,buf,sizeof(verify));st->BootServices->FreePool(raw);
    if(!recovery_profile_valid(&verify,ctx)||!bytes_equal(&verify,&sel->journal,sizeof(verify)))return 0;
    sel->active_copy=target;sel->state=PEREGRINUS_PREBOOT_HEALTHY;return 1;
}

enum tpm_anchor_state{TPM_ANCHOR_UNAVAILABLE,TPM_ANCHOR_INVALID,TPM_ANCHOR_READABLE};
struct tpm_anchor_report{enum tpm_anchor_state state;uint64_t counter;uint32_t manufacturer;};
static struct tpm_anchor_report read_tpm_anchor(EFI_SYSTEM_TABLE* st){
    struct tpm_anchor_report r={TPM_ANCHOR_UNAVAILABLE,0,0};
    if(!st||!st->BootServices||!st->BootServices->LocateProtocol)return r;
    EFI_TCG2_PROTOCOL* t=0;if(EFI_ERROR(st->BootServices->LocateProtocol(&g_tcg2,0,(void**)&t))||!t||!t->GetCapability||!t->SubmitCommand)return r;
    EFI_TCG2_BOOT_SERVICE_CAPABILITY cap={0};cap.Size=(uint8_t)sizeof(cap);if(EFI_ERROR(t->GetCapability(t,&cap))||!cap.TPMPresentFlag)return r;
    if(cap.MaxCommandSize<35||cap.MaxResponseSize<29){r.state=TPM_ANCHOR_INVALID;return r;}r.manufacturer=cap.ManufacturerID;
    uint8_t cmd[64]={0},rsp[256]={0};size_t n=peregrinus_tpm_build_nv_read_public(cmd,sizeof(cmd),PEREGRINUS_TPM_NV_INDEX);
    if(!n||EFI_ERROR(t->SubmitCommand(t,(uint32_t)n,cmd,sizeof(rsp),rsp))){r.state=TPM_ANCHOR_INVALID;return r;}
    struct peregrinus_tpm_nv_public pub={0};if(!peregrinus_tpm_parse_nv_read_public(rsp,sizeof(rsp),&pub)||(pub.attributes&TPMA_NV_WRITTEN)==0){r.state=TPM_ANCHOR_INVALID;return r;}
    n=peregrinus_tpm_build_nv_read_counter(cmd,sizeof(cmd),PEREGRINUS_TPM_NV_INDEX);for(unsigned i=0;i<sizeof(rsp);++i)rsp[i]=0;
    if(!n||EFI_ERROR(t->SubmitCommand(t,(uint32_t)n,cmd,sizeof(rsp),rsp))||!peregrinus_tpm_parse_nv_read_counter(rsp,sizeof(rsp),&r.counter)){r.state=TPM_ANCHOR_INVALID;return r;}
    r.state=TPM_ANCHOR_READABLE;return r;
}

static EFI_DEVICE_PATH_PROTOCOL* limine_device_path(EFI_HANDLE image,EFI_SYSTEM_TABLE* st,const CHAR16* file_path,UINTN file_bytes){
    EFI_LOADED_IMAGE_PROTOCOL* li=0;EFI_DEVICE_PATH_PROTOCOL* base=0;EFI_STATUS s=st->BootServices->HandleProtocol(image,&g_loaded_image,(void**)&li);if(EFI_ERROR(s)||!li)return 0;
    s=st->BootServices->HandleProtocol(li->DeviceHandle,&g_device_path,(void**)&base);if(EFI_ERROR(s)||!base)return 0;
    UINTN base_bytes=0;EFI_DEVICE_PATH_PROTOCOL* p=base;for(unsigned guard=0;guard<128;++guard){const UINTN len=node_len(p);if(len<4)return 0;if(p->Type==0x7f&&p->SubType==0xff)break;base_bytes+=len;p=(EFI_DEVICE_PATH_PROTOCOL*)((uint8_t*)p+len);if(guard==127)return 0;}
    const UINTN file_node=4+file_bytes,total=base_bytes+file_node+4;uint8_t* outp=0;if(EFI_ERROR(st->BootServices->AllocatePool(EfiLoaderData,total,(void**)&outp)))return 0;
    copy_bytes(outp,base,base_bytes);uint8_t* f=outp+base_bytes;f[0]=0x04;f[1]=0x04;f[2]=(uint8_t)(file_node&0xff);f[3]=(uint8_t)(file_node>>8);copy_bytes(f+4,file_path,file_bytes);uint8_t* e=f+file_node;e[0]=0x7f;e[1]=0xff;e[2]=4;e[3]=0;return (EFI_DEVICE_PATH_PROTOCOL*)outp;
}
static EFI_STATUS chainload_limine(EFI_HANDLE image,EFI_SYSTEM_TABLE* st,int staged){const CHAR16* path=staged?limine_staged_path:limine_committed_path;const UINTN bytes=staged?sizeof(limine_staged_path):sizeof(limine_committed_path);EFI_DEVICE_PATH_PROTOCOL* dp=limine_device_path(image,st,path,bytes);if(!dp)return EFI_INVALID_PARAMETER;EFI_HANDLE child=0;EFI_STATUS s=st->BootServices->LoadImage(0,image,dp,0,0,&child);st->BootServices->FreePool(dp);if(EFI_ERROR(s))return s;return st->BootServices->StartImage(child,0,0);}

static EFI_STATUS committed_recovery_boot(EFI_HANDLE image,EFI_SYSTEM_TABLE* st){
    struct recovery_disk_context ctx={0};
    if(!find_recovery_partition(image,st,&ctx)){out_ascii(st,"IA_RECOVERY Block I/O not found on boot disk; FAIL-CLOSED\r\n");return EFI_SECURITY_VIOLATION;}
    struct peregrinus_preboot_journal_selection sel={0};
    if(!load_recovery_journal(st,&ctx,&sel)){out_ascii(st,"Recovery Journal missing/split-brain/profile mismatch; FAIL-CLOSED\r\n");return EFI_SECURITY_VIOLATION;}
    uint8_t slot=peregrinus_rj_choose_slot(&sel.journal);struct peregrinus_boot_request br={0};const int manual=read_request(st,&br);
    if(manual&&br.choice==PEREGRINUS_BOOT_LKG&&br.requested_security_epoch==PEREGRINUS_LKG_EPOCH)slot=PEREGRINUS_RJ_SLOT_LKG;
    (void)consume_request(st);
    if(slot==PEREGRINUS_RJ_SLOT_LKG&&sel.journal.lkg_attempts>=sel.journal.max_attempts){out_ascii(st,"CURRENT and LKG exhausted; FAIL-CLOSED\r\n");return EFI_SECURITY_VIOLATION;}
    if(!peregrinus_rj_prepare_attempt(&sel.journal,slot)||!persist_recovery_journal(st,&ctx,&sel)){out_ascii(st,"Pre-boot Recovery Journal persistence failed; FAIL-CLOSED\r\n");return EFI_SECURITY_VIOLATION;}
    if(!clear_oneshot(st))return EFI_SECURITY_VIOLATION;
    if(slot==PEREGRINUS_RJ_SLOT_LKG){if(!set_lkg_oneshot(st))return EFI_SECURITY_VIOLATION;out_ascii(st,"Pre-boot Recovery Journal selection: LKG\r\n");}else out_ascii(st,"Pre-boot Recovery Journal selection: CURRENT\r\n");
    return chainload_limine(image,st,0);
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE* st){
    (void)g_force_reloc;out_ascii(st,"Peregrinus OS Muro 1.0 Stable Trusted Boot Controller\r\n");
    if(!secure_boot_active(st)){out_ascii(st,"Secure Boot: NOT TRUSTED; CURRENT-only path\r\n");(void)consume_request(st);(void)clear_oneshot(st);return chainload_limine(image,st,0);}
    out_ascii(st,"Secure Boot: ACTIVE\r\n");
    if(!peregrinus_tpm_commit_profile_valid(PEREGRINUS_TPM_COMMIT_BASE,PEREGRINUS_TPM_COMMIT_NEXT)){out_ascii(st,"TPM profile unprovisioned; CURRENT-only path\r\n");(void)consume_request(st);(void)clear_oneshot(st);return chainload_limine(image,st,0);}
    const struct tpm_anchor_report tpm=read_tpm_anchor(st);if(tpm.state!=TPM_ANCHOR_READABLE){out_ascii(st,"TPM anchor unavailable/invalid; CURRENT-only path\r\n");(void)consume_request(st);(void)clear_oneshot(st);return chainload_limine(image,st,0);}
    const enum peregrinus_tpm_commit_state cs=peregrinus_tpm_commit_classify(tpm.counter,PEREGRINUS_TPM_COMMIT_BASE,PEREGRINUS_TPM_COMMIT_NEXT);
    if(cs==PEREGRINUS_TPM_COMMIT_STALE||cs==PEREGRINUS_TPM_COMMIT_FUTURE){out_ascii(st,"TPM counter incompatible with media; FAIL-CLOSED\r\n");return EFI_SECURITY_VIOLATION;}
    if(cs==PEREGRINUS_TPM_COMMIT_COMMITTED){
        if(!PEREGRINUS_RECOVERY_JOURNAL_LIVE){out_ascii(st,"TPM state: COMMITTED; pre-boot disk journal gate disabled; CURRENT-only path\r\n");(void)consume_request(st);(void)clear_oneshot(st);return chainload_limine(image,st,0);}
        out_ascii(st,"TPM state: COMMITTED; IA_RECOVERY pre-boot journal enabled\r\n");return committed_recovery_boot(image,st);
    }
    if(cs!=PEREGRINUS_TPM_COMMIT_STAGED){out_ascii(st,"TPM state invalid; CURRENT-only path\r\n");(void)consume_request(st);(void)clear_oneshot(st);return chainload_limine(image,st,0);}
    out_ascii(st,"TPM transition still STAGED; journal auto-fallback disabled\r\n");(void)clear_oneshot(st);struct peregrinus_boot_request br={0};const int valid=read_request(st,&br);
    if(valid&&peregrinus_tpm_state_allows_lkg(cs,PEREGRINUS_CURRENT_EPOCH,PEREGRINUS_LKG_EPOCH,br.requested_security_epoch)&&peregrinus_request_allows_lkg_floor(&br,PEREGRINUS_CURRENT_EPOCH,PEREGRINUS_LKG_EPOCH,PEREGRINUS_PRECOMMIT_MIN_EPOCH)){if(consume_request(st)&&set_lkg_oneshot(st))out_ascii(st,"Trusted one-shot selection: staged LKG\r\n");}
    else if(valid)(void)consume_request(st);return chainload_limine(image,st,1);
}
