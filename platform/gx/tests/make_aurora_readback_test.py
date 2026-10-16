"""Generate a synthetic C++ test using the current production readback code.

No compiler, renderer, game, ROM or save is started by this script.
The coordinator compiles/runs the generated source separately.
GPU delivery and GX encoding are injected boundaries; request selection,
hash checks, queue retirement, conversion and dump retirement are extracted
verbatim from gx_aurora.cpp, never reimplemented in the test.
"""
from pathlib import Path
import argparse
import hashlib


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


parser = argparse.ArgumentParser()
parser.add_argument("output", type=Path)
args = parser.parse_args()
source_path = Path(__file__).resolve().parents[1] / "src/gx_aurora.cpp"
raw = source_path.read_bytes()
source = raw.decode("utf-8")
prefix = r'''
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <stdexcept>
uint64_t hashBytes(const void* p, uint32_t n) {
    uint64_t h=1469598103934665603ull;
    const auto* b=static_cast<const uint8_t*>(p);
    for(uint32_t i=0;i<n;++i) h=(h^b[i])*1099511628211ull;
    return h;
}
static unsigned encodes=0, invalidations=0, destroys=0;
static bool shortEncode=false;
uint32_t texLevelBytes(uint32_t, uint32_t w,uint32_t h) { return w*h; }
uint32_t encodeTexture(const uint8_t* p,uint32_t,uint32_t w,uint32_t h,uint8_t* out) {
    ++encodes;
    std::memset(out,p[0],w*h);
    return w*h-(shortEncode?1:0);
}
void GXPC_InvalidateRange(const void*,uint32_t) { ++invalidations; }
void sab_destroy_copy_tex(void*) { ++destroys; }
struct Result {
    const void* dest; uint32_t w,h; uint64_t frame,id; int success;
    std::vector<uint32_t> pixels;
};
static std::deque<Result> results;
static Result delivered;
int sab_poll_copy_readback_request(const void** d,uint32_t* w,uint32_t* h,uint64_t* frame,
                                   const uint32_t** rgba,uint64_t* id,int* success) {
    if(results.empty()) return 0;
    delivered=std::move(results.front()); results.pop_front();
    *d=delivered.dest; *w=delivered.w; *h=delivered.h; *frame=delivered.frame;
    *id=delivered.id; *success=delivered.success;
    *rgba=delivered.pixels.empty()?nullptr:delivered.pixels.data();
    return 1;
}
'''
metadata = source[source.index("struct PendingCopy {"):source.index("bool copyWriteBackEnabled()")]
conversion = source[source.index("inline uint8_t quant("):source.index("// Frame dumps,")]
dump_metadata = source[source.index("uint8_t s_dumpMarkers[32];"):source.index("const char* dumpDir()")]
body = function(source, "bool writeFrameDump(") + function(source, "void storeCopyReadbacks()")
tests = r'''
static unsigned passed=0;
void expect(bool ok,const char* message) {
    if(!ok) throw std::runtime_error(message);
}
void done(const char* name) { ++passed; std::printf("PASS %s\n",name); }
void reset() {
    s_pendingCopies.clear(); s_appliedCopyRequests.clear(); s_dumpPending.clear();
    results.clear(); encodes=invalidations=destroys=s_writeBackCount=0; shortEncode=false;
}
void add(uint64_t id,void* dest,uint32_t w=2,uint32_t h=2,uint32_t fmt=1,uint64_t frame=7) {
    PendingCopy p{};
    p.requestId=id;p.dest=dest;p.frameId=frame;p.width=w;p.height=h;
    p.bytes=w*h;p.fmt=fmt;p.layout=1;p.hash=hashBytes(dest,p.bytes);
    s_pendingCopies.emplace(id,p);
}
void result(uint64_t id,const void* dest,uint32_t pixel=0xaa443322u,int success=1,
            uint32_t w=2,uint32_t h=2,bool nullPixels=false,uint64_t frame=7) {
    results.push_back(Result{dest,w,h,frame,id,success,
        success&&!nullPixels?std::vector<uint32_t>(w*h,pixel):std::vector<uint32_t>{}});
}
bool all(const uint8_t* b,uint8_t value) { return std::all_of(b,b+4,[=](uint8_t v){return v==value;}); }
int main() {
    try {
        uint8_t a[4]{}, b[4]{};
        reset(); add(1,a); result(1,a); storeCopyReadbacks();
        expect(all(a,0x22)&&s_pendingCopies.empty()&&encodes==1&&invalidations==1,"unchanged destination");
        done("unchanged texture receives result");

        reset(); std::memset(a,0,4); add(2,a); std::memset(a,0x77,4); result(2,a); storeCopyReadbacks();
        expect(all(a,0x77)&&s_pendingCopies.empty()&&!encodes&&!invalidations,"CPU edit overwritten");
        done("CPU edit wins before conversion/encoding");

        reset(); std::memset(a,0,4); add(3,a); GXPC_InvalidateRange(a,4); result(3,a); storeCopyReadbacks();
        expect(all(a,0x22)&&encodes==1&&invalidations==2,"flush-only cancelled valid copy");
        done("flush without changing bytes preserves pending copy");

        reset(); std::memset(a,0,4); add(4,a); add(5,a,2,2,9);
        result(4,a,0,0,2,2,true,0); storeCopyReadbacks();
        expect(s_pendingCopies.count(5)==1&&s_pendingCopies.count(4)==0&&!encodes,"drop retired another id");
        result(5,a); storeCopyReadbacks();
        expect(all(a,0x33)&&s_pendingCopies.empty()&&encodes==1,"copy after drop used wrong format");
        done("drop tombstone does not shift later copy format");

        reset(); std::memset(a,0,4); add(6,a); add(7,a,2,2,9);
        result(6,a); result(7,a); storeCopyReadbacks();
        expect(all(a,0x33)&&encodes==2&&s_pendingCopies.empty(),"same-frame distinct copies failed");
        done("same-frame same-destination copies retain unique formats and chained hashes");

        reset(); std::memset(a,0,4); add(8,a); add(9,a,2,2,9);
        result(9,a,0xaa440022u); result(8,a); storeCopyReadbacks();
        expect(all(a,0)&&encodes==1&&s_appliedCopyRequests[a]==9,"older result restored after newer unchanged bytes");
        done("late older result rejected even when newest pixels equal original bytes");

        reset(); std::memset(a,0,4); add(10,a); result(999,a); storeCopyReadbacks();
        expect(s_pendingCopies.count(10)==1&&!encodes,"unknown id consumed metadata");
        done("unknown result preserves valid pending metadata");

        reset(); std::memset(a,0,4); std::memset(b,0,4); add(11,a); add(12,b);
        result(11,b); storeCopyReadbacks();
        expect(all(a,0)&&all(b,0)&&s_pendingCopies.count(11)==0&&s_pendingCopies.count(12)==1&&!encodes,
               "wrong destination wrote bytes or retired another id");
        done("destination mismatch retires only its identified request");

        reset(); add(13,a); result(13,a,0xaa443322u,1,1,4); storeCopyReadbacks();
        expect(s_pendingCopies.empty()&&!encodes&&!invalidations,"dimension mismatch accepted");
        done("different dimensions with equal byte count rejected");

        reset(); add(14,a); result(14,a,0,1,2,2,true); storeCopyReadbacks();
        expect(s_pendingCopies.empty()&&!encodes,"success with null pixel pointer accepted");
        done("null pixels rejected safely");

        reset(); std::memset(a,0,4); add(15,a); shortEncode=true; result(15,a); storeCopyReadbacks();
        expect(all(a,0)&&s_pendingCopies.empty()&&!invalidations&&!s_writeBackCount,"partial encoding wrote texture");
        done("incomplete encoded result is never written");

        reset(); std::memset(a,0,4); std::memset(b,0,4); add(16,a); add(17,b);
        const auto bh=s_pendingCopies.at(17).hash;
        result(16,a); storeCopyReadbacks();
        expect(s_pendingCopies.at(17).hash==bh&&all(b,0),"cross-destination hash update");
        done("hash propagation stays within same destination and byte count");

        reset(); std::memset(a,0,4); add(18,a); add(19,a); result(18,a); storeCopyReadbacks();
        std::memset(a,0x66,4); result(19,a); storeCopyReadbacks();
        expect(all(a,0x66)&&encodes==1&&s_pendingCopies.empty(),"CPU edit after own earlier write lost");
        done("CPU edit after chained copy still wins");

        reset(); const void* marker=&s_dumpMarkers[0];
        s_dumpPending[marker]=PendingDump{123,"__nonexistent_readback_test_directory__",21};
        result(20,marker,0,0); storeCopyReadbacks();
        expect(s_dumpPending.count(marker)==1&&!destroys,"stale marker retired replacement");
        done("stale dump marker result preserves replacement");

        result(21,marker,0,0); storeCopyReadbacks();
        expect(s_dumpPending.empty()&&destroys==1,"failed dump did not retire marker");
        done("failed dump retires its exact marker without pixel access");

        std::printf("%u tests passed\n",passed);
        return 0;
    } catch(const std::exception& e) {
        std::fprintf(stderr,"FAIL: %s\n",e.what()); return 1;
    }
}
'''
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text("// gx_aurora.cpp sha256: " + hashlib.sha256(raw).hexdigest() + "\n" +
                       prefix + metadata + conversion + dump_metadata + body + tests,
                       encoding="utf-8")
print(args.output.resolve())
