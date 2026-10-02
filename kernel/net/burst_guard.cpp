#include "burst_guard.hpp"
namespace peregrinus::net::burst {
void Guard::reset(){for(auto& e:entries_)e={};replacement_=0;denied_total_=0;}
bool Guard::admit(uint32_t source,uint64_t seq){
    Entry* slot=nullptr;for(auto& e:entries_)if(e.used&&e.source==source){slot=&e;break;}
    if(!slot){for(auto& e:entries_)if(!e.used){slot=&e;break;}}
    if(!slot){slot=&entries_[replacement_];replacement_=(replacement_+1)%capacity;}
    if(!slot->used||slot->source!=source||seq<slot->window_start||seq-slot->window_start>=window_events){*slot={true,source,seq,0,0};}
    if(slot->admitted>=max_new_flows_per_window){++slot->denied;++denied_total_;return false;}
    ++slot->admitted;return true;
}
bool self_test(){Guard g;g.reset();for(uint16_t i=0;i<max_new_flows_per_window;++i)if(!g.admit(1,i))return false;if(g.admit(1,max_new_flows_per_window))return false;if(g.total_denied()!=1)return false;if(!g.admit(1,window_events+1))return false;if(!g.admit(2,window_events+2))return false;return true;}
}
