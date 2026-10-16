"""Extract the production widescreen mapper; no compiler/game is started."""
from pathlib import Path
import hashlib
import json
import sys

ROOT = Path(__file__).resolve().parents[3]
source_path = ROOT / 'platform/gx/src/gx_aurora.cpp'
source = source_path.read_text(encoding='utf-8')

def block(start):
    p = source.index(start)
    first = source.index('{', p)
    end, level = first + 1, 1
    while level:
        level += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[p:end]

prefix = r'''
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vector>
enum {XFR_MATIDX_A=0x18,XFR_VIEWPORT=0x1a,XFR_PROJ=0x20,
      BP_GENMODE=0,BP_TREF=0x28,BP_ZMODE=0x40,BP_CMODE0=0x41,BP_TX_IMAGE0=0x88,
      XF_MEM_WORDS=0x1000,VF_MTX=1u<<12};
struct State { uint32_t bp[256]{},xfReg[0x1000]{},xfMem[XF_MEM_WORDS]{}; const uint8_t* texImage[8]{}; } g;
float s_widescreen=1.f;
bool s_hud=false,s_stretch2D=false;
std::vector<std::pair<float,float>> s_hudPanes;
std::unordered_set<const void*> s_efbCopyDestinations;
std::vector<uint8_t> s_prim;
uint32_t s_primFmt=0,s_primStride=12;
struct Layout {uint32_t mtx=0;} layout;
const Layout& vtxFmtLayout(uint32_t){return layout;}
int bpTexReg(int base,int map){return base+(map&3)+((map&4)?0x20:0);}
'''
tests = r'''
void xf(uint32_t reg,float value){std::memcpy(&g.xfReg[reg],&value,4);}
void matrix(uint32_t word,float value){std::memcpy(&g.xfMem[word],&value,4);}
bool eq(float a,float b){return std::fabs(a-b)<0.0001f;}
void expect(bool value,const char* label){if(!value)throw std::runtime_error(label);std::printf("PASS %s\n",label);}
void setup(){
    g={};s_widescreen=4.f/3.f;s_hud=false;s_stretch2D=false;s_hudPanes.clear();s_efbCopyDestinations.clear();
    s_primFmt=0;s_primStride=12;
    xf(XFR_VIEWPORT,320.f);xf(XFR_VIEWPORT+3,662.f);
    g.xfReg[XFR_PROJ+6]=1;
    xf(XFR_PROJ,2.f/640.f);xf(XFR_PROJ+1,-1.f);xf(XFR_PROJ+2,-2.f/448.f);
    matrix(0,1.f);matrix(5,1.f);matrix(10,1.f);
    const float vertices[]={100,100,0,200,100,0};s_prim.resize(sizeof vertices);std::memcpy(s_prim.data(),vertices,sizeof vertices);
    g.bp[BP_ZMODE]=1u<<4;g.bp[BP_CMODE0]=0;
}
int main(){try{
    setup();auto m=drawMapping(2);expect(eq(m.scale,1)&&eq(m.offset,0),"Bath depth-only orthographic keeps logical copy coordinates");
    setup();g.bp[BP_CMODE0]=1u<<3;m=drawMapping(2);expect(eq(m.scale,.75f)&&eq(m.offset,80),"J2D colour orthographic still centred at 16:9");
    setup();g.bp[BP_CMODE0]=1u<<4;m=drawMapping(2);expect(eq(m.scale,.75f)&&eq(m.offset,80),"Alpha-only orthographic keeps original mapping");
    setup();g.bp[BP_ZMODE]=0;m=drawMapping(2);expect(eq(m.scale,.75f)&&eq(m.offset,80),"No depth writes does not select depth-only exception");
    setup();g.xfReg[XFR_PROJ+6]=0;xf(XFR_PROJ,1);xf(XFR_PROJ+2,1);m=drawMapping(2);expect(eq(m.scale,.75f)&&eq(m.offset,80),"Perspective world depth-only keeps original mapping");
    setup();s_widescreen=1;m=drawMapping(2);expect(eq(m.scale,1)&&eq(m.offset,0),"4:3 remains identity");
    setup();g.bp[BP_CMODE0]=1u<<3;s_hud=true;s_hudPanes.emplace_back(500,600);m=drawMapping(2);expect(eq(m.scale,.75f)&&eq(m.offset,160),"HUD right pane still anchors at edge");
    setup();g.bp[BP_CMODE0]=1u<<3;xf(XFR_VIEWPORT,256);xf(XFR_VIEWPORT+3,598);m=drawMapping(2);expect(eq(m.scale,1)&&eq(m.offset,0),"512-wide pollution viewport retains original identity");
    setup();s_widescreen=1.75f;m=drawMapping(2);expect(eq(m.scale,1)&&eq(m.offset,0),"Depth-only pass identity also holds at 21:9");
    std::puts("9 mapping cases passed");return 0;
}catch(const std::exception& error){std::fprintf(stderr,"FAIL %s\n",error.what());return 1;}}
'''
out = Path(sys.argv[1]) if len(sys.argv)>1 else ROOT/'artifacts/corona-depth-fix-20261016/mapping-test.cpp'
out.parent.mkdir(parents=True,exist_ok=True)
parts = [block('struct DrawMapping'), block('float xfFloat('), block('DrawMapping drawMapping(')]
text = prefix + parts[0]+';\n'+parts[1]+'\n'+parts[2]+'\n'+tests
out.write_text(text,encoding='utf-8')
out.with_suffix('.inputs.json').write_text(json.dumps({'source':str(source_path),'sha256':hashlib.sha256(source_path.read_bytes()).hexdigest(),'extracted_sha256':hashlib.sha256('\n'.join(parts).encode()).hexdigest(),'cases':9,'compiler_started':False,'game_started':False,'mocks':'GX register state, geometry storage and vertex-layout accessor; production drawMapping and xfFloat are extracted without rewriting'},indent=2),encoding='utf-8')
print(out)
