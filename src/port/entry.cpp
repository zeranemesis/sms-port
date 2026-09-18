// Adapted from the Party Board (Marioparty4) port layer, credit: TwilitRealm
#include <aurora/main.h>

#include "port/main.h"
#include "port/recomp_dolphin_sdk.h"
#include "port/recomp_host.h"

#include <cstring>

int main(int argc, char *argv[])
{
    if (argc == 2 && std::strcmp(argv[1], "--recomp-hostcall-self-test") == 0) {
        return sms::recomp::run_self_test() ? 0 : 1;
    }
    if (argc == 2 && std::strcmp(argv[1], "--recomp-dolphin-sdk-self-test") == 0) {
        return sms::recomp::dolphin_sdk::run_dolphin_sdk_self_test() ? 0 : 1;
    }
    return port_main(argc, argv);
}
