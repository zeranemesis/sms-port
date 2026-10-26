#include "pc_services.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <cassert>
#include <iostream>
#include <unistd.h>
int main() {
 namespace fs=std::filesystem;
 const auto root=fs::path("/data/local/tmp")/("sms-card-test-"+std::to_string(getpid()));
 fs::create_directories(root/"card");
 setenv("SMS_SAVE_DIR",(root/"card").c_str(),1);
 setenv("SMS_FRONTEND_DIR",(root/"frontend").c_str(),1);
 std::ofstream(root/"card/index.txt")<<"test card";
 std::ofstream(root/"card/test.dat")<<"original";
 std::ofstream(root/"card/ignore.txt")<<"skip";
 fs::create_symlink(root/"card/test.dat",root/"card/link.dat");
 auto a=sms_frontend::backup_saves(),b=sms_frontend::backup_saves();
 assert(a.ok&&b.ok&&a.path!=b.path);
 assert(!fs::exists(fs::path(a.path)/"link.dat")&&!fs::exists(fs::path(a.path)/"ignore.txt"));
 assert(sms_frontend::save_backups().size()==2);
 assert(!sms_frontend::prepare_save_restore(a.path+"/../bad").ok);
 assert(sms_frontend::prepare_save_restore(a.path).ok);
 assert(std::string(getenv("SMS_SAVE_DIR"))==(root/"card").string());
 assert(sms_frontend::activate_pending_save_restore().ok);
 auto selected=std::string(getenv("SMS_SAVE_DIR"));
 assert(selected!=(root/"card").string());
 assert(fs::exists(fs::path(selected)/"test.dat")&&fs::exists(root/"card/test.dat"));
 assert(!fs::exists(root/"frontend/pending-card.txt"));
 assert(sms_frontend::activate_pending_save_restore().ok);
 assert(selected==getenv("SMS_SAVE_DIR"));
 fs::remove_all(root);
 std::cout<<"PASS: Android backup, enumeration, unique copies, symlink exclusion, deferred restore, original preservation and restart selection\n";
}
