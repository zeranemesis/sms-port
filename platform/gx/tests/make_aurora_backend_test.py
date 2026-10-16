"""Extract backend queue/allocation code; mock WebGPU allocation, not the policy."""
from pathlib import Path
import sys

root=Path(__file__).resolve().parents[3]
source=(root/'third_party/aurora/lib/gfx/sms_readback.cpp').read_text()
header=(root/'third_party/aurora/lib/gfx/sms_readback.hpp').read_text()
bridge=(root/'aurora-bridge/bridge.cpp').read_text()
def block(text,start):
    pos=text.index(start); begin=text.index('{',pos); level=1; end=begin+1
    while level:
        level+=(text[end]=='{')-(text[end]=='}'); end+=1
    return text[pos:end]

prefix=r'''
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>
namespace wgpu {
namespace BufferUsage { constexpr int Storage=1,CopySrc=2,MapRead=4,CopyDst=8,Uniform=16; }
struct Buffer { bool valid=false; explicit operator bool() const{return valid;} };
struct BufferDescriptor {const char* label;int usage;uint64_t size;};
}
struct Device {
    std::deque<bool> outcomes;
    unsigned calls=0;
    wgpu::Buffer CreateBuffer(const wgpu::BufferDescriptor*) {
        ++calls; bool ok=true;
        if(!outcomes.empty()){ok=outcomes.front();outcomes.pop_front();}
        return {ok};
    }
} g_device;
struct Params {uint32_t words[8];};
'''
chunks=[prefix,block(header,'struct CopyRequest')+';',block(source,'struct CopyResult')+';',r'''
enum class SlotState {Available,Recording};
struct CopySlot {wgpu::Buffer storage,readback,params;uint64_t capacity=0;SlotState state=SlotState::Available;};
std::mutex g_mutex;
bool g_active=true;
std::deque<CopyResult> g_copyResults;
CopyResult g_returnedCopy;
CopyRequest g_pendingCopy;
''']
for name in ('bool ensure_copy_slot(', 'CopyRequest take_pending_copy(',
             'void aurora_sms_request_copy_readback(', 'void aurora_sms_reject_copy_readback(',
             'int aurora_sms_poll_copy_readback('):
    chunks.append(block(source,name))
chunks.append(r'''
bool s_ready=true;
namespace aurora {
namespace gx {bool is_depth_format(uint32_t fmt){return fmt==0x11;}}
namespace webgpu {struct Instance {explicit operator bool()const{return true;}void ProcessEvents(){}};Instance g_instance;}
namespace gfx {namespace render_worker {bool is_idle(){return true;}}}
}
void GXSetTexCopySrc(uint16_t,uint16_t,uint16_t,uint16_t){}
using GXTexFmt=uint32_t;
constexpr int GX_FALSE=0,GX_TRUE=1;
void GXSetTexCopyDst(uint16_t,uint16_t,GXTexFmt,int){}
void GXCopyTex(void*,int){}
''')
names=('void sab_copy_tex(', 'void sab_copy_tex_request(', 'int sab_poll_copy_readback(',
       'int sab_poll_copy_readback_request(')
for name in names:
    definition=block(bridge,name)
    chunks.append(definition[:definition.index('{')].strip()+';')
for name in names: chunks.append(block(bridge,name))
chunks.append(r'''
void expect(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
void reset(){g_copyResults.clear();g_returnedCopy={};g_pendingCopy={};g_active=true;s_ready=true;}
bool poll(uint64_t id,bool succeeded){
    const void* dest=nullptr;uint32_t w=0,h=0;uint64_t frame=0,actual=0;const uint32_t* px=nullptr;int success=-1;
    const int got=sab_poll_copy_readback_request(&dest,&w,&h,&frame,&px,&actual,&success);
    expect(got==1&&actual==id&&bool(success)==succeeded,"result identity/status");
    expect(bool(px)==succeeded,"pixel pointer validity");return true;
}
int main(){try{
    int dest=0;unsigned passed=0;
    reset();aurora_sms_request_copy_readback(&dest,8,8,11);auto first=take_pending_copy();
    expect(first.requestId==11&&first.dest==&dest&&g_pendingCopy.dest==nullptr,"take request");++passed;
    reset();aurora_sms_request_copy_readback(&dest,8,8,12);aurora_sms_request_copy_readback(&dest,8,8,13);
    poll(12,false);expect(take_pending_copy().requestId==13,"overwrite preserves newest");++passed;
    reset();aurora_sms_reject_copy_readback(&dest,8,8,14);poll(14,false);++passed;
    reset();g_copyResults.push_back(CopyResult{CopyRequest{&dest,2,2,15},123,{1,2,3,4}});
    poll(15,true);expect(g_returnedCopy.rgba[3]==4,"pixels retained until next poll");++passed;
    reset();g_copyResults.push_back(CopyResult{CopyRequest{&dest,2,2,16},123,{}});
    g_copyResults.push_back(CopyResult{CopyRequest{&dest,2,2,17},124,{7,8,9,10}});
    const void* d;uint32_t w,h;uint64_t frame;const uint32_t* px;
    expect(sab_poll_copy_readback(&d,&w,&h,&frame,&px)==1&&frame==124&&px[0]==7,"legacy poll skips failure");++passed;
    reset();sab_copy_tex(&dest,0,0,2,2,2,2,0,0,1);
    expect(take_pending_copy().requestId==0,"legacy request ABI");++passed;
    reset();sab_copy_tex_request(&dest,0,0,2,2,2,2,0,0,1,18);
    expect(take_pending_copy().requestId==18,"new request ABI");++passed;
    reset();sab_copy_tex_request(nullptr,0,0,2,2,2,2,0,0,1,19);poll(19,false);++passed;
    reset();sab_copy_tex_request(&dest,0,0,2,2,0,2,0,0,1,20);poll(20,false);++passed;
    reset();sab_copy_tex_request(&dest,0,0,2,2,2,2,0x11,0,1,21);poll(21,false);++passed;
    reset();sab_copy_tex_request(&dest,0,0,2,2,2,2,0,0,0,0);
    expect(!g_pendingCopy.dest&&g_copyResults.empty(),"no readback requested");++passed;
    reset();g_active=false;aurora_sms_reject_copy_readback(&dest,2,2,22);
    expect(g_copyResults.empty(),"shutdown cancellation");++passed;
    CopySlot slot;g_device.calls=0;g_device.outcomes={true,false,true};
    expect(!ensure_copy_slot(slot,16),"partial allocation must fail");++passed;
    const auto before=g_device.calls;expect(ensure_copy_slot(slot,16)&&g_device.calls>before,"partial allocation retried");++passed;
    const auto complete=g_device.calls;expect(ensure_copy_slot(slot,16)&&g_device.calls==complete,"complete allocation reused");++passed;
    slot.params={};expect(ensure_copy_slot(slot,16)&&g_device.calls>complete,"missing params retried");++passed;
    std::printf("%u backend tests passed\n",passed);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
''')
target=Path(sys.argv[1]);target.parent.mkdir(parents=True,exist_ok=True)
target.write_text('\n'.join(chunks)+'\n')
print(target)
