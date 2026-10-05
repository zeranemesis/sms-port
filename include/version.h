#ifndef VERSION_H
#define VERSION_H

#ifdef VERSION_GMSJ01
#define GMSJ01(value) value
#else
#define GMSJ01(value)
#endif

#ifdef VERSION_GMSP01
#define GMSP01(value) value
#else
#define GMSP01(value)
#endif

// The US build (GMSE01) is this fork's target and not an upstream one. A
// VERSION_SELECT without a GMSE01(...) entry expands to `()` there and fails
// to compile, so every region-dependent value gets an explicit US decision
// instead of silently inheriting another region's.
#ifdef VERSION_GMSE01
#define GMSE01(value) value
#else
#define GMSE01(value)
#endif

#define VERSION_SELECT_JOIN(a, b, c, d, e, f, g, h, ...) a b c d e f g h

#define VERSION_SELECT(...) (VERSION_SELECT_JOIN(__VA_ARGS__, , , , , , , ))

#endif
