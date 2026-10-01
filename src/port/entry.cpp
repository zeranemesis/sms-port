// Adapted from the Party Board (Marioparty4) port layer, credit: TwilitRealm
#include <aurora/main.h>

#include "port/main.h"
#include "port/recomp_boot.h"
#include "port/recomp_crash.h"
#include "port/recomp_dolphin_sdk.h"
#include "port/recomp_gx_copy.h"
#include "port/recomp_gx_fifo.h"
#include "port/recomp_host.h"
#include "port/recomp_interrupt.h"
#include "port/recomp_pad.h"

#include <cstring>

int main(int argc, char *argv[])
{
    sms::recomp::crash::install();
    if (argc == 2 && std::strcmp(argv[1], "--recomp-hostcall-self-test") == 0) {
        return sms::recomp::run_self_test() ? 0 : 1;
    }
    if (argc == 2 && std::strcmp(argv[1], "--recomp-dolphin-sdk-self-test") == 0) {
        return sms::recomp::dolphin_sdk::run_dolphin_sdk_self_test() ? 0 : 1;
    }
    if (argc == 2 && std::strcmp(argv[1], "--recomp-mmio-self-test") == 0) {
        return sms::recomp::run_mmio_self_test() ? 0 : 1;
    }
    if (argc == 2 && std::strcmp(argv[1], "--recomp-decrementer-self-test") == 0) {
        return sms::recomp::interrupt::run_decrementer_self_test() ? 0 : 1;
    }
    if (argc == 2 && std::strcmp(argv[1], "--recomp-pe-lane-self-test") == 0) {
        return sms::recomp::interrupt::run_pe_lane_self_test() ? 0 : 1;
    }
    if (argc == 2 && std::strcmp(argv[1], "--recomp-pad-self-test") == 0) {
        return sms::recomp::pad::run_pad_self_test() ? 0 : 1;
    }
    if (argc == 2 && std::strcmp(argv[1], "--recomp-locked-cache-self-test") == 0) {
        return sms::recomp::run_locked_cache_self_test() ? 0 : 1;
    }
    if (argc == 2 && std::strcmp(argv[1], "--recomp-fpu-context-self-test") == 0) {
        return sms::recomp::run_fpu_context_self_test() ? 0 : 1;
    }
    if (argc == 2 && std::strcmp(argv[1], "--recomp-fifo-stream-self-test") == 0) {
        return sms::recomp::gx_fifo::run_fifo_stream_self_test() ? 0 : 1;
    }
    if (argc == 2 && std::strcmp(argv[1], "--recomp-gx-copy-self-test") == 0) {
        return sms::recomp::gx_copy::run_gx_copy_self_test() ? 0 : 1;
    }
    return port_main(argc, argv);
}
