"""Extract the real Android filter-pointer decoder for an isolated ARM64 check."""
from pathlib import Path
import hashlib
import json

ROOT = Path(__file__).resolve().parents[2]
source_path = ROOT / 'platform/audio/dsp_hle.cpp'
source = source_path.read_text(encoding='utf-8')
start = source.index('const uint32_t* setup_filter_pointer(')
end = source.index('\n}\n', start) + 3
function = source[start:end]
output = ROOT / 'artifacts/android-audio-filter/filter-pointer-test.cpp'
test = r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
FUNCTION
static const uint32_t first[] = {17, 29, 41};
static const uint32_t second[] = {53, 67, 79};
static void expect(bool ok) { if (!ok) std::abort(); }
int main() {
    expect(reinterpret_cast<uintptr_t>(first) > UINT32_MAX);
    expect(reinterpret_cast<uintptr_t>(second) > UINT32_MAX);
    const uint32_t a = uint32_t(reinterpret_cast<uintptr_t>(first));
    const uint32_t b = uint32_t(reinterpret_cast<uintptr_t>(second));
    expect(a && b && a != b);
    expect(setup_filter_pointer(a, first) == first);
    expect(setup_filter_pointer(b, second) == second);
    expect(setup_filter_pointer(a, first)[2] == 41);
    expect(setup_filter_pointer(b, second)[2] == 79);
    expect(setup_filter_pointer(0, first) == nullptr);
    expect(setup_filter_pointer(0x80048000u, first) == reinterpret_cast<const uint32_t*>(uintptr_t(0x80048000u)));
    expect(setup_filter_pointer(b, first) == reinterpret_cast<const uint32_t*>(uintptr_t(b)));
    std::printf("PASS audio filter decoder: two real high tables, exact low32 identity, readable native bytes, null/MEM1/foreign preserved\n");
}
'''
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(test.replace('FUNCTION', function), encoding='utf-8')
output.with_suffix('.json').write_text(json.dumps(dict(source=str(source_path),
    source_sha256=hashlib.sha256(source_path.read_bytes()).hexdigest(),
    extracted_function_sha256=hashlib.sha256(function.encode()).hexdigest(),
    scope='Exact production address decoder only; real DSP/game integration is a separate runtime check'), indent=2) + '\n')
print(output)
