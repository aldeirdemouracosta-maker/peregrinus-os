#include "watchdog.hpp"
namespace peregrinus::security::watchdog {
namespace { State g{}; }
void reset(){g={Stage::none,false,0};}
bool checkpoint(Stage next){
    if(g.tripped)return false;
    const uint8_t want=uint8_t(g.current)+1u;
    if(uint8_t(next)!=want){g.tripped=true;return false;}
    g.current=next;++g.checkpoints;return true;
}
const State& state(){return g;}
const char* stage_name(Stage s){switch(s){case Stage::boot_entry:return "BOOT_ENTRY";case Stage::cpu_ready:return "CPU_READY";case Stage::memory_ready:return "MEMORY_READY";case Stage::integrity_ready:return "INTEGRITY_READY";case Stage::devices_ready:return "DEVICES_READY";case Stage::recovery_ready:return "RECOVERY_READY";case Stage::policy_ready:return "POLICY_READY";case Stage::complete:return "COMPLETE";default:return "NONE";}}
bool self_test(){reset();if(!checkpoint(Stage::boot_entry)||!checkpoint(Stage::cpu_ready))return false;if(checkpoint(Stage::integrity_ready))return false;if(!state().tripped)return false;reset();for(uint8_t i=1;i<=8;++i)if(!checkpoint(Stage(i)))return false;return !state().tripped&&state().current==Stage::complete&&state().checkpoints==8;}
}
