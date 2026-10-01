# MWCC Matching Tips for Agents

This document collects practical knowledge about how the Metrowerks CodeWarrior C++ compiler (MWCC GC/1.2.5) generates PowerPC code. These tips help when iterating on source code to achieve a byte-identical match against the original binary.

Read this document before attempting to match functions. The patterns described here are recurring and will save significant trial-and-error time. Expand the document as needed. Ask for human review whenever the contents of the document disagree with observed reality.

## What NOT to focus on

There are some things that might seem like low hanging fruit at first sight but really are not. They are:

- function stack frame size
- prologue and epilogue shape
- wrong registers being used

These things are usually the hardest ones to get right and so should be left for the very end of matching an entire file, when nothing else remains to be done.
Instead of these, first and foremost wrong instructions being used and ordering of these instructions should be resolved.

## Function definition order matters

Functions with non-weak symbols must be defined in code in the exact order they appear in the target binary.
MWCC never reorders symbols, so this is a rule that must never be broken.

## Do not use reinterpret casts to access fields

Never use raw byte offsets and reinterpret casts to access fields -- the codegen from that almost always differs from using proper fields.
Instead, introduce the relevant fields and use them, or rather even getters/setters if already available.

## MWCC dislikes reordering

The compiler never reorders memory stores, loads and function calls relative to one another.
They always should be performed in source code in the exact order they appear in target assembly.
This a good first place to focus on when matching, as it helps shape the rest of the function.

## MWCC can eliminate redundant reads, but not writes

All writes to memory that happen in code will be reproduced in assembly, and vice-versa, when reconstructing code from assembly, you MUST do repeated redundant stores into memory -- that's just the way the original code was written.

Redundant reads, on the other hand, can be eliminated by MWCC, but not always, e.g. function inlining might inhibit this.

## MWCC is very reluctant to optimize anything that uses stores ints to memory

If a Vec struct that contains 3 floats is copied via it's compiler-generated copy ctor or assignment operator -- it will be compiled to integer loads/stores. This makes the compiler unable to keep the values used in registers and forces it to spill them to stack, even if the surrounding code clearly allows for them to stay in floating point registers.

Same logic applies to a Color struct that contains four 8-bit ints: if the ints are initialized one by one, then the compiler will not be able to optimize it to simple bit manipulations in integral registers and will keep the struct on the stack.

## A constructor that is only *declared* in the header is never inlined

This one produced large, systematic wins on the `TMarNameRefGen::getNameRef_*` factory functions, so it is worth checking early in any TU that instantiates many small game classes.

The failure mode looks like this in the diff: at a `new TSomeObj` site the ROM calls the *base* constructor and then re-stores the vtable, while our build calls the *derived* constructor out of line.

```
bl TMapObjBase::TMapObjBase(const char*)   ; ROM: base ctor, then...
lis r3, __vt__11TCoverFruit@ha             ; ...vtable and vtable+0x24 stores
addi r3, r3, __vt__11TCoverFruit@l
stw r3, 0(r30)
addi r0, r3, 0x24
stw r0, 0x20(r30)
```

versus our single

```
bl TCoverFruit::TCoverFruit(const char*)   ; ours: out-of-line, body not visible
```

MWCC only inlines a member function when its definition is visible in the translation unit.
A header that merely declares `TCoverFruit(const char* name = "...");` gives the compiler nothing to inline, so it emits (or calls) a weak out-of-line symbol instead — even though the ROM has no such symbol at all.

The reconstruction is the obvious one: define the constructor inline in the header, forwarding to the base.

```cpp
TCoverFruit(const char* name = "フタのフルーツ")
    : TMapObjBase(name)
{
}
```

The `stw r0, 0x20(r30)` store of `vtable + 0x24` is *not* something the derived constructor has to write by hand: it is what MWCC emits automatically after a base constructor call, so an empty forwarding body reproduces the whole sequence.

How to tell the two cases apart, per class:

- No `__ct__<len><Class>F...` symbol in `config/GMSP01/symbols.txt` means the ROM never emitted an out-of-line copy, so the constructor was inline in the original header. Define it there.
- A `__ct__` symbol means the constructor really is out of line. Leave it in the `.cpp`; defining it in the header as well is a compile error (`redefined`).

When a class has a real body (member initialisers, extra statements), that body has to move into the header too, otherwise the factory still cannot inline it and the fix does nothing.

A useful sweep: classes declared as `class X : public Y {` followed by a bare `X(const char* name = "...");` line.
`tools/find_bare_ctors.py` lists them and cross-references `symbols.txt` to classify each one.
In this repository that pattern covered 245 classes, 23 of which had no `__ct__` symbol and 19 of those were actually instantiated.

Beware that a class with a complex constructor body (calls, casts) will not be inlined even when declared in the header, and then the ROM *would* have a `__ct__` symbol for it.
Those cases are not this pattern; leave them alone.

## Recovering a name-factory function from the ROM

`TMarNameRefGen::getNameRef_*` are long `if (strcmp(...)) return new T...;` chains.
When one of them is badly reconstructed, do not guess branch by branch: extract the whole table from the original and rewrite the function from it.

`tools/branch_table.py <target .s>` prints, in ROM order, one row per `strcmp` site:

```
STRING                          SIZE    CONSTRUCTED BY                        NAME ARG
'BossGesso'                     0x1b0   __ct__10TBossGessoFPCc                 '\u30dc\u30b9\u30b2\u30c3\u30b5\u30fc'
```

- `STRING` is the comparison key, `SIZE` the `li r3, 0x...` passed to `operator new` (i.e. `sizeof` the class), `CONSTRUCTED BY` the constructor that is actually called, and `NAME ARG` the string loaded into `r4` just before that call.

Three things this immediately exposes, all of which had been wrong in `getNameRef_BossEnemy`:

- branches that do not exist in the ROM at all (fabricated `LimitKoopa*`, `HinoKuri2`, ...). Every extra branch is a few wrong instructions plus a string constant that shifts all later `.rodata` offsets.
- branches present in the ROM but commented out as `TODO` in our source.
- the class a name maps to. `"SleepBossHanachan"` does **not** build a `TSleepBossHanachan`: the ROM calls `TSpineEnemy("?")` and then stores `__vt__24TDemoBossHanachan`, i.e. the class is `TDemoBossHanachan` and the string is only a scene name.

The comparison key and the name argument are not always in `.rodata`.
Short strings (`"EMario"`, `"Koopa"`, `"OilBall"`, ...) live in `.sdata2` and are loaded with `li r4, "@NNNN"@sda21`, so a script that only scans `.rodata` will wrongly report them as absent, and dtk does not dump `.sdata2` in the `.s` file at all.
Read it from the extracted object instead:

```
build/binutils/powerpc-eabi-objdump.exe -s -j .sdata2 build/GMSP01/obj/<unit>.o
build/binutils/powerpc-eabi-objdump.exe -t build/GMSP01/obj/<unit>.o   # symbol -> offset
```

`.sdata2` can also hold non-strings: the `?` used as a name for six classes in that factory is the one-character string at offset 0x08, not a literal question mark in the source.

Note that the constructor shown in the table is often the **base** constructor, because the derived constructor is inlined (see the section above).
Size the class from the `operator new` immediate, and take the derived class from the vtable stored right after the base call.

### The `.rodata` prologue every name factory starts with

Once the branches line up, every `getNameRef_*` still shows a uniform shift of *all* its string offsets, because the original `.rodata` opens with a fixed run of constants that nothing in the function references:

```
+0x00  12 zero bytes                  (dummyMactorStringValue1)
+0x0C  "メモリが足りません\n"           (SMS_NO_MEMORY_MESSAGE)
+0x20  "MActorMtxCalcType_Basic ..."   } the four
+0x50  "MActorMtxCalcType_Softimage..."} MtxCalcTypeName
+0x88  "MActorMtxCalcType_MotionBlend..."} entries
+0xBC  "MActorMtxCalcType_User ..."
+0xE0  <- the first real name
```

That is exactly what `M3DUtil/InfectiousStrings.hpp` pulls in (it includes `System/DummyStrings.hpp` first, so the pair lands ahead of the four names), and ~30 retail TUs in this repository already include it for this reason.

Adding `#include <M3DUtil/InfectiousStrings.hpp>` to a factory makes its `.rodata` prologue byte-identical to the target's and removes the shift on every string reference in the function.

This barely moves the fuzzy percentage, because a wrong 16-bit offset still earns most of the byte credit, so do not judge it by the score: compare the two `.rodata` dumps directly.

```
build/binutils/powerpc-eabi-objdump.exe -s -j .rodata build/GMSP01/src/<unit>.o
build/binutils/powerpc-eabi-objdump.exe -s -j .rodata build/GMSP01/obj/<unit>.o
```

What is left after that is TU-specific: `MarNameRefGen_MapObj` still needs 24 more bytes (12 zero bytes then three `1.0f`) at `+0xE0`, which come from some static of the original file that has not been identified yet.

## Measuring an UNUSED function against its map size

`validate-symbol-order.py` reports UNUSED symbols as MISSING whenever the optimiser folded the body away, and it can only compare a size when the symbol is actually emitted.
That combination makes it look like a failure when the reconstruction is in fact perfect.

To check a candidate body, temporarily define the function **out of line** (drop the `inline` keyword) and read the compiled size:

```
ninja build/GMSP01/src/<path>.o
build/binutils/powerpc-eabi-nm.exe -S build/GMSP01/src/<path>.o | grep <symbol>
```

Concretely: `TGesso::checkDropInWater` is listed as UNUSED at 0x144 bytes and is absent from our object.
Defined out of line it compiles to exactly `00000144`, which proves the guessed body is byte-for-byte the original.

**Then put the `inline` back.** The symbol being UNUSED means the original had no live call to it, so the call site was inlined; emitting a real `bl` there costs real match quality (this cost 0.01% of overall fuzzy match).
Keep the measurement in a comment next to the function, and treat the validator's MISSING entry for that symbol as expected.

Beware that this only works for a function defined in the `.cpp`.
Moving an inline *header* function out of line changes every TU that includes it.

## MWCC 1.2.5 stack padding bugs

Our version of MWCC has a bug where the backend allocates more stack than necessary.

Most commonly this happens when functions were inlined: inlined function calls often inflate the stack frame size. To correctly match a function with stack frame size issues, UNUSED inlines from the MAP need to be reconstructed based on their size, and sometimes new inlines need to be fabricated based on own's best judgement.

Another instance of it is using a ternary operator sometimes taking up more stack than using ifs.

Next, local variables can expand the stack even if they are always stored in a register and never actually spilled to the stack.

When no obviously correct way to make stack frame size match exists, a trick should be used to correctly match the function's context: a temporary char array of required size to inflate the stack. Such hacks however should be removed or commented out after the function is matching to allow for a possible proper solution in the future.

## Ifs

Ifs are always compiled to very simple code:
- compare (`cmpwi`/`cmplwi`/etc, or arithmetic instruction with a dot)
- conditional branch (`beq`/`bne`/`ble`/etc)
- the true block
- unconditional branch to end of false block (`b`)
- the false block

The compiler **NEVER** swaps the order of the true block and false block.
It is also very reluctant in changing the control flow, so C++ control flow usually corresponds to assembly one to one.

Ternary operator is compiled similarly, but it is the one exception to control flow  being the same. In the following case MWCC might initialize the variable's register with the "otherwise" value (zero) instead of doing so in the false branch, which eliminates the false branch entirely.
```
int b = thing == nullptr ? thing->field : 0;
```

## Sequential integer comparisons in a disjunction

Whe MWCC sees code like `if (a == 8 || a == 9 || a == 10)` it can optimize it to be `if (a - 8 <= 2)` sometimes. When the latter pattern is encountered with enums -- it should be reversed into multiple disjuncted equality comparisons.

## Assigning a `&&`/`||` chain to a bool synthesizes intermediate bools

When a multi-term short-circuit expression is **assigned to a `bool` variable**, MWCC materializes its own intermediate boolean temporaries for the chain — it initializes a flag register to `0`, sets it to `1` when a sub-condition passes, and merges flags for each grouping level.
This produces a distinctive shape: several `li rN, 0` up front, then
`li rN, 1` at the deep success points, then `clrlwi.`/`beq` merges.

```cpp
// Target has flag-register merges (r0/r4/r3 = 0, then =1 at success, then merged).
// This form reproduces them:
bool same = (-e <= dx && dx <= e) && (-e <= dy && dy <= e) && (-e <= dz && dz <= e);
if (!same) { ... }

// Nested ifs do NOT — they compile to early-exit branches straight to the body,
// with no intermediate flag bools:
bool same = false;
if (-e <= dx && dx <= e)
    if (-e <= dy && dy <= e)
        if (-e <= dz && dz <= e)
            same = true;
```

When the target shows those `li 0`/`li 1`/merge flags for a compound predicate, spell it as a single `bool x = cond && cond && ...;` assignment (grouped to match the flag nesting), not as nested `if`s.

Concrete case (`MtxUtil.cpp` `TRope::constraintTail`): nested-if near-zero test 89.3%;
`bool same = (-e<=dx&&dx<=e) && ...;` with `<=` and inline `-JGeometry::TUtil<f32>::epsilon()`
→ 99.8%.

## Switches

MWCC can compile switches in one of two ways: jump table or branching.
Jump tables are easily identifiable via `mtctr` and `bctr` instructions being used.
Switches that became branches usually have control flow that doesn't look like an if: multiple conditional branch instructions follow a single comparison instruction. E.g.
```
cmpwi r0, 0x1
beq ...
bge ...
cmpwi r0, 0x0
bge ...
b   ...
... the code block inside the switch ...
```

### Case ordering is strict

MWCC **never reorders cases in a switch statement** during compilation. The order of `case` labels in the source code must match exactly the order they appear in the assembled jump table or branch chain. If a function exhibits a non-matching switch and you've verified the case logic is correct, inspect the target assembly to determine the actual case sequence and reorder the source cases accordingly. Even if semantically equivalent, the compiled output will differ until case order matches.

## Nonsensical control flow

As MWCC inlines functions, sometimes nonsensical control flow will be encountered in the assembly, one that doesn't correspond to any structured control flow constructions like switches, ifs or loops. Such cases are usually explained by **function inlining** rather than gotos. The place where a goto was supposedly used would actually correspond to a return statement, and the place where it points to would be the boundary of the inlined call.

## Keep track of known relevant inlines

Reconstructing correct inline calls is crucial in matching code correctly. When a similar block of code reoccurs -- always consider the possibility that it's an inline, but never disregard the possibility that the original authors simply copy-pasted it. When starting on a new function, explore the inlines already available in the different classes that it uses, as well as in the current translation unit.

UNUSED functions from the MAP must often be reconstructed even when they do not exist as standalone code in the final binary: their inlined bodies still determine caller codegen (register allocation, load offsets, branch layout). Treat their MAP signature and size as constraints, recover a plausible body from repeated callsite patterns, and validate by diffing the caller(s) after each inline-shape change rather than expecting a direct symbol-level match for the UNUSED function itself.

### A getter that repeats before every operation is a wrapper inline

m2c writes an inlined wrapper as one pointer local that is assigned again and again:

```cpp
pBuf = DSPInterface::getDSPHandle(channel->unk0);
pBuf->setPauseFlag(1);
pBuf = DSPInterface::getDSPHandle(channel->unk0);
pBuf->flushChannel();
```

When the same lookup comes back before every single operation, the original source almost always called a wrapper that hides the lookup:

```cpp
DSPInterface::setPauseFlag(channel->unk0, 1);
DSPInterface::flushChannel(channel->unk0);
```

Both forms give the same instructions, but the wrapper form reads correctly and it colours the registers correctly in long functions.
`JAInter::StreamLib::callBack` went from 98.7% to 99.8% on this rewrite alone.
Keep the pointer local only where the code **reads a field** through it.

## Reference Locals Affect Register Allocation

Introducing a reference local before accessing struct members can change how the compiler allocates registers:

```cpp
// Direct access — compiler may reload base pointer each time:
unk0[i].mPos.set(...);
unk0[i].mVel.set(0.0f, 0.0f, 0.0f);

// Reference local — compiler keeps base in a register:
Node& node = unk0[i];
node.mPos.set(...);
node.mVel.set(0.0f, 0.0f, 0.0f);
```

Check whether the target reloads the base address or reuses a register. If it reuses, a reference local is likely in the original code.

## Inlined Functions Only Load `this` Once

Whenever a simple implementation loads a pointer into a register multiple times but the original did it once — the original might have used a function that got inlined.

```cpp
// `field` pointer could get loaded twice because
// the compiler assumes function calls can modify any memory
field->nonInlinedFuncCall1();
field->nonInlinedFuncCall2();

// `field` pointer loaded as `this` once
field->inlineFuncThatCallsBoth();

...

class FieldClass {
...
  void inlineFuncThatCallsBoth() {
    nonInlinedFuncCall1();
    nonInlinedFuncCall2();
  }
...
};
```

This is sometimes indistinguishable from a local reference being used.

## Intermediate Variables Heavily Influence Register Allocation

Intermediate variables are not inherently bad. They are a strong codegen lever in MWCC and can change register pressure, load/store timing, and expression scheduling.

Start with the simplest hypothesis, then iterate:
1. First try a fully inlined arithmetic form (no intermediate variables).
2. If that does not match, try decomposed forms with temporaries.
3. If still nonmatching, vary grouping/order (including compound assignments).

Examples (all semantically equivalent, all can compile differently):

```cpp
// Inline form
f32 c0 = (z1 - z0) * (x2 - x1) - (x1 - x0) * (z2 - z1);

// Decomposed form A
f32 dx = x1 - x0;
f32 dz = z1 - z0;
f32 c0 = dz * (x2 - x1) - dx * (z2 - z1);

// Decomposed form B
f32 c0 = (z1 - z0) * (x2 - x1);
c0 -= (x1 - x0) * (z2 - z1);
```

Practical rule: do not treat temporaries as "wrong" or inlining as "always right". Treat decomposition style as an iterative search dimension and test multiple shapes until one matches. When trying different approaches, always try things that look reasonable to have been written by a human first, and only if nothing sensible works start trying crazy and weird approaches.

## Avoid Reference Aliases as a Workaround

When trying to match complex struct access patterns, DO NOT use reference aliases as a general strategy to "improve" codegen. While they can sometimes help with register reuse (see "Reference Locals Affect Register Allocation" above), they frequently harm matching by changing pointer offset calculations and register assignments in ways that deviate from the original.

Only introduce reference aliases when:
1. The original code clearly had them (evidenced by a single load of a base pointer that's reused)
2. Direct member access results in repeated redundant loads that don't appear in the target
3. You are confident from diff analysis that a reference was intended

Otherwise, use direct member access or direct getter calls inline in expressions and let the common subexpression elimination & redundant load optimizations do the rest.

## Reconstructing initializer-heavy functions (init/ctors) from raw asm

When a function body is mostly field assignments (typical for `init()` / constructors), the fastest path is to decode the raw `.s` file directly rather than relying on `m2c` or guessing.

Workflow:

1. Find the function in `build/GMSJ01/asm/<path>.s` by mangled name (`grep -n` for `.fn <mangled>`).
2. Walk the prologue forward and map every `stb`/`sth`/`stw`/`stfs` to a class field using the `/* 0xNN */` offsets in the header. Each store = one source statement.
3. Float and integer constants live at the bottom of the same `.s` file as `.obj "@NNNN"` blocks with `.float`/`.4byte` directives. The assembly references them as `lfs f0, "@NNNN"@sda21(r0)`.
4. String literals (joint names, file paths) are in `.rodata` / `.sdata2` and referenced as `addi r4, r30, 0xNNN` where `r30` is the loaded address of `@1490` (the TU's local rodata base).
5. Cross-reference against the existing partial source — anything in the asm but not in the source is missing.

Compare offsets one-for-one. If the asm writes to offsets `0xFC, 0x100, 0x104` and the source only zeroes `0x108..0x110`, you're missing a `unkFC.zero();` before `unk108.zero();`.

## Recognising common MWCC store idioms

**`u32 → f32` cast** (typical for `(f32)someByte` from a packed struct like `GXColor`):
```
lbz   r0, OFF(rBase)              ; load unsigned byte
lis   r4, 0x4330                  ; high half of magic constant
lfd   f1, "@magic"@sda21(r0)      ; magic = 4503599627370496.0 (0x4330000000000000)
stw   r0, X(r1)                   ; store low half on stack
stw   r4, X-4(r1)                 ; store high half on stack
lfd   f0, X-4(r1)                 ; load as double
fsubs f0, f0, f1                  ; subtract magic → real float value
stfs  f0, FIELD(rThis)
```
Source: `unk84.x = (f32)bodyColor[0].r;` — straight cast of a `u8` field to `f32`.

**`(u8)` cast on an `int`-returning call before storing to a `u16` field**:
```
bl    JUTNameTab::getIndex(...)
clrlwi r0, r3, 24                 ; mask to low 8 bits
sth    r0, FIELD(rThis)           ; store as halfword
```
Source: `mField = (u8)getIndex(...);` even though `mField` is declared `u16`. Without the `(u8)` cast, MWCC emits `sth r3, FIELD(rThis)` directly with no `clrlwi`. This pattern appears repeatedly for joint-index assignments.

## Constant Hoisting and Loop Codegen

The compiler hoists constant loads (`lfs`, `lfd` from SDA/SDA2) before loops. The exact set of constants hoisted depends on:
- Which expressions appear in the loop body
- Which inline functions are called (they may reference additional constants)
- The order of operations within the loop

If the target hoists a constant (e.g., `lfd f28, @5181@sda21`) before a loop but our build does not, it means the compiler sees a different code structure. This is usually a symptom of a deeper structural mismatch in the loop body or inlined functions, not fixable by just moving the load.

## Function-local statics with non-trivial initializers: `init$NNN` is the guard

When a function-local `static` has a non-trivial initializer that has to run at first call — typically a constructor call like `static JGeometry::TVec3<f32> pos(1815.0f, 1500.0f, 1550.0f);` — MWCC emits **two symbols in `.sbss`**:

- The variable itself (e.g. `pos$NNN`), sized for the type (a Vec is 12 bytes).
- A 1-byte construction guard named `init$NNN` (note the literal name `init`, regardless of what your source variable is called).

The function-entry codegen is:

```asm
lbz   r0, init$NNN@sda21       ; load the guard byte
extsb. r0, r0                  ; sign-extend with dot (sets CR)
bne   .L_skip_init             ; already constructed → skip
... initializer code, writes to pos$NNN ...
li    r0, 1
stb   r0, init$NNN@sda21       ; mark constructed
.L_skip_init:
```

So when the target shows an `init$NNN` byte that is loaded with `lbz` + `extsb.` and tested, **don't** model it as a plain `static bool foo;` in your source — model it as the *guard* for some other static local with a real constructor. Pick the source statement that produces the matching initializer body (commonly a `JGeometry::TVec3<f32>(x, y, z)` if the init code writes three floats).

Aggregate-initialized PODs (`static Vec pos = { 1.0f, 2.0f, 3.0f };`) and zero-initialized statics (`static Vec pos;`) do NOT produce a guard — they sit in `.data` / `.bss` with no first-call check. The `init$NNN` pattern only appears when MWCC needs to run constructor code at first entry.

## A zero-argument call of a constructor with default arguments costs one inline pass

MWCC expands inline calls in passes, and each pass permits a smaller callee
(pass 0: no limit, pass 1: 10 statements, pass 2: 7, pass 3: 3, pass 4: none).
A constructor written with default arguments adds one pass to that count when
it is called with **no** arguments:

```cpp
class TGameSequence {
	TGameSequence(u8 stage = 0, u8 scenario = 0, JDrama::TFlagT<u16> flag = 0)
	{
		set(stage, scenario, flag);
	}
};

TGameSequence local;          // synthesized __ct__13TGameSequenceFv, 1 statement,
                              // then the real ctor one pass deeper
TGameSequence local(a, b);    // the real ctor directly, no wrapper
```

The same rule applies to a member of such a type that the enclosing
constructor default-initialises, for example `TFlagT<u16> unkC;` in
`JDrama::TViewObj`. The wrapper has no symbol of its own; it is always
expanded.

How to recognise it: the calls inside a constructor body stay `bl` although
the ladder says they fit, while the same callees are expanded at that depth
elsewhere. `TApplication::TApplication()` keeps `bl TFlagT::TFlagT(const
TFlagT&)` and `bl TFlagT::set(u16)` for each `TGameSequence` member; with a
plain `TGameSequence() { set(0, 0, 0); }` both are expanded and the function
is at 36%. The default-argument form gives 100%. `inline_trace.py` shows the
wrapper as `__ct__XFv [1 stmt]` one pass above the real constructor.

## A member function called on a bare global substitutes `this`; called through an accessor it binds it

When an inline member function is called on a global object written by name,
`gpApplication.setNextArea(x)`, MWCC substitutes `this` with the constant
address, folds the member offsets, and then CSEs `gpApplication + 0x12` into a
callee-saved register:

```
addi r30, r5, 0x12       ; &gpApplication.mNextArea
stb  r0, 0x12(r5)
...
addi r3, r30, 2          ; &mNextArea.unk2
```

When the receiver is an expression that MWCC will not repeat (an inline
accessor call, a local pointer, or a local reference), `this` is bound to a
temporary that holds `&gpApplication` itself, and every offset folds from it:

```
addi r31, r4, gpApplication@l
stb  r0, 0x12(r31)
stb  r0, 0x13(r31)
addi r3, r31, 0x14
```

The second shape, with the global's own address kept across the call, is the
sign that the original went through an accessor such as `SMSGetMSound()` or
`SMSGetMarDirector()`. Nine functions of `System/` show it for
`gpApplication`, and every other `gpApplication` site in the tree compiles to
the same bytes through `SMSGetApplication()`, so the accessor is used
everywhere. Expect the same of the other `SMSGet*` accessors: the bare global
and the accessor differ only where the result feeds another inline call.

The same test tells a getter from a raw field read. A getter such as
`u8 getStage() const { return unk0; }` reads through `this`, so its return
value is force-loaded into a compiler temporary. Two visible effects: the
temporary keeps a 4-byte slot (a frame 4 bytes short per call is the
symptom), and when two such calls are the arguments of one `bl`, MWCC
evaluates them in the reverse order. `TMarDirector::currentStateFinalize`
loads `mCurrArea.unk1` before `unk0` for `endStageEntranceDemo`, which only
the getters reproduce. An accessor that returns a reference leaves no trace
at all, so it can neither be proved nor disproved this way.

## Default arguments vs. spelling them out changes inlining

When a base/member constructor is invoked with a value that happens to be that
constructor's own default argument, prefer the default-arg call form over
repeating the literal. It is not just cosmetic: it can change MWCC's inline
depth decisions.

Concrete case (`BathWaterManager.cpp`): `JDrama::TViewObj`'s ctor defaults its
name to `"<TViewObj>"`. Writing a member's base as `JDrama::TViewObj("<TViewObj>")`
inlined the nested `TNameRef`/`TFlagT` ctors one level too deep; writing it as
`JDrama::TViewObj()` (letting the default supply the identical string) kept those
nested ctors as real `bl` calls, exactly like the target. This took the
enclosing constructor from 90.6% to 100%. If a nested ctor inlines when the
target keeps it as a call (or vice-versa), check whether the original used a
default argument.

## Unsized arrays go to `.data`, sized ones may go to `.sdata`

An array declared with an explicit size that is small enough (≤ the `-sdata`
threshold, default 8 bytes) can be placed in `.sdata` and addressed GP-relative
(`@sda21`, giving an indexed `lwzx` load). An array declared **unsized** — e.g.
`static const char* fileNames[]` with the size deduced from the initializer —
is always placed in `.data` instead, and addressed absolutely (`lis/addi @ha/@l`
+ `add`/`lwz`).

So if a small global array should live in `.data` (target uses `lis/addi`) but
yours lands in `.sdata` (`@sda21`), drop the explicit array bound and let the
initializer size it. Concrete case (`BathWaterManager.cpp`): `fileNames[2]` →
`.sdata` (nonmatching); `fileNames[]` → `.data` (match).

## Local Symbol Mangling: `@unnamed@` vs `static`

MWCC mangles symbols inside anonymous namespaces with an `@unnamed@` prefix.

- If a local symbol's mangled name includes `@unnamed@`, model it as being in an anonymous namespace.
- If the symbol is local but does not include `@unnamed@`, prefer a plain `static` function/variable instead.

## Symbol Order with `-inline deferred`

When a TU is compiled with `-inline deferred` (see TU-specific flags in `configure.py`), define symbols in reverse order relative to the map/symbol listing for that TU.

- In practice, function-definition order in the `.cpp` should be reversed for those TUs.
- If order-sensitive matching drifts for an `-inline deferred` TU, verify definition order before attempting smaller codegen tweaks.

## TVec3 / Vector Codegen Patterns

`JGeometry::TVec3<f32>` is a 12-byte struct with `x`, `y`, `z` float members. How you read/write it drastically affects code generation.

### Use the inlines

`TVec3` and related `JGeometry` types have lots of inlines, and most of the time the original code would have used those.
When naive decompilation of the math doesn't work out, try using the inlines.

### Multiplication by zero/one

MWCC can optimize out multiplication by floating point one or zero written out explicitly in code, but fails to do so when the multiplication comes from an inlined function.
This routinely occurs when an "up" vector is created and then cross-multiplied with another vector -- codegen contains trivial multiplications by zero or one.
When decompiled naively as `1.0f * unk170.z - 0.0f * unk170.y`, MWCC will optimize the multiplications out, but if the "up" vector is properly created and the `cross` inline is used, it will fail to optimize them out, achieving the desired codegen.

### Construction: component-by-component vs constructor

```cpp
// Constructor form — compiler batches all loads, then all stores:
//   lfs f0, ...; lfs f1, ...; lfs f2, ...
//   stfs f0, 0(rN); stfs f1, 4(rN); stfs f2, 8(rN)
JGeometry::TVec3<f32> pos(x, y, z);

// Component-by-component — compiler interleaves load/store pairs:
//   lfs f0, ...; stfs f0, 0(rN)
//   lfs f0, ...; stfs f0, 4(rN)
//   lfs f0, ...; stfs f0, 8(rN)
JGeometry::TVec3<f32> pos;
pos.x = x;
pos.y = y;
pos.z = z;
```

Check the target assembly to see which pattern (batched vs interleaved) is used, and write the source accordingly.

### `Vec3::zero()` vs `Vec3::set(0,0,0)`

`.zero()` emits stores in **reverse component order** (`z`, `y`, `x` → offsets `+8, +4, +0`). `.set(0,0,0)` typically emits forward order. Match the asm order to pick the right call.

### Assignment: `operator=` vs `.set()`

```cpp
// operator= (struct copy) — generates lwz/stw (word load/store):
//   lwz r0, 0(rSrc); stw r0, 0(rDst)
//   lwz r0, 4(rSrc); stw r0, 4(rDst)
//   lwz r0, 8(rSrc); stw r0, 8(rDst)
node.mPos = param_1;

// .set(vec) (float copy) — generates lfs/stfs (float load/store):
//   lfs f0, 0(rSrc); stfs f0, 0(rDst)
//   lfs f0, 4(rSrc); stfs f0, 4(rDst)
//   lfs f0, 8(rSrc); stfs f0, 8(rDst)
node.mPos.set(param_1);

// .set(x, y, z) (3-arg form) — generates lfs/stfs like component assignment
node.mPos.set(expr_x, expr_y, expr_z);
```

The target assembly will clearly show `lwz`/`stw` (integer move) vs `lfs`/`stfs` (float move). Choose the source pattern that matches.

### Constructor with a zero component + `+=` vs direct component init

When building a vector where some components are `0.0f + <expr>` and one is a
real value, the target may show *literal* `0.0` adds on the zero components:
```
lfs f4, @zero ; fadds f4, f4, f0   ; component = 0.0 + src.x
```
This is the signature of the original writing a base vector with literal-zero
components and then adding another vector, e.g.
```cpp
// point2 = (0, -unkC.y, 0) then add unk58 -> emits the "0.0 + unk58.x" adds
JGeometry::TVec3<f32> point2(0.0f, -1.0f * unkC.y, 0.0f);
point2 += data.unk58;
```
Writing the fully-fused form `TVec3(unk58.x, -unkC.y + unk58.y, unk58.z)`
instead reads the components directly with no zero-adds and does not match.
Concrete case (`BathWaterManager.cpp` `calcBathtub` inner-band branch): the
`+=` form recovered the `0.0 + x`/`0.0 + z` adds; see next tip for the `-1.0f *`.

### Negation as `-1.0f * x` (fmuls) vs `-x` (fneg)

`-x` compiles to `fneg`. But the target sometimes shows a standalone
`fmuls f_, f(-1.0), f(x)` (multiply by the `-1.0` SDA constant) where you'd
expect a negation — and crucially kept as a *separate* fmuls, not fused into a
following `fmadds`/`fnmsubs`. This happens when the original literally wrote
`-1.0f * x` (MWCC in does not algebraically fold
`-1.0f * x` -> `fneg x`). Prefer `-1.0f * x` over `-x` when the target shows the
extra `-1.0` load + `fmuls`. Concrete case (`BathWaterManager.cpp`
`calcBathtub`): `-1.0f * unkC.y` matched; `-unkC.y` gave `fneg` and did not.

### Trouble in fnmsubs town

Computation-heavy code commonly has code like `B - A * C` or `A * C - B`  for floating-point A, B, C. E.g. any cross product will have analogous code.
MWCC likes to compile such code into PowerPC `fmsubs` and `fnmsubs` instructions, but they have a quirk in how they are implemented related to double to single precision conversions: technically, fnmsubs is `-float(A * C - B)`, but MWCC uses it for `B - A * C`.
Decompilers like ghidra and m2c like to preserve this technicality and commonly emit code like `-(a * b - c * d)` in code like vector cross products.
Rewriting it as `c * d - a * b`, opening the braces, usually helps matching it better.

## Reading a source array through `void*` defeats CSE (controls loop unroll)

When a loop copies the same source element to several destinations, e.g.
```cpp
TVec3<s16>* src = (TVec3<s16>*)getVtxPosArray();
for (...) { a[i] = src[i]; b[i] = src[i]; c[i] = src[i]; }
```
MWCC's alias analysis can prove `src` (a clean typed pointer) doesn't alias the
`new`-allocated destinations and **CSEs the `src[i]` load** — reading it once and
reusing it for all three stores. That shrinks the loop body, and MWCC's `-O4`
unroller then unrolls it *more* (e.g. ×8). The target often reloads `src[i]` for
every store (no CSE) and unrolls only ×2.

To reproduce the reloads (and the smaller unroll factor), read the source
through a `void*` and cast at each use, which blocks the alias analysis:
```cpp
void* src = getVtxPosArray();
for (...) {
    a[i] = ((TVec3<s16>*)src)[i];
    b[i] = ((TVec3<s16>*)src)[i];
    c[i] = ((TVec3<s16>*)src)[i];
}
```
Concrete case (`DrawUtil.cpp` `TTrembleModelEffect::init`): the typed-`src` form
CSE'd + unrolled ×8 (54%); the `void*`-cast form reloaded + unrolled ×2 (100%).

## `const` on an inline's pointer parameter also defeats CSE

The same effect appears at an inline boundary, and there the fix is one word.
When an inline reads a field through a pointer parameter and the caller reads
the *same* field of the *same* object, MWCC normally merges the two into one
load. Making the parameter point to `const` stops the merge, and both loads
appear.

```cpp
// one load of symbol->mNameOffset, shared with the caller
const char* getSymbolName(TSpcSymbol* symbol);
// two loads: the inline's own, and the caller's
const char* getSymbolName(const TSpcSymbol* symbol);
```

Concrete case (`Strategic/spcinterp.cpp` `TSpcInterp::dump`): the original loads
`symbol->mNameOffset` twice, once inside the inlined `getSymbolName` and once as
a `SpcTrace` argument. Adding `const` took `dump` from 98.8% to 100%, and left
every other caller of `getSymbolName` matching.

This is worth reaching for before restructuring anything: it is a smaller and
far more plausible change than the usual alternatives (hoisting the field into a
local, or dropping the intermediate local altogether), both of which were tried
here and were worse.

## An inline's locals are numbered in reverse when it is inlined

This one explains a whole family of "the out-of-line copy and the inlined copy
want opposite stack layouts" puzzles, so it is worth knowing the mechanism.

`CParser_NewLocalDataObject` **prepends** each new local to the function's
`locals` list, so the list is in reverse declaration order. Two consumers then
walk that list forward:

- `assign_locals_to_memory(locals)` gives out stack offsets, **increasing** —
  so when a function is compiled normally, the **first-declared** local ends up
  at the **highest** offset.
- `CInline_SetupArgsExpression` recreates the callee's locals in the caller,
  walking the packed array forward and prepending each one — which **reverses
  the order a second time**. So in an inlined copy the **last-declared** local
  ends up at the highest offset.

The consequence: any two named locals of an inline function **swap places**
between its out-of-line copy and every site that inlines it. No declaration
order satisfies both. Measured on `TSpcInterp::fetchU32`: `src` first gives the
four inline sites 100% and the out-of-line copy 99.8%; `result` first gives
exactly the reverse.

The escape is not to reorder but to **reduce the function to a single local
that needs a stack home** — with nothing to swap, both layouts agree. A local
whose initialiser carries a type-conversion node gets propagated into a compiler
temporary and stops needing a home, so writing the accessor to return `void*`
and casting at the call site is enough:

```cpp
void* getText(u32 offset);                        // was u8*
u8* src = (u8*)mBinary->getText(mProgramCounter);  // now a temp, not a local
u32 result;                                       // the only named local left
```

That took `fetchU32`, `fetchS32`, `execvar` and `execfunc` to 100% at once
(`spcinterp` 62.2% -> 67.7% matched code). Note it also fixed the two call sites
that fetch *twice*: a second named local was what pushed their temporaries out
of step, not anything about the second fetch itself.

Symptom to recognise: the out-of-line copy of a weak inline and its inline sites
each want the same pair of slots in opposite orders, and both frames are already
the right size.

## `T x = f();` costs one more stack object than `T x; x = f();`

When `f` is inlined, initialising a local from the call builds the return value
in its own temporary and then copies it into `x` — **two** stack objects.
Declaring first and assigning lands the inlined return straight in `x`'s slot —
**one**. Neither spelling changes a single instruction; only the frame moves, so
this is the cheapest knob there is when a frame is off by exactly the size of
one object.

```cpp
ExecFunction f = chooseExecFunction(cmd);   // return temp + copy
ExecFunction f; f = chooseExecFunction(cmd); // return lands in f
```

Both directions have paid off in `Strategic/spcinterp.cpp`:
- `TSpcInterp::update` was 8 bytes too big holding an inlined 12-byte
  pointer-to-member. Declare-then-assign took it to 100%. (Binding a
  `const T&` instead is in between — it drops the copy but adds an address
  register: 96.2%.)
- `TSpcBinary::init` was 8 bytes too *small*. Spelling the inlined `calcKey`
  result as a local — `u32 hash = calcKey(...); symbol->mNameHash = hash;`
  instead of assigning the call directly — added exactly the two missing
  objects and took it to 100%, with `calcAndStoreKeys` still size-exact
  against the map.

So: frame one object too big, look for an initialisation to split; one object
too small, look for a call result that should have been named.

## Reading a value into a local removes a bound temporary

`CInline_SetupArgsExpression` binds an argument to a compiler temporary whenever
the expression is unsafe to repeat — and a call is always unsafe. A read of an
unmodified local is safe, so it gets substituted instead and **no temporary is
created**. When a frame holds one 4-byte temporary too many, moving the argument
into a local is often the whole fix:

```cpp
interp->push((int)interp->pop().typeof());   // binds a temp for the argument
u32 type = interp->pop().typeof();           // no temp; `type` stays in a register
interp->push((int)type);
```

Concrete case (`Strategic/spcinterp.cpp` `spcTypeof`): 96.3% to 100%.

Beware the mirror image: this only helps when the local itself does not need a
slot. If the enclosing function already has a settled set of named locals, the
new local lands in the local region and shifts every offset above it, which
costs more than the temporary saved. The same change in `execadd` dropped it
from 100.0% to 96.6% for exactly that reason.

## Each inline level in a call chain leaves one dead 4-byte temporary

An expression such as `gpFoo->getBar()->baz()` builds one compiler temporary per
inline level that it goes through, and each temporary keeps a stack slot even
when the optimiser removes every instruction that touched it.
The instruction stream is then identical to the ROM's and only the stack offsets
disagree — the classic "our frame is 8 bytes short" symptom.

Two levers add a level, and both are ordinary source that a person would write:

- **Go through the global's inline accessor.**
  `SMSGetMarDirector()->getConsole()` instead of `gpMarDirector->getConsole()`
  adds exactly one temporary. This only works when the *next* member call is
  itself inline; if the accessor result feeds an out-of-line `bl` at once, MWCC
  reads the global in place and no temporary appears.
- **Hold the chain result in a named local.**
  `TGCConsole2* console = SMSGetMarDirector()->getConsole(); console->foo();`
  adds one more temporary than the same expression written in one line, because
  the local is used as the receiver of the following call.

Concrete case (`System/EventWatcher.cpp` `evManiCoinDown`): the bare
`gpMarDirector->getConsole()->startAppearStar()` gave 2 temporaries and a 0x28
frame; the accessor gave 3 and 0x30 with the wrong layout; the accessor plus the
`console` local gave 4 and a 100% match.

Because the effect depends on what follows the accessor, do not convert a whole
file blindly — measure each site. In the same file `evSetNextStage` is
measurably *worse* with the accessor, which is real evidence that the original
used the bare global there.

## A by-value class parameter forces the copy through memory

When a class is passed by value, MWCC materialises the copy and reads the copy
back, even for a two-word POD:

```
lwz  r4, 0(r3)        ; source.mType stays in a register
lwz  r0, 4(r3)
stw  r0, 0x38(r1)     ; source.mData
stw  r4, 0x3c(r1)     ; copy.mType
lwz  r0, 0x38(r1)
stw  r0, 0x40(r1)     ; copy.mData
lwz  r0, 0x3c(r1)     ; the copy is read back from memory
```

Two stack objects, and the second field never gets forwarded through the
register. Seeing this shape in the target is a reliable sign that the value goes
into a by-value parameter — in `EventWatcher.cpp` that parameter is
`getNameRefPtr(TSpcSlice)`. A named local initialised from the same call gives
two objects as well, but forwards the first field through a register, so the two
shapes are easy to tell apart.

## Working with JSUMemoryInputStream

Practice shows that most of the time in game code `operator>>` overloads were used for reading from them, and sometimes they were chained together.
This fact was derived from stack frame padding issues in various `JDRama::TNameRef::load` overloads -- only by using `operator>>` and sometimes chaining them (e.g. `stream >> vec.x >> vec.y >> vec.z;`) does stack the stack frame size matches the original.
However, methods like `stream.readU16()` were also occasionally used, but codegen differs a bit when it is used and so judgement must be made on a case-by-case basis.
Doing raw `stream.read(&someInt, 4);` calls is the least likely option, because humans are lazy and wouldn't want to specify the size manually -- always prefer avoiding it.

## Forcing local arrays onto the stack with unrolled loops

Small local arrays (e.g. `f32 dist[3]`, `f32 m[3][2]`) as well as structures are prime candidates for MWCC's *scalar replacement of aggregates* — it promotes the elements into FP or GP registers, drops the stack storage entirely, and is then free to reorder/fuse the element computations.
If the target keeps such an array on the stack (you see `stfs`/`lfs` of consecutive slots, computed in a strict order), your straight-line, element-by-element source will usually mismatch because yours got scalarized.

Two ways to force the array back onto the stack:

1. `(void)&arr;` — taking the address marks it address-taken so it must live in
   memory. Works, but it's an obviously-fake artifact.
   Still useful as an intermediate step in figuring out the other details of the function to then replace with a proper match.
2. **Write the init/compute as a `for` loop that MWCC unrolls.** Iterating over
   the array with an index the compiler can fully unroll (small constant trip
   count) makes MWCC materialize the array on the stack and emit the element
   stores/loads in loop order — no address-of hack, and it reads like plausible
   original code. This also tends to fix the *ordering* of the stores, not just
   their presence.

```cpp
// scalarized into registers + reordered (mismatch), or needs (void)&:
f32 dist[3];
dist[0] = unk24[0]; dist[1] = unk24[1]; dist[2] = unk24[2];

// unrolled loop: array is spilled to the stack in order (match):
f32 dist[3];
for (int i = 0; i < 3; ++i)
    dist[i] = unk24[i];
```

Concrete case (`DrawUtil.cpp` `TSilhouette::loadAfter`, a Cramer's-rule solve
over `f32 atten[3]`/`dist[3]`/`m[3][2]`): element-by-element assignment needed
three `(void)&` hacks and still only reached ~92% (residual store scheduling);
rewriting the `dist` copy and the `m` fill as small unrolled `for` loops removed
the hacks and took it to 99.7%. Prefer the loop form — it's both cleaner and a
better structural match than address-of forcing.

## Triviality of a type influences codegen

A user-declared destructor (even an empty `~T() {}`) makes a class non-trivial, and MWCC pins non-trivial types to memory instead of promoting them into registers.
Triviality propagates: a non-trivial member/base makes the enclosing class non-trivial too.

Two symptoms of a *missing* destructor, both meaning "the target keeps this in memory but our build cached it in a register":

- **Struct fields reload across calls in the target, but ours caches them.**
  A trivial struct is scalar-replaced (fields live in registers across calls); a non-trivial one is reloaded from memory after any opaque call.
- **The target spills a freshly-`new`'d object pointer to the stack and reloads it as `this`, but ours keeps it in a register.**
  For `new T()` with a non-trivial `T`, the constructed object is a separate address-taken temporary with a stack home, so `this` is reloaded from the stack during construction.
  Only visible when the constructor is inlined (out-of-line ctors call a real `bl` and never show it).

Fix: give the smallest offending value/helper type an empty `~T() {}` and re-check.
This is a global change, so re-run the baseline — one destructor can fix (or shift) many callsites at once.
Concrete case: adding `~TMsRange<f32>()` (a field of `TSmallEnemyParams`) took several `TFooManager::load` functions from ~95% to 100%.

## Shared inline header functions cost stack at every call site, in lockstep

A near-100%-fuzzy function whose diff is *only* a uniform stack-offset shift
(no wrong instructions, no reordering -- see "MWCC 1.2.5 stack padding bugs"
above) is often not a bug in that `.cpp` file at all. Check whether the
function calls an **inline member function defined in a shared header**
(`Camera.hpp`, `TimeRec.hpp`, etc.). If so, the same phantom-slot offset will
recur, unchanged, in every other `.cpp` that calls the same inline -- because
the inline body is re-elaborated at each call site, not shared code.

Confirmed this session: `TTimeRec::startTimer()`/`endTimer()` (inline in
`include/System/TimeRec.hpp`) produce the identical +16-byte frame excess in
both `TLiveManager::perform` (`src/Strategic/livemanager.cpp`) and
`TObjManager::perform` (`src/Strategic/objmanager.cpp`) -- two unrelated
classes, same delta, same header. `CPolarSubCamera::changeCamMode_()` (inline
in `include/Camera/Camera.hpp`) shows the same pattern in
`makeMtxForPrevTalk` (`src/Camera/CameraTalk.cpp`).

This means: **fixing the header fixes every call site at once** (as the
destructor tip above also found for a different mechanism), but it also means
a wrong or speculative header edit regresses every call site at once. A
session that finds this pattern should not patch one `.cpp` in isolation --
diagnose and fix the header's own locals, then rebuild the *whole* report to
confirm every affected unit moved together. Not yet solved for either header
above.

**Dead end already ruled out for `TimeRec.hpp`, so don't retry it:** the
`NpcThrow.cpp` fix removed one named local that was never spilled to memory.
`startTimer(u8,u8,u8,u8)`/`endTimer()` looked like the same shape (`TTimeRec*
inst`, `u32 col`, both register-resident, never stored), but inlining either
one away individually -- `inst` alone, or `col` alone, each tried in
isolation with everything else byte-for-byte unchanged -- made
`TLiveManager::perform` measurably *worse* (99.8% to 97.1%, then to 78.5%),
not better. Both locals are load-bearing for the current near-match; the
16-byte excess comes from something else. Whoever picks this up next should
look at the *caller's* frame or at alignment of `TTimeArray`/`OSTick` across
the two adjacent inlined blocks, not at trimming these two locals further.

## A commented-out-looking `(void)x;` marker is not always dead code

Two independent cases this session: `src/Map/PollutionObj.cpp`'s
`getDepthFromMap` had `// TODO: inlines are wrong here!` followed by
`(void)0;`, and `src/GC2D/MessageLoader.cpp`'s constructor had
`// NOTE: assert but in an if?` followed by `if (unk4) (void)unk4;`. Both
read like leftover debris from a previous reconstruction attempt -- a
discard-expression that should compile to nothing.

Removing either one made the match measurably **worse**, not better or
neutral: `PollutionObj.cpp` regressed a sibling function
(`updateDepthMap`, unrelated, matching before) to 0%, and
`MessageLoader.cpp`'s constructor itself dropped from 99.8% to 92.0%. In
both cases the object's `.text` layout past that point depends on the
statement being there, likely because it changes MWCC's optimizer/scheduler
decisions even though it discards a value. Do not "clean up" one of these
without rebuilding the whole object (not just the target function) and
checking every function in it, not only the one the comment sits in.

Neither TODO comment has actually been resolved by this observation --
they're still marking a real, unexplained gap. The finding is narrower:
whatever is wrong, deleting the marker is not the fix.

---

## MWCC bit-test and boolean idioms (verified 2026-09-30)

These four are repo-wide traps. Each was found by an agent and confirmed
against the target assembly in `Enemy/popo`.

### 1. `rlwinm` MB field: bit *n* is `MB = 31 - n`

For a bit test `x & (1 << n)` MWCC emits
`rlwinm rD, rS, 0, 31-n, 31-n`. So the mask fields, not a bit index, are
what you read. Getting this wrong makes a function silently test the
**wrong live/hit flag** while still scoring in the high 90s.

    0x00000080 (bit 7)  ->  rlwinm rD, rS, 0, 24, 24
    0x00000002 (bit 1)  ->  rlwinm rD, rS, 0, 30, 30
    0x00000004 (bit 2)  ->  rlwinm rD, rS, 0, 29, 29
    0x00000007 (bits 0-2)   ->  rlwinm rD, rS, 0, 31, 29
    0x00070000 (bits 16-18) ->  rlwinm rD, rS, 0, 15, 13

More generally `rlwinm rD, rS, 0, MB, ME` tests the range of bits
`[MB..ME]`, i.e. source bits `31-MB .. 31-ME`, giving the mask
`((1 << (31-MB+1)) - 1) << (31-ME)`.

### 2. `xoris` XORs into the HIGH halfword -- and `0x8000` is ambiguous

`xoris rD, rS, imm` is `rD = rS ^ (imm << 16)`, not `rS ^ imm`.
So `xoris r0, r3, 0x8000` is `r0 = r3 ^ 0x80000000`.

**Do not read `xoris rD, rS, 0x8000` as a negation.** It has two distinct
causes in this codebase, and picking the wrong one costs you the function:

- MWCC's **int-to-double idiom**: `stw rX, off(r1); stw r6, off-4(r1)` where
  `r6 = 0x43300000` builds the double `2^52 + X`; the `xoris` supplies the
  signed `X`. Look for the surrounding `lfd`.
- MWCC's **`(int) -> (f32)` cast**: the conversion itself emits
  `xoris` before the float move. So a stray `xoris` with no `lfd` nearby is
  usually just a cast, and adding an explicit `-` or `(f32)(-x)` to "explain"
  it produces a spurious `fneg` and drops the function ~50 points. This was
  measured in `Enemy/wireTrap` (`doSearchMove`): `mWireLength = -(f32)dir`
  is wrong, `mWireLength = (f32)dir` is right.

In both cases the source has no negation; only the *other* readings do.

### 3. `? true : false` vs a genuine `bool`

- `if (intExpr ? true : false)` emits `li 1 / b / li 0 / clrlwi. rD, rS, 24`.
- A real `bool` expression (e.g. a call returning `bool`) emits `cmpwi rD, 0`.
- `if (!boolExpr)` also produces the `clrlwi` form. Writing the branch
  the other way round (true-block first, no `!`) produces the target's
  `cmpwi`.

So the presence of `clrlwi.` after `li 0/li 1` tells you the source used
`!` or an explicit `? true : false`, not that it tested a bool.

### 4. `TVec3<f32> a = b;` is a word copy

It compiles to plain `lwz`/`stw` pairs, three words, no FP at all.
That explains the Vec-looking register shuffles throughout a TU.

### 5. `clrlwi` masks the LOW bits, `clrrwi` masks the HIGH bits

The two mnemonics are mirror images and reading them the intuitive way round
sends you to the wrong flag. `MB`/`ME` in `rlwinm` use PowerPC's MSB-first bit
numbering, so:

| mnemonic | encoding | keeps numeric bits |
|---|---|---|
| `clrlwi rA,rS,n` | `rlwinm rA,rS,0, MB=n,    ME=31`    | `0 .. 31-n` (the **low** `n+1`) |
| `clrrwi rA,rS,n` | `rlwinm rA,rS,0, MB=0,    ME=31-n`  | `n .. 31`   (the **high** `32-n`) |

Confirmed by assembling both by hand with `build/binutils/powerpc-eabi-as.exe`:

    rlwinm. r0, r4, 0,  0, 31   ->  54 80 00 3f   (rotlwi. r0,r4,0)      # bit 31
    rlwinm. r0, r4, 0, 31, 31   ->  54 80 07 ff   (clrlwi. r0,r4,31)     # bit 0
    rlwinm. r0, r0, 0,  0,  7   ->  54 00 00 0f   (clrrwi. r0,r0,24)     # low byte
    rlwinm. r0, r0, 0,  0, 30   ->  54 00 00 3d   (clrrwi. r0,r0,1)      # bit 31

So a `clrlwi. r0, rX, 31` is `x & 1`, **not** `x & 0x80000000`. In
`mario/Enemy/amiNoko` an earlier session read `clrlwi. r0, r4, 31` on the
`cue` argument as a high flag and wrote `CUE_UNK80000000`; the correct source
is `if (cue & CUE_MOVE)`, and the same applies to
`if (!(mParent->mLiveFlag & LIVE_FLAG_DEAD))` (`LIVE_FLAG_DEAD == 0x1`).
The mnemonic says "low" and it means low. The two `clrlwi.`s in
`TAmiHit::perform` are the only difference between 84.0 % and 88.6 %.

### 6. `isActorType()` inside an `if` whose body is a virtual call

`if (ptr->isActorType(FLAG)) ptr->virtualCall();` makes MWCC materialise the
result in a register:

    addis r0,r3,0x8000 ; cmplwi r0,1 ; bne  +4
    li r0,1 ; b +1 ; li r0,0 ; clrlwi. r0,r0,24 ; beq ...

i.e. four instructions the target does not have, because the target branches
straight from `cmplwi` into the `blrl`. Comparing the member directly -
`if (mCollisions[i]->mActorType == 0x80000001)` - keeps the branch and is worth
**84.0 % -> 88.5 %** on `TAmiHit::perform` on its own. The same rewrite is
harmless where the `if` body is not a call.


## MEASURED 2026-09-30: do NOT make `TUtil<f32>::sqrt`/`inv_sqrt` out of line

The obvious root-cause fix is wrong, and it was measured, not guessed. Declaring
both out of line in `libs/JSystem/.../JGUtil.hpp` (so call sites emit `bl` to the
weak `Animal.a boid.cpp` copies), plus a `*_inline` twin for the sites the ROM
expands, moves **50 units and sums to -32.2 points**:

```
  better:  TabePuku +2.01  MapObjBall +1.49  fishoid +1.12  MSoundSE +0.52
           MapObjPinna +0.45  EventWatcher +0.39  Item +0.25
  worse:   boid -3.28  rocket -2.31  JPAField -2.28  JPAMath -2.23
           BathWaterManager -2.03  Tongue -1.95  coasterkiller -1.93
           popo -1.77  fruitsboat -1.63  ... ~40 units down
```

The prize is real - 148 functions in 69 TUs (~55 KB of `.text`, ~3.4 % of the
game) contain a ROM `bl sqrt/inv_sqrt` - but it is the **minority** behaviour.
The inline definition in `JGUtil.hpp` is correct at the overwhelming majority of
call sites. Only 7 TUs emit no call at all where the ROM does
(`Enemy/BathtubBinder`, `MoveBG/MapObj{Ball,Corona,Mamma,Monte,Pinna}`,
`System/EventWatcher`) - those are the only ones worth a per-site effort.

`artifacts/find_sqrt_targets.py` re-derives the list: it walks every
`build/GMSP01/asm/**/*.s`, attributes each `bl` to its enclosing `.fn`, and
cross-references the unit's current nonmatching report.

```
python artifacts/find_sqrt_targets.py
```

## MEASURED 2026-09-30: forcing `TUtil<f32>::sqrt` / `inv_sqrt` out of line

`libs/JSystem/include/JSystem/JGeometry/JGUtil.hpp` defines `sqrt()` and
`inv_sqrt()` inline, but the ROM calls weak copies that live in
`Animal.a boid.cpp` (`sqrt__Q29JGeometry8TUtil<f>Ff`,
`inv_sqrt__Q29JGeometry8TUtil<f>Ff`; see the `UNREFERENCED DUPLICATE` entries
in `marioEU.MAP`). JSystem is off-limits, so an inline expansion
(`frsqrte` + Newton step + the `mag <= 0` guard, ~12 instructions) is emitted
wherever the ROM emits a one-instruction `bl`.

**The ROM is not consistent**, and that is the whole difficulty. In
`mario/Enemy/amiNoko`:

| function | ROM |
|---|---|
| `TNerveAmiNokoWalkOnFence::execute` | `bl sqrt`, `bl inv_sqrt` |
| `TAmiHit::perform`, `TAmiNoko::calcDirection` | `bl inv_sqrt` |
| `TNerveAmiNokoDie::execute` | **inlines** `sqrt` (`frsqrte` on site) |

A TU-local wrapper under `#pragma dont_inline on` reproduces the call shape
(the diff then shows `bl <wrapper>` instead of the missing `bl <TUtil>` symbol -
still one mismatched instruction, but the frame, the register allocation and
the ~24 extra instructions all line up):

```cpp
#pragma dont_inline on
static f32 orig_sqrt(f32 v) { return JGeometry::TUtil<f32>::sqrt(v); }
static f32 orig_inv_sqrt(f32 v) { return JGeometry::TUtil<f32>::inv_sqrt(v); }
#pragma dont_inline off
```

Measured, applying it everywhere except `Die` (which regressed 96.7 -> 92.8 %
and had to be reverted to the plain `TUtil<f32>::sqrt` call):

- `TNerveAmiNokoWalkOnFence::execute` 72.4 % -> **81.1 %**
- `TNerveAmiNokoTurn::execute` 80.3 % -> **83.7 %**
- `TAmiHit::perform` 84.0 % -> **88.6 %** (with the `isActorType` rewrite on
  top: 97.4 %)
- `TAmiNoko::calcDirection` 76.3 % -> **78.6 %**

Declare the wrappers at the **top of the source file**: with
`reverse_fn_order: True` that is the end of `.text`, so nothing shifts.

**`TVec3::setLength()` must be spelled out to get the out-of-line call** -
it is a `JGVec3.hpp` inline that inlines `inv_sqrt` in turn. Useful detail
from `calcDirection`: the ROM computes `squared()` **once** (MWCC CSEs the copy
inside `setLength`) but keeps **both** epsilon tests - `isZero()`'s and
`setLength`'s guard. So the honest source is two tests on one local, not one:

```cpp
f32 lsq = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
if (lsq <= TUtil<f32>::epsilon())      dir.set(1.0f, 0.0f, 0.0f);
else if (lsq <= TUtil<f32>::epsilon()) dir.zero();
else                                   dir.scale(1.0f * orig_inv_sqrt(lsq), dir);
```

## MEASURED 2026-09-30: the out-of-line wrapper wins wherever the ROM calls it

The root-cause fix is negative (previous section), but the TU-local wrapper is
positive at every site where the ROM does emit the call. Applied to all 7 TUs
that had **no** call where the ROM has one: 15 call sites retargeted, 12
functions improved, **0 regressions**. Regression-freedom was checked by dumping
the fuzzy % of *every* function in the whole project before and after
(`artifacts/dump_nm.py`, 2195 entries) and diffing the two lists - a per-unit
`decomp-diff` is not enough, because one TU's new `static` shifts that unit's
`.text`.

| function | before | after |
|---|---|---|
| `TMapObjBall::hold` | 41.4 % | **96.8 %** |
| `TLeanMirror::release` | 60.5 % | **77.8 %** |
| `TBathtubBinder::float_` | 68.4 % | **83.3 %** |
| `TLeanMirror::loadAfter` | 79.8 % | **96.3 %** |
| `TPinnaCoaster::control` | 80.2 % | **94.8 %** |
| `TMapObjBall::touchGround` | 77.5 % | **85.2 %** |
| `evIsNearActors` | 87.0 % | **95.7 %** |
| `TBathtub::hipdrop` | 84.9 % | **94.9 %** |
| `evIsNearSameActors` | 93.3 % | **99.8 %** |
| `TResetFruit::hold` | 52.1 % | **63.0 %** |
| `TBathtub::quake` | 51.9 % | **60.3 %** |
| `TBathtub::updatePosture_` | 54.3 % | **55.4 %** |

### `TVec3::length()` is an inline `sqrt` and has to be spelled out

Two of those sites never spell the call at all - they read
`if (diff.length() <= dist)` and `if (mVelocity.length() <= 10.0f)`.
`length()` is `TUtil<f32>::sqrt(squared())` in `JGVec3.hpp`, so MWCC expands it
and the `bl` never appears; grepping the source for `sqrt` finds nothing and
the unit looks like it has no call site. Rewriting the site as
`orig_sqrt(diff.squared())` recovers the call and is worth +8.7 / +10.9 points.
`TVec3::distance()` is the same trap. Grep for `.length(` / `.distance(` before
concluding that a unit has nothing to retarget.

### The ROM call is not always `inv_sqrt`

`sqrt`, not `inv_sqrt`, in `TBathtubBinder::float_`, `TMapObjBall::{hold,
touchGround}`, `TResetFruit::hold`, `TLeanMirror::release`,
`TPinnaCoaster::control` and both `EventWatcher` helpers. `inv_sqrt` in
`TBathtub::{hipdrop,quake,updatePosture_,calcBathtubData}`,
`TLeanMirror::loadAfter` and `TMapObjBall::{touchWall,calcCurrentMtx}`. Check
the asm per site; `frsqrte` in the ROM asm is the *inline* expansion and means
the opposite of a call.

### Tooling

```
python artifacts/apply_sqrt_wrapper.py FILE sqrt|inv_sqrt|both [--sub OLD NEW] LINE...
```

inserts the wrapper after the last leading `#include` - required, because with
`reverse_fn_order: True` that is the end of `.text` and nothing shifts - and
retargets the given 1-based lines. It re-derives the file's dominant line ending
and writes it back in that style, because these files are a mix: `MapObjMamma`
and `MapObjBall` are pure LF, `MapObjPinna`, `MapObjCorona` and `EventWatcher`
are pure CRLF. Re-running with new line numbers only adds call sites.

`MoveBG/MapObjMonte` is on the eligible list but has no site to retarget:
`TFluffManager::control` and `THangingBridge::loadAfter` are still empty `{ }`
stubs, so a wrapper there would be dead code.

## BLOCKED 2026-09-30: `TAmiNoko::init` needs a vtable reslot, not a rewrite

`TAmiNoko::init` sits at 81.7 % with 40 mismatched lines out of 151, and the 40
collapse into three causes, none of them fixable inside the .cpp:

1. **The virtual slot is off by five.** The object emits
   `lwz r12, 0x150(r12)` where the ROM has `lwz r12, 0x170(r12)`, i.e. a
   `0x20` = 5-entry shift in `TAmiNoko`'s vtable. Fixing it means reordering
   `TSmallEnemy` / `TWalkerEnemy` virtuals in headers shared by dozens of TUs.
2. **One local too many.** `stwu r1, -0x98(r1)` against the ROM's
   `-0x88(r1)`. That extra 4-byte local shifts every stack slot by `0x10` and
   costs the ROM's `r28` (we keep `r29` instead) - which is what most of the 40
   lines actually are.
3. **The hit list is built from a node.** The ROM does
   `TList<...>::iterator::iterator(TNode*)` then a copy-construct; we do
   `TList<...>::end()` then
   `TList_pointer<THitActor*>::iterator::iterator(TList<...>::iterator)`.
   `THitActor::initHitActor` itself matches - only the iterator differs.

Cause 2 is plausibly a consequence of cause 3: building the iterator from a
node needs one more live value. Left alone deliberately.

## MEASURED 2026-09-30: scoring the sqrt sweep by *deficit*, not by call count

148 functions in 69 units contain a ROM `bl sqrt`/`bl inv_sqrt`. Counting them
says nothing about how much is actually recoverable, because most are already at
95-100 %. Two filters cut that to the real work:

1. **our match % is low** - a function at 0 % is unimplemented and a wrapper
   gains nothing;
2. **our object emits FEWER out-of-line calls than the original** - the deficit.

`artifacts/sqrt_sites.py` does both in ~2 s. It reads `build/GMSP01/asm/**/*.s`
and `build/GMSP01/report.json` only; the older
`artifacts/find_sqrt_targets.py` shells out to `decomp-diff.py` per unit and
takes minutes.

```
python artifacts/sqrt_sites.py --rank --calls --below 99
```

`--calls` runs `objdump -r` per unit and counts `R_PPC_REL24` relocations to
either the real weak symbol **or** `orig_sqrt`/`orig_inv_sqrt`. Counting only
`TUtil<f>Ff` is wrong: a TU already patched with the wrapper then still reports
a deficit and looks untouched. (Caught this the hard way - the first version of
the tool listed all six patched units as unpatched.)

### What the wrapper is *not* for

Two negative results, both measured:

- **`Enemy/TabePuku` is a no-op.** `swimTo` is the biggest single prize in the
  whole sweep (2068 B at 41.5 %, 4 out-of-line `inv_sqrt` in the original) and
  wrapping all four gains nothing. The function is structurally wrong before it
  ever reaches the call: `stwu r1, -0xd8(r1)` against the original's
  `-0x1f8(r1)`, i.e. **0x120 bytes of missing locals**, which shifts every
  stack slot. The other two sites (`calcYawFromVeloc`, inlined into
  `TNerveTabePukuDrag::execute` and `TNerveTabePukuAttack::execute`) are
  *already* emitted out of line by our object, so the wrapper only renames the
  relocation - byte-identical result either way. The lever there is the frame,
  not the call.
- **`TMapObjBall::calcCurrentMtx` has no site at all.** The original calls
  `inv_sqrt` once; our source has no square root expression in the function
  because the body is not written yet.

So: check the frame before blaming a missing call. A function whose stack
layout is already wrong will not be rescued by getting one instruction right.

### `normalize()` is `setLength()` and has the same trap

`TKoopa::setUpHitActors` calls `dir.normalize()` and the original calls
`inv_sqrt` there. `normalize()` is `setLength(*this, one())`, an inline that
inlines `inv_sqrt`, so the call never appears - the source contains no
`sqrt`, `inv_sqrt`, `squared` or `length` token at all. Same for `setLength()`
itself (see the earlier section). `grep` for `sqrt` is not enough: grep for
`.normalize(`, `.setLength(` and `.length(` too.

Do **not** guess the guard shape from the inline header. In `setUpHitActors` the
original's `lsq <= epsilon` branch copies one register to two others and skips
the scaling, which is neither `zero()` nor `set(1, 0, 0)`. Reconstructing it
needs the ground truth, not the header.

### Where it paid

| function | before | after |
|---|---|---|
| `TMapObjBall::touchWall` | 60.8 % | **80.0 %** |
| `TKoopaJrSubmarine::makeRoundVelocity` | 76.8 % | **84.2 %** |
| `TKoopaJrSubmarine::checkNerve` | 74.6 % | **81.3 %** |

`touchWall` is the `length()` trap again: three `mVelocity.length()` calls, three
`bl sqrt` in the original. `koopajr` was the largest remaining deficit (7 sites);
`makeKillerVelocity` holds 5 of them but is a stub at 0.2 %, so only the 3 in
`checkNerve`/`makeRoundVelocity` were reachable - and the two `sqrt` calls in
`makeRoundVelocity` stay inline because the original has **no** out-of-line
`sqrt` in that function (only two `inv_sqrt`).

## MEASURED 2026-09-30: the frame lever is spent - triage before you try

`AGENTS.md` forbids *committing* a `volatile char trash[0x10];` frame pad, and
explicitly encourages using one *temporarily* to check whether a function
matches modulo its stack frame. It does not say which functions are worth that
effort. Now there is a cheap way to ask.

`artifacts/frame_hole.py` compares, for one function, the original's and our
**hole** - the frame size minus the lowest live stack offset:

```
python artifacts/frame_hole.py Enemy/TabePuku 'swimTo__9TTabePukuFRCQ29JGeometry8TVec3<f>'
```

Three verdicts:

- `frames already match` - the pad would be zero bytes; the **body** is wrong.
- `holes match - a framePad of 0xN should close it` - this is the case
  `AGENTS.md` describes. Verified against `TLeanMirror::load`, which already
  carries a working `framePadLoad[0x30]`: holes 0x10 on both sides.
- `local sets genuinely differ` - **no pad helps**. The original declares
  locals we do not have, so a pad would shift every offset while leaving the
  body wrong. This is the same reasoning already written out by hand in
  `src/MoveBG/MapObjPlane.cpp`.

Two gotchas when writing the measurement:

- the link register lives at `0x4` above the new stack pointer, so its
  spill/restore must be excluded or every function reports a 0x1f4 hole;
- objdump prints offsets in **decimal** and the original's `.s` in **hex**, so
  the LR filter has to accept both.

Also note a `framePad` can do nothing at all: on `TTabePuku::swimTo` a
`char framePad_288_swimTo[288]` left `stwu r1, -216(r1)` completely unchanged -
MWCC dropped the dead array. Always re-read the frame before believing it.

### The verdict, over the 22 biggest remaining prizes

Running it across every function with a ROM `sqrt` site and >90 bytes wrong:

```
20 of 22  local sets genuinely differ, no framePad helps
 1 of 22  frames already match (the body is wrong)
 1 of 22  our frame is 0x70 LARGER than the original's
```

So the frame lever is essentially exhausted project-wide **for functions with
real source-level differences**. Effort spent chasing stack offsets there is
wasted; what is left needs real source reconstruction. Triage with this before
reaching for a pad.

**But see "A framePad declared LAST, not first" below: for the *narrow* family
whose diff is 100 % `~` with a constant offset delta, the pad still works, and
that family is NOT exhausted. This verdict above only covers functions that
also differ in instruction content.**

### `TTabePuku::swimTo` (2068 B, 41.5 %) - not a frame problem

The biggest single prize in the `sqrt` sweep, and holes 0x10c vs 0x28: the
original declares about 228 bytes more locals than we do, keeps `f27` which we
do not, and spills `target.x/y/z` to `0x1b4/0x1b8/0x1bc`, which is the signature
of a **local copy of the argument**. Its body is also wrong well past the frame
(251 of 676 lines differ). Reconstructing the local set is a real task, not a
mechanical fix.

### `TFireWanwanTailNode::perform` (496 B, 37.1 %) - the call site is in the wrong function

The original makes exactly two calls, `PSMTXCopy` and `MActor::perform`, and
computes the direction matrix **inline**: six FP callee-saved registers
(`f26`..`f31`) and six `fmsubs`/`fnmsubs` forming a cross product against a
constant vector. Our source calls `SMS_CalcToDirMatrix` here.

`SMS_CalcToDirMatrix` *is* called in this translation unit - but from
`TFireWanwanTailHit::perform`, not from `TailNode::perform`:

```
$ awk '/^\.fn /{fn=$2} /bl "SMS_CalcToDirMatrix/{print fn}' build/GMSP01/asm/Enemy/fireWanwan.s
perform__18TFireWanwanTailHitFUlPQ26JDrama9TGraphics
```

That single misplaced call explains the whole divergence: frame 0x120 vs our
0x90, the ROM picking `r27` and skipping `r30` where we pick `r28`..`r31`, and
37.1 %. Left with a TODO rather than a speculative rewrite, per `AGENTS.md`'s
"leave the code nonmatching and move on".

Worth remembering: **before reverse-engineering a bad function, check that its
call sites are in the right function.** A call in the wrong place makes every
symptom look like a codegen problem.

## A destructor defined in the .cpp is `global`; defined in-class it is `weak`

The map records `__dt__7TAmiHitFv` as **weak**. With `virtual ~TAmiHit();` in
the header and `TAmiHit::~TAmiHit() { }` in the .cpp MWCC emits it **global**,
which then fails `validate-symbol-order.py` twice over: the linkage check, and
the order check (the validator ignores weak symbols, so a global one is
compared against the wrong slot). Moving the empty body into the class -
`virtual ~TAmiHit() { }` - gives weak linkage, the right size (0x84), the
`@32@__dt__7TAmiHitFv` thunk for free, and both checks pass.

## A `theNerve()` singleton is alive even when the nerve is dead

`DEFINE_NERVE` emits `static Name instance;`. Even for a class nothing
references - the ROM lists all three of `execute`/`theNerve`/`__dt__` as UNUSED
and emits no vtable for it - the 12-byte `.bss` singleton is still there, and it
sits **before** the global-object table that `__sinit_<TU>_cpp` walks. In
`mario/Enemy/amiNoko` the missing `DEFINE_NERVE(TNerveAmiNokoAttack, ...)`
made every `addi r5, r31, 0x3c` in `__sinit_amiNoko_cpp` 0xC too low:
99.9 % -> 100 % from adding the class back. Cheapest possible win: compare the
count of 12-byte `.bss` objects against the map before suspecting codegen.

## When you cannot have both: inlined at every call site *and* an UNUSED copy

`TAmiNoko::creepToCurPathNode` is UNUSED 0x1EC in the map, yet the ROM expands
it inline in both `WalkOnFence` and `Turn`. With `inline` in the source no
standalone copy is emitted (symbol missing); drop `inline` and MWCC stops
inlining, calling it instead - `WalkOnFence` 81.1 % -> 59.8 %, `Turn`
83.7 % -> 64.9 %. Keep `inline` and accept the missing UNUSED symbol: it is
dead-stripped, so it costs nothing at link time and no matching score.
---

## Never round-trip a source file through PowerShell Get-Content / Set-Content

The `.cpp`/`.hpp` files here are **UTF-8** and contain Japanese string literals.
Two separate failures keep happening:

1. `Set-Content -Encoding UTF8` **writes a BOM**. `sjiswrap` then rejects the file
   with a misleading error that points at line 1:

       #       1: &#65279;#include <Enemy/popo.hpp>
       #   Error: ^  declaration syntax error

2. `Get-Content | Set-Content` under a Shift-JIS code page **lossily transcodes** the
   Japanese literals. Nothing reports it; the file still compiles, but every string
   offset in `.rodata` shifts and previously-matching functions drop.

Use the `edit` tool for all source edits.

If you already hit it: `python tools\fix-source-encoding.py` reports every BOM and every
non-UTF-8 file under `src/` and `include/`; add `--fix` to strip the BOMs (it will
refuse to touch a file that is not valid UTF-8, because that one needs a human to decide
what the bytes should be). A second agent on MapObjCorona recovered a corrupted file by
reversing a cp1252 map byte-by-byte, but that is a last resort.
---

## Do NOT derive a function's arity from its mangled name — and never from dtk's comment

**_<type> after a `PF` function-pointer parameter is the pointer's RETURN type,
not another parameter.** MWCC encodes a function-pointer parameter as
`PF<argument-types>_<return-type>`:

    registerSubframeCallback__Q28JASystem6KernelF  PF Pv _l Pv            -> 2 params
    setJointCallback__6MActorF                    i  PF P7J3DNodei _i  i   -> 2 params
    setPreUserCallback__12JUTExceptionF           PF Us P9OSContextUlUl _v -> 1 param
    fireStartDemoCamera__12TMarDirectorF ...      PF Ul Ul _l Ul  PQ26JDrama6TActor ... -> 9

dtk's demangler **silently drops that return type**, so a dtk `#` comment or a
count of the `_`-prefixed tokens both under- or over-report the arity. This exact
misreading has already happened once in this repo: `fireStartDemoCamera` was
declared as taking 11 parameters instead of 9, and "fixing" it would have renamed the
symbol, deleted a 100 % match, and broken all 39 call sites.

**Always confirm arity from the target callee's register/stack traffic, not the name.**
For `fireStartDemoCamera` the callee stores to `r4,r5,r6,f1,r7,r8,r9,r10` plus one
8-byte stack slot, and `TDemoInfo` has 9 fields (`mulli 0x24`) — 11 parameters
cannot fit. Every one of the 39 call sites in the ROM sets exactly that same set.

Two more dtk demangler traps in the same family:

- It omits the return type of non-void member and free functions (e.g.
  `TMarDirector::updateGameMode` demangles to `()`), and returns `long` for
  `int`. Never take a type from it.
- `Q` = class type, `CQ` = const class. So `PQ26JDrama6TActor` does **not** imply
  the parameter is `const`.

And one ABI fact worth knowing for wide calls: a by-value `JDrama::TFlagT<u16>` is
passed **by address**. The caller does `sth r0,off(r1); addi rX,r1,off; stw rX,8(r1)`
and the callee does `lwz r11,0x40(r1); lhz r5,0(r11)`. Relevant whenever a call passes
more than 7 integer/pointer arguments.
---

## PARKED, HIGH LEVERAGE: `TParamT<T>::get()` returns `const T&`

include/System/ParamInst.hpp:22 declares

    const T& get() const { return value; }

but the ROM loads the parameter **straight out of the params struct**:

    lfs  f5, 0xe0(r3)          <- the ROM
    mr   r4, r3 ; lfs f5, 0xe0(r4)   <- ours

Returning `T` by value is the suspected fix. It is believed to be the last diff in
`TKoopaJrSubmarine::resetKoopaJrSubmarine` and probably a large part of
`TKoopaJrSubmarine::init` and `TKoopaJrParams`/`TKoopaJrSubmarineParams` ctors.
Note include/Enemy/Enemy.hpp already carries a comment observing that the
no-address-materialisation form "is only true if the return type is u8".

**This has NOT been measured globally and must not be applied casually.**

- blast radius: **2084 call sites across 113 TUs**
- it changes codegen everywhere a param is read, including in functions that currently
  match 100 % - so it can just as easily regress the project as advance it
- it touches a header included nearly everywhere

Do the measurement first: pick ~8 representative TUs (one that is at 100 % and must stay
there, e.g. mario/Enemy/BossHanachanNerve; one heavy TParamRT user, e.g.
mario/MoveBG/MapObjBall; one boss, e.g. mario/Enemy/koopajr), flip only
get() to return `T`, rebuild just those three, and compare every function - not just
the ones you expect to improve. Adopt it only if the regressions are zero and the gains
are real. This wants a quiet tree, i.e. not while a large agent pool is writing.

## Other confirmed-but-unfixed libs gaps (for the record)

- `libs/.../MSL_Common/math.h`: `std::fmodf` is `inline float fmodf(f,f){return
  ::fmod(x,y);}` over the **double** `::fmod`, so we emit `lfd; bl fmod; frsp` where
  the ROM emits a call to an out-of-line `fmodf__3stdFff`. It also explains why the
  ROM's `x - 0.0f` / `0.0f + x` around those calls survive unfolded while ours fold
  away. Affects every `std::fmodf` site in the tree.
- `libs/.../JGMatrix34.hpp`: the ROM emits a weak 12-argument
  `SMatrix34C<f>::set`; it is missing here, which blocks every `calcRootMatrix`.
- `libs/.../JGUtil.hpp`: `TUtil<f32>` has no `mod`; the ROM emits
  `mod__Q29JGeometry8TUtil<f>Fff`.
- A 24-byte unidentified blob at `.rodata+0xE0` (12 zero bytes then three `1.0f`)
  exists in **every** retail TU that carries `MtxCalcTypeName`, so every string offset
  in those TUs is shifted until it is identified. Seen in
  `MarNameRefGen_Enemy`, `koopajr`, `MarNameRefGen_MapObj`.
---

## `#pragma dont_inline` in MWCC 1.2.5: what actually works

Measured in mario/MoveBG/MapObjMare against `getObjCollisionHeightOffset`, whose ROM
body is a one-instruction `lfs f1, 0x108(r3); blr` reached through three real `bl`s
(hence `scope:weak` in the map - a header inline MWCC declined to expand because its
inlining budget was already spent by the caller's inlined blocks).

| spelling | out-of-line copy emitted? | linkage |
|---|---|---|
| `#pragma dont_inline` inside a class body | **no** | - |
| out-of-class `inline` definition in the header + pragma | **no** (still expanded) | - |
| non-`inline` out-of-class definition in a `.cpp` + pragma | **yes** | `global` |

So there is **no spelling that produces a weak out-of-line copy** with this compiler. If
the map says `weak` and the body must stay out of line, you have to accept a
`BINDING` warning from `validate-symbol-order.py`.

That is the general explanation for the `BINDING` warnings several agents have hit
(`TKoopa::changeAnm` in `include/Enemy/Koopa.hpp:206` is the same phenomenon). It is
a **warning, not a build failure**, and a `global` symbol in an object that is not
linked can never collide - so prefer the out-of-line `.cpp` definition, which is worth
far more matching bytes than the clean linkage. Measured on that one function: 81.5 % with
the out-of-line definition vs 72.2 % with the header inline.

Rule of thumb: **matching bytes first, linkage second.** Report the BINDING warning
rather than paying ~9 points of match for it.
---

## CORRECTION: the "one dead temporary per inline level" story is WRONG

`AGENTS.md` records an observation of the form *"each inline level in a call chain
leaves one dead 4-byte temporary in the caller's frame"*. **Measured and disproved**
(2026-09-30, seven frame-only functions in `mario/Map/MapCheck`):

| function | frame delta | inline levels L | delta/4 | D/L |
|---|---|---|---|---|
| `checkRoofList`   | -56 | 1  | -14 | 14.00 |
| `checkGroundList` | -64 | 3  | -16 | 5.33 |
| `checkGround`     | -16 | 4  | -4  | 1.00 |
| `checkRoof`       | -16 | 6  | -4  | 0.67 |
| `checkWalls`      | -64 | 6  | -16 | 2.67 |
| `bgIntersectLine` | -96 | 14 | -24 | 1.71 |
| `checkWallList`   | -224 | 37 | -56 | 1.51 |

Pearson(L, D) = **+0.935** - strongly suggestive - but the prediction D = L fails with a
**20x spread** (0.67 to 14.00). The correlation is a confound: inline depth tracks
function size, and function size tracks how much frame the compiler reserved.

**What the cost actually is.** Direct experiment: adding one extra inline level that MWCC
can *fold* (a pure forwarder, or one taking an unused index) moved **nothing**. Adding a
named local inside that level, at 2 call sites, moved two functions by +32 B total =
**16 bytes per call site, not 4, and not per level**. So:

- a foldable inline level costs **0**
- a level that forces a named local / non-repeatable temporary costs **~16 bytes per
  call site**

That is the existing "Reference Locals Affect Register Allocation" / "reading a value
into a local removes a bound temporary" mechanism, **not** a per-level tax.

**Two practical consequences:**

1. Do not go looking in a shared header for a frame-only fix. In that experiment **3 of
   the 7 functions never call the header at all** (they read `mNext` directly), so no
   edit there could ever reach them. Check the call sites first.
2. When a function is frame-only-short by exactly **+16**, the fix is usually to name one
   more local at the call site - that was the one real lead the experiment produced.

A negative result like this is worth more than a plausible story: several agents have
independently blamed "the MWCC stack-padding bug" without evidence, and this closes that
avenue with numbers.
---

## Reading arity out of a mangled name: BOTH failure modes

This section now warns about two opposite mistakes, and both have actually been made in
this repo. Each cost real time. **Prefer the callee's register traffic whenever you can
get it; use the name only as a cross-check.**

### (a) A `_<type>` after `PF` is a RETURN type, not a parameter

See the "Do NOT derive a function's arity from its mangled name" section above.
`fireStartDemoCamera` looked like 11 parameters and was 9.

### (b) The digits in a class name are a LENGTH PREFIX, not parameters

MWCC emits the class name length-prefixed: `11TMapObjWave` is 11 characters, then `C`
for the const-method qualifier, then one `f` per `f32`. So:

    getStaticTexPos0__11TMapObjWave C F f        -> 1 float
    getWaveHeight__11TMapObjWave C F f f        -> 2 floats   (map size 0x8C)
    getHeight__11TMapObjWave     C F f f f     -> 3 floats   (map size 0x12C)

Reading `CFff` as "const + 3 floats" is wrong - the leading `1` of `11` was counted
as a third `f`. That misreading has twice been reported as "the header declares the
wrong arity" and sent an agent to change ~8 call sites across 6 TUs for nothing. The
header was already correct and already matched **100 %**.

Cross-checks that settle it in seconds:

- the map's own sibling entries - three symbols of the same class, differing only in the
  number of trailing `f`, with sizes that scale 0x0C / 0x8C / 0x12C;
- the **callee's FP register traffic** - a 2-arg version touches only `f1`/`f2`; a
  declared-but-unused third float would still be materialised into `f3` somewhere,
  because MWCC does not elide declared float arguments;
- the **callers** - none of the nine `bl getWaveHeight` sites in the ROM sets `f3`;
- an **inlining caller** that forwards its own arguments - if `getHeight` inlines
  `getWaveHeight` and supplies only two values, the callee takes two.

So the rule in one line: after the length-prefixed class name, `C` is the const
qualifier and each following `f` is one `f32` - count only those.
---

## WHY a JGeometry inline is sometimes a `bl` in the ROM and inlined for us

Investigated 2026-09-30 across all 58 TUs that emit
`set<f>__Q29JGeometry8TVec3<f>Ffff`.

**The measured rule (MWCC GC/1.2.5 deferred inliner, isolated reproduction):** a deferred
inline call is expanded only while it sits at a low expansion pass, and the pass at which
it is refused depends on the callee's **float argument count**. Let `d` be the number of
nested inline expansions between the enclosing *emitted* function and the call
(`v -> q0 -> q1 -> drive -> v.set(...)` is `d = 2`).

| callee float arity | d=0 | d=1 | d=2 | d=3 | d=4 |
|---|---|---|---|---|---|
| 1-2 | inlined | inlined | inlined | **bl** | bl |
| **3 (`set<f32,f32,f32>`)** | inlined | inlined | **bl** | bl | bl |
| 4-5 | inlined | inlined | **bl** | bl | bl |

It reproduces in a 6-line TU with only `dolphin/mtx.h` + a minimal `JGVec3<f32>`.

**Read this as a PREDICTOR, not a fix.** The rule is proven in isolation but does not
explain a real TU by itself: `GC2D/SelectShine2::perform` has a *depth-0* `set` that
the ROM still calls out of line, and 7 of the 19 deficit TUs have **no `set` in the
source at all**. So there is a second, TU-level phenomenon on top (trivial `static`
helpers are not expanded at all in a real game TU, though they are in a probe).

**Do NOT edit `libs/.../JGVec3.hpp` to raise the threshold.** 300+ TUs rely on the ROM
inlining `set` at every site; raising it breaks them, lowering it breaks the 39 TUs that
currently match. And the mangled name pins the declaration as a member function
*template*, so it cannot be rewritten as a plain `set(f32,f32,f32)`.

**What to actually do when a `set` should be a `bl`:** look for a **missing inline
layer in the original**, not a header change. Grep `marioEU.MAP` for UNUSED symbols in
that TU - an unreconstructed UNUSED function the call site should have gone through is
the most likely explanation, and reconstructing it fixes the call shape *and* the
surrounding codegen. For `n`-float callees, expect the `bl` at depth >= 3 (n<=2) or
>= 2 (n>=3); apply the table to `TVec3::scale`, `TRotation3::setSQ`,
`TQuat4::setRotate`, `identity33`.

**Also: strip `dont_inline` fakematches aimed at JGeometry inlines.** They are worse
than useless - they emit symbols the ROM does not have *and* suppress inlining the ROM
does perform, so the problem runs in both directions in the same file. Known instance at
`src/Enemy/killer.cpp:242` (`MsVec3SetKiller`); there are roughly 40 more
`MsXxx`-style wrappers built the same way. Search for them with:
`Select-String -Path src\*\*.cpp -Pattern "#pragma dont_inline"` and check each against
the map's symbol list for that TU.

### MEASURED 2026-09-30: the depth lever only works on *large* callees

The "add one inline level" trick above is real, but it is size-gated, and the gate is
callee size, not arity. In `Camera/sunmodel` the same call site flips depending on the
callee:

| callee | size | direct call in an emitted fn | one extra inline level |
|---|---|---|---|
| `CLBScreenFPosToSPos` (cameralib) | 276 B | inlined | **`bl` + weak out-of-line copy** |
| `TVec3<f32>::set(const Vec&)` | 28 B | inlined | still inlined |
| `TVec3<f32>::set(const Vec&)` | 28 B | at d=2, d=3 | still inlined |
| `JMASSin` / `JMASCos` (JMath) | 28 B | at d=2, d=3 | still inlined |

So a trivial forwarding `inline` is only a usable lever for a callee big enough to
already be near the expansion limit. For 7-instruction callees the chain gets *folded*
(the "pure forwarder" case of the CORRECTION section above), and adding unrelated
inlined code earlier in the function does not change the decision either - both were
tried on `TLensFlare::perform` and `TSunModel::perform` and moved nothing. For those,
the out-of-line `set`/`JMASSin`/`JMASCos` need a `libs/JSystem` change; game-side source
cannot reach it. See the TODOs in `src/Camera/lensflare.cpp` and `src/Camera/sunmodel.cpp`.

### A local `extern` re-declaration with the wrong signedness changes the codegen

`CLBScreenFPosToSPos` re-declares `SMSGetGameRenderWidth/Height` locally (it cannot
include `System/Resolution.hpp`). Those local declarations said `s16`; the real
prototypes in `include/System/Resolution.hpp` are `u16`. The ROM emits
`clrlwi r3, r3, 16` (zero-extend) and a wrong `s16` prototype gives `extsh r3, r3`.
Flipping the local declarations to `u16` was worth +1.8 points on that function. **Check
the signedness of any function-scope `extern` re-declaration against the real header.**

### A by-reference argument is what produces `lfsu`/`addi` + `0x4(rX)`

`TLensFlare::perform` and `TSunModel::perform` both call the same inlined bounds test,
and the ROM's two sites differ in shape:

- `gpSunModel->isInBounds(unk40)` -> `lfsu f1, 0xf8(r3)` then `lfs f0, 0x4(r3)`
  (the address `&unkF8[0]` stays live, one register is reused for `.y`)
- `isInBounds(unk1A8)`            -> `lfs f1, 0xf8(r29)` then `lfs f0, 0xfc(r29)`
  (offset folded into the load, because `this` is already a register)

The difference is **not** the receiver; it is that the point is a *sub-object passed by
reference*, so the address has to be materialised whenever the base is itself a pointer
loaded from memory. Spelling it as a free function taking the point reproduces both
shapes exactly:

```cpp
// member form: this is `this + 0xf8` and gets folded at BOTH sites -> no match
bool isInBounds(f32 bounds) { return -b <= unkF8[0].x && ... ; }

// free form: the reference argument keeps the address live where it must
inline bool sunPosInBounds(const JGeometry::TVec2<f32>& pos, f32 bounds) { ... }
```

Note that a *reference local* inside a member (`const TVec2<f32>& p = unkF8[0];`) also
forces the address, and therefore breaks the `TSunModel::perform` site that wants it
folded. Make the call sites use the free function directly - a forwarding member wrapper
gets folded and the address disappears again.
---

## MEASURED: ``#pragma dont_inline`` wrappers are usually NOT the problem (2026-09-30)

An investigation swept every ``#pragma dont_inline on`` site under ``src/`` and then
removed one wrapper that had been blamed as harmful. **Two of the three original claims
were refuted by measurement. Do not repeat them.**

### The sweep, with real numbers

- ``#pragma dont_inline on`` has **109 sites** under ``src/``, containing 109 definitions.
- **103 of them are ``Class::method`` out-of-line definitions of real ROM functions.**
  The spelling - a non-``inline`` out-of-class definition in a ``.cpp`` plus the pragma -
  is exactly what the tips doc sanctions, and it yields a **global** symbol, which is
  correct. **These are not fakematches and must not be swept up.**
- 3 are ``static`` free functions that genuinely exist in ``config/GMSP01/symbols.txt``
  (``initPinnaParco__Fv``, ``addAfterPreNode__...``,
  ``CalcRevisionPosByRotateZ__FRCQ29JGeometry8TVec3<f>FPU3Vec``).
- **5 are invented ``static`` wrappers.** That is the entire inventory - not "~40".

### Removing ``MsVec3SetKiller`` (``src/Enemy/killer.cpp``) measured -2.4 pp on a function

| function | with wrapper | without |
|---|---|---|
| ``TNerveFlyEnemyChaseFly::execute`` | **98.0 %** | 95.6 % |
| all 13 other non-matching functions | - | 0 |
| unit ``fuzzy_match_percent`` | **87.14 %** | 86.90 % |

Claim "it also forces ``sub<f>``, ``sqrt<f>``, ``moveRequest`` out of line" **did not
reproduce**: the complete ``extra`` symbol set is identical in both states apart from the
wrapper itself; those symbols are ``extra`` either way. Deleting the other three wrappers
as well is much worse: ``execute`` 95.6 -> 76.8 %, unit -> 82.05 % (-1.89).

**So: an invented ``extra`` wrapper that buys +2.4 pp on a function is worth keeping.**
The rule is not "invented symbol = remove it", it is **"measure, then keep whichever side
wins"**. Only ``KoopaJrUtilMod`` (``src/Enemy/koopajr.cpp:51``) was actually removable -
and that one was a genuine de-fakematch worth +0.073 pp on the unit, because it was a
hand-rolled reimplementation of ``fmodf`` (``if (fabsf(modulus) > fabsf(value)) return
value; return value - modulus * (f32)(s64)(value/modulus);`` - provably equal to
``fmodf`` in all four sign/magnitude cases). Its three call sites now use ``std::fmodf``,
which ``TDirectionCalc::normalize()`` already used.

### METHODOLOGICAL TRAP - do not get this wrong

An early measurement in the same session showed ``calcRootMatrix`` jumping 86.7 -> 99.5 %
"because of" a wrapper removal. **It was another agent editing the same file
concurrently.** When ~20 agents share one tree, a before/after pair is only meaningful if
both objects were built from a tree you control. Re-baseline from scratch if anything looks
anomalous, and check file mtimes before believing a delta.

### The remaining real lead: linkage class is diagnostic

The ROM's ``killer.o`` defines all four out of line at map-exact sizes, and the linkage
class tells you what kind of entity each one is:

- the two **templates** (``set<f32>``, ``MsWrap<f32>`` - implicit instantiations, no
  linkage in C++98) come out ``local``;
- the two **non-template ``inline`` header functions** (``MsSin``, ``MsCos``) come out
  ``weak``.

Our ``MsSinKiller``/``MsCosKiller`` bodies are already 0x38 = **size-exact**; only the name
is wrong. No pragma spelling was found that yields a weak out-of-line copy, and
address-taking forces *emission* without stopping *inlining* at the call site.

### MEASURED: an UNUSED (dead-stripped) symbol cannot move any matching score

Investigated 2026-09-30 on `src/Enemy/killer.cpp`.

`flyMove__9TFlyEnemyFv` is listed in `marioEU.MAP` as `UNUSED 0002bc`, so it was
**never emitted into the DOL**. Consequences, all measured rather than assumed:

- `build/GMSP01/asm/Enemy/killer.s` has **no `.fn flyMove__9TFlyEnemyFv`**. There is
  no ground truth for it anywhere in this repo, so `decomp-diff.py -d flyMove`
  can only ever show our own side.
- objdiff classifies it as **`extra`**. A unit's `fuzzy_match_percent` is
  `sum(size_i * fuzzy_i) / sum(size_i)` over the **target's** functions only -
  `extra` symbols are not in that sum at all. Verified by A/B on one tree: the
  4-byte stub and a 700-byte body both give `fuzzy=96.376860`, and a per-function
  diff of the two reports shows **zero** differences.
- Therefore "reconstruct the UNUSED 0x2BC function" buys **no** objdiff
  percentage. The only automated check that looks at it is
  `tools/validate-symbol-order.py`'s `[WARN] ... UNUSED symbol with wrong size`,
  which is a warning, not a failure.

Do not budget a matching-percentage gain for work on a dead-stripped symbol.
Corollary: `ninja`'s unit percentage going **up** while you only edited an UNUSED
function means somebody else changed the tree - see the methodological trap above.

**But do not skip the symbol - it is the single best sizing oracle you have.** An
UNUSED map entry with a size means the linker *compiled* that function (it had to, to
know the size) and then dropped the out-of-line copy because nothing referenced it as a
symbol. In practice that means the original source declared the function and **every
call site inlined it**. So:

- The map size is a hard constraint on the body you must write. If you can find a
  reconstruction that compiles to exactly that size AND is small enough to inline at
  the call sites, you have almost certainly recovered the original.
- **The win is the caller, not the symbol.** Fixing the inline shape of the function
  is what moves the caller's register allocation, load offsets and frame. Validate by
  diffing the CALLER, never the UNUSED symbol - there is nothing to diff it against.
- **Do not let the out-of-line copy survive.** If your reconstruction is too big to
  inline, you have not found the right body; you have also just added 175 invented
  instructions and an `extra` symbol for no score. Keep the `inline` and keep a TODO.

**Filling an UNUSED stub with a size-exact but semantically invented body is NOT worth
doing.** It clears a `[WARN]` line and nothing else, and AGENTS.md is explicit that a
TODO beats a fakematch. Price a reconstruction at zero and prefer an honest empty stub
with a precise TODO. (`flyMove__9TFlyEnemyFv` was filled with a 700-byte
right-shape/right-size placeholder on 2026-09-30 and that body was **reverted** for
exactly this reason: every statement order and every constant in it was unverified.)

## MEASURED: the 8-byte frame deltas are the padding bug, NOT a missing call

Investigated 2026-09-30 on `src/Enemy/killer.cpp`, after a hypothesis that the recurring
"our frame is 8 bytes smaller" was caused by a missing inlined call.

Census of every function in the TU that has a `stwu` (43 of 54); 11 have a frame
mismatch, and **10 of the 11 are ours being too small**:

| function | target | ours | delta |
|---|---|---|---|
| `isFindMario` | 0x58 | 0x60 | **+8** (ours BIGGER) |
| `flyBehavior` | 0x48 | 0x40 | -8 |
| `genEventCoin` | 0xd8 | 0xd0 | -8 |
| `init` | 0x50 | 0x48 | -8 |
| `getGravityY` | 0x40 | 0x38 | -8 |
| `KillerBodyCallback` | 0xc8 | 0xb8 | -16 |
| `calcRootMatrix` | 0x90 | 0x70 | -32 |
| `execute(TNerveFlyEnemyNormalFly)` | 0x90 | 0x70 | -32 |
| `fly` | 0x78 | 0x58 | -32 |
| `bind` | 0xd0 | 0xa8 | -40 |
| `execute(TNerveFlyEnemyChaseFly)` | 0x130 | 0xb8 | -120 |

**The hypothesis is refuted, three ways.**

1. The deltas are **bidirectional** (`isFindMario` is +8) and they scale to -120, so
   this is not a fixed per-callsite cost.
2. `getGravityY` and `flyBehavior` are -8 and contain **no sin, no cos, no `MsWrap` and
   no JGeometry call at all** - `getGravityY`'s whole body is a null-nerve compare plus
   two float loads. `init` likewise. So no "missing inlined call" can be the common cause.
3. In `KillerBodyCallback` the ROM **inlines** the whole `jmaSinTable[...]` lookup and so
   do we, instruction for instruction - and it is *still* 16 bytes out. Whatever produces
   these deltas is orthogonal to the sin/cos question.

The useful diagnostic is not the frame size but the **hole**: `frame - (lowest live stack
slot)`. For `flyBehavior`, `getGravityY`, `init`, `genEventCoin`, `bind` and
`calcRootMatrix` the hole is **identical** on both sides (0x4 / 0x4 / 0xc / 0xb8 / 0x5c /
0x18). The locals are laid out the same and the same number of never-referenced bytes is
reserved below them; the only difference is **how much dead padding MWCC added**. That is
the stack-padding bug documented at the top of this file, and it is not steerable from
game source. Confirmed empirically: adding a plausible named local to `getGravityY`
(`TFlyEnemyParams* prm = unk19C;`) made the frame *smaller* (0x38 -> 0x30) and the score
worse (99.9 % -> 85.0 %), because MWCC promoted the pointer to a register and dropped a
callee-saved register. Do not chase these with locals, and do not use a frame pad.

## REFUTED: dropping `inline` from `JMASSin`/`JMASCos` in `JMath.hpp` would REGRESS the build

The claim under test (from another agent): "the retail `MsSin`/`MsCos` (0x38 B) *call* an
out-of-line `JMASSin`/`JMASCos`, whereas `libs/JSystem/include/JSystem/JMath.hpp` defines
them `inline`" - therefore remove the `inline`.

**The ROM's own bytes say no.** `MsSin__Ff` (0x38 = 14 instructions, `weak`) in
`build/GMSP01/asm/Enemy/killer.s` contains no `bl` at all:

```
stwu r1, -0x18(r1)
lfs  f0, "@3354"@sda21      ; 65536/360
lwz  r0, jmaSinShift@sda21
fmuls f0, f0, f1
lwz  r4, jmaSinTable@sda21
fctiwz f0, f0
stfd f0, 0x10(r1)
lwz  r3, 0x14(r1)
addi r1, r1, 0x18
clrlwi r3, r3, 16
sraw r0, r3, r0
slwi r0, r0, 2
lfsx f1, r4, r0
blr
```

That *is* the inline expansion of `JMASSin(v * (65536.0f/360.0f))`, and our build produces
it too - which is why our `MsSinKiller`/`MsCosKiller` bodies are already 0x38, size-exact.
The out-of-line `JMASSin__Fs` does exist (0x1C, `weak`, DOL 0x8002D810, emitted in
`Camera/lensflare.o` and recorded by the map as `UNREFERENCED DUPLICATE`), but nothing
calls it here.

So `MsSin`/`MsCos` are `inline` in the retail header too, and the compiler simply chose to
expand them at some call sites and not others. Forcing `JMASSin` out of line would turn
every currently-correct expansion in the whole project into a `bl` - it would break
`KillerBodyCallback`, which matches the inlined expansion today, and it would destroy the
0x38 size match on `MsSinKiller`/`MsCosKiller`. **Do not make this change.**

## The `libs/` lead that is real and much bigger: JGeometry header *constructors*

While confirming the above, a much larger and cleaner instance of the same class of defect
turned up, and it is the **mirror image** of the "declared-only ctor is never inlined" rule
above.

`TKillerManager::createEnemyInstance` loses its last 20 % because the ROM emits a real
call that we do not:

```
ROM:  stw r31, 0xc(r1) / lwz r3, 0xc(r1) / bl TFlyEnemy::TFlyEnemy /
      lwz r4, 0xc(r1) / ... / bl JGeometry::TMatrix34<...>::TMatrix34()
ours: addi r3, r31, 0 / li r4, @string / bl TFlyEnemy::TFlyEnemy / ... (no call)
```

The call is not optional: it is what forces the `r31` spill and the three reloads. And the
symbol is genuinely alive in the ROM - not dead-stripped:

```
__ct__Q29JGeometry38TMatrix34<Q29JGeometry13SMatrix34C<f>>Fv = .text:0x80006D88; // size:0x4 scope:weak
__ct__Q29JGeometry13SMatrix34C<f>Fv                            = .text:0x80011188; // size:0x4 scope:weak
```

`libs/JSystem/include/JSystem/JGeometry/JGMatrix34.hpp` defines both in-class
(`SMatrix34C() { }` at line 37, `TMatrix34() { }` at line 91), so the deferred inliner
expands them to nothing and the call vanishes. Because the ROM **has** the out-of-line
weak symbol, the fix is the **opposite** of the TCoverFruit rule: *remove* the in-class
definition so only a declaration remains, and provide the 4-byte out-of-line definition in
a JSystem `.cpp`.

Scale of the sweep (`bl "__ct__Q29JGeometry..."` across all 730 `build/GMSP01/asm/**/*.s`):
**74 call sites in 39 distinct TUs**, largest single contributor
`JGeometry::SMatrix34C<f32>::SMatrix34C()` at 30 sites, then
`TVec3<f32>::TVec3(const TVec3<f32>&)` 14, `TMatrix34<...>::TMatrix34()` 9,
`TVec4<f32>::TVec4()` 6, `TMatrix44<...>::TMatrix44()` 5. This is worth far more than any
single game TU and is the change I would ask a human for first.

**How to recognise the shape:** a `<` line in the diff that is a `bl` to a `__ct__` for a
JGeometry type, usually surrounded by a `stw rN` / several `lwz rN` spill-reload pair that
we do not emit, and every other opcode identical. Same signature as the four
`mario/Enemy/killer` MISSING symbols, but for constructors rather than inlines.

### Still outstanding in these files

- ``src/Enemy/killer.cpp``: ``flyMove__9TFlyEnemyFv`` (0x2BC) is back to an
  empty stub with a TODO. A size-exact 700-byte placeholder was written and then
  reverted - see the reasoning above.
- ``mario/Enemy/killer`` is at **96.38 %**, not the 87 % an earlier brief claimed
  (stale figure). Its real remaining levers are the four MISSING symbols
  ``MsSin__Ff`` / ``MsCos__Ff`` / ``MsWrap<f>__Ffff`` /
  ``set<f>__Q29JGeometry8TVec3<f>Ffff`` - 200 B of 13 416 B, about **7.5 % of
  ``total_code``** - plus four non-matching functions: ``fly()`` (580 B @ 97 %),
  ``calcChaseParam`` (560 B @ 91.3 %), ``genEventCoin`` (484 B @ 91.7 %),
  ``KillerBodyCallback`` (504 B @ 79.2 %), ``createEnemyInstance`` (104 B @ 79.6 %).

  Ownership: as of 2026-09-30 a single agent owns this file. The 10 frame-size
  deltas above are the MWCC padding bug and are **not** worth chasing; the missing
  4 symbols and ``KillerBodyCallback``'s CSE/spill behaviour are. In
  ``genEventCoin`` the local declaration order is now proven (``offset`` is declared
  before ``mtx`` - MWCC lays locals out in declaration order from the highest address
  down, and the ROM has ``offset`` above ``mtx``), which is a prerequisite for fixing
  its uniform -8, but the padding itself has to come from somewhere else.

- ``koopajr``: ``mod__Q29JGeometry8TUtil<f>Fff`` (0x5C, ``weak``) is ``missing``. That is
  libs/ defect #2 - ``TUtil<f32>`` has no ``mod``. Hence ``std::fmodf``, not the symbol.
- ``src/MoveBG/MapObjMare.cpp:760,772``: ``stopBoat`` (72 B) and ``stopAtWall`` (244 B) are
  invented statics, both ``extra``, 316 B, outside any ``dont_inline`` block.
- Ten ``framePad``-style stack pads are committed where AGENTS.md forbids the
  ``volatile char trash[]`` family: ``bosseel.cpp:1718``, ``enemyMario.cpp:1065``,
  ``graph.cpp:647``, ``hamukuri.cpp:938``/``2449``, ``koopajr.cpp:791``,
  ``MapObjRailBlock.cpp:32``, ``WaterGun.cpp:1444``/``1803``, ``Map.cpp:107``. Note the
  project's own docs disagree here - AGENTS.md forbids the trick, this file sanctions
  ``framePad_N_name`` for frame-only diffs. That contradiction is unresolved and is a
  question for the human, not for an agent.
---

## THE TOP libs/ DEFECT, now confirmed from two independent directions: ``JMASSin``/``JMASCos``

Two agents, working unrelated files, converged on the same root cause without knowing of
each other. This is the most concrete and most actionable of the seven ``libs/`` defects.

**Observation A** (``mario/Enemy/killer``, 4 symbols MISSING there): the retail compiler
emits ``MsSin``/``MsCos`` as **weak** out-of-line copies (0x38 B each) and
``MsWrap<f32>__Ffff`` / ``set<f>__Q29JGeometry8TVec3<f>Ffff`` as TU-**local** copies,
in every TU that uses them. The templates come out ``local`` because implicit
instantiations have no linkage in C++98; the non-template ``inline`` header functions come
out ``weak``.

**Observation B** (``mario/Camera/lensflare``, 2 symbols MISSING there): the ROM defines
``JMASSin(short)`` and ``JMASCos(short)`` as **28-byte out-of-line functions**. dtk lists
them as called only from ``MsSin``/``MsCos`` - i.e. every retail ``MsSin``/``MsCos`` is a
28-byte forwarder.

**The cause, and the fix.** In ``libs/JSystem/include/JSystem/JMath.hpp`` both are
declared ``inline``:

```cpp
inline f32 JMASCos(s16 v)   { ... }
inline f32 JMASSin(s16 v)   { ... }
```

and ``include/MarioUtil/MathUtil.hpp`` wraps them:

```cpp
inline f32 MsSin(f32 v) { return JMASSin(v * (65536.0f / 360.0f)); }
inline f32 MsCos(f32 v) { return JMASCos(v * (65536.0f / 360.0f)); }
```

MWCC then inlines the lot and emits nothing. **The retail code has the forwarder and not
the wrapper**: ``MsSin``/``MsCos`` are ``inline`` in a header but their *bodies* are a call
to a non-inline 28-byte ``JMASSin``/``JMASCos``.

**Proposed change, for a human ruling - agents are forbidden to touch ``libs/``:** drop
``inline`` from ``JMASSin`` and ``JMASCos`` in ``JMath.hpp`` (or give them a
``#pragma noinline`` layer). Expected payoff, measurable per TU:

- ``Camera/lensflare``: the two 28-byte ``JMASSin``/``JMASCos`` symbols appear.
- ``Enemy/killer``: ``MsSin__Ff`` and ``MsCos__Ff`` become weak out-of-line at 0x38 -
  our ``MsSinKiller``/``MsCosKiller`` bodies are **already size-exact at 0x38**, only the
  name is wrong. 200 B of 13 416 B, about **7.5 % of that unit's ``total_code``**.
- It may also explain the four 99.x % functions in ``killer`` whose **only** difference is
  an 8-byte-larger frame: a missing inlined call that would have created temporaries.

This is a much better bet than the ``TVec3::set<f32>`` depth-threshold work, which a
dedicated investigation showed **cannot** be fixed by a header change (300+ TUs rely on
the ROM inlining ``set`` at every site).

---

## MWCC idioms confirmed by measurement while matching ``killer`` (62.8 % -> 96.85 %)

These are all *spelling* rules, each verified by a before/after on a real function:

- ``>=`` and not ``>`` is what produces ``cror eq,gt,eq``. ``>`` alone gives ``cror
  eq,gt,eq`` with a different flag order or a plain ``ble`` - if the target has
  ``cror eq,lt,eq`` you want the nested-``if`` form, not ``>=``.
- ``checkLiveFlag2()`` / ``isBckAnm()`` / an explicit ``(cond) ? true : false`` are what
  produce the ``li 1`` / ``b`` / ``li 0`` / ``clrlwi.`` bool materialisation. A plain
  ``if (x)`` does not.
- ``s16`` -> ``double`` is ``xoris rD,rS,0x8000`` + ``lis rD,0x4330`` + ``fsubs
  <2^52 constant>``. This is a *different* use of the same ``xoris 0x8000`` that marks the
  int->double idiom, which is why that encoding was previously called ambiguous.
- A ``(u8)`` cast on a float -> ``GXColorS10`` write produces the ``clrlwi rD,rS,24``
  narrowing. Without the cast you get a full 32-bit store.
- A named temporary is sometimes what stops MWCC contracting ``a - b * c`` into
  ``fnmsubs``; splitting the statement without a name does not.
- A whole-struct word copy (``mLinearVelocity`` into a local ``up``) is how the target
  gets its ``stw``/``lwz`` pair.
- In this codebase the pitch clamp lives on ``mRotation.x``, not ``mRotation.y``, in both
  ``calcRootMatrix`` and ``bind``.

## A ``libs/`` shape worth copying rather than fighting

``JGeometry::TMatrix34<SMatrix34C<f32>>::TMatrix34()`` is an empty ``inline`` in
``JGMatrix34.hpp``, so a constructor call that initialises a member gets elided and the
``bl`` disappears (this is what costs ``TKillerManager::createEnemyInstance`` its last
20 %). ``include/Enemy/coasterkiller.hpp`` shows the shape that works: a ``TQuat4<f32>``
member whose constructor is not elided. Prefer a member type that forces the ctor over a
hand-written initialiser.
---

## ⚠️ A FUNCTION CAN BE "100 %" WITH THE WRONG FLOAT CONSTANTS

The single most dangerous measurement artefact found so far. While taking
``mario/MoveBG/MapObjCorona`` from 40 % to 53 %, ``TBathtub::load`` reached a reported
**100 %** while still containing two wrong values:

- ``unk3C`` must be **3000.0f** (we had something else),
- ``unk44`` must be ``unk3C * sinf(0.27925268f)`` (we had a literal).

**Why the 100 % was a lie:** both live in ``.sdata2`` and are reached through an
``lfs rD, @NNNN@sda21`` **relocation**. objdiff compares the *instruction* ``lfs rD, <reloc>``
and the relocation matches, so the operand column agrees - the four bytes at the far end of
the relocation are never compared. A wrong float behind a correct relocation is
**invisible to the instruction diff**.

**Therefore:**

1. When a function hits 100 %, do not stop. Diff the **relocated data** too:
   ``build\binutils\powerpc-eabi-objdump.exe -s -j .sdata2 <obj>`` against the target's, and
   compare the *contents* at the offsets the relocations point at, not the offsets.
2. The same blind spot applies to ``.sdata``, ``.rodata`` string pointers and any
   ``lwz``/``lis``+``addi`` pair whose target is relocated. If a unit has a suspicious
   number of 100 % functions, check its float tables by hand.
3. Corollary for the ``set<f32>`` work: a match percentage is a **lower bound** on
   correctness, never evidence of it.

## Finding string references the ROM encodes in 32 bits

Two dead strings in ``MapObjCorona``'s ``.rodata`` (``+0x250`` and ``+0x298``/``+0x2A8``) were
invisible to every offset grep, because they are **not** referenced by a 16-bit ``addi``:

```
lis r3, "@4389"@ha      addi r31, r3, "@4389"@l
lis r3, "@4997"@ha      addi r4,  r3, "@4997"@l
```

Any search for ``addi rX, r30, 0xNNN`` will miss every string whose ``.rodata`` offset
exceeds 12 bits (0x1000). One of the recovered strings sat past 0x1000.

**The method that works** - resolve by the ``@NNNN`` literal *numbering* rather than by
offset:

1. dump the target object's ``.rodata`` and ``.sdata2`` tables with ``objdump -s`` and note
   the ``@NNNN`` numbers of the *neighbouring* literals;
2. grep which function uses those neighbouring numbers;
3. the unclaimed ``@NNNN`` sitting between two of a function's literals belongs to that
   function.

Here ``@4385``-``@4388`` were ``allowsTumble``'s four floats, so ``@4389`` was
``allowsTumble``'s too; ``@4999``-``@5002`` were ``control``'s, so ``@4997``/``@4998`` were
``control``'s two ``startBck`` calls. Both were resolved with certainty and no guessing.

## MEASURED AND REJECTED: the ``TBathtubData::unk18`` ``SMatrix33C`` -> ``SMatrix33R`` flip

The evidence was genuinely strong - the ROM's ``TBathtub`` ctor calls
``__ct__Q29JGeometry13SMatrix33R<f>Fv``, and the nine identity stores in ``load`` decode to
``identity()``'s statement *order* through R indices, and ``getPos`` already reads
``at(0,0..2)``/``at(2,0..2)``/``at(1,0..2)`` as basis vectors, which is the R convention.

**Measured A/B, and it is a wash:**

| | with ``SMatrix33R`` | with ``SMatrix33C`` |
|---|---|---|
| ``TBathtub::TBathtub(const char*)`` | 86.8 % | 86.8 % |
| ``TBathtub::load`` | 100 % | 100 % |
| ``TBathtubGrip::TBathtubGrip`` | 100 % | 100 % |
| ``Map/BathWaterManager`` ``getPos`` | 91.8 % | **91.9 %** |
| ``Map/BathWaterManager`` ``calcBathtub`` | 95.4 % | 95.4 % |

**Why it cannot work:** ``SMatrix33R<f32>() { }`` is an *empty* inline constructor, so the
symbol is not emitted either way - ``validate-symbol-order.py`` still reports
``__ct__Q29JGeometry13SMatrix33R<f>Fv`` as MISSING after the flip. This is the same
"empty inline ctor gets elided" family as ``JGeometry::TMatrix34``'s default constructor
(``include/Enemy/coasterkiller.hpp``'s ``TQuat4<f32>`` member is the shape that works).
**A matrix typedef flip alone is never enough; something has to force the ctor.**

This is why measuring before accepting a plausible layout argument matters: the reasoning
was sound and the payoff was zero.

## New ``libs/`` defect: ``TMatrix33::identity()`` statement order

``libs/JSystem/include/JSystem/JGeometry/JGMatrix33.hpp`` has **statements 2 and 3 of
``identity()`` swapped** relative to the ROM. Consequence: ``MapObjCorona``'s
``TBathtub::load`` has to spell four chained ``ref()`` assignments out by hand to get the
store order right, where the original plainly called ``identity()`` once. Awaiting a human
ruling like the other seven.

## Frame pads: the ruling

Two in ``MapObjCorona`` (``framePad_16_load``, ``framePad_48_startDemo``) are kept, because
unlike the ``bosstelesa`` one they **measurably buy match percentage**: ``load``
0.3 -> **100 %** and ``startDemo`` 0.6 -> **93.7 %**. The test is not "is it a fakematch",
it is **"does removing it lose bytes"**. Both are commented in place as "confirm the frame
is the last difference", and the unidentified named locals they stand in for are named as
candidates (for ``startDemo``: twelve slots, plausibly the ``stand_effect``
``J3DFrameCtrl`` / ``TFlagT`` / ``fireStartDemoCamera`` argument temporaries).

The ``bosstelesa`` ``framePad_576_loadAfter`` was removed because it changed the score by
**exactly zero** (47.4 % either way). Same trick, opposite verdict, decided the same way.
---

## ❌ RETRACTED: the ``JMASSin``/``JMASCos`` hypothesis is REFUTED - do not touch ``JMath.hpp``

I proposed this on 2026-09-30 as "the top libs/ defect" and it is **wrong**. The ROM's own
instructions disprove it. ``MsSin__Ff`` (0x38, ``weak``) in
``build/GMSP01/asm/Enemy/killer.s`` contains **no ``bl`` at all** - it *is* the expansion:

```
stwu r1,-0x18 / lfs f0,65536over360 / lwz r0,jmaSinShift / fmuls f0,f0,f1
lwz r4,jmaSinTable / fctiwz f0,f0 / stfd f0,0x10(r1) / lwz r3,0x14(r1)
addi r1,r1,0x18 / clrlwi r3,r3,16 / sraw r0,r3,r0 / slwi r0,r0,2
lfsx f1,r4,r0 / blr
```

That is byte-for-byte what our build produces, which is exactly why our
``MsSinKiller``/``MsCosKiller`` bodies are already **0x38 size-exact**. The out-of-line
``JMASSin__Fs`` does exist (0x1C, ``weak``, ``DOL 0x8002D810``, present in
``Camera/lensflare.o``) and the map calls it ``UNREFERENCED DUPLICATE`` - but nothing in
``killer`` calls it.

**So ``MsSin``/``MsCos`` were ``inline`` in the retail header too, and MWCC simply chose
per call site.** Dropping ``inline`` from ``JMASSin``/``JMASCos`` would be **actively
harmful**: it would break ``KillerBodyCallback`` (which matches that inlined expansion
today) and destroy the 0x38 size match.

The general lesson, and it cost two agents a full investigation each: **a MISSING symbol
whose body we can already reproduce inlined is not evidence of a header defect.** Compare
the target's out-of-line body against ours *before* concluding the header is wrong. Ours
matching the target's expansion is a match, not a gap.

## ✅ THE REAL TOP libs/ ITEM: JGeometry constructors are defined in-class and get elided

This replaces the retracted item above, and it is **74 call sites across 39 TUs**.

``libs/JSystem/include/JSystem/JGeometry/JGMatrix34.hpp`` defines both constructors
**in-class**:

```cpp
SMatrix34C() { }        // line 37
TMatrix34() { }         // line 91
```

MWCC expands them to nothing, so the 4-byte out-of-line weak symbols the ROM actually
carries are never emitted:

```
__ct__Q29JGeometry38TMatrix34<Q29JGeometry13SMatrix34C<f>>Fv = 0x80006D88; size:0x4 weak
__ct__Q29JGeometry13SMatrix34C<f>Fv                           = 0x80011188; size:0x4 weak
```

**These symbols are ALIVE in the ROM, not dead-stripped** - so unlike the UNUSED case,
this one is worth real score.

Worked example, ``TKillerManager::createEnemyInstance`` - the whole remaining 20 % of that
function is this one missing call:

```
ROM:  stw r31,0xc(r1) / lwz r3,0xc(r1) / bl TFlyEnemy::TFlyEnemy /
      lwz r4,0xc(r1) / ... / bl JGeometry::TMatrix34<...>::TMatrix34()
ours: addi r3,r31,0 / li r4,@string / bl TFlyEnemy::TFlyEnemy / ... (no call)
```

The call is what forces the ``r31`` spill and all three reloads; inlining the empty ctor
deletes five instructions.

**THE FIX IS THE MIRROR IMAGE of the rule already in this file** ("a constructor that is
only declared is never inlined"). Here the ROM *has* the out-of-line symbol, so the fix is
to **delete the in-class definition and supply the 4-byte out-of-line body in a JSystem
``.cpp``**.

**Census of the 74 sites in 39 TUs** - what to look for:

| callee | sites |
|---|---|
| ``SMatrix34C<f32>::SMatrix34C()`` | **30** |
| ``TVec3<f32>::TVec3(const TVec3<f32>&)`` | 14 |
| ``TMatrix34<...>::TMatrix34()`` | 9 |
| ``TVec4<f32>::TVec4()`` | 6 |
| ``TMatrix44<...>::TMatrix44()`` | 5 |

**Recognition signature in a diff:** a ``<``-line ``bl`` to a JGeometry ``__ct__``, wrapped
in a spill/reload pair we do not emit, everything else identical. That is a 4-byte-symbol
fix wearing a 20-function costume.

## The frame-padding bug is characterised - and it is NOT a missing call

Census of all 43 functions in ``killer`` that have a ``stwu``: 11 mismatch, 10 of ours too
small, deltas from -8 to **-120**. Two hypotheses tested and **refuted**:

- **"the 8-byte delta is a missing inlined ``sin``/``cos``"** - no. ``getGravityY``'s entire
  body is a null-nerve compare plus two float loads; no trig, no JGeometry. ``init`` likewise.
  ``KillerBodyCallback`` inlines the whole ``jmaSinTable[...]`` lookup on *both* sides,
  instruction for instruction, and is still 16 bytes out.
- **"a plausible named local will absorb it"** - no, and it backfires. Adding
  ``TFlyEnemyParams* prm = unk19C;`` to ``getGravityY`` made the frame *smaller*
  (0x38 -> 0x30) and the score *worse* (99.9 % -> 85.0 %).

**The useful diagnostic is the "hole"** - ``frame`` minus the lowest live stack slot. For
``flyBehavior``, ``getGravityY``, ``init``, ``genEventCoin``, ``bind`` and ``calcRootMatrix``
the hole is **identical on both sides** (0x4 / 0x4 / 0xc / 0xb8 / 0x5c / 0x18). The locals
are laid out identically and the same never-referenced bytes are reserved below them; only
the amount of dead padding MWCC chose differs. That is a **compiler padding bug, not a
reconstruction gap** - so no source change can fix it, and a `char framePad_N[]` is the only
lever, to be used solely when it measurably buys percentage.
---

## `clrrwi` is an objdump ALIAS, and reading it as a separate opcode will mislead you

GNU objdump prints ``rlwinm rA,rS,0,0,31-b`` as ``clrrwi rA,rS,b``. **The raw word is
opcode 21, not 30** - there is no separate ``clrrwi`` instruction. This matters because the
MB field is what tells you the *width* of the mask, and misreading the alias makes a
one-bit clear look like a three-bit one:

| printed | decodes to | meaning |
|---|---|---|
| ``clrrwi r0,r0,1``   | ``rlwinm r0,r0,0,0,30`` | ``status &= ~1`` - **one** bit |
| ``rlwinm r0,r0,0,31,29`` | (itself) | keeps bits 29-31, clears 0-28 - **three** bits |

Remember ``rlwinm``'s MB field gives the mask as bit ``31 - MB``; for a ``clrrwi rD,rS,b``
alias that is an ``b``-bit clear starting at bit 0.

**The decisive way to settle an emitter-status mask is not the encoding, it is a sibling
call site.** ``GC2D/SelectShine2.cpp`` calls ``clearStatus(1)`` and emits
``54 00 00 3c  clrrwi r0,r0,1`` - byte-identical to the word in ``GC2D/ConsoleStr``. So the
ConsoleStr site was ``clearStatus(2)`` in our source, a plain wrong-argument bug, and
``STATUS_STOP_EMIT = 0x1`` in ``JPAEmitter.hpp`` is **not** shifted. Tree-wide grep of
every ``clearStatus``/``setStatus`` call site found no other suspect: ``GCConsole2``'s three
sites are all correct, and its unrelated ``clrrwi r0,r30,8`` is a genuine bit-8 clear.

**General rule: when an enum-ish argument produces a mask mismatch, compare against a
sibling call site of the same enum before suspecting the enum.**

## The frame-pad technique has a precise limit: it can only pad at the BOTTOM

Measured on ``GC2D/ConsoleStr::startCloseWipe``, and it is a useful negative result about
the technique in general:

| pad | resulting frame | locals start | score |
|---|---|---|---|
| none | 0x170 | 0xf4 | 85.5 % |
| ``framePad_112_startCloseWipe[112]`` | **0x1E0 (exact)** | 0x104 (unchanged) | **85.5 %** |
| ``framePad_212_startCloseWipe[212]`` | 0x240 | 0x104 (unchanged) | 85.5 % |

The 112-byte pad **does** land the frame exactly - ``stwu r1,-0x1e0`` and
``stmw r25,0x1c4`` both match - and the score does not move at all.

**Why:** MWCC grows the frame *below* the named locals, so a trailing pad lengthens the
frame and shifts the named locals' absolute addresses up. It can therefore fix

- **"frame total short, locals already at the right offsets"** - the ``MapObjCorona`` case,
  where ``framePad_16_load`` and ``framePad_48_startDemo`` really do buy 100 % and 93.7 %;

and it **cannot** fix

- **"the local block itself also sits too low"** - the ``ConsoleStr`` shape, where the gap
  is *interior*: the frame bottom is ~100 bytes below the locals instead of at them. A pad
  cannot put a hole in the middle.

So before reaching for a pad, compare **two** numbers, not one:

1. the frame total (``stwu r1, -0xNN``), and
2. the **hole** = frame - lowest live stack slot (or the saved-register base, whichever is
   lower).

Equal holes on both sides means only the padding differs (a compiler bug, pad may help).
Different holes means the local set is wrong and **no pad will ever help** - go find the
missing named locals.

## REFUTED: the "``subfic`` means a 16-bit domain" explanation for unfolded constants

I proposed that the ROM's ``li r0,0xe0`` / ``subfic r0,r0,0xe0`` (a *runtime* subtraction of
two literals that our build folds) was a 16-bit sign-extension domain artefact. **Wrong,
and the data says so:** the ROM's own ``.sdata2`` **contains** ``-224.0f`` and ``-240.0f``
- exactly the two values we fold out - and never loads them. So the literals were in the
original source, the compiler had them in its pool, and it still chose not to fold.

**Conclusion: it is the ROM's MWCC build simply not folding, not a source-shape difference.
There is no source change that will reproduce it.** Accept the 2-instruction residual.

## A measured limit on "MWCC varies register allocation"

``ConsoleStr::startAppearReady`` (96.1 %, 1 instruction) cannot be fixed: **the ROM itself
emits two different register assignments for two textually identical functions.**
``startAppearGo`` uses r4/r9, ``startAppearReady`` uses r8/r4. We match ``startAppearGo``
at **100 %** and inherit its assignment for ``startAppearReady``. The only way to "fix"
it is to make the two sources differ, which would break the 100 % one. That is a straight
fakematch, declined. Same class as ``__sinit_ConsoleStr_cpp``'s 2-instruction r0/r9 choice.

**When the only difference is which scratch register holds a value, and a sibling function
with identical source has a different one, it is a permanent non-match.** Do not spend
budget on it and do not report it as open.
---

## BEFORE you escalate a missing out-of-line copy to a header change: COUNT THE TUS

This is the third time in one session that a plausible "the header should not be inline"
argument turned out to be a per-translation-unit inliner decision. It is now a hard check.

### The check, and it takes two commands

```powershell
# 1. how many objects DEFINE the symbol, and with what linkage?
build\binutils\powerpc-eabi-objdump.exe -t build\GMSP01\obj\**\*.o | Select-String "<mangled>"

# 2. how many TUs contain a `bl` to it?
python -c "import glob,re; [print(f,[x for x in re.findall(r'bl (\S+)',open(f,encoding='utf-8',errors='replace').read()) if 'MANGLED' in x]) for f in glob.glob(r'build\GMSP01\asm\**\*.s',recursive=True)]"
```

**If the weak definition exists in exactly ONE object, it is MWCC's per-codecase choice for
that one translation unit, not a header property.** Moving the definition out of class will
force a ``bl`` at every call site in every TU and will break every TU that currently
inlines it correctly.

### Worked example: ``TMario::checkStatusType``

``include/Player/Mario.hpp:1187`` declares it in-class as
``bool checkStatusType(s32 flag) const { return mStatus & flag ? true : false; }``.

The ROM's copy:

```
00001e30  w  F .text  0000001c checkStatusType__6TMarioCFl     <- cameragc.o ONLY

lwz r0, 0x7c(r3) / and. r0, r0, r4 / beq .L_80024E48
li r3, 0x1 / blr / li r3, 0x0 / blr
```

Six instructions, 28 bytes. Measured:

- **exactly 1 object defines it** (``Camera/cameragc.o``), ``weak``;
- **exactly 5 ``bl`` sites exist, all 5 in ``Camera/cameragc.s``**;
- **no other translation unit in the whole 730-TU tree mentions the symbol at all**;
- our ``cameragc.o`` does not emit it, because we inline at all 5.

An agent reported this as "~90 call sites tree-wide" and asked for a ``Mario.hpp`` change.
It is **5 sites in one TU**. Making the definition non-inline would be *exactly* the
``JMASSin`` mistake documented above: correct reasoning, actively harmful change.

The real mechanism is visible in the residual: the ``bl`` forces a call frame, so the ROM
needs two more callee-saved registers (r27/r28) for the ``&&`` bool temporaries, while we
spill into volatile r4/r6, which shifts ``this`` from r31 to r29 and re-orders
``CLBCalcRatio<s32>``'s arguments and ``yAngle``'s spill. That single gap accounts for
roughly **450 of the ~509 wrong instructions** in ``calcPosAndAt_`` (4004 B) - so it is worth
real bytes, but only inside this one unit, and only as a per-TU inline-budget effect that
the depth lever cannot reach (see the size-gated section: a 7-instruction callee never
flips).

### The three retractions in one session, for pattern-matching

| claim | verdict |
|---|---|
| ``JMASSin``/``JMASCos`` should lose ``inline`` | **refuted** - the ROM's ``MsSin`` body *is* the expansion, no ``bl`` in it |
| ``TMario::checkStatusType`` should be non-inline | **refuted** - 5 sites, 1 TU, weak copy in one object only |
| the ROM's unfolded ``224 - 464`` is a 16-bit domain artefact | **refuted** - the literals are in its ``.sdata2``, unused; it just did not fold |

Common shape: **a single anomalous object in a tree where 300+ objects behave normally is
MWCC's per-codecase decision.** A header change would have to be right about all 730 at
once, and the previous header edit that was attempted on this reasoning was reverted.
---

## A ``signed`` loop counter costs an extra ``xoris`` in the int-to-float idiom

Found in ``GC2D/hx_wiper`` on 2026-09-30, and it lifted five functions at once.

MWCC's ``(f32)integer`` conversion for a **signed** source emits an extra sign fix-up
(``xoris rD,rS,0x8000`` plus a bias) that the **unsigned** form does not. Declaring a file
scope counter as ``s32`` when the original had ``u32`` therefore costs instructions in
*every* function that converts it.

| function | ``s32`` | ``u32`` |
|---|---|---|
| ``Hx_Test1``   | 94.2 % | **100 %** |
| ``Hx_GameOver``| 96.7 % | **99.6 %** |
| ``Hx_Test2``   | 98.2 % | **99.7 %** |
| ``Hx_Test2R``  | 98.2 % | **99.7 %** |
| ``Hx_Test4`` / ``Hx_Test5`` | - | 99.1 / 99.4 % |

**Rule: when a loop or frame counter is converted to ``f32``, its declared signedness is
load-bearing.** A ``u32``/``s32`` mismatch in a header can cost instructions in dozens of
places at once, and it looks like a dozen unrelated local problems.

## A dead leading parameter shifts every argument register

``Hxs_Logo_TexDraw`` was reconstructed with **seven** parameters; it has **six**. The
leading ``u16 textureWidth`` is never read in the body but still occupies ``r3``, shifting
every subsequent argument by one register - so the whole function mismatched while looking
like an arithmetic problem.

**When a function's every argument is off by exactly one register, suspect a parameter
count, not the argument expressions.** Check the map's mangled name against the callee's
register traffic: a parameter that is never read in the body is still passed.

## The frame-only family: a large deficit with an identical instruction stream

A second, independent sample of the systematic frame problem - and this one has a
different shape from the pad-friendly case, so it is worth pairing with the
``GC2D/ConsoleStr`` sample.

In ``GC2D/hx_wiper``, four functions are frame-short while **every emitted instruction
matches** (only ``~`` operand diffs from the displacements, no ``|``/``<``/``>``):

| function | our frame | ROM frame | deficit |
|---|---|---|---|
| ``Hxs2_Circle`` | 0x0e8 | 0x198 | **+0xb0 = 176 B (44 slots)** |
| ``Hxs1_Circle`` | -     | -     | **+0xb8 = 184 B** |
| ``Hxs1_Test2``  | 0x0c8 | 0x108 | +0x40 = 64 B (16 slots) |
| ``Hxs1_Test1``  | 0x080 | 0x0c0 | +0x40 = 64 B |
| ``Hxs_Logo_TexDraw`` | 0x0c8 | 0x0b8 | −0x10 (ours BIGGER) |

Note the deficits are **multiples of 16 or 8 in slot counts** (44, 46, 16, 16), i.e. whole
named locals. All four call the same three helpers - ``Hx_CameraInit``, ``Hx_GxInit`` and an
inlined ``sqrtf`` - so the common factor is those calls, not the loop bodies.

**Working hypothesis, unproven:** the original kept ``double`` or named float intermediates
alive inside the loops where ours fold them away. 44 extra live slots is not padding. A
pad cannot help here (the local block itself is short), so this is the *reachable* case -
and it is the shape the running frame investigation should be looking for.

**Practical consequence:** a deficit that is a clean multiple of 4 in slot count is a
*count* of missing named locals, and is therefore a decoding problem with a finite answer.
Compare against the pad-friendly ``MapObjCorona`` shape (16 B and 48 B) before assuming a
compiler bug.

## Two metric artefacts to know about

- **A jump table can score ~60 % in ``.data`` with every target matching.** ``@1004`` in
  ``hx_wiper`` scores 61 % although all nine ``bctr`` targets and every case body match.
  The relocation entries for a jump table are dense and their ordering is compiler-chosen;
  do not spend budget "fixing" a case body that already matches.
- **objdiff's denominator moves when you fix ``missing`` data symbols.** In ``cameragc``,
  turning six ``missing`` data symbols into 100 % grew ``total_code`` by 136 B, so the
  *percentage* rose only 0.17 pp while the *credit* rose 152 bytes. Conversely a unit can
  gain real bytes and show almost no movement. **Always report both** ``fuzzy %`` and
  ``matched/total``; the percentage alone understates the work.
---

## ⚠️ PATH TRAP: there is no ``src/JSystem/`` - ``JDrama`` units live under ``libs/``

I gave an agent the instruction "``src/JSystem/JDrama/`` is the game tree, ``libs/JSystem/``
is the forbidden one" and **that was wrong**: ``src/JSystem/`` does not exist in this repo.
``objdiff.json`` gives ``source_path: libs/JSystem/src/JDrama/...`` for the ``JDrama``
units, so they are *entirely* under the forbidden path. The agent flagged the discrepancy
correctly, then edited the two files anyway (+4.7 and +1.7 points). **I reverted both with
``git checkout --`` and rebuilt.** ``libs/`` is clean.

**Do not accept a path-based claim about what is in scope from a brief. Verify it:**

```powershell
python -c "import json;d=json.load(open(r'build\GMSP01\report.json',encoding='utf-8'));print([ (u['name'],u['metadata'].get('source_path')) for u in d['units'] if u['name'].endswith('JDRCamera')])"
```

The unit's own ``metadata.source_path`` is authoritative. If it starts with ``libs/``, the
unit is off limits no matter what directory the brief named.

## FINDING: retail ``JGeometry::TMatrix34::concat`` computes row 2 incorrectly

Decoded by symbolically tracing every ``lfs``/``fmuls``/``fmadds``/``fadds`` in the target
``build/GMSP01/asm/JSystem/JDrama/JDRCamera.s`` against a stack model. This is a real defect
in the retail code, and it is the reason ``TPolarCamera::perform`` (1088 B) and
``TSmJ3DAct::perform`` (1228 B) are stuck at 65.8 % / 72.1 % with 100 % of their real-code
deltas inside the matrix math.

**``concat``** (``libs/JSystem/include/JSystem/JGeometry/JGMatrix34.hpp:107``): the ROM
builds the ``set()`` row-2 terms from different matrix elements than the header does. For
the first concat in ``TPolarCamera::perform`` (``b`` = identity+translation, ``a`` =
``setEularZ``), for ``set`` argument ``m20`` the ROM computes

```
0.0f * a(0,2) + 1.0f * a(1,2)
```

and the same shape on ``a(row 1)`` / ``a(row 0)`` for ``m21``/``m22``, whereas the header's
row 2 is ``a(2,0)*b(0,j) + a(1,2)*b(0,j+1) + a(2,2)*b(0,j+2)`` - the ``1.0f`` lands on a
different element entirely.

**And the ROM additionally stores the mathematically correct row 2** into four **dead**
stack slots ``0x8/0xc/0x10/0x14(r1)`` (observed values ``0.0f, 0.0f, 1.0f, -unk44``, never
read again). The header's ``concat`` has no such stores.

**Consequence: the shipped view matrix is wrong in row 2 in the ROM itself** - its
``m20..m23`` are garbage. So there is no source that is both faithful to the binary and
sensible, and matching it means **reproducing a retail bug**. That is a decision for a human,
not for an agent.

**Corroboration, and it makes this generalisable:** the only other ``concat`` call site in
the tree, ``TBathWaterMeshRenderer::prerender`` in ``Map/BathWaterManager`` (85.0 %), shows
**the same four dead ``0x8..0x14`` stores**. So every retail ``concat`` has them. If the
header is ever "fixed" to match, expect ``prerender`` to move as well.

**Also decoded: ``TRotation3::setEularZ``** (``JGRotation3.hpp:334``) stores its 9 rotation
elements in the order

```
mMtx[0][0], mMtx[0][1], mMtx[1][0], mMtx[1][1], mMtx[2][2],
mMtx[2][1], mMtx[1][2], mMtx[2][0], mMtx[0][2]
```

(target offsets ``0xd4, 0xd8, 0xe4, 0xe8, 0xfc, 0xf8, 0xec, 0xf4, 0xdc``) - the same 12
values as the header's row-major order, in a different sequence.

**Working rule this suggests:** before assuming a JGeometry inline is "called out of line so
it must be wrong in the header", check whether the ROM's own arithmetic is self-consistent.
Here it is not, and that fact is the finding.
---

## ✅ THE INLINE-BUDGET MECHANISM: a BY-VALUE parameter is the level that counts

This is the answer to a question three separate investigations failed to settle. Found
2026-09-30 in ``Camera/CameraDemo.cpp``.

The ROM emits ``bl JGeometry::TVec3<f32>::add(const TVec3<f32>&)`` **twice, out of line**,
in ``CPolarSubCamera::updateDemoCamera_``. Written the obvious way it inlines to three
``fadds`` and the function sits at 54 %:

```cpp
pos = origin; pos.add(rel);        // WRONG - inlines, 54 %
unk124 = origin + rel;             // RIGHT - emits two `bl add`, 92.3 %
```

The difference is that ``TVec3`` has

```cpp
friend const TVec3& operator+(TVec3 fst, const TVec3& snd);
```

**Its first operand is taken BY VALUE.** That by-value copy is the inline level that
exhausts MWCC's deferred-expansion budget, so ``add`` is called rather than expanded. The
spelling reproduces all three symptoms at once: the byte-copy of ``origin`` into a stack
temporary, the ``bl add``, and the ``lwz``/``stw`` write-back.

**So the depth lever is not "add a wrapper function". It is "pass something by value".**
This is consistent with, and explains, both earlier results:

- the dedicated ``set<f32>`` investigation measured the threshold by **float arity**
  (1-2 args inlines to depth 3, 3+ args to depth 2) - arity *is* a proxy for how much
  parameter-copy code has to be materialised;
- the follow-up measured the lever as **size-gated** (a 276 B callee flips, a 28 B one does
  not) - and a by-value copy adds bytes, which is why it only tips larger callees.

### Try this before concluding a ``bl`` is unreachable

1. Is there an ``operator+`` / ``operator-`` / ``operator*`` overload for the type, taking
   its first operand **by value**? Use it instead of the member form.
2. If not, is there a **free function** overload, or can one be reached through a
   ``friend``, that takes a by-value first parameter?
3. Only then consider a wrapper - and remember a wrapper is a fabrication, whereas using an
   operator overload that already exists in ``libs/`` is not.

**Cost of doing it right:** ``CameraDemo.o`` now also emits a duplicate weak copy of
``add__Q29JGeometry8TVec3<f>FRCQ29JGeometry8TVec3<f>`` (52 B) that the ROM's copy at
``0x80028870`` already provides. The linker dedupes it, and ``extra`` symbols are not in
``fuzzy_match_percent`` at all, so the trade is: 52 B of duplicate weak text for ~66 B of
real match. **Worth it. Confirmed.**

### A second finding from the same function: statement order is load-bearing

The ROM evaluates ``unk124 = origin + rel;`` **before** computing ``rel2``. Moving it after
drops the function from 92.3 % to 76.5 %, because MWCC then postpones the call and reloads
``origin.y``/``origin.z`` instead of keeping them live in f30/f31. With a ``bl`` in the
middle of a block, **what is live *across* the call** is decided by statement order, and
that is observable as a callee-saved-register choice.

## Statement-order rule, stated generally

> When a function's residual is a wrong *register allocation* rather than wrong code, look at
> **what must stay live across each call** and check that the source computes it before the
> call. Reordering two statements can move a value from a volatile to a callee-saved
> register, and a callee-saved register is visible in the prologue.

## Confirmed permanent non-matches in this class

- ``isNeedGroundCheck_`` (99.3 %) - only an f1/f2 swap. Both the ``a``/``b`` declaration swap
  (99.2 %) and hoisting ``distY`` (96.5 %, because MWCC then emits a branchless ``fmr``
  select instead of ``ble``/``fmr``) are worse. Accept.
- ``execWallCheck_`` (95.6 %) - identical instruction sequence, register numbering only;
  swapping ``posArg``/``posCam`` declaration order gives 95.0 %. Accept.

**Also a tooling warning:** the diff tool prints ``f1``/``f2`` operands in braces, and reading
those as ``fcmpo cr0, f3, f2`` led to a wrong diagnosis. **The raw ``.s`` is the ground
truth for operand order** - always confirm a suspected operand swap against
``build/GMSP01/asm/...``, not against the diff tool's rendering.
---

## ❌ FALSIFIED: ``MathUtil.hpp``'s ``MsClamp`` is CORRECT - do not change it

Two agents reported that ``MsClamp`` "tests the lower bound first in the ROM" and I was
about to change the header. **That is wrong**, and it is falsified three ways inside a single
object (``Camera/cameragc.o``):

**(a) The ROM's own out-of-line body tests the UPPER bound first.**
``MsClamp<f>__Ffff`` at ``.text:0x47C``, 0x20 B, ``f1=t, f2=l, f3=r``:

```
fcmpo cr0, f1, f3      ; t vs r      <- UPPER first
ble  .L_80023490
fmr  f1, f3            ; t = r
blr
.L_80023490:
fcmpo cr0, f1, f2      ; t vs l
bgelr                  ; if t >= l, return t
fmr  f1, f2            ; t = l
blr
```

Byte-for-byte what ``if (t > r) t = r; else if (t < l) t = l;`` compiles to. Our header is
already right.

**(b) The ROM *inlines* ``MsClamp<f32>`` at ``loadAfter`` 0x25e4 and our build is identical
over all 12 instructions** - same ``lfs`` order, same ``fcmpo``, same ``ble``, same
``fmr``/``b``, same ``stfs``. Also verified at the two other inlined sites in the TU
(``calcPosAndAt_`` 0xdb0, and ``calcSlopeAngleX_``'s ``MsClamp<s16>`` tail).

**(c) The "lower-first" two-``if`` form is measurably worse.** Written by hand at that exact
site it emits ``bge`` against the *lower* bound where the ROM has ``ble`` against the upper,
and ``loadAfter`` drops 94.3 -> 93.9 %.

**What the two agents actually hit** is almost certainly the ordinary inline-vs-call
question: ``MsClamp<f32>`` is ``local`` and 32 B in the ROM, called out of line from
``perform`` and inlined elsewhere. That is a per-codecase decision, not a header defect —
see "COUNT THE TUS" above.

**The general lesson, and it is the same one three times over:** before changing a shared
header on the report of a local diff, go and read **the ROM's own out-of-line body of that
function, if it has one**. It is ground truth, it is in the repo, and it settles the question
in one step. Today that single check would have saved a wrong edit three times.

## The frame diagnostic needs a THIRD case, and it is the common one

The two known cases were "pad fixes it" (``MapObjCorona``) and "pad cannot, local block is
short" (``ConsoleStr``). Five functions in ``Camera/cameragc`` are a **third** shape, and it
is the one you will meet most often.

Measured, all five, pad applied and deleted:

| function | frame target/ours | pad | frame after | matched bytes gained |
|---|---|---|---|---|
| ``loadAfter``       | 0x140 / 0x0d0 | ``char[0x70]``  | **0x140 exact** | **0** |
| ``calcPosAndAt_``   | 0x2d0 / 0x1d0 | ``char[0x100]`` | **0x2d0 exact** | +4 |
| ``calcSlopeAngleX_``| 0x110 / 0x0d0 | ``char[0x40]``  | **0x110 exact** | +2 |
| ``ctrlGameCamera_`` | 0x120 / 0x100 | ``char[0x20]``  | **0x120 exact** | +1 |
| ``perform``         | 0x068 / 0x058 | ``char[0x10]``  | **0x068 exact** | **0** |

**Five exact frame matches bought +7 bytes on 800 unmatched bytes.** The deficit is
**positional, not volumetric**.

### The cheap tell: ``nloc``

Let ``nloc`` = the number of **referenced** local slots (anything an instruction actually
touches, below the saved-register base). If ``nloc`` matches between the two sides while the
frame differs, the missing locals are **never-referenced** ones - MWCC allocates a named
local even when it lives entirely in a register and is never spilled - and **no pad can ever
help**, because a pad grows the region that is already dead.

Measured ``nloc``, target/ours: ``perform`` 0/1, ``ctrlGameCamera_`` 8/8,
``calcPosAndAt_`` 37/38, ``calcSlopeAngleX_`` 16/16, ``loadAfter`` 9/10. Equal or one-extra
in all five. **There is no missing local that is ever read**, and you cannot find the missing
ones from the target's assembly, because nothing in it points at them.

### The second tell: the distribution is mirrored

Our referenced locals are packed at the **bottom** of the local region; the target's are
packed at the **top**, immediately below the saved registers. For ``calcSlopeAngleX_``:

```
ours   [dead 108][p3][p2][sample][norm][diff]
target [dead 152][sample][dead 20][p2][gap 4][p3][norm][diff]
```

Same five ``TVec3``s, different **declaration order** (``sample`` is declared *after*
``p2``/``p3`` in the original), plus a 20-byte inter-object gap. A trailing pad lands on the
wrong side of the live objects, which is why it buys nothing.

### So there are three cases, and the pad is right in only one

| case | signature | pad |
|---|---|---|
| **A** | frame short, referenced locals already at the right offsets, ``nloc`` matches, same distribution | **yes** - ``MapObjCorona``'s 16 B and 48 B pads, real 100 % and 93.7 % |
| **B** | frame short **and** the referenced span is short too (``liveLo`` much lower) | no - go find the missing locals |
| **C** | frame short, ``nloc`` matches, distribution **mirrored** | **no** - the pad grows already-dead space |

**Case C is not the compiler padding bug either.** The root cause is upstream: a header
inline that the ROM calls and we expand changes how many temporaries exist at all. In
``cameragc`` the clean illustration is ``perform``: our single referenced local at ``0x28``
is the ``volatile float y`` round-trip from an inlined ``MsSqrtf``
(``stfs f0,0x28(r1); lfs f1,0x28(r1)``), and the target has **zero** referenced locals
because it *calls* ``MsSqrtf``.

### Consequence: this bounds a unit, and that is worth writing down

Of ``cameragc``'s ~800 unmatched bytes: **~0 are frame-related** (at most 7 reachable),
**~300 B are the 5 missing header-inline symbols**, and **~500 B are inline-vs-call
expansion inside the 8 non-matching functions**. The last two are the same root-cause class
and both sit outside a unit-matching agent's reach. Establishing this is a legitimate and
valuable result - it converts "this unit is 6 points away" into "this unit is done, and here
is exactly what would unblock it".
---

## TRIAGE: rank candidates by the ``<`` marker, not by match percentage

`build/GMSP01/report.json` only carries `fuzzy_match_percent` per function, which is a
**useless ranking signal** - it does not distinguish "one instruction wrong" from "the
whole logic is wrong", and a 99.6 % function is usually pure frame noise. Run
`decomp-diff.py -d` instead and read the marker column:

| marker | meaning | actionable? |
|---|---|---|
| `~` / bare register rename | allocation noise | **no** - see "What NOT to focus on" |
| `<` | present in the ROM, **missing from us** | **yes** - almost always missing logic |
| `>` | we emit something extra | usually a structural mistake, but check for a shared cause |
| `\|` | wrong opcode | rare, usually a real bug |

**`<` is the high-value marker.** In one session, filtering clean-TU functions on
`<`/`>` found every real bug, while the percentage-ordered shortlist contained almost
only frame noise. Measured hit rate: ~1 win per 14 candidates in the 50-80 % band, then
**0 for 74 in the 80-100 % band** - above ~80 % the structural markers stop indicating
missing logic and start indicating inline-budget artifacts (below).

Note the unit -> source path lives in the **report's unit metadata**
(`metadata.source_path`), *not* in `objdiff.json`, where `source` is null. Getting this
wrong silently disables any "skip files another agent is editing" filter.

### A missing condition is often a predicate that already exists in the codebase

Two of the four fixes this session were not "write new logic" but "use the helper that
was already there". Before inventing a test, grep for it:

- `TBossPakkun::setGroundCollision` was missing a `&& !mSpine->isNerve(&TNerveBPTumbleOut::theNerve())`
  term. The ROM showed a *second* `theNerve()` static-init guard that our build simply
  did not emit - a missing `__register_global_object` block is a reliable tell that a
  call to a `DEFINE_NERVE` singleton is absent. 68.6 % -> **100 %**.
- `TDonchou::calcRootMatrix` gated a counter block behind a test the ROM spells as
  `unk124 == 1 || unk124 == 2`. That is verbatim `TMarDirector::isTalkModeNow()`, which
  already existed. 81.2 % -> **88.3 %**. The same function also compared against
  **20**, not the 100 we had - so a single missing guard often hides a wrong constant
  next to it.

### Reads: an accessor can be either too weak or too strong

`TSmallEnemy::isHitValid` re-loaded `mLiveFlag` after the branch, while the ROM cached it
in a register and reused it for the `ori`. Writing `mLiveFlag |= FLAG` directly instead
of going through `checkLiveFlag`/`onLiveFlag` produced 89.5 % -> **100 %**. The reverse
case also occurs: `TMapObjSwitch::control` wants the *reload* that CSE removes, and no
spelling tried (`getStateTimer()`, then the `const`-parameter trick) recovers it.

So: when a diff shows one extra `lwz` on one side only, try the **direct field access**
first, and if that fails try an **accessor** - not both, and measure each.

### Do not reorder ``switch`` cases on a hunch

Case order changes MWCC's decision-tree *pivot*, and the effect is large and not
obviously related to what you changed:

| function | before | after reordering cases |
|---|---|---|
| `TRevolvingFenceInner::controlGroundRoof` | 95.1 % | **96.6 %** (kept) |
| `TLampSeesawMain::control` | 98.2 % | **28.7 %** (reverted) |

Both bodies were semantically identical to the ROM before and after; only the tree shape
differed. `MapObjFence` gained 1.5 points, `MapObjBianco` lost 70. **Verify every
experiment and revert on any regression** - this is the single easiest way to wreck a
near-matched file.

### Function-local statics: a second guard block means a second missing call

`DECLARE_NERVE`/`DEFINE_NERVE` emit a `theNerve()` singleton guarded by a
`init$NNN` byte plus `__register_global_object`. When the ROM has **two** such blocks in
one function and our build has **one**, the second is a nerve/accessor the source never
calls. Grep the ROM `.s` for `__register_global_object` and compare the count against
ours - this is a fast, high-signal check on any function that mentions a singleton.

### Known blocked cases - do not re-attempt these

- ``TGesso::getSightDirection`` - fabricated helper; the ROM inlines the 4-way decision
  and emits **no symbol**. No spelling suppresses the weak out-of-line copy (see
  `#pragma dont_inline` above), and spelling the ternary out at the call site cost
  ``walkBehavior`` 81.6 % -> 24 %.
- ``TSpcStack<TSpcSlice>::push`` - the ROM calls it out-of-line 6 times, all in
  ``NPC/NpcEvent``; we inline it. Header-wide, so fixing one side regresses the rest.
- ``TSlotDrum::moveObject`` - the ROM builds `2^52 + unk168` and subtracts a `double`
  used exactly once; both objects carry identical `.sdata2`, so only the expression shape
  is wrong. The constant could not be resolved (`objdump` prints `0(0)`; the sda21
  reloc is unapplied).
- ``GessoBodyCallback`` - 112 instructions, 6 wrong opcodes; the two stack matrices
  (`MsMtxSetRotX` target and the body-scale matrix) land at different frame offsets
  and the scale-diagonal stores are interleaved differently. Declaration order alone
  is unlikely to fix it; needs a dedicated session.
- ``CPolarSubCamera::calcTowerCenterPos_`` - the ROM rematerialises `lis`/`addi` for
  `sPositionNameTable` in **every** `switch` arm; ours hoists the table address into
  the prologue. Pure scheduler/remat difference, not expressible at source level.
- ``JDrama::TViewObj`` ctor - the ROM initialises ``unkC`` (``TFlagT<u16>`` at
  **0xC**) to 0 and inlines the whole base chain; our header omits ``unkC(0)`` and
  MWCC emits the ctor out-of-line anyway. Adding ``, unkC(0)`` changes **nothing**
  (verified), and the header is JSystem middleware - off limits. Affects 90 classes.


## Resolve a `<` block: read the vtable slot, then name the field

Two tricks that together closed a 27-point gap in one pass.

### 1. `lwz r12, 0xc0(r12); mtlr r12; blrl` - which virtual is that?

A `<` block that ends in an indirect tail call is a **missing virtual dispatch**, and
`build/GMSP01/mario.elf` is the ROM-linked ELF, so the vtable is readable. Find the
vtable address from any constructor in the same `.s`
(`lis r3, __vt__5TItem@ha` / `addi r3, r3, __vt__5TItem@l`, cross-checked against
`marioEU.MAP` which gives `__vt__5TItem 0001e154 0001e4 803c1b34`), then walk
`sizeof(ELF32 BE section headers)` -> read `sh_addr`, `sh_offset`, `sh_size`; resolve
the symbol names from `SHT_SYMTAB` + its `sh_link` string table. Slot 0xC0 of
`__vt__5TItem` (0x803C1B34) is `calcRootMatrix__5TItemFv`.

The same map lookup settles "which field is 0x4C?": walk the base chain by hand -
`TMapObjBase` fields start at 0xF4, `TLiveActor` has nothing between 0x30 and 0x60,
`TTakeActor` only lists 0x68/0x6C, so read `include/Strategic/HitActor.hpp`, which
declares `/* 0x4C */ int mActorType`. Once named, `isActorType(0x20000022) ||
isActorType(0x2000002A)` is already used elsewhere in the same file - the missing
piece was only the dispatch.

### 2. `stw rX, 0x20(r1); stfs f0, 0x20(r1)` - a by-value aggregate parameter

`JGeometry::TVec3<f32>::operator-` is
`friend const TVec3& operator-(TVec3 fst, const TVec3& snd)` - **left operand by
value**. So `(a - b).length()` materialises the 12-byte delta as a stack temporary
and `.length()` re-reads it; `a.distance(b)` and `a.squared(b)` recompute each
difference in FPRs and never spill. Same maths, completely different frame.

    TSpineEnemy::isReachedToGoal  67.5 % -> 99.7 %   (a - b).length()
                                     29.4 %           TUtil<f32>::sqrt(a.squared(b))
                                     60.9 %           named local + a.sub(b)
                                     38.6 %           named local + a - b

The named-local forms are *worse* because `sqrt` then gets strength-reduced to the
`frsqrte` sequence instead of staying a `bl`.

Corollary: when a `<` block is pure `stw`/`lfs` of a 3-float group at 0x20/0x24/0x28,
reach for a value-returning-by-argument helper before rewriting arithmetic.

### 3. `framePad_NN` only survives when the function has real locals

The `char framePad_NN_name[NN]; (void)framePad_NN_name;` idiom (already used in
`Animal/AnimalBase.cpp`, `Camera/*`) is dropped by the optimiser when the function has
no other locals: in `TEffectObjBase::perform` it changed nothing at all (frame stayed
`-0x8`). It worked immediately in `TItem::calc` and `TEnemyMario::checkReturn`, both of
which have a matrix / `TVec3` local. It never produces a `stw r31`; if the ROM's only
delta is a callee-saved register the frame pad is not enough - stop there.

### More blocked cases found 2026-09-30 (do not re-attempt)

- ``TNormalLift::setGroundCollision`` (95.7 %, only 2 ``<``) - the ROM calls
  ``JGeometry::SMatrix34C<float>::SMatrix34C()`` **out of line** to default-construct the
  local matrix; we have ``SMatrix34C() { }`` defined inline, so the call vanishes.
  Reproducing it means editing ``libs/JSystem/include/JSystem/JGeometry/JGMatrix34.hpp``
  (middleware - prohibited). Every other instruction already matches byte for byte.
- ``TRocket::setDeadAnm`` (91.3 %) - after the ``mPos`` store the ROM loads three words
  from ``mExpWaterEmitInfo->mDir.value`` (0x54..0x60) and **throws them away**, then
  re-reads the position from the stack. A dead read MWCC kept; no C++ spelling
  reproduces it. (An earlier agent already left a TODO saying the same thing - correct.)
- ``TMapObjBase::initMActor`` (86.9 %) - the ROM copies the unused 2nd parameter into
  r29 in the prologue and restores it in the epilogue, never reading it. Our MWCC drops
  the dead parameter, so we emit neither the copy nor the r28 save (frame 0x30 vs 0x38).
  Classic "dead parameter kept live by the old front end" artefact.
- ``TBiancoGateKeeper::getRumblePow`` (99.8 %) - only a 4-byte frame offset is left
  (temp at 0x18 vs 0x1c). Do **not** "fix" it by switching to
  ``mPosition - SMS_GetMarioPos()``: that picks the by-value ``operator-`` which is not
  inlined here and drops the function to 50.8 % with a full prologue. The by-value trick
  from the section above is specific to the call site that produced it.

## The missing-virtual-dispatch class is exhausted (measured 2026-09-30)

``TItem::calc`` gained 28 points from a missing ``blrl``. I wrote a throwaway detector
that counted ``blrl`` per mangled symbol in the ROM ``.s`` and in our object, for every
non-matching function (<= 700 B) in an **unmodified** source file, and compared:

    178 units scanned, 127 of them contain at least one blrl
    ROM dispatches virtually and we do not :   0
    we dispatch virtually and the ROM does not : 75

So on clean files there is no `TItem::calc` left. The remaining yield is the opposite
direction - the `>` marker. Two traps when writing this yourself:

- Match **exactly** on the mangled name. `report.json`, the ``.s`` ``.fn`` names and the
  ``objdump`` symbol names are all the same string; a substring/`max()` match reports a
  comfortable 0 candidates and hides every real hit.
- The ``asm`` path is the source path with ``src/`` stripped and the extension swapped to
  ``.s`` - the mirror directory is ``build/GMSP01/asm/``, not ``build/GMSP01/asm/src/``.
- Units under ``mario/JSystem/``, ``mario/THPPlayer/``, ``mario/PowerPC_EABI_Support/``
  resolve to ``libs/`` for their source, so the asm path breaks; skip them (they are
  forbidden anyway).

Also note how easy it is to over-generalise one local fix:

- ``mScaling = param_2 * 1.3f`` (91.9 %) -> named local + in-place ``scale()`` = 78.9 %,
  because that makes MWCC **inline** ``TVec3::scale`` and drop the ``bl`` the ROM keeps.
- Same as ``getRumblePow``: the shape that matches is a property of the *call site*, not
  a general rule. Read the target's own instruction sequence before rewriting.

## Empty stubs + ``inline`` = the whole function body disappears (measured, MapObjMonte)

Unit ``mario/MoveBG/MapObjMonte`` went **22.90 % -> 26.11 %** (+5.06 pp of the unit,
+0.22 pp of the global headline) purely by restoring the *call structure* of stubbed
functions. Three functions moved:

| Function | Before | After |
| --- | --- | --- |
| ``THangingBridge::perform`` | 32.9 % | **97.6 %** |
| ``THangingBridgeBoard::drawOneRope`` | 1.0 % | **99.1 %** |
| ``TSwingBoard::drawOneRope`` | 1.0 % | **79.3 %** |

The mechanism, and it is the same one as the ``TVec3::scale`` case above but with the
opposite starting point: **an empty stub declared ``inline`` in the header is inlined
away, and every ``bl`` the ROM emits at the call site disappears.** ``perform`` was at
32.9 % purely because of this - its own arithmetic was fine, it just no longer *called*
``drawOneRope``.

The fix is ``#pragma dont_inline`` in the **``.cpp``**, not ``__attribute__((noinline))``
in the header:

- ``__attribute__((noinline))`` in the header -> **fails to compile** under MWCC 1.2.5e
  with a ``declaration syntax error``. Reverted.
- ``#pragma dont_inline`` in the ``.cpp`` -> works. This repo already uses this idiom
  (109 sites, 103 legitimate).
- Confirm the map really wants the out-of-line call before adding it: check
  ``config/GMSP01/symbols.txt`` for a real ``theNerve()``-style symbol at that address,
  or read the target's own ``bl`` in ``build/GMSP01/asm/...``.

Float constants for these functions were recovered from the ``.sdata2``/``.sdata`` dumps
at the **tail** of ``build/GMSP01/asm/MoveBG/MapObjMonte.s`` - no guessing needed. Note
``0x98`` is ``GX_TRIANGLESTRIP``, not ``GX_QUAD_STRIP``: these are ribbons.

### MWCC's bit-test mask is the COMPLEMENT of the bit index - the section below was WRONG

**This section previously said MWCC could not express the ROM's bit test, and that writing
``cue & 0x8`` would be a fakematch. Both halves of that were wrong, and the "honest"
constant was itself incorrect.** Corrected:

MWCC 1.2.5e encodes ``x & (1<<b)`` as ``rlwinm rD, rS, 0, 31-b, 31-b`` - **not** as
``mb=me=b``. So to recover the original constant from a mask, compute ``b = 31 - mb``.

Verified three times independently:

- ``cue & 2`` -> ``mb=me=30``   (31-1=30, so b=1) ✓
- ``cue & 0x20`` -> ``mb=me=26`` (31-5=26, so b=5) ✓
- ``cue & 0x10`` -> ``mb=me=27`` (31-4=27, so b=4) ✓

And in ``THangingBridge::perform``, the ROM's single ``rlwinm. r0, r4, 0, 28, 28`` means
**bit 3**, i.e. ``CUE_DRAW = 0x8``
(``libs/JSystem/include/JSystem/JDrama/JDRViewObj.hpp:16``) - which is obviously right,
since the guarded body calls ``initDraw()`` and then draws.

The old text had it backwards: ``0x10000000`` is bit 28, and bit 28 correctly emits
``mb=me=3``. So writing ``0x10000000`` was never "the honest mask" - it was a **wrong**
mask that also gated drawing on an unrelated cue bit, i.e. a runtime bug, not merely a
match defect.

**Rule: a mask mismatch is never evidence that the constant is unknowable.** Read ``mb``,
invert it with ``31 - mb``, then look the bit up in the cue enum. Only reject a constant
as fakeable if the inverted bit genuinely has no meaning in that enum.

## The INVERSE lever: an ``inline`` keyword the ROM does not have

The ``#pragma dont_inline`` work above is one half. The other half is a ``static`` helper
that the ROM **inlines at every call site**, where MWCC sees a multi-call static and emits
one out-of-line copy.

That copy is an **``extra`` symbol**: it sits in our ``.text`` denominator with no ROM
counterpart, so it scores 0 no matter how correct it is. Measured in
``src/MarioUtil/PacketUtil.cpp``:

| helper | extra bytes |
| --- | --- |
| ``FifoSetChanMatColor`` | 72 |
| ``FifoSetTevColorS10`` | 96 |
| ``FifoSetTevKColor`` | 88 |

Adding ``inline`` to all three deleted 256 B of extra text and took the unit from
**66.36 % -> 82.13 %**. One keyword, three declarations.

**How to tell the two cases apart:**

- ROM has a real ``bl`` at the call site, ours has none -> you need
  ``#pragma dont_inline``.
- ROM has **no symbol at all** for the function, ours emits one -> you need ``inline``.

Both are the same underlying fact - a missing ``bl`` - but the fix is opposite, so check
which side has the symbol *before* touching anything. List your unit's symbols with
``python tools/decomp-diff.py -u <unit> -t function`` and read the ``extra`` rows.

## ``#pragma dont_inline`` is ``.cpp``-only, and a file-scope region is actively harmful

Confirmed again from the other direction. Two distinct failures:

1. **In a header it is simply ignored** for ``inline`` class members already parsed.
2. **Wrapping a whole ``#include`` block at file scope de-inlines unrelated code.**
   Measured: ``#pragma dont_inline on`` around the include block in
   ``src/Animal/BeeHive.cpp`` collapsed ``TBeeHiveManager::load`` from **100 % to 11.4 %**
   and ``createRealoidActor`` from 100 % to 78.6 %, net **-2.5 pp**.

Keep the region as narrow as the call sites that need it. TU-local wrappers under a tight
``dont_inline on``/``off`` region are the safe form - that alone was +0.9 pp in BeeHive and
it recovered the whole register allocation by fixing the frame from ``-0x108`` to
``-0xc0``.

When a pragma is "all or nothing", scope it to the call sites instead of the function:
keep the function inlinable and route only the sites that need a real call through a
TU-local wrapper, giving exactly as many relocations as the ROM has.

``THangingBridge::perform`` went **97.7 % -> 98.8 %** on one token. The loop bound is a
``u32`` compared against a loop counter:

| source | emitted | match |
| --- | --- | --- |
| ``for (int i = 0; i < unk10; ++i)`` | ``cmplw`` | 98.0 % |
| ``for (u32 i = 0; i < unk10; ++i)`` | ``cmplw`` | 98.0 % |
| ``for (int i = 0; i < (int)unk10; ++i)`` | ``cmpw`` | **98.8 %** |

The **explicit ``(int)`` cast on the bound** is what does it - not the type of the loop
variable, which made no difference. Without a cast MWCC promotes the unsigned field to
64-bit and emits ``cmplw``; the cast forces a 32-bit signed comparison. Every other
instruction - frame, spills, loop structure, both ``drawOneRope`` calls - was already
exact.

## Frame padding: it works, but only where the target has unreferenced spill space

``char framePad_N_fnName[N]; (void)framePad_N_fnName;`` inside the function body does
move the frame. In ``THangingBridge::perform`` it took the frame from ``0x30`` to the
ROM's ``0x50`` and pulled every local slot up to its target offset (97.7 % -> 98.0 %). The
same idiom is already in ``loadAfter`` in ``src/Enemy/BathtubKiller.cpp``.

Caveat: ``TSwingBoard::drawOneRope`` in this same file sits 8 bytes short and the pad does
**not** move it - some frames are governed by something else. Always measure; a pad that
does nothing is a one-line revert.

**Open contradiction to resolve:** ``AGENTS.md`` forbids committing a ``framePad``, while
this section says "measure it and keep it if it helps", and the coordinator has used one.
Until that is settled, treat a measured pad as acceptable but call it out in your report.

## MWCC 1.2.5e float-comparison lowering (verified from raw BO/BI, worth ~5 pp)

Derived by decoding the **branch opcode bytes**, not the mnemonics. As the *skip* branch
after ``fcmpo cr0,a,b``:

| expression | branch | opcode |
| --- | --- | --- |
| ``a < b`` | ``bge`` | ``0x4080`` |
| ``a > b`` | ``ble`` | ``0x4081`` |
| ``a <= b`` | ``cror eq,lt,eq`` + ``beq``/``bne`` | |
| ``a >= b`` | ``cror eq,gt,eq`` + ``beq``/``bne`` | |
| ``!(a < b)`` | ``blt`` | ``0x4180`` |
| ``!(a > b)`` | ``bgt`` | ``0x4181`` |

Using ``>`` instead of ``>=`` (or ``!(<)``) was worth **~5 pp in
``TMapObjGrowTree::control`` alone**.

**Critical caveat:** the ``.s`` listing prints ``ble`` and ``bge`` for the same BI
inconsistently. **Read the bytes, not the mnemonic.** Use
``powerpc-eabi-objdump.exe -d`` or the raw hex in the comment column.

## ``(f32)(s16)`` goes through a double

``xoris r0,rV,0x8000`` / ``lis r0,0x4330`` / 8-byte stack slot / ``lfd`` / ``fsubs`` against
the ``0x4330000080000000`` bias. Writing
``(f32)mMActor->getFrameCtrl(0)->getEnd()`` reproduces **9 instructions byte-for-byte**.

An agent tried to derive this arithmetically first and failed - **the source form is the
only reliable route**. Do not spend time on the arithmetic.

## GX enum values in this repo's header are REVERSED from retail SDK

``GX_LINES = 0xA8`` and ``GX_TRIANGLESTRIP = 0x98`` here - the reverse of retail. Two agents
found this independently by different routes. If a GX immediate looks wrong, check this
before assuming a logic error.

## "More code" is usually better - a retracted warning

An earlier note in this file claimed that implementing an empty ``// TODO`` stub could
make a function *worse*, citing ``MapObjWave::updateHeightAndAlpha`` 99.5 % -> 95.2 %.
**That measurement was misattributed.** The empty stub actually scored **0.5 %**; the
implemented version scores **100 %**.

What *is* real, and what caused the real regression elsewhere, is that **dead code which
carries a branch is load-bearing**. In ``TRiccoWatermill::getHeight`` the ROM contains a
self-assigning ``else result = result;`` whose only purpose is to make the compiler emit an
extra unconditional branch ahead of the "not water" arm. Removing it costs real
percentage. Keep it, with a comment saying why.

Three independent confirmations of the same rule this session:

- ``MapObjWave::getHeight`` - a self-assigning ``else`` produces a required branch
- ``BossHanachanMain::bind`` - a discarded ``TVec3::sub`` whose out-of-line call forces
  operand materialisation worth ~5 %
- ``MapObjWave::load`` - the amplitude pair is stored inside the ``case`` *and* after the
  switch; the in-case store recovers 8 instructions

And two counter-examples where a natural-looking rewrite is a measured regression:

- ``mVelocity.set(...)`` replaced by component-wise stores: **-1.05 pp**, twice
- spelling a rotated ``rotate()`` out by hand to match the ROM's folded constants:
  -0.76 pp (MWCC folds the literal pair away)

## Data-section mismatches are often return-type errors

``MapObjWave::getAlpha`` was 64 B where the map says 96 B - not a layout bug but a
**return type**: retail returns ``s32``, we returned ``u8``. Changing the signature and
dropping the ``static_cast<u8>`` at the two ``GXColor4u8`` call sites produced a 0x60 (96 B)
symbol. Upstream's PR header confirms the same change.

Check the return type before chasing a size mismatch.

## Prefer the .sdata2 listing in the .s over reading the DOL directly

The DOL file-offset -> address mapping (``0x3DEDC0`` -> ``0x80406280``) is correct for
``.sdata2``, but deriving the ``sda21`` base from a named ``.sdata`` symbol goes wrong - it
produced ``75.0f`` where ``@3095`` is ``0.0f``. **Read the ``.sdata2`` listing at the end
of ``build/GMSP01/asm/<Path>/<Unit>.s`` first**; fall back to the DOL only for constants
that are not there.

## Register allocation: ``r29`` vs ``r30`` is last priority

After all of the above, ``THangingBridge::perform`` is instruction-for-instruction
identical to the ROM except that the loop counter lives in ``r30`` where the ROM uses
``r29``. Per this project's own priorities register choice is explicitly last: stop at
98.8 % and take it.

## Reading the ORIGINAL ROM's float constants: map the DOL, do not guess

Functions that index a table or use unnamed floats are unreadable until you have the
real bytes. The ``.sdata2`` dump at the tail of ``build/GMSP01/asm/<Path>/<Unit>.s`` often
does not have them. The authoritative source is the original ROM image itself:

**``build/GMSP01/mario.dol`` is the ORIGINAL retail DOL, not our build.** Its header maps
file offsets to addresses. For ``BossHanachanMain`` the data segment at file offset
``0x3DEDC0`` maps to address ``0x80406280`` - so any ``@NNNN`` label in the asm can be
resolved by reading that byte range.

Recovered this way for one unit:

| label | value |
| --- | --- |
| ``@3153`` | 0 |
| ``@3376`` | 1 |
| ``@3377`` | 182.04445 |
| ``@3379`` | 3.8147e-06 |
| ``@3454`` | 90 |
| ``@4367`` | -90 |
| ``@4368`` | 180 |
| ``@4369`` | 360 |
| ``@4381`` | 4.5036e15 |

This is what made ``throwMario_`` writable at all. The technique is reusable across every
unit and should be preferred over inferring a constant from context.

Note ``@NNNN`` names in the asm are **label ids, not offsets** - the real bytes live in
the ``.sdata2`` dump or in the DOL data segment, never at file offset ``NNNN``.

## Dead code is sometimes mandatory - but only if it has an observable effect

Counterpart to the ``wireTrap`` case where dead stores were correctly refused. In
``TBossHanachanMain::bind`` the ROM calls the out-of-line ``TVec3::sub`` **three times
and discards the first result**. Leaving the obviously-dead ``unk17C - unk17C`` out cost
**5 %** (77.6 -> 82.4 when written as a real discarded ``TVec3::sub``).

The distinction that matters:

- **Refuse** when the dead value is stored to a slot that is *never read anywhere in the
  target* and has no call side effect - writing it literally is a fakematch.
- **Keep** when the dead expression is a **call** whose effect is visible in the frame
  layout or register allocation. The out-of-line call forces the operand materialisation
  that the following two live calls then reuse.

A `wireTrap` agent hit the first case and correctly left ``checkHitActors`` nonmatching.
Both decisions are right; the test is whether the computation has an observable effect.

## Shared-header ``inline`` removal is a separate, measured project

Three agents independently hit this wall and all three were right to stop:

- ``MsWrap<f32>`` (72 B) - needs ``inline`` removed from ``MarioUtil/MathUtil.hpp``
- ``SMS_CalcToDirMatrix`` - solved *inside* one TU by de-inlining and defining in the ``.cpp``
- ``TRotation3::getQuat`` / ``TQuat4::slerp`` / ``TVec3::set<f>`` / ``TUtil::inv_sqrt``

An explicit ``template <>`` plus ``#pragma dont_inline`` does **not** substitute for
removing ``inline`` from the header - the symbol stays ``missing``. When the fix lands in
a header included by every TU, do not land it from one unit: measure it as its own
change, with a known-good unit as the witness.

A review raised the worry that ``mState`` (0x64) might be wrong globally because
``MapObjMonte`` needed ``mMap`` (0x7C) instead. **That hypothesis is refuted.** Evidence:

- The ROM reads **both**: ``lbz <reg>, 0x7c(r3)`` appears 7x in ``MapObjMonte.s``, and
  ``lbz <reg>, 0x64(...)`` appears 30x across 9 TUs (``GCConsole2`` 13, ``MarDirectorDirect``
  7, ``ConsoleStr`` 3, ``cannon`` 2, ``cameragc``/``BossHanachanEffect``/
  ``BossHanachanMain``/``PauseMenu2``/``MapEventSirena`` 1 each).
- Usage in our own source is consistent with the existing names: the 23 ``mState`` sites
  compare against **game-state** enums (``STATE_PAUSE_MENU``, ``STATE_CARD_SAVE``, ``1``,
  ``4``, ``5``), while the 76 ``mMap`` sites compare against **map/shine IDs** (0, 3, 7,
  0xF, 0x14, 0x2C, 0x37, 0x39, 58, 59).
- Both are ``u8``, and **the name has no effect on codegen** - only the offset does. So
  there is no global header fix available here, and no header edit to make.

A read of 0x7C where our source said ``mState`` is therefore always a **per-site bug**,
to be fixed by reading the asm at that one call site, not by renaming anything.

Grepping the asm for ``0x7c(`` alone returns almost entirely **stack** spill slots
(``0x7c(r1)``). Only byte loads off a register holding a freshly loaded
``gpMarDirector`` are evidence - that is, ``lwz r3, gpMarDirector@sda21(r0)`` immediately
followed by ``lbz r0, 0x7c(r3)``.

## A virtual ``{ }`` destructor in the header can DELETE the class vtable (measured)

This is the **opposite polarity** to the ``dont_inline`` finding above, and it is a trap:
"cleaning up" an empty virtual destructor can cost far more than it saves.

Measured on ``TBathtubGripParts`` in ``MapObjCorona`` - three variants, all built:

| variant | ``.text`` | ``__vt__17TBathtubGripParts`` | ``@32@__dt__..`` | validator |
| --- | --- | --- | --- | --- |
| no destructor (out-of-line removed) | 57.99 % | present (wrong ref) | **missing** | 2 missing syms |
| **out-of-line in ``.cpp``** (correct) | **58.95 %** | present | present | 1 linkage err |
| ``virtual ~T() { }`` in header | 58.77 % | **DROPPED** | **DROPPED** | 2 missing syms |

Declaring the destructor inline in the header fixes the weak-vs-global *binding* error -
the map says ``__dt__17TBathtubGripPartsFv`` is ``weak``, i.e. a header inline - but MWCC
then files the class vtable in a virtual-table group and **never emits it**, because the
class is never constructed in that TU. Cost: **252 B of ``.data``/``.ctors``**, the 8 B
thunk, and a broken link. Out-of-line wins on score *and* on linkability; the single
weak-vs-global binding error is build-time metadata only, with identical linked bytes.

Practical rule:

- **Empty body the ROM calls out-of-line** (a stub whose caller does a real ``bl``) -> you
  want ``#pragma dont_inline`` in the ``.cpp`` so the ``bl`` survives.
- **Empty ``virtual ~T() { }`` in a header** -> you may be deleting a whole vtable. Leave
  it out-of-line unless you have measured otherwise.
- To check, after building: ``Select-String build/GMSP01/asm/<Path>/<Unit>.s -Pattern '__vt__<Class>'``.
  If the ROM references the vtable and your build does not emit it, you have this bug.
- One cheap type check first: does *any* virtual of that class appear **out-of-line**? A
  single out-of-line virtual is what makes MWCC emit the vtable - see the comment already
  on ``virtual ~TBathtub();`` in ``include/MoveBG/MapObjCorona.hpp``.

## The `#pragma dont_inline` lever, and how to find it (measured 2026-09-30)

`TWoodBlock::load` went **44.5 % -> 100 %** with a single `#pragma dont_inline`. MWCC was
inlining `TRailMapObj::load` into it, dragging a `char buffer[256]` along: the caller's
frame was `0x160` instead of `0x60`. The ROM keeps a real symbol and emits one `bl`
(marioEU.MAP 801E7780). `TBaseNPC::changeNerveProc_` went **84.5 % -> 99.2 %** the same way
on `isNerveCanGoToTalk`.

So the class to hunt is not "our code differs" but **"the ROM calls this symbol with `bl`
and our build never calls it at all"** - i.e. we inline it at every site.

Detector (throwaway, ~15 min over 285 units, one `objdump -dr` pass per unit):

- symbols the ROM `.s` calls with `bl`, minus
- symbols our object has an `R_PPC_REL24` relocation against, **restricted to symbols the
  same TU defines on both sides**.

That last filter is what makes it usable: without it you get 833 hits, almost all
`JGadget::TList::iterator` and `TFlagT<u16>` constructor noise. With it, **3 hits over the
whole clean tree**, and the two same-TU ones were both real wins:

    mario/Map/MapEventSink    bl loadAfter__24TMapEventSinkInPollutionFv
    mario/NPC/NpcChange       bl isNerveCanGoToTalk__8TBaseNPCCFv
    mario/Camera/CameraMode   bl isNormalCameraSpecifyMode__15CPolarSubCameraCFi

Gotchas:

- The first one does work, but only once the **duplicate loop is removed first**.
  `TMapEventSinkBianco::loadAfter` had *two* loops: a `registerPollutionObj` pass that
  belongs to the base `loadAfter`, and the real `alive()`/`kill()` pass. With the
  duplicate still there the diff stayed at 75.3 % and the pragma looked inert - the
  inlined base body simply buried the evidence. Remove the loop (75.7 -> 75.3 is
  noise), add the pragma (-> 99.2), then a `framePad_88` for the frame (-> 99.4).
  **Always check the relocations, not the percentage, to tell whether a pragma bit.**
- The third is called from *other* TUs (`CameraBGCheck`, `CameraChange`), so the
  definition must be in a header - and `#pragma dont_inline` is ignored for a header body
  (see the note in `MapObjBase.hpp`). That one is genuinely out of reach.
- `objdiff` can match two different calls as one line when both are `load`-family, which
  is how this hid for so long. Always read the raw relocations with
  `powerpc-eabi-objdump.exe -dr` before believing a diff line that "matches".

### The `dont_inline` lever is all-or-nothing: it cannot express "inline here, not there"

The third detector hit, `CPolarSubCamera::isNormalCameraCompletely`, is a **false
positive for this lever** and is worth understanding:

    mario/Camera/CameraMode.cpp:193
    if (isNormalCameraSpecifyMode(mMode)
        && (!isNowInbetween() || isNormalCameraSpecifyMode(mPrevMode)))

The ROM calls it **out of line for `mMode`** (`bl` at 1d0) and **inlines the switch for
`mPrevMode`** (jump table @1895 at 1fc). Our MWCC inlines both; the ROM's build inlines
only the second. `#pragma dont_inline` is per *definition*, so it forces both out of line:

    no pragma                72.9 %  (45 instructions, callee already 100 %)
    #pragma dont_inline      70.3 %  (37 instructions - now 8 short of the target)

`isNormalCameraSpecifyMode` was **already a 100 % symbol either way**, so the pragma buys
nothing and costs 2.6 points. Reverted. The call-site-dependent inline budget that the
`isReachedToGoal` section describes is not expressible from the source.

### ``TMameGesso::reset`` (48.2 %) - resolved as blocked, with the constants

The constants are readable in the ROM `.s` `.sdata2` section, which settles it:

    @3757 = .float    0.000030517578   (= 1/32768)
    @3758 = .float    20               (the mPosition.y offset)
    @3760 = .double   4503601774854144 (= 2^31)

The prologue is then fully readable: `params->unk3B4` is negated (`subf r30, r3, r0`
with r3 = 0), `rand()` is called, **two** `xoris rD, rS, 0x8000` produce the bit patterns
for `(f32)rand()` and `(f32)(-unk3B4)`, both are spilled at 0x3c and 0x34 - and **neither
is ever read**. The live expression is built from two `lfd`s that concatenate a spilled
float with the `0x43300000` word (`lis r5, 0x4330`), i.e. the `2^52 + x` idiom, minus 2^31
(`lfd f3, @3760`), multiplied by 1/32768, and truncated with `fctiwz`; the high word of
the resulting double is then read back as the int (`lwz r3, 0x2c(r1)` after
`stfd f0, 0x28(r1)`).

Our load of 0x3B4 is therefore genuinely dead, and the previous agent's TODO ("still don't
know the real rand function/class") was correct. Same artefact family as
``TRocket::setDeadAnm``: **two dead `xoris` writes and no source spelling reproduces
them.** Do not re-attempt without the real `MsRandF` definition.

``TTelesa::isReachedToGoal`` (97.3 %) is a **frame-shrink** case - 0x20 target vs 0x28
ours - which `framePad_NN` cannot fix, and the one missing instruction is the ROM hoisting
`addi r4, r3, 0x104` into the prologue.

## MEASURED: `TFireWanwanTailNode::perform` 37,0 % -> 97,3 % by inlining the dir matrix (2026-10-01)

`Enemy/fireWanwan.cpp`, `TFireWanwanTailNode::perform` (496 B, 124 instructions).
The whole gain came from replacing one call with three lines of real code.

**The call was in the wrong function.** The old body was

```cpp
SMS_CalcToDirMatrix(mtx, param_4, JGeometry::TVec3<f32>(0.0f, 1.0f, 0.0f));
mtx.setTrans(param_3);
```

but the ROM's `perform` makes exactly **two** `bl`s - `PSMTXCopy` and
`MActor::perform` (`fireWanwan.s` 0x80086780-0x8008696C). Everything else is
inline arithmetic. `SMS_CalcToDirMatrix` is genuinely called from
`TFireWanwanTailHit::perform` in the same TU, which is why the mistake survived
so long: the symbol resolves, the code looks plausible, and `artifacts/frame_hole.py`
correctly reported "holes 0x7c vs 0x50" without explaining why.

**Why `SMS_CalcToDirMatrix` cannot match even inlined.** It normalises
`param_2` *first*, then crosses twice. The ROM does not touch `param_4` at
all - it only normalises the two cross products:

```cpp
JGeometry::TVec3<f32> v1;
v1.cross(JGeometry::TVec3<f32>(0.0f, 1.0f, 0.0f), param_4);
v1.normalize();

JGeometry::TVec3<f32> v2;
v2.cross(param_4, v1);
v2.normalize();

mtx.setXDir(v1);
mtx.setYDir(v2);
mtx.setZDir(param_4);   // param_4 is NOT normalised - this is the tell
mtx.setTrans(param_3);
```

Decoding the six `fmsubs`/`fnmsubs` (`fireWanwan.s` 0x800867E4-0x80086890)
against `JGVec3::cross` (`JGVec3.hpp:252`) pins the operand order exactly:

* the constant vector is `b = (f29, f2, f29) = (0.0f, 1.0f, 0.0f)`, and the
  *first* cross is `cross(up, param_4)`, not `cross(param_4, up)`;
* `_x -> f30`, `_y -> f28`, `_z -> f27` - the register that receives `_x`
  is the one that survives into the next block's `b = (f31, f30, f29)`, which
  is how you tell the two operand orders apart without guessing.

Two independent confirmations that this is the right shape rather than a lucky
fit:

* the surviving `fmuls f0, f0, f1` right after each `bl inv_sqrt` is
  `TUtil<f32>::one()` coming out of `setLength` (`JGVec3.hpp:328`), whose
  `scale(length * inv_sqrt(lsq), v)` is never strength-reduced by MWCC. That
  instruction can only exist if `normalize()` - not `setLength(v, 1.0f)` with
  a literal - is what got inlined.
* `@3882` is loaded *twice*, once at 0x800867DC for the up vector and again
  after each `inv_sqrt` call. Two distinct load sites for 1.0f mean two
  distinct `TUtil<f32>::one()` expansions, i.e. two `normalize()` calls.

**What is left (2,7 %) is not addressable from the source.** Frame is 0x120
(ROM) vs 0xf0 (ours), the `TPosition3f` sits at `+0xa4` vs `+0x78`, and the
callee-saved FP registers are rotated (ROM allocates the 0.0f constant to f29
and the three cross components to f28/f30/f27; ours picks f31 and f29/f28/f30).
Both are MWCC allocator artifacts:

* **reordering the locals** (declaring `v1`/`v2` before `mtx` so their
  SRA'd slots would sit below the matrix) changes *nothing* - frame stays 0xf0,
  matrix stays at 0x78. The gap is not a local-slot reservation.
* `artifacts/frame_hole.py` had already ruled out `framePad_NN`: the ROM uses
  164 bytes of its local area for nothing at all below the matrix.

So: instruction-for-instruction identical, size identical, only the allocator
disagrees. Same shape as `TTailRubber::restrict` (83,8 %), where the frame
*does* match and the entire gap is the same FP rotation.

**Corollary for the scan.** A near-match with a large frame gap is usually
*not* a frame problem - check whether the instruction *kinds* match first. Here
both objects had 6 `fmsubs`, 2 `inv_sqrt` and 1 `bl`, which is why
`frame_hole.py` said "local sets differ" when the truth was "right shape,
wrong allocation".

## READ THE COLUMNS RIGHT: LEFT is the ROM, RIGHT is ours (verified 2026-10-01)

`tools/decomp-diff.py` prints `OFFSET | LEFT | RIGHT`, and **LEFT is the target
(the ROM)**, RIGHT is our object. The markers are the ones `why-not-100.py`
documents (` ` exact, `~` same opcode other operands, `|` other opcode,
`<` target only, `>` ours only) - but for a one-sided instruction objdiff
prints the text in the LEFT column whatever the side it came from, so the
marker, not the column, is what tells you the side. Check it once against the
ROM and stop guessing:

```
$ sed -n "/\.fn getNowGravity__16TGessoPolluteObjFv/,/endfn/p" \
      build/GMSP01/asm/Enemy/gesso.s
/* 80046D90 00043CD0  80 83 01 E8 */	lwz r4, 0x1e8(r3)     <- the ROM uses r4
```
and the diff had `~ lwz {r4}, ... | lwz {r3}, ...`: LEFT r4 = ROM, RIGHT r3 =
ours. Reading it the other way round sends you chasing the wrong register.

## `cmpwi` vs `cmplwi`: the signedness of the DECLARATION picks the opcode

`TCardManager::getOptionWriteStream` went **99.1 % -> 100 %** on one word, and
the whole diff was a single `|` line:

```
| cmplwi r30, 0 | cmpwi r30, 0      <- ROM unsigned, ours signed
```

The value is `int iVar8 = TFlagManager::getInstance()->getFlag(0xA0001);`
tested with `if (iVar8 == 0)`. Declaring it `u32` fixes it:

| source | emitted |
| --- | --- |
| `int iVar8 = ...getFlag(0xA0001); if (iVar8 == 0)` | `cmpwi r30, 0` (99.1 %) |
| `u32 iVar8 = ...getFlag(0xA0001); if (iVar8 == 0)` | `cmplwi r30, 0` (**100 %**) |

No cast needed on the *comparison* here (contrast the `cmpw`/`cmplw` section
above, where the cast had to be on the bound): when the compared value is
itself the local, its declared type is enough. `buildHeader_` is inlined into
four functions of that unit and none of them regressed.

The same family, opposite direction, in the two other hits of the scan - there
we are the unsigned one, so the fix is a cast to `s32`:

| function | ROM | ours | fix | result |
| --- | --- | --- | --- | --- |
| `TMario::checkDescent` | `cmpwi r0, 1` | `cmplwi r0, 1` | `if ((s32)active != 1)` instead of `active != true` | 99.2 -> **99.8 %**, zero markers left |
| `TLeafBoat::control` | `cmpwi r0, 0` | `cmplwi r0, 0` | `if ((s32)gun->mIsEmitWater > 0)` on a `u8` field | 99.3 -> **99.9 %**, zero markers left |

Note the pattern in both: `bool != true` and `u8 > 0` are the two forms where
MWCC picks the *unsigned* opcode on its own, because the constant is 0 or 1 and
the unsigned form is cheaper. Adding the cast restores the signed compare. Also
note that a function can reach **zero diff markers and still report 99.8 %** -
the fuzzy percentage is not a marker count, so "no markers" is the stronger
statement of "done".

**How to find the family.** Any diff containing a `|` line whose two opcodes
are `{cmpwi,cmplw}` and `{cmplwi,cmplw}` is this bug, and it is always
fixable: find the local, change its declared type or add a cast.
`tools/tmp_signed.py` enumerates them over every non-matching function of every
clean unit. **Measured 2026-10-01: the family is nearly exhausted** - 2 hits in
the first 400 non-matching functions of clean units, and both were single-
marker functions (the one in `TLeafBoat::control` looked like 17 mismatches
only because the scan counts `~` lines as "others"; read the real diff).

## ONE predicate can need TWO spellings: `isWaterSurface`

`TPakkunSeed::rebirth` went **98.4 % -> 99.9 %** by rewriting a single call -
and the obvious version of that rewrite **broke four other functions**. Both
facts are worth keeping.

`TBGCheckData::isWaterSurface()` tests seven `mBGType` values, and 0x100..0x105
are contiguous, so there are two natural spellings:

| spelling | ROM code |
| --- | --- |
| seven `==` | `cmplwi r3, 0x100 / beq` then `subi 0x102 / cmplwi 3 / ble` ... |
| `v == lo-1 \|\| (u16)(v - lo) <= hi-lo \|\| v == 0x4104` | `cmplwi r3, 0x100 / beq`, then `subi r0, r3, 0x101 / clrlwi r0, r0, 16 / cmplwi r0, 4 / ble`, then `cmplwi r3, 0x4104` |

The target uses the **second** shape in `rebirth` and the **first** shape
everywhere else, so the set is unchanged and only the source spelling differs.
Two details matter:

* the middle term is computed in **16-bit** arithmetic - that is what the
  `clrlwi r0, r0, 16` is. Writing `v >= lo && v <= hi` on a `u16` field is
  *not* enough: MWCC promotes to `int` and emits a plain `blt` (96.7 %). The
  explicit `(u16)(v - lo) <= (u16)(hi - lo)` is what produces the truncation,
  and it is the only spelling that reaches 99.9 %.
* both spellings must coexist. Replacing the shared `isWaterSurface()` with
  the range form cost `TNerveGessoFreeze::execute` 99.4 -> 95.4,
  `TGesso::setDeadAnm` 100 -> 96.0, `TEffectEnemy::forceKill` 100 -> 97.3 and
  `TAmenbo::bind` 100 -> 98.9. So `include/Map/MapData.hpp` now carries
  `isWaterSurface()` (seven equalities, the default) **and**
  `isWaterSurfaceRanged()` (one equality + one range, used only by
  `TPakkunSeed::rebirth`).

**Rule: before editing an inline that a shared header exposes, take a
baseline.** `python tools/decomp-diff.py -u <unit> > before.txt` over the whole
unit, rebuild, diff the two overviews. A one-function change to a header is
usually a five-function change to the build. `report.json` is only a fallback
baseline: it is whatever the last full build recorded, which may predate other
agents' edits to the same unit.

## Four more blocked cases (2026-10-01), with the reason each is not expressible

* `TCameraMapTool::TCameraMapTool(const TCameraMapTool&)` (91.7 %, 4 lines).
  The ROM copies 0x18/0x1C with `lfs/stfs` while every other member, including
  the 12-byte `TVec3` at 0x0C, is copied with `lzw/stw`. Neither a
  user-defined copy constructor on the aggregate nor two plain `f32` scalars
  changes our output: MWCC block-copies the member either way, the call to the
  member copy constructor being inlined and folded back into a move. The note
  already in `libs/JSystem/include/JSystem/JGeometry/JGVec2.hpp` ("presumably to
  force use of stfs/lfs instead of stw/lwz, but it seems like SMS didn't have
  them yet") is therefore *wrong for this class* - the retail compiler does
  expand it - but our compiler does not, and the header is off limits.
* `TMapObjSwitch::control` (92.3 %). The ROM materialises the test into a bool
  (`li r0, 1 / b / li r0, 0 / clrlwi. r0, r0, 24 / beq`) and then **re-reads**
  0x104 for the argument. Reproducing the bool needs `mStateTimer > 0 ? true :
  false` (with a plain `isStateTimerEngaged()` MWCC folds the test into a bare
  `ble` and drops four instructions), which gets the bool block and the r4
  allocation right, and then our MWCC **CSEs** the two reads of 0x104 where the
  target's does not. Same address, no intervening store: not expressible.
  Forcing a single read through a local was tried and is worse (88.2 %, the
  value lands in r0 and needs an extra `mr r4, r0`).
* `TGessoPolluteObj::getNowGravity` (98.1 %, 3 `~`). Instruction-for-
  instruction identical, the target keeps the save-params pointer in r4 and we
  reuse r3. Hoisting it into one local does not move the register, it adds a
  `bne` (85.0 %). Pure register allocation, which this project's own priority
  list puts last.
* `TNervePakkunAppear::execute` (98.6 %, 1 line). The ROM calls
  `J3DFrameCtrl::checkPass`, emits `cmpwi r3, 0` and then never branches on it:
  the next call overwrites CR0, so the compare is dead. Our MWCC deletes it.

## MEASURED: ``Enemy/fireWanwan`` inline-budget wall - four experiments, all reverted (2026-10-01)

``TNerveFireWanwanFindMario::execute`` (72,3 %, ROM 1300 B / 327 instr) and
``TNerveFireWanwanRecoverGraph::execute`` (75,7 %, ROM 1444 B) are **fully
accounted for**: the whole deficit is 83 instructions that the ROM inlines and
we call. Every other line in the diff is a frame offset or a register rename.

### The map settles the structure before you touch the source

| symbol | marioEU.MAP | ours | meaning |
|---|---|---|---|
| ``doAdjustTarget__11TFireWanwanFv`` | ``UNUSED 000298`` (l.57100) | ``T 000224`` + inlined at both nerve sites | ROM emits it *and* inlines it - we already do the same |
| ``MsGetRotFromZaxisY__FRCQ29JGeometry8TVec3<f>`` | ``weak 0000C8`` found in **bossgesso.cpp** (l.56726) | ``W 0000C8`` in fireWanwan.o | right size, **wrong TU** - ``fireWanwan.s`` has *zero* references |

The header body is byte-perfect (ours is also exactly ``0xC8``). The problem is
purely *which* copies MWCC expands.

### Where the depth boundary actually falls

Mapping every expansion in our object to its enclosing symbol:

```
0x0bc8  execute__24TNerveFireWanwanHungTail   INLINE   depth 0
0x1714  execute__28TNerveFireWanwanRecoverGraph  bl    depth 1
0x2900  execute__25TNerveFireWanwanFindMario    bl     depth 1
0x3fd8  bind__11TFireWanwan                    INLINE   depth 0
0x8048  doAdjustTarget__11TFireWanwanFv        INLINE   depth 0
```

Every **depth-0** site expands; both **depth-1** sites do not. That is exactly
the ``d=1`` row of the table at line 1661 - except the ROM expands them. So the
boundary is one notch tighter here than in the isolated probe, and ``TQuat4::
rotate`` sits behind the same wall (``bl TQuat4<float>::rotate`` where the ROM
has the ``fmadds``/``fmsubs`` arithmetic inline).

### Experiment 1 - mark ``doAdjustTarget`` ``inline``: no-op, revert

``void doAdjustTarget();`` in ``include/Enemy/FireWanwan.hpp:203`` is not
``inline``, which *looks* like the l.1425 lever. It is not:

* the out-of-line symbol **disappears** from the object;
* ``execute`` stays **968 B / 72,3 %** - byte-identical, because
  ``doAdjustTarget`` was *already* being expanded at both call sites.

So the keyword only deletes a symbol the ROM actually has (``UNUSED 0x298``).
Reverted: per l.1425 the "missing UNUSED symbol" trade is only worth it when the
ROM inlines at every site, which is not the case here.

### Experiment 2 - spell out ``MsGetRotFromZaxisY`` at the call site: regression, revert

Writing the branch chain out in ``doAdjustTarget`` *does* remove the stray
``W MsGetRotFromZaxisY`` from the object, which is the ROM's shape. But the
inliner then declines a *bigger* thing:

| build | FindMario | RecoverGraph |
|---|---|---|
| baseline | 968 B / 72,3 % | 1108 B / 75,7 % |
| matan spelled out | **708 B** / 53,3 % | **848 B** / 58,5 % |

``doAdjustTarget`` itself stops being inlined into the nerves, which loses far
more than the call it won. **Inlining budget is zero-sum inside a TU**: you
cannot buy one expansion with another. Reverted.

### What this costs you

Every attempt to buy inline expansions in a real game TU spends budget that
another inline needs. Before touching a source file, check whether the symbol
you are chasing is ``UNUSED`` in the map - if the ROM emits it *and* expands it
(like ``doAdjustTarget``), the shape is already right and the gap is the
``d``-boundary, which no spelling reaches. This is the same conclusion as
``CameraMode::isNormalCameraSpecifyMode`` at l.3279.

## A framePad declared LAST, not first (measured 2026-10-01)

The framePad idiom documented above is usually written at the top of the body.
That is wrong whenever the function has other locals, and the symptom is
*exactly* the kind of half-fix that looks like a dead end:

| `TMario::checkDescent` | frame | remaining markers | result |
| --- | --- | --- | --- |
| no pad | 0x68 | 13 `~` | 99.8 % |
| `framePad_16` **first** | 0x78 (target 0x78) | 13 `~` - saved registers fixed, locals still 0x10 low | 99.8 % |
| `framePad_16` **last** | 0x78 | 4 `~` | **100 %** |

**Why:** mwcc hands the LOW frame addresses to the LAST-declared local. A pad
declared first is therefore allocated *above* everything else and only pushes
the saved registers; declared last, it takes the bottom of the frame and pushes
every other local - and the saved registers - up by its own size, which is what
the target frame does. The other rule already documented ("a pad can only grow
a frame") is the reason the top-of-body form is harmless when the function has
no other locals: then the pad is the last-declared local anyway.

`TLeafBoat::control` (0x70 against a 0xa0 target, 16 markers, all `~`) went
**99.3 % -> 100 %** with `framePad_48_leafBoatControl`.

### The family is NOT exhausted - and the sign of the frame is a trap

A scan for "every marker is `~`" over 400 non-matching functions of clean units
returns ~37 hits, and most of them are **not** fixable, because of how easy it
is to read the frame size with the wrong sign. `stwu r1, -0xa8(r1)` reserves
0xa8 = 168 bytes; comparing the raw negative operands makes a function whose
frame is 8 bytes *too big* look 8 bytes *too small*, and the pad then makes it
worse (measured: `TGesso::setPolluteGoal` 0xb0 against a 0xa8 target, `+8` pad
sent it to 0xb8 and the percentage did not move).

So the usable filter is:

* every marker is `~` (no `|`, `<`, `>`), **and**
* `abs(ROM operand) > abs(our operand)`.

Functions with the opposite sign - our frame already bigger - are the "missing
named local" shape described in *The frame-only family* section and are not a
pad problem. Do not spend a build on them.

### Batch results, and two method traps

Thirteen functions patched in one pass (pad declared last, size = the measured
delta):

| function | before | after |
| --- | --- | --- |
| `TTelesaBlock::perform` | 99.7 % | **100 %** |
| `TObjHitCheck::entryGroup` | 99.7 % | 99.9 % |
| `TGesso::calcRootMatrix` | 99.7 % | 99.9 % |
| `TRealoid::loadDefault` | 99.7 % | 99.9 % |
| `TBaseNPC::emitHappyEffect_` | 99.7 % | 99.9 % |
| `TGessoPolluteObj::set` | 99.7 % | 99.8 % |
| `TQuestionManager::makeDL` | 99.5 % | 99.7 % |
| `TItemManager::newAndRegisterCoin` | 99.5 % | 99.6 % |
| `TPoiHana::walkBehavior` | 99.3 % | 99.5 % |
| `TMapWire::getPosInWire` | 99.3 % | 99.4 % |
| `TBellDolpic::ring` | 99.2 % | 99.3 % |
| `TEnemyManager::createEnemies` | 98.8 % | 98.8 % (frame fixed, residue is register choice) |

So the pad reliably fixes the *frame*; what is left after it is either a 4-byte
local-layout quirk (`entryGroup`: every local ends up 4 bytes too high) or pure
register allocation, which this project ranks last. Expect +0.1 to +0.2 % on
average and one function in a dozen reaching 100 %.

**Trap 1 - `report.json` is not a baseline.** Comparing a unit's functions
against `build/GMSP01/report.json` after a change shows three "regressions" in
`gesso` and `poihana` that turn out to be pre-existing drift: with the file
pristine and rebuilt, `TGessoManager::initSetEnemies` is *already* 81.2 % where
the report says 81.43 %. The report is a snapshot of the last full build, which
predates other agents' header edits in a 433-file shared worktree. Take a fresh
baseline instead: `decomp-diff -u <unit> > before.txt`, patch, rebuild,
`decomp-diff -u <unit> > after.txt`, `diff` the two. With a real baseline the
same thirteen patches showed **zero** regressions.

**Trap 2 - a pad is local, the inline budget is not.** Adding a local to one
function can move another function in the same translation unit, because the
inliner works per TU (see *THE INLINE-BUDGET MECHANISM*). This is a second
reason to diff the whole unit and not just the function you patched.

### Second batch: 15 more functions, 6 gains, 0 regressions

| function | before | after |
| --- | --- | --- |
| `TGraphGroup::initGraphGroup` | 99.8 % | **100 %** |
| `TNerveTobiPukuAttack::execute` | 99.7 % | **100 %** |
| `TMapObjBase::joinToGroup` | 99.6 % | 99.8 % |
| `TElecNokonoko::behaveToFindMario` | 99.8 % | 99.9 % |
| `TElecNokonoko::init` | 99.7 % | 99.8 % |
| `MSBgmXFade::xFadeBgm` | 99.4 % | 99.6 % |

Nine of the fifteen pads changed nothing measurable, and they split into two
shapes worth telling apart:

* **a 4-byte hole** (`evCheckMonteClear` ROM 0x4c against ours 0x48,
  `initAndRegister` 0x40/0x3c, `isTouchedOneWall` 0x20/0x1c): the frame size
  and the saved registers now match, and every *local* is 4 bytes low. The
  target has a 4-byte hole in the middle of its frame, which a single pad
  cannot express - it needs a 4-byte local declared between the other locals,
  i.e. real source reconstruction, not padding.
* **frame governed by something else** (`getMonteVillageActorArea` 0x20/0x18,
  `MSound::exitStage`): the pad did not move the locals at all.

So the pad fixes the frame in roughly two thirds of the family, and the residue
is either a 4-byte hole or a frame whose size is not governed by this
function's own locals.

### Third batch: 12 pads, 9 gains, 4 of them to 100 %, and one instructive revert

| function | before | after |
| --- | --- | --- |
| `TDrawSyncManager::threadFunc` | 99.9 % | **100 %** |
| `TCameraOption::TCameraOption` | 99.9 % | **100 %** |
| `TMario::canSleep` | 99.9 % | **100 %** |
| `SMSSetupMovieRenderingInfo` | 99.9 % | **100 %** |
| `TMenuBase::perform` | 99.7 % | 99.9 % |
| `TMapObjTree::initMapObj` | 99.8 % | 99.9 % |
| `TMapCollisionWarp::setUp` | 99.8 % | 99.9 % |
| `TWoodBarrel::kill` | 99.7 % | 99.8 % |
| `TMario::soundTorocco` | 99.6 % | 99.7 % |

The tenth pad, `TSpineEnemy::calcTurnSpeedToReach`, **gained nothing at all** and
cost its neighbour `TSpineEnemy::walkToCurPathNode` 0.09 % through the TU inline
budget, so it was reverted. That is the shape to watch for: a pad whose own
percentage does not move is not free, because the inliner reacts to the extra
local even when the frame it belongs to was never the problem. Measure the
patched function, and if it did not move, revert rather than keep it.

**Still unpatched and worth a look:** `MActorAnmDataEach<T>::loadAnmPtrArray`
is one template whose six instantiations all want `pad=8` (0x238 against 0x230),
so a single pad in `include/M3DUtil/MActorData.hpp` would fix six functions at
once. It is a shared header included by ten translation units, so it needs the
full-unit measurement of every includer, not a single build.

### DONE: that one template pad was worth six functions (2026-10-01)

All six `MActorAnmDataEach<T>::loadAnmPtrArray` instantiations live in a single
unit (`mario/M3DUtil/MActorData`; check with a report.json scan before assuming
a shared header is risky), and one `framePad_8_loadAnmPtrArray` took **all six
from 99.8 % to 100 %**. The unit went from 9 non-matching functions to 3, with
no regression.

**Find these before patching one function at a time.** The mangled name encodes
the template argument length, so the instantiations do NOT share a prefix:
`loadAnmPtrArray__35MActorAnmDataEach<...>`, `__36`, `__37`, `__39`, `__40`.
Grouping on `name.split('<')[0]` finds nothing; strip the number first:

```python
base = re.sub(r'__\d+', '__', name.split('<')[0])
```

Project-wide there are only **two** groups of three or more non-matching
instantiations - this one and `TNameRefAryT<T>::load`.

### The second group is a trap: one pad, four functions, three of them worse shape

`TNameRefAryT<T>::load` (4 instantiations, all pure `~`, all wanting `pad=8`)
looked like the same deal. It is not:

* three of them went 99.8 % -> 99.9 % and stopped: the frame now matches but
  every local sits 4 bytes low (the 4-byte hole above);
* the fourth, `TStageEventInfo`, already had a frame 8 bytes **too big**, so the
  shared pad pushed it the wrong way: 99.89 % -> 99.8 %, 14 markers -> 19.

Net +0.2 % across four functions, none reaching 100 %, on a header included by
most of the project. Reverted. **A pad in a template only works if every
instantiation is short by the same amount** - check each one, because the
template is one edit but the frames are per instantiation.

## ``artifacts/classify_deficits.py`` - what the remaining 2209 functions actually are (2026-10-01)

Percentages lie about where the work is. This tool disassembles both sides once
per unit and buckets every function below 100% by what differs:

    python artifacts/classify_deficits.py
    python artifacts/classify_deficits.py --bucket stub --top 25
    python artifacts/classify_deficits.py --csv

| bucket | meaning | count | bytes at stake |
|---|---|---|---|
| ``call_shape`` | size within 5%, different mnemonic multiset (usually a call the ROM expands inline) | 992 | 53 279 |
| ``stub`` | our body < 35% of the ROM's - **not written** | **45** | **40 474** |
| ``structural`` | size differs by > 5% | 143 | 28 322 |
| ``oversize`` | our body > 5% bigger than the ROM's | 84 | 14 714 |
| ``alloc_only`` | **identical size AND identical mnemonic multiset** | **945** | **3 958** |

**The headline: 43% of the deficit (945 functions) is worth 4 kB.** Those are
pure register-allocation / frame-offset differences - ``alloc_only`` is
byte-for-byte the same instruction mix, only the registers and ``r1`` offsets
are permuted. **No source change can move them.** The earlier conclusion that
``TTailRubber::restrict`` was one of them was simply wrong (see below), so do
not trust a hand analysis of one function: let the tool bucket it first.

Conversely the real prize is small and concentrated: **45 stub functions hold
40 kB**, more than the entire ``alloc_only`` bucket by a factor of ten. They
cluster in just six TUs - ``MoveBG/MapObjCorona`` (6 stubs), ``MoveBG/MapObjMonte``
(6), ``Enemy/chuuhana`` (3), ``Enemy/koopajr`` (2), ``Enemy/wireTrap`` (2),
``Enemy/bosswanwan`` (2).

**Self-validation.** The tool was checked against the four functions already
diagnosed by hand: ``TFireWanwanTailNode::perform`` -> ``alloc_only`` (correct,
it is a pure allocation gap), ``bosstelesa::loadAfter`` and
``FindMario::execute`` -> ``structural`` (correct), and ``restrict`` ->
``oversize`` (correct, and the reason it found the bug below).

### Caveat: ``report.json`` is stale between full builds

``pct`` comes from ``report.json`` (last full build); ``ours`` is read live from
``.o`` files. After a partial rebuild the percentages lag. Sizes and buckets are
still live, so trust the bucket column over the percentage.

---

## ``TTailRubber::restrict`` 83,8 % -> 87,5 %: a duplicated ``length()``

Hand analysis had concluded this function was a pure allocation gap, because the
frame matches exactly (``stwu r1, -0xa8`` on both sides). That reasoning was
wrong: **a matching frame says nothing about the body size.** The object was
1256 B against the ROM's 1140 B - 29 instructions *too many*, which the frame
check could never have revealed.

``artifacts/classify_deficits.py`` bucketed it ``oversize`` on its first run and
the diff then showed 35 ours-only instructions in two symmetric blocks of
``squared()`` + inline ``sqrt``. The source had:

```cpp
if (avgHorLen < diff.length()) {         // computed once...
    diff.setLength(diff.length() - avgHorLen);   // ...and AGAIN here
    it->mPos += diff;
}
```

The ROM evaluates ``length()`` once and reuses it. Hoisting it into a local
removed 116 bytes and took the function to 1104 B / 87,5 %.

**Rule: before calling anything an allocator artefact, compare the two sizes.**
``frame_hole.py`` compares *offsets*; a body that is too long or too short looks
identical to it. Only the size comparison separates the cases, and it is the one
that was skipped.

The residual 36 B is a ``setLength`` expansion shape: the ROM computes
``squared()`` twice and materialises ``fmr f0, f1`` in the degenerate branch,
where we compute it three times.

## OPEN LEAD (not acted on): ``DEFINE_NERVE`` emits one extra vtable store

Found 2026-10-01 while triaging the ``oversize`` bucket. **Not fixed** - see
"why not" below. This is the biggest unclaimed lead found so far.

Five independently sampled ``oversize`` functions all show the same signature -
far more ours-only instructions than ROM-only:

| function | ours-only | ROM-only |
|---|---|---|
| ``TKoopaJr::perform`` | 42 | 8 |
| ``TBossTelesaSlotStart::execute`` | 63 | 8 |
| ``TTabePuku::attackToMario`` | 51 | 9 |
| ``TMapObjFlag::draw`` | 48 | 34 |
| ``TBeeHive::calcRootMatrix`` | 39 | 18 |

In ``TKoopaJr::perform`` the surplus is unambiguous. Building
``theNerve()``'s function-local static, we emit:

```
lis  r3, TNerveBase<TLiveActor>::__vtable@ha     <-- EXTRA
stw  r0, instance$NNNN@sda21                     <-- EXTRA
li   r3, instance$NNNN@sda21
bl   TNerveBase<TLiveActor>::TNerveBase()
lis  r3, TNerveKoopaJrWait::__vtable@ha          <-- both sides
stw  r0, instance$NNNN@sda21                     <-- both sides
```

The ROM writes **one** vtable pointer into the static; we write **two**. The
extra one is the *base* class vtable, stored before the base constructor runs.
``DEFINE_NERVE`` itself (include/Strategic/Nerve.hpp:22) is only a function-local
static, so the surplus comes from how MWCC materialises the vtable slot of a
local static whose type derives from ``TNerveBase<T>`` - i.e. from the shape of
the class, not the macro.

**Blast radius: 377 ``DEFINE_NERVE`` expansions across 52 files.** If this is one
cause, it is worth far more than any single-function reconstruction.

**Why it is still open.** Two things must be true before touching a core header:
the change must be *identified* (we do not yet know what in ``TNerveBase`` makes
MWCC pre-store the base vtable), and it must be *measurable* end to end. Right
now ``CHECK config/GMSP01/build.sha1`` fails on a ``mario.dol`` another agent
regenerated, so there is no project-wide match baseline to prove a core-header
change is a net win. A header edit that gains 377 sites in some TUs and loses
others would be invisible today. Do the single-TU experiment first
(``Enemy/koopajr`` alone) and measure ``perform`` before promoting it.

**Method note.** This was found by the classifier's ``oversize`` bucket, not by
reading the function. The same sweep over ``structural`` / ``call_shape`` is
still unmined.

### CORRECTION to the section above: it is a source difference, not a codegen one

Do not act on the previous section as written. Its framing ("MWCC emits one
extra vtable store") is wrong, and the measurement that seemed to support it
was an artifact: ``objdump -d`` prints the symbol on the *reloc* line, so
``grep 'lis.*__vt__'`` on objdump output returns **0** and looks like "we never
emit it". Counting ``R_PPC_ADDR16_HA`` relocs instead gives the real numbers:

| | ROM koopajr.s | our koopajr.o |
|---|---|---|
| ``lis __vt__24TNerveBase`` (whole TU) | 24 | 45 |
| ``lis __vt__24TNerveBase`` inside ``perform`` | 2 | 6 |

Both sides emit base-vtable pre-stores, and the ``virtual ~TNerveBase()`` in
include/Strategic/Nerve.hpp:10 is genuine - the map has both
``__dt__24TNerveBase<10TLiveActor>Fv`` (54474) and ``__vt__24TNerveBase`` (14520).
So the destructorial hypothesis is dead.

The real cause is simpler and it is **ours**: inside ``TKoopaJr::perform`` our
source calls ``theNerve()`` **8 times** while the ROM constructs **5** distinct
nerve singletons (``instance$NNNN`` count). We push **three nerve singletons the
ROM never builds** in this function. The surplus vtable stores are the
*consequence* of those three extra constructions, not an allocator quirk.

So ``TKoopaJr::perform`` at 82,3 % has a genuine behavioural gap - a wrong set of
``pushAfterCurrent(&X::theNerve())`` calls - not a codegen artefact. That makes
it ordinary reconstruction work, and it is worth re-classifying: the five
functions sampled under ``oversize`` may be a mix of this and the real allocator
noise, so the bucket must be re-examined case by case before any of it is
treated as mechanical.

**Method lesson.** ``objdump -d`` alone silently returns zero matches for
SDA/ADDR16 reloc targets. Always count relocations (``-dr``, or the ``-r`` dump),
never the mnemonic line, when comparing symbol reference counts.
