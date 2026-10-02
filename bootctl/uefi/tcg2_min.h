#pragma once
#include "uefi_min.h"

typedef struct { uint8_t Major; uint8_t Minor; } EFI_TCG2_VERSION;
typedef struct {
    uint8_t Size;
    EFI_TCG2_VERSION StructureVersion;
    EFI_TCG2_VERSION ProtocolVersion;
    uint32_t HashAlgorithmBitmap;
    uint32_t SupportedEventLogs;
    BOOLEAN TPMPresentFlag;
    uint16_t MaxCommandSize;
    uint16_t MaxResponseSize;
    uint32_t ManufacturerID;
    uint32_t NumberOfPcrBanks;
    uint32_t ActivePcrBanks;
} EFI_TCG2_BOOT_SERVICE_CAPABILITY;

struct EFI_TCG2_PROTOCOL;
typedef EFI_STATUS (EFIAPI *EFI_TCG2_GET_CAPABILITY)(struct EFI_TCG2_PROTOCOL*,EFI_TCG2_BOOT_SERVICE_CAPABILITY*);
typedef EFI_STATUS (EFIAPI *EFI_TCG2_SUBMIT_COMMAND)(struct EFI_TCG2_PROTOCOL*,uint32_t,uint8_t*,uint32_t,uint8_t*);
typedef struct EFI_TCG2_PROTOCOL {
    EFI_TCG2_GET_CAPABILITY GetCapability;
    void* GetEventLog;
    void* HashLogExtendEvent;
    EFI_TCG2_SUBMIT_COMMAND SubmitCommand;
    void* GetActivePcrBanks;
    void* SetActivePcrBanks;
    void* GetResultOfSetActivePcrBanks;
} EFI_TCG2_PROTOCOL;
