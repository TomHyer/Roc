#pragma once

typedef unsigned char uint8;
typedef char sint8;
typedef unsigned short uint16;
typedef short sint16;
typedef unsigned int uint32;
typedef int sint32;
typedef unsigned long long uint64;
typedef long long sint64;

INLINE uint64 Bit(int loc)
{
	return 1ull << loc;
}

template<class C> INLINE bool T(const C& x)
{
	return x != 0;
}
template<class T> INLINE bool F(const T& x)
{
	return x == 0;
}

#ifndef HNI
INLINE void Cut(uint64& x)
{
	x &= x - 1;
}
#else
INLINE void Cut(uint64& x)
{
	x = _blsr_u64(x);
}
#endif

INLINE bool Multiple(const uint64& b)
{
	return T(b & (b - 1));
}
INLINE bool Single(const uint64& b)
{
	return F(b & (b - 1));
}
INLINE bool HasBit(const uint64& bb, int loc)
{
	return T(bb & Bit(loc));
}

INLINE int lsb(uint64 x)
{
	unsigned long y;
	_BitScanForward64(&y, x);
	return y;
}

INLINE int msb(uint64 x)
{
	unsigned long y;
	_BitScanReverse64(&y, x);
	return y;
}

INLINE int popcnt(uint64 x)
{
	x = x - ((x >> 1) & 0x5555555555555555);
	x = (x & 0x3333333333333333) + ((x >> 2) & 0x3333333333333333);
	x = (x + (x >> 4)) & 0x0f0f0f0f0f0f0f0f;
	return (x * 0x0101010101010101) >> 56;
}

typedef int(*pop_func_t)(const uint64&);
int pop0(const uint64& b) { return popcnt(b); }
struct pop0_
{
	INLINE int operator()(const uint64& b) const { return pop0(b); }
	INLINE pop_func_t Imp() const { return pop0; }
};
int pop1(const uint64& b) { return static_cast<int>(_mm_popcnt_u64(b)); }
struct pop1_
{
	INLINE int operator()(const uint64& b) const { return pop1(b); }
	INLINE pop_func_t Imp() const { return pop1; }
};

INLINE int lsb(uint64 x);
INLINE int msb(uint64 x);
INLINE int popcnt(uint64 x);
INLINE int MSBZ(const uint64& x)
{
	return x ? msb(x) : 63;
}
INLINE int LSBZ(const uint64& x)
{
	return x ? lsb(x) : 0;
}
template<bool me> int NB(const uint64& x)
{
	return me ? msb(x) : lsb(x);
}
template<bool me> int NBZ(const uint64& x)
{
	return me ? MSBZ(x) : LSBZ(x);
}

