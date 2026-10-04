# Third-party notices

## Limine Boot Protocol / Bootloader

Peregrinus uses a small subset of the Limine protocol structures/constants in source form and integrates with the Limine bootloader at deployment time.

- Project: Limine
- Upstream: https://github.com/limine-bootloader/limine
- Protocol/header license: 0BSD
- Copyright: Mintsuki and contributors
- Pinned deployment release: v12.9.1

The Limine bootloader binary is **not** embedded in this source ZIP. `scripts/fetch-limine.sh` retrieves the pinned release when the user intentionally builds a boot image.

## Boot Loader Interface

Jó 0.7 implements the standard EFI Boot Loader Interface variable `LoaderEntryOneShot` with vendor GUID `4a67b082-0a4c-41cf-b6c7-440b29bb8c4f`. This is an interoperability interface/specification; no systemd source code is copied into Peregrinus.

## Cryptographic design

Peregrinus intentionally avoids adding a new public-key crypto library to the kernel at this stage. UEFI Secure Boot authenticates the signed EFI applications, Limine authenticates its enrolled configuration and BLAKE2b-pinned kernel files, and the kernel keeps its existing SHA-256 `.text` seal as defense in depth.

## TCG EFI / TPM 2.0 interfaces

Jó 0.7 uses the standard EFI TCG2 protocol ABI and TPM 2.0 command wire formats for `GetCapability`, `TPM2_NV_ReadPublic`, and `TPM2_NV_Read`. These are interoperability specifications; Peregrinus does not vendor a TPM software stack in the boot controller.

Administrative inspection and the retained epoch-commit tooling may use the separately installed `tpm2-tools` project. It is not bundled into Peregrinus; the UEFI controller and kernel do not link to it.

## Linux kernel technical references — Jó 0.8

Jó 0.8 consults the upstream Linux iTCO watchdog, LPC/ICH resource and ATA command definitions as public hardware-interoperability references. No Linux driver source is vendored or linked into Peregrinus. The Peregrinus implementation is independently written and remains governed by the project license; upstream Linux files retain their own GPL-2.0/GPL-2.0+ licensing.


## UEFI Block I/O / Partition Information — Jó 1.0

Jó 1.0 implements the standard UEFI Block I/O and Partition Information protocol ABIs directly for interoperability. The controller uses firmware-produced GPT partition handles to locate `IA_RECOVERY`, validates that it belongs to the same physical disk as the boot EFI image, and performs a narrowly scoped A/B journal transaction. No EDK II source code is copied or linked.


## Jó 1.0 direct recovery commit

The post-boot success transaction uses Peregrinus-owned AHCI code and standard ATA command values already documented in the project. No third-party storage implementation is linked or copied. The SAFE profile leaves this write path disabled; RECOVERY-LIVE is an explicit qualification profile.

## Network protocol / NIC references — Muro 1.0

Muro 1.0 consults the Intel 8254x Software Developer Manual for descriptor-ring/register semantics and current Linux/QEMU material for interoperability checks around the Intel 82540EM (`8086:100e`). ARP/IPv4/ICMP wire formats are implemented independently from protocol specifications. No Intel sample code, QEMU code, Linux driver code, lwIP, Netfilter, nftables or other network stack is copied or linked into Peregrinus.

## font8x8 (framebuffer text console)

`kernel/console/font8x8.hpp` contains the glyph tables of `font8x8_basic.h` and `font8x8_ext_latin.h` from
https://github.com/dhepper/font8x8 by Daniel Hepper, **Public Domain**, derived from the
public-domain IBM VGA fonts (via Marcel Sondaar). Glyph data unchanged; only converted to a
`const uint8_t` C++ table.
