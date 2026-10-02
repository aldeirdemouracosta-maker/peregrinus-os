#pragma once
#include <stdint.h>
#include <stddef.h>

typedef uint64_t UINTN;
typedef uint64_t EFI_STATUS;
typedef void* EFI_HANDLE;
typedef uint16_t CHAR16;
typedef uint8_t BOOLEAN;
typedef uint64_t EFI_LBA;

#define EFIAPI __attribute__((ms_abi))
#define EFI_SUCCESS 0
#define EFI_ERROR(s) (((s) >> 63) != 0)
#define EFIERR(a) (0x8000000000000000ULL | (a))
#define EFI_NOT_FOUND EFIERR(14)
#define EFI_INVALID_PARAMETER EFIERR(2)
#define EFI_SECURITY_VIOLATION EFIERR(26)
#define EFI_UNSUPPORTED EFIERR(3)
#define EFI_WRITE_PROTECTED EFIERR(8)
#define EFI_NO_MEDIA EFIERR(12)
#define EFI_MEDIA_CHANGED EFIERR(13)
#define EFI_OUT_OF_RESOURCES EFIERR(9)

#define EFI_VARIABLE_NON_VOLATILE 0x00000001u
#define EFI_VARIABLE_BOOTSERVICE_ACCESS 0x00000002u
#define EFI_VARIABLE_RUNTIME_ACCESS 0x00000004u
#define EfiLoaderData 2u
#define ByProtocol 2u

typedef struct { uint32_t Data1; uint16_t Data2; uint16_t Data3; uint8_t Data4[8]; } EFI_GUID;
typedef struct { uint64_t Signature; uint32_t Revision; uint32_t HeaderSize; uint32_t CRC32; uint32_t Reserved; } EFI_TABLE_HEADER;

typedef struct EFI_DEVICE_PATH_PROTOCOL {
    uint8_t Type; uint8_t SubType; uint8_t Length[2];
} EFI_DEVICE_PATH_PROTOCOL;

typedef EFI_STATUS (EFIAPI *EFI_TEXT_STRING)(void*, CHAR16*);
typedef struct { void* Reset; EFI_TEXT_STRING OutputString; } EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef EFI_STATUS (EFIAPI *EFI_GET_VARIABLE)(CHAR16*, EFI_GUID*, uint32_t*, UINTN*, void*);
typedef EFI_STATUS (EFIAPI *EFI_SET_VARIABLE)(CHAR16*, EFI_GUID*, uint32_t, UINTN, void*);
typedef struct {
    EFI_TABLE_HEADER Hdr;
    void* GetTime; void* SetTime; void* GetWakeupTime; void* SetWakeupTime;
    void* SetVirtualAddressMap; void* ConvertPointer;
    EFI_GET_VARIABLE GetVariable; void* GetNextVariableName; EFI_SET_VARIABLE SetVariable;
    void* GetNextHighMonotonicCount; void* ResetSystem; void* UpdateCapsule;
    void* QueryCapsuleCapabilities; void* QueryVariableInfo;
} EFI_RUNTIME_SERVICES;

typedef EFI_STATUS (EFIAPI *EFI_ALLOCATE_POOL)(uint32_t, UINTN, void**);
typedef EFI_STATUS (EFIAPI *EFI_FREE_POOL)(void*);
typedef EFI_STATUS (EFIAPI *EFI_HANDLE_PROTOCOL)(EFI_HANDLE, EFI_GUID*, void**);
typedef EFI_STATUS (EFIAPI *EFI_LOAD_IMAGE)(BOOLEAN, EFI_HANDLE, EFI_DEVICE_PATH_PROTOCOL*, void*, UINTN, EFI_HANDLE*);
typedef EFI_STATUS (EFIAPI *EFI_START_IMAGE)(EFI_HANDLE, UINTN*, CHAR16**);
typedef EFI_STATUS (EFIAPI *EFI_LOCATE_PROTOCOL)(EFI_GUID*, void*, void**);
typedef EFI_STATUS (EFIAPI *EFI_LOCATE_HANDLE_BUFFER)(uint32_t, EFI_GUID*, void*, UINTN*, EFI_HANDLE**);
typedef struct {
    EFI_TABLE_HEADER Hdr;
    void* RaiseTPL; void* RestoreTPL; void* AllocatePages; void* FreePages; void* GetMemoryMap;
    EFI_ALLOCATE_POOL AllocatePool; EFI_FREE_POOL FreePool;
    void* CreateEvent; void* SetTimer; void* WaitForEvent; void* SignalEvent; void* CloseEvent; void* CheckEvent;
    void* InstallProtocolInterface; void* ReinstallProtocolInterface; void* UninstallProtocolInterface;
    EFI_HANDLE_PROTOCOL HandleProtocol; void* Reserved; void* RegisterProtocolNotify; void* LocateHandle;
    void* LocateDevicePath; void* InstallConfigurationTable;
    EFI_LOAD_IMAGE LoadImage; EFI_START_IMAGE StartImage;
    void* Exit; void* UnloadImage; void* ExitBootServices; void* GetNextMonotonicCount; void* Stall;
    void* SetWatchdogTimer; void* ConnectController; void* DisconnectController; void* OpenProtocol;
    void* CloseProtocol; void* OpenProtocolInformation; void* ProtocolsPerHandle; EFI_LOCATE_HANDLE_BUFFER LocateHandleBuffer;
    EFI_LOCATE_PROTOCOL LocateProtocol; void* InstallMultipleProtocolInterfaces; void* UninstallMultipleProtocolInterfaces;
    void* CalculateCrc32; void* CopyMem; void* SetMem; void* CreateEventEx;
} EFI_BOOT_SERVICES;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    CHAR16* FirmwareVendor; uint32_t FirmwareRevision;
    EFI_HANDLE ConsoleInHandle; void* ConIn;
    EFI_HANDLE ConsoleOutHandle; EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* ConOut;
    EFI_HANDLE StandardErrorHandle; EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* StdErr;
    EFI_RUNTIME_SERVICES* RuntimeServices; EFI_BOOT_SERVICES* BootServices;
    UINTN NumberOfTableEntries; void* ConfigurationTable;
} EFI_SYSTEM_TABLE;

typedef struct {
    uint32_t Revision; EFI_HANDLE ParentHandle; EFI_SYSTEM_TABLE* SystemTable;
    EFI_HANDLE DeviceHandle; EFI_DEVICE_PATH_PROTOCOL* FilePath; void* Reserved; uint32_t LoadOptionsSize;
    void* LoadOptions; void* ImageBase; uint64_t ImageSize; uint32_t ImageCodeType;
    uint32_t ImageDataType; void* Unload;
} EFI_LOADED_IMAGE_PROTOCOL;


typedef struct EFI_BLOCK_IO_MEDIA {
    uint32_t MediaId;
    BOOLEAN RemovableMedia;
    BOOLEAN MediaPresent;
    BOOLEAN LogicalPartition;
    BOOLEAN ReadOnly;
    BOOLEAN WriteCaching;
    uint32_t BlockSize;
    uint32_t IoAlign;
    EFI_LBA LastBlock;
    EFI_LBA LowestAlignedLba;
    uint32_t LogicalBlocksPerPhysicalBlock;
    uint32_t OptimalTransferLengthGranularity;
} EFI_BLOCK_IO_MEDIA;

struct EFI_BLOCK_IO_PROTOCOL;
typedef EFI_STATUS (EFIAPI *EFI_BLOCK_RESET)(struct EFI_BLOCK_IO_PROTOCOL*, BOOLEAN);
typedef EFI_STATUS (EFIAPI *EFI_BLOCK_READ)(struct EFI_BLOCK_IO_PROTOCOL*, uint32_t, EFI_LBA, UINTN, void*);
typedef EFI_STATUS (EFIAPI *EFI_BLOCK_WRITE)(struct EFI_BLOCK_IO_PROTOCOL*, uint32_t, EFI_LBA, UINTN, void*);
typedef EFI_STATUS (EFIAPI *EFI_BLOCK_FLUSH)(struct EFI_BLOCK_IO_PROTOCOL*);
typedef struct EFI_BLOCK_IO_PROTOCOL {
    uint64_t Revision;
    EFI_BLOCK_IO_MEDIA* Media;
    EFI_BLOCK_RESET Reset;
    EFI_BLOCK_READ ReadBlocks;
    EFI_BLOCK_WRITE WriteBlocks;
    EFI_BLOCK_FLUSH FlushBlocks;
} EFI_BLOCK_IO_PROTOCOL;

#pragma pack(push, 1)
typedef struct {
    EFI_GUID PartitionTypeGUID;
    EFI_GUID UniquePartitionGUID;
    EFI_LBA StartingLBA;
    EFI_LBA EndingLBA;
    uint64_t Attributes;
    CHAR16 PartitionName[36];
} EFI_PARTITION_ENTRY;

typedef struct {
    uint32_t Revision;
    uint32_t Type;
    uint8_t System;
    uint8_t Reserved[7];
    union {
        uint8_t Mbr[16];
        EFI_PARTITION_ENTRY Gpt;
    } Info;
} EFI_PARTITION_INFO_PROTOCOL;
#pragma pack(pop)

#define PARTITION_TYPE_GPT 0x02u
