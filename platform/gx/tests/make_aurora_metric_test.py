"""Extract metric normalization and verify native/display counts in one frame."""
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
source=(root/'platform/gx/src/gx_aurora.cpp').read_text(encoding='utf-8')
def block(signature):
    start=source.index(signature);opening=source.index('{',start);end=opening+1;level=1
    while level:
        level+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
prefix=r'''
#include <array>
#include <cstdint>
#include <cstdio>
#include <map>
#include <stdexcept>
#include <vector>
'''
data=r'''
std::map<uint64_t,MetricFrame> s_metricFrames;
std::map<uint16_t,uint32_t> s_tokenValues;
std::array<uint32_t,255> values{};
bool pending=false;
bool sab_poll_pixel_metrics(uint64_t* frame,const uint32_t** counts){
  if(!pending)return false;pending=false;*frame=7;*counts=values.data();return true;
}
'''
tests=r'''
void expect(bool value,const char* message){if(!value)throw std::runtime_error(message);std::printf("PASS %s\n",message);}
void run(float area,std::vector<MetricSlot> slots,std::initializer_list<uint32_t> counts){
  values={};size_t i=0;for(auto v:counts)values[i++]=v;
  s_metricFrames.clear();s_tokenValues.clear();s_metricFrames[7]={slots,area};pending=true;collectPixelMetrics();
}
MetricSlot slot(int token,float area=0,float scale=1,uint32_t tris=0){MetricSlot s;s.token=token;s.pixelArea=area;s.drawScale=scale;s.tris=tris;return s;}
int main(){try{
  run(4,{slot(1),slot(2,1)}, {408,102});
  expect(s_tokenValues[1]==102 && s_tokenValues[2]==102,"mixed 2x display/native mask counts preserve the same 102 GX pixels");
  expect(s_tokenValues[2]>=50,"native mask cannot prematurely trigger Noki rock cleaned threshold 50");
  run(4,{slot(3,0,.75f)}, {306});expect(s_tokenValues[3]==102,"widescreen draw scale remains applied to display metrics");
  run(0,{slot(4)}, {102});expect(s_tokenValues[4]==102,"missing display scale falls back to one");
  run(4,{slot(5,1,1,2)}, {102});expect(s_tokenValues[5]-8==102,"retail triangle correction still yields native pollution count");
  std::puts("5 metric normalization tests passed");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
'''
Path(sys.argv[1]).write_text(prefix+block('struct MetricSlot')+';\n'+block('struct MetricFrame')+';\n'+data+
                           block('void collectPixelMetrics()')+tests,encoding='utf-8')
