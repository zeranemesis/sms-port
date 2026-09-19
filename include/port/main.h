// Adapted from the Party Board (Marioparty4) port layer, credit: TwilitRealm
#pragma once

#include <filesystem>

namespace sms {
extern std::filesystem::path ConfigPath;
extern bool IsRunning;
}

extern "C" int port_main(int argc, char* argv[]);
