#include "security/watchdog.hpp"
int main(){return peregrinus::security::watchdog::self_test()?0:1;}
