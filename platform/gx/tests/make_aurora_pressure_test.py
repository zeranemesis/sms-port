"""Extract the real bounded copy-pool wait and submission policy."""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
source = (root/'third_party/aurora/lib/gfx/sms_readback.cpp').read_text(encoding='utf-8')

def block(signature):
    start = source.index(signature)
    opening = source.index('{', start)
    end, level = opening+1, 1
    while level:
        level += (source[end]=='{')-(source[end]=='}')
        end += 1
    return source[start:end]

prefix = r'''
#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <thread>
constexpr size_t CopySlots=16;
enum class SlotState {Available,Recording,PendingSubmit,CopySubmitted,MapPending};
struct CopySlot {SlotState state=SlotState::Available;};
std::array<CopySlot,CopySlots> g_copySlots;
std::mutex g_mutex;
std::condition_variable g_copyAvailable;
bool g_active=true;
'''
tests = r'''
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);std::printf("PASS %s\n",message);}
void full(){std::lock_guard lock{g_mutex};g_active=true;for(auto& s:g_copySlots)s.state=SlotState::MapPending;}
int main(){try{
  require(copy_capacity()==16,"bounded pool capacity stays 16");
  g_copySlots[0].state=SlotState::PendingSubmit;
  g_copySlots[1].state=SlotState::Recording;
  submit_pending_copies();
  require(g_copySlots[0].state==SlotState::CopySubmitted && g_copySlots[1].state==SlotState::Recording,
          "checkpoint promotes encoded copies and leaves unfinished copies alone");
  full();
  std::thread callback([]{std::this_thread::sleep_for(std::chrono::milliseconds(20));
    {std::lock_guard lock{g_mutex};g_copySlots[15].state=SlotState::Available;}g_copyAvailable.notify_all();});
  bool ready=wait_for_copy_capacity(1000);callback.join();
  require(ready && g_copySlots[0].state==SlotState::MapPending,"delayed last-slot callback releases pressure without retiring earlier copies");
  full();auto start=std::chrono::steady_clock::now();
  require(!wait_for_copy_capacity(20) && std::chrono::steady_clock::now()-start<std::chrono::seconds(1),
          "missing GPU completion times out with bounded wait");
  full();std::thread shutdown([]{std::this_thread::sleep_for(std::chrono::milliseconds(20));
    {std::lock_guard lock{g_mutex};g_active=false;}g_copyAvailable.notify_all();});
  ready=wait_for_copy_capacity(1000);shutdown.join();require(!ready,"shutdown wakes waiter and rejects further submission");
  full();std::thread spurious([]{std::this_thread::sleep_for(std::chrono::milliseconds(5));g_copyAvailable.notify_all();});
  ready=wait_for_copy_capacity(20);spurious.join();require(!ready,"notification without free slot cannot admit a copy");
  std::puts("6 copy-pressure tests passed");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
'''
out = Path(sys.argv[1])
out.write_text(prefix+'\n'.join(block(s) for s in (
    'uint32_t copy_capacity()', 'void submit_pending_copies()', 'bool wait_for_copy_capacity('))+tests,
    encoding='utf-8')
print(out)
