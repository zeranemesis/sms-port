#include "pc_services.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#include <wincrypt.h>
#endif

namespace sms_frontend {
namespace {
std::mutex services_mutex;
UpdateStatus update;
bool installation_prepared = false;
std::vector<std::string> unlocked_ids;
bool achievements_loaded = false;
std::string environment(const char* name) {
    const char* value = std::getenv(name);
    return value ? value : "";
}
std::string data_directory() {
    std::string path = environment("SMS_FRONTEND_DIR");
    if (!path.empty()) return path;
    path = environment("APPDATA");
    if (!path.empty()) return path + "/sms-port/frontend";
    path = environment("XDG_DATA_HOME");
    if (!path.empty()) return path + "/sms-port/frontend";
    path = environment("HOME");
    return (path.empty() ? "." : path) + "/.local/share/sms-port/frontend";
}
#ifdef _WIN32
std::wstring wide(const std::string& value) {
    if (value.empty()) return L"";
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), (int)value.size(), NULL, 0);
    if (!count) return L"";
    std::wstring result(count, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), (int)value.size(), &result[0], count);
    return result;
}
std::string utf8(const std::wstring& value) {
    int count = WideCharToMultiByte(CP_UTF8, 0, value.data(), (int)value.size(), NULL, 0, NULL, NULL);
    std::string result(count, '\0');
    if (count) WideCharToMultiByte(CP_UTF8, 0, value.data(), (int)value.size(), &result[0], count, NULL, NULL);
    return result;
}
bool mkdirs(const std::string& path) {
    std::wstring p = wide(path);
    if (p.empty()) return false;
    for (size_t i = 1; i <= p.size(); ++i) {
        if (i != p.size() && p[i] != L'/' && p[i] != L'\\') continue;
        if (i == 2 && p[1] == L':') continue;
        std::wstring prefix = p.substr(0, i);
        if (!CreateDirectoryW(prefix.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
    }
    return true;
}
bool copy_card(const std::string& from, const std::string& to, std::string& error) {
    // A CARD image consists only of index.txt and flat .dat/.stat files.
    // Do not traverse symlinks, junctions, subdirectories, or unknown files.
    DWORD root_attributes = GetFileAttributesW(wide(from).c_str());
    if (root_attributes == INVALID_FILE_ATTRIBUTES || !(root_attributes & FILE_ATTRIBUTE_DIRECTORY)
        || (root_attributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
        error = "Invalid card directory."; return false;
    }
    std::vector<std::wstring> names;
    WIN32_FIND_DATAW file;
    HANDLE search = FindFirstFileW(wide(from + "/*").c_str(), &file);
    if (search == INVALID_HANDLE_VALUE) { error = "Cannot read card directory."; return false; }
    bool index = false;
    do {
        std::wstring name = file.cFileName;
        if (name == L"." || name == L"..") continue;
        if (file.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) continue;
        bool accepted = name == L"index.txt";
        if (name.size() >= 4) accepted = accepted || name.substr(name.size()-4) == L".dat";
        if (name.size() >= 5) accepted = accepted || name.substr(name.size()-5) == L".stat";
        if (!accepted) continue;
        index = index || name == L"index.txt";
        names.push_back(name);
    } while (FindNextFileW(search, &file));
    DWORD find_error = GetLastError();
    FindClose(search);
    if (find_error != ERROR_NO_MORE_FILES || !index) { error = "Card index missing or unreadable."; return false; }
    if (!mkdirs(to)) { error = "Cannot create destination."; return false; }
    for (size_t i = 0; i < names.size(); ++i) {
        if (!CopyFileW(wide(from + "/" + utf8(names[i])).c_str(), wide(to + "/" + utf8(names[i])).c_str(), TRUE)) {
            error = "Card copy failed; original card was preserved."; return false;
        }
    }
    return true;
}
#else
bool mkdirs(const std::string&) { return false; }
bool copy_card(const std::string&, const std::string&, std::string& error) {
    error = "Card backup is currently supported on Windows."; return false;
}
#endif
std::string unique_directory(const std::string& category) {
    std::ostringstream out;
    out << data_directory() << "/" << category << "/" << (long long)std::time(NULL);
#ifdef _WIN32
    out << "-" << GetCurrentProcessId() << "-" << GetTickCount64();
#endif
    return out.str();
}
bool atomic_text(const std::string& path, const std::string& text) {
    const std::string temp = path + ".tmp";
    std::ofstream output(temp.c_str(), std::ios::binary | std::ios::trunc);
    output << text;
    output.close();
    if (!output) return false;
#ifdef _WIN32
    return MoveFileExW(wide(temp).c_str(), wide(path).c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return std::rename(temp.c_str(), path.c_str()) == 0;
#endif
}
const char* ids[] = {"first_shine", "ten_shines", "fifty_shines", "all_shines", "first_blue_coin", "all_blue_coins"};
const char* titles[] = {"Premier Soleil", "Dix Soleils", "Cinquante Soleils", "Les 120 Soleils", "Première pièce bleue", "Les 240 pièces bleues"};
void load_achievements() {
    if (achievements_loaded) return;
    achievements_loaded = true;
    std::ifstream in((data_directory()+"/achievements.txt").c_str());
    std::string id;
    while (std::getline(in, id)) for (unsigned i = 0; i < 6; ++i)
        if (id == ids[i] && std::find(unlocked_ids.begin(), unlocked_ids.end(), id) == unlocked_ids.end()) unlocked_ids.push_back(id);
}

#ifdef _WIN32
struct Internet {
    HMODULE dll;
    decltype(&InternetOpenW) open;
    decltype(&InternetOpenUrlW) url;
    decltype(&InternetReadFile) read;
    decltype(&InternetCloseHandle) close;
    decltype(&InternetSetOptionW) option;
    decltype(&HttpQueryInfoW) query;
    Internet() : dll(LoadLibraryW(L"wininet.dll")) {
        open = dll ? (decltype(open))GetProcAddress(dll,"InternetOpenW") : NULL;
        url = dll ? (decltype(url))GetProcAddress(dll,"InternetOpenUrlW") : NULL;
        read = dll ? (decltype(read))GetProcAddress(dll,"InternetReadFile") : NULL;
        close = dll ? (decltype(close))GetProcAddress(dll,"InternetCloseHandle") : NULL;
        option = dll ? (decltype(option))GetProcAddress(dll,"InternetSetOptionW") : NULL;
        query = dll ? (decltype(query))GetProcAddress(dll,"HttpQueryInfoW") : NULL;
    }
    ~Internet() { if (dll) FreeLibrary(dll); }
    bool valid() const { return open && url && read && close && option && query; }
};
bool fetch(const std::string& address, const std::string& destination, std::string& body, size_t limit, std::string& error) {
    Internet api;
    if (!api.valid()) { error = "Windows Internet API unavailable."; return false; }
    HINTERNET session = api.open(L"SMS-PAL-Frontend/1", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!session) { error = "Cannot open network session."; return false; }
    DWORD timeout = 20000;
    api.option(session, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof timeout);
    api.option(session, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof timeout);
    const wchar_t* headers = L"Accept: application/vnd.github+json\r\n";
    HINTERNET request = api.url(session, wide(address).c_str(), headers, (DWORD)-1,
        INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!request) { api.close(session); error = "HTTPS request failed."; return false; }
    DWORD status = 0, length = sizeof status;
    bool ok = api.query(request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &length, NULL) && status == 200;
    std::ofstream output;
    if (!destination.empty() && ok) { output.open(destination.c_str(), std::ios::binary|std::ios::trunc); ok = bool(output); }
    char buffer[65536]; DWORD count = 0; size_t total = 0;
    while (ok) {
        if (!api.read(request, buffer, sizeof buffer, &count)) { ok = false; break; }
        if (!count) break;
        if (total > limit || count > limit - total) { ok = false; error = "Download exceeds size limit."; break; }
        total += count;
        if (destination.empty()) body.append(buffer, count);
        else { output.write(buffer,count); ok = bool(output); }
    }
    if (output.is_open()) { output.close(); ok = ok && bool(output); }
    api.close(request); api.close(session);
    if (!ok && error.empty()) { std::ostringstream e; e << "Download failed (HTTP " << status << ")."; error = e.str(); }
    return ok;
}
bool sha256_file(const std::string& path, std::string& digest) {
    HMODULE dll = LoadLibraryW(L"advapi32.dll");
    if (!dll) return false;
    auto acquire = (decltype(&CryptAcquireContextW))GetProcAddress(dll,"CryptAcquireContextW");
    auto create = (decltype(&CryptCreateHash))GetProcAddress(dll,"CryptCreateHash");
    auto hash = (decltype(&CryptHashData))GetProcAddress(dll,"CryptHashData");
    auto get = (decltype(&CryptGetHashParam))GetProcAddress(dll,"CryptGetHashParam");
    auto destroy = (decltype(&CryptDestroyHash))GetProcAddress(dll,"CryptDestroyHash");
    auto release = (decltype(&CryptReleaseContext))GetProcAddress(dll,"CryptReleaseContext");
    HCRYPTPROV provider = 0; HCRYPTHASH handle = 0;
    bool ok = acquire && create && hash && get && destroy && release;
    if (ok) ok = acquire(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT) != 0;
    if (ok) ok = create(provider,CALG_SHA_256,0,0,&handle) != 0;
    std::ifstream in(path.c_str(), std::ios::binary);
    ok = ok && bool(in);
    char bytes[65536];
    while (ok && in) {
        in.read(bytes,sizeof bytes);
        if (in.gcount()) ok = hash(handle,(BYTE*)bytes,(DWORD)in.gcount(),0) != 0;
    }
    ok = ok && !in.bad();
    BYTE result[32]; DWORD size = sizeof result;
    if (ok) ok = get(handle,HP_HASHVAL,result,&size,0) && size == sizeof result;
    if (ok) { const char* hex="0123456789abcdef"; digest.clear(); for (unsigned i=0;i<32;++i) { digest+=hex[result[i]>>4];digest+=hex[result[i]&15]; } }
    if (handle) destroy(handle);
    if (provider) release(provider,0);
    FreeLibrary(dll); return ok;
}
#endif

// Read a JSON string without accepting control characters or truncated escapes.
bool json_string(const std::string& json, size_t& pos, std::string& value) {
    if (pos >= json.size() || json[pos++] != '"') return false;
    value.clear();
    while (pos < json.size()) {
        char c = json[pos++];
        if (c == '"') return true;
        if ((unsigned char)c < 32) return false;
        if (c == '\\') {
            if (pos >= json.size()) return false;
            c = json[pos++];
            if (c == 'n') c = '\n'; else if (c == 'r') c = '\r'; else if (c == 't') c = '\t';
            else if (c != '"' && c != '\\' && c != '/') return false;
        }
        value += c;
    }
    return false;
}
std::string field(const std::string& json, const std::string& key) {
    size_t p = 0;
    while (p < json.size()) {
        if (json[p] != '"') { ++p; continue; }
        std::string name;
        if (!json_string(json,p,name)) return "";
        size_t colon = json.find_first_not_of(" \r\n\t",p);
        if (name != key || colon == std::string::npos || json[colon] != ':') continue;
        p = json.find_first_not_of(" \r\n\t",colon+1);
        std::string result;
        return p != std::string::npos && json_string(json,p,result) ? result : "";
    }
    return "";
}
bool release_asset(const std::string& json, UpdateStatus& result) {
    result.version = field(json,"tag_name");
    size_t array = json.find("\"assets\"");
    if (result.version.empty() || array == std::string::npos) return false;
    array = json.find('[',array);
    if (array == std::string::npos) return false;
    bool quoted = false, escaped = false; int depth = 0; size_t start = 0;
    for (size_t p=array+1;p<json.size();++p) {
        char c=json[p];
        if (quoted) { if (escaped) escaped=false; else if(c=='\\') escaped=true; else if(c=='"') quoted=false; continue; }
        if(c=='"') { quoted=true; continue; }
        if(c==']' && depth==0) break;
        if(c=='{') { if(depth++==0) start=p; }
        if(c!='}' || --depth!=0) continue;
        std::string object=json.substr(start,p-start+1);
        std::string name=field(object,"name"), lower=name;
        std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char ch){return (char)std::tolower(ch);});
        if (lower.find("windows")==std::string::npos || lower.find("pal")==std::string::npos
            || lower.find("64")==std::string::npos || lower.size()<4 || lower.substr(lower.size()-4)!=".zip") continue;
        std::string url=field(object,"browser_download_url"), digest=field(object,"digest");
        const std::string prefix="https://github.com/zeranemesis/sms-port/releases/download/";
        if (url.compare(0,prefix.size(),prefix)!=0 || digest.size()!=71 || digest.substr(0,7)!="sha256:") continue;
        digest=digest.substr(7);
        if(digest.find_first_not_of("0123456789abcdef")!=std::string::npos) continue;
        result.asset_name=name;result.sha256=digest;result.download_url=url;return true;
    }
    return false;
}
} // namespace

std::string save_directory() {
    std::string p=environment("SMS_SAVE_DIR"); if(!p.empty()) return p;
    p=environment("XDG_DATA_HOME"); if(!p.empty()) return p+"/sms-port/card-a";
    p=environment("APPDATA"); if(!p.empty()) return p+"/sms-port/card-a";
    p=environment("HOME");return (p.empty()?".":p)+"/.local/share/sms-port/card-a";
}
ServiceResult backup_saves() {
    std::lock_guard<std::mutex> lock(services_mutex);
    std::string path=unique_directory("backups"),error;
    if(!copy_card(save_directory(),path,error)) return ServiceResult(false,error,path);
    if(!atomic_text(path+"/backup.complete","SMS-PAL card backup\n")) return ServiceResult(false,"Could not finalize backup.",path);
    return ServiceResult(true,"Save backup created.",path);
}
std::vector<std::string> save_backups() {
    std::lock_guard<std::mutex> lock(services_mutex); std::vector<std::string> result;
#ifdef _WIN32
    std::string root=data_directory()+"/backups";
    WIN32_FIND_DATAW f;HANDLE search=FindFirstFileW(wide(root+"/*").c_str(),&f);
    if(search!=INVALID_HANDLE_VALUE) {
        do { std::wstring n=f.cFileName;if(n==L"."||n==L"..")continue;
            if(!(f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||(f.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))continue;
            std::string path=root+"/"+utf8(n);
            if(GetFileAttributesW(wide(path+"/backup.complete").c_str())!=INVALID_FILE_ATTRIBUTES)result.push_back(path);
        }while(FindNextFileW(search,&f));FindClose(search);
    }
#endif
    std::sort(result.rbegin(),result.rend());return result;
}
ServiceResult prepare_save_restore(const std::string& backup) {
    std::lock_guard<std::mutex> lock(services_mutex);
    const std::string prefix=data_directory()+"/backups/";
    if(backup.compare(0,prefix.size(),prefix)!=0 || backup.find("..")!=std::string::npos)
        return ServiceResult(false,"Choose a backup from the backup list.");
    std::string marker;std::ifstream complete((backup+"/backup.complete").c_str());std::getline(complete,marker);
    if(marker!="SMS-PAL card backup")return ServiceResult(false,"Backup is incomplete.");
    std::string path=unique_directory("restored-cards"),error;
    if(!copy_card(backup,path,error))return ServiceResult(false,error,path);
    if(!mkdirs(data_directory()) || !atomic_text(data_directory()+"/pending-card.txt",path+"\n"))
        return ServiceResult(false,"Could not record pending restore.",path);
    return ServiceResult(true,"Restore prepared. Restart the game to use this card; the previous card is preserved.",path);
}
ServiceResult activate_pending_save_restore() {
    std::lock_guard<std::mutex> lock(services_mutex);
    std::string manifest=data_directory()+"/pending-card.txt",path;
    std::ifstream input(manifest.c_str());std::getline(input,path);input.close();
    if(path.empty()) {std::ifstream active((data_directory()+"/active-card.txt").c_str());std::getline(active,path);}
    if(path.empty())return ServiceResult(true,"No pending restore.");
    const std::string prefix=data_directory()+"/restored-cards/";
    if(path.compare(0,prefix.size(),prefix)!=0 || path.find("..")!=std::string::npos)return ServiceResult(false,"Invalid restored card path.");
#ifdef _WIN32
    DWORD attr=GetFileAttributesW(wide(path).c_str());
    if(attr==INVALID_FILE_ATTRIBUTES || !(attr&FILE_ATTRIBUTE_DIRECTORY) || (attr&FILE_ATTRIBUTE_REPARSE_POINT)
        || GetFileAttributesW(wide(path+"/index.txt").c_str())==INVALID_FILE_ATTRIBUTES)
        return ServiceResult(false,"Restored card is missing.");
    if(!SetEnvironmentVariableW(L"SMS_SAVE_DIR",wide(path).c_str()) || _putenv_s("SMS_SAVE_DIR",path.c_str())!=0)
        return ServiceResult(false,"Could not select restored card.");
    if(!atomic_text(data_directory()+"/active-card.txt",path+"\n"))return ServiceResult(false,"Could not persist restored card selection.");
    DeleteFileW(wide(manifest).c_str());
    return ServiceResult(true,"Restored card selected.",path);
#else
    return ServiceResult(false,"Restore requires Windows.");
#endif
}
void observe_game_progress(int shines,int coins) {
    if(shines<0 || shines>120 || coins<0 || coins>240)return;
    std::lock_guard<std::mutex> lock(services_mutex);load_achievements();
    bool condition[]={shines>=1,shines>=10,shines>=50,shines==120,coins>=1,coins==240};bool changed=false;
    for(unsigned i=0;i<6;++i)if(condition[i] && std::find(unlocked_ids.begin(),unlocked_ids.end(),ids[i])==unlocked_ids.end()) {
        unlocked_ids.push_back(ids[i]);changed=true;
    }
    if(changed && mkdirs(data_directory())) {std::string data;for(size_t i=0;i<unlocked_ids.size();++i)data+=unlocked_ids[i]+"\n";atomic_text(data_directory()+"/achievements.txt",data);}
}
std::vector<Achievement> achievements() {
    std::lock_guard<std::mutex> lock(services_mutex);load_achievements();std::vector<Achievement> result;
    for(unsigned i=0;i<6;++i){Achievement a;a.id=ids[i];a.title=titles[i];a.description="Progression réelle du fichier de sauvegarde chargé (succès local).";
        a.unlocked=std::find(unlocked_ids.begin(),unlocked_ids.end(),a.id)!=unlocked_ids.end();result.push_back(a);}return result;
}
UpdateStatus update_status(){std::lock_guard<std::mutex> lock(services_mutex);return update;}
std::string installed_release() {
#ifdef _WIN32
    wchar_t module[32768]; DWORD length = GetModuleFileNameW(NULL, module, 32768);
    if (!length || length >= 32768) return "";
    std::wstring path(module, length);
    size_t slash = path.find_last_of(L"/\\");
    if (slash == std::wstring::npos) return "";
    std::ifstream input(utf8(path.substr(0, slash) + L"/sms-build.json").c_str(), std::ios::binary);
    std::ostringstream json; json << input.rdbuf();
    return field(json.str(), "release_tag");
#else
    return "";
#endif
}
void check_updates(const std::string& installed_tag) {
    {std::lock_guard<std::mutex> lock(services_mutex);if(update.state==UpdateState::Checking||update.state==UpdateState::Downloading)return;
        update=UpdateStatus();update.state=UpdateState::Checking;}
    std::thread([installed_tag]() {
        UpdateStatus result;std::string json,error;
#ifdef _WIN32
        bool ok=fetch("https://api.github.com/repos/zeranemesis/sms-port/releases/latest","",json,4*1024*1024,error);
        if(ok)ok=release_asset(json,result);
        if(ok){result.state=(!installed_tag.empty()&&result.version==installed_tag)?UpdateState::UpToDate:UpdateState::Available;
            result.message="Release Windows 64 PAL avec SHA-256 publiée par le dépôt.";}
        else {result.state=UpdateState::Failed;result.message=error.empty()?"No Windows 64 PAL ZIP release with a published SHA-256 digest.":error;}
#else
        result.state=UpdateState::Failed;result.message="Update download requires Windows.";
#endif
        std::lock_guard<std::mutex> lock(services_mutex);update=result;
    }).detach();
}
void download_update() {
    UpdateStatus selection;
    {std::lock_guard<std::mutex> lock(services_mutex);if(update.state!=UpdateState::Available)return;selection=update;update.state=UpdateState::Downloading;}
    std::thread([selection]() mutable {
#ifdef _WIN32
        std::string dir=unique_directory("updates"),path=dir+"/sms-windows-64-pal.zip",body,error,digest;
        bool ok=mkdirs(dir)&&fetch(selection.download_url,path+".part",body,1024ULL*1024*1024,error);
        if(ok)ok=sha256_file(path+".part",digest)&&digest==selection.sha256;
        if(ok)ok=MoveFileExW(wide(path+".part").c_str(),wide(path).c_str(),MOVEFILE_WRITE_THROUGH)!=0;
        if(ok){selection.state=UpdateState::Ready;selection.staged_path=path;selection.message="SHA-256 verified. Update ZIP staged; extract/install after quitting the game.";}
        else{DeleteFileW(wide(path+".part").c_str());selection.state=UpdateState::Failed;selection.message=error.empty()?"Download or SHA-256 verification failed.":error;}
#else
        selection.state=UpdateState::Failed;selection.message="Update download requires Windows.";
#endif
        std::lock_guard<std::mutex> lock(services_mutex);update=selection;
    }).detach();
}
ServiceResult prepare_update_installation(bool restart_after_install) {
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(services_mutex);
    if(installation_prepared)return ServiceResult(false,"Installation is already prepared; quit normally.");
    if(update.state!=UpdateState::Ready || update.staged_path.empty())
        return ServiceResult(false,"Download and verify an update first.");
    std::string digest;
    if(!sha256_file(update.staged_path,digest) || digest!=update.sha256)
        return ServiceResult(false,"The staged archive has changed; download it again.");
    wchar_t module[32768];DWORD length=GetModuleFileNameW(NULL,module,32768);
    if(!length || length>=32768)return ServiceResult(false,"Cannot resolve executable path.");
    std::wstring executable(module,length);
    size_t slash=executable.find_last_of(L"/\\");
    if(slash==std::wstring::npos || _wcsicmp(executable.substr(slash+1).c_str(),L"sms.exe")!=0)
        return ServiceResult(false,"Update installation requires the sms.exe executable.");
    std::string work=unique_directory("installations");
    if(!mkdirs(work))return ServiceResult(false,"Cannot create installation staging directory.");
    // All variable data is in a JSON file; it is never interpolated into a
    // shell command or the helper's source. PowerShell uses literal paths.
    auto json_quote=[](const std::string& text) {
        std::string result="\"";
        for(size_t i=0;i<text.size();++i){unsigned char c=text[i];
            if(c=='"'||c=='\\'){result+='\\';result+=(char)c;}
            else if(c<32){const char* hex="0123456789abcdef";result+="\\u00";result+=hex[c>>4];result+=hex[c&15];}
            else result+=(char)c;}
        return result+"\"";
    };
    std::ostringstream options;
    options << "{\"archive\":" << json_quote(update.staged_path)
            << ",\"release\":" << json_quote(update.version)
            << ",\"sha256\":" << json_quote(update.sha256)
            << ",\"destination\":" << json_quote(utf8(executable.substr(0,slash)))
            << ",\"exe\":" << json_quote(utf8(executable))
            << ",\"pid\":" << GetCurrentProcessId()
            << ",\"restart\":" << (restart_after_install?"true":"false") << "}";
    std::string script=R"SMSPS(param([Parameter(Mandatory=$true)][string]$OptionsPath)
$ErrorActionPreference = 'Stop'
$taskDirectory = [System.IO.Path]::GetFullPath([System.IO.Path]::GetDirectoryName($OptionsPath))
$logPath = Join-Path $taskDirectory 'installation.log'
$installed = New-Object System.Collections.Generic.List[object]
$complete = $false
function Log([string]$Text) { Add-Content -LiteralPath $logPath -Value ((Get-Date -Format o) + ' ' + $Text) }
try {
    $options = Get-Content -LiteralPath $OptionsPath -Raw | ConvertFrom-Json
    $archive = [System.IO.Path]::GetFullPath([string]$options.archive)
    $destination = [System.IO.Path]::GetFullPath([string]$options.destination)
    $exe = [System.IO.Path]::GetFullPath([string]$options.exe)
    if ([System.IO.Path]::GetDirectoryName($exe) -ne $destination -or [System.IO.Path]::GetFileName($exe) -ine 'sms.exe') { throw 'Invalid executable destination.' }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $options.sha256) { throw 'Archive SHA256 mismatch.' }
    $process = Get-Process -Id ([int]$options.pid) -ErrorAction SilentlyContinue
    if ($process) {
        Log 'Waiting for the game to exit normally.'
        # A helper never kills the game. A timeout leaves installation untouched.
        if (-not $process.WaitForExit(600000)) { throw 'Game still running after ten minutes; installation cancelled.' }
    }
    $extract = Join-Path $taskDirectory 'extracted'
    $rollback = Join-Path $taskDirectory 'rollback'
    New-Item -ItemType Directory -Path $extract | Out-Null
    New-Item -ItemType Directory -Path $rollback | Out-Null
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    # Lock the exact bytes being verified against writes for the full extraction.
    $archiveStream = [System.IO.File]::Open($archive, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
    $hasher = [System.Security.Cryptography.SHA256]::Create()
    try {
        $archiveDigest = [System.BitConverter]::ToString($hasher.ComputeHash($archiveStream)).Replace('-', '').ToLowerInvariant()
        if ($archiveDigest -ne $options.sha256) { throw 'Archive changed while waiting for shutdown.' }
        $archiveStream.Position = 0
        $zip = New-Object System.IO.Compression.ZipArchive -ArgumentList @($archiveStream, [System.IO.Compression.ZipArchiveMode]::Read, $false)
    } catch { $archiveStream.Dispose(); throw }
    finally { $hasher.Dispose() }
    try {
        $entries = @($zip.Entries)
        if ($entries.Count -gt 4096) { throw 'Archive has too many entries.' }
        [long]$total = 0
        foreach ($entry in $entries) {
            $relative = $entry.FullName.Replace('/', '\')
            if ([System.IO.Path]::IsPathRooted($relative) -or $relative.Contains(':')) { throw 'Unsafe ZIP entry.' }
            $entryPath = [System.IO.Path]::GetFullPath((Join-Path $extract $relative))
            if (-not $entryPath.StartsWith($extract + '\', [System.StringComparison]::OrdinalIgnoreCase)) { throw 'ZIP traversal rejected.' }
            $unixType = (($entry.ExternalAttributes -shr 16) -band 0xF000)
            if ($unixType -eq 0xA000) { throw 'ZIP symlink rejected.' }
            $total += $entry.Length
            if ($total -gt 2147483648) { throw 'Expanded archive exceeds limit.' }
            if ($entry.FullName.EndsWith('/') -or $entry.FullName.EndsWith('\')) { continue }
            [System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($entryPath)) | Out-Null
            [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $entryPath, $false)
        }
    } finally { $zip.Dispose(); $archiveStream.Dispose() }
    $binaries = @(Get-ChildItem -LiteralPath $extract -Filter 'sms.exe' -Recurse -File)
    if ($binaries.Count -ne 1) { throw 'Release must contain exactly one sms.exe.' }
    $releaseRoot = $binaries[0].DirectoryName
    $manifestPath = Join-Path $releaseRoot 'sms-build.json'
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ($manifest.region -cne 'GMSP01' -or $manifest.architecture -cne 'x64') { throw 'Release is not Windows 64 PAL GMSP01.' }
    $manifest | Add-Member -MemberType NoteProperty -Name release_tag -Value ([string]$options.release) -Force
    [System.IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json), (New-Object System.Text.UTF8Encoding($false)))
    $stream = [System.IO.File]::OpenRead($binaries[0].FullName)
    $reader = New-Object System.IO.BinaryReader($stream)
    try {
        if ($reader.ReadUInt16() -ne 0x5A4D) { throw 'Missing executable MZ header.' }
        $stream.Position = 0x3C; $pe = $reader.ReadInt32()
        if ($pe -lt 64 -or $pe -gt ($stream.Length - 26)) { throw 'Invalid PE header offset.' }
        $stream.Position = $pe
        if ($reader.ReadUInt32() -ne 0x00004550 -or $reader.ReadUInt16() -ne 0x8664) { throw 'Executable is not Windows x64.' }
        $stream.Position = $pe + 24
        if ($reader.ReadUInt16() -ne 0x20B) { throw 'Executable is not PE32+.' }
    } finally { $reader.Dispose(); $stream.Dispose() }
    # Only shipped binaries are installed. Saves, settings, mods, ROMs and all
    # directories stay untouched, including files supplied by the user.
    $files = @(Get-ChildItem -LiteralPath $releaseRoot -File | Where-Object { $_.Name -ieq 'sms.exe' -or $_.Name -ieq 'sms-build.json' -or $_.Extension -ieq '.dll' })
    foreach ($file in $files) {
        $target = Join-Path $destination $file.Name
        if (Test-Path -LiteralPath $target) {
            $item = Get-Item -LiteralPath $target
            if ($item.PSIsContainer -or ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint)) { throw 'Installation target is not an ordinary file.' }
            Copy-Item -LiteralPath $target -Destination (Join-Path $rollback $file.Name)
        }
    }
    foreach ($file in $files) {
        $target = Join-Path $destination $file.Name
        $temporary = Join-Path $destination ($file.Name + '.sms-update-' + [System.Guid]::NewGuid().ToString('N'))
        $backup = Join-Path $rollback $file.Name
        Copy-Item -LiteralPath $file.FullName -Destination $temporary
        $installed.Add([pscustomobject]@{ Target=$target; Backup=$backup; Temporary=$temporary; Existed=(Test-Path -LiteralPath $backup) })
        if (Test-Path -LiteralPath $target) { [System.IO.File]::Replace($temporary, $target, $null) }
        else { [System.IO.File]::Move($temporary, $target) }
    }
    $complete = $true
    Log 'Installation succeeded; rollback files retained.'
} catch {
    Log ('Installation failed: ' + $_.Exception.Message)
    for ($i = $installed.Count - 1; $i -ge 0; --$i) {
        $item = $installed[$i]
        try {
            if ($item.Existed) { Copy-Item -LiteralPath $item.Backup -Destination $item.Target -Force }
            elseif (Test-Path -LiteralPath $item.Target) { Remove-Item -LiteralPath $item.Target }
            if (Test-Path -LiteralPath $item.Temporary) { Remove-Item -LiteralPath $item.Temporary }
        } catch { Log ('Rollback requires manual recovery from ' + $item.Backup + ': ' + $_.Exception.Message) }
    }
}
if ($complete -and $options.restart) {
    try { Start-Process -FilePath $exe -WorkingDirectory $destination -WindowStyle Hidden }
    catch { Log ('Update installed but restart failed: ' + $_.Exception.Message) }
}
)SMSPS";
    if(!atomic_text(work+"/options.json",options.str()) || !atomic_text(work+"/install.ps1",script))
        return ServiceResult(false,"Cannot write installation helper.",work);
    wchar_t system[32768];UINT system_length=GetSystemDirectoryW(system,32768);
    if(!system_length || system_length>=32768)return ServiceResult(false,"Cannot resolve Windows system directory.",work);
    std::wstring powershell=std::wstring(system,system_length)+L"\\WindowsPowerShell\\v1.0\\powershell.exe";
    // Windows paths cannot contain a quote; the literal filenames are passed as
    // argv, not embedded in a PowerShell -Command expression.
    std::wstring command=L"\""+powershell+L"\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \""+
        wide(work+"/install.ps1")+L"\" -OptionsPath \""+wide(work+"/options.json")+L"\"";
    std::vector<wchar_t> buffer(command.begin(),command.end());buffer.push_back(0);
    STARTUPINFOW startup={};startup.cb=sizeof startup;startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION process={};
    if(!CreateProcessW(powershell.c_str(),buffer.data(),NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startup,&process))
        return ServiceResult(false,"Could not start deferred installation helper.",work);
    CloseHandle(process.hThread);CloseHandle(process.hProcess);
    installation_prepared=true;
    update.message="Installation prepared. Quit normally; the helper will validate, install and retain rollback copies.";
    return ServiceResult(true,update.message,work);
#else
    (void)restart_after_install;
    return ServiceResult(false,"Deferred installation requires Windows.");
#endif
}
} // namespace sms_frontend

extern "C" void sms_frontend_observe_game_progress(int shines, int blueCoins)
{
    sms_frontend::observe_game_progress(shines, blueCoins);
}

extern "C" void sms_frontend_activate_save_restore()
{
    sms_frontend::activate_pending_save_restore();
}
