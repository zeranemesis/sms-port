#include "gamebanana.h"
#include <mutex>
#include <thread>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
namespace sms_frontend { namespace {
std::mutex mutex;
BananaStatus status;
bool busy=false;
std::string decode(const std::string& hex) {
    std::string out;
    for(size_t i=0;i+1<hex.size();i+=2) out.push_back((char)std::stoi(hex.substr(i,2),nullptr,16));
    return out;
}
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> r; std::istringstream in(line); std::string v;
    while(std::getline(in,v,'\t')) r.push_back(v);
    if(!line.empty()&&line.back()=='\t')r.emplace_back();
    return r;
}
#ifdef _WIN32
std::wstring wide(const std::string& s) {
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0);
    std::wstring r(n,L'\0'); if(n) MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),r.data(),n); return r;
}
std::string quote(const std::filesystem::path& p) {
    std::string s=p.u8string(),r="'"; for(char c:s) {r+=c;if(c=='\'')r+='\'';} return r+"'";
}
// Values supplied by the UI are numeric IDs. Paths are quoted as PowerShell
// literals. The script never invokes archive contents or a command shell.
const char* script=R"PS(
$ErrorActionPreference='Stop'
[Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
function H($s){[BitConverter]::ToString([Text.Encoding]::UTF8.GetBytes([string]$s)).Replace('-','')}
function Emit($line){[IO.File]::AppendAllText($result,$line+"`n",[Text.Encoding]::UTF8)}
function Api($url){Invoke-RestMethod -Uri $url -TimeoutSec 35 -UserAgent 'SMS-PAL-Frontend/1'}
function SafeParents($path){
 $p=[IO.DirectoryInfo]::new([IO.Path]::GetFullPath($path))
 while($p){if($p.Exists -and ($p.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Dossier de mods redirigé : installation refusée.'};$p=$p.Parent}
}
try {
 if($action -eq 'catalogue') {
  $data=Api ('https://gamebanana.com/apiv11/Mod/Index?_aFilters[Generic_Game]=5798&_nPerpage=20&_nPage='+$page)
  Emit ("COUNT`t"+$data._aMetadata._nRecordCount)
  foreach($m in $data._aRecords){if($m._aGame._idRow -eq 5798){Emit ("MOD`t"+$m._idRow+"`t"+(H $m._sName)+"`t"+(H $m._aSubmitter._sName)+"`t"+(H $m._aRootCategory._sName))}}
 } else {
  $m=Api ('https://gamebanana.com/apiv11/Mod/'+$mod+'?_csvProperties=_idRow,_sName,_aGame,_aFiles')
  if($m._aGame._idRow -ne 5798){throw 'Ce mod ne concerne pas Super Mario Sunshine.'}
  if($action -eq 'files') {
   foreach($f in $m._aFiles){Emit ("FILE`t"+$f._idRow+"`t"+(H $f._sFile)+"`t"+$f._nFilesize)}
  } else {
   $f=@($m._aFiles|Where-Object {$_._idRow -eq $file})
   if($f.Count -ne 1){throw 'Fichier absent du catalogue.'}; $f=$f[0]
   if([IO.Path]::GetExtension($f._sFile).ToLowerInvariant() -ne '.zip'){Emit ("UNSUPPORTED`t"+(H 'Archive non ZIP : installation native non prise en charge.'));exit}
   if($f._nFilesize -gt 536870912){throw 'Archive trop grande (limite 512 Mio).'}
   $url=[Uri]$f._sDownloadUrl
   if($url.Scheme -ne 'https' -or $url.Host -ne 'gamebanana.com' -or $url.AbsolutePath -ne ('/dl/'+$file)){throw 'Adresse de téléchargement non valide.'}
   $archive=Join-Path $work 'download.zip'
   Add-Type -AssemblyName System.Net.Http
   $client=New-Object Net.Http.HttpClient
   try {
    $client.Timeout=[TimeSpan]::FromMinutes(15)
    $response=$client.GetAsync($url,[Net.Http.HttpCompletionOption]::ResponseHeadersRead).Result
    $response.EnsureSuccessStatusCode()|Out-Null
    $length=$response.Content.Headers.ContentLength
    if($length -gt 536870912){throw 'Téléchargement trop grand.'}
    $input=$response.Content.ReadAsStreamAsync().Result; $output=[IO.File]::Create($archive)
    try {$buffer=New-Object byte[] 65536;[long]$count=0;while(($n=$input.Read($buffer,0,$buffer.Length)) -gt 0){$count+=$n;if($count -gt 536870912){throw 'Téléchargement trop grand.'};$output.Write($buffer,0,$n)}} finally {$output.Dispose();$input.Dispose()}
   } finally {$client.Dispose()}
   if($f._sMd5Checksum -and (Get-FileHash -LiteralPath $archive -Algorithm MD5).Hash -ne $f._sMd5Checksum){throw 'Somme de contrôle GameBanana incorrecte.'}
   Add-Type -AssemblyName System.IO.Compression.FileSystem
   $zip=[IO.Compression.ZipFile]::OpenRead($archive)
   try {
    if($zip.Entries.Count -gt 20000){throw 'Archive contenant trop de fichiers.'}
    $files=New-Object Collections.Generic.List[object];[long]$total=0
    foreach($e in $zip.Entries){
     $name=$e.FullName.Replace('\','/');$parts=$name.Split('/')
     if($name.StartsWith('/') -or $name.Contains(':') -or @($parts|Where-Object {$_ -eq '..' -or $_ -eq '.' -or $_ -match '[<>|?*]' -or $_.EndsWith('.') -or $_.EndsWith(' ') -or $_ -match '^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\.|$)'}).Count){throw 'Chemin dangereux dans cette archive.'}
     if((($e.ExternalAttributes -shr 16) -band 0xF000) -eq 0xA000){throw 'Lien symbolique interdit.'}
     if($name.EndsWith('/')){continue};$total+=$e.Length;if($total -gt 2147483648 -or $e.Length -gt 536870912){throw 'Archive décompressée trop grande.'}
     $ext=[IO.Path]::GetExtension($name).ToLowerInvariant()
     if($ext -in @('.exe','.dll','.bat','.cmd','.ps1','.com','.dol','.elf','.rel','.iso','.gcm','.rvz','.xdelta','.ips','.bps','.ppf','.gct')){Emit ("UNSUPPORTED`t"+(H 'Ce mod contient du code ou un patch console incompatible avec le port natif.'));exit}
     $files.Add(@{entry=$e;name=$name})
    }
    $payload=@($files|Where-Object {[IO.Path]::GetExtension($_.name).ToLowerInvariant() -notin @('.txt','.md','.pdf','.jpg','.jpeg')})
    if(!$payload.Count){Emit ("UNSUPPORTED`t"+(H 'Aucun fichier de jeu ou pack de textures installable.'));exit}
    $textures=@($payload|Where-Object {[IO.Path]::GetFileName($_.name) -match '^tex1_.+\.(png|dds)$'})
    $key='gamebanana-'+$mod+'-'+$file
    if($textures.Count -eq $payload.Count){$dest=Join-Path $root ('mods/textures/'+$key);$mode='textures'}
    else {$dest=Join-Path $root ('mods/'+$key);$mode='files'}
    SafeParents $dest
    if([IO.Directory]::Exists($dest) -or [IO.File]::Exists($dest)){throw 'Installation déjà présente : aucun fichier remplacé.'}
    $stage=Join-Path $work 'staged';[IO.Directory]::CreateDirectory($stage)|Out-Null
    $seen=New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    foreach($item in $payload){
     if($mode -eq 'textures'){$relative=$item.name}
     else {
      $match=[regex]::Match($item.name,'(?:^|/)files/(.+)$');if($match.Success){$relative=$match.Groups[1].Value}else{$relative=$item.name}
      if($relative -notmatch '^(data|scene|sound|movie|card)/' -or [IO.Path]::GetExtension($relative).ToLowerInvariant() -notin @('.szs','.arc','.bin','.bti','.bmd','.bdl','.bck','.btk','.brk','.bpk','.bva','.bas','.thp','.ast','.aw','.bnk')){Emit ("UNSUPPORTED`t"+(H 'Disposition non compatible : attend files/data, files/scene, files/sound, files/movie, files/card ou tex1_*.png/dds.'));exit}
      $relative='files/'+$relative
     }
     if(!$seen.Add($relative)){throw 'Chemins dupliqués dans cette archive.'}
     $target=[IO.Path]::GetFullPath((Join-Path $stage $relative))
     if(!$target.StartsWith([IO.Path]::GetFullPath($stage)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Chemin hors installation.'}
     [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))|Out-Null
     $source=$item.entry.Open();$output=[IO.File]::Create($target);try{$source.CopyTo($output)}finally{$source.Dispose();$output.Dispose()}
    }
    [IO.File]::WriteAllText((Join-Path $stage 'gamebanana-source.txt'),('https://gamebanana.com/mods/'+$mod+"`nfile="+$file))
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($dest))|Out-Null
    [IO.Directory]::Move($stage,$dest)
    Emit ("INSTALLED`t"+(H $key)+"`t"+(H $dest)+"`t"+(H $mode))
   } finally {$zip.Dispose()}
  }
 }
} catch {Emit ("ERROR`t"+(H $_.Exception.Message))}
)PS";
#endif
void start(const std::string& action,int page,int mod,int file) {
    {std::lock_guard<std::mutex> lock(mutex);if(busy)return;busy=true;status.state=action=="install"?BananaState::Downloading:BananaState::Loading;status.message="GameBanana : opération en cours..."; if(action=="catalogue"){status.mods.clear();status.files.clear();status.selected_mod=0;status.page=page;}else if(action=="files"){status.files.clear();status.selected_mod=mod;}}
    std::thread([=]{
      try {
#ifdef _WIN32
        auto root=std::filesystem::current_path();
        // Native runtime resolves mods relative to its project working directory.
        auto work=root/"mods"/".gamebanana-cache"/(std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
        // Reject junctions before creating the temporary helper below mods.
        for(auto p=work.parent_path();!p.empty();p=p.parent_path()) {
          DWORD attrs=GetFileAttributesW(p.wstring().c_str());
          if(attrs!=INVALID_FILE_ATTRIBUTES && (attrs&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Dossier de mods redirigé : opération refusée.");
          if(p==p.parent_path())break;
        }
        std::filesystem::create_directories(work);auto result=work/"result.txt", ps=work/"request.ps1";
        std::ofstream out(ps,std::ios::binary);out << "\xEF\xBB\xBF" << "$action='"<<action<<"';$page="<<page<<";$mod="<<mod<<";$file="<<file<<";$root="<<quote(root)<<";$work="<<quote(work)<<";$result="<<quote(result)<<"\nAdd-Type -AssemblyName System.Net.Http\n" <<script;out.close();
        wchar_t system[MAX_PATH];GetSystemDirectoryW(system,MAX_PATH);std::wstring app=std::wstring(system)+L"\\WindowsPowerShell\\v1.0\\powershell.exe";
        std::wstring command=L"\""+app+L"\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \""+ps.wstring()+L"\"";
        STARTUPINFOW si={};si.cb=sizeof(si);PROCESS_INFORMATION pi={};
        if(!CreateProcessW(app.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,root.wstring().c_str(),&si,&pi))throw std::runtime_error("Impossible de lancer le service GameBanana.");
        CloseHandle(pi.hThread);WaitForSingleObject(pi.hProcess,INFINITE);DWORD exit=0;GetExitCodeProcess(pi.hProcess,&exit);CloseHandle(pi.hProcess);
        std::ifstream in(result,std::ios::binary);if(!in || exit!=0)throw std::runtime_error("Aucune réponse valide du service GameBanana.");
        BananaStatus updated;{std::lock_guard<std::mutex> lock(mutex);updated=status;}
        updated.state=BananaState::Ready;updated.message="Catalogue GameBanana chargé.";
        std::string line;while(std::getline(in,line)){if(line.size()>=3 && (unsigned char)line[0]==0xef)line.erase(0,3);if(!line.empty()&&line.back()=='\r')line.pop_back();auto v=split(line);if(v.empty())continue;
          if(v[0]=="COUNT"&&v.size()>1)updated.total=std::stoi(v[1]);
          if(v[0]=="MOD"&&v.size()>4){BananaMod m;m.id=std::stoi(v[1]);m.name=decode(v[2]);m.author=decode(v[3]);m.category=decode(v[4]);m.url="https://gamebanana.com/mods/"+v[1];updated.mods.push_back(m);}
          if(v[0]=="FILE"&&v.size()>3){BananaFile f;f.id=std::stoi(v[1]);f.name=decode(v[2]);f.bytes=std::stoull(v[3]);f.zip=std::filesystem::path(f.name).extension()==".zip";updated.files.push_back(f);}
          if((v[0]=="ERROR"||v[0]=="UNSUPPORTED")&&v.size()>1){updated.state=v[0]=="ERROR"?BananaState::Failed:BananaState::Unsupported;updated.message=decode(v[1]);}
          if(v[0]=="INSTALLED"&&v.size()>3){updated.state=BananaState::Installed;updated.installed_mod=decode(v[1]);updated.installed_path=decode(v[2]);updated.message=decode(v[3])=="textures"?"Textures installées. Activez les textures HD.":"Mod installé. Activez-le dans les mods de fichiers.";}
        }
        {std::lock_guard<std::mutex> lock(mutex);status=updated;busy=false;}
#else
        throw std::runtime_error("GameBanana : service Windows requis.");
#endif
      }catch(const std::exception& e){std::lock_guard<std::mutex> lock(mutex);status.state=BananaState::Failed;status.message=e.what();busy=false;}
    }).detach();
}
} // namespace
void gamebanana_catalogue(int page){start("catalogue",page<1?1:page,0,0);}
void gamebanana_files(int id){if(id>0)start("files",1,id,0);}
void gamebanana_install(int mod,int file){if(mod>0&&file>0)start("install",1,mod,file);}
BananaStatus gamebanana_status(){std::lock_guard<std::mutex> lock(mutex);return status;}
}
