// Read-only native thread snapshot for this debuggable Android app only.
// PTRACE_SEIZE/INTERRUPT avoids a process-wide SIGSTOP; each thread is detached
// immediately after reading registers and a bounded frame-pointer chain.
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <elf.h>
#include <dirent.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
struct Registers { unsigned long long x[31], sp, pc, pstate; };
static bool peek(pid_t tid, unsigned long long address, unsigned long long& value) {
    errno=0; long data=ptrace(PTRACE_PEEKDATA,tid,reinterpret_cast<void*>(address),nullptr);
    if(errno)return false; value=static_cast<unsigned long long>(data); return true;
}
int main(int argc,char** argv) {
    if(argc!=2&&argc!=4)return 2;
    const pid_t pid=std::atoi(argv[1]); if(pid<=1||pid==getpid())return 2;
    const std::string base="/proc/"+std::to_string(pid);
    std::ifstream cmd(base+"/cmdline");std::string name;std::getline(cmd,name,'\0');
    if(name!="org.smsunified.aurora:game")return 3;
    if(argc==4) {
        const auto address=std::strtoull(argv[2],nullptr,16);
        const auto length=std::strtoul(argv[3],nullptr,10);
        if(length>4096)return 2;
        if(ptrace(PTRACE_SEIZE,pid,nullptr,nullptr)<0)return 5;
        ptrace(PTRACE_INTERRUPT,pid,nullptr,nullptr);
        int status=0;waitpid(pid,&status,__WALL);
        for(unsigned long i=0;i<length;i+=8){unsigned long long v=0;if(!peek(pid,address+i,v))break;std::printf("%llx %016llx\n",address+i,v);}
        ptrace(PTRACE_DETACH,pid,nullptr,nullptr);return 0;
    }
    DIR* dir=opendir((base+"/task").c_str());if(!dir)return 4;
    std::vector<pid_t> tids;while(auto* item=readdir(dir))if(item->d_name[0]>='0'&&item->d_name[0]<='9')tids.push_back(std::atoi(item->d_name));closedir(dir);
    std::sort(tids.begin(),tids.end());
    for(pid_t tid:tids) {
        std::ifstream comm(base+"/task/"+std::to_string(tid)+"/comm");std::string label;std::getline(comm,label);
        std::printf("thread %d %s\n",tid,label.c_str());
        if(ptrace(PTRACE_SEIZE,tid,nullptr,nullptr)<0){std::printf("seize error %s\n",strerror(errno));continue;}
        if(ptrace(PTRACE_INTERRUPT,tid,nullptr,nullptr)<0){ptrace(PTRACE_DETACH,tid,nullptr,nullptr);continue;}
        int status=0;bool stopped=false;
        for(int retry=0;retry<500;++retry){if(waitpid(tid,&status,__WALL|WNOHANG)==tid){stopped=WIFSTOPPED(status);break;}usleep(1000);}
        if(stopped){
            Registers r{};iovec io{&r,sizeof(r)};
            if(ptrace(PTRACE_GETREGSET,tid,reinterpret_cast<void*>(NT_PRSTATUS),&io)==0){
                std::printf(" pc %llx lr %llx sp %llx fp %llx\n",r.pc,r.x[30]&0xffffffffffffULL,r.sp,r.x[29]);
                auto fp=r.x[29];
                for(int depth=0;depth<24&&fp&&!(fp&7);++depth){
                    unsigned long long next=0,lr=0;if(!peek(tid,fp,next)||!peek(tid,fp+8,lr))break;
                    std::printf(" frame %d %llx\n",depth,lr&0xffffffffffffULL);
                    if(next<=fp||next-fp>16*1024*1024)break;fp=next;
                }
            }
        }else std::puts("stop timeout");
        ptrace(PTRACE_DETACH,tid,nullptr,nullptr);
        std::fflush(stdout);
    }
}
