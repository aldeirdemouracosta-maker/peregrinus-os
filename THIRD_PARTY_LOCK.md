# Peregrinus OS — Third-party version lock

## Limine

- Upstream: `limine-bootloader/limine`
- Pinned release for distributable boot images: **v12.9.1**.
- The kernel source package carries only the small protocol/header subset needed by Peregrinus.
- Secure Boot deployment uses a pinned Limine EFI binary whose configuration BLAKE2b hash is enrolled before that EFI binary is signed.
- Muro 1.0 retains the trusted-boot path built on Limine's documented Boot Loader Interface handling (`LoaderEntryOneShot`) and measured-boot support.

## Cryptography

No new third-party crypto library is vendored in Muro 1.0. The kernel keeps its internal SHA-256 integrity seal, while boot-file authentication remains delegated to UEFI Secure Boot + pinned Limine configuration/file hashes.

## TPM / TCG

- Interface: UEFI `EFI_TCG2_PROTOCOL`.
- TPM command model: TPM 2.0 Library (current TCG family 2.0 specifications).
- Peregrinus boot path is read-only with respect to TPM NV in the kernel and UEFI boot controller: no `NV_DefineSpace`, `NV_Increment`, `NV_Write`, or `NV_UndefineSpace` command is present there. The epoch-commit tooling from Jó 0.6 is retained for future upgrades, and Muro 1.0 adds no TPM mutation to the boot path. The temporary UEFI boot-success variable and legacy NVRAM journal code have been removed. The IA_RECOVERY success writer is disabled in SAFE builds and enabled only by explicit recovery-commit qualification profiles.
- `tpm2-tools` is optional for external inspection/provisioning and is not vendored.


## UEFI storage protocols — Muro 1.0

Muro 1.0 uses only standard UEFI interoperability ABIs for `EFI_BLOCK_IO_PROTOCOL`, `EFI_PARTITION_INFO_PROTOCOL`, Device Path and Loaded Image. No EDK II storage driver source is vendored. The normal EFI controller compiles the live recovery-journal backend out; the explicit test controller enables it with `PEREGRINUS_RECOVERY_JOURNAL_LIVE=1`.


## Muro 0.1

No third-party firewall code was imported. Linux Netfilter/nftables are architectural references only. The Peregrinus firewall policy engine in `kernel/security/firewall.*` is project code.

## Muro 1.0 network qualification

No third-party network stack or NIC driver code is vendored. Intel 8254x documentation, RFC Ethernet/IPv4/ARP/ICMP semantics, QEMU e1000 identifiers and Linux e1000 register/PCI-ID definitions are interoperability references only. The e1000 polling driver, ARP/ICMP control plane, reply guard, burst guard and datapath are independently written Peregrinus project code. The SAFE build compiles the live ownership path out; the QEMU qualification profile enables it explicitly.
