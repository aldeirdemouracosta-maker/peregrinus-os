#pragma once
namespace peregrinus::cpu {
void report();
bool hypervisor_present();
bool qemu_qualification_environment();
const char* hypervisor_vendor();
}
