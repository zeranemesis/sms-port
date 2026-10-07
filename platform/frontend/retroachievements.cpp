#include "retroachievements.h"
#include "retroachievements_http.h"
#include "../disc/gcdisc.h"
#include <rc_client.h>
#include <rc_hash.h>
#include <rc_consoles.h>
#include <rc_error.h>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <algorithm>

namespace sms_frontend { namespace ra {
namespace {
struct Queue { std::mutex mutex; std::deque<std::function<void()>> work; };
rc_client_t* client=nullptr;
std::shared_ptr<Queue> queue;
unsigned long long generation=0;
std::string message="RetroAchievements : non connecté", discPath, userAgent;
bool gameLoaded=false;
// Native pointers, allocation order, endianness and structure layout differ
// from GameCube RAM. Reading the reserved MEM1 arena is not a translation.
// Missing reads must fail, rather than inventing zeroes or native addresses.
uint32_t RC_CCONV read_memory(uint32_t, uint8_t*, uint32_t, rc_client_t*) { return 0; }
void RC_CCONV server_call(const rc_api_request_t* request, rc_client_server_callback_t callback,
                         void* data, rc_client_t*) {
    const std::string url=request->url, body=request->post_data?request->post_data:"";
    const std::string type=request->content_type?request->content_type:"application/x-www-form-urlencoded";
    const auto current=generation; const auto agent=userAgent;
    std::weak_ptr<Queue> weak=queue;
    std::thread([url,body,type,current,agent,weak,callback,data] {
        auto response=ra_http::request(url,body,type,agent);
        if(auto q=weak.lock()) {
            std::lock_guard<std::mutex> lock(q->mutex);
            q->work.emplace_back([current,response=std::move(response),callback,data] {
                if(current!=generation || !client) return;
                rc_api_server_response_t result{};
                result.body=response.body.c_str(); result.body_length=response.body.size();
                result.http_status_code=response.status?response.status:RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR;
                callback(&result,data);
            });
        }
    }).detach();
}
struct DiscReader { GCDisc* disc=nullptr; int64_t offset=0; std::vector<uint8_t> extracted; };
std::vector<uint8_t> file_bytes(const std::filesystem::path& path) {
    std::ifstream f(path,std::ios::binary|std::ios::ate); if(!f) return {};
    const auto size=f.tellg(); if(size<=0 || size>16*1024*1024) return {};
    std::vector<uint8_t> out(static_cast<size_t>(size)); f.seekg(0); if(!f.read(reinterpret_cast<char*>(out.data()),size)) return {}; return out;
}
uint32_t be32(const uint8_t* p) { return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3]; }
void* RC_CCONV hash_open(const char* path) {
    std::filesystem::path root=std::filesystem::u8path(path);
    std::error_code ec;
    if(!std::filesystem::is_directory(root,ec)) {
        GCDisc* d=gcdisc_open(path,0); if(!d) return nullptr; return new DiscReader{d,0,{}};
    }
    if(root.filename()=="files") root=root.parent_path();
    auto boot=file_bytes(root/"sys"/"boot.bin"), bi=file_bytes(root/"sys"/"bi2.bin"),
         app=file_bytes(root/"sys"/"apploader.img"), dol=file_bytes(root/"sys"/"main.dol");
    if(boot.size()!=0x440 || bi.size()!=0x2000 || app.size()<0x20 || dol.size()<0x100) return nullptr;
    if(std::memcmp(boot.data(),"GMSP01",6)!=0 || be32(boot.data()+0x1c)!=0xc2339f3d) return nullptr;
    const uint32_t dolOffset=be32(boot.data()+0x420);
    const size_t size=std::max(size_t(dolOffset)+dol.size(),size_t(0x2440)+app.size());
    if(size>16*1024*1024 || dolOffset<0x2440+app.size()) return nullptr;
    auto* d=new DiscReader; d->extracted.resize(size);
    std::copy(boot.begin(),boot.end(),d->extracted.begin());
    std::copy(bi.begin(),bi.end(),d->extracted.begin()+0x440);
    std::copy(app.begin(),app.end(),d->extracted.begin()+0x2440);
    std::copy(dol.begin(),dol.end(),d->extracted.begin()+dolOffset);
    return d;
}
void RC_CCONV hash_seek(void* p,int64_t offset,int origin) {
    auto* d=static_cast<DiscReader*>(p);
    if(origin==SEEK_SET) d->offset=offset;
    else if(origin==SEEK_CUR) d->offset+=offset;
    else if(origin==SEEK_END) d->offset=int64_t(d->disc?gcdisc_size(d->disc):d->extracted.size())+offset;
}
int64_t RC_CCONV hash_tell(void* p) { return static_cast<DiscReader*>(p)->offset; }
size_t RC_CCONV hash_read(void* p,void* buffer,size_t bytes) {
    auto* d=static_cast<DiscReader*>(p); if(d->offset<0 || bytes>0xffffffffu) return 0;
    if(!d->disc) {
        if(uint64_t(d->offset)>=d->extracted.size()) return 0;
        auto got=std::min(bytes,d->extracted.size()-size_t(d->offset));
        std::memcpy(buffer,d->extracted.data()+d->offset,got); d->offset+=got; return got;
    }
    auto got=gcdisc_read(d->disc,uint64_t(d->offset),buffer,uint32_t(bytes)); d->offset+=got; return got;
}
void RC_CCONV hash_close(void* p) { auto* d=static_cast<DiscReader*>(p); if(d->disc) gcdisc_close(d->disc); delete d; }
std::string disc_hash(const std::string& path) {
    rc_hash_iterator_t it{}; rc_hash_initialize_iterator(&it,path.c_str(),nullptr,0);
    it.callbacks.filereader.open=hash_open; it.callbacks.filereader.seek=hash_seek;
    it.callbacks.filereader.tell=hash_tell; it.callbacks.filereader.read=hash_read; it.callbacks.filereader.close=hash_close;
    char hash[33]{}; int ok=rc_hash_generate(hash,RC_CONSOLE_GAMECUBE,&it);
    rc_hash_destroy_iterator(&it);
    // Extracted folders omit padding; reconstruction is accepted only if the
    // official hasher produces the published original PAL identifier.
    std::error_code ec;
    if(std::filesystem::is_directory(std::filesystem::u8path(path),ec) && std::strcmp(hash,"58d598ccfa63fb89b81e8ead70f2dbdb")) return "";
    return ok?hash:"";
}
void load_game() {
    if(discPath.empty()) { message="Connecté. Une image ISO/CISO est nécessaire pour identifier le jeu ; traduction mémoire native indisponible."; return; }
    message="Identification RetroAchievements du disque…";
    const auto path=discPath; const auto current=generation; std::weak_ptr<Queue> weak=queue;
    std::thread([path,current,weak] {
        auto hash=disc_hash(path);
        if(auto q=weak.lock()) {
            std::lock_guard<std::mutex> lock(q->mutex);
            q->work.emplace_back([current,hash] {
                if(current!=generation || !client) return;
                if(hash.empty()) { message="Connecté. Disque illisible pour RetroAchievements : utiliser une ISO/CISO originale."; return; }
                rc_client_begin_load_game(client,hash.c_str(),[](int result,const char* error,rc_client_t* c,void*) {
                    if(result!=RC_OK) { message=std::string("Compte connecté ; jeu non chargé : ")+(error?error:"aucun jeu compatible"); return; }
                    const auto* game=rc_client_get_game_info(c);
                    if(!game || game->id!=6049) { rc_client_unload_game(c); message="Ce disque ne correspond pas au jeu RetroAchievements 6049."; return; }
                    gameLoaded=true;
                    message="Super Mario Sunshine : succès du compte chargés. Déblocage suspendu : traduction mémoire native non validée.";
                },nullptr);
            });
        }
    }).detach();
}
void RC_CCONV logged_in(int result,const char* error,rc_client_t* c,void*) {
    if(result!=RC_OK) { message=std::string("Connexion refusée : ")+(error?error:"erreur réseau"); return; }
    const auto* user=rc_client_get_user_info(c);
    message=std::string("Connecté : ")+(user?user->display_name:""); load_game();
}
}
void initialize() {
    if(client) return;
    if(!ra_http::available()) { message="RetroAchievements : HTTPS indisponible dans cette compilation."; return; }
    queue=std::make_shared<Queue>(); client=rc_client_create(read_memory,server_call);
    rc_client_set_hardcore_enabled(client,0);
    // Until console-state translation has been validated, the official client
    // is account/list only. It cannot submit invented achievement events.
    rc_client_set_spectator_mode_enabled(client,1);
    char clause[128]{}; rc_client_get_user_agent_clause(client,clause,sizeof(clause));
    userAgent=std::string("SunshinePALPort/0.1.0 (Windows x64) ")+clause;
    if(const char* path=std::getenv("SMS_DISC_IMAGE")) discPath=path;
    if(discPath.empty()) if(const char* path=std::getenv("SMS_DISC_ROOT")) discPath=path;
}
void shutdown() { ++generation; if(client) rc_client_destroy(client); client=nullptr; queue.reset(); gameLoaded=false; }
void pump() {
    if(!client || !queue) return;
    std::deque<std::function<void()>> ready;
    { std::lock_guard<std::mutex> lock(queue->mutex); ready.swap(queue->work); }
    for(auto& fn:ready) fn();
    rc_client_idle(client);
}
void game_tick() { /* No do_frame until the console-state translator is validated. */ }
void login(const std::string& user,const std::string& password) {
    initialize(); if(!client) return;
    if(user.empty() || password.empty()) { message="Saisir le nom RetroAchievements et le mot de passe."; return; }
    ++generation; rc_client_logout(client); gameLoaded=false;
    message="Connexion à RetroAchievements…";
    rc_client_begin_login_with_password(client,user.c_str(),password.c_str(),logged_in,nullptr);
}
void logout() { ++generation; if(client) rc_client_logout(client); gameLoaded=false; message="RetroAchievements : déconnecté"; }
void set_disc_path(const std::string& path) { discPath=path; if(client && rc_client_get_user_info(client)) { gameLoaded=false; rc_client_unload_game(client); load_game(); } }
std::string status() { return message; }
std::string username() { const auto* user=client?rc_client_get_user_info(client):nullptr; return user?user->display_name:""; }
bool evaluation_available() { return false; }
std::vector<Achievement> achievements() {
    std::vector<Achievement> out; if(!client || !gameLoaded) return out;
    auto* list=rc_client_create_achievement_list(client,RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE,RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
    if(!list) return out;
    for(uint32_t b=0;b<list->num_buckets;++b) for(uint32_t i=0;i<list->buckets[b].num_achievements;++i) {
        const auto* a=list->buckets[b].achievements[i]; if(a->id>=101000000u) continue;
        out.push_back({a->id,a->title,a->description,a->points,(a->unlocked & RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE)!=0,a->measured_progress});
    }
    rc_client_destroy_achievement_list(list); return out;
}
} }
