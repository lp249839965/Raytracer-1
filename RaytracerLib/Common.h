#pragma once

#include <inttypes.h>
#include <stddef.h>


// C++ nonstandard extension: nameless struct
#pragma warning(disable : 4201)

// structure was padded due to alignment specifier
#pragma warning(disable : 4324)


#define RT_UNUSED(x) (void)(x)
#define RT_INLINE inline
#define RT_FORCE_INLINE __forceinline
#define RT_FORCE_NOINLINE __declspec(noinline)
#define RT_ALIGN(x) __declspec(align(x))

#define RT_CACHE_LINE_SIZE 64u

// force global variable definition to be shared across all compilation units
#define RT_GLOBAL_CONST extern const __declspec(selectany)

#define RT_PREFETCH_L1(addr) _mm_prefetch((const char*)(addr), _MM_HINT_T0);
#define RT_PREFETCH_L2(addr) _mm_prefetch((const char*)(addr), _MM_HINT_T1);
#define RT_PREFETCH_L3(addr) _mm_prefetch((const char*)(addr), _MM_HINT_T2);

#if defined(__LINUX__) | defined(__linux__)
using Uint64 = uint64_t;
using Int64 = int64_t;
#elif defined(_WINDOWS)
using Uint64 = unsigned __int64;
using Int64 = __int64;
#endif // defined(__LINUX__) | defined(__linux__)

using Uint32 = unsigned int;
using Int32 = signed int;
using Uint16 = unsigned short;
using Int16 = signed short;
using Uint8 = unsigned char;
using Int8 = signed char;
using Bool = bool;
using Float = float;
using Double = double;


namespace rt {

union Bits32
{
    Float f;
    Uint32 ui;
    Int32 si;
};

union Bits64
{
    Double f;
    Uint64 ui;
    Int64 si;
};

} // namespace rt
