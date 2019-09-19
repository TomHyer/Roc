// This code is public domain, as defined by the "CC0" Creative Commons license

//#define REGRESSION
//#define W32_BUILD
#define _CRT_SECURE_NO_WARNINGS
//#define CPU_TIMING
//#define TWO_PHASE
#define LARGE_PAGES
#define MP_NPS
//#define TIME_TO_DEPTH
#define TB 1
//#define HNI
//#define VERBOSE

#ifdef W32_BUILD
#define NTDDI_VERSION 0x05010200
#define _WIN32_WINNT 0x0501
#else
#define _WIN32_WINNT 0x0600
#endif

#include <iostream>
#include <fstream>
#include <array>
#include <numeric>
#include <string>
#include <vector>
#include <thread>
#include <cmath>
#include <algorithm>
#include <chrono>
typedef std::chrono::high_resolution_clock::time_point time_point;
#include <windows.h>
#undef min
#undef max
#include <intrin.h>
#include <assert.h>
template<class T> std::string Str(const T& src) { return std::to_string(src); }
// Windows-specific
typedef HANDLE mutex_t;
typedef HANDLE event_t;

//#include "TunerParams.inc"

using namespace std;
#define INLINE __forceinline
typedef unsigned char uint8;
typedef char sint8;
typedef unsigned short uint16;
typedef short sint16;
typedef unsigned int uint32;
typedef int sint32;
typedef unsigned long long uint64;
typedef long long sint64;

#define TEMPLATE_ME(f) (me ? f<1> : f<0>)
template<class T> INLINE int Sgn(const T& x)
{
	return x == 0 ? 0 : (x > 0 ? 1 : -1);
}
template<class T> INLINE const T& Min(const T& x, const T& y)
{
	return x < y ? x : y;
}
template<class T> INLINE constexpr const T& Max(const T& x, const T& y)
{
	return x < y ? y : x;
}
template<class T> INLINE constexpr T Abs(const T& x)
{
	return x > 0 ? x : -x;
}
template<class T> INLINE constexpr T Square(const T& x)
{
	return x * x;
}
template<class C> INLINE bool T(const C& x)
{
	return x != 0;
}
template<class T> INLINE bool F(const T& x)
{
	return x == 0;
}
template<class T> INLINE bool Even(const T& x)
{
	return F(x & 1);
}
template<class C> INLINE bool Odd(const C& x)
{
	return T(x & 1);
}

#ifdef TWO_PHASE
typedef sint32 packed_t;

INLINE packed_t Pack2(sint16 op, sint16 eg)
{
	return op + (static_cast<sint32>(eg) << 16);
}
INLINE packed_t Pack4(sint16 op, sint16 mid, sint16 eg, sint16 asym)
{
	return Pack2(op, eg);
}

INLINE sint16 Opening(packed_t x) { return static_cast<sint16>(x & 0xFFFF); }
INLINE sint16 Middle(packed_t x) { return 0; }
INLINE sint16 Endgame(packed_t x) { return static_cast<sint16>((x >> 16) + ((x >> 15) & 1)); }
INLINE sint16 Closed(packed_t x) { return 0; }
#else
typedef sint64 packed_t;

constexpr packed_t Pack4(sint16 op, sint16 mid, sint16 eg, sint16 asym)
{
	return op + (static_cast<sint64>(mid) << 16) + (static_cast<sint64>(eg) << 32) + (static_cast<sint64>(asym) << 48);
}
constexpr packed_t Pack2(sint16 x, sint16 y)
{
	return Pack4(x, (x + y) / 2, y, 0);
}

INLINE sint16 Opening(packed_t x) { return static_cast<sint16>(x & 0xFFFF); }
INLINE sint16 Middle(packed_t x) { return static_cast<sint16>((x >> 16) + ((x >> 15) & 1)); }
INLINE sint16 Endgame(packed_t x) { return static_cast<sint16>((x >> 32) + ((x >> 31) & 1)); }
INLINE sint16 Closed(packed_t x) { return static_cast<sint16>((x >> 48) + ((x >> 47) & 1)); }

#endif


// unsigned for king_att
constexpr uint32 UPack(uint16 x, uint16 y)
{
	return x + (static_cast<uint32>(y) << 16);
}
INLINE uint16 UUnpack1(uint32 x)
{
	return static_cast<uint16>(x & 0xFFFF);
}
INLINE uint16 UUnpack2(uint32 x)
{
	return static_cast<sint16>(x >> 16);
}


INLINE int constexpr FileOf(int loc)
{
	return loc & 7;
}
INLINE constexpr int RankOf(int loc)
{
	return loc >> 3;
}
template<bool me> INLINE int OwnRank(int loc)
{
	return me ? (7 - RankOf(loc)) : RankOf(loc);
}
INLINE int OwnRank(bool me, int loc)
{
	return me ? (7 - RankOf(loc)) : RankOf(loc);
}
INLINE int NDiagOf(int loc)
{
	return 7 - FileOf(loc) + RankOf(loc);
}
INLINE int SDiagOf(int loc)
{
	return FileOf(loc) + RankOf(loc);
}
INLINE constexpr int Dist(int x, int y)
{
	return Max(Abs(RankOf(x) - RankOf(y)), Abs(FileOf(x) - FileOf(y)));
}

INLINE constexpr uint64 Bit(int loc)
{
	return 1ull << loc;
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

INLINE int From(uint16 move)
{
	return (move >> 6) & 0x3f;
}
INLINE int To(uint16 move)
{
	return move & 0x3f;
}
inline void SetScore(int* move, uint16 score)
{
	*move = (*move & 0xFFFF) | (score << 16);
}  // notice this operates on long-form (int) moves

constexpr uint64 Filled = 0xFFFFFFFFFFFFFFFF;
constexpr uint64 Interior = 0x007E7E7E7E7E7E00;
constexpr uint64 Boundary = ~Interior;
constexpr uint64 LightArea = 0x55AA55AA55AA55AA;
constexpr uint64 DarkArea = ~LightArea;

INLINE uint32 High32(const uint64& x)
{
	return (uint32)(x >> 32);
}
INLINE uint32 Low32(const uint64& x)
{
	return (uint32)(x);
}

constexpr int N_KILLER = 2;

constexpr int MAX_PHASE = 128;
constexpr int MIDDLE_PHASE = 64;
constexpr int PHASE_M2M = MAX_PHASE - MIDDLE_PHASE;

constexpr int MAX_HEIGHT = 128;

typedef sint16 score_t;

constexpr score_t CP_EVAL = 4;	// numeric value of 1 centipawn, in eval phase
constexpr score_t CP_SEARCH = 4;	// numeric value of 1 centipawn, in search phase (# of value equivalence classes in 1-cp interval)

									// helper to divide intermediate quantities to form scores
									// note that straight integer division (a la Gull) creates an attractor at 0
									// we support this, especially for weights inherited from Gull which have not been tuned for Roc
template<int DEN, int SINK = DEN> struct Div_
{
	int operator()(int x) const
	{
		constexpr int shift = std::numeric_limits<int>::max() / (2 * DEN);
		constexpr int shrink = (SINK - DEN) / 2;
		const int y = x > 0 ? Max(0, x - shrink) : Min(0, x + shrink);
		return (y + DEN * shift) / DEN - shift;
	}
};

constexpr uint8 White = 0;
constexpr uint8 Black = 1;
constexpr uint8 WhitePawn = 2;
constexpr uint8 BlackPawn = 3;
constexpr uint8 IPawn[2] = { WhitePawn, BlackPawn };
constexpr uint8 WhiteKnight = 4;
constexpr uint8 BlackKnight = 5;
constexpr uint8 IKnight[2] = { WhiteKnight, BlackKnight };
constexpr uint8 WhiteLight = 6;
constexpr uint8 BlackLight = 7;
constexpr uint8 ILight[2] = { WhiteLight, BlackLight };
constexpr uint8 WhiteDark = 8;
constexpr uint8 BlackDark = 9;
constexpr uint8 IDark[2] = { WhiteDark, BlackDark };
constexpr uint8 WhiteRook = 10;
constexpr uint8 BlackRook = 11;
constexpr uint8 IRook[2] = { WhiteRook, BlackRook };
constexpr uint8 WhiteQueen = 12;
constexpr uint8 BlackQueen = 13;
constexpr uint8 IQueen[2] = { WhiteQueen, BlackQueen };
constexpr uint8 WhiteKing = 14;
constexpr uint8 BlackKing = 15;
constexpr uint8 IKing[2] = { WhiteKing, BlackKing };
INLINE bool IsBishop(uint8 piece)
{
	return piece >= WhiteLight && piece < WhiteRook;
}

constexpr uint8 CanCastle_OO = 1;
constexpr uint8 CanCastle_oo = 2;
constexpr uint8 CanCastle_OOO = 4;
constexpr uint8 CanCastle_ooo = 8;

constexpr uint16 FlagCastling = 0x1000;  // also overloaded to flag "joins", desirable quiet moves
constexpr uint16 FlagEP = 0x2000;
constexpr uint16 FlagPKnight = 0x4000;
constexpr uint16 FlagPBishop = 0x6000;
constexpr uint16 FlagPRook = 0xA000;
constexpr uint16 FlagPQueen = 0xC000;

INLINE bool IsPromotion(uint16 move)
{
	return T(move & 0xC000);
}
INLINE bool IsCastling(int piece, uint16 move)
{
	return piece >= WhiteKing && abs(To(move) - From(move)) == 2;
}
INLINE bool IsEP(uint16 move)
{
	return (move & 0xF000) == FlagEP;
}
template<bool me> INLINE uint8 Promotion(uint16 move)
{
	uint8 retval = (move & 0xF000) >> 12;
	if (retval == WhiteLight && HasBit(DarkArea, To(move)))
		retval += 2;
	return retval + (me ? 1 : 0);
}

#ifndef W32_BUILD
INLINE int lsb(uint64 x)
{
	register unsigned long y;
	_BitScanForward64(&y, x);
	return y;
}

INLINE int msb(uint64 x)
{
	register unsigned long y;
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

#else
INLINE int lsb(uint64 x) {
	_asm {
		mov eax, dword ptr x[0]
		test eax, eax
		jz l_high
		bsf eax, eax
		jmp l_ret
		l_high : bsf eax, dword ptr x[4]
				 add eax, 20h
				 l_ret :
	}
}

INLINE int msb(uint64 x) {
	_asm {
		mov eax, dword ptr x[4]
		test eax, eax
		jz l_low
		bsr eax, eax
		add eax, 20h
		jmp l_ret
		l_low : bsr eax, dword ptr x[0]
				l_ret :
	}
}

INLINE int popcnt(uint64 x) {
	unsigned int x1, x2;
	x1 = (unsigned int)(x & 0xFFFFFFFF);
	x1 -= (x1 >> 1) & 0x55555555;
	x1 = (x1 & 0x33333333) + ((x1 >> 2) & 0x33333333);
	x1 = (x1 + (x1 >> 4)) & 0x0F0F0F0F;
	x2 = (unsigned int)(x >> 32);
	x2 -= (x2 >> 1) & 0x55555555;
	x2 = (x2 & 0x33333333) + ((x2 >> 2) & 0x33333333);
	x2 = (x2 + (x2 >> 4)) & 0x0F0F0F0F;
	return ((x1 * 0x01010101) >> 24) + ((x2 * 0x01010101) >> 24);
}
#endif

typedef int(*pop_func_t)(const uint64&);
int pop0(const uint64& b) { return popcnt(b); }
struct pop0_
{
	INLINE int operator()(const uint64& b) const { return pop0(b); }
	INLINE pop_func_t Imp() const { return pop0; }
};
#ifdef W32_BUILD
#define pop1 pop0
#define pop1_ pop0_
#else
int pop1(const uint64& b) { return static_cast<int>(_mm_popcnt_u64(b)); }
struct pop1_
{
	INLINE int operator()(const uint64& b) const { return pop1(b); }
	INLINE pop_func_t Imp() const { return pop1; }
};
#endif

constexpr int MatWQ = 1;
constexpr int MatBQ = 3 * MatWQ;
constexpr int MatWR = 3 * MatBQ;
constexpr int MatBR = 3 * MatWR;
constexpr int MatWL = 3 * MatBR;
constexpr int MatBL = 2 * MatWL;
constexpr int MatWD = 2 * MatBL;
constexpr int MatBD = 2 * MatWD;
constexpr int MatWN = 2 * MatBD;
constexpr int MatBN = 3 * MatWN;
constexpr int MatWP = 3 * MatBN;
constexpr int MatBP = 9 * MatWP;
constexpr int TotalMat = 2 * (MatWQ + MatBQ) + MatWL + MatBL + MatWD + MatBD + 2 * (MatWR + MatBR + MatWN + MatBN) + 8 * (MatWP + MatBP) + 1;
constexpr int FlagUnusualMaterial = 1 << 30;

constexpr array<int, 16> MatCode = { 0, 0, MatWP, MatBP, MatWN, MatBN, MatWL, MatBL, MatWD, MatBD, MatWR, MatBR, MatWQ, MatBQ, 0, 0 };

template<class Function, std::size_t... Indices>
constexpr auto make_array_helper(Function f, std::index_sequence<Indices...>)
->std::array<typename std::result_of<Function(std::size_t)>::type, sizeof...(Indices)>
{
	return { { f(Indices)... } };
}

template<int N, class Function>
constexpr auto make_array(Function f)
->std::array<typename std::result_of<Function(std::size_t)>::type, N>
{
	return make_array_helper(f, std::make_index_sequence<N>{});
}


namespace Calc
{
	constexpr uint64 FileA = 0x0101010101010101;
	constexpr uint64 Line0 = 0x00000000000000FF;
	constexpr uint64 NDiagMain = 0x8040201008040201;
	constexpr uint64 SDiagMain = 0x0102040810204080;
}
constexpr array<uint64, 8> File = { Calc::FileA, Calc::FileA << 1, Calc::FileA << 2, Calc::FileA << 3, Calc::FileA << 4, Calc::FileA << 5, Calc::FileA << 6, Calc::FileA << 7 };
constexpr array<uint64, 8> Line = { Calc::Line0, (Calc::Line0 << 8), (Calc::Line0 << 16), (Calc::Line0 << 24), (Calc::Line0 << 32), (Calc::Line0 << 40), (Calc::Line0 << 48), (Calc::Line0 << 56) };

namespace Calc
{
	template<typename PRED_> constexpr uint64 BuildBB(PRED_ pred, int sq, int n = 64)
	{
		return n > 0 ? BuildBB<PRED_>(pred, sq, n - 1) | (pred(sq, n - 1) ? Bit(n - 1) : 0ull) : 0ull;
	}

	constexpr uint64 HLine(int sq)
	{
		return Line[RankOf(sq)] ^ Bit(sq);
	}
	constexpr uint64 VLine(int sq)
	{
		return File[FileOf(sq)] ^ Bit(sq);
	}
	constexpr uint64 Diag2(uint64 main, int which)
	{
		return which < 0 ? main >> (-8 * which) : main << (8 * which);
	}
	constexpr uint64 NDiag(int sq)
	{
		return Diag2(NDiagMain, RankOf(sq) - FileOf(sq)) ^ Bit(sq);
	}
	constexpr uint64 SDiag(int sq)
	{
		return Diag2(SDiagMain, RankOf(sq) + FileOf(sq) - 7) ^ Bit(sq);
	}

	constexpr bool IsNAtt(int from, int to)
	{
		return Square(RankOf(from) - RankOf(to)) + Square(FileOf(from) - FileOf(to)) == 5;
	}
	constexpr uint64 NAtt(int sq)
	{
		return BuildBB(IsNAtt, sq);
	}
	constexpr bool IsKAtt(int from, int to)
	{
		return Dist(from, to) == 1;
	}
	constexpr uint64 KAtt(int sq)
	{
		return BuildBB(IsKAtt, sq);
	}
}
constexpr array<uint64, 64> HLine = make_array<64>(Calc::HLine);
constexpr array<uint64, 64> VLine = make_array<64>(Calc::VLine);
constexpr array<uint64, 64> NDiag = make_array<64>(Calc::NDiag);
constexpr array<uint64, 64> SDiag = make_array<64>(Calc::SDiag);
constexpr array<uint64, 64> NAtt = make_array<64>(Calc::NAtt);
constexpr array<uint64, 64> KAtt = make_array<64>(Calc::KAtt);

namespace Calc
{
	constexpr uint64 RMask(int sq)
	{
		return ::HLine[sq] | ::VLine[sq];
	}
	constexpr uint64 BMask(int sq)
	{
		return ::NDiag[sq] | ::SDiag[sq];
	}

	constexpr bool IsNAttAtt(int from, int to)
	{
		return !!(::NAtt[from] & ::NAtt[to]);
	}
	constexpr uint64 NAttAtt(int sq)
	{
		return BuildBB(IsNAttAtt, sq, 64);
	}

	constexpr bool IsKAttAtt(int from, int to)
	{
		return !!(::KAtt[from] & ::KAtt[to]);
	}
	constexpr uint64 KAttAtt(int sq)
	{
		return BuildBB(IsKAttAtt, sq, 64);
	}
}
constexpr array<uint64, 64> RMask = make_array<64>(Calc::RMask);
constexpr array<uint64, 64> BMask = make_array<64>(Calc::BMask);
constexpr array<uint64, 64> NAttAtt = make_array<64>(Calc::NAttAtt);
constexpr array<uint64, 64> KAttAtt = make_array<64>(Calc::KAttAtt);

namespace Calc
{
	constexpr uint64 QMask(int sq)
	{
		return ::RMask[sq] | ::BMask[sq];
	}
}
constexpr array<uint64, 64> QMask = make_array<64>(Calc::QMask);

struct GMaterial;
struct GPawnEntry;
struct GEvalInfo
{
	uint64 occ, area[2], free[2], klocus[2];
	GPawnEntry* PawnEntry;
	const GMaterial* material;
	packed_t score;
	packed_t king_score[2];
	uint32 king_att[2];
	uint16 mul;
	uint8 king[2];
};

// special calculators for specific material
// irritatingly, they need to be told what pop() to use
// setting any of these to call also triggers a call to eval_stalemate
// so they should never call eval_stalemate themselves
typedef void(*eval_special_t)(GEvalInfo& EI, pop_func_t pop);
// here is a function that exists only to trigger eval_stalemate
void eval_null(GEvalInfo&, pop_func_t) {}
void eval_unwinnable(GEvalInfo& EI, pop_func_t) { EI.mul = 0; }

struct GMaterial
{
	sint16 score, closed;
	array<eval_special_t, 2> eval;
	array<uint8, 2> mul;
	uint8 phase;
};

constexpr int MAGIC_SIZE = 107648;
struct CommonData_
{
	array<GMaterial, TotalMat> Material;
	array<uint64, MAGIC_SIZE> MagicAttacks;
	array<array<array<uint64, 64>, 64>, 2> Kpk;
	array<array<uint64, 64>, 64> Between, FullLine;
	array<packed_t, 16 * 64> PstVals;
	array<array<uint64, 64>, 16> PieceKey;
	array<array<int, 16>, 16> MvvLva;  // [piece][capture]
	array<int, 256> SpanWidth, SpanGap;
	array<array<uint64, 64>, 2> BishopForward, PAtt, PMove, PWay, PCone, PSupport;
	array<uint64, 64> BMagic, BMagicMask, RMagic, RMagicMask;
	array<uint64, 64> OneIn;
	array<uint64, 64> KingFrontal, KingFlank;
	array<array<packed_t, 28>, 2> MobQueen;
	array<uint8, 256> PieceFromChar;
	array<int, 64> ROffset;
	array<array<packed_t, 15>, 2> MobBishop, MobRook;
	array<array<packed_t, 9>, 2> MobKnight;
	array<short, 64> BShift, BOffset, RShift;
	array<uint64, 16> CastleKey;
	array<array<uint64, 8>, 2> Forward;
	array<uint64, 8> EPKey;
	array<uint64, 8> West, East, PIsolated;
	array<packed_t, 8> PasserGeneral, PasserBlocked, PasserFree, PasserSupported, PasserProtected, PasserConnected, PasserOutside, PasserCandidate, PasserClear;
	array<uint8, 64> UpdateCastling;
	array<array<sint16, 8>, 3> Shelter;
	array<uint16, 16> KingAttackScale;
	array<int, 8> KingCenterScale;
	array<uint8, 16> LogDist;
	array<sint16, 8> PasserAtt, PasserDef, PasserAttLog, PasserDefLog;
	array<sint16, 4> StormBlocked, StormShelterAtt, StormConnected, StormOpen, StormFree;
	uint64 TurnKey;
};
CommonData_* DATA = nullptr;
const CommonData_* RO = nullptr;	// generally we access DATA through RO

#define opp (1 ^ (me))
#define MY_TURN (!Current->turn == opp)

INLINE uint64 ShiftNW(const uint64& target)
{
	return (target & (~(File[0] | Line[7]))) << 7;
}
INLINE uint64 ShiftNE(const uint64& target)
{
	return (target & (~(File[7] | Line[7]))) << 9;
}
INLINE uint64 ShiftSE(const uint64& target)
{
	return (target & (~(File[7] | Line[0]))) >> 7;
}
INLINE uint64 ShiftSW(const uint64& target)
{
	return (target & (~(File[0] | Line[0]))) >> 9;
}
template<bool me> INLINE uint64 ShiftW(const uint64& target)
{
	return me ? ShiftSW(target) : ShiftNW(target);
}
template<bool me> INLINE uint64 ShiftE(const uint64& target)
{
	return me ? ShiftSE(target) : ShiftNE(target);
}
template<bool me> INLINE uint64 Shift(const uint64& target)
{
	return me ? target >> 8 : target << 8;
}

// THIS IS A NON-TEMPLATE FUNCTION BECAUSE MY COMPILER (Visual C++ 2015, Community Edition) CANNOT INLINE THE TEMPLATE VERSION CORRECTLY
INLINE const uint64& OwnLine(bool me, int n)
{
	return Line[me ? 7 - n : n];
}

// Constants controlling play
constexpr int PliesToEvalCut = 50;	// halfway to 50-move
constexpr int KingSafetyNoQueen = 8;	// numerator; denominator is 16
constexpr int SeeThreshold = 40 * CP_EVAL;
constexpr int DrawCapConstant = 100 * CP_EVAL;
constexpr int DrawCapLinear = 0;	// numerator; denominator is 64
constexpr int DeltaDecrement = (3 * CP_SEARCH) / 2;	// 5 (+91/3) vs 3
constexpr int TBMinDepth = 7;

inline int MapPositive(int scale, int param)
{
	return param > scale ? param : (scale * scale) / (2 * scale - param);
}

constexpr int FailLoInit = 22;
constexpr int FailHiInit = 31;
constexpr int FailLoGrowth = 37;	// numerator; denominator is 64
constexpr int FailHiGrowth = 26;	// numerator; denominator is 64
constexpr int FailLoDelta = 27;
constexpr int FailHiDelta = 24;
constexpr int InitiativeConst = 3 * CP_SEARCH;
constexpr int InitiativePhase = 3 * CP_SEARCH;
constexpr int FutilityThreshold = 50 * CP_SEARCH;

#define IncV(var, x) (me ? (var -= (x)) : (var += (x)))
#define DecV(var, x) IncV(var, -(x))

constexpr sint16 KpkValue = 300 * CP_EVAL;
constexpr sint16 EvalValue = 30000;
constexpr sint16 MateValue = 32760 - 8 * (CP_SEARCH - 1);

#if TB
constexpr sint16 TBMateValue = 31380;
constexpr sint16 TBCursedMateValue = 13;
const int TbValues[5] = { -TBMateValue, -TBCursedMateValue, 0, TBCursedMateValue, TBMateValue };
constexpr int NominalTbDepth = 33;
inline int TbDepth(int depth) { return Min(depth + NominalTbDepth, 117); }

constexpr uint32 TB_RESULT_FAILED = 0xFFFFFFFF;
extern unsigned TB_LARGEST;
bool tb_init_fwd(const char*);

#define TB_CUSTOM_POP_COUNT pop1
#define TB_CUSTOM_LSB lsb
#define TB_CUSTOM_BSWAP32 _byteswap_ulong 
template<class F_, typename... Args_> int TBProbe(F_ func, bool me, const Args_&&... args)
{
	return func(Piece(White), Piece(Black),
		King(White) | King(Black),
		Queen(White) | Queen(Black),
		Rook(White) | Rook(Black),
		Bishop(White) | Bishop(Black),
		Knight(White) | Knight(Black),
		Pawn(White) | Pawn(Black),
		Current->ply,
		Current->castle_flags,
		Current->ep_square,
		(me == White), std::forward<Args_>(args)...);
}



unsigned tb_probe_root_fwd(
	uint64_t _white,
	uint64_t _black,
	uint64_t _kings,
	uint64_t _queens,
	uint64_t _rooks,
	uint64_t _bishops,
	uint64_t _knights,
	uint64_t _pawns,
	unsigned _rule50,
	unsigned _ep,
	bool     _turn);

static inline unsigned tb_probe_root_checked(
	uint64_t _white,
	uint64_t _black,
	uint64_t _kings,
	uint64_t _queens,
	uint64_t _rooks,
	uint64_t _bishops,
	uint64_t _knights,
	uint64_t _pawns,
	unsigned _rule50,
	unsigned _castling,
	unsigned _ep,
	bool     _turn)
{
	if (_castling != 0)
		return TB_RESULT_FAILED;
	return tb_probe_root_fwd(_white, _black, _kings, _queens, _rooks, _bishops, _knights, _pawns, _rule50, _ep, _turn);
}

int GetTBMove(unsigned res, int* best_score);

#endif

/*
general move:
0 - 11: from & to
12 - 15: flags
16 - 23: history
24 - 25: spectial moves: killers, refutations...
26 - 30: MvvLva
delta move:
0 - 11: from & to
12 - 15: flags
16 - 31: sint16 delta + (sint16)0x4000
*/
constexpr array<int, 16> MvvLvaVictim = { 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 3, 3, 3, 3 };
constexpr array<int, 16> MvvLvaAttacker = { 0, 0, 5, 5, 4, 4, 3, 3, 3, 3, 2, 2, 1, 1, 6, 6 };
constexpr array<int, 16> MvvLvaAttackerKB = { 0, 0, 9, 9, 7, 7, 5, 5, 5, 5, 3, 3, 1, 1, 11, 11 };

INLINE int PawnCaptureMvvLva(int attacker) { return MvvLvaAttacker[attacker]; }
constexpr int MaxPawnCaptureMvvLva = MvvLvaAttacker[15];  // 6
INLINE int KnightCaptureMvvLva(int attacker) { return MaxPawnCaptureMvvLva + MvvLvaAttackerKB[attacker]; }
constexpr int MaxKnightCaptureMvvLva = MaxPawnCaptureMvvLva + MvvLvaAttackerKB[15];  // 17
INLINE int BishopCaptureMvvLva(int attacker) { return MaxPawnCaptureMvvLva + MvvLvaAttackerKB[attacker] + 1; }
constexpr int MaxBishopCaptureMvvLva = MaxPawnCaptureMvvLva + MvvLvaAttackerKB[15] + 1;  // usually 18
INLINE int RookCaptureMvvLva(int attacker) { return MaxBishopCaptureMvvLva + MvvLvaAttacker[attacker]; }
constexpr int MaxRookCaptureMvvLva = MaxBishopCaptureMvvLva + MvvLvaAttacker[15];  // usually 24
INLINE int QueenCaptureMvvLva(int attacker) { return MaxRookCaptureMvvLva + MvvLvaAttacker[attacker]; }

INLINE int MvvLvaPromotionCap(int capture) { return RO->MvvLva[((capture) < WhiteRook) ? WhiteRook : ((capture) >= WhiteQueen ? WhiteKing : WhiteKnight)][BlackQueen]; }
INLINE int MvvLvaPromotionKnightCap(int capture) { return RO->MvvLva[WhiteKing][capture]; }
INLINE int MvvLvaXrayCap(int capture) { return RO->MvvLva[WhiteKing][capture]; }
constexpr int RefOneScore = (0xFF << 16) | (3 << 24);
constexpr int RefTwoScore = (0xFF << 16) | (2 << 24);

INLINE int ExtToFlag(int ext)
{
	return ext << 16;
}
INLINE int ExtFromFlag(int flags)
{
	return (flags >> 16) & 0xF;
}
constexpr int FlagHashCheck = 1 << 20;  // first 20 bits are reserved for the hash killer and extension
constexpr int FlagHaltCheck = 1 << 21;
constexpr int FlagCallEvaluation = 1 << 22;
constexpr int FlagDisableNull = 1 << 23;
constexpr int FlagNeatSearch = FlagHashCheck | FlagHaltCheck | FlagCallEvaluation;
constexpr int FlagNoKillerUpdate = 1 << 24;
constexpr int FlagReturnBestMove = 1 << 25;

typedef struct
{
	array<uint64, 16> bb;
	array<uint8, 64> square;
} GBoard;
__declspec(align(64)) GBoard Board[1];
array<uint64, 2048> Stack;
int sp, save_sp;
//uint64 nodes, tb_hits, check_node, check_node_smp;
GBoard SaveBoard[1];

// macros using global "Board"
INLINE uint64& Piece(int i) { return Board->bb[i]; }
INLINE uint64& Pawn(int me) { return Piece(WhitePawn | me); }
INLINE uint64& Knight(int me) { return Piece(WhiteKnight | me); }
INLINE uint64 Bishop(int me) { return Piece(WhiteLight | me) | Piece(WhiteDark | me); }
INLINE uint64& Rook(int me) { return Piece(WhiteRook | me); }
INLINE uint64& Queen(int me) { return Piece(WhiteQueen | me); }
INLINE uint64& King(int me) { return Piece(WhiteKing | me); }
INLINE uint64 NonPawn(int me) { return Piece(me) ^ Pawn(me); }
INLINE uint64 NonPawnKing(int me) { return NonPawn(me) ^ King(me); }
INLINE uint64 Major(int me) { return Rook(me) | Queen(me); }
INLINE uint64 Minor(int me) { return Knight(me) | Bishop(me); }
INLINE uint64 PieceAll() { return Piece(White) | Piece(Black); }
INLINE uint64 PawnAll() { return Pawn(White) | Pawn(Black); }
INLINE uint64 NonPawnKingAll() { return NonPawnKing(White) | NonPawnKing(Black); }
INLINE uint8& PieceAt(int sq) { return Board->square[sq]; }

typedef struct
{
	array<uint8, 64> square;
	uint64 key, pawn_key;
	packed_t material, pst;
	uint16 move;
	uint8 turn, castle_flags, ply, ep_square, piece, capture;
} GPosData;
typedef struct
{
	array<uint64, 2> att, patt, xray;
	uint64 key, pawn_key, eval_key, passer, threat, mask;
	packed_t material, pst;
	int *start, *current;
	int best;
	score_t score;
	array<uint16, N_KILLER + 1> killer;
	array<uint16, 2> ref;
	uint16 move;
	uint8 turn, castle_flags, ply, ep_square, capture, gen_flags, piece, stage, mul;
	sint32 moves[230];
} GData;
__declspec(align(64)) GData Data[MAX_HEIGHT];
GData* Current = Data;
constexpr uint8 FlagSort = 1 << 0;
constexpr uint8 FlagNoBcSort = 1 << 1;
GData SaveData[1];

enum
{
	stage_search,
	s_hash_move,
	s_good_cap,
	s_special,
	s_quiet,
	s_bad_cap,
	s_none,
	stage_evasion,
	e_hash_move,
	e_ev,
	e_none,
	stage_razoring,
	r_hash_move,
	r_cap,
	r_checks,
	r_none
};
constexpr int StageNone = (1 << s_none) | (1 << e_none) | (1 << r_none);

struct Progress_
{
	uint8 count_;	// packed W,B
	constexpr Progress_() : count_(0) {}
	constexpr Progress_(uint8 nw, uint8 nb) : count_((nw - 1) << 4 | (nb - 1)) {}
	bool Reachable() const;	// implementation after SHARED is declared
};
inline Progress_ Progress() { return Progress_(popcnt(Piece(White)), popcnt(Piece(Black))); }

struct GEntry
{
	uint32 key;
	uint16 date;
	uint16 move;
	score_t low;
	score_t high;
	uint8 low_depth;
	uint8 high_depth;
};
constexpr GEntry NullEntry = { 0, 1, 0, 0, 0, 0, 0 };

struct GPawnEntry
{
	uint64 key;
	packed_t score;
	array<sint16, 2> shelter;
	array<uint8, 2> passer, draw;
};
constexpr GPawnEntry NullPawnEntry = { 0, 0,{ 0, 0 },{ 0, 0 },{ 0, 0 } };
constexpr int N_PAWN_HASH = 1 << 20;
array<GPawnEntry, N_PAWN_HASH> PawnHash;
constexpr int PAWN_HASH_MASK = N_PAWN_HASH - 1;

typedef struct
{
	int knodes;
	int ply;
	uint32 key;
	uint16 date;
	uint16 move;
	score_t value;
	score_t exclusion;
	uint8 depth;
	uint8 ex_depth;
} GPVEntry;
constexpr GPVEntry NullPVEntry = { 0, 0, 0, 1, 0, 0, 0, 0, 0 };
constexpr int N_PV_HASH = 1 << 20;
constexpr int PV_CLUSTER = 1 << 2;
constexpr int PV_HASH_MASK = N_PV_HASH - PV_CLUSTER;

array<int, 256> RootList;

template<class T> void prefetch(T* p)
{
	_mm_prefetch(reinterpret_cast<const char*>(p), _MM_HINT_NTA);
}

#ifndef HNI
inline uint64 BishopAttacks(int sq, const uint64& occ)
{
	return RO->MagicAttacks[RO->BOffset[sq] + (((RO->BMagicMask[sq] & occ) * RO->BMagic[sq]) >> RO->BShift[sq])];
}
inline uint64 RookAttacks(int sq, const uint64& occ)
{
	return RO->MagicAttacks[RO->ROffset[sq] + (((RO->RMagicMask[sq] & occ) * RO->RMagic[sq]) >> RO->RShift[sq])];
}
#else
inline uint64 BishopAttacks(int sq, const uint64& occ)
{
	return  RO->MagicAttacks[RO->BOffset[sq] + _pext_u64(occ, RO->BMagicMask[sq])];
}
inline uint64 RookAttacks(int sq, const uint64& occ)
{
	return RO->MagicAttacks[RO->ROffset[sq] + _pext_u64(occ, RO->RMagicMask[sq])];
}
#endif
INLINE uint64 QueenAttacks(int sq, const uint64& occ)
{
	return BishopAttacks(sq, occ) | RookAttacks(sq, occ);
}

INLINE packed_t& XPst(CommonData_* data, int piece, int sq)
{
	return data->PstVals[(piece << 6) | sq];
};
INLINE packed_t Pst(int piece, int sq)
{
	return RO->PstVals[(piece << 6) | sq];
};

array<array<array<uint16, 2>, 64>, 16> HistoryVals;

INLINE int* AddMove(int* list, int from, int to, int flags, int score)
{
	*list = ((from) << 6) | (to) | (flags) | (score);
	return ++list;
}
INLINE int* AddCapturePP(int* list, int att, int vic, int from, int to, int flags)
{
	return AddMove(list, from, to, flags, RO->MvvLva[att][vic]);
}
INLINE int* AddCaptureP(int* list, int piece, int from, int to, int flags)
{
	return AddCapturePP(list, piece, PieceAt(to), from, to, flags);
}
INLINE int* AddCaptureP(int* list, int piece, int from, int to, int flags, uint8 min_vic)
{
	return AddCapturePP(list, piece, Max(min_vic, PieceAt(to)), from, to, flags);
}
INLINE int* AddCapture(int* list, int from, int to, int flags)
{
	return AddCaptureP(list, PieceAt(from), from, to, flags);
}

INLINE uint16 JoinFlag(uint16 move)
{
	return (move & FlagCastling) ? 1 : 0;
}
INLINE uint16& HistoryScore(int join, int piece, int from, int to)
{
	return HistoryVals[piece][to][join];
}
INLINE int HistoryMerit(uint16 hs)
{
	return hs / (hs & 0x00FF);	// differs by 1 from Gull convention
}
INLINE int HistoryP(int join, int piece, int from, int to)
{
	return HistoryMerit(HistoryScore(join, piece, from, to)) << 16;
}
INLINE int History(int join, int from, int to)
{
	return HistoryP(join, PieceAt(from), from, to);
}
INLINE uint16& HistoryM(int move)
{
	return HistoryScore(JoinFlag(move), PieceAt(From(move)), From(move), To(move));
}
INLINE int HistoryInc(int depth)
{
	return Square(Min((depth) >> 1, 8));
}
INLINE void HistoryBad(uint16* hist, int inc)
{
	if ((*hist & 0x00FF) >= 256 - inc)
		*hist = ((*hist & 0xFEFE) >> 1);
	*hist += inc;
}
INLINE void HistoryBad(int move, int depth)
{
	HistoryBad(&HistoryM(move), HistoryInc(depth));
}
INLINE void HistoryGood(uint16* hist, int inc)
{
	HistoryBad(hist, inc);
	*hist += inc << 8;
}
INLINE void HistoryGood(int move, int depth)
{
	HistoryGood(&HistoryM(move), HistoryInc(depth));
}
INLINE int* AddHistoryP(int* list, int piece, int from, int to, int flags)
{
	return AddMove(list, from, to, flags, HistoryP(JoinFlag(flags), piece, from, to));
}

sint16 DeltaVals[16 * 4096];
INLINE sint16& DeltaScore(int piece, int from, int to)
{
	return DeltaVals[(piece << 12) | (from << 6) | to];
}
INLINE sint16& Delta(int from, int to)
{
	return DeltaScore(PieceAt(from), from, to);
}
INLINE sint16& DeltaM(int move)
{
	return Delta(From(move), To(move));
}
INLINE int* AddCDeltaP(int* list, int margin, int piece, int from, int to, int flags)
{
	return DeltaScore(piece, from, to) < margin
		? list
		: AddMove(list, from, to, flags, (DeltaScore(piece, from, to) + 0x4000) << 16);
}

typedef struct
{
	uint16 ref[2];
	uint16 check_ref[2];
} GRef;
GRef Ref[16 * 64];
INLINE GRef& RefPointer(int piece, int from, int to)
{
	return Ref[((piece) << 6) | (to)];
}
INLINE GRef& RefM(int move)
{
	return RefPointer(PieceAt(To(move)), From(move), To(move));
}
INLINE void UpdateRef(int ref_move)
{
	auto& dst = RefM(Current->move).ref;
	if (T(Current->move) && dst[0] != ref_move)
	{
		dst[1] = dst[0];
		dst[0] = ref_move;
	}
}
INLINE void UpdateCheckRef(int ref_move)
{
	auto& dst = RefM(Current->move).check_ref;
	if (T(Current->move) && dst[0] != ref_move)
	{
		dst[1] = dst[0];
		dst[0] = ref_move;
	}
}

char info_string[1024];
char pv_string[1024];
char score_string[16];
char mstring[65536];
int MultiPV[256];
int pvp;
int pv_length;
int LastDepth, LastTime, LastValue, LastExactValue, PrevMove, InstCnt;
sint64 LastSpeed;
int PVN, PVHashing = 1, SearchMoves, SMPointer, Previous;
typedef struct
{
	int Change, Singular, Early, FailLow, FailHigh;
	bool Bad;
} GSearchInfo;
GSearchInfo CurrentSI[1], BaseSI[1];
#ifdef CPU_TIMING
int CpuTiming = 0, UciMaxDepth = 0, UciMaxKNodes = 0, UciBaseTime = 1000, UciIncTime = 5;
int GlobalTime[2] = { 0, 0 };
int GlobalInc[2] = { 0, 0 };
int GlobalTurn = 0;
constexpr sint64 CyclesPerMSec = 3400000;
#endif
constexpr int Aspiration = 1, LargePages = 1;
constexpr int TimeSingTwoMargin = 20;
constexpr int TimeSingOneMargin = 30;
constexpr int TimeNoPVSCOMargin = 60;
constexpr int TimeNoChangeMargin = 70;
constexpr int TimeRatio = 120;
constexpr int PonderRatio = 120;
constexpr int MovesTg = 26;
constexpr int InfoLag = 5000;
constexpr int InfoDelay = 1000;
time_point StartTime, InfoTime, CurrTime;
uint16 SMoves[256];

jmp_buf Jump, ResetJump;
HANDLE StreamHandle;

INLINE int ExclSingle(int depth)
{
	return 8 * CP_SEARCH;
}
INLINE int ExclDouble(int depth)
{
	return 16 * CP_SEARCH;
}

// EVAL

const sint8 DistC[8] = { 3, 2, 1, 0, 0, 1, 2, 3 };
const sint8 RankR[8] = { -3, -2, -1, 0, 1, 2, 3, 4 };

static constexpr uint16 SeeValue[16] = { 0, 0, 360, 360, 1300, 1300, 1300, 1300, 1300, 1300, 2040, 2040, 3900, 3900, 30000, 30000 };
static constexpr int PieceType[16] = { 0, 0, 0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 4, 4, 5, 5 };
static constexpr array<int, 5> Phase = { 0, SeeValue[4], SeeValue[6], SeeValue[10], SeeValue[12] };
static constexpr int PhaseMin = 2 * Phase[3] + Phase[1] + Phase[2];
static constexpr int PhaseMax = 16 * Phase[0] + 3 * Phase[1] + 3 * Phase[2] + 4 * Phase[3] + 2 * Phase[4];

#define V(x) (x)

INLINE constexpr int ArrayIndex(int width, int row, int column)
{
	return row * width + column;
}
template<class T_> constexpr auto Av(const T_& x, int width, int row, int column) -> decltype(x[0])
{
	return x[ArrayIndex(width, row, column)];
}
template<class T_> constexpr auto TrAv(const T_& x, int w, int r, int c) -> decltype(x[0])
{
	return Av(x, 0, 0, ((r * (2 * w - r + 1)) / 2) + c);
}
template<class T_> constexpr auto Sa(const T_& x, int y) -> decltype(x[0])
{
	return Av(x, 0, 0, y);
}
template<class C_> INLINE constexpr packed_t Ca4(const C_& x, int y)
{
	return Pack4(Av(x, 4, y, 0), Av(x, 4, y, 1), Av(x, 4, y, 2), Av(x, 4, y, 3));
}

// EVAL WEIGHTS

// tuner: start
enum
{  // tuner: enum
	IMatLinear,
	IMatQuadMe = IMatLinear + 5,
	IMatQuadOpp = IMatQuadMe + 14,
	IBishopPairQuad = IMatQuadOpp + 10,
	iMatClosed = IBishopPairQuad + 9,
	IMatSpecial = iMatClosed + 6,
	IPstQuadWeights = IMatSpecial + 40,
	IPstLinearWeights = IPstQuadWeights + 96,
	IPstQuadMixedWeights = IPstLinearWeights + 96,
	IMobilityLinear = IPstQuadMixedWeights + 48,
	IMobilityLog = IMobilityLinear + 16,
	IMobilityLocus = IMobilityLog + 16,
	IShelterValue = IMobilityLocus + 16,
	IStormQuad = IShelterValue + 15,
	IStormLinear = IStormQuad + 5,
	IStormHof = IStormLinear + 5,
	IPasserQuad = IStormHof + 2,
	IPasserLinear = IPasserQuad + 36,
	IPasserConstant = IPasserLinear + 36,
	IPasserAttDefQuad = IPasserConstant + 36,
	IPasserAttDefLinear = IPasserAttDefQuad + 4,
	IPasserAttDefConst = IPasserAttDefLinear + 4,
	IPasserSpecial = IPasserAttDefConst + 4,
	IIsolated = IPasserSpecial + 12,
	IUnprotected = IIsolated + 10,
	IBackward = IUnprotected + 6,
	IDoubled = IBackward + 4,
	IRookSpecial = IDoubled + 4,
	ITactical = IRookSpecial + 20,
	IKingDefence = ITactical + 12,
	IPawnSpecial = IKingDefence + 8,
	IBishopSpecial = IPawnSpecial + 8,
	IKnightSpecial = IBishopSpecial + 4,
	IPin = IKnightSpecial + 10,
	IKingRay = IPin + 10,
	IKingAttackWeight = IKingRay + 6
};

// pawn, knight, bishop, rook, queen, pair
static constexpr array<int, 6> MatLinear = { 39, -11, -14, 86, -15, -1 };
static constexpr int MatWinnable = 160;

// T(pawn), pawn, knight, bishop, rook, queen
const int MatQuadMe[21] = { // tuner: type=array, var=1000, active=0
	NULL, 0, 0, 0, 0, 0,
	-33, 17, -23, -155, -247,
	15, 296, -105, -83,
	-162, 327, 315,
	-861, -1013,
	NULL
};
const int MatQuadOpp[15] = { // tuner: type=array, var=1000, active=0
	0, 0, 0, 0, 0,
	-14, -96, -20, -278,
	35, 39, 49,
	9, -2,
	75
};
const int BishopPairQuad[9] = { // tuner: type=array, var=1000, active=0
	-38, 164, 99, 246, -84, -57, -184, 88, -186
};
static constexpr array<int, 6> MatClosed = { -20, 22, -33, 18, -2, 26 };

namespace Params
{
	enum
	{
		MatRB,
		MatRN,
		MatQRR,
		MatQRB,
		MatQRN,
		MatQ3,
		MatBBR,
		MatBNR,
		MatNNR,
		MatM,
		MatPawnOnly
	};
	static constexpr array<int, 44> MatSpecial = {  // tuner: type=array, var=120, active=0
		52, 0, -52, 0,
		40, 2, -36, 0,
		32, 40, 48, 0,
		16, 20, 24, 0,
		20, 28, 36, 0,
		-12, -22, -32, 0,
		-10, 20, 64, 0,
		6, 21, 20, 0,
		0, -12, -24, 0,
		4, 8, 12, 0,
		0, 0, 0, -100 };
}
namespace Values
{
#define VALUE(name) static constexpr packed_t name = Ca4(Params::MatSpecial, Params::name)
	VALUE(MatRB);
	VALUE(MatRN);
	VALUE(MatQRR);
	VALUE(MatQRB);
	VALUE(MatQRN);
	VALUE(MatQ3);
	VALUE(MatBBR);
	VALUE(MatBNR);
	VALUE(MatNNR);
	VALUE(MatM);
	VALUE(MatPawnOnly);
#undef VALUE
}

namespace PstW
{
	struct Weights_
	{
		struct Phase_
		{
			array<int, 4> quad_;
			array<int, 4> linear_;
			array<int, 2> quadMixed_;
		} op_, md_, eg_, cl_;
	};

	static constexpr Weights_ Pawn = {
		{ { -48, -275, 165, 0 },{ -460, -357, -359, 437 },{ 69, -28 } },
	{ { -85, -171, 27, 400 },{ -160, -133, 93, 1079 },{ 13, -6 } },
	{ { -80, -41, -85, 782 },{ 336, 303, 295, 1667 },{ -35, 13 } },
	{ { 2, 13, 11, 23 },{ 6, 14, 37, -88 },{ 14, -2 } } };
	static constexpr Weights_ Knight = {
		{ { -134, 6, -12, -72 },{ -680, -343, -557, 1128 },{ -32, 14 } },
	{ { -315, -123, -12, -90 },{ -449, -257, -390, 777 },{ -24, -3 } },
	{ { -501, -246, -12, -107 },{ 61, -274, -357, 469 },{ -1, -16 } },
	{ { -12, -5, -2, -22 },{ 96, 69, -64, -23 },{ -5, -8 } } };
	static constexpr Weights_ Bishop = {
		{ { -123, -62, 54, -116 },{ 24, -486, -350, -510 },{ 8, -58 } },
	{ { -168, -49, 24, -48 },{ -323, -289, -305, -254 },{ -7, -21 } },
	{ { -249, -33, 4, -14 },{ -529, -232, -135, 31 },{ -32, 0 } },
	{ { 4, -10, 9, -13 },{ 91, -43, -34, 29 },{ -13, -10 } } };
	static constexpr Weights_ Rook = {
		{ { -260, 12, -49, 324 },{ -777, -223, 245, 670 },{ -7, -25 } },
	{ { -148, -88, -9, 165 },{ -448, -278, -63, 580 },{ -7, 0 } },
	{ { 13, -149, 14, 46 },{ -153, -225, -246, 578 },{ -6, 16 } },
	{ { 0, 8, -15, 8 },{ -32, -29, 10, -51 },{ -6, -23 } } };
	static constexpr Weights_ Queen = {
		{ { -270, -18, -19, -68 },{ -520, 444, 474, -186 },{ 18, -6 } },
	{ { -114, -209, 21, -103 },{ -224, -300, 73, 529 },{ -13, 1 } },
	{ { 2, -341, 58, -160 },{ 40, -943, -171, 1328 },{ -34, 27 } },
	{ { -3, -26, 9, 5 },{ -43, -18, -107, 60 },{ 5, 12 } } };
	static constexpr Weights_ King = {
		{ { -266, -694, -12, 170 },{ 1077, 3258, 20, -186 },{ -18, 3 } },
	{ { -284, -451, -31, 43 },{ 230, 1219, -425, 577 },{ -1, 5 } },
	{ { -334, -157, -67, -93 },{ -510, -701, -863, 1402 },{ 37, -8 } },
	{ { 22, 14, -16, 0 },{ 7, 70, 40,  78 } ,{ 9, -3 } } };
}

// coefficient (Linear, Log, Locus) * phase (4)
static constexpr array<int, 12> MobCoeffsKnight = { 1281, 857, 650, 27, 2000, 891, 89, -175, 257, 289, -47, 163 };
static constexpr array<int, 12> MobCoeffsBishop = { 1484, 748, 558, 127, 1687, 1644, 1594, -565, -96, 437, 136, 502 };
static constexpr array<int, 12> MobCoeffsRook = { 1096, 887, 678, 10, -565, 248, 1251, -5, 74, 72, 45, -12 };
static constexpr array<int, 12> MobCoeffsQueen = { 597, 876, 1152, -7, 1755, 324, -1091, -9, 78, 109, 17, -12 };
static constexpr int N_LOCUS = 22;

// file type (3) * distance from 2d rank/open (5)
static constexpr array<int, 15> ShelterValue = {  // tuner: type=array, var=26, active=0
	8, 36, 44, 0, 0,	// h-pawns
	48, 72, 44, 0, 8,	// g
	96, 28, 32, 0, 0	// f
};

enum
{
	StormHofValue,
	StormHofScale,
	StormOfValue,
	StormOfScale
};
static constexpr array<int, 4> ShelterMod = { 0, 0, 88, 0 };

enum
{
	StormBlockedMul,
	StormShelterAttMul,
	StormConnectedMul,
	StormOpenMul,
	StormFreeMul
};
static constexpr array<int, 5> StormQuad = {  // tuner: type=array, var=640, active=0
	504, 1312, 1852, 860, 356
};
static constexpr array<int, 5> StormLinear = {  // tuner: type=array, var=1280, active=0
	332, 624, 1752, 1284, 48
};

// type (9: general, blocked, free, supported, protected, connected, outside, candidate, clear) * phase (4)
static constexpr array<int, 36> PasserQuad = {  // tuner: type=array, var=128, active=0
	76, 64, 52, 0,
	84, 48, 12, 0,
	-96, 204, 504, 0,
	0, 130, 260,  0,
	128, 176, 224,  0,
	36, 14, -6,  0,
	128, 32, -64, 0,
	52, 34, 16,  0,
	4, 4, 4, 0 };
static constexpr array<int, 36> PasserLinear = {  // tuner: type=array, var=512, active=0
	164, 86, 8, 0,
	444, 394, 344, 0,
	712, 582, 452, 0,
	808, 434, 60, 0,
	-244, -80, 84, 0,
	124, 172, 221, 0,
	344, 356, 368, 0,
	108, 122, 136, 0,
	-72, -50, -28, 0 };
static constexpr array<int, 36> PasserConstant = {  // tuner: type=array, var=2048, active=0
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0 };
// type (2: att, def) * scaling (2: linear, log) 
static constexpr array<int, 4> PasserAttDefQuad = { // tuner: type=array, var=500, active=0
	764, 204, 332, 76
};
static constexpr array<int, 4> PasserAttDefLinear = { // tuner: type=array, var=500, active=0
	2536, 16, 932, 264
};
static constexpr array<int, 4> PasserAttDefConst = { // tuner: type=array, var=500, active=0
	0, 0, 0, 0
};
enum { PasserOnePiece, PasserOpKingControl, PasserOpMinorControl, PasserOpRookBlock };
// case(4) * phase(3 -- no opening)
static constexpr array<int, 12> PasserSpecial = { // tuner: type=array, var=100, active=0
	26, 52, 0
};
namespace Values
{
	static constexpr packed_t PasserOpRookBlock =
		Pack4(0, Av(PasserSpecial, 3, ::PasserOpRookBlock, 0), Av(PasserSpecial, 3, ::PasserOpRookBlock, 1), Av(PasserSpecial, 3, ::PasserOpRookBlock, 2));
}

namespace Params
{
	enum
	{
		IsolatedOpen,
		IsolatedClosed,
		IsolatedBlocked,
		IsolatedDoubledOpen,
		IsolatedDoubledClosed
	};
	static constexpr array<int, 20> Isolated = {
		36, 28, 19, 1,
		40, 21, 1, 12,
		-40, -20, -3, -3,
		0, 10, 45, 3,
		27, 27, 36, 8 };
}
namespace Values
{
#define VALUE(name) static constexpr packed_t name = Ca4(Params::Isolated, Params::name)
	VALUE(IsolatedOpen);
	VALUE(IsolatedClosed);
	VALUE(IsolatedBlocked);
	VALUE(IsolatedDoubledOpen);
	VALUE(IsolatedDoubledClosed);
#undef VALUE
}

namespace Params
{
	enum
	{
		UpBlocked,
		PasserTarget,
		ChainRoot
	};
	static constexpr array<int, 12> Unprotected = {  // tuner: type=array, var=26, active=0
		16, 18, 20, 0,
		-20, -12, -4, 0,
		36, 16, -4, 0 };
}
namespace Values
{
#define VALUE(name) static constexpr packed_t name = Ca4(Params::Unprotected, Params::name)
	VALUE(UpBlocked);
	VALUE(PasserTarget);
	VALUE(ChainRoot);
#undef VALUE
}

namespace Params
{
	enum
	{
		BackwardOpen,
		BackwardClosed
	};
	static constexpr array<int, 8> Backward = {  // tuner: type=array, var=26, active=0
		68, 54, 40, 0,
		16, 10, 4, 0 };
}
namespace Values
{
#define VALUE(name) static constexpr packed_t name = Ca4(Params::Backward, Params::name)
	VALUE(BackwardOpen);
	VALUE(BackwardClosed);
#undef VALUE
}

namespace Params
{
	enum
	{
		DoubledOpen,
		DoubledClosed
	};
	static constexpr array<int, 8> Doubled = {  // tuner: type=array, var=26, active=0
		12, 6, 0, 0,
		4, 2, 0, 0 };
}
namespace Values
{
#define VALUE(name) static constexpr packed_t name = Ca4(Params::Doubled, Params::name)
	VALUE(DoubledOpen);
	VALUE(DoubledClosed);
#undef VALUE
}

namespace Params
{
	enum
	{
		RookHof,
		RookHofWeakPAtt,
		RookOf,
		RookOfOpen,
		RookOfMinorFixed,
		RookOfMinorHanging,
		RookOfKingAtt,
		Rook7th,
		Rook7thK8th,
		Rook7thDoubled
	};
	static constexpr array<int, 40> RookSpecial = {  // tuner: type=array, var=26, active=0
		32, 16, 0, 0,
		8, 4, 0, 0,
		44, 38, 32, 0,
		-4, 2, 8, 0,
		-4, -4, -4, 0,
		56, 26, -4, 0,
		20, 0, -20, 0,
		-20, -10, 0, 0,
		-24, 4, 32, 0,
		-28, 48, 124, 0 };
}
namespace Values
{
#define VALUE(name) static constexpr packed_t name = Ca4(Params::RookSpecial, Params::name)
	VALUE(RookHof);
	VALUE(RookHofWeakPAtt);
	VALUE(RookOf);
	VALUE(RookOfOpen);
	VALUE(RookOfMinorFixed);
	VALUE(RookOfMinorHanging);
	VALUE(RookOfKingAtt);
	VALUE(Rook7th);
	VALUE(Rook7thK8th);
	VALUE(Rook7thDoubled);
#undef VALUE
}

namespace Params
{
	enum
	{
		TacticalMajorPawn,
		TacticalMinorPawn,
		TacticalMajorMinor,
		TacticalMinorMinor,
		TacticalThreat,
		TacticalDoubleThreat,
		TacticalUnguardedQ
	};
	static constexpr array<int, 32> Tactical = {  // tuner: type=array, var=51, active=0
		-6, 12, 23, 0,
		-1, 10, 22, -1,
		34, 81, 112, 5,
		76, 105, 133, -1,
		79, 64, 45, -3,
		164, 106, 48, 0,
		-1,	9,	39,	-10
	};
}
namespace Values
{
#define VALUE(name) static constexpr packed_t name = Ca4(Params::Tactical, Params::name)
	VALUE(TacticalMajorPawn);
	VALUE(TacticalMinorPawn);
	VALUE(TacticalMajorMinor);
	VALUE(TacticalMinorMinor);
	VALUE(TacticalThreat);
	VALUE(TacticalDoubleThreat);
	VALUE(TacticalUnguardedQ);
#undef VALUE
}

namespace Params
{
	enum
	{
		KingDefKnight,
		KingDefBishop,
		KingDefQueen
	};
	static constexpr array<int, 12> KingDefence = {  // tuner: type=array, var=13, active=0
		8, 4, 0, 0,
		0, 2, 4, 0,
		16, 8, 0, 0 };
}
namespace Values
{
#define VALUE(name) static constexpr packed_t name = Ca4(Params::KingDefence, Params::name)
	VALUE(KingDefKnight);
	VALUE(KingDefBishop);
	VALUE(KingDefQueen);
#undef VALUE
}

namespace Params
{
	enum
	{
		PawnChainLinear,
		PawnChain,
		PawnBlocked,
		PawnFileSpan
	};
	static constexpr array<int, 16> PawnSpecial = {  // tuner: type=array, var=26, active=0
		44, 40, 36, 0,
		36, 26, 16, 0,
		0, 18, 36, 0,
		4, 4, 4, 0,
	};
}
namespace Values
{
#define VALUE(name) static constexpr packed_t name = Ca4(Params::PawnSpecial, Params::name)
	VALUE(PawnChainLinear);
	VALUE(PawnChain);
	VALUE(PawnBlocked);
	VALUE(PawnFileSpan);
#undef VALUE
}

namespace Params
{
	enum
	{
		BishopPawnBlock,
		BishopOppPawnOffColor,
		BishopOppPawnBlock,
		BishopOutpostNoMinor
	};
	static constexpr array<int, 16> BishopSpecial = { // tuner: type=array, var=20, active=0
		0, 6, 14, 6,
		12, 4, 12, 0,
		8, 10, 8, 2,
		60, 60, 45, 0
	};
}
namespace Values
{
#define VALUE(name) static constexpr packed_t name = Ca4(Params::BishopSpecial, Params::name)
	VALUE(BishopPawnBlock);
	VALUE(BishopOppPawnOffColor);
	VALUE(BishopOppPawnBlock);
	VALUE(BishopOutpostNoMinor);
#undef VALUE
}

namespace Params
{
	enum
	{
		KnightOutpost,
		KnightOutpostProtected,
		KnightOutpostPawnAtt,
		KnightOutpostNoMinor,
		KnightPawnSpread,
		KnightPawnGap
	};
	static constexpr array<int, 20> KnightSpecial = {  // tuner: type=array, var=26, active=0
		40, 40, 24, 0,
		41, 40, 0, 0,
		44, 44, 18, 0,
		41, 40, 0, 0,
		0, 4, 15, -10,
		0, 2, 5, 0
	};
}
namespace Values
{
#define VALUE(name) static constexpr packed_t name = Ca4(Params::KnightSpecial, Params::name)
	VALUE(KnightOutpost);
	VALUE(KnightOutpostProtected);
	VALUE(KnightOutpostPawnAtt);
	VALUE(KnightOutpostNoMinor);
	VALUE(KnightPawnSpread);
	VALUE(KnightPawnGap);
#undef VALUE
}

namespace Params
{
	enum
	{
		WeakPin,
		StrongPin,
		ThreatPin,
		SelfPawnPin,
		SelfPiecePin
	};
	static constexpr array<int, 20> Pin = {  // tuner: type=array, var=51, active=0
		84, 120, 156, 0,
		24, 172, 320, 0,
		180, 148, 116, 0,
		32, 34, 36, 0,
		245, 191, 138, 0 };
}
namespace Values
{
#define VALUE(name) static constexpr packed_t name = Ca4(Params::Pin, Params::name)
	VALUE(WeakPin);
	VALUE(StrongPin);
	VALUE(ThreatPin);
	VALUE(SelfPawnPin);
	VALUE(SelfPiecePin);
#undef VALUE
}

namespace Params
{
	enum
	{
		QKingRay,
		RKingRay,
		BKingRay
	};
	static constexpr array<int, 12> KingRay = {  // tuner: type=array, var=51, active=0
		17, 26, 33, -2,
		-14, 15, 42, 0,
		43, 14, -9, -1 };
}
namespace Values
{
#define VALUE(name) static constexpr packed_t name = Ca4(Params::KingRay, Params::name)
	VALUE(QKingRay);
	VALUE(RKingRay);
	VALUE(BKingRay);
#undef VALUE
}

static constexpr array<int, 11> KingAttackWeight = {  // tuner: type=array, var=51, active=0
	56, 88, 44, 64, 60, 104, 116, 212, 192, 256, 64 };
constexpr uint16 KingAttackThreshold = 48;

static constexpr array<uint64, 2> Outpost = { 0x00007E7E3C000000ull, 0x0000003C7E7E0000ull };
static constexpr array<int, 2> PushW = { 7, -9 };
static constexpr array<int, 2> Push = { 8, -8 };
static constexpr array<int, 2> PushE = { 9, -7 };

static constexpr uint32 KingNAttack1 = UPack(1, KingAttackWeight[0]);
static constexpr uint32 KingNAttack = UPack(2, KingAttackWeight[1]);
static constexpr uint32 KingBAttack1 = UPack(1, KingAttackWeight[2]);
static constexpr uint32 KingBAttack = UPack(2, KingAttackWeight[3]);
static constexpr uint32 KingRAttack1 = UPack(1, KingAttackWeight[4]);
static constexpr uint32 KingRAttack = UPack(2, KingAttackWeight[5]);
static constexpr uint32 KingQAttack1 = UPack(1, KingAttackWeight[6]);
static constexpr uint32 KingQAttack = UPack(2, KingAttackWeight[7]);
static constexpr uint32 KingPAttack = UPack(2, 0);
static constexpr uint32 KingAttack = UPack(1, 0);
static constexpr uint32 KingAttackSquare = KingAttackWeight[8];
static constexpr uint32 KingNoMoves = KingAttackWeight[9];
static constexpr uint32 KingShelterQuad = KingAttackWeight[10];	// a scale factor, not a score amount

template<int N> array<uint16, N> CoerceUnsigned(const array<int, N>& src)
{
	array<uint16, N> retval;
	for (int ii = 0; ii < N; ++ii)
		retval[ii] = static_cast<uint16>(max(0, src[ii]));
	return retval;
}
static constexpr array<uint16, 16> XKingAttackScale = { 0, 1, 1, 2, 4, 5, 8, 12, 15, 19, 23, 28, 34, 39, 39, 39 };

// tuner: stop

// END EVAL WEIGHTS
time_point now()
{
	return std::chrono::high_resolution_clock::now();
}
uint32 millisecs(const time_point& t1, const time_point& t2)
{
	auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
	return static_cast<uint32>(fabs(time_span.count()) * 1000.0);
}

#define log_msg(...)
#define error_msg(format, ...)                                              \
    do {                                                                \
        log_msg("error_msg: " format "\n", ##__VA_ARGS__);                      \
        fprintf(stderr, "error_msg: " format "\n", ##__VA_ARGS__);          \
        abort();                                                        \
    } while (false)

// data sharing for multithreading
void delete_object(void *addr, size_t size)
{
	if (!UnmapViewOfFile(addr))
		error_msg("failed to unmap object (%d)", GetLastError());
}

// SMP

// Windows threading routines
constexpr size_t PAGE_SIZE = 4096;
constexpr int PIPE_BUF = 4096;
constexpr int PATH_MAX = 4096;
inline size_t size_to_page(size_t size)
{
	return ((size - 1) / PAGE_SIZE) * PAGE_SIZE + PAGE_SIZE;
}
int get_num_cpus()
{
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return sysinfo.dwNumberOfProcessors;
}

mutex_t mutex_init()
{
	SECURITY_ATTRIBUTES attr;
	memset(&attr, 0, sizeof(attr));
	attr.nLength = sizeof(attr);
	attr.bInheritHandle = TRUE;
	mutex_t retval = CreateMutex(&attr, FALSE, NULL);
	if (retval == NULL)
		error_msg("failed to create mutex (%d)", GetLastError());
	return retval;
}

void mutex_lock0(mutex_t *mutex)
{
	if (WaitForSingleObject(*mutex, INFINITE) != WAIT_OBJECT_0)
		error_msg("failed to lock mutex (%d)", GetLastError());
}
static bool mutex_try_lock(mutex_t *mutex, uint64_t timeout)
{
	switch (WaitForSingleObject(*mutex, (DWORD)timeout))
	{
	case WAIT_OBJECT_0:
		return true;
	case WAIT_TIMEOUT:
		return false;
	default:
		error_msg("failed to lock mutex (%d)", GetLastError());
	}
}

void mutex_unlock0(mutex_t *mutex)
{
	if (!ReleaseMutex(*mutex))
		error_msg("failed to unlock mutex (%d)", GetLastError());
}

void mutex_discard(mutex_t* mutex)
{
	mutex_unlock0(mutex);
	//	CloseHandle(mutex);
}

event_t event_init()
{
	SECURITY_ATTRIBUTES attr;
	memset(&attr, 0, sizeof(attr));
	attr.nLength = sizeof(attr);
	attr.bInheritHandle = TRUE;
	event_t retval = CreateEvent(&attr, TRUE, FALSE, NULL);
	if (retval == NULL)
		error_msg("failed to create event (%d)", GetLastError());
	return retval;
}

void event_discard(event_t* event)
{
	if (event)
		CloseHandle(event);
}

string object_name(const char *basename, int id, int idx)
{
	return "Local\\Roc" + to_string(id) + basename + to_string(idx);
}

static DWORD forward_input(void* param)
{
	char buf[4 * PIPE_BUF];
	HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
	HANDLE out = (HANDLE)param;
	while (true)
	{
		DWORD len;
		if (!ReadFile(in, buf, sizeof(buf), &len, NULL))
			error_msg("failed to read input (%d)", GetLastError());
		if (len == 0)
		{
			CloseHandle(out);
			return 0;
		}

		DWORD ptr = 0;
		while (ptr < len)
		{
			DWORD writelen;
			if (!WriteFile(out, buf + ptr, len - ptr, &writelen, NULL))
				error_msg("failed to forward input (%d)", GetLastError());
			ptr += writelen;
		}

		FlushFileBuffers(out);
	}
}

bool get_line(char *line, unsigned linelen, DWORD timeout)
{
	static char buf[4 * PIPE_BUF];
	static unsigned ptr = 0, end = 0;
	unsigned i = 0;

	static HANDLE handle = INVALID_HANDLE_VALUE;
	static event_t event = NULL;
	static bool init = false;
	if (!init)
	{
		handle = GetStdHandle(STD_INPUT_HANDLE);
		if (GetFileType(handle) == FILE_TYPE_PIPE)
		{
			char name[256];
			int res = snprintf(name, sizeof(name) - 1, "\\\\.\\pipe\\Roc%u_pipe", GetCurrentProcessId());
			if (res < 0 || res >= sizeof(name) - 1)
				error_msg("failed to create pipe name");
			HANDLE out = CreateNamedPipe(name,
				PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
				PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 2,
				4 * PIPE_BUF, 4 * PIPE_BUF, 0, NULL);
			if (out == INVALID_HANDLE_VALUE)
				error_msg("failed to create named pipe #1 (%d)", GetLastError());
			handle = CreateFile(name, GENERIC_READ, 0, NULL,
				OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
			if (handle == INVALID_HANDLE_VALUE)
				error_msg("failed to create named pipe #2 (%d)", GetLastError());
			HANDLE thread = CreateThread(NULL, 0, forward_input, (LPVOID)out, 0, NULL);
			if (thread == NULL)
				error_msg("failed to create thread (%d)", GetLastError());
		}
		event = event_init();
		init = true;
	}

	while (true)
	{
		bool space = false;
		while (ptr < end)
		{
			if (i >= linelen)
				error_msg("input buffer overflow");
			char c = buf[ptr++];
			switch (c)
			{
			case ' ': case '\r': case '\t': case '\n':
				if (!space)
				{
					line[i++] = ' ';
					space = true;
				}
				if (c == '\n')
				{
					line[i - 1] = '\0';
					return false;
				}
				continue;
			default:
				space = false;
				line[i++] = c;
				continue;
			}
		}

		OVERLAPPED overlapped;
		memset(&overlapped, 0, sizeof(overlapped));
		overlapped.hEvent = event;
		DWORD len;
		if (!ReadFile(handle, buf, sizeof(buf), &len, &overlapped))
		{
			if (GetLastError() != ERROR_IO_PENDING)
				error_msg("failed to read input (%d)", GetLastError());
			bool timedout = false;

			switch (WaitForSingleObject(event, timeout))
			{
			case WAIT_TIMEOUT:
				if (!CancelIo(handle))
					error_msg("failed to cancel input (%d)", GetLastError());
				timedout = true;
				break;
			case WAIT_OBJECT_0:
				break;
			default:
				error_msg("failed to wait for input (%d)", GetLastError());
			}
			if (!GetOverlappedResult(handle, &overlapped, &len, FALSE))
			{
				if (timedout && GetLastError() == ERROR_OPERATION_ABORTED)
					return true;
				error_msg("failed to get input result (%d)", GetLastError());
			}
		}

		if (len == 0)
		{
			line[0] = EOF;
			return false;
		}

		ptr = 0;
		end = len;
	}
}

static void put_line(const char *line, size_t linelen)
{
	if (linelen > PIPE_BUF)
	{
		log_msg("warning: output \"%s\" too long (max is %u, got %u)\n", line, PIPE_BUF, linelen);
		return;
	}
	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD len;
	if (!WriteFile(handle, line, (DWORD)linelen, &len, NULL) || len != linelen)
		error_msg("failed to write output (%d)", GetLastError());
	FlushFileBuffers(handle);
}

typedef struct
{
	char name[256];
	HANDLE handle;
} GHandleInfo;
vector<GHandleInfo> MyHandles;

void* init_object_ex(const char *mapping_name, size_t true_size, void *addr, bool create, bool readonly, const void *value)
{
	size_t size = size_to_page(true_size);
	DWORD access = readonly && !value ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS;
	HANDLE handle = INVALID_HANDLE_VALUE;
	if (mapping_name)
	{
		if (create)
		{
			handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_EXECUTE_READWRITE, (DWORD)(size >> 32), (DWORD)(size & 0xFFFFFFFF), mapping_name);
			GHandleInfo hi;
			hi.handle = handle;
			strncpy(hi.name, mapping_name, sizeof(hi.name) - 1);
			MyHandles.push_back(hi);
		}
		else
		{
			handle = OpenFileMapping(access, FALSE, mapping_name);
		}
	}
	else
		handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, (DWORD)(size >> 32), (DWORD)(size & 0xFFFFFFFF), NULL);

	if ((handle == INVALID_HANDLE_VALUE || handle == NULL) && (create || GetLastError() != ERROR_ALREADY_EXISTS))
		error_msg("failed to open file mapping \"%s\" (%d)", mapping_name, GetLastError());
	void* ptr = MapViewOfFileEx(handle, access, 0, 0, size, addr);
	if (!ptr)
		error_msg("failed to map file mapping \"%s\" (%d)", mapping_name, GetLastError());
	if (value)
	{
		memcpy(ptr, value, true_size);
		DWORD old_prot;
		if (readonly && !VirtualProtect(ptr, size, PAGE_READONLY, &old_prot))
			error_msg("failed to protect object \"%s\" (%d)", mapping_name, GetLastError());
	}
	if (!create || !mapping_name)
		CloseHandle(handle);
	return ptr;
}

void* share_object_ex(const char *mapping_name, void *addr, bool readonly)
{
	DWORD access = readonly ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS;
	HANDLE handle = OpenFileMapping(access, FALSE, mapping_name);
	if ((handle == INVALID_HANDLE_VALUE || handle == NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
		error_msg("failed to open file mapping \"%s\" (%d)", mapping_name, GetLastError());
	void* ptr = MapViewOfFileEx(handle, access, 0, 0, 0, addr);
	if (!ptr)
		error_msg("failed to map file mapping \"%s\" (%d)", mapping_name, GetLastError());
	CloseHandle(handle);
	return ptr;
}

template<class T_> T_* init_object(T_* base_addr, const char *object, bool create, bool readonly, size_t n = 1, const T_* value = nullptr)
{
	return reinterpret_cast<T_*>(init_object_ex(object, n * sizeof(T_), base_addr, create, readonly, value));
}
template<class T_> T_* share_object(T_* base_addr, const char *object, bool readonly)
{
	return reinterpret_cast<T_*>(share_object_ex(object, base_addr, readonly));
}

void remove_object(const char *object)
{
	for (auto&& hi : MyHandles)
	{
		if (strcmp(hi.name, object) == 0)
		{
			hi.name[0] = '\0';
			CloseHandle(hi.handle);
			hi.handle = NULL;
			return;
		}
	}
	error_msg("failed to remove object \"%s\"", object);
}

void event_signal(event_t* event)
{
	SetEvent(*event);
	ResetEvent(*event);
}

#ifdef DEBUG
constexpr int maxNumThreads = 1;
#else
#ifdef W32_BUILD
constexpr int maxNumThreads = 32;  // mustn't exceed 32
#else
constexpr int maxNumThreads = 256;
#endif
#endif

constexpr int MAX_PV_LEN = 256;
struct ThreadOwn_
{
	size_t nodes;
	size_t tbHits;
	int pid;
	int id;
	int depth;
	int selDepth;
	int bestMove;
	int bestScore;
	int PV[MAX_PV_LEN];
	bool newGame;
};
vector<ThreadOwn_*> THREADS;
ThreadOwn_* INFO = 0;
uint64 check_node;

constexpr size_t HASH_CLUSTER = 4;
struct Settings_
{
	int nThreads = 1;
	int tbMinDepth = 7;
	size_t nHash = 0;
	size_t hashMask = 0;
	int contempt = 8;
	int wobble = 0;
	unsigned parentPid = numeric_limits<unsigned>::max();
	char tbPath[PATH_MAX];
};
Settings_* SETTINGS = 0;

struct WorkerList_
{
	struct Record_
	{
		int pvs_, next_;
	};
	array<Record_, maxNumThreads + 1> vals_;
	void Init()
	{
		for (int jj = 0; jj <= maxNumThreads; ++jj)
			vals_[jj] = { jj, jj };
	}
	void Insert(int iw);
	void Remove(int iw);
	bool Empty() const
	{
		return vals_[maxNumThreads].pvs_ == maxNumThreads;
	}
};

struct SharedInfo_
{
	WorkerList_ working;
	Progress_ rootProgress;
	mutex_t mutex = NULL;
	mutex_t outMutex = NULL;
	event_t goEvent = NULL;
	int rootDepth;
	int depthLimit;
	time_point startTime;
	uint64_t softTimeLimit;
	uint64_t hardTimeLimit;
	GBoard rootBoard;
	GData rootData;
	struct {
		int depth, value, move, worker;
		bool failLow;
	} best;
	uint64_t rootStack[1024];
	unsigned rootSp;
	bool stopAll;
	uint16_t date;
	volatile bool ponder;
};

SharedInfo_* SHARED = 0;
inline bool Progress_::Reachable() const
{
	assert(SHARED);
	if ((count_ >> 4) > (SHARED->rootProgress.count_ >> 4))
		return false;
	if ((count_ & 0xF) > (SHARED->rootProgress.count_ & 0xF))
		return false;
	return true;
}

void mutex_lock1(mutex_t *mutex, int lineno)
{
	mutex_lock0(mutex);
	if (SHARED && !SHARED->working.Empty())
	{
		string what = "id name locked to " + Str(GetCurrentProcessId()) + " at " + Str(lineno) + "\n";
		put_line(what.c_str(), what.size());
	}
}
void mutex_unlock1(mutex_t *mutex, int lineno)
{
	if (SHARED && !SHARED->stopAll)
	{
		char line[64];
		int len = snprintf(line, sizeof(line) - 1, "id name unlocked by %d at %d\n", GetCurrentProcessId(), lineno);
		put_line(line, len);
	}
	mutex_unlock0(mutex);
}

#define mutex_lock(M) mutex_lock1(M, __LINE__)
#define mutex_unlock(M) mutex_unlock1(M, __LINE__)


struct SharedLock_
{
	int line_;
	SharedLock_(int line) : line_(line)
	{
		mutex_lock0(&SHARED->mutex);
	}
	~SharedLock_()
	{
		mutex_unlock0(&SHARED->mutex);
	}
};
#define LOCK_SHARED SharedLock_ _(__LINE__);

void Say(const string& what)
{
	if (SHARED)
		mutex_lock0(&SHARED->outMutex);
	put_line(what.c_str(), what.size());
	if (SHARED)
		mutex_unlock0(&SHARED->outMutex);
}
#ifdef VERBOSE
#define VSAY Say
static int debug_loc = 0;
#define HERE debug_loc = __LINE__;
#else
#define VSAY(x)
#define HERE
#endif

void WorkerList_::Insert(int iw)
{
	assert(iw >= 0 && iw < maxNumThreads);
	assert(vals_[iw].pvs_ == iw && vals_[iw].next_ == iw);	// double insertion
	VSAY("Adding worker " + Str(iw) + "\n");
	int last = vals_[maxNumThreads].pvs_;
	vals_[iw].pvs_ = last;
	vals_[iw].next_ = maxNumThreads;
	vals_[last].next_ = iw;
	vals_[maxNumThreads].pvs_ = iw;
}
void WorkerList_::Remove(int iw)
{
	assert(iw >= 0 && iw < maxNumThreads);
	assert(vals_[iw].pvs_ != iw && vals_[iw].next_ != iw);	// double deletion
	VSAY("Removing worker " + Str(iw) + "\n");
	vals_[vals_[iw].pvs_].next_ = vals_[iw].next_;
	vals_[vals_[iw].next_].pvs_ = vals_[iw].pvs_;
	vals_[iw].pvs_ = vals_[iw].next_ = iw;
	//if (Empty()) VSAY("No more workers\n");
}


GEntry* HASH = 0;
GPVEntry* PVHASH = 0;
GPawnEntry* PAWNHASH = 0;

void move_to_string(int move, char string[])
{
	assert(move);
	int pos = 0;
	string[pos++] = ((move >> 6) & 7) + 'a';
	string[pos++] = ((move >> 9) & 7) + '1';
	string[pos++] = (move & 7) + 'a';
	string[pos++] = ((move >> 3) & 7) + '1';
	if (IsPromotion(move))
	{
		if ((move & 0xF000) == FlagPQueen)
			string[pos++] = 'q';
		else if ((move & 0xF000) == FlagPKnight)
			string[pos++] = 'n';
		else if ((move & 0xF000) == FlagPRook)
			string[pos++] = 'r';
		else if ((move & 0xF000) == FlagPBishop)
			string[pos++] = 'b';
	}
	string[pos] = 0;
}

int move_from_string(char string[])
{
	int from, to, move;
	from = ((string[1] - '1') * 8) + (string[0] - 'a');
	to = ((string[3] - '1') * 8) + (string[2] - 'a');
	move = (from << 6) | to;
	if (Board->square[from] >= WhiteKing && abs(to - from) == 2)
		move |= FlagCastling;
	if (T(Current->ep_square) && to == Current->ep_square)
		move |= FlagEP;
	if (string[4] != 0)
	{
		if (string[4] == 'q')
			move |= FlagPQueen;
		else if (string[4] == 'n')
			move |= FlagPKnight;
		else if (string[4] == 'r')
			move |= FlagPRook;
		else if (string[4] == 'b')
			move |= FlagPBishop;
	}
	return move;
}

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

template<int me> inline uint64 PAtts(uint64 p)
{
	uint64 temp = Shift<me>(p);
	return ((temp & 0xFEFEFEFEFEFEFEFE) >> 1) | ((temp & 0x7F7F7F7F7F7F7F7F) << 1);
}

INLINE uint8 FileOcc(uint64 occ)
{
	uint64 t = occ;
	t |= t >> 32;
	t |= t >> 16;
	t |= t >> 8;
	return static_cast<uint8>(t & 0xFF);
}

INLINE int FileSpan(const uint64& occ)
{
	return RO->SpanWidth[FileOcc(occ)];
}


bool IsIllegal(bool me, int move)
{
	return ((HasBit(Current->xray[opp], From(move)) && F(Bit(To(move)) & RO->FullLine[lsb(King(me))][From(move)])) ||
		(IsEP(move) && T(Line[RankOf(From(move))] & King(me)) && T(Line[RankOf(From(move))] & Major(opp)) &&
			T(RookAttacks(lsb(King(me)), PieceAll() ^ Bit(From(move)) ^ Bit(Current->ep_square - Push[me])) & Major(opp))));
};
INLINE bool IsCheck(bool me)
{
	return T(Current->att[(me) ^ 1] & King(me));
};
INLINE bool IsRepetition(int margin, int move)
{
	return margin > 0
		&& Current->ply >= 2
		&& (Current - 1)->move == ((To(move) << 6) | From(move))
		&& F(PieceAt(To(move)))
		&& F((move) & 0xF000);
};

inline uint64 XBMagicAttacks(const CommonData_* data, int i, uint64 occ)
{
	uint64 att = 0;
	for (uint64 u = BMask[i]; T(u); Cut(u))
		if (F(data->Between[i][lsb(u)] & occ))
			att |= data->Between[i][lsb(u)] | Bit(lsb(u));
	return att;
}
INLINE uint64 BMagicAttacks(int i, uint64 occ)
{
	return XBMagicAttacks(RO, i, occ);
}

uint64 XRMagicAttacks(const CommonData_* data, int i, uint64 occ)
{
	uint64 att = 0;
	for (uint64 u = RMask[i]; T(u); Cut(u))
		if (F(data->Between[i][lsb(u)] & occ))
			att |= data->Between[i][lsb(u)] | Bit(lsb(u));
	return att;
}
INLINE uint64 RMagicAttacks(int i, uint64 occ)
{
	return XRMagicAttacks(RO, i, occ);
}

uint16 rand16()
{
	static uint64 seed = 1;
	seed = (seed * 6364136223846793005ull) + 1442695040888963407ull;
	return static_cast<uint16>((seed >> 32) & 0xFFFF);
}

uint64 rand64()
{
	uint64 key = rand16();
	key <<= 16;
	key |= rand16();
	key <<= 16;
	key |= rand16();
	key <<= 16;
	return key | rand16();
}

// DEBUG debug cry for help
static int debugLine;
static bool boardExists = 0;
static bool debugWK = 1;
static bool debugBK = 1;
static std::ofstream SHOUT("C:\\dev\\debug.txt");
constexpr int TheMemorySize = 200;
struct MemEntry_
{
	GData d_;
	GBoard b_;
	int line_;
};
array<MemEntry_, TheMemorySize> TheImages;
thread_local int TheImageLoc = 0;

struct MoveScope_
{
	MoveScope_() {
		boardExists = 1;
	}
	~MoveScope_() {
		boardExists = 0;
	}
};
void AccessViolation(uint64 seed)	// any nonzero input should fail
{
	Say(Str(Current->patt[(seed >> 32) | (seed << 32)]));
}
void UpdateDebug(int line)
{
	debugLine = line;
	//AccessViolation(boardExists && !King(0));
	//AccessViolation(boardExists && !King(1));
	TheImages[TheImageLoc].b_ = *Board;
	TheImages[TheImageLoc].d_ = *Current;
	TheImages[TheImageLoc].line_ = line;
	if (++TheImageLoc == TheMemorySize)
		TheImageLoc = 0;
}



//#define MOVING MoveScope_ debugMoveScope; UpdateDebug(debugLine);
//#define HI UpdateDebug(__LINE__);
//#define BYE UpdateDebug(__LINE__);
//#define MOVING UpdateDebug(__LINE__);
#define HI
#define BYE
#define MOVING

void init_misc(CommonData_* data)
{
	for (int i = 0; i < 64; ++i)
	{
		data->BMagicMask[i] = data->RMagicMask[i] = data->OneIn[i] = 0;
		data->PAtt[0][i] = data->PAtt[1][i] = data->PMove[0][i] = data->PMove[1][i] = data->PWay[0][i] = data->PWay[1][i] = data->PCone[0][i] = data->PCone[1][i]
			= data->PSupport[0][i] = data->PSupport[1][i] = data->BishopForward[0][i] = data->BishopForward[1][i] = 0;
		for (int j = 0; j < 64; ++j)
			data->Between[i][j] = data->FullLine[i][j] = 0;
	}

	for (int i = 0; i < 64; ++i)
	{
		for (int j = 0; j < 64; ++j)
		{
			if (i == j)
				continue;
			uint64 u = Bit(j);
			if (j == i + 8)
				data->PMove[0][i] |= u;
			if (j == i - 8)
				data->PMove[1][i] |= u;
			if (abs(FileOf(i) - FileOf(j)) == 1)
			{
				if (RankOf(j) >= RankOf(i))
				{
					data->PSupport[1][i] |= u;
					if (RankOf(j) - RankOf(i) == 1)
						data->PAtt[0][i] |= u;
				}
				if (RankOf(j) <= RankOf(i))
				{
					data->PSupport[0][i] |= u;
					if (RankOf(i) - RankOf(j) == 1)
						data->PAtt[1][i] |= u;
				}
			}
			else if (FileOf(i) == FileOf(j))
			{
				if (RankOf(j) > RankOf(i))
					data->PWay[0][i] |= u;
				else
					data->PWay[1][i] |= u;
			}
		}

		if (FileOf(i) == 0)
			data->OneIn[i] |= File[1];
		if (FileOf(i) == 7)
			data->OneIn[i] |= File[6];
		if (RankOf(i) == 0)
			data->OneIn[i] |= Line[1];
		if (RankOf(i) == 7)
			data->OneIn[i] |= Line[6];
		data->BMagicMask[i] = BMask[i] & Interior;
		data->RMagicMask[i] = RMask[i];
		data->PCone[0][i] = data->PWay[0][i];
		data->PCone[1][i] = data->PWay[1][i];
		if (FileOf(i) > 0)
		{
			data->RMagicMask[i] &= ~File[0];
			data->PCone[0][i] |= data->PWay[0][i - 1];
			data->PCone[1][i] |= data->PWay[1][i - 1];
		}
		if (RankOf(i) > 0)
			data->RMagicMask[i] &= ~Line[0];
		if (FileOf(i) < 7)
		{
			data->RMagicMask[i] &= ~File[7];
			data->PCone[0][i] |= data->PWay[0][i + 1];
			data->PCone[1][i] |= data->PWay[1][i + 1];
		}
		if (RankOf(i) < 7)
			data->RMagicMask[i] &= ~Line[7];
	}

	for (int i = 0; i < 8; ++i)
	{
		data->West[i] = 0;
		data->East[i] = 0;
		data->Forward[0][i] = data->Forward[1][i] = 0;
		data->PIsolated[i] = 0;
		for (int j = 0; j < 8; ++j)
		{
			if (i < j)
				data->Forward[0][i] |= Line[j];
			else if (i > j)
				data->Forward[1][i] |= Line[j];
			if (i < j)
				data->East[i] |= File[j];
			else if (i > j)
				data->West[i] |= File[j];
		}
		if (i > 0)
			data->PIsolated[i] |= File[i - 1];
		if (i < 7)
			data->PIsolated[i] |= File[i + 1];
	}
	for (int i = 0; i < 64; ++i)
	{
		for (uint64 u = QMask[i]; T(u); Cut(u))
		{
			int j = lsb(u);
			int k = Sgn(RankOf(j) - RankOf(i));
			int l = Sgn(FileOf(j) - FileOf(i));
			for (int n = i + 8 * k + l; n != j; n += (8 * k + l))
				data->Between[i][j] |= Bit(n);
		}
		for (uint64 u = BMask[i]; T(u); Cut(u))
		{
			int j = lsb(u);
			data->FullLine[i][j] = BMask[i] & BMask[j];
		}
		for (uint64 u = RMask[i]; T(u); Cut(u))
		{
			int j = lsb(u);
			data->FullLine[i][j] = RMask[i] & RMask[j];
		}
		data->BishopForward[0][i] |= data->PWay[0][i];
		data->BishopForward[1][i] |= data->PWay[1][i];
		for (int j = 0; j < 64; j++)
		{
			if ((data->PWay[1][j] | Bit(j)) & BMask[i] & data->Forward[0][RankOf(i)])
				data->BishopForward[0][i] |= Bit(j);
			if ((data->PWay[0][j] | Bit(j)) & BMask[i] & data->Forward[1][RankOf(i)])
				data->BishopForward[1][i] |= Bit(j);
		}
	}

	for (int i = 0; i < 16; ++i)
		for (int j = 0; j < 16; ++j)
		{
			if (j < WhitePawn)
				data->MvvLva[i][j] = 0;
			else if (j < WhiteKnight)
				data->MvvLva[i][j] = PawnCaptureMvvLva(i) << 26;
			else if (j < WhiteLight)
				data->MvvLva[i][j] = KnightCaptureMvvLva(i) << 26;
			else if (j < WhiteRook)
				data->MvvLva[i][j] = BishopCaptureMvvLva(i) << 26;
			else if (j < WhiteQueen)
				data->MvvLva[i][j] = RookCaptureMvvLva(i) << 26;
			else
				data->MvvLva[i][j] = QueenCaptureMvvLva(i) << 26;
		}

	for (int i = 0; i < 256; ++i)
		data->PieceFromChar[i] = 0;
	data->PieceFromChar[66] = 6;
	data->PieceFromChar[75] = 14;
	data->PieceFromChar[78] = 4;
	data->PieceFromChar[80] = 2;
	data->PieceFromChar[81] = 12;
	data->PieceFromChar[82] = 10;
	data->PieceFromChar[98] = 7;
	data->PieceFromChar[107] = 15;
	data->PieceFromChar[110] = 5;
	data->PieceFromChar[112] = 3;
	data->PieceFromChar[113] = 13;
	data->PieceFromChar[114] = 11;

	data->TurnKey = rand64();
	for (int i = 0; i < 8; ++i) data->EPKey[i] = rand64();
	for (int i = 0; i < 16; ++i) data->CastleKey[i] = rand64();
	for (int i = 0; i < 16; ++i)
		for (int j = 0; j < 64; ++j)
			data->PieceKey[i][j] = i ? rand64() : 0;
	for (int i = 0; i < 16; ++i)
		data->LogDist[i] = (int)(10.0 * log(1.01 + i));
	for (int i = 1; i < 256; ++i)
	{
		data->SpanWidth[i] = msb(i) - lsb(i);
		int& gap = data->SpanGap[i] = 0;
		for (int j = 0, last = 9; j < 8; ++j)
		{
			if (i & Bit(j))
				last = j;
			else
				gap = max(gap, j - 1 - last);
		}
	}
	data->SpanWidth[0] = data->SpanGap[0] = 0;

	data->UpdateCastling = { 0xFF ^ CanCastle_OOO, 0xFF, 0xFF, 0xFF,
		0xFF ^ (CanCastle_OO | CanCastle_OOO), 0xFF, 0xFF, 0xFF ^ CanCastle_OO,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF ^ CanCastle_ooo, 0xFF, 0xFF, 0xFF,
		0xFF ^ (CanCastle_oo | CanCastle_ooo), 0xFF, 0xFF, 0xFF ^ CanCastle_oo };
}

void init_magic(CommonData_* data)
{
	data->BMagic = { 0x0048610528020080, 0x00c4100212410004, 0x0004180181002010, 0x0004040188108502, 0x0012021008003040, 0x0002900420228000,
		0x0080808410c00100, 0x000600410c500622, 0x00c0056084140184, 0x0080608816830050, 0x00a010050200b0c0, 0x0000510400800181,
		0x0000431040064009, 0x0000008820890a06, 0x0050028488184008, 0x00214a0104068200, 0x004090100c080081, 0x000a002014012604,
		0x0020402409002200, 0x008400c240128100, 0x0001000820084200, 0x0024c02201101144, 0x002401008088a800, 0x0003001045009000,
		0x0084200040981549, 0x0001188120080100, 0x0048050048044300, 0x0008080000820012, 0x0001001181004003, 0x0090038000445000,
		0x0010820800a21000, 0x0044010108210110, 0x0090241008204e30, 0x000c04204004c305, 0x0080804303300400, 0x00a0020080080080,
		0x0000408020220200, 0x0000c08200010100, 0x0010008102022104, 0x0008148118008140, 0x0008080414809028, 0x0005031010004318,
		0x0000603048001008, 0x0008012018000100, 0x0000202028802901, 0x004011004b049180, 0x0022240b42081400, 0x00c4840c00400020,
		0x0084009219204000, 0x000080c802104000, 0x0002602201100282, 0x0002040821880020, 0x0002014008320080, 0x0002082078208004,
		0x0009094800840082, 0x0020080200b1a010, 0x0003440407051000, 0x000000220e100440, 0x00480220a4041204, 0x00c1800011084800,
		0x000008021020a200, 0x0000414128092100, 0x0000042002024200, 0x0002081204004200 };

	data->RMagic = { 0x00800011400080a6, 0x004000100120004e, 0x0080100008600082, 0x0080080016500080, 0x0080040008000280, 0x0080020005040080,
		0x0080108046000100, 0x0080010000204080, 0x0010800424400082, 0x00004002c8201000, 0x000c802000100080, 0x00810010002100b8,
		0x00ca808014000800, 0x0002002884900200, 0x0042002148041200, 0x00010000c200a100, 0x00008580004002a0, 0x0020004001403008,
		0x0000820020411600, 0x0002120021401a00, 0x0024808044010800, 0x0022008100040080, 0x00004400094a8810, 0x0000020002814c21,
		0x0011400280082080, 0x004a050e002080c0, 0x00101103002002c0, 0x0025020900201000, 0x0001001100042800, 0x0002008080022400,
		0x000830440021081a, 0x0080004200010084, 0x00008000c9002104, 0x0090400081002900, 0x0080220082004010, 0x0001100101000820,
		0x0000080011001500, 0x0010020080800400, 0x0034010224009048, 0x0002208412000841, 0x000040008020800c, 0x001000c460094000,
		0x0020006101330040, 0x0000a30010010028, 0x0004080004008080, 0x0024000201004040, 0x0000300802440041, 0x00120400c08a0011,
		0x0080006085004100, 0x0028600040100040, 0x00a0082110018080, 0x0010184200221200, 0x0040080005001100, 0x0004200440104801,
		0x0080800900220080, 0x000a01140081c200, 0x0080044180110021, 0x0008804001001225, 0x00a00c4020010011, 0x00001000a0050009,
		0x0011001800021025, 0x00c9000400620811, 0x0032009001080224, 0x001400810044086a };

	data->BShift = { 58, 59, 59, 59, 59, 59, 59, 58, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 57, 57, 57, 57, 59, 59, 59, 59, 57, 55, 55, 57, 59, 59,
		59, 59, 57, 55, 55, 57, 59, 59, 59, 59, 57, 57, 57, 57, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 58, 59, 59, 59, 59, 59, 59, 58 };

	data->BOffset = { 0,    64,   96,   128,  160,  192,  224,  256,  320,  352,  384,  416,  448,  480,  512,  544,  576,  608,  640,  768,  896,  1024,
		1152, 1184, 1216, 1248, 1280, 1408, 1920, 2432, 2560, 2592, 2624, 2656, 2688, 2816, 3328, 3840, 3968, 4000, 4032, 4064, 4096, 4224,
		4352, 4480, 4608, 4640, 4672, 4704, 4736, 4768, 4800, 4832, 4864, 4896, 4928, 4992, 5024, 5056, 5088, 5120, 5152, 5184 };

	data->RShift = { 52, 53, 53, 53, 53, 53, 53, 52, 53, 54, 54, 54, 54, 54, 54, 53, 53, 54, 54, 54, 54, 54, 54, 53, 53, 54, 54, 54, 54, 54, 54, 53,
		53, 54, 54, 54, 54, 54, 54, 53, 53, 54, 54, 54, 54, 54, 54, 53, 53, 54, 54, 54, 54, 54, 54, 53, 52, 53, 53, 53, 53, 53, 53, 52 };

	data->ROffset = { 5248,  9344,  11392, 13440, 15488, 17536, 19584, 21632, 25728, 27776, 28800, 29824, 30848, 31872, 32896,  33920,
		35968, 38016, 39040, 40064, 41088, 42112, 43136, 44160, 46208, 48256, 49280, 50304, 51328, 52352, 53376,  54400,
		56448, 58496, 59520, 60544, 61568, 62592, 63616, 64640, 66688, 68736, 69760, 70784, 71808, 72832, 73856,  74880,
		76928, 78976, 80000, 81024, 82048, 83072, 84096, 85120, 87168, 91264, 93312, 95360, 97408, 99456, 101504, 103552 };

	for (int i = 0; i < 64; ++i)
	{
		int bits = 64 - data->BShift[i];
		array<int, 16> bit_list = { 0 };
		uint64 u = data->BMagicMask[i];
		for (int j = 0; T(u); Cut(u), ++j)
			bit_list[j] = lsb(u);
		for (int j = 0; j < Bit(bits); ++j)
		{
			u = 0;
			for (int k = 0; k < bits; ++k)
				if (Odd(j >> k))
					u |= Bit(bit_list[k]);
#ifndef HNI
			int index = static_cast<int>(data->BOffset[i] + ((data->BMagic[i] * u) >> data->BShift[i]));
#else
			int index = static_cast<int>(data->BOffset[i] + _pext_u64(u, data->BMagicMask[i]));
#endif
			data->MagicAttacks[index] = XBMagicAttacks(data, i, u);
		}
		bits = 64 - data->RShift[i];
		u = data->RMagicMask[i];
		for (int j = 0; T(u); Cut(u), ++j)
			bit_list[j] = lsb(u);
		for (int j = 0; j < Bit(bits); ++j)
		{
			u = 0;
			for (int k = 0; k < bits; ++k)
				if (Odd(j >> k))
					u |= Bit(bit_list[k]);
#ifndef HNI
			int index = static_cast<int>(data->ROffset[i] + ((data->RMagic[i] * u) >> data->RShift[i]));
#else
			int index = static_cast<int>(data->ROffset[i] + _pext_u64(u, data->RMagicMask[i]));
#endif
			data->MagicAttacks[index] = XRMagicAttacks(data, i, u);
		}
	}
}

void gen_kpk(CommonData_* data)
{
	int turn, wp, wk, bk, to, cnt, old_cnt, un;
	uint64 bwp, bwk, bbk, u;
	uint8 Kpk_gen[2][64][64][64];

	memset(Kpk_gen, 0, 2 * 64 * 64 * 64);

	cnt = 0;
	old_cnt = 1;
start:
	if (cnt == old_cnt)
		goto end;
	old_cnt = cnt;
	cnt = 0;
	for (turn = 0; turn < 2; ++turn)
	{
		for (wp = 0; wp < 64; ++wp)
		{
			for (wk = 0; wk < 64; ++wk)
			{
				for (bk = 0; bk < 64; ++bk)
				{
					if (Kpk_gen[turn][wp][wk][bk])
						continue;
					++cnt;
					if (wp < 8 || wp >= 56)
						goto set_draw;
					if (wp == wk || wk == bk || bk == wp)
						goto set_draw;
					bwp = Bit(wp);
					bwk = Bit(wk);
					bbk = Bit(bk);
					if (data->PAtt[White][wp] & bbk)
					{
						if (turn == White)
							goto set_draw;
						else if (F(KAtt[wk] & bwp))
							goto set_draw;
					}
					un = 0;
					if (turn == Black)
					{
						u = KAtt[bk] & (~(KAtt[wk] | data->PAtt[White][wp]));
						if (F(u))
							goto set_draw;
						for (; T(u); Cut(u))
						{
							to = lsb(u);
							if (Kpk_gen[turn ^ 1][wp][wk][to] == 1)
								goto set_draw;
							else if (Kpk_gen[turn ^ 1][wp][wk][to] == 0)
								++un;
						}
						if (F(un))
							goto set_win;
					}
					else
					{
						for (u = KAtt[wk] & (~(KAtt[bk] | bwp)); T(u); Cut(u))
						{
							to = lsb(u);
							if (Kpk_gen[turn ^ 1][wp][to][bk] == 2)
								goto set_win;
							else if (Kpk_gen[turn ^ 1][wp][to][bk] == 0)
								++un;
						}
						to = wp + 8;
						if (to != wk && to != bk)
						{
							if (to >= 56)
							{
								if (F(KAtt[to] & bbk))
									goto set_win;
								if (KAtt[to] & bwk)
									goto set_win;
							}
							else
							{
								if (Kpk_gen[turn ^ 1][to][wk][bk] == 2)
									goto set_win;
								else if (Kpk_gen[turn ^ 1][to][wk][bk] == 0)
									++un;
								if (to < 24)
								{
									to += 8;
									if (to != wk && to != bk)
									{
										if (Kpk_gen[turn ^ 1][to][wk][bk] == 2)
											goto set_win;
										else if (Kpk_gen[turn ^ 1][to][wk][bk] == 0)
											++un;
									}
								}
							}
						}
						if (F(un))
							goto set_draw;
					}
					continue;
				set_draw:
					Kpk_gen[turn][wp][wk][bk] = 1;
					continue;
				set_win:
					Kpk_gen[turn][wp][wk][bk] = 2;
					continue;
				}
			}
		}
	}
	if (cnt)
		goto start;
end:
	for (turn = 0; turn < 2; ++turn)
	{
		for (wp = 0; wp < 64; ++wp)
		{
			for (wk = 0; wk < 64; ++wk)
			{
				data->Kpk[turn][wp][wk] = 0;
				for (bk = 0; bk < 64; ++bk)
				{
					if (Kpk_gen[turn][wp][wk][bk] == 2)
						data->Kpk[turn][wp][wk] |= Bit(bk);
				}
			}
		}
	}
}

void Regularize(int* op, int* md, int* eg)
{
	if (*op * *eg >= 0)
		return;
	const int adj = Min(abs(*op), Min(abs(*md), abs(*eg))) * (*op + *eg > 0 ? 1 : -1);
	*op += adj;
	*eg += adj;
	*md -= adj;
}

void init_pst(CommonData_* data)
{
	fill(data->PstVals.begin(), data->PstVals.end(), 0);

	for (int i = 0; i < 64; ++i)
	{
		int r = RankOf(i);
		int f = FileOf(i);
		int d = abs(f - r);
		int e = abs(f + r - 7);
		array<int, 4> distL = { DistC[f], DistC[r],  RankR[d] + RankR[e], RankR[r] };
		array<int, 4> distQ = { DistC[f] * DistC[f], DistC[r] * DistC[r], RankR[d] * RankR[d] + RankR[e] * RankR[e], RankR[r] * RankR[r] };
		array<int, 2> distM = { DistC[f] * DistC[r], DistC[f] * RankR[r] };
		array<const PstW::Weights_*, 6> weights = { &PstW::Pawn, &PstW::Knight, &PstW::Bishop, &PstW::Rook, &PstW::Queen, &PstW::King };
		for (int j = 2; j < 16; j += 2)
		{
			int index = PieceType[j];
			const PstW::Weights_& src = *weights[index];
			int op = 0, md = 0, eg = 0, cl = 0;
			for (int k = 0; k < 2; ++k)
			{
				op += src.op_.quadMixed_[k] * distM[k];
				md += src.md_.quadMixed_[k] * distM[k];
				eg += src.eg_.quadMixed_[k] * distM[k];
				cl += src.cl_.quadMixed_[k] * distM[k];
			}
			for (int k = 0; k < 4; ++k)
			{
				op += src.op_.quad_[k] * distQ[k] + src.op_.linear_[k] * distL[k];
				md += src.md_.quad_[k] * distQ[k] + src.md_.linear_[k] * distL[k];
				eg += src.eg_.quad_[k] * distQ[k] + src.eg_.linear_[k] * distL[k];
				cl += src.cl_.quad_[k] * distQ[k] + src.cl_.linear_[k] * distL[k];
			}
			// Regularize(&op, &md, &eg);
			Div_<64> d64;
			XPst(data, j, i) = Pack4(d64(op), d64(md), d64(eg), d64(cl));
		}
	}

	XPst(data, WhiteKnight, 56) -= Pack2(100 * CP_EVAL, 0);
	XPst(data, WhiteKnight, 63) -= Pack2(100 * CP_EVAL, 0);
	// now for black
	for (int i = 0; i < 64; ++i)
		for (int j = 3; j < 16; j += 2)
		{
			auto src = XPst(data, j - 1, 63 - i);
			XPst(data, j, i) = Pack4(-Opening(src), -Middle(src), -Endgame(src), -Closed(src));
		}

	Current->pst = 0;
	for (int i = 0; i < 64; ++i)
		if (PieceAt(i))
		{
			Current->pst += Pst(PieceAt(i), i);
		}
}

double KingLocusDist(int x, int y)
{
	return sqrt(1.0 * Square(RankOf(x) - RankOf(y)) + Square(FileOf(x) - FileOf(y)));
}
uint64 make_klocus(int k_loc, double center_weight, double spine_weight)
{
	if (N_LOCUS <= 0)
		return 0ull;
	array<pair<double, int>, 64> temp;
	for (int ii = 0; ii < 64; ++ii)
	{
		auto kDist = KingLocusDist(k_loc, ii);
		auto spineDist = fabs(3.5 - FileOf(ii));
		auto centerDist = sqrt(Square(3.5 - RankOf(ii)) + Square(spineDist));
		auto useDist = center_weight * centerDist + spine_weight * spineDist + kDist;
		temp[ii] = { useDist, ii };
	}
	sort(temp.begin(), temp.end());
	uint64 retval = 0ull;
	int ii = N_LOCUS;
	// include elements tied with the cutoff
	while (ii < 64 && temp[ii].first == temp[N_LOCUS - 1].first)
		++ii;
	while (ii)
		retval |= Bit(temp[--ii].second);
	return retval;
}

template<class T_> void init_mobility
(const array<int, 12>& coeffs,
	T_* mob)
{
	// ordering of coeffs is (linear*4, log*4, locus*4)
	auto m1 = [&](int phase, int pop)->sint16
	{
		double val = pop * (coeffs[phase] - coeffs[phase + 8]) + log(1.0 + pop) * coeffs[phase + 4];
		return static_cast<sint16>(val / 64.0);
	};
	auto m2 = [&](int pop)->packed_t
	{
		return Pack4(m1(0, pop), m1(1, pop), m1(2, pop), m1(3, pop));
	};
	auto l1 = [&](int phase, int pop)->sint16
	{
		return static_cast<sint16>(pop * coeffs[phase + 8] / double(N_LOCUS));
	};
	auto l2 = [&](int pop)->packed_t
	{
		return Pack4(l1(0, pop), l1(1, pop), l1(2, pop), l1(3, pop));
	};

	const auto p_max = (*mob)[0].size();
	for (int pop = 0; pop < p_max; ++pop)
	{
		(*mob)[0][pop] = m2(pop);
		(*mob)[1][pop] = l2(pop);
	}
}


template<int N> array<packed_t, N / 4> PackAll(const array<int, N>& src)
{
	const int M = N / 4;
	array<packed_t, M> dst;
	for (int ii = 0; ii < M; ++ii)
		dst[ii] = Ca4(src, ii);
	return dst;
}

uint64 within(int loc, int dist)
{
	uint64 retval = 0ull;
	for (int i = 0; i < 64; ++i)
		if (Dist(i, loc) <= dist)
			retval |= Bit(i);
	return retval;
}

void init_eval(CommonData_* data)
{
	init_mobility(MobCoeffsKnight, &data->MobKnight);
	init_mobility(MobCoeffsBishop, &data->MobBishop);
	init_mobility(MobCoeffsRook, &data->MobRook);
	init_mobility(MobCoeffsQueen, &data->MobQueen);
	for (int i = 0; i < 64; ++i)
	{
		data->KingFrontal[i] = make_klocus(i, 1.0, -0.25) & within(i, 3);
		data->KingFlank[i] = make_klocus(i, 0.0, -0.75);
	}

	for (int i = 0; i < 3; ++i)
		for (int j = 7; j >= 0; --j)
		{
			data->Shelter[i][j] = 0;
			for (int k = 1; k < Min(j, 5); ++k)
				data->Shelter[i][j] += Av(ShelterValue, 5, i, k - 1);
			if (!j)  // no pawn in file
				data->Shelter[i][j] = data->Shelter[i][7] + Av(ShelterValue, 5, i, 4);
		}

	for (int i = 0; i < 4; ++i)
	{
		data->StormBlocked[i] = ((Sa(StormQuad, StormBlockedMul) * i * i) + (Sa(StormLinear, StormBlockedMul) * (i + 1))) / 100;
		data->StormShelterAtt[i] = ((Sa(StormQuad, StormShelterAttMul) * i * i) + (Sa(StormLinear, StormShelterAttMul) * (i + 1))) / 100;
		data->StormConnected[i] = ((Sa(StormQuad, StormConnectedMul) * i * i) + (Sa(StormLinear, StormConnectedMul) * (i + 1))) / 100;
		data->StormOpen[i] = ((Sa(StormQuad, StormOpenMul) * i * i) + (Sa(StormLinear, StormOpenMul) * (i + 1))) / 100;
		data->StormFree[i] = ((Sa(StormQuad, StormFreeMul) * i * i) + (Sa(StormLinear, StormFreeMul) * (i + 1))) / 100;
	}

	for (int i = 0; i < 8; ++i)
	{
		int im2 = Max(i - 2, 0);
		auto quad1 = [&](int row, int col, int rr) { return (Av(PasserQuad, 4, row, col) * rr + Av(PasserLinear, 4, row, col)) * rr + Av(PasserConstant, 4, row, col); };
		auto quad = [&](int row, int col) { return quad1(row, col, 5) > 0 ? Max(0, quad1(row, col, im2)) : Min(0, quad1(row, col, im2)); }; // no sign changes please
		auto pack16ths = [&](int which) { return Pack4(quad(which, 0) / 16, quad(which, 1) / 16, quad(which, 2) / 16, quad(which, 3) / 16); };
		data->PasserGeneral[i] = pack16ths(0);
		data->PasserBlocked[i] = pack16ths(1);
		data->PasserFree[i] = pack16ths(2);
		data->PasserSupported[i] = pack16ths(3);
		data->PasserProtected[i] = pack16ths(4);
		data->PasserConnected[i] = pack16ths(5);
		data->PasserOutside[i] = pack16ths(6);
		data->PasserCandidate[i] = pack16ths(7);
		data->PasserClear[i] = pack16ths(8);

		auto attdef = [&](int k) { return PasserAttDefQuad[k] * im2*im2 + PasserAttDefLinear[k] * im2 + PasserAttDefConst[k]; };
		data->PasserAtt[i] = attdef(0);
		data->PasserDef[i] = attdef(2);
		data->PasserAttLog[i] = attdef(1);
		data->PasserDefLog[i] = attdef(3);
	}

	data->KingAttackScale = { 0, 1, 1, 2, 4, 5, 8, 12, 15, 19, 23, 28, 34, 39, 39, 39 };
	data->KingCenterScale = { 64, 65, 74, 70, 68, 70, 61, 62 };	// would be better if this were symmetric, but I want to keep king off queenside
}

// all these special-purpose endgame evaluators

template<bool me> uint16 krbkrx()
{
	if (King(opp) & Interior)
		return 1;
	return 16;
}

template<bool me> uint16 kpkx()
{
	uint64 u = me == White ? RO->Kpk[Current->turn][lsb(Pawn(White))][lsb(King(White))] & Bit(lsb(King(Black)))
		: RO->Kpk[Current->turn ^ 1][63 - lsb(Pawn(Black))][63 - lsb(King(Black))] & Bit(63 - lsb(King(White)));
	return T(u) ? 32 : T(Piece(opp) ^ King(opp));
}

template<bool me> uint16 knpkx()
{
	if (Pawn(me) & OwnLine(me, 6) & (File[0] | File[7]))
	{
		int sq = lsb(Pawn(me));
		if (KAtt[sq] & King(opp) & (OwnLine(me, 6) | OwnLine(me, 7)))
			return 0;
		if (PieceAt(sq + Push[me]) == IKing[me] && (KAtt[lsb(King(me))] & KAtt[lsb(King(opp))] & OwnLine(me, 7)))
			return 0;
	}
	else if (Pawn(me) & OwnLine(me, 5) & (File[0] | File[7]))
	{
		int sq = lsb(Pawn(me)), to = sq + Push[me];
		if (PieceAt(sq + Push[me]) == IPawn[opp])
		{
			if (KAtt[to] & King(opp) & OwnLine(me, 7))
				return 0;
			if ((KAtt[to] & KAtt[lsb(King(opp))] & OwnLine(me, 7)) && (!(NAtt[to] & Knight(me)) || Current->turn == opp))
				return 0;
		}
	}
	return 32;
}

template<bool me> uint16 krpkrx()
{
	int mul = 32;
	int sq = lsb(Pawn(me));
	int rrank = OwnRank<me>(sq);
	int o_king = lsb(King(opp));
	int o_rook = lsb(Rook(opp));
	int m_king = lsb(King(me));
	int add_mat = T(Piece(opp) ^ King(opp) ^ Rook(opp));
	int clear = F(add_mat) || F((RO->PWay[opp][sq] | RO->PIsolated[FileOf(sq)]) & RO->Forward[opp][RankOf(sq + Push[me])] & (Piece(opp) ^ King(opp) ^ Rook(opp)));

	if (!clear)
		return 32;
	if (!add_mat && !(Pawn(me) & (File[0] | File[7])))
	{
		int m_rook = lsb(Rook(me));
		if (OwnRank<me>(o_king) < OwnRank<me>(m_rook) && OwnRank<me>(m_rook) < rrank && OwnRank<me>(m_king) >= rrank - 1 &&
			OwnRank<me>(m_king) > OwnRank<me>(m_rook) &&
			((KAtt[m_king] & Pawn(me)) || (MY_TURN && abs(FileOf(sq) - FileOf(m_king)) <= 1 && abs(rrank - OwnRank<me>(m_king)) <= 2)))
			return 127;
		if (KAtt[m_king] & Pawn(me))
		{
			if (rrank >= 4)
			{
				if ((FileOf(sq) < FileOf(m_rook) && FileOf(m_rook) < FileOf(o_king)) || (FileOf(sq) > FileOf(m_rook) && FileOf(m_rook) > FileOf(o_king)))
					return 127;
			}
			else if (rrank >= 2)
			{
				if (!(Pawn(me) & (File[1] | File[6])) && rrank + abs(FileOf(sq) - FileOf(m_rook)) > 4 &&
					((FileOf(sq) < FileOf(m_rook) && FileOf(m_rook) < FileOf(o_king)) || (FileOf(sq) > FileOf(m_rook) && FileOf(m_rook) > FileOf(o_king))))
					return 127;
			}
		}
	}

	if (RO->PWay[me][sq] & King(opp))
	{
		if (Pawn(me) & (File[0] | File[7]))
			mul = Min(mul, add_mat << 3);
		if (rrank <= 3)
			mul = Min(mul, add_mat << 3);
		if (rrank == 4 && OwnRank<me>(m_king) <= 4 && OwnRank<me>(o_rook) == 5 && T(King(opp) & (OwnLine(me, 6) | OwnLine(me, 7))) &&
			(!MY_TURN || F(RO->PAtt[me][sq] & RookAttacks(lsb(Rook(me)), PieceAll()) & (~KAtt[o_king]))))
			mul = Min(mul, add_mat << 3);
		if (rrank >= 5 && OwnRank<me>(o_rook) <= 1 && (!MY_TURN || IsCheck(me) || Dist(m_king, sq) >= 2))
			mul = Min(mul, add_mat << 3);
		if (T(King(opp) & (File[1] | File[2] | File[6] | File[7])) && T(Rook(opp) & OwnLine(me, 7)) && T(RO->Between[o_king][o_rook] & (File[3] | File[4])) &&
			F(Rook(me) & OwnLine(me, 7)))
			mul = Min(mul, add_mat << 3);
		return mul;
	}
	else if (rrank == 6 && (Pawn(me) & (File[0] | File[7])) && ((RO->PSupport[me][sq] | RO->PWay[opp][sq]) & Rook(opp)) && OwnRank<me>(o_king) >= 6)
	{
		int dist = abs(FileOf(sq) - FileOf(o_king));
		if (dist <= 3)
			mul = Min(mul, add_mat << 3);
		if (dist == 4 && ((RO->PSupport[me][o_king] & Rook(me)) || Current->turn == opp))
			mul = Min(mul, add_mat << 3);
	}

	if (KAtt[o_king] & RO->PWay[me][sq] & OwnLine(me, 7))
	{
		if (rrank <= 4 && OwnRank<me>(m_king) <= 4 && OwnRank<me>(o_rook) == 5)
			mul = Min(mul, add_mat << 3);
		if (rrank == 5 && OwnRank<me>(o_rook) <= 1 && !MY_TURN || (F(KAtt[m_king] & RO->PAtt[me][sq] & (~KAtt[o_king])) && (IsCheck(me) || Dist(m_king, sq) >= 2)))
			mul = Min(mul, add_mat << 3);
	}

	if (T(RO->PWay[me][sq] & Rook(me)) && T(RO->PWay[opp][sq] & Rook(opp)))
	{
		if (King(opp) & (File[0] | File[1] | File[6] | File[7]) & OwnLine(me, 6))
			mul = Min(mul, add_mat << 3);
		else if ((Pawn(me) & (File[0] | File[7])) && (King(opp) & (OwnLine(me, 5) | OwnLine(me, 6))) && abs(FileOf(sq) - FileOf(o_king)) <= 2 &&
			FileOf(sq) != FileOf(o_king))
			mul = Min(mul, add_mat << 3);
	}

	if (abs(FileOf(sq) - FileOf(o_king)) <= 1 && abs(FileOf(sq) - FileOf(o_rook)) <= 1 && OwnRank<me>(o_rook) > rrank && OwnRank<me>(o_king) > rrank)
		mul = Min(mul, (Pawn(me) & (File[3] | File[4])) ? 12 : 16);

	return mul;
}

template<bool me> uint16 krpkbx()
{
	if (!(Pawn(me) & OwnLine(me, 5)))
		return 32;
	int sq = lsb(Pawn(me));
	if (!(RO->PWay[me][sq] & King(opp)))
		return 32;
	int diag_sq = NB<me>(BMask[sq + Push[me]]);
	if (OwnRank<me>(diag_sq) > 1)
		return 32;
	uint64 mdiag = RO->FullLine[sq + Push[me]][diag_sq] | Bit(sq + Push[me]) | Bit(diag_sq);
	int check_sq = NB<me>(BMask[sq - Push[me]]);
	uint64 cdiag = RO->FullLine[sq - Push[me]][check_sq] | Bit(sq - Push[me]) | Bit(check_sq);
	if ((mdiag | cdiag) & (Piece(opp) ^ King(opp) ^ Bishop(opp)))
		return 32;
	if (cdiag & Bishop(opp))
		return 0;
	if ((mdiag & Bishop(opp)) && (Current->turn == opp || !(King(me) & RO->PAtt[opp][sq + Push[me]])))
		return 0;
	return 32;
}

template<bool me> uint16 kqkp()
{
	if (F(KAtt[lsb(King(opp))] & Pawn(opp) & OwnLine(me, 1) & (File[0] | File[2] | File[5] | File[7])))
		return 32;
	if (RO->PWay[opp][lsb(Pawn(opp))] & (King(me) | Queen(me)))
		return 32;
	if (Pawn(opp) & (File[0] | File[7]))
		return 1;
	else
		return 4;
}

template<bool me> uint16 kqkrpx()
{
	int rsq = lsb(Rook(opp));
	uint64 pawns = KAtt[lsb(King(opp))] & RO->PAtt[me][rsq] & Pawn(opp) & Interior & OwnLine(me, 6);
	if (pawns && OwnRank<me>(lsb(King(me))) <= 4)
		return 0;
	return 32;
}

template<bool me> uint16 krkpx()
{
	if (T(KAtt[lsb(King(opp))] & Pawn(opp) & OwnLine(me, 1)) & F(RO->PWay[opp][NB<me>(Pawn(opp))] & King(me)))
		return 0;
	return 32;
}

template<bool me> uint16 krppkrpx()
{
	if (Current->passer & Pawn(me))
	{
		if (Single(Current->passer & Pawn(me)))
		{
			int sq = lsb(Current->passer & Pawn(me));
			if (RO->PWay[me][sq] & King(opp) & (File[0] | File[1] | File[6] | File[7]))
			{
				int opp_king = lsb(King(opp));
				if (KAtt[opp_king] & Pawn(opp))
				{
					int king_file = FileOf(opp_king);
					if (!((~(File[king_file] | RO->PIsolated[king_file])) & Pawn(me)))
						return 1;
				}
			}
		}
		return 32;
	}
	if (F((~(RO->PWay[opp][lsb(King(opp))] | RO->PSupport[me][lsb(King(opp))])) & Pawn(me)))
		return 0;
	return 32;
}

template<bool me> uint16 krpppkrppx()
{
	if (T(Current->passer & Pawn(me)) || F((KAtt[lsb(Pawn(opp))] | KAtt[msb(Pawn(opp))]) & Pawn(opp)))
		return 32;
	if (F((~(RO->PWay[opp][lsb(King(opp))] | RO->PSupport[me][lsb(King(opp))])) & Pawn(me)))
		return 0;
	return 32;
}

template<bool me> uint16 kbpkbx()
{
	int sq = lsb(Pawn(me));
	uint64 u;
	if ((T(Board->bb[ILight[me]]) && T(Board->bb[IDark[opp]])) || (T(Board->bb[IDark[me]]) && T(Board->bb[ILight[opp]])))
	{
		if (OwnRank<me>(sq) <= 4)
			return 0;
		if (T(RO->PWay[me][sq] & King(opp)) && OwnRank<me>(sq) <= 5)
			return 0;
		for (u = Bishop(opp); T(u); Cut(u))
		{
			if (OwnRank<me>(lsb(u)) <= 4 && T(BishopAttacks(lsb(u), PieceAll()) & RO->PWay[me][sq]))
				return 0;
			if (Current->turn == opp && T(BishopAttacks(lsb(u), PieceAll()) & Pawn(me)))
				return 0;
		}
	}
	else if (T(RO->PWay[me][sq] & King(opp)) && T(King(opp) & LightArea) != T(Bishop(me) & LightArea))
		return 0;
	return 32;
}

template<bool me> uint16 kbpknx()
{
	uint64 u;
	if (T(RO->PWay[me][lsb(Pawn(me))] & King(opp)) && T(King(opp) & LightArea) != T(Bishop(me) & LightArea))
		return 0;
	if (Current->turn == opp)
		for (u = Knight(opp); T(u); Cut(u))
			if (NAtt[lsb(u)] & Pawn(me))
				return 0;
	return 32;
}

template<bool me> uint16 kbppkbx()
{
	int sq1 = NB<me>(Pawn(me));
	int sq2 = NB<opp>(Pawn(me));
	int o_king = lsb(King(opp));
	int o_bishop = lsb(Bishop(opp));

	if (FileOf(sq1) == FileOf(sq2))
	{
		if (OwnRank<me>(sq2) <= 3)
			return 0;
		if (T(RO->PWay[me][sq2] & King(opp)) && OwnRank<me>(sq2) <= 5)
			return 0;
	}
	else if (RO->PIsolated[FileOf(sq1)] & Pawn(me))
	{
		if (T(King(opp) & LightArea) != T(Bishop(me) & LightArea))
		{
			if (HasBit(KAtt[o_king] | King(opp), sq2 + Push[me]) && HasBit(BishopAttacks(o_bishop, PieceAll()), sq2 + Push[me]) &&
				HasBit(KAtt[o_king] | King(opp), (sq2 & 0xF8) | FileOf(sq1)) && HasBit(BishopAttacks(o_bishop, PieceAll()), (sq2 & 0xFFFFFFF8) | FileOf(sq1)))
				return 0;
		}
	}
	return 32;
}

template<bool me> uint16 krppkrx()
{
	int sq1 = NB<me>(Pawn(me));	// trailing pawn
	int sq2 = NB<opp>(Pawn(me));	// leading pawn

	if ((Piece(opp) ^ King(opp) ^ Rook(opp)) & RO->Forward[me][RankOf(sq1 - Push[me])])
		return 32;
	if (FileOf(sq1) == FileOf(sq2))
	{
		if (T(RO->PWay[me][sq2] & King(opp)))
			return 16;
	}
	else if (T(RO->PIsolated[FileOf(sq2)] & Pawn(me)) && T((File[0] | File[7]) & Pawn(me)) && T(King(opp) & Shift<me>(Pawn(me))))
	{
		if (OwnRank<me>(sq2) == 5 && OwnRank<me>(sq1) == 4 && T(Rook(opp) & (OwnLine(me, 5) | OwnLine(me, 6))))
			return 10;
		else if (OwnRank<me>(sq2) < 5)
			return 16;
	}
	int r2 = lsb(Rook(opp)), rf = FileOf(r2);
	const uint64 mask = RO->West[rf] & King(me) ? RO->West[rf] : RO->East[rf];
	if (mask & (Rook(me) | Pawn(me)))
		return 32;
	return 16;
}


template<bool me> bool eval_stalemate(GEvalInfo& EI)
{
	bool retval = (F(NonPawnKing(opp)) && Current->turn == opp && F(Current->att[me] & King(opp)) && F(KAtt[EI.king[opp]] & (~(Current->att[me] | Piece(opp)))) &&
		F(Current->patt[opp] & Piece(me)) && F(Shift<opp>(Pawn(opp)) & (~EI.occ)));
	if (retval)
		EI.mul = 0;
	return retval;
}

template<bool me> void eval_pawns_only(GEvalInfo& EI, pop_func_t pop)
{
	int number = pop(Pawn(me));
	int kOpp = lsb(King(opp));
	int sq = FileOf(kOpp) <= 3 ? (me ? 0 : 56) : (me ? 7 : 63);

	if (F(Pawn(me) & (~RO->PWay[opp][sq])))
	{
		if ((KAtt[sq] | Bit(sq)) & King(opp))
			EI.mul = 0;
		else if ((KAtt[sq] & KAtt[lsb(King(opp))] & OwnLine(me, 7)) && PieceAt(sq - Push[me]) == IPawn[opp] && PieceAt(sq - 2 * Push[me]) == IPawn[me])
			EI.mul = 0;
	}
	else if ((King(opp) & OwnLine(me, 6) | OwnLine(me, 7)) && abs(FileOf(sq) - FileOf(lsb(King(opp)))) <= 3 && !(Pawn(me) & (~RO->PSupport[me][sq])) &&
		(Pawn(me) & OwnLine(me, 5) & Shift<opp>(Pawn(opp))))
		EI.mul = 0;
	if (number == 1)
	{
		EI.mul = Min(EI.mul, kpkx<me>());
		if (Piece(opp) == King(opp) && EI.mul == 32)
			IncV(Current->score, KpkValue);
	}
}

template<bool me> void eval_single_bishop(GEvalInfo& EI, pop_func_t pop)
{
	int number = pop(Pawn(me));
	int sq = Piece(ILight[me]) ? (me ? 0 : 63) : (me ? 7 : 56);
	if (F(Pawn(me) & (~RO->PWay[opp][sq])))
	{
		if ((KAtt[sq] | Bit(sq)) & King(opp))
			EI.mul = 0;
		else if ((KAtt[sq] & KAtt[lsb(King(opp))] & OwnLine(me, 7)) && PieceAt(sq - Push[me]) == IPawn[opp] && PieceAt(sq - 2 * Push[me]) == IPawn[me])
			EI.mul = 0;
	}
	else if ((King(opp) & OwnLine(me, 6) | OwnLine(me, 7)) && abs(FileOf(sq) - FileOf(lsb(King(opp)))) <= 3 && !(Pawn(me) & (~RO->PSupport[me][sq])) &&
		(Pawn(me) & OwnLine(me, 5) & Shift<opp>(Pawn(opp))))
		EI.mul = 0;

	if (number == 1)
	{
		sq = lsb(Pawn(me));
		if ((Pawn(me) & (File[1] | File[6]) & OwnLine(me, 5)) && PieceAt(sq + Push[me]) == IPawn[opp] &&
			((RO->PAtt[me][sq + Push[me]] | RO->PWay[me][sq + Push[me]]) & King(opp)))
			EI.mul = 0;
	}
	if (Bishop(opp) && Single(Bishop(opp)) && T(Piece(ILight[me])) != T(Piece(ILight[opp])))
	{
		int pcnt = 0;
		if (T(King(opp) & LightArea) == T(Bishop(opp) & LightArea))
		{
			uint64 u;
			for (u = Pawn(me); u; Cut(u))
			{
				if (pcnt >= 2)
					break;
				++pcnt;
				int sq = lsb(u);
				if (!(RO->PWay[me][sq] & (RO->PAtt[me][EI.king[opp]] | RO->PAtt[opp][EI.king[opp]])))
				{
					if (!(RO->PWay[me][sq] & Pawn(opp)))
						break;
					int bsq = lsb(Bishop(opp));
					uint64 att = BishopAttacks(bsq, EI.occ);
					if (!(att & RO->PWay[me][sq] & Pawn(opp)))
						break;
					if (!(RO->BishopForward[me][bsq] & att & RO->PWay[me][sq] & Pawn(opp)) && pop(RO->FullLine[lsb(att & RO->PWay[me][sq] & Pawn(opp))][bsq] & att) <= 2)
						break;
				}
			}
			if (!u)
			{
				EI.mul = 0;
				return;
			}
		}

		// check for partial block
		if (pcnt <= 2 && Multiple(Pawn(me)) && !Pawn(opp) && !(Pawn(me) & Boundary) && EI.mul)
		{
			int sq1 = lsb(Pawn(me));
			int sq2 = msb(Pawn(me));
			int fd = abs(FileOf(sq2) - FileOf(sq1));
			if (fd >= 5)
				EI.mul = 32;
			else if (fd >= 4)
				EI.mul = 26;
			else if (fd >= 3)
				EI.mul = 20;
		}
		if ((KAtt[EI.king[opp]] | Current->patt[opp]) & Bishop(opp))
		{
			uint64 push = Shift<me>(Pawn(me));
			if (!(push & (~(Piece(opp) | Current->att[opp]))) && (King(opp) & (Board->bb[ILight[opp]] ? LightArea : DarkArea)))
			{
				EI.mul = Min<uint16>(EI.mul, 8);
				int bsq = lsb(Bishop(opp));
				uint64 att = BishopAttacks(bsq, EI.occ);
				uint64 prp = (att | KAtt[EI.king[opp]]) & Pawn(opp) & (Board->bb[ILight[opp]] ? LightArea : DarkArea);
				uint64 patt = ShiftW<opp>(prp) | ShiftE<opp>(prp);
				if ((KAtt[EI.king[opp]] | patt) & Bishop(opp))
				{
					uint64 double_att = (KAtt[EI.king[opp]] & patt) | (patt & att) | (KAtt[EI.king[opp]] & att);
					if (!(push & (~(King(opp) | Bishop(opp) | prp | double_att))))
					{
						EI.mul = 0;
						return;
					}
				}
			}
		}
	}
	if (number == 1)
	{
		if (Bishop(opp))
			EI.mul = Min(EI.mul, kbpkbx<me>());
		else if (Knight(opp))
			EI.mul = Min(EI.mul, kbpknx<me>());
	}
	else if (number == 2 && T(Bishop(opp)))
		EI.mul = Min(EI.mul, kbppkbx<me>());
}

template<bool me> void eval_np(GEvalInfo& EI, pop_func_t)
{
	assert(Knight(me) && Single(Knight(me)) && Pawn(me) && Single(Pawn(me)));
	EI.mul = Min(EI.mul, knpkx<me>());
}
template<bool me> void eval_knppkbx(GEvalInfo& EI, pop_func_t)
{
	assert(Knight(me) && Single(Knight(me)) && Multiple(Pawn(me)) && Bishop(opp) && Single(Bishop(opp)));
	static const uint64 AB = File[0] | File[1], ABC = AB | File[2];
	static const uint64 GH = File[6] | File[7], FGH = GH | File[5];
	if (F(Pawn(me) & ~AB) && T(King(opp) & ABC))
	{
		uint64 back = RO->Forward[opp][RankOf(lsb(King(opp)))];
		if (T(back & Pawn(me)))
			EI.mul = Min<uint16>(EI.mul, T(King(me) & AB & ~back) ? 24 : 8);
	}
	if (F(Pawn(me) & ~GH) && T(King(opp) & FGH))
	{
		uint64 back = RO->Forward[opp][RankOf(lsb(King(opp)))];
		if (T(back & Pawn(me)))
			EI.mul = Min<uint16>(EI.mul, T(King(me) & GH & ~back) ? 24 : 8);
	}
}

template<bool me> inline void check_forced_stalemate(uint16* mul, int kloc)
{
	if (F(KAtt[kloc] & ~Current->att[me])
		&& F(Shift<opp>(Pawn(opp)) & ~PieceAll()))
		*mul -= (3 * *mul) / 4;
}
template<bool me> INLINE void check_forced_stalemate(uint16* mul)
{
	check_forced_stalemate<me>(mul, lsb(King(opp)));
}

template<bool me> void eval_krbkrx(GEvalInfo& EI, pop_func_t)
{
	assert(Rook(me) && Single(Rook(me)) && Bishop(me) && Single(Bishop(me)) && Rook(opp) && Single(Rook(opp)));
	EI.mul = Min(EI.mul, krbkrx<me>());
	check_forced_stalemate<me>(&EI.mul);
}
template<bool me> void eval_krkpx(GEvalInfo& EI, pop_func_t)
{
	assert(Rook(me) && Single(Rook(me)) && F(Pawn(me)) && Pawn(opp));
	EI.mul = Min(EI.mul, krkpx<me>());
	//	if (T(Minor(opp)))
	//		EI.mul = Min(EI.mul, RO->OneIn[lsb(King(opp))] & Rook(me) ? 6 : 0);
}
template<bool me> void eval_krpkrx(GEvalInfo& EI, pop_func_t pop)
{
	assert(Rook(me) && Single(Rook(me)) && Pawn(me) && Single(Pawn(me)) && Rook(opp) && Single(Rook(opp)));
	uint16 new_mul = krpkrx<me>();
	EI.mul = (new_mul <= 32 ? Min(EI.mul, new_mul) : new_mul);
	check_forced_stalemate<me>(&EI.mul);
}
template<bool me> void eval_krpkbx(GEvalInfo& EI, pop_func_t pop)
{
	assert(Rook(me) && Single(Rook(me)) && Pawn(me) && Single(Pawn(me)) && Bishop(opp));
	EI.mul = Min(EI.mul, krpkbx<me>());
}
template<bool me> void eval_krppkrx(GEvalInfo& EI, pop_func_t pop)
{
	EI.mul = Min(EI.mul, krppkrx<me>());
	check_forced_stalemate<me>(&EI.mul);
}
template<bool me> void eval_krppkrpx(GEvalInfo& EI, pop_func_t pop)
{
	eval_krppkrx<me>(EI, pop);
	EI.mul = Min(EI.mul, krppkrpx<me>());
	check_forced_stalemate<me>(&EI.mul);
}
template<bool me> void eval_krpppkrppx(GEvalInfo& EI, pop_func_t pop)
{
	EI.mul = Min(EI.mul, krpppkrppx<me>());
	check_forced_stalemate<me>(&EI.mul);
}

template<bool me> void eval_kqkpx(GEvalInfo& EI, pop_func_t pop)
{
	EI.mul = Min(EI.mul, kqkp<me>());
}
template<bool me> void eval_kqkrpx(GEvalInfo& EI, pop_func_t pop)
{
	EI.mul = Min(EI.mul, kqkrpx<me>());
	//	if (T(Minor(opp)))
	//		EI.mul = Min(EI.mul, RO->OneIn[lsb(King(opp))] & Queen(me) ? 4 : 0);
	check_forced_stalemate<me>(&EI.mul);
}


void calc_material(int index, GMaterial& material)
{
	array<int, 2> pawns, knights, light, dark, rooks, queens, bishops, major, minor, tot, count, mat, mul, closed;
	int i = index;
	queens[White] = i % 3;
	i /= 3;
	queens[Black] = i % 3;
	i /= 3;
	rooks[White] = i % 3;
	i /= 3;
	rooks[Black] = i % 3;
	i /= 3;
	light[White] = i % 2;
	i /= 2;
	light[Black] = i % 2;
	i /= 2;
	dark[White] = i % 2;
	i /= 2;
	dark[Black] = i % 2;
	i /= 2;
	knights[White] = i % 3;
	i /= 3;
	knights[Black] = i % 3;
	i /= 3;
	pawns[White] = i % 9;
	i /= 9;
	pawns[Black] = i % 9;
	for (int me = 0; me < 2; ++me)
	{
		bishops[me] = light[me] + dark[me];
		major[me] = rooks[me] + queens[me];
		minor[me] = bishops[me] + knights[me];
		tot[me] = 3 * minor[me] + 5 * rooks[me] + 9 * queens[me];
		count[me] = major[me] + minor[me] + pawns[me];
		mat[me] = mul[me] = 32;
	}
	int score = (SeeValue[WhitePawn] + Av(MatLinear, 0, 0, 0)) * (pawns[White] - pawns[Black])
		+ (SeeValue[WhiteKnight] + Av(MatLinear, 0, 0, 1)) * (knights[White] - knights[Black])
		+ (SeeValue[WhiteLight] + Av(MatLinear, 0, 0, 2)) * (bishops[White] - bishops[Black])
		+ (SeeValue[WhiteRook] + Av(MatLinear, 0, 0, 3)) * (rooks[White] - rooks[Black])
		+ (SeeValue[WhiteQueen] + Av(MatLinear, 0, 0, 4)) * (queens[White] - queens[Black])
		+ (50 * CP_EVAL + Av(MatLinear, 0, 0, 5)) * ((bishops[White] / 2) - (bishops[Black] / 2));
	for (int me = 0; me < 2; ++me)
	{
		if (pawns[me] > pawns[opp] && queens[me] >= queens[opp] &&
			((major[me] > major[opp] && major[me] + minor[me] >= major[opp] + minor[opp]) || (major[me] == major[opp] && minor[me] > minor[opp])))
			score += (me ? -1 : 1) * MatWinnable;
	}

	int phase = Phase[PieceType[WhitePawn]] * (pawns[White] + pawns[Black])
		+ Phase[PieceType[WhiteKnight]] * (knights[White] + knights[Black])
		+ Phase[PieceType[WhiteLight]] * (bishops[White] + bishops[Black])
		+ Phase[PieceType[WhiteRook]] * (rooks[White] + rooks[Black])
		+ Phase[PieceType[WhiteQueen]] * (queens[White] + queens[Black]);
	material.phase = Min((Max(phase - PhaseMin, 0) * MAX_PHASE) / (PhaseMax - PhaseMin), MAX_PHASE);

	packed_t special = 0;
	for (int me = 0; me < 2; ++me)
	{
		if (queens[me] == queens[opp])
		{
			if (rooks[me] - rooks[opp] == 1)
			{
				if (knights[me] == knights[opp] && bishops[opp] - bishops[me] == 1)
					IncV(special, Values::MatRB);
				else if (bishops[me] == bishops[opp] && knights[opp] - knights[me] == 1)
					IncV(special, Values::MatRN);
				else if (knights[me] == knights[opp] && bishops[opp] - bishops[me] == 2)
					DecV(special, Values::MatBBR);
				else if (bishops[me] == bishops[opp] && knights[opp] - knights[me] == 2)
					DecV(special, Values::MatNNR);
				else if (bishops[opp] - bishops[me] == 1 && knights[opp] - knights[me] == 1)
					DecV(special, Values::MatBNR);
			}
			else if (rooks[me] == rooks[opp])
			{
				if (minor[me] - minor[opp] == 1)
					IncV(special, Values::MatM);
				else if (minor[me] == minor[opp] && pawns[me] > pawns[opp])
					IncV(special, Values::MatPawnOnly);
			}
		}
		else if (queens[me] - queens[opp] == 1)
		{
			if (rooks[opp] - rooks[me] == 2 && minor[opp] - minor[me] == 0)
				IncV(special, Values::MatQRR);
			else if (rooks[opp] - rooks[me] == 1 && knights[opp] == knights[me] && bishops[opp] - bishops[me] == 1)
				IncV(special, Values::MatQRB);
			else if (rooks[opp] - rooks[me] == 1 && knights[opp] - knights[me] == 1 && bishops[opp] == bishops[me])
				IncV(special, Values::MatQRN);
			else if ((major[opp] + minor[opp]) - (major[me] + minor[me]) >= 2)
				IncV(special, Values::MatQ3);
		}
	}
	score += (Opening(special) * material.phase + Endgame(special) * (MAX_PHASE - (int)material.phase)) / MAX_PHASE;

	array<int, 2> quad = { 0, 0 };
	auto mqm = [&](int i, int j) { return TrAv(MatQuadMe, 6, i, j); };
	auto mqo = [&](int i, int j) { return TrAv(MatQuadOpp, 5, i, j); };
	for (int me = 0; me < 2; me++)
	{
		quad[me] += T(pawns[me]) * (pawns[me] * mqm(0, 0) + knights[me] * mqm(0, 1) + bishops[me] * mqm(0, 2) + rooks[me] * mqm(0, 3) + queens[me] * mqm(0, 4));
		quad[me] += pawns[me] * (pawns[me] * mqm(1, 0) + knights[me] * mqm(1, 1) + bishops[me] * mqm(1, 2) + rooks[me] * mqm(1, 3) + queens[me] * mqm(1, 4));
		quad[me] += knights[me] * (knights[me] * mqm(2, 0) + bishops[me] * mqm(2, 1) + rooks[me] * mqm(2, 2) + queens[me] * mqm(2, 3));
		quad[me] += bishops[me] * (bishops[me] * mqm(3, 0) + rooks[me] * mqm(3, 1) + queens[me] * mqm(3, 2));
		quad[me] += rooks[me] * (rooks[me] * mqm(4, 0) + queens[me] * mqm(4, 1));

		quad[me] += T(pawns[me]) * (knights[opp] * mqo(0, 0) + bishops[opp] * mqo(0, 1) + rooks[opp] * mqo(0, 2) + queens[opp] * mqo(0, 3));
		quad[me] += pawns[me] * (knights[opp] * mqo(1, 0) + bishops[opp] * mqo(1, 1) + rooks[opp] * mqo(1, 2) + queens[opp] * mqo(1, 3));
		quad[me] += knights[me] * (bishops[opp] * mqo(2, 0) + rooks[opp] * mqo(2, 1) + queens[opp] * mqo(2, 2));
		quad[me] += bishops[me] * (rooks[opp] * mqo(3, 0) + queens[opp] * mqo(3, 1));
		quad[me] += rooks[me] * queens[opp] * mqo(4, 0);

		if (light[me] * dark[me])
			quad[me] += pawns[me] * Av(BishopPairQuad, 0, 0, 0)
			+ knights[me] * Av(BishopPairQuad, 0, 0, 1)
			+ rooks[me] * Av(BishopPairQuad, 0, 0, 2)
			+ queens[me] * Av(BishopPairQuad, 0, 0, 3)
			+ pawns[opp] * Av(BishopPairQuad, 0, 0, 4)
			+ knights[opp] * Av(BishopPairQuad, 0, 0, 5)
			+ bishops[opp] * Av(BishopPairQuad, 0, 0, 6)
			+ rooks[opp] * Av(BishopPairQuad, 0, 0, 7)
			+ queens[opp] * Av(BishopPairQuad, 0, 0, 8);

		closed[me] = pawns[me] * Av(MatClosed, 0, 0, 0)
			+ knights[me] * Av(MatClosed, 0, 0, 1)
			+ bishops[me] * Av(MatClosed, 0, 0, 2)
			+ rooks[me] * Av(MatClosed, 0, 0, 3)
			+ queens[me] * Av(MatClosed, 0, 0, 4)
			+ light[me] * dark[me] * Av(MatClosed, 0, 0, 5);
	}
	score += ((quad[White] - quad[Black]) * CP_EVAL) / 100;

	for (int me = 0; me < 2; ++me)
	{
		if (tot[me] - tot[opp] <= 3)
		{
			if (!pawns[me])
			{
				if (tot[me] <= 3)
					mul[me] = 0;
				if (tot[me] == tot[opp] && major[me] == major[opp] && minor[me] == minor[opp])
					mul[me] = major[me] + minor[me] <= 2 ? 0 : (major[me] + minor[me] <= 3 ? 16 : 32);
				else if (minor[me] + major[me] <= 2)
				{
					if (bishops[me] < 2)
						mat[me] = (bishops[me] && rooks[me]) ? 8 : 1;
					else if (bishops[opp] + rooks[opp] >= 1)
						mat[me] = 1;
					else
						mat[me] = 32;
				}
				else if (tot[me] - tot[opp] < 3 && minor[me] + major[me] - minor[opp] - major[opp] <= 1)
					mat[me] = 4;
				else if (minor[me] + major[me] <= 3)
					mat[me] = 8 * (1 + bishops[me]);
				else
					mat[me] = 8 * (2 + bishops[me]);
			}
			if (pawns[me] <= 1)
			{
				mul[me] = Min(28, mul[me]);
				if (rooks[me] == 1 && queens[me] + minor[me] == 0 && rooks[opp] == 1)
					mat[me] = Min(23, mat[me]);
			}
		}
		if (!major[me])
		{
			if (!minor[me])
			{
				if (!tot[me] && pawns[me] < pawns[opp])
					DecV(score, (pawns[opp] - pawns[me]) * SeeValue[WhitePawn]);
			}
			else if (minor[me] == 1)
			{
				if (pawns[me] <= 1 && minor[opp] >= 1)
					mat[me] = 1;
				if (bishops[me] == 1)
				{
					if (minor[opp] == 1 && bishops[opp] == 1 && light[me] != light[opp])
					{
						mul[me] = Min(mul[me], 15);
						if (pawns[me] - pawns[opp] <= 1)
							mul[me] = Min(mul[me], 11);
					}
				}
			}
			else if (!pawns[me] && knights[me] == 2 && !bishops[me])
				mat[me] = (!tot[opp] && pawns[opp]) ? 6 : 0;
		}
		//		else if (F(queens[me] + queens[opp] + minor[opp] + pawns[opp]) && rooks[me] == rooks[opp] && minor[me] == 1 && T(pawns[me]))	// RNP or RBP vs R
		//			mat[me] += 24 / (pawns[me] + rooks[me]);
		//		else if (F(queens[me] + minor[me] + major[opp] + pawns[opp]) && rooks[me] == minor[opp] && T(pawns[me]))	// RP vs minor
		//			mat[me] = 40;

		if (!mul[me])
			mat[me] = 0;
		if (mat[me] <= 1 && tot[me] != tot[opp])
			mul[me] = Min(mul[me], 8);
	}
	if (bishops[White] == 1 && bishops[Black] == 1 && light[White] != light[Black])
	{
		mul[White] = Min(mul[White], 19 + 5 * knights[Black] + 2 * major[Black]);
		mul[Black] = Min(mul[Black], 19 + 5 * knights[White] + 2 * major[White]);
	}
	else if (!minor[White] && !minor[Black] && major[White] == 1 && major[Black] == 1 && rooks[White] == rooks[Black])
	{
		for (int me = 0; me < 2; ++me)
			mul[me] = Min(mul[me], 24);
	}
	if (queens[White] == 1 && queens[Black] == 1 && !rooks[White] && !rooks[Black] && !knights[White] && !knights[Black] && bishops[White] == bishops[Black])
	{
		for (int me = 0; me < 2; ++me)
			mul[me] = Min(mul[me], 26);
	}
	for (int me = 0; me < 2; ++me)
		material.mul[me] = mul[me];
	material.score = (score * mat[score > 0 ? White : Black]) / 32;
	material.closed = closed[White] - closed[Black]; // *mat[score > 0 ? White : Black]) / 32;
	material.eval = { nullptr, nullptr };
	for (int me = 0; me < 2; ++me)
	{
		if (F(major[me] + minor[me]))
			material.eval[me] = TEMPLATE_ME(eval_pawns_only);
		else if (F(major[me]) && minor[me] == 1)
		{
			if (bishops[me])
				material.eval[me] = pawns[me] ? TEMPLATE_ME(eval_single_bishop) : eval_unwinnable;
			else if (pawns[me] == 2 && bishops[opp] == 1)
				material.eval[me] = TEMPLATE_ME(eval_knppkbx);
			else if (pawns[me] <= 1)
				material.eval[me] = pawns[me] ? TEMPLATE_ME(eval_np) : eval_unwinnable;
		}
		else if (!pawns[me] && !queens[me] && rooks[me] == 1 && !knights[me] && bishops[me] == 1 && rooks[opp] == 1 && !minor[opp] && !queens[opp])
			material.eval[me] = TEMPLATE_ME(eval_krbkrx);
		else if (F(minor[me]) && major[me] == 1)
		{
			if (rooks[me])
			{
				if (F(pawns[me]) && T(pawns[opp]))
					material.eval[me] = TEMPLATE_ME(eval_krkpx);
				else if (rooks[opp] == 1)
				{
					if (pawns[me] == 1)
						material.eval[me] = TEMPLATE_ME(eval_krpkrx);
					else if (pawns[me] == 2)
						material.eval[me] = F(pawns[opp]) ? TEMPLATE_ME(eval_krppkrx) : TEMPLATE_ME(eval_krppkrpx);
					else if (pawns[me] == 3 && T(pawns[opp]))
						material.eval[me] = TEMPLATE_ME(eval_krpppkrppx);
				}
				else if (pawns[me] == 1 && T(bishops[opp]))
					material.eval[me] = TEMPLATE_ME(eval_krpkbx);
			}
			else if (F(pawns[me]) && T(pawns[opp]))
			{
				if (F(NonPawnKing(opp)) && Single(Pawn(opp)))
					material.eval[me] = TEMPLATE_ME(eval_kqkpx);
				else if (rooks[opp])
					material.eval[me] = TEMPLATE_ME(eval_kqkrpx);
			}
		}
		else if (major[opp] + minor[opp] == 0)
			material.eval[me] = eval_null;	// just force the stalemate check
	}
}

void init_material(CommonData_* dst)
{
	memset(&dst->Material[0], 0, TotalMat * sizeof(GMaterial));
	for (int index = 0; index < TotalMat; ++index)
		calc_material(index, dst->Material[index]);
}

void init_data(CommonData_* dst)
{
	RO = dst;
	init_misc(dst);
	init_magic(dst);
	gen_kpk(dst);
	init_pst(dst);
	init_eval(dst);
	init_material(dst);
}


void create_child
(const char *hashName,
	const char *pvHashName,
	const char *pawnHashName,
	const char *dataName,
	const char *settingsName,
	const char *sharedName,
	const char *infoName)
{
	char name[PATH_MAX];
	char command[10 * PATH_MAX];
	PROCESS_INFORMATION procInfo;
	STARTUPINFO startInfo;

	memset(&procInfo, 0, sizeof(procInfo));
	memset(&startInfo, 0, sizeof(startInfo));

	startInfo.cb = sizeof(STARTUPINFO);
	startInfo.dwFlags |= STARTF_USESTDHANDLES;
	startInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	startInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	startInfo.hStdInput = INVALID_HANDLE_VALUE;

	if (GetModuleFileName(NULL, name, sizeof(name) - 1) >= sizeof(name) - 1)
		error_msg("failed to get module name");
	int len = snprintf(command, sizeof(command) - 1,
		"\"%s\" child \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"",
		name, hashName, pvHashName, pawnHashName, dataName, settingsName, sharedName, infoName);
	if (len < 0 || len >= sizeof(command) - 1)
		error_msg("failed to create command line for child");

	BOOL success = CreateProcess(NULL, command, NULL, NULL, TRUE, 0, NULL, NULL, &startInfo, &procInfo);
	if (!success)
		error_msg("failed to create child process (%d)", GetLastError());
	CloseHandle(procInfo.hThread);
}

static void create_children(const Settings_& settings)
{
	constexpr size_t maxNHash = ((size_t)1 << 43) / sizeof(GEntry);   // <= 8TB
	constexpr int maxSyzygyProbeDepth = 64;

	int nThreads = Min(maxNumThreads, settings.nThreads);
	int tbMinDepth = Min(maxSyzygyProbeDepth, settings.tbMinDepth);
	int contempt = Max(0, settings.contempt);
	int wobble = Max(0, settings.wobble);
	int pid = GetCurrentProcessId();

	log_msg("settings: nThreads=%d, hashSize=%Iu, tbMinDepth=%d, syzygyPath=\"%s\", contempt=%d, wobble=%d\n",
		settings.nThreads, settings.nHash * sizeof(GEntry), settings.tbMinDepth, settings.tbPath, settings.contempt, settings.wobble);

	// Create shared objects:
	string dataName = object_name("DATA", pid, 0);
	RO = DATA = init_object(DATA, dataName.c_str(), true, false);
	init_data(DATA);

	string settingsName = object_name("SETTINGS", pid, 0);
	SETTINGS = init_object(SETTINGS, settingsName.c_str(), true, true, 1, &settings);

	string hashName = object_name("HASH", pid, 0);
	HASH = init_object(HASH, hashName.c_str(), true, false, SETTINGS->nHash);

	string pvHashName = object_name("PVHASH", pid, 0);
	PVHASH = init_object(PVHASH, pvHashName.c_str(), true, false, N_PV_HASH);

	string pawnHashName = object_name("PAWNHASH", pid, 0);
	PAWNHASH = init_object(PAWNHASH, pawnHashName.c_str(), true, false, N_PAWN_HASH);

	string sharedName = object_name("SHARED", pid, 0);
	SHARED = init_object(SHARED, sharedName.c_str(), true, false);
	SHARED->mutex = mutex_init();
	SHARED->outMutex = mutex_init();
	SHARED->goEvent = event_init();
	SHARED->working.Init();
	SHARED->stopAll = true;

	// Create children: 
	THREADS.resize(nThreads);
	for (int i = 0; i < nThreads; i++)
	{
		string infoName = object_name("INFO", pid, i);
		THREADS[i] = init_object<ThreadOwn_>(nullptr, infoName.c_str(), true, false);
		THREADS[i]->newGame = true;
		THREADS[i]->id = i;
	}
	{
		LOCK_SHARED;
		for (int i = 0; i < nThreads; i++)
		{
			SHARED->working.Insert(i);
			string infoName = object_name("INFO", pid, i);
			create_child(hashName.c_str(), pvHashName.c_str(), pawnHashName.c_str(), dataName.c_str(), settingsName.c_str(), sharedName.c_str(), infoName.c_str());
		}
	}

	INFO = init_object(INFO, nullptr, true, false);
	INFO->pid = GetCurrentProcessId();
#if TB
	tb_init_fwd(settings.tbPath);
#endif

	// Wait for threads to finish initializing:
	while (!SHARED->working.Empty())
		Sleep(1);

	// Cleanup:
	remove_object(dataName.c_str());
	remove_object(settingsName.c_str());
	remove_object(hashName.c_str());
	remove_object(pvHashName.c_str());
	remove_object(pawnHashName.c_str());
	remove_object(sharedName.c_str());
	for (int i = 0; i < nThreads; i++)
	{
		string infoName = object_name("INFO", pid, i);
		remove_object(infoName.c_str());
	}
}

static void nuke_child(unsigned pid)
{
	HANDLE handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, (DWORD)pid);
	if (handle == NULL)
		return;
	TerminateProcess(handle, EXIT_SUCCESS);
	if (WaitForSingleObject(handle, INFINITE) != WAIT_OBJECT_0)
		error_msg("failed to terminate child (%d)", GetLastError());
	CloseHandle(handle);
}

static void nuke_children(void)
{
	for (size_t i = 0; i < SETTINGS->nThreads; i++)
		nuke_child(THREADS[i]->pid);
	delete_object(HASH, SETTINGS->nHash * sizeof(*HASH));
	delete_object(PVHASH, N_PV_HASH * sizeof(*PVHASH));
	delete_object(PAWNHASH, N_PAWN_HASH * sizeof(*PAWNHASH));
	//	event_discard(&SHARED->goEvent);
	for (size_t i = 0; i < SETTINGS->nThreads; i++)
		delete_object(THREADS[i], sizeof(*THREADS[i]));
	delete_object(DATA, sizeof(*DATA));
	delete_object(SETTINGS, sizeof(*SETTINGS));
	delete_object(SHARED, sizeof(*SHARED));
	delete_object(INFO, sizeof(*INFO));
}

static void reset(const Settings_& settings)
{
	nuke_children();
	create_children(settings);
}

static void emergency_stop(void)
{
	// Something is wrong -- threads are not stopping, or have crashed.
	// It is unspecified if the global mutex is locked.  We no longer care
	// about races.
	// Reset everthing.  We may lose on time, but so be it.
	bool go = !SHARED->working.Empty();
	int bestMove = 0;
	for (const auto& t : THREADS)
		if (bestMove = t->bestMove)
			break;
	// Log this incident:
	log_msg("warning: threads crashed or deadlocked; initiating emergency reset\n");
	reset(*SETTINGS);
	if (go)
	{
		char moveStr[16];
		move_to_string(bestMove, moveStr);
		Say("bestmove " + string(moveStr) + "\n");
	}
}

void wait_for_stop()
{
	const uint64_t maxStopTime = 300;       // Max 300ms to stop all threads;
	auto stopTime = now(), currTime = stopTime;
	while (true)
	{
		Sleep(1);
		currTime = now();
		LOCK_SHARED;
		if (SHARED->working.Empty())
			break;	// we are done
	}
	if (SHARED->best.move)
	{
		// workers failed low, get the best cached move
		LOCK_SHARED;
		char moveStr[16];
		move_to_string(SHARED->best.move, moveStr);
		Say("bestmove " + string(moveStr) + "\n");
		SHARED->best = { 0, 0, 0, 0 };
	}
}

static void stop()
{
	//LOCK_SHARED;
	SHARED->stopAll = true;
}

static void go()
{
#if TB
	if (popcnt(PieceAll()) <= int(TB_LARGEST))
	{
		auto res = TBProbe(tb_probe_root_checked, T(Current->turn));
		if (res != TB_RESULT_FAILED)
		{
			int bestScore;
			int best_move = GetTBMove(res, &bestScore);
			char movStr[16];
			move_to_string(best_move, movStr);
			Say("info depth 1 seldepth 1 score cp " + Str(bestScore / CP_SEARCH) + " nodes 1 tbhits 1 pv " + string(movStr) + "\n");
			Say("bestmove " + string(movStr) + "\n");
			return;
		}
	}
#endif

	SHARED->date++;
	assert(PVN == 1);       // Multi-PV NYI.
	assert(INFO->pid == SETTINGS->parentPid);
	memcpy(&SHARED->rootBoard, Board, sizeof(GBoard));
	memcpy(&SHARED->rootData, Current, sizeof(GData));
	memcpy(&SHARED->rootStack, &Stack[0], sp * sizeof(uint64_t));
	SHARED->rootSp = sp;
	SHARED->best = { 0, 0, 0, 0 };
	SHARED->stopAll = false;
	for (int i = 0; i < SETTINGS->nThreads; i++)
	{
		THREADS[i]->nodes = 0;
		THREADS[i]->tbHits = 0;
		THREADS[i]->depth = 0;
		THREADS[i]->selDepth = 0;
		THREADS[i]->bestMove = 0;
		THREADS[i]->bestScore = 0;
		THREADS[i]->PV[0] = 0;
	}
	event_signal(&SHARED->goEvent);
}

static void wait_for_go(void)
{
	if (WaitForSingleObject(SHARED->goEvent, INFINITE) != WAIT_OBJECT_0)
		error_msg("Wait-for-go error %d\n", GetLastError());
}

static inline void check_for_stop(void)
{
	if (SHARED->stopAll) throw 3;
}

#ifndef W32_BUILD
#define SET_BIT(var, bit) (InterlockedOr(&(var), 1 << (bit)))
#define SET_BIT_64(var, bit) (InterlockedOr64(&(var), Bit(bit)));
#define ZERO_BIT_64(var, bit) (InterlockedAnd64(&(var), ~Bit(bit)));
#define TEST_RESET_BIT(var, bit) (InterlockedBitTestAndReset64(&(var), bit))
#define TEST_RESET(var) (InterlockedExchange64(&(var), 0))
#else
#define SET_BIT(var, bit) (_InterlockedOr(&(var), 1 << (bit)))
#define SET_BIT_64(var, bit)                                        \
    {                                                               \
        if ((bit) < 32)                                             \
            _InterlockedOr((LONG*)&(var), 1 << (bit));              \
        else                                                        \
            _InterlockedOr(((LONG*)(&(var))) + 1, 1 << ((bit)-32)); \
    }
#define ZERO_BIT_64(var, bit)                                           \
    {                                                                   \
        if ((bit) < 32)                                                 \
            _InterlockedAnd((LONG*)&(var), ~(1 << (bit)));              \
        else                                                            \
            _InterlockedAnd(((LONG*)(&(var))) + 1, ~(1 << ((bit)-32))); \
    }
#define TEST_RESET_BIT(var, bit) (InterlockedBitTestAndReset(&(var), bit))
#define TEST_RESET(var) (InterlockedExchange(&(var), 0))
#endif
#define SET(var, value) (InterlockedExchange(&(var), value))

#define LOCK(lock)                                                     \
    {                                                                  \
        while (InterlockedCompareExchange(&(lock), 1, 0)) _mm_pause(); \
    }
#define UNLOCK(lock)  \
    {                 \
        SET(lock, 0); \
    }

// END SMP

void setup_board()
{
	int i;

	uint64 occ = 0;
	sp = 0;
	if (SHARED)
	{
		++SHARED->date;
		if (SHARED->date > 0x8000)
		{  // mustn't ever happen
			SHARED->date = 2;
			// now GUI must wait for readyok... we have plenty of time :)
			std::fill(HASH, HASH + SETTINGS->nHash, NullEntry);
			std::fill(PVHASH, PVHASH + N_PV_HASH, NullPVEntry);
		}
	}
	Current->material = 0;
	Current->pst = 0;
	Current->key = RO->PieceKey[0][0];
	if (Current->turn)
		Current->key ^= RO->TurnKey;
	Current->key ^= RO->CastleKey[Current->castle_flags];
	if (Current->ep_square)
		Current->key ^= RO->EPKey[FileOf(Current->ep_square)];
	Current->pawn_key = 0;
	Current->pawn_key ^= RO->CastleKey[Current->castle_flags];
	for (i = 0; i < 16; ++i) Piece(i) = 0;
	for (i = 0; i < 64; ++i)
	{
		if (PieceAt(i))
		{
			Piece(PieceAt(i)) |= Bit(i);
			Piece(PieceAt(i) & 1) |= Bit(i);
			occ |= Bit(i);
			Current->key ^= RO->PieceKey[PieceAt(i)][i];
			if (PieceAt(i) < WhiteKnight)
				Current->pawn_key ^= RO->PieceKey[PieceAt(i)][i];
			if (PieceAt(i) < WhiteKing)
				Current->material += MatCode[PieceAt(i)];
			else
				Current->pawn_key ^= RO->PieceKey[PieceAt(i)][i];
			Current->pst += Pst(PieceAt(i), i);
		}
	}
	if (popcnt(Piece(WhiteKnight)) > 2 || popcnt(Piece(WhiteLight)) > 1 || popcnt(Piece(WhiteDark)) > 1 || popcnt(Piece(WhiteRook)) > 2 || popcnt(Piece(WhiteQueen)) > 2 ||
		popcnt(Piece(BlackKnight)) > 2 || popcnt(Piece(BlackLight)) > 1 || popcnt(Piece(BlackDark)) > 1 || popcnt(Piece(BlackRook)) > 2 || popcnt(Piece(BlackQueen)) > 2)
		Current->material |= FlagUnusualMaterial;
	Current->capture = 0;
	for (int ik = 1; ik <= N_KILLER; ++ik) Current->killer[ik] = 0;
	Current->ply = 0;
	Stack[sp] = Current->key;
}

const char* get_board(const char* fen)
{
	int pos, i, j;
	unsigned char c;

	Current = Data;
	memset(Board, 0, sizeof(GBoard));
	memset(Current, 0, sizeof(GData));
	pos = 0;
	c = fen[pos];
	while (c == ' ') c = fen[++pos];
	for (i = 56; i >= 0; i -= 8)
	{
		for (j = 0; j <= 7;)
		{
			if (c <= '8')
				j += c - '0';
			else
			{
				PieceAt(i + j) = RO->PieceFromChar[c];
				if (Even(SDiagOf(i + j)) && (PieceAt(i + j) / 2) == 3)
					PieceAt(i + j) += 2;
				++j;
			}
			c = fen[++pos];
		}
		c = fen[++pos];
	}
	if (c == 'b')
		Current->turn = 1;
	c = fen[++pos];
	c = fen[++pos];
	if (c == '-')
		c = fen[++pos];
	if (c == 'K')
	{
		Current->castle_flags |= CanCastle_OO;
		c = fen[++pos];
	}
	if (c == 'Q')
	{
		Current->castle_flags |= CanCastle_OOO;
		c = fen[++pos];
	}
	if (c == 'k')
	{
		Current->castle_flags |= CanCastle_oo;
		c = fen[++pos];
	}
	if (c == 'q')
	{
		Current->castle_flags |= CanCastle_ooo;
		c = fen[++pos];
	}
	c = fen[++pos];
	if (c != '-')
	{
		i = c + fen[++pos] * 8 - 489;
		j = i ^ 8;
		if (PieceAt(i) != 0)
			i = 0;
		else if (PieceAt(j) != (3 - Current->turn))
			i = 0;
		else if (PieceAt(j - 1) != (Current->turn + 2) && PieceAt(j + 1) != (Current->turn + 2))
			i = 0;
		Current->ep_square = i;
	}
	setup_board();
	return fen + pos;
}

void init_search(int clear_hash)
{
	for (int ip = 0; ip < 16; ++ip)
		for (int it = 0; it < 64; ++it)
		{
			HistoryVals[ip][it][0] = (1 << 8) | 2;
			HistoryVals[ip][it][1] = (1 << 8) | 2;
		}

	memset(DeltaVals, 0, 16 * 4096 * sizeof(sint16));
	memset(Ref, 0, 16 * 64 * sizeof(GRef));
	memset(Data + 1, 0, 127 * sizeof(GData));
	if (clear_hash)
	{
		SHARED->date = 1;
		fill(HASH, HASH + SETTINGS->nHash, NullEntry);
		fill(PVHASH, PVHASH + N_PV_HASH, NullPVEntry);
	}
	get_board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	INFO->nodes = INFO->tbHits = 0;
	INFO->bestMove = INFO->bestScore = 0;
	LastTime = LastValue = LastExactValue = InstCnt = 0;
	LastSpeed = 0;
	PVN = 1;
	SearchMoves = 0;
	LastDepth = 128;
	memset(CurrentSI, 0, sizeof(GSearchInfo));
	memset(BaseSI, 0, sizeof(GSearchInfo));
}

INLINE GEntry* probe_hash()
{
	GEntry* start = HASH + (High32(Current->key) & SETTINGS->hashMask);
	for (GEntry* Entry = start; Entry < start + HASH_CLUSTER; ++Entry)
		if (Low32(Current->key) == Entry->key)
		{
			Entry->date = SHARED->date;
			return Entry;
		}
	return nullptr;
}

INLINE GPVEntry* probe_pv_hash()
{
	GPVEntry* start = PVHASH + (High32(Current->key) & PV_HASH_MASK);
	for (GPVEntry* PVEntry = start; PVEntry < start + PV_CLUSTER; ++PVEntry)
		if (Low32(Current->key) == PVEntry->key)
		{
			PVEntry->date = SHARED->date;
			return PVEntry;
		}
	return nullptr;
}

template<bool me> void do_move(int move)
{
	MOVING
		GEntry* Entry;
	GPawnEntry* PawnEntry;
	int from, to, piece, capture;
	GData* Next;
	uint64 u, mask_from, mask_to;

	to = To(move);
	Next = Current + 1;
	Next->ep_square = 0;
	capture = PieceAt(to);
	from = From(move);
	piece = PieceAt(from);
	PieceAt(from) = 0;
	PieceAt(to) = piece;
	Next->piece = piece;
	mask_from = Bit(from);
	mask_to = Bit(to);
	Piece(piece) ^= mask_from;
	Piece(me) ^= mask_from;
	Piece(piece) |= mask_to;
	Piece(me) |= mask_to;
	Next->castle_flags = Current->castle_flags;
	Next->turn = opp;
	const auto& pKey = RO->PieceKey[piece];
	Next->capture = capture;
	Next->pst = Current->pst + Pst(piece, to) - Pst(piece, from);
	Next->key = Current->key ^ pKey[to] ^ pKey[from];
	Next->pawn_key = Current->pawn_key;

	if (T(capture))
	{
		Piece(capture) ^= mask_to;
		Piece(opp) ^= mask_to;
		Next->pst -= Pst(capture, to);
		Next->key ^= RO->PieceKey[capture][to];
		if (capture == IPawn[opp])
			Next->pawn_key ^= RO->PieceKey[IPawn[opp]][to];
		Next->material = Current->material - MatCode[capture];
		if (T(Current->material & FlagUnusualMaterial) && capture >= WhiteKnight)
		{
			if (popcnt(Piece(WhiteQueen)) <= 2 && popcnt(Piece(BlackQueen)) <= 2 && popcnt(Piece(WhiteLight)) <= 1 && popcnt(Piece(BlackLight)) <= 1 &&
				popcnt(Piece(WhiteDark)) <= 1 && popcnt(Piece(BlackDark)) <= 1 && popcnt(Piece(WhiteKnight)) <= 2 && popcnt(Piece(BlackKnight)) <= 2 && 
				popcnt(Piece(WhiteRook)) <= 2 && popcnt(Piece(BlackRook)) <= 2)
				Next->material ^= FlagUnusualMaterial;
		}
		if (piece == IPawn[me])
		{
			if (IsPromotion(move))
			{
				piece = Promotion<me>(move);
				PieceAt(to) = piece;
				Next->material += MatCode[piece] - MatCode[IPawn[me]];
				if (IsBishop(piece) ? T(Piece(piece)) : Multiple(Piece(piece)))
					Next->material |= FlagUnusualMaterial;
				Pawn(me) ^= mask_to;
				Piece(piece) |= mask_to;
				Next->pst += Pst(piece, to) - Pst(IPawn[me], to);
				Next->key ^= pKey[to] ^ RO->PieceKey[piece][to];
				Next->pawn_key ^= pKey[from];
			}
			else
				Next->pawn_key ^= pKey[from] ^ pKey[to];

			PawnEntry = &PawnHash[Next->pawn_key & PAWN_HASH_MASK];
			prefetch(PawnEntry);
		}
		else if (piece >= WhiteKing)
		{
			Next->pawn_key ^= pKey[from] ^ pKey[to];
			PawnEntry = &PawnHash[Next->pawn_key & PAWN_HASH_MASK];
			prefetch(PawnEntry);
		}
		else if (capture < WhiteKnight)
		{
			PawnEntry = &PawnHash[Next->pawn_key & PAWN_HASH_MASK];
			prefetch(PawnEntry);
		}

		if (Current->castle_flags && (piece >= WhiteRook || capture >= WhiteRook))
		{
			Next->castle_flags &= RO->UpdateCastling[to] & RO->UpdateCastling[from];
			Next->key ^= RO->CastleKey[Current->castle_flags] ^ RO->CastleKey[Next->castle_flags];
			Next->pawn_key ^= RO->CastleKey[Current->castle_flags] ^ RO->CastleKey[Next->castle_flags];
		}
		if (F(Next->material & FlagUnusualMaterial))
			prefetch(&RO->Material[Next->material]);
		if (Current->ep_square)
			Next->key ^= RO->EPKey[FileOf(Current->ep_square)];
		Next->ply = 0;
	}
	else
	{
		Next->ply = Current->ply + 1;
		Next->material = Current->material;
		if (piece == IPawn[me])
		{
			Next->ply = 0;
			Next->pawn_key ^= pKey[to] ^ pKey[from];
			if (IsEP(move))
			{
				PieceAt(to ^ 8) = 0;
				u = Bit(to ^ 8);
				Next->key ^= RO->PieceKey[IPawn[opp]][to ^ 8];
				Next->pawn_key ^= RO->PieceKey[IPawn[opp]][to ^ 8];
				Next->pst -= Pst(IPawn[opp], to ^ 8);
				Pawn(opp) &= ~u;
				Piece(opp) &= ~u;
				Next->material -= MatCode[IPawn[opp]];
			}
			else if (IsPromotion(move))
			{
				piece = Promotion<me>(move);
				PieceAt(to) = piece;
				Next->material += MatCode[piece] - MatCode[IPawn[me]];
				if (IsBishop(piece) ? T(Piece(piece)) : Multiple(Piece(piece)))
					Next->material |= FlagUnusualMaterial;
				Pawn(me) ^= mask_to;
				Piece(piece) |= mask_to;
				Next->pst += Pst(piece, to) - Pst(IPawn[me], to);
				Next->key ^= RO->PieceKey[piece][to] ^ pKey[to];
				Next->pawn_key ^= pKey[to];
			}
			else if ((to ^ from) == 16)
			{
				if (RO->PAtt[me][(to + from) >> 1] & Pawn(opp))
				{
					Next->ep_square = (to + from) >> 1;
					Next->key ^= RO->EPKey[FileOf(Next->ep_square)];
				}
			}
			PawnEntry = &PawnHash[Next->pawn_key & PAWN_HASH_MASK];
			prefetch(PawnEntry);
		}
		else
		{
			if (piece >= WhiteRook)
			{
				if (Current->castle_flags)
				{
					Next->castle_flags &= RO->UpdateCastling[to] & RO->UpdateCastling[from];
					Next->key ^= RO->CastleKey[Current->castle_flags] ^ RO->CastleKey[Next->castle_flags];
					Next->pawn_key ^= RO->CastleKey[Current->castle_flags] ^ RO->CastleKey[Next->castle_flags];
				}
				if (piece >= WhiteKing)
				{
					Next->pawn_key ^= pKey[to] ^ pKey[from];
					PawnEntry = &PawnHash[Next->pawn_key & PAWN_HASH_MASK];
					prefetch(PawnEntry);
				}
			}

			if (IsCastling(piece, move))
			{
				Next->ply = 0;
				int rold = to + ((to & 4) ? 1 : -2);
				int rnew = to + ((to & 4) ? -1 : 1);
				mask_to |= Bit(rnew);
				PieceAt(rold) = 0;
				PieceAt(rnew) = IRook[me];
				Piece(IRook[me]) ^= Bit(rold);
				Piece(me) ^= Bit(rold);
				Piece(IRook[me]) |= Bit(rnew);
				Piece(me) |= Bit(rnew);
				Next->pst += Pst(IRook[me], rnew) - Pst(IRook[me], rold);
				Next->key ^= RO->PieceKey[IRook[me]][rnew] ^ RO->PieceKey[IRook[me]][rold];
			}
		}

		if (Current->ep_square)
			Next->key ^= RO->EPKey[FileOf(Current->ep_square)];
	}	// end F(Capture)

	Next->key ^= RO->TurnKey;
	Entry = HASH + (High32(Next->key) & SETTINGS->hashMask);
	prefetch(Entry);
	++sp;
	Stack[sp] = Next->key;
	Next->move = move;
	Next->gen_flags = 0;
	++Current;
	++INFO->nodes;
	assert(King(me) && King(opp));
	BYE
}
INLINE void do_move(bool me, int move)
{
	if (me)
		do_move<true>(move);
	else
		do_move<false>(move);
}

template<bool me> void undo_move(int move)
{
	MOVING
		const int from = From(move), to = To(move);
	uint64 bFrom = Bit(from), bTo = Bit(to);
	int piece;
	if (IsPromotion(move))
	{
		Piece(PieceAt(to)) ^= bTo;
		piece = IPawn[me];
	}
	else
		piece = PieceAt(to);
	PieceAt(from) = piece;
	Piece(piece) |= bFrom;
	Piece(me) |= bFrom;
	Piece(piece) &= ~bTo;
	Piece(me) ^= bTo;
	PieceAt(to) = Current->capture;
	if (Current->capture)
	{
		Piece(Current->capture) |= bTo;
		Piece(opp) |= bTo;
	}
	else
	{
		if (IsCastling(piece, move))
		{
			bool isQS = FileOf(to) == 2;
			const int rold = to + (isQS ? -2 : 1);
			const int rnew = to + (isQS ? 1 : -1);
			PieceAt(rnew) = 0;
			PieceAt(rold) = IRook[me];
			Rook(me) ^= Bit(rnew);
			Piece(me) ^= Bit(rnew);
			Rook(me) |= Bit(rold);
			Piece(me) |= Bit(rold);
		}
		else if (IsEP(move))
		{
			int xLoc = to ^ 8;
			PieceAt(xLoc) = IPawn[opp];
			Piece(opp) |= Bit(xLoc);
			Pawn(opp) |= Bit(xLoc);
		}
	}
	--Current;
	--sp;
	BYE
}
INLINE void undo_move(bool me, int move)
{
	if (me)
		undo_move<true>(move);
	else
		undo_move<false>(move);
}

void do_null()
{
	GData* Next;
	GEntry* Entry;

	Next = Current + 1;
	Next->key = Current->key ^ RO->TurnKey;
	Entry = HASH + (High32(Next->key) & SETTINGS->hashMask);
	prefetch(Entry);
	Next->pawn_key = Current->pawn_key;
	Next->eval_key = 0;
	Next->turn = Current->turn ^ 1;
	Next->material = Current->material;
	Next->pst = Current->pst;
	Next->ply = 0;
	Next->castle_flags = Current->castle_flags;
	Next->ep_square = 0;
	Next->capture = 0;
	if (Current->ep_square)
		Next->key ^= RO->EPKey[FileOf(Current->ep_square)];
	++sp;
	Next->att[White] = Current->att[White];
	Next->att[Black] = Current->att[Black];
	Next->patt[White] = Current->patt[White];
	Next->patt[Black] = Current->patt[Black];
	Next->xray[White] = Current->xray[White];
	Next->xray[Black] = Current->xray[Black];
	Stack[sp] = Next->key;
	Next->threat = Current->threat;
	Next->passer = Current->passer;
	Next->score = -Current->score;
	Next->move = 0;
	Next->gen_flags = 0;
	++Current;
	++INFO->nodes;
}

void undo_null()
{
	--Current;
	--sp;
}

struct ScopedMove_
{
	int move_;
	ScopedMove_(int move) : move_(move)
	{
		if (Current->turn)
			do_move<1>(move);
		else
			do_move<0>(move);
	}
	~ScopedMove_()
	{
		if (Current->turn ^ 1)
			undo_move<1>(move_);
		else
			undo_move<0>(move_);
	}
};

typedef struct
{
	uint64 patt[2], double_att[2];
	int king[2];
	packed_t score;
} GPawnEvalInfo;

template<bool me, class POP> INLINE void eval_pawns(GPawnEntry* PawnEntry, GPawnEvalInfo& PEI)
{
	static const array<array<uint64, 2>, 2> RFileBlockMask = { array<uint64, 2>({ 0x0202000000000000, 0x8080000000000000 }), array<uint64, 2>({ 0x0202, 0x8080 }) };
	POP pop;
	int kf = FileOf(PEI.king[me]);
	int kr = RankOf(PEI.king[me]);
	int start, inc;
	if (kf <= 3)
	{
		start = Max(kf - 1, 0);
		inc = 1;
	}
	else
	{
		start = Min(kf + 1, 7);
		inc = -1;
	}
	int shelter = 0, sScale = 0;
	uint64 mpawns = Pawn(me) & RO->Forward[me][me ? Min(kr + 1, 7) : Max(kr - 1, 0)];
	for (int file = start, i = 0; i < 3; file += inc, ++i)
	{
		shelter += RO->Shelter[i][OwnRank<me>(NBZ<me>(mpawns & File[file]))];
		if (Pawn(opp) & File[file])
		{
			int oppP = NB<me>(Pawn(opp) & File[file]);
			int rank = OwnRank<opp>(oppP);
			if (rank < 6)
			{
				if (rank >= 3)
					shelter += RO->StormBlocked[rank - 3];
				if (uint64 u = (RO->PIsolated[FileOf(oppP)] & RO->Forward[opp][RankOf(oppP)] & Pawn(me)))
				{
					int meP = NB<opp>(u);
					uint64 att_sq = RO->PAtt[me][meP] & RO->PWay[opp][oppP];  // may be zero
					if (abs(kf - FileOf(meP)) <= 1
						&& (!(PEI.double_att[me] & att_sq) || (Current->patt[opp] & att_sq))
						&& F(RO->PWay[opp][meP] & Pawn(me))
						&& (!(PawnAll() & RO->PWay[opp][oppP] & RO->Forward[me][RankOf(meP)])))
					{
						if (rank >= 3)
						{
							shelter += RO->StormShelterAtt[rank - 3];
							if (HasBit(PEI.patt[opp], oppP + Push[opp]))
								shelter += RO->StormConnected[rank - 3];
							if (!(RO->PWay[opp][oppP] & PawnAll()))
								shelter += RO->StormOpen[rank - 3];
						}
						if (rank <= 4 && !(RO->PCone[me][oppP] & King(opp)))
							shelter += RO->StormFree[rank - 1];
					}
				}
			}
		}
		else
		{
			if (i > 0 || T((File[file] | File[file + inc]) & (Rook(opp) | Queen(opp))) || T(RFileBlockMask[me][inc > 0] & ~(Pawn(opp) | King(opp))))
			{
				if (F(Pawn(me) & File[file]))
				{
					shelter += ShelterMod[StormOfValue];
					sScale += ShelterMod[StormOfScale];
				}
				else
				{
					shelter += ShelterMod[StormHofValue];
					sScale += ShelterMod[StormHofScale];
				}
			}
		}
	}
	PawnEntry->shelter[me] = shelter + (shelter * sScale) / 64;

	PawnEntry->passer[me] = 0;
	uint64 b;
	for (uint64 u = Pawn(me); T(u); u ^= b)
	{
		int sq = lsb(u);
		b = Bit(sq);
		int rank = RankOf(sq);
		int rrank = OwnRank<me>(sq);
		int file = FileOf(sq);
		uint64 way = RO->PWay[me][sq];
		int next = PieceAt(sq + Push[me]);

		int isolated = !(Pawn(me) & RO->PIsolated[file]);
		int doubled = T(Pawn(me) & (File[file] ^ b));
		int open = !(PawnAll() & way);

		if (isolated)
		{
			DecV(PEI.score, open ? Values::IsolatedOpen : Values::IsolatedClosed);
			if (F(open) && next == IPawn[opp])
				DecV(PEI.score, Values::IsolatedBlocked);
			if (doubled)
				DecV(PEI.score, open ? Values::IsolatedDoubledOpen : Values::IsolatedDoubledClosed);
		}
		else
		{
			if (doubled)
			{
				if (!(Pawn(me) & RO->PCone[opp][sq]))
					DecV(PEI.score, open ? Values::IsolatedDoubledOpen : Values::IsolatedDoubledClosed);	// unsupportable trailing pawn is as bad as isolated
				else
					DecV(PEI.score, open ? Values::DoubledOpen : Values::DoubledClosed);
			}
			if (rrank >= 3 && (b & (File[2] | File[3] | File[4] | File[5])) && next != IPawn[opp] && (RO->PIsolated[file] & Line[rank] & Pawn(me)))
				IncV(PEI.score, Values::PawnChainLinear * (rrank - 3) + Values::PawnChain);
		}
		int backward = 0;
		if (!(RO->PSupport[me][sq] & Pawn(me)))
		{
			if (isolated)
				backward = 1;
			else if (uint64 v = (PawnAll() | PEI.patt[opp]) & way)
				if (OwnRank<me>(NB<me>(PEI.patt[me] & way)) > OwnRank<me>(NB<me>(v)))
					backward = 1;
		}
		if (backward)
			DecV(PEI.score, open ? Values::BackwardOpen : Values::BackwardClosed);
		else
		{
			if (open && (F(Pawn(opp) & RO->PIsolated[file]) || pop(Pawn(me) & RO->PIsolated[file]) >= pop(Pawn(opp) & RO->PIsolated[file])))
				IncV(PEI.score, RO->PasserCandidate[rrank]);  // IDEA: more precise pawn counting for the case of, say,
															  // white e5 candidate with black pawn on f5 or f4...
		}

		if (F(PEI.patt[me] & b) && next == IPawn[opp])  // unprotected and can't advance
		{
			DecV(PEI.score, Values::UpBlocked);
			if (backward)
			{
				if (rrank <= 2)
				{
					DecV(PEI.score, Values::PasserTarget);
					if (rrank <= 1)
						DecV(PEI.score, Values::PasserTarget);
				}	// Gull 3 was thinking of removing this term, because fitted weight is negative

				for (uint64 v = RO->PAtt[me][sq] & Pawn(me); v; Cut(v)) if ((RO->PSupport[me][lsb(v)] & Pawn(me)) == b)
				{
					DecV(PEI.score, Values::ChainRoot);
					break;
				}
			}
		}
		if (open && F(RO->PIsolated[file] & RO->Forward[me][rank] & Pawn(opp)))
		{
			PawnEntry->passer[me] |= (uint8)(1 << file);
			if (rrank <= 2)
				continue;
			IncV(PEI.score, RO->PasserGeneral[rrank]);
			// IDEA: average the distance with the distance to the promotion square? or just use the latter?
			int dist_att = Dist(PEI.king[opp], sq + Push[me]);
			int dist_def = Dist(PEI.king[me], sq + Push[me]);
			int value = dist_att * RO->PasserAtt[rrank] + RO->LogDist[dist_att] * RO->PasserAttLog[rrank]
				- dist_def * RO->PasserDef[rrank] - RO->LogDist[dist_def] * RO->PasserDefLog[rrank];  // IDEA -- incorporate side-to-move in closer-king check?
																									  // IDEA -- scale down rook pawns?
			IncV(PEI.score, Pack2(0, value / 256));
			if (PEI.patt[me] & b)
				IncV(PEI.score, RO->PasserProtected[rrank]);
			if (F(Pawn(opp) & RO->West[file]) || F(Pawn(opp) & RO->East[file]))
				IncV(PEI.score, RO->PasserOutside[rrank]);
		}
	}

	uint64 files = Pawn(me) | (Pawn(me) >> 32);
	files |= files >> 16;
	files = (files | (files >> 8)) & 0xFF;
	int file_span = (files ? (msb(files) - lsb(files)) : 0);
	IncV(PEI.score, Values::PawnFileSpan * file_span);
	PawnEntry->draw[me] = (7 - file_span) * Max(5 - pop(files), 0);
}

template<class POP> INLINE void eval_pawn_structure(const GEvalInfo& EI, GPawnEntry* PawnEntry)
{
	GPawnEvalInfo PEI;
	for (int i = 0; i < sizeof(GPawnEntry) / sizeof(int); ++i)
		*(((int*)PawnEntry) + i) = 0;
	PawnEntry->key = Current->pawn_key;

	PEI.patt[White] = Current->patt[White];
	PEI.patt[Black] = Current->patt[Black];
	PEI.double_att[White] = ShiftW<White>(Pawn(White)) & ShiftE<White>(Pawn(White));
	PEI.double_att[Black] = ShiftW<Black>(Pawn(Black)) & ShiftE<Black>(Pawn(Black));
	PEI.king[White] = EI.king[White];
	PEI.king[Black] = EI.king[Black];
	PEI.score = 0;

	eval_pawns<White, POP>(PawnEntry, PEI);
	eval_pawns<Black, POP>(PawnEntry, PEI);

	PawnEntry->score = PEI.score;
}

template<bool me, class POP> INLINE void eval_queens(GEvalInfo& EI)
{
	POP pop;
	uint64 u, b;
	for (u = Queen(me); T(u); u ^= b)
	{
		int sq = lsb(u);
		b = Bit(sq);
		uint64 att = QueenAttacks(sq, EI.occ);
		Current->att[me] |= att;
		if (QMask[sq] & King(opp))
			if (uint64 v = RO->Between[EI.king[opp]][sq] & EI.occ)
				if (Single(v))
				{
					Current->xray[me] |= v;
					int square = lsb(v);
					int piece = PieceAt(square);
					int katt = 0;
					if (piece == IPawn[me])
					{
						if (!PieceAt(square + Push[me]))
							IncV(EI.score, Values::SelfPawnPin);
					}
					else if ((piece & 1) == me)
					{
						//						if (piece < WhiteRook)	// double major attack is handled elsewhere
						{
							IncV(EI.score, Values::SelfPiecePin);
							katt = 1;
						}
					}
					else if (piece != IPawn[opp] && !(((BMask[sq] & Bishop(opp)) | (RMask[sq] & Rook(opp)) | Queen(opp)) & v))
					{
						IncV(EI.score, Values::WeakPin);
						if (!(Current->patt[opp] & v))
							katt = 1;
					}
					if (katt && !(att & EI.area[opp]))
						EI.king_att[me] += KingAttack;
				}
				else if (v == (v & Minor(opp)))
					IncV(EI.score, Values::QKingRay);

		if (uint64 a = att & EI.area[opp])
		{
			EI.king_att[me] += Single(a) ? KingQAttack1 : KingQAttack;
			uint64 b = att & ((Rook(me) & RMask[sq]) | (Bishop(me) & BMask[sq]));
			for (uint64 v = a; T(v); Cut(v))
				if (RO->FullLine[sq][lsb(v)] & b)
					EI.king_att[me]++;
		}
		uint64 control = EI.free[me];
		if (uint64 dbl = att & RO->PWay[me][sq] & Rook(me))  // we are supporting a rook
			control &= QueenAttacks(sq, EI.occ & ~dbl);
		else
			control &= att;
		IncV(EI.score, RO->MobQueen[0][pop(control)]);
		IncV(EI.score, RO->MobQueen[1][pop(control & RO->KingFlank[EI.king[opp]])]);
		if (control & Pawn(opp))
			IncV(EI.score, Values::TacticalMajorPawn);
		if (control & Minor(opp))
			IncV(EI.score, Values::TacticalMajorMinor);
		if (att & EI.area[me])
			IncV(EI.score, Values::KingDefQueen);
	}
}

template<bool me, class POP> INLINE void eval_rooks(GEvalInfo& EI)
{
	POP pop;
	uint64 u, b;
	for (u = Rook(me); T(u); u ^= b)
	{
		int sq = lsb(u);
		b = Bit(sq);
		uint64 att = RookAttacks(sq, EI.occ);
		Current->att[me] |= att;
		if (RMask[sq] & King(opp))
			if (uint64 v = RO->Between[EI.king[opp]][sq] & EI.occ)
				if (Single(v))
				{
					Current->xray[me] |= v;
					int square = lsb(v);
					int piece = PieceAt(square);
					int katt = 0;
					if (piece == IPawn[me])
					{
						if (!PieceAt(square + Push[me]))
							IncV(EI.score, Values::SelfPawnPin);
					}
					else if ((piece & 1) == me)
					{
						//						if (piece < WhiteRook)	// double major attack is handled elsewhere
						{
							IncV(EI.score, Values::SelfPiecePin);
							katt = 1;
						}
					}
					else if (piece != IPawn[opp])
					{
						if (piece < WhiteRook)
						{
							IncV(EI.score, Values::WeakPin);
							if (!(Current->patt[opp] & v))
								katt = 1;
						}
						else if (piece >= WhiteQueen)
							IncV(EI.score, Values::ThreatPin);
					}
					if (katt && F(att & EI.area[opp]))
						EI.king_att[me] += KingAttack;
				}
				else if (F(v & ~Minor(opp) & ~Queen(opp)))
					IncV(EI.score, Values::RKingRay);

		if (uint64 a = att & EI.area[opp])
		{
			EI.king_att[me] += Single(a) ? KingRAttack1 : KingRAttack;
			for (uint64 v = att & EI.area[opp]; T(v); Cut(v))
				if (RO->FullLine[sq][lsb(v)] & att & Major(me))
					EI.king_att[me]++;
		}
		Current->threat |= att & Queen(opp);
		uint64 control = EI.free[me];
		if (uint64 dbl = att & File[FileOf(sq)] & Major(me))
		{	// doubled r/q
			control &= RookAttacks(sq, EI.occ & ~dbl);
		}
		else if (uint64 behind = att & Current->passer & Pawn(me) & RO->PWay[opp][sq])
		{	// we are in front of a pawn
			control &= att & File[FileOf(sq)];	// restrict to squares in front of passer
		}
		else
			control &= att;
		IncV(EI.score, RO->MobRook[0][pop(control)]);
		IncV(EI.score, RO->MobRook[1][pop(control & RO->KingFlank[EI.king[opp]])]);
		if (control & Pawn(opp))
			IncV(EI.score, Values::TacticalMajorPawn);
		if (control & Minor(opp))
			IncV(EI.score, Values::TacticalMajorMinor);
		if (!(RO->PWay[me][sq] & Pawn(me)))
		{
			IncV(EI.score, Values::RookHof);
			packed_t hof_score = 0;
			if (!(RO->PWay[me][sq] & Pawn(opp)))
			{
				IncV(EI.score, Values::RookOf);
				if (att & OwnLine(me, 7))
					hof_score += Values::RookOfOpen;
				else if (uint64 target = att & RO->PWay[me][sq] & Minor(opp))
				{
					if (!(Current->patt[opp] & target))
					{
						hof_score += Values::RookOfMinorHanging;
						if (RO->PWay[me][sq] & King(opp))
							hof_score += Values::RookOfKingAtt;
					}
					else
						hof_score += Values::RookOfMinorFixed;
				}
			}
			else if (att & RO->PWay[me][sq] & Pawn(opp))
			{
				uint64 square = lsb(att & RO->PWay[me][sq] & Pawn(opp));
				if (!(RO->PSupport[opp][square] & Pawn(opp)))
					hof_score += Values::RookHofWeakPAtt;
			}
			IncV(EI.score, hof_score);
			if (RO->PWay[opp][sq] & att & Major(me))
				IncV(EI.score, hof_score);
		}
		if ((b & OwnLine(me, 6)) && ((King(opp) | Pawn(opp)) & (OwnLine(me, 6) | OwnLine(me, 7))))
		{
			IncV(EI.score, Values::Rook7th);
			if (King(opp) & OwnLine(me, 7))
				IncV(EI.score, Values::Rook7thK8th);
			if (Major(me) & att & OwnLine(me, 6))
				IncV(EI.score, Values::Rook7thDoubled);
		}
	}
}

template<bool me, class POP> INLINE void eval_bishops(GEvalInfo& EI)
{
	POP pop;
	uint64 b;
	for (uint64 u = Bishop(me); T(u); u ^= b)
	{
		int sq = lsb(u);
		b = Bit(sq);
		uint64 att = BishopAttacks(sq, EI.occ);
		Current->att[me] |= att;
		if (BMask[sq] & King(opp))
			if (uint64 v = RO->Between[EI.king[opp]][sq] & EI.occ)
				if (Single(v))  // pin or discovery threat
				{
					Current->xray[me] |= v;
					int square = lsb(v);
					int piece = PieceAt(square);
					int katt = 0;
					if (piece == IPawn[me])
					{
						if (!PieceAt(square + Push[me]))
							IncV(EI.score, Values::SelfPawnPin);
					}
					else if ((piece & 1) == me)
					{
						if (piece < WhiteQueen)
						{
							IncV(EI.score, Values::SelfPiecePin);
							katt = 1;
						}
					}
					else if (piece != IPawn[opp])
					{
						if (piece < WhiteLight)
						{
							IncV(EI.score, Values::StrongPin);
							if (!(Current->patt[opp] & v))
								katt = 1;
						}
						else if (piece >= WhiteRook)
							IncV(EI.score, Values::ThreatPin);
					}
					if (katt && F(att & EI.area[opp]))
						EI.king_att[me] += KingAttack;
				}
				else if (F(v & ~Knight(opp) & ~Major(opp)))
					IncV(EI.score, Values::BKingRay);

		if (uint64 a = att & EI.area[opp])
			EI.king_att[me] += Single(a) ? KingBAttack1 : KingBAttack;
		uint64 control = att & EI.free[me];
		IncV(EI.score, RO->MobBishop[0][pop(control)]);
		IncV(EI.score, RO->MobBishop[1][pop(control & RO->KingFrontal[EI.king[opp]])]);
		if (control & Pawn(opp))
			IncV(EI.score, Values::TacticalMinorPawn);
		if (control & Knight(opp))
			IncV(EI.score, Values::TacticalMinorMinor);
		if (att & EI.area[me])
			IncV(EI.score, Values::KingDefBishop);
		Current->threat |= att & Major(opp);
		const uint64& myArea = (b & LightArea) ? LightArea : DarkArea;
		uint64 v = RO->BishopForward[me][sq] & Pawn(me) & myArea;
		v |= (v & (File[2] | File[3] | File[4] | File[5] | BMask[sq])) >> 8;	// the ">>8" is just a trick to double-count these without two calls to pop()
		DecV(EI.score, Values::BishopPawnBlock * pop(v));
		DecV(EI.score, Values::BishopOppPawnOffColor * pop(Pawn(opp) & ~myArea));
		DecV(EI.score, Values::BishopOppPawnBlock * pop(Pawn(opp) & myArea & ~EI.free[me]));
		if (T(b & Outpost[me])
			&& F(Knight(opp))
			&& T(Current->patt[me] & b)
			&& F(Pawn(opp) & RO->PIsolated[FileOf(sq)] & RO->Forward[me][RankOf(sq)])
			&& F(Piece((T(b & LightArea) ? WhiteLight : WhiteDark) | opp)))
			IncV(EI.score, Values::BishopOutpostNoMinor);
	}
}

template<bool me, class POP> INLINE void eval_knights(GEvalInfo& EI)
{
	POP pop;
	uint64 u, b;
	for (u = Knight(me); T(u); u ^= b)
	{
		int sq = lsb(u);
		b = Bit(sq);
		uint64 att = NAtt[sq];
		Current->att[me] |= att;
		if (uint64 a = att & EI.area[opp])
			EI.king_att[me] += Single(a) ? KingNAttack1 : KingNAttack;
		Current->threat |= att & Major(opp);
		uint64 control = att & EI.free[me];
		IncV(EI.score, RO->MobKnight[0][pop(control)]);
		IncV(EI.score, RO->MobKnight[1][pop(control & RO->KingFrontal[EI.king[opp]])]);
		if (control & Pawn(opp))
			IncV(EI.score, Values::TacticalMinorPawn);
		if (control & Bishop(opp))
			IncV(EI.score, Values::TacticalMinorMinor);
		if (att & EI.area[me])
			IncV(EI.score, Values::KingDefKnight);
		if ((b & Outpost[me]) && !(Pawn(opp) & RO->PIsolated[FileOf(sq)] & RO->Forward[me][RankOf(sq)]))
		{
			IncV(EI.score, Values::KnightOutpost);
			if (Current->patt[me] & b)
			{
				IncV(EI.score, Values::KnightOutpostProtected);
				if (att & EI.free[me] & Pawn(opp))
					IncV(EI.score, Values::KnightOutpostPawnAtt);
				if (F(Knight(opp)) && F(Piece((T(b & LightArea) ? WhiteLight : WhiteDark) | opp)))
					IncV(EI.score, Values::KnightOutpostNoMinor);
			}
		}
		int pf = FileOcc(PawnAll());
		int width = max(0, RO->SpanWidth[pf] - 2);
		DecV(EI.score, Values::KnightPawnSpread * width);
		int gap = Square(RO->SpanGap[pf]) / pop(NonPawnKing(me));
		DecV(EI.score, Values::KnightPawnGap * gap);
	}
}

template<bool me, class POP> INLINE void eval_king(GEvalInfo& EI)
{
	POP pop;
	uint16 cnt = Min<uint16>(15, UUnpack1(EI.king_att[me]));
	uint16 score = UUnpack2(EI.king_att[me]);
	if (cnt >= 2 && T(Queen(me)))
	{
		score += (EI.PawnEntry->shelter[opp] * KingShelterQuad) / 64;
		if (uint64 u = Current->att[me] & EI.area[opp] & (~Current->att[opp]))
			score += pop(u) * KingAttackSquare;
		if (!(KAtt[EI.king[opp]] & (~(Piece(opp) | Current->att[me]))))
			score += KingNoMoves;
	}
	score = score < KingAttackThreshold
			? Square(score) / (2 * KingAttackThreshold)
			: score - KingAttackThreshold / 2;
	int adjusted = ((score * RO->KingAttackScale[cnt]) >> 3) + EI.PawnEntry->shelter[opp];
	int kf = FileOf(EI.king[opp]);
	adjusted = (adjusted * RO->KingCenterScale[kf]) / 64;
	if (!Queen(me))
		adjusted = (adjusted * KingSafetyNoQueen) / 16;
	// add a correction for defense-in-depth
	if (adjusted > 10)
	{
		uint64 holes = RO->KingFrontal[EI.king[opp]] & ~Current->att[opp];
		int nHoles = pop(holes);
		int nIncursions = pop(holes & Current->att[me]);
		uint64 personnel = NonPawnKing(opp);
		uint64 guards = RO->KingFrontal[EI.king[opp]] & personnel;
		uint64 awol = personnel ^ guards;
		int nGuards = pop(guards) + pop(guards & Queen(opp));
		int nAwol = pop(awol) + pop(awol & Queen(opp));
		adjusted += (adjusted * (max(0, nAwol - nGuards) + max(0, 3 * nIncursions + nHoles - 10))) / 32;
	}

	static constexpr array<int, 4> PHASE = { 12, 8, 2, -2 };
	int op = (PHASE[0] * adjusted) / 16;
	int md = (PHASE[1] * adjusted) / 16;
	int eg = (PHASE[2] * adjusted) / 16;
	int cl = (PHASE[3] * adjusted) / 16;
	EI.king_score[me] = Pack4(op, md, eg, cl);
	IncV(EI.score, EI.king_score[me]);
}

template<bool me, class POP> INLINE void eval_passer(GEvalInfo& EI)
{
	bool sr_me = Rook(me) && !Minor(me) && !Queen(me) && Single(Rook(me));
	bool sr_opp = Rook(opp) && !Minor(opp) && !Queen(opp) && Single(Rook(opp));

	for (uint8 u = EI.PawnEntry->passer[me]; T(u); u &= (u - 1))
	{
		int file = lsb(u);
		int sq = NB<opp>(File[file] & Pawn(me));  // most advanced in this file
		int rank = OwnRank<me>(sq);
		Current->passer |= Bit(sq);
		if (rank <= 2)
			continue;
		if (!PieceAt(sq + Push[me]))
			IncV(EI.score, RO->PasserBlocked[rank]);
		uint64 way = RO->PWay[me][sq];
		int connected = 0, supported = 0, hooked = 0, unsupported = 0, free = 0;
		if (!(way & Piece(opp)))
		{
			IncV(EI.score, RO->PasserClear[rank]);
			if (RO->PWay[opp][sq] & Major(me))
			{
				int square = NB<opp>(RO->PWay[opp][sq] & Major(me));
				if (F(RO->Between[sq][square] & EI.occ))
					supported = 1;
			}
			if (RO->PWay[opp][sq] & Major(opp))
			{
				int square = NB<opp>(RO->PWay[opp][sq] & Major(opp));
				if (F(RO->Between[sq][square] & EI.occ))
					hooked = 1;
			}
			for (uint64 v = RO->PAtt[me][sq - Push[me]] & Pawn(me); T(v); Cut(v))
			{
				int square = lsb(v);
				if (F(Pawn(opp) & (VLine[square] | RO->PIsolated[FileOf(square)]) & RO->Forward[me][RankOf(square)]))
					++connected;
			}
			if (connected)
			{
				IncV(EI.score, RO->PasserConnected[rank] * min(file + 2, 9 - file));
			}
			if (!hooked && !(Current->att[opp] & way))
			{
				IncV(EI.score, RO->PasserFree[rank]);
				free = 1;
			}
			else
			{
				uint64 attacked = Current->att[opp] | (hooked ? way : 0);
				if (supported || (!hooked && connected) || (!(Major(me) & way) && !(attacked & (~Current->att[me]))))
					IncV(EI.score, RO->PasserSupported[rank]);
				else
					unsupported = 1;
			}
		}

		if (sr_me)
		{
			if (rank == 6 && T(way & Rook(me)))
				DecV(EI.score, Values::PasserOpRookBlock);
		}
	}
}

template<class POP> INLINE packed_t eval_threat(const uint64& threat)
{
	POP pop;
	if (Single(threat))
		return threat ? Values::TacticalThreat : 0;
	// according to Gull, second threat is extra DoubleThreat, third and after are simple Threat again
	return Values::TacticalDoubleThreat + pop(threat) * Values::TacticalThreat;
}

template<bool me, class POP> INLINE void eval_pieces(GEvalInfo& EI)
{
	POP pop;
	Current->threat |= Current->att[opp] & (~Current->att[me]) & Piece(me);
	DecV(EI.score, eval_threat<POP>(Current->threat & Piece(me)));
	if (T(Queen(me)) && F(Queen(opp)))
		IncV(EI.score, pop((Piece(opp) ^ King(opp)) & ~Current->att[opp]) * Values::TacticalUnguardedQ);
}


template<class POP> void eval_unusual_material(GEvalInfo& EI)
{
	POP pop;
	Current->score = Endgame(EI.score)
		+ SeeValue[WhitePawn] * (pop(Pawn(White)) - pop(Pawn(Black)))
		+ SeeValue[WhiteKnight] * (pop(Minor(White)) - pop(Minor(Black)))
		+ SeeValue[WhiteRook] * (pop(Rook(White)) - pop(Rook(Black)))
		+ SeeValue[WhiteQueen] * (pop(Queen(White)) - pop(Queen(Black)));
}

template<class POP, int me> int closure_x()
{
	POP pop;
	int run = 0;
	uint64 mine = Pawn(me);
	int np = pop(mine);
	uint64 keep = ~(mine | Pawn(opp));	// hard stop if we run into a pawn
	uint64 soft = (Current->patt[opp] | Piece(opp)) & ~mine;
	keep &= ~Shift<me>(soft);// if we run into a piece or pawn capture, count 1 then stop
	for (; ;)
	{
		mine = keep & Shift<me>(mine);
		if (F(mine))
			break;
		run += pop(mine);
	}
	return 2 * np - run;
}

template<class POP> int closure()
{
	// closure_x can return up to 16; we want to return about -128 to +128
	return 4 * (closure_x<POP, 0>() + closure_x<POP, 1>());
}

template<bool me> void eval_castling(GEvalInfo& EI)
{
	static constexpr array<int, 2> now = { 30, 10 };
	static constexpr array<int, 2> later = { 15, 5 };
	uint64 occ = PieceAll();
	if (can_castle(occ, me, true))
		IncV(EI.score, Pack4(now[0], 0, 0, 0));
	else if (Current->castle_flags & (me ? CanCastle_oo : CanCastle_OO))
		IncV(EI.score, Pack4(later[0], 0, 0, 0));
	if (can_castle(occ, me, false))
		IncV(EI.score, Pack4(now[1], 0, 0, 0));
	else if (Current->castle_flags & (me ? CanCastle_ooo : CanCastle_OOO))
		IncV(EI.score, Pack4(later[1], 0, 0, 0));
}

template<bool me, class POP> void eval_sequential(GEvalInfo& EI)
{
	POP pop;
	Current->xray[me] = 0;
	EI.king_att[me] = T(Current->patt[me] & EI.area[opp]) * KingPAttack;
	DecV(EI.score, pop(Shift<opp>(EI.occ) & Pawn(me)) * Values::PawnBlocked);
	EI.free[me] = Queen(opp) | King(opp) | (~(Current->patt[opp] | Pawn(me) | King(me)));
	eval_queens<me, POP>(EI);
	EI.free[me] |= Rook(opp);
	eval_rooks<me, POP>(EI);
	EI.free[me] |= Minor(opp);
	eval_bishops<me, POP>(EI);
	eval_knights<me, POP>(EI);
}

template<class POP> struct PhasedScore_
{
	const GMaterial& mat_;
	int clx_;
	PhasedScore_(const GMaterial& mat) : mat_(mat), clx_(closure<POP>()) {}
	int operator()(packed_t score)
	{
		int phase = mat_.phase, op = Opening(score), eg = Endgame(score), md = Middle(score), cl = Closed(score);
		int retval;
		if (mat_.phase > MIDDLE_PHASE)
			retval = (op * (phase - MIDDLE_PHASE) + md * (MAX_PHASE - phase)) / PHASE_M2M;
		else
			retval = (md * phase + eg * (MIDDLE_PHASE - phase)) / MIDDLE_PHASE;
		retval += static_cast<sint16>((clx_ * (Min<int>(phase, MIDDLE_PHASE) * cl + MIDDLE_PHASE * mat_.closed)) / 8192);	// closure is capped at 128, phase at 64
		return retval;
	}
};

INLINE int king_att_depreciation(int score, int my_katt, int opp_katt)
{
	constexpr int threshold = 110 * CP_EVAL;
	return max(0, min(score - threshold, my_katt) / 6) + max(0, min(score - threshold, opp_katt) / 6);
}


template<class POP> void evaluation()
{
	POP pop;
	GEvalInfo EI;

	if (Current->eval_key == Current->key)
		return;
	Current->eval_key = Current->key;

	EI.king[White] = lsb(King(White));
	EI.king[Black] = lsb(King(Black));
	EI.occ = PieceAll();
	Current->patt[White] = ShiftW<White>(Pawn(White)) | ShiftE<White>(Pawn(White));
	Current->patt[Black] = ShiftW<Black>(Pawn(Black)) | ShiftE<Black>(Pawn(Black));
	EI.area[White] = (KAtt[EI.king[White]] | King(White)) & ((~Current->patt[White]) | Current->patt[Black]);
	EI.area[Black] = (KAtt[EI.king[Black]] | King(Black)) & ((~Current->patt[Black]) | Current->patt[White]);
	Current->att[White] = Current->patt[White];
	Current->att[Black] = Current->patt[Black];
	Current->passer = 0;
	Current->threat = (Current->patt[White] & NonPawn(Black)) | (Current->patt[Black] & NonPawn(White));
	EI.score = Current->pst;
	if (F(Current->material & FlagUnusualMaterial))
		EI.material = &RO->Material[Current->material];
	else
		EI.material = nullptr;

	eval_sequential<White, POP>(EI);
	eval_sequential<Black, POP>(EI);

	EI.PawnEntry = &PawnHash[Current->pawn_key & PAWN_HASH_MASK];
	if (Current->pawn_key != EI.PawnEntry->key)
		eval_pawn_structure<POP>(EI, EI.PawnEntry);
	EI.score += EI.PawnEntry->score;

	eval_king<White, POP>(EI);
	eval_king<Black, POP>(EI);
	Current->att[White] |= KAtt[EI.king[White]];
	Current->att[Black] |= KAtt[EI.king[Black]];

	eval_passer<White, POP>(EI);
	eval_pieces<White, POP>(EI);
	eval_passer<Black, POP>(EI);
	eval_pieces<Black, POP>(EI);

	if (Current->material & FlagUnusualMaterial)
	{
		eval_unusual_material<POP>(EI);
		Current->score = (Current->score * CP_SEARCH) / CP_EVAL;
	}
	else
	{
		if (!EI.material)
			Say("No material");
		const GMaterial& mat = *EI.material;
		PhasedScore_<POP> value(mat);
		Current->score = mat.score + value(EI.score);
		// cash out king attacks
//		if (Current->score > 0)
//			Current->score -= king_att_depreciation(Current->score, value(EI.king_score[White]), value(EI.king_score[Black]));
//		else
//			Current->score += king_att_depreciation(-Current->score, value(EI.king_score[Black]), value(EI.king_score[White]));
		
		// apply contempt before drawishness
		if (SETTINGS->contempt > 0)
		{
			int maxContempt = (mat.phase * SETTINGS->contempt * CP_EVAL) / 64;
			int mySign = F(Data->turn) ? 1 : -1;
			if (Current->score * mySign > 2 * maxContempt)
				Current->score += mySign * maxContempt;
			else if (Current->score * mySign > 0)
				Current->score += Current->score / 2;
		}

		if (Current->ply >= PliesToEvalCut)
			Current->score /= 2;
		const int drawCap = DrawCapConstant + (DrawCapLinear * abs(Current->score)) / 64;  // drawishness of positions can cancel this much of the score
		if (Current->score > 0)
		{
			EI.mul = mat.mul[White];
			if (mat.eval[White] && !eval_stalemate<White>(EI))
				mat.eval[White](EI, pop.Imp());
			else if (EI.mul <= 32)
				EI.mul = Min<uint16>(EI.mul, 36 - value.clx_ / 6);
			Current->score -= (Min<int>(Current->score, drawCap) * EI.PawnEntry->draw[White]) / 64;
		}
		else if (Current->score < 0)
		{
			EI.mul = mat.mul[Black];
			if (mat.eval[Black] && !eval_stalemate<Black>(EI))
				mat.eval[Black](EI, pop.Imp());
			else if (EI.mul <= 32)
				EI.mul = Min<uint16>(EI.mul, 36 - value.clx_ / 6);
			Current->score += (Min<int>(-Current->score, drawCap) * EI.PawnEntry->draw[Black]) / 64;
		}
		else
			EI.mul = Min(mat.mul[White], mat.mul[Black]);
		Current->score = (Current->score * EI.mul * CP_SEARCH) / (32 * CP_EVAL);
	}
	if (Current->turn)
		Current->score = -Current->score;
	if (F(Current->capture) && T(Current->move) && F(Current->move & 0xE000) && Current > Data)
	{
		sint16& delta = DeltaScore(Current->piece, From(Current->move), To(Current->move));
		const sint16 observed = -Current->score - (Current - 1)->score;
		if (observed >= delta)
			delta = observed;
		else
			delta -= DeltaDecrement;
	}
}

int HardwarePopCnt;
INLINE void evaluate()
{
	HardwarePopCnt ? evaluation<pop1_>() : evaluation<pop0_>();
	BYE
}

template<bool me> bool is_legal(int move)
{
	int from, to, piece, capture;
	uint64 u, occ;

	from = From(move);
	to = To(move);
	piece = Board->square[from];
	capture = Board->square[to];
	if (piece == 0)
		return 0;
	if ((piece & 1) != Current->turn)
		return 0;
	if (capture)
	{
		if ((capture & 1) == (piece & 1))
			return 0;
		if (capture >= WhiteKing)
			return 0;
	}
	occ = PieceAll();
	u = Bit(to);
	if (piece >= WhiteLight && piece < WhiteKing)
	{
		if ((QMask[from] & u) == 0)
			return 0;
		if (RO->Between[from][to] & occ)
			return 0;
	}
	if (IsEP(move))
	{
		if (piece >= WhiteKnight)
			return 0;
		if (Current->ep_square != to)
			return 0;
		return 1;
	}
	if (IsCastling(piece, move) && Board->square[from] < WhiteKing)
		return 0;
	if (IsPromotion(move) && Board->square[from] >= WhiteKnight)
		return 0;
	if (piece == IPawn[me])
	{
		if (u & RO->PMove[me][from])
		{
			if (capture)
				return 0;
			if (T(u & OwnLine(me, 7)) && !IsPromotion(move))
				return 0;
			return 1;
		}
		else if (to == (from + 2 * Push[me]))
		{
			if (capture)
				return 0;
			if (PieceAt(to - Push[me]))
				return 0;
			if (F(u & OwnLine(me, 3)))
				return 0;
			return 1;
		}
		else if (u & RO->PAtt[me][from])
		{
			if (capture == 0)
				return 0;
			if (T(u & OwnLine(me, 7)) && !IsPromotion(move))
				return 0;
			return 1;
		}
		else
			return 0;
	}
	else if (piece == IKing[me])
	{
		if (me == White)
		{
			if (IsCastling(piece, move))
			{
				if (u & 0x40)
				{
					if (F(Current->castle_flags & CanCastle_OO))
						return 0;
					if (occ & 0x60)
						return 0;
					if (Current->att[Black] & 0x70)
						return 0;
				}
				else
				{
					if (F(Current->castle_flags & CanCastle_OOO))
						return 0;
					if (occ & 0xE)
						return 0;
					if (Current->att[Black] & 0x1C)
						return 0;
				}
				return 1;
			}
		}
		else
		{
			if (IsCastling(piece, move))
			{
				if (u & 0x4000000000000000)
				{
					if (F(Current->castle_flags & CanCastle_oo))
						return 0;
					if (occ & 0x6000000000000000)
						return 0;
					if (Current->att[White] & 0x7000000000000000)
						return 0;
				}
				else
				{
					if (F(Current->castle_flags & CanCastle_ooo))
						return 0;
					if (occ & 0x0E00000000000000)
						return 0;
					if (Current->att[White] & 0x1C00000000000000)
						return 0;
				}
				return 1;
			}
		}
		if (F(KAtt[from] & u))
			return 0;
		if (Current->att[opp] & u)
			return 0;
		return 1;
	}
	piece = (piece >> 1) - 2;
	if (piece == 0)
	{
		if (u & NAtt[from])
			return 1;
		else
			return 0;
	}
	else
	{
		if (piece <= 2)
		{
			if (BMask[from] & u)
				return 1;
		}
		else if (piece == 3)
		{
			if (RMask[from] & u)
				return 1;
		}
		else
			return 1;
		return 0;
	}
}
INLINE bool is_legal(bool me, int move)
{
	return me ? is_legal<1>(move) : is_legal<0>(move);
}

template<bool me> bool is_check(int move)
{  // doesn't detect castling and ep checks
	uint64 king;
	int from, to, piece, king_sq;

	from = From(move);
	to = To(move);
	king = King(opp);
	king_sq = lsb(king);
	piece = PieceAt(from);
	if (HasBit(Current->xray[me], from) && !HasBit(RO->FullLine[king_sq][from], to))
		return true;
	if (piece < WhiteKnight)
	{
		if (RO->PAtt[me][to] & king)
			return true;
		if (HasBit(OwnLine(me, 7), to) && T(king & OwnLine(me, 7)) && F(RO->Between[to][king_sq] & PieceAll()))
			return true;
	}
	else if (piece < WhiteLight)
	{
		if (NAtt[to] & king)
			return true;
	}
	else if (piece < WhiteRook)
	{
		if (BMask[to] & king)
			if (F(RO->Between[king_sq][to] & PieceAll()))
				return true;
	}
	else if (piece < WhiteQueen)
	{
		if (RMask[to] & king)
			if (F(RO->Between[king_sq][to] & PieceAll()))
				return true;
	}
	else if (piece < WhiteKing)
	{
		if (QMask[to] & king)
			if (F(RO->Between[king_sq][to] & PieceAll()))
				return true;
	}
	return false;
}

void pick_pv(int pvPtr, int pvLen)
{
	GEntry *Entry;
	GPVEntry *PVEntry;
	int i, depth, move;
	if (pvPtr >= Min(pvLen, MAX_PV_LEN))
	{
		INFO->PV[pvPtr] = 0;
		return;
	}
	move = 0;
	depth = -256;
	if ((Entry = probe_hash()) && T(Entry->move) && Entry->low_depth > depth)
	{
		depth = Entry->low_depth;
		move = Entry->move;
	}
	if ((PVEntry = probe_pv_hash()) && T(PVEntry->move) && PVEntry->depth > depth)
	{
		depth = PVEntry->depth;
		move = PVEntry->move;
	}
	evaluate();
	if (Current->att[Current->turn] & King(Current->turn ^ 1))
		INFO->PV[pvPtr] = 0;
	else if (move && is_legal(T(Current->turn), move))
	{
		INFO->PV[pvPtr] = move;
		pvPtr++;
		if (Current->turn) do_move<1>(move); else do_move<0>(move);
		if (Current->ply >= 100) goto finish;
		for (i = 4; i <= Current->ply; i += 2)
		{
			if (Stack[sp - i] == Current->key)
			{
				INFO->PV[pvPtr] = 0;
				goto finish;
			}
		}
		pick_pv(pvPtr, pvLen);
	finish:
		if (Current->turn ^ 1) undo_move<1>(move); else undo_move<0>(move);
	}
	else
		INFO->PV[pvPtr] = 0;
}

template<bool me> bool draw_in_pv()
{
	if ((Current - Data) >= 126)
		return true;
	if (Current->ply >= 100)
		return true;
	for (int i = 4; i <= Current->ply; i += 2)
		if (Stack[sp - i] == Current->key)
			return true;
	if (GPVEntry* PVEntry = probe_pv_hash())
	{
		if (!PVEntry->value)
			return true;
		if (int move = PVEntry->move)
		{
			do_move<me>(move);
			bool value = draw_in_pv<opp>();
			undo_move<me>(move);
			return value;
		}
	}
	return false;
}

INLINE int HashMerit(int date, uint8 depth)
{
	return 8 * date + depth;
}
inline int HashMerit(const GEntry& entry)
{
	return HashMerit(entry.date, Max<int>(entry.high_depth, entry.low_depth));
}
INLINE int HashMerit(const GPVEntry& entry)
{
	return HashMerit(entry.date, entry.depth);
}

void hash_high(int value, int depth)
{
	int i;
	GEntry* best, *Entry;

	// search for an old entry to overwrite
	int minMerit = 0x70000000;
	for (i = 0, best = Entry = HASH + (High32(Current->key) & SETTINGS->hashMask); i < HASH_CLUSTER; ++i, ++Entry)
	{
		if (Entry->key == Low32(Current->key))
		{
			Entry->date = SHARED->date;
			if (depth > Entry->high_depth || (depth == Entry->high_depth && value < Entry->high))
			{
				if (Entry->low <= value)
				{
					Entry->high_depth = depth;
					Entry->high = value;
				}
				else if (Entry->low_depth < depth)
				{
					Entry->high_depth = depth;
					Entry->high = value;
					Entry->low = value;
				}
			}
			return;
		}
		int merit = HashMerit(*Entry);
		if (merit < minMerit)
		{
			minMerit = merit;
			best = Entry;
		}
	}
	best->date = SHARED->date;
	best->key = Low32(Current->key);
	best->high = value;
	best->high_depth = depth;
	best->low = 0;
	best->low_depth = 0;
	best->move = 0;
	return;
}

// POSTPONED -- can hash_low return something better than its input?
int hash_low(int move, int value, int depth)
{
	int i;
	GEntry* best, *Entry;

	int min_merit = 0x70000000;
	move &= 0xFFFF;
	for (i = 0, best = Entry = HASH + (High32(Current->key) & SETTINGS->hashMask); i < HASH_CLUSTER; ++i, ++Entry)
	{
		if (Entry->key == Low32(Current->key))
		{
			Entry->date = SHARED->date;
			if (depth > Entry->low_depth || (depth == Entry->low_depth && value > Entry->low))
			{
				if (move)
					Entry->move = move;
				if (Entry->high >= value)
				{
					Entry->low_depth = depth;
					Entry->low = value;
				}
				else if (Entry->high_depth < depth)
				{
					Entry->low_depth = depth;
					Entry->low = value;
					Entry->high = value;
				}
			}
			else if (F(Entry->move))
				Entry->move = move;
			return value;
		}
		int merit = HashMerit(*Entry);
		if (merit < min_merit)
		{
			min_merit = merit;
			best = Entry;
		}
	}
	best->date = SHARED->date;
	best->key = Low32(Current->key);
	best->high = 0;
	best->high_depth = 0;
	best->low = value;
	best->low_depth = depth;
	best->move = move;
	return value;
}

void hash_exact(int move, int value, int depth, int exclusion, int ex_depth, int knodes)
{
	GPVEntry* best;
	GPVEntry* PVEntry;
	int i;

	int minMerit = 0x70000000;
	for (i = 0, best = PVEntry = PVHASH + (High32(Current->key) & PV_HASH_MASK); i < PV_CLUSTER; ++i, ++PVEntry)
	{
		if (PVEntry->key == Low32(Current->key))
		{
			PVEntry->date = SHARED->date;
			PVEntry->knodes += knodes;
			if (PVEntry->depth <= depth)
			{
				PVEntry->value = value;
				PVEntry->depth = depth;
				PVEntry->move = move;
				PVEntry->ply = Current->ply;
				if (ex_depth)
				{
					PVEntry->exclusion = exclusion;
					PVEntry->ex_depth = ex_depth;
				}
			}
			return;
		}
		int merit = HashMerit(*PVEntry);
		if (merit < minMerit)
		{
			minMerit = merit;
			best = PVEntry;
		}
	}
	best->key = Low32(Current->key);
	best->date = SHARED->date;
	best->value = value;
	best->depth = depth;
	best->move = move;
	best->exclusion = exclusion;
	best->ex_depth = ex_depth;
	best->knodes = knodes;
	best->ply = Current->ply;
}

template<bool me> int extension(int pv, int move, int depth, bool* check = nullptr)
{
	if (is_check<me>(move))
	{
		constexpr array<array<int, 2>, 2> DepthCut = { { { { 16, 16 } },{ { 50, 50 } } } };
		if (check) *check = true;
		const bool isQ = (PieceAt(From(move)) & ~Black) == WhiteQueen;
		return see<me>(move, -SeeThreshold, SeeValue) + T(depth < DepthCut[pv][T(isQ)]);
	}
	if (check) *check = false;
	int from = From(move);
	if (HasBit(Current->passer, from) && OwnRank(T(Current->turn), from) >= 5)
	{
		if (depth < 16)
			return 1 + pv;
	}

	if (depth > 2)
		if (uint64 om = Major(opp))
			if (T(King(me) & 0XFFC300000000C3FF) && depth < (Multiple(om) ? 12 : 8))
			{
				int kf = FileOf(lsb(King(me)));
				if (T((PAtts<me>(Pawn(me) & File[kf]) & Piece(opp)) | (PAtts<opp>(Pawn(opp) & File[kf]) & Piece(me))))
					return 1;
			}

	return 0;
}

void sort(int* start, int* finish)
{
	for (int* p = start; p < finish - 1; ++p)
	{
		int* best = p;
		int value = *p;
		int previous = *p;
		for (int* q = p + 1; q < finish; ++q)
			if ((*q) > value)
			{
				value = *q;
				best = q;
			}
		*best = previous;
		*p = value;
	}
}

template<class I> void sort_moves(I start, I finish)
{
	for (I p = start + 1; p < finish; ++p)
		for (I q = p - 1; q >= start; --q)
			if (((*q) >> 16) < ((*(q + 1)) >> 16))
				swap(*q, *(q + 1));
}

INLINE int pick_move()
{
	register int move = *(Current->current);
	if (F(move))
		return 0;
	register int* best = Current->current;
	for (register int* p = Current->current + 1; T(*p); ++p)
	{
		if ((*p) > move)
		{
			best = p;
			move = *p;
		}
	}
	*best = *(Current->current);
	*(Current->current) = move;
	Current->current++;
	return move & 0xFFFF;
}

INLINE void apply_wobble(int* move, int depth)
{
	int mp = (((*move & 0xFFFF) * 529) >> 9) & 1;
	*move += (mp + depth + INFO->id) % (SETTINGS->wobble + 1);	// (minimal) bonus for right parity
}

INLINE bool is_killer(uint16 move)
{
	for (int ik = 1; ik <= N_KILLER; ++ik)
		if (move == Current->killer[ik])
			return true;
	return false;
}

template<bool me> void gen_next_moves(int depth)
{
	int* p, *q, *r;
	Current->gen_flags &= ~FlagSort;
	switch (Current->stage)
	{
	case s_hash_move:
	case r_hash_move:
	case e_hash_move:
		Current->moves[0] = Current->killer[0];
		Current->moves[1] = 0;
		return;
	case s_good_cap:
	{
		Current->mask = Piece(opp);
		r = gen_captures<me>(Current->moves);
		for (q = r - 1, p = Current->moves; q >= p;)
		{
			int move = (*q) & 0xFFFF;
			if (!see<me>(move, 0, SeeValue))
			{
				int next = *p;
				*p = *q;
				*q = next;
				++p;
			}
			else
				--q;
		}
		Current->start = p;
		Current->current = p;
		sort(p, r);
	}
	return;
	case s_special:
		Current->current = Current->start;
		p = Current->start;
		for (int ik = 1; ik <= N_KILLER; ++ik)
			if (T(Current->killer[ik]))
				*p++ = Current->killer[ik];
		if (Current->ref[0] && !is_killer(Current->ref[0]))
			*p++ = Current->ref[0];
		if (Current->ref[1] && !is_killer(Current->ref[1]))
			*p++ = Current->ref[1];
		*p = 0;
		return;
	case s_quiet:
		p = gen_quiet_moves<me>(Current->start);
		Current->gen_flags |= FlagSort;
		Current->current = Current->start;
		for (q = Current->start; *q; ++q)
			apply_wobble(&*q, depth);
		return;
	case s_bad_cap:
		*(Current->start) = 0;
		Current->current = Current->moves;
		if (!(Current->gen_flags & FlagNoBcSort))
			sort(Current->moves, Current->start);
		return;
	case r_cap:
		r = gen_captures<me>(Current->moves);
		Current->current = Current->moves;
		sort(Current->moves, r);
		return;
	case r_checks:
		r = gen_checks<me>(Current->moves);
		Current->current = Current->moves;
		sort(Current->moves, r);
		return;
	case e_ev:
		Current->mask = Filled;
		r = gen_evasions<me>(Current->moves);
		mark_evasions(Current->moves);
		sort(Current->moves, r);
		Current->current = Current->moves;
		return;
	}
}

template<bool me, bool root> int get_move(int depth)
{
	int move;

	if (root)
	{
		move = (*Current->current) & 0xFFFF;
		Current->current++;
		return move;
	}
	for (; ; )
	{
		if (F(*Current->current))
		{
			Current->stage++;
			if ((1 << Current->stage) & StageNone)
				return 0;
			gen_next_moves<me>(depth);
			continue;
		}
		if (Current->gen_flags & FlagSort)
			move = pick_move();
		else
		{
			move = (*Current->current) & 0xFFFF;
			Current->current++;
		}

		if (Current->stage == s_quiet)
		{
			if (!is_killer(move) && move != Current->ref[0] && move != Current->ref[1])
				return move;
		}
		else if (Current->stage == s_special)
		{
			if (!PieceAt(To(move)) && is_legal<me>(move))
				return move;
		}
		else
			return move;
	}
}

template<bool me> bool see(int move, int margin, const uint16* mat_value)
{
	int from, to, piece, capture, delta, sq, pos;
	uint64 clear, def, att, occ, b_area, r_slider_att, b_slider_att, r_slider_def, b_slider_def, r_area, u, new_att, my_bishop, opp_bishop;
	from = From(move);
	to = To(move);
	piece = mat_value[PieceAt(from)];
	capture = mat_value[PieceAt(to)];
	delta = piece - capture;
	if (delta <= -margin)
		return 1;
	if (piece == mat_value[WhiteKing])
		return 1;
	if (HasBit(Current->xray[me], from))
		return 1;
	if (piece > (mat_value[WhiteKing] >> 1))
		return 1;
	if (IsEP(move))
		return 1;
	if (!HasBit(Current->att[opp], to))
		return 1;
	att = RO->PAtt[me][to] & Pawn(opp);
	if (T(att) && delta + margin > mat_value[WhitePawn])
		return 0;
	clear = ~Bit(from);
	def = RO->PAtt[opp][to] & Pawn(me) & clear;
	if (T(def) && delta + mat_value[WhitePawn] + margin <= 0)
		return 1;
	att |= NAtt[to] & Knight(opp);
	if (T(att) && delta > mat_value[WhiteDark] - margin)
		return 0;
	occ = PieceAll() & clear;
	b_area = BishopAttacks(to, occ);
	opp_bishop = Bishop(opp);
	if (delta > mat_value[IDark[me]] - margin)
		if (b_area & opp_bishop)
			return 0;
	my_bishop = Bishop(me);
	b_slider_att = BMask[to] & (opp_bishop | Queen(opp));
	r_slider_att = RMask[to] & Major(opp);
	b_slider_def = BMask[to] & (my_bishop | Queen(me)) & clear;
	r_slider_def = RMask[to] & Major(me) & clear;
	att |= (b_slider_att & b_area);
	def |= NAtt[to] & Knight(me) & clear;
	r_area = RookAttacks(to, occ);
	att |= (r_slider_att & r_area);
	def |= (b_slider_def & b_area);
	def |= (r_slider_def & r_area);
	att |= KAtt[to] & King(opp);
	def |= KAtt[to] & King(me) & clear;
	while (true)
	{
		if (u = (att & Pawn(opp)))
		{
			capture -= piece;
			piece = mat_value[WhitePawn];
			sq = lsb(u);
			occ ^= Bit(sq);
			att ^= Bit(sq);
			for (new_att = RO->FullLine[to][sq] & b_slider_att & occ & (~att); T(new_att); Cut(new_att))
			{
				pos = lsb(new_att);
				if (F(RO->Between[to][pos] & occ))
				{
					att |= Bit(pos);
					break;
				}
			}
		}
		else if (u = (att & Knight(opp)))
		{
			capture -= piece;
			piece = mat_value[WhiteKnight];
			att ^= (~(u - 1)) & u;
		}
		else if (u = (att & opp_bishop))
		{
			capture -= piece;
			piece = mat_value[WhiteDark];
			sq = lsb(u);
			occ ^= Bit(sq);
			att ^= Bit(sq);
			for (new_att = RO->FullLine[to][sq] & b_slider_att & occ & (~att); T(new_att); Cut(new_att))
			{
				pos = lsb(new_att);
				if (F(RO->Between[to][pos] & occ))
				{
					att |= Bit(pos);
					break;
				}
			}
		}
		else if (u = (att & Rook(opp)))
		{
			capture -= piece;
			piece = mat_value[WhiteRook];
			sq = lsb(u);
			occ ^= Bit(sq);
			att ^= Bit(sq);
			for (new_att = RO->FullLine[to][sq] & r_slider_att & occ & (~att); T(new_att); Cut(new_att))
			{
				pos = lsb(new_att);
				if (F(RO->Between[to][pos] & occ))
				{
					att |= Bit(pos);
					break;
				}
			}
		}
		else if (u = (att & Queen(opp)))
		{
			capture -= piece;
			piece = mat_value[WhiteQueen];
			sq = lsb(u);
			occ ^= Bit(sq);
			att ^= Bit(sq);
			for (new_att = RO->FullLine[to][sq] & (r_slider_att | b_slider_att) & occ & (~att); T(new_att); Cut(new_att))
			{
				pos = lsb(new_att);
				if (F(RO->Between[to][pos] & occ))
				{
					att |= Bit(pos);
					break;
				}
			}
		}
		else if (u = (att & King(opp)))
		{
			capture -= piece;
			piece = mat_value[WhiteKing];
		}
		else
			return 1;
		if (capture < -(mat_value[WhiteKing] >> 1))
			return 0;
		if (piece + capture < margin)
			return 0;
		if (u = (def & Pawn(me)))
		{
			capture += piece;
			piece = mat_value[WhitePawn];
			sq = lsb(u);
			occ ^= Bit(sq);
			def ^= Bit(sq);
			for (new_att = RO->FullLine[to][sq] & b_slider_def & occ & (~att); T(new_att); Cut(new_att))
			{
				pos = lsb(new_att);
				if (F(RO->Between[to][pos] & occ))
				{
					def |= Bit(pos);
					break;
				}
			}
		}
		else if (u = (def & Knight(me)))
		{
			capture += piece;
			piece = mat_value[WhiteKnight];
			def ^= (~(u - 1)) & u;
		}
		else if (u = (def & my_bishop))
		{
			capture += piece;
			piece = mat_value[WhiteDark];
			sq = lsb(u);
			occ ^= Bit(sq);
			def ^= Bit(sq);
			for (new_att = RO->FullLine[to][sq] & b_slider_def & occ & (~att); T(new_att); Cut(new_att))
			{
				pos = lsb(new_att);
				if (F(RO->Between[to][pos] & occ))
				{
					def |= Bit(pos);
					break;
				}
			}
		}
		else if (u = (def & Rook(me)))
		{
			capture += piece;
			piece = mat_value[WhiteRook];
			sq = lsb(u);
			occ ^= Bit(sq);
			def ^= Bit(sq);
			for (new_att = RO->FullLine[to][sq] & r_slider_def & occ & (~att); T(new_att); Cut(new_att))
			{
				pos = lsb(new_att);
				if (F(RO->Between[to][pos] & occ))
				{
					def |= Bit(pos);
					break;
				}
			}
		}
		else if (u = (def & Queen(me)))
		{
			capture += piece;
			piece = mat_value[WhiteQueen];
			sq = lsb(u);
			occ ^= Bit(sq);
			def ^= Bit(sq);
			for (new_att = RO->FullLine[to][sq] & (r_slider_def | b_slider_def) & occ & (~att); T(new_att); Cut(new_att))
			{
				pos = lsb(new_att);
				if (F(RO->Between[to][pos] & occ))
				{
					def |= Bit(pos);
					break;
				}
			}
		}
		else if (u = (def & King(me)))
		{
			capture += piece;
			piece = mat_value[WhiteKing];
		}
		else
			return 0;
		if (capture > (mat_value[WhiteKing] >> 1))
			return 1;
		if (capture - piece >= margin)
			return 1;
	}
}

template<bool me> void gen_root_moves()
{
	int *p, depth = -256;

	int killer = 0;
	if (GEntry* Entry = probe_hash())
	{
		if (T(Entry->move) && Entry->low_depth > depth)
		{
			depth = Entry->low_depth;
			killer = Entry->move;
		}
	}
	if (GPVEntry* PVEntry = probe_pv_hash())
	{
		if (PVEntry->depth > depth && T(PVEntry->move))
		{
			depth = PVEntry->depth;
			killer = PVEntry->move;
		}
	}

	Current->killer[0] = killer;
	if (IsCheck(me))
		Current->stage = stage_evasion;
	else
	{
		Current->stage = stage_search;
		Current->ref[0] = RefM(Current->move).ref[0];
		Current->ref[1] = RefM(Current->move).ref[1];
	}
	Current->gen_flags = 0;
	p = &RootList[0];
	Current->current = Current->moves;
	Current->moves[0] = 0;
	while (int move = get_move<me, 0>(0))
	{
		if (IsIllegal(me, move))
			continue;
		if (p > &RootList[0] && move == killer)
			continue;
		if (SearchMoves)
		{
			auto stop = &SMoves[SMPointer];
			if (find(&SMoves[0], stop, move) == stop)
				continue;
		}
		*p = move;
		++p;
	}
	*p = 0;
}

template<bool me> INLINE bool forkable(int dst)
{
	if (NAttAtt[dst] & King(me))
	{
		for (uint64 nn = Knight(opp) & NAttAtt[dst]; nn; Cut(nn))
		{
			if (T(NAtt[dst] & NAtt[lsb(nn)] & NAtt[lsb(King(me))]))
				return true;
		}
	}
	return false;
}

template<bool me> int* gen_captures(int* list)
{
	static const int MvvLvaPromotion = RO->MvvLva[WhiteQueen][BlackQueen];
	static const int MvvLvaPromotionKnight = RO->MvvLva[WhiteKnight][BlackKnight];
	static const int MvvLvaPromotionBad = RO->MvvLva[WhiteKnight][BlackPawn];

	uint64 u, v;

	if (Current->ep_square)
		for (v = RO->PAtt[opp][Current->ep_square] & Pawn(me); T(v); Cut(v))
			list = AddMove(list, lsb(v), Current->ep_square, FlagEP, RO->MvvLva[IPawn[me]][IPawn[opp]]);
	for (u = Pawn(me) & OwnLine(me, 6); T(u); Cut(u))
	{
		int from = lsb(u), to = from + Push[me];
		if (F(PieceAt(to)))
		{
			list = AddMove(list, from, to, FlagPQueen, forkable<me>(to) ? MvvLvaPromotion : MvvLvaPromotionKnight);
			list = AddMove(list, from, to, FlagPKnight, T(NAtt[to] & King(opp)) ? MvvLvaPromotionKnight : MvvLvaPromotionBad);
			list = AddMove(list, from, to, FlagPRook, MvvLvaPromotionBad);
			list = AddMove(list, from, to, FlagPBishop, MvvLvaPromotionBad);
		}
	}
	for (v = ShiftW<opp>(Current->mask) & Pawn(me) & OwnLine(me, 6); T(v); Cut(v))
	{
		list = AddMove(list, lsb(v), lsb(v) + PushE[me], FlagPQueen, MvvLvaPromotionCap(PieceAt(lsb(v) + PushE[me])));
		if (HasBit(NAtt[lsb(King(opp))], lsb(v) + PushE[me]))
			list = AddMove(list, lsb(v), lsb(v) + PushE[me], FlagPKnight, MvvLvaPromotionKnightCap(PieceAt(lsb(v) + PushE[me])));
	}
	for (v = ShiftE<opp>(Current->mask) & Pawn(me) & OwnLine(me, 6); T(v); Cut(v))
	{
		list = AddMove(list, lsb(v), lsb(v) + PushW[me], FlagPQueen, MvvLvaPromotionCap(PieceAt(lsb(v) + PushW[me])));
		if (HasBit(NAtt[lsb(King(opp))], lsb(v) + PushW[me]))
			list = AddMove(list, lsb(v), lsb(v) + PushW[me], FlagPKnight, MvvLvaPromotionKnightCap(PieceAt(lsb(v) + PushW[me])));
	}
	if (T(Current->att[me] & Current->mask))
	{
		for (v = ShiftW<opp>(Current->mask) & Pawn(me) & (~OwnLine(me, 6)); T(v); Cut(v))
			list = AddCaptureP(list, IPawn[me], lsb(v), lsb(v) + PushE[me], 0);
		for (v = ShiftE<opp>(Current->mask) & Pawn(me) & (~OwnLine(me, 6)); T(v); Cut(v))
			list = AddCaptureP(list, IPawn[me], lsb(v), lsb(v) + PushW[me], 0);
		for (v = KAtt[lsb(King(me))] & Current->mask & (~Current->att[opp]); T(v); Cut(v))
			list = AddCaptureP(list, IKing[me], lsb(King(me)), lsb(v), 0);
		for (u = Knight(me); T(u); Cut(u))
			for (v = NAtt[lsb(u)] & Current->mask; T(v); Cut(v))
				list = AddCaptureP(list, IKnight[me], lsb(u), lsb(v), 0);
		for (u = Bishop(me); T(u); Cut(u))
			for (v = BishopAttacks(lsb(u), PieceAll()) & Current->mask; T(v); Cut(v))
				list = AddCapture(list, lsb(u), lsb(v), 0);
		for (u = Rook(me); T(u); Cut(u))
			for (v = RookAttacks(lsb(u), PieceAll()) & Current->mask; T(v); Cut(v))
				list = AddCaptureP(list, IRook[me], lsb(u), lsb(v), 0);
		for (u = Queen(me); T(u); Cut(u))
			for (v = QueenAttacks(lsb(u), PieceAll()) & Current->mask; T(v); Cut(v))
				list = AddCaptureP(list, IQueen[me], lsb(u), lsb(v), 0);
	}
	return NullTerminate(list);
}

template<class T_> T_* NullTerminate(T_* list)
{
	*list = 0;
	return list;
}

template<bool me> int* gen_evasions(int* list)
{
	static const int MvvLvaPromotion = RO->MvvLva[WhiteQueen][BlackQueen];

	uint64 b, u;
	//	pair<uint64, uint64> pJoins = pawn_joins(me, Pawn(me));

	int king = lsb(King(me));
	uint64 att = (NAtt[king] & Knight(opp)) | (RO->PAtt[me][king] & Pawn(opp));
	for (u = (BMask[king] & (Bishop(opp) | Queen(opp))) | (RMask[king] & (Rook(opp) | Queen(opp))); T(u); u ^= b)
	{
		b = Bit(lsb(u));
		if (F(RO->Between[king][lsb(u)] & PieceAll()))
			att |= b;
	}
	int att_sq = lsb(att);  // generally the only attacker
	uint64 esc = KAtt[king] & (~(Piece(me) | Current->att[opp])) & Current->mask;
	if (PieceAt(att_sq) >= WhiteLight)
		esc &= ~RO->FullLine[king][att_sq];
	else if (PieceAt(att_sq) >= WhiteKnight)
		esc &= ~NAtt[att_sq];

	Cut(att);
	if (att)
	{  // second attacker (double check)
		att_sq = lsb(att);
		if (PieceAt(att_sq) >= WhiteLight)
			esc &= ~RO->FullLine[king][att_sq];
		else if (PieceAt(att_sq) >= WhiteKnight)
			esc &= ~NAtt[att_sq];

		for (; T(esc); Cut(esc))
			list = AddCaptureP(list, IKing[me], king, lsb(esc), 0);
		return NullTerminate(list);
	}

	// not double check
	if (T(Current->ep_square) && Current->ep_square == att_sq + Push[me] && HasBit(Current->mask, att_sq))
	{
		for (u = RO->PAtt[opp][Current->ep_square] & Pawn(me); T(u); Cut(u))
			list = AddMove(list, lsb(u), att_sq + Push[me], FlagEP, RO->MvvLva[IPawn[me]][IPawn[opp]]);
	}
	for (u = RO->PAtt[opp][att_sq] & Pawn(me); T(u); Cut(u))
	{
		int from = lsb(u);
		if (HasBit(OwnLine(me, 7), att_sq))
			list = AddMove(list, from, att_sq, FlagPQueen, MvvLvaPromotionCap(PieceAt(att_sq)));
		else if (HasBit(Current->mask, att_sq))
			list = AddCaptureP(list, IPawn[me], from, att_sq, 0);
	}
	for (; T(esc); Cut(esc))
		list = AddCaptureP(list, IKing[me], king, lsb(esc), 0);
	// now check interpositions
	uint64 inter = RO->Between[king][att_sq];
	for (u = Shift<opp>(inter) & Pawn(me); T(u); Cut(u))
	{
		int from = lsb(u);
		if (HasBit(OwnLine(me, 6), from))
			list = AddMove(list, from, from + Push[me], FlagPQueen, MvvLvaPromotion);
		else if (F(~Current->mask))
			list = AddMove(list, from, from + Push[me], 0, 0);
	}

	if (F(Current->mask))
		return NullTerminate(list);

	if (F(~Current->mask))
	{
		for (u = Shift<opp>(Shift<opp>(inter)) & OwnLine(me, 1) & Pawn(me); T(u); Cut(u))
		{
			int from = lsb(u);
			if (F(PieceAt(from + Push[me])))
			{
				int to = from + 2 * Push[me];
				list = AddMove(list, from, to, 0, 0);
			}
		}
	}
	inter = (inter | Bit(att_sq)) & Current->mask;
	for (u = Knight(me); T(u); Cut(u))
		for (esc = NAtt[lsb(u)] & inter; T(esc); Cut(esc))
			list = AddCaptureP(list, IKnight[me], lsb(u), lsb(esc), 0);
	for (u = Bishop(me); T(u); Cut(u))
		for (esc = BishopAttacks(lsb(u), PieceAll()) & inter; T(esc); Cut(esc))
			list = AddCapture(list, lsb(u), lsb(esc), 0);
	for (u = Rook(me); T(u); Cut(u))
		for (esc = RookAttacks(lsb(u), PieceAll()) & inter; T(esc); Cut(esc))
			list = AddCaptureP(list, IRook[me], lsb(u), lsb(esc), 0);
	for (u = Queen(me); T(u); Cut(u))
		for (esc = QueenAttacks(lsb(u), PieceAll()) & inter; T(esc); Cut(esc))
			list = AddCaptureP(list, IQueen[me], lsb(u), lsb(esc), 0);
	return NullTerminate(list);
}

void mark_evasions(int* list)
{
	for (; T(*list); ++list)
	{
		register int move = (*list) & 0xFFFF;
		if (F(PieceAt(To(move))) && F(move & 0xE000))
		{
			if (move == Current->ref[0])
				*list |= RefOneScore;
			else if (move == Current->ref[1])
				*list |= RefTwoScore;
			else if (find(Current->killer.begin() + 1, Current->killer.end(), move) != Current->killer.end())
			{
				int ik = static_cast<int>(find(Current->killer.begin() + 1, Current->killer.end(), move) - Current->killer.begin());
				*list |= (0xFF >> Max(0, ik - 2)) << 16;
				if (ik == 1)
					*list |= 1 << 24;
			}
			else
				*list |= HistoryP(JoinFlag(move), PieceAt(From(move)), From(move), To(move));
		}
	}
}

template<bool me> INLINE uint64 PawnJoins()
{
	auto threat = Current->att[opp] & Current->passer;
	return Shift<me>(Current->passer) | ShiftW<opp>(threat) | ShiftE<opp>(threat);
}

INLINE bool can_castle(const uint64& occ, bool me, bool kingside)
{
	if (me == White)
	{
		return kingside
			? T(Current->castle_flags & CanCastle_OO) && F(occ & 0x60) && F(Current->att[Black] & 0x70)
			: T(Current->castle_flags & CanCastle_OOO) && F(occ & 0xE) && F(Current->att[Black] & 0x1C);
	}
	else
	{
		return kingside
			? T(Current->castle_flags & CanCastle_oo) && F(occ & 0x6000000000000000) && F(Current->att[White] & 0x7000000000000000)
			: T(Current->castle_flags & CanCastle_ooo) && F(occ & 0x0E00000000000000) && F(Current->att[White] & 0x1C00000000000000);
	}
}
template<bool me> int* gen_quiet_moves(int* list)
{
	uint64 u, v;
	auto safe3 = [&](int loc) { return HasBit(Current->att[opp] & ~Current->att[me], loc) ? 0 : FlagCastling; };

	uint64 occ = PieceAll();
	if (me == White)
	{
		if (can_castle(occ, me, true))
			list = AddHistoryP(list, IKing[White], 4, 6, FlagCastling);
		if (can_castle(occ, me, false))
			list = AddHistoryP(list, IKing[White], 4, 2, FlagCastling);
	}
	else
	{
		if (can_castle(occ, me, true))
			list = AddHistoryP(list, IKing[Black], 60, 62, FlagCastling);
		if (can_castle(occ, me, false))
			list = AddHistoryP(list, IKing[Black], 60, 58, FlagCastling);
	}

	uint64 free = ~occ;
	auto pTarget = PawnJoins<me>();
	auto pFlag = [&](int to) {return HasBit(pTarget, to) ? FlagCastling : 0; };
	for (v = Shift<me>(Pawn(me)) & free & (~OwnLine(me, 7)); T(v); Cut(v))
	{
		int to = lsb(v);
		if (HasBit(OwnLine(me, 2), to) && F(PieceAt(to + Push[me])))
			list = AddHistoryP(list, IPawn[me], to - Push[me], to + Push[me], pFlag(to + Push[me]));
		list = AddHistoryP(list, IPawn[me], to - Push[me], to, pFlag(to));
	}

	for (u = Knight(me); T(u); Cut(u))
	{
		int from = lsb(u);
		for (v = free & NAtt[from]; T(v); Cut(v))
		{
			int to = lsb(v);
			int flag = NAtt[to] & Major(opp) ? FlagCastling : 0;
			list = AddHistoryP(list, IKnight[me], from, to, flag);
		}
	}

	for (u = Bishop(me); T(u); Cut(u))
	{
		int from = lsb(u);
		int which = HasBit(LightArea, from) ? ILight[me] : IDark[me];
		for (v = free & BishopAttacks(from, occ); T(v); Cut(v))
		{
			int to = lsb(v);
			int flag = BMask[to] & (RO->PAtt[opp][to] & Pawn(me) ? Major(opp) : Rook(opp)) ? FlagCastling : 0;
			list = AddHistoryP(list, which, from, to, flag);
		}
	}

	for (u = Rook(me); T(u); Cut(u))
	{
		int from = lsb(u);
		for (v = free & RookAttacks(from, occ); T(v); Cut(v))
		{
			int to = lsb(v);
			int flag = (RO->PAtt[opp][to] & Pawn(me)) && (RMask[to] & Queen(opp)) ? FlagCastling : 0;
			list = AddHistoryP(list, IRook[me], from, to, flag);
		}
	}
	for (u = Queen(me); T(u); Cut(u))
	{
		int from = lsb(u);
		for (v = free & QueenAttacks(from, occ); T(v); Cut(v))
		{
			int to = lsb(v);
			//int flag = Pst(IQueen[me], to) > Pst(IQueen[me], from) ? FlagCastling : 0;
			list = AddHistoryP(list, IQueen[me], from, to, 0);
		}
	}
	for (v = KAtt[lsb(King(me))] & free & (~Current->att[opp]); T(v); Cut(v))
		list = AddHistoryP(list, IKing[me], lsb(King(me)), lsb(v), 0);

	return NullTerminate(list);
}

template<bool me> int* gen_checks(int* list)
{
	static const int MvvLvaXray = RO->MvvLva[WhiteQueen][WhitePawn];

	int king;
	uint64 u, v, target, b_target, r_target, clear;

	clear = ~(Piece(me) | Current->mask);
	king = lsb(King(opp));
	// discovered checks
	for (u = Current->xray[me] & Piece(me); T(u); Cut(u))
	{
		int from = lsb(u);
		target = clear & (~RO->FullLine[king][from]);
		if (PieceAt(from) == IPawn[me])
		{
			if (!HasBit(OwnLine(me, 7), from + Push[me]))
			{
				if (HasBit(target, from + Push[me]) && F(PieceAt(from + Push[me])))
				{
					list = AddMove(list, from, from + Push[me], 0, MvvLvaXray);
					// now check double push
					const int to2 = from + 2 * Push[me];
					if (HasBit(OwnLine(me, 1), from) && HasBit(target, to2) && F(PieceAt(to2)))
						list = AddMove(list, from, to2, 0, MvvLvaXray);
				}

				for (v = RO->PAtt[me][from] & target & Piece(opp); T(v); Cut(v))
					list = AddMove(list, from, lsb(v), 0, MvvLvaXrayCap(PieceAt(lsb(v))));
			}
		}
		else
		{
			if (PieceAt(from) < WhiteLight)
				v = NAtt[from] & target;
			else if (PieceAt(from) < WhiteRook)
				v = BishopAttacks(from, PieceAll()) & target;
			else if (PieceAt(from) < WhiteQueen)
				v = RookAttacks(from, PieceAll()) & target;
			else if (PieceAt(from) < WhiteKing)
				v = QueenAttacks(from, PieceAll()) & target;
			else
				v = KAtt[from] & target & (~Current->att[opp]);
			for (; T(v); Cut(v))
				list = AddMove(list, from, lsb(v), 0, MvvLvaXrayCap(PieceAt(lsb(v))));
		}
	}

	const uint64 nonDiscover = ~(Current->xray[me] & Piece(me));  // exclude pieces already checked
	for (u = Knight(me) & NAttAtt[king] & nonDiscover; T(u); Cut(u))
		for (v = NAtt[king] & NAtt[lsb(u)] & clear; T(v); Cut(v))
			list = AddCaptureP(list, IKnight[me], lsb(u), lsb(v), 0);

	for (u = KAttAtt[king] & Pawn(me) & (~OwnLine(me, 6)) & nonDiscover; T(u); Cut(u))
	{
		int from = lsb(u);
		for (v = RO->PAtt[me][from] & RO->PAtt[opp][king] & clear & Piece(opp); T(v); Cut(v))
			list = AddCaptureP(list, IPawn[me], from, lsb(v), 0);
		if (F(PieceAt(from + Push[me])) && HasBit(RO->PAtt[opp][king], from + Push[me]))
			list = AddMove(list, from, from + Push[me], 0, 0);
	}

	b_target = BishopAttacks(king, PieceAll()) & clear;
	r_target = RookAttacks(king, PieceAll()) & clear;
	for (u = Board->bb[(T(King(opp) & LightArea) ? WhiteLight : WhiteDark) | me] & nonDiscover; T(u); Cut(u))
		for (v = BishopAttacks(lsb(u), PieceAll()) & b_target; T(v); Cut(v))
			list = AddCapture(list, lsb(u), lsb(v), 0);
	for (u = Rook(me) & nonDiscover; T(u); Cut(u))
		for (v = RookAttacks(lsb(u), PieceAll()) & r_target; T(v); Cut(v))
			list = AddCaptureP(list, IRook[me], lsb(u), lsb(v), 0);
	for (u = Queen(me) & nonDiscover; T(u); Cut(u))
	{
		uint64 contact = KAtt[king];
		int from = lsb(u);
		for (v = QueenAttacks(from, PieceAll()) & (b_target | r_target); T(v); Cut(v))
		{
			int to = lsb(v);
			if (HasBit(contact, to))
				list = AddCaptureP(list, IQueen[me], from, to, 0, T(Boundary & King(opp)) || OwnRank<me>(to) == 7 ? IPawn[opp] : IRook[opp]);
			else
				list = AddCaptureP(list, IQueen[me], from, to, 0);
		}
	}

	if (OwnRank<me>(king) == 4)
	{	  // check for double-push checks	
		for (u = Pawn(me) & OwnLine(me, 1) & nonDiscover & RO->PAtt[opp][king - 2 * Push[me]]; T(u); Cut(u))
		{
			int from = lsb(u);
			int to = from + 2 * Push[me];
			if (F(PieceAt(from + Push[me])) && F(PieceAt(to)))
				list = AddMove(list, from, to, 0, 0);
		}
	}
	return NullTerminate(list);
}

template<bool me> int* gen_delta_moves(int margin, int* list)
{
	uint64 occ = PieceAll();
	uint64 free = ~occ;
	if (me == White)
	{
		if (can_castle(occ, me, true))
			list = AddCDeltaP(list, margin, IKing[White], 4, 6, FlagCastling);
		if (can_castle(occ, me, false))
			list = AddCDeltaP(list, margin, IKing[White], 4, 2, FlagCastling);
	}
	else
	{
		if (can_castle(occ, me, true))
			list = AddCDeltaP(list, margin, IKing[Black], 60, 62, FlagCastling);
		if (can_castle(occ, me, false))
			list = AddCDeltaP(list, margin, IKing[Black], 60, 58, FlagCastling);
	}
	for (uint64 v = Shift<me>(Pawn(me)) & free & (~OwnLine(me, 7)); T(v); Cut(v))
	{
		int to = lsb(v);
		if (HasBit(OwnLine(me, 2), to) && F(PieceAt(to + Push[me])))
			list = AddCDeltaP(list, margin, IPawn[me], to - Push[me], to + Push[me], 0);
		list = AddCDeltaP(list, margin, IPawn[me], to - Push[me], to, 0);
	}
	for (uint64 u = Knight(me); T(u); Cut(u))
	{
		int from = lsb(u);
		for (uint64 v = free & NAtt[lsb(u)]; T(v); Cut(v))
			list = AddCDeltaP(list, margin, IKnight[me], from, lsb(v), 0);
	}
	for (uint64 u = Piece(WhiteLight | me); T(u); Cut(u))
	{
		int from = lsb(u);
		for (uint64 v = free & BishopAttacks(lsb(u), occ); T(v); Cut(v))
			list = AddCDeltaP(list, margin, ILight[me], from, lsb(v), 0);
	}
	for (uint64 u = Piece(WhiteDark | me); T(u); Cut(u))
	{
		int from = lsb(u);
		for (uint64 v = free & BishopAttacks(lsb(u), occ); T(v); Cut(v))
			list = AddCDeltaP(list, margin, IDark[me], from, lsb(v), 0);
	}
	for (uint64 u = Rook(me); T(u); Cut(u))
	{
		int from = lsb(u);
		for (uint64 v = free & RookAttacks(lsb(u), occ); T(v); Cut(v))
			list = AddCDeltaP(list, margin, IRook[me], from, lsb(v), 0);
	}
	for (uint64 u = Queen(me); T(u); Cut(u))
	{
		int from = lsb(u);
		for (uint64 v = free & QueenAttacks(lsb(u), occ); T(v); Cut(v))
			list = AddCDeltaP(list, margin, IQueen[me], from, lsb(v), 0);
	}
	int from = lsb(King(me));
	for (uint64 v = KAtt[lsb(King(me))] & free & (~Current->att[opp]); T(v); Cut(v))
		list = AddCDeltaP(list, margin, IKing[me], from, lsb(v), 0);
	return NullTerminate(list);
}

template<bool me> int SingularExtension(int ext, int prev_ext, int margin_one, int margin_two, int depth, int killer)
{
	int value = -MateValue;
	int singular = 0;
	if (ext < (prev_ext ? 1 : 2))
	{
		value = IsCheck(me) ? scout_evasion<me, 1>(margin_one, depth, killer) : scout<me, 1>(margin_one, depth, killer);
		if (value < margin_one)
			singular = 1;
	}
	if (value < margin_one && ext < (prev_ext ? (prev_ext >= 2 ? 1 : 2) : 3))
	{
		value = IsCheck(me) ? scout_evasion<me, 1>(margin_two, depth, killer) : scout<me, 1>(margin_two, depth, killer);
		if (value < margin_two)
			singular = 2;
	}
	return singular;
}

template<bool me> INLINE uint64 capture_margin_mask(int alpha, int* score)
{
	uint64 retval = Piece(opp);
	if (Current->score + 200 * CP_SEARCH < alpha) {
		if (Current->att[me] & Pawn(opp)) {
			retval ^= Pawn(opp);
			*score = Current->score + 200 * CP_SEARCH;
		}
		if (Current->score + 500 * CP_SEARCH < alpha) {
			if (Current->att[me] & Minor(opp)) {
				retval ^= Minor(opp);
				*score = Current->score + 500 * CP_SEARCH;
			}
			if (Current->score + 700 * CP_SEARCH < alpha) {
				if (Current->att[me] & Rook(opp)) {
					retval ^= Rook(opp);
					*score = Current->score + 700 * CP_SEARCH;
				}
				if (Current->score + 1400 * CP_SEARCH < alpha && (Current->att[me] & Queen(opp))) {
					retval ^= Queen(opp);
					*score = Current->score + 1400 * CP_SEARCH;
				}
			}
		}
	}
	return retval;
}

#define HALT_CHECK {\
    if (Current->ply >= 100) return 0; \
    for (int ihc = 4; ihc <= Current->ply; ihc += 2) if (Stack[sp - ihc] == Current->key) return 0; \
	if ((Current - Data) >= 126) {evaluate(); return Current->score; }}

template<bool me, bool pv> int q_search(int alpha, int beta, int depth, int flags)
{
	int i, value, score, move, hash_move, hash_depth;
	GEntry* Entry;
	auto finish = [&](int score)
	{
		if (depth >= -2 && (depth >= 0 || Current->score + FutilityThreshold >= alpha))
			hash_high(score, 1);
		return score;
	};

	check_for_stop();
	if (flags & FlagHaltCheck)
		HALT_CHECK;
#ifdef CPU_TIMING
#ifndef TIMING
	if (nodes > check_node + 0x4000)
	{
#else
	if (nodes > check_node + 0x100)
	{
#endif
		check_node = nodes;
#ifdef TIMING
		if (LastDepth >= 6)
#endif
			check_time(nullptr, 1);
	}
#endif
	if (flags & FlagCallEvaluation)
		evaluate();
	if (IsCheck(me))
		return q_evasion<me, pv>(alpha, beta, depth, FlagHashCheck);

	int tempo = InitiativeConst;
	if (F(NonPawnKing(me) | (Current->passer & Pawn(me) & Shift<opp>(Current->patt[me] | ~Current->att[opp]))))
		tempo = 0;
	else if (F(Current->material & FlagUnusualMaterial) && Current->material < TotalMat)
		tempo += (InitiativePhase * RO->Material[Current->material].phase) / MAX_PHASE;
	score = Current->score + tempo;
	if (score > alpha)
	{
		if (score >= beta)
			return score;
		alpha = score;
	}

	hash_move = hash_depth = 0;
	if (flags & FlagHashCheck)
	{
		for (i = 0, Entry = HASH + (High32(Current->key) & SETTINGS->hashMask); i < HASH_CLUSTER; ++Entry, ++i)
		{
			if (Low32(Current->key) == Entry->key)
			{
				if (T(Entry->low_depth))
				{
					if (Entry->low >= beta && !pv)
						return Entry->low;
					if (Entry->low_depth > hash_depth && T(Entry->move))
					{
						hash_move = Entry->move;
						hash_depth = Entry->low_depth;
					}
				}
				if (T(Entry->high_depth) && Entry->high <= alpha && !pv)
					return Entry->high;
				break;
			}
		}
	}

	Current->mask = capture_margin_mask<me>(alpha, &score);

	int nTried = 0;
	if (T(hash_move))
	{
		if (HasBit(Current->mask, To(hash_move))
			|| T(hash_move & 0xE000)
			|| (depth >= -8 && (Current->score + DeltaM(hash_move) > alpha || T(is_check<me>(hash_move)))))
		{
			move = hash_move;
			if (is_legal<me>(move) && !IsIllegal(me, move))
			{
				if (SeeValue[PieceAt(To(move))] > SeeValue[PieceAt(From(move))])
					++nTried;
				do_move<me>(move);
				value = -q_search<opp, pv>(-beta, -alpha, depth - 1, FlagNeatSearch);
				undo_move<me>(move);
				if (value > score)
				{
					score = value;
					if (value > alpha)
					{
						if (value >= beta)
							return hash_low(move, score, 1);
						alpha = value;
					}
				}
				if (F(Bit(To(hash_move)) & Current->mask)
					&& F(hash_move & 0xE000)
					&& !pv
					&& alpha >= beta - 1
					&& (depth < -2 || depth <= -1 && Current->score + FutilityThreshold < alpha))
					return alpha;
			}
		}
	}

	// done with hash move
	(void)gen_captures<me>(Current->moves);
	Current->current = Current->moves;
	while (move = pick_move())
	{
		if (move != hash_move && !IsIllegal(me, move) && see<me>(move, -SeeThreshold, SeeValue))
		{
			if (SeeValue[PieceAt(To(move))] > SeeValue[PieceAt(From(move))])
				++nTried;
			do_move<me>(move);
			value = -q_search<opp, pv>(-beta, -alpha, depth - 1, FlagNeatSearch);
			undo_move<me>(move);
			if (value > score)
			{
				score = value;
				if (score > alpha)
				{
					if (score >= beta)
						return hash_low(move, Max(score, beta), 1);
					alpha = score;
				}
			}
		}
	}

	if (depth < -2 || (depth <= -1 && Current->score + FutilityThreshold < alpha))
		return finish(score);
	gen_checks<me>(Current->moves);
	Current->current = Current->moves;
	while (move = pick_move())
	{
		if (move != hash_move && !IsIllegal(me, move) && !IsRepetition(alpha + 1, move) && see<me>(move, -SeeThreshold, SeeValue))
		{
			do_move<me>(move);
			value = -q_evasion<opp, pv>(-beta, -alpha, depth - 1, FlagNeatSearch);
			undo_move<me>(move);
			if (value > score)
			{
				score = value;
				if (score > alpha)
				{
					if (score >= beta)
						return hash_low(move, Max(score, beta), 1);
					alpha = score;
				}
			}
		}
	}

	if (T(nTried) || Current->score + 30 * CP_SEARCH < alpha || T(Current->threat & Piece(me)) || T(Current->xray[opp] & NonPawn(opp)) ||
		T(Pawn(opp) & OwnLine(me, 1) & Shift<me>(~PieceAll())))
		return finish(score);
	int margin = alpha - Current->score + 6 * CP_SEARCH;
	gen_delta_moves<me>(margin, Current->moves);
	Current->current = Current->moves;
	while (move = pick_move())
	{
		if (move != hash_move && !IsIllegal(me, move) && !IsRepetition(alpha + 1, move) && see<me>(move, -SeeThreshold, SeeValue))
		{
			++nTried;
			do_move<me>(move);
			value = -q_search<opp, pv>(-beta, -alpha, depth - 1, FlagNeatSearch);
			undo_move<me>(move);
			if (value > score)
			{
				score = value;
				if (score > alpha)
				{
					if (score >= beta)
					{
						if (N_KILLER >= 1 && Current->killer[1] != move)
						{
							for (int jk = N_KILLER; jk > 1; --jk)
								Current->killer[jk] = Current->killer[jk - 1];
							Current->killer[1] = move;
						}
						return hash_low(move, Max(score, beta), 1);
					}
					alpha = score;
				}
			}
			if (nTried >= 3)
				break;
		}
	}
	return finish(score);
	}

template<bool me, bool pv> int q_evasion(int alpha, int beta, int depth, int flags)
{
	int i, value, pext, score, move, cnt, hash_move, hash_depth;
	int* p;
	GEntry* Entry;

	score = static_cast<int>(Current - Data) - MateValue;
	if (flags & FlagHaltCheck)
		HALT_CHECK;

	hash_move = hash_depth = 0;
	if (flags & FlagHashCheck)
	{
		for (i = 0, Entry = HASH + (High32(Current->key) & SETTINGS->hashMask); i < HASH_CLUSTER; ++Entry, ++i)
		{
			if (Low32(Current->key) == Entry->key)
			{
				if (T(Entry->low_depth))
				{
					if (Entry->low >= beta && !pv)
						return Entry->low;
					if (Entry->low_depth > hash_depth && T(Entry->move))
					{
						hash_move = Entry->move;
						hash_depth = Entry->low_depth;
					}
				}
				if (T(Entry->high_depth) && Entry->high <= alpha && !pv)
					return Entry->high;
				break;
			}
		}
	}

	if (flags & FlagCallEvaluation)
		evaluate();
	Current->mask = Filled;
	if (Current->score - 10 * CP_SEARCH <= alpha && !pv)
	{
		score = Current->score - 10 * CP_SEARCH;
		Current->mask = capture_margin_mask<me>(alpha, &score);
	}

	alpha = Max(score, alpha);
	(void)gen_evasions<me>(Current->moves);
	Current->current = Current->moves;
	if (F(Current->moves[0]))
		return score;
	if (F(Current->moves[1]))
		pext = 1;
	else
	{
		pext = 0;
		Current->ref[0] = RefM(Current->move).check_ref[0];
		Current->ref[1] = RefM(Current->move).check_ref[1];
		mark_evasions(Current->moves);
		if (T(hash_move) && (T(Bit(To(hash_move)) & Current->mask) || T(hash_move & 0xE000)))
		{
			for (p = Current->moves; T(*p); ++p)
			{
				if (((*p) & 0xFFFF) == hash_move)
				{
					*p |= 0x7FFF0000;
					break;
				}
			}
		}
	}
	cnt = 0;
	while (move = pick_move())
	{
		if (IsIllegal(me, move))
			continue;
		++cnt;
		if (IsRepetition(alpha + 1, move))
		{
			score = Max(0, score);
			continue;
		}
		if (F(PieceAt(To(move))) && F(move & 0xE000))
		{
			if (cnt > 3 && F(is_check<me>(move)) && !pv)
				continue;
			value = Current->score + DeltaM(move) + 10 * CP_SEARCH;
			if (value <= alpha && !pv)
			{
				score = Max(value, score);
				continue;
			}
		}
		do_move<me>(move);
		value = -q_search<opp, pv>(-beta, -alpha, depth - 1 + pext, FlagNeatSearch);
		undo_move<me>(move);
		if (value > score)
		{
			score = value;
			if (value > alpha)
			{
				if (value >= beta)
					return score;
				alpha = value;
			}
		}
	}
	return score;
}

template<bool exclusion> int cut_search(int move, int hash_move, int score, int beta, int depth, int flags)
{
	if (exclusion)
		return score;
	Current->best = move;
	if (depth >= 10)
		score = Min(beta, score);
	hash_low(move, score, depth);
	if (F(PieceAt(To(move))) && F(move & 0xE000))
	{
		if (Current->killer[1] != move && F(flags & FlagNoKillerUpdate))
		{
			for (int jk = N_KILLER; jk > 1; --jk)
				Current->killer[jk] = Current->killer[jk - 1];
			Current->killer[1] = move;
		}
		if (Current->stage == s_quiet && (move & 0xFFFF) == (*(Current->current - 1) & 0xFFFF))
			HistoryGood(*(Current->current - 1), depth);	// restore history information
		else
			HistoryGood(move, depth);
		if (move != hash_move && Current->stage == s_quiet)
			for (auto p = Current->start; p < (Current->current - 1); ++p)
				HistoryBad(*p, depth);
		UpdateRef(move);
	}
	return score;
};

INLINE int RazoringThreshold(int score, int depth, int height)
{
	int shift = (20 + (3 * depth * (15 + Max(height, depth))) / 4 + 100 * Max(depth - 18, 0)) * CP_SEARCH;
	return score + shift + FutilityThreshold;
}

INLINE int reduction_n(int depth, int n)
{
	return msb(Square(Square(Square(uint64(n))))) / (6 + depth / 10);
}

template<bool me, bool exclusion> int scout(int beta, int depth, int flags)
{
	int value, cnt, flag, moves_to_play, score, move, ext, hash_move, singular, played, high_depth, high_value, hash_value,
		new_depth, move_back, hash_depth;
	int height = (int)(Current - Data);

	if (depth <= 1)
		return q_search<me, 0>(beta - 1, beta, 1, flags);
	if (flags & FlagHaltCheck)
	{
		if (height - MateValue >= beta)
			return beta;
		if (MateValue - height < beta)
			return beta - 1;
		HALT_CHECK;
	}

	if (exclusion)
	{
		cnt = high_depth = singular = played = 0;
		flag = 1;
		score = beta - 1;
		high_value = MateValue;
		hash_value = -MateValue;
		hash_depth = -1;
		hash_move = flags & 0xFFFF;
	}
	else
	{
		if (flags & FlagCallEvaluation)
			evaluate();
		if (IsCheck(me))
			return scout_evasion<me, 0>(beta, depth, flags & (~(FlagHaltCheck | FlagCallEvaluation)));

		value = Current->score - (90 + depth * 8 + Max(depth - 5, 0) * 32) * CP_SEARCH;
		if (value >= beta && depth <= 13 && T(NonPawnKing(me)) && F(Pawn(opp) & OwnLine(me, 1) & Shift<me>(~PieceAll())) && F(flags & (FlagReturnBestMove | FlagDisableNull)))
			return value;

		value = Current->score + FutilityThreshold;
		if (value < beta && depth <= 3)
			return Max(value, q_search<me, 0>(beta - 1, beta, 1, FlagHashCheck | (flags & 0xFFFF)));

		high_depth = 0;
		high_value = MateValue;
		hash_value = -MateValue;
		hash_depth = -1;
		Current->best = hash_move = flags & 0xFFFF;
		if (GEntry* Entry = probe_hash())
		{
			if (Entry->high < beta && Entry->high_depth >= depth)
				return Entry->high;
			if (Entry->high_depth > high_depth)
			{
				high_depth = Entry->high_depth;
				high_value = Entry->high;
			}
			if (T(Entry->move) && Entry->low_depth > hash_depth)
			{
				Current->best = hash_move = Entry->move;
				hash_depth = Entry->low_depth;
				if (Entry->low_depth)
					hash_value = Entry->low;
			}
			if (Entry->low >= beta && Entry->low_depth >= depth)
			{
				if (Entry->move)
				{
					Current->best = Entry->move;
					if (F(PieceAt(To(Entry->move))) && F(Entry->move & 0xE000))
					{
						if (Current->killer[1] != Entry->move && F(flags & FlagNoKillerUpdate))
						{
							for (int jk = N_KILLER; jk > 1; --jk)
								Current->killer[jk] = Current->killer[jk - 1];
							Current->killer[1] = Entry->move;
						}
						UpdateRef(Entry->move);
					}
					return Entry->low;
				}
				if (F(flags & FlagReturnBestMove))
					return Entry->low;
			}
		}

#if TB
		if (hash_depth < NominalTbDepth && depth >= TBMinDepth && unsigned(popcnt(PieceAll())) <= TB_LARGEST) {
			auto res = TBProbe(tb_probe_wdl, me);
			if (res != TB_RESULT_FAILED) {
				++INFO->tbHits;
				hash_high(TbValues[res], TbDepth(depth));
				hash_low(0, TbValues[res], TbDepth(depth));
				return TbValues[res];
			}
		}
#endif

		if (GPVEntry* PVEntry = (depth < 20 ? nullptr : probe_pv_hash()))
		{
			hash_low(PVEntry->move, PVEntry->value, PVEntry->depth);
			hash_high(PVEntry->value, PVEntry->depth);
			if (PVEntry->depth >= depth)
			{
				if (PVEntry->move)
					Current->best = PVEntry->move;
				if (F(flags & FlagReturnBestMove) && ((Current->ply <= PliesToEvalCut && PVEntry->ply <= PliesToEvalCut) || (Current->ply >= PliesToEvalCut && PVEntry->ply >= PliesToEvalCut)))
					return PVEntry->value;
			}
			if (T(PVEntry->move) && PVEntry->depth > hash_depth)
			{
				Current->best = hash_move = PVEntry->move;
				hash_depth = PVEntry->depth;
				hash_value = PVEntry->value;
			}
		}
		score = depth < 10 ? height - MateValue : beta - 1;
		if (depth >= 12 && (F(hash_move) || hash_value < beta || hash_depth < depth - 12) && (high_value >= beta || high_depth < depth - 12) &&
			F(flags & FlagDisableNull))
		{
			new_depth = depth - 8;
			value = scout<me, 0>(beta, new_depth, FlagHashCheck | FlagNoKillerUpdate | FlagDisableNull | FlagReturnBestMove | hash_move);
			if (value >= beta)
			{
				if (Current->best)
					hash_move = Current->best;
				hash_depth = new_depth;
				hash_value = beta;
			}
		}
		if (depth >= 4 && Current->score + 3 * CP_SEARCH >= beta && F(flags & (FlagDisableNull | FlagReturnBestMove)) && (high_value >= beta || high_depth < depth - 10) &&
			(depth < 12 || (hash_value >= beta && hash_depth >= depth - 12)) && beta > -EvalValue && T(NonPawnKing(me)))
		{
			new_depth = depth - 8;
			do_null();
			value = -scout<opp, 0>(1 - beta, new_depth, FlagHashCheck);
			undo_null();
			if (value >= beta)
			{
				if (depth < 12)
					hash_low(0, value, depth);
				return value;
			}
		}

		cnt = flag = singular = played = 0;
		if (T(hash_move))
		{
			move = hash_move;
			if (is_legal<me>(move) && !IsIllegal(me, move))
			{
				++cnt;
				ext = extension<me>(0, move, depth);
				if (depth >= 16 && hash_value >= beta && hash_depth >= (new_depth = depth - Min(12, depth / 2)))
				{
					int margin_one = beta - ExclSingle(depth);
					int margin_two = beta - ExclDouble(depth);
					int prev_ext = ExtFromFlag(flags);
					singular = SingularExtension<me>(ext, prev_ext, margin_one, margin_two, new_depth, hash_move);
					if (singular)
						ext = Max(ext, singular + (prev_ext < 1) - (singular >= 2 && prev_ext >= 2));
				}
				int to = To(move);
				if (depth < 16 && to == To(Current->move) && T(PieceAt(to)))	// recapture extension
					ext = Max(ext, 2);
				new_depth = depth - 2 + ext;
				do_move<me>(move);
				value = -scout<opp, 0>(1 - beta, new_depth,
					FlagNeatSearch | ((hash_value >= beta && hash_depth >= depth - 12) ? FlagDisableNull : 0) | ExtToFlag(ext));
				undo_move<me>(move);
				++played;
				if (value > score)
				{
					score = value;
					if (value >= beta)
						return cut_search<exclusion>(move, hash_move, score, beta, depth, flags);
				}
			}
		}
	}

	// done with hash move
	Current->killer[0] = 0;
	Current->stage = stage_search;
	Current->gen_flags = 0;
	Current->ref[0] = RefM(Current->move).ref[0];
	Current->ref[1] = RefM(Current->move).ref[1];
	move_back = 0;
	if (beta > 0 && Current->ply >= 2 && F((Current - 1)->move & 0xF000))
	{
		move_back = (To((Current - 1)->move) << 6) | From((Current - 1)->move);
		if (PieceAt(To(move_back)))
			move_back = 0;
	}
	moves_to_play = 3 + Square(depth) / 6;
	int margin = RazoringThreshold(Current->score, depth, height);
	if (margin < beta)
	{
		flag = 1;
		score = Max(margin, score);
		Current->stage = stage_razoring;
		Current->mask = Piece(opp);
		value = margin + (200 + 6 * depth) * CP_SEARCH;
		if (value < beta)
		{
			score = Max(value, score);
			Current->mask ^= Pawn(opp);
		}
	}
	Current->current = Current->moves;
	Current->moves[0] = 0;
	if (depth <= 5)
		Current->gen_flags |= FlagNoBcSort;

	while (move = get_move<me, 0>(depth))
	{
		if (move == hash_move)
			continue;
		if (IsIllegal(me, move))
			continue;
		++cnt;
		if (move == move_back)
		{
			score = Max(0, score);
			continue;
		}
		bool check;
		ext = extension<me>(0, move, depth, &check);
		new_depth = depth - 2 + ext;
		if (F(PieceAt(To(move))) && F(move & 0xE000))
		{
			if (!is_killer(move))
			{
				if (!check && cnt > moves_to_play)
				{
					Current->gen_flags &= ~FlagSort;
					continue;
				}
				if (depth >= 6)
				{
					int reduction = reduction_n(depth, cnt);
					if (move == Current->ref[0] || move == Current->ref[1])
						reduction = Max(0, reduction - 1);
					if (reduction >= 2 && !(Queen(White) | Queen(Black)) && popcnt(NonPawnKingAll()) <= 4)
						reduction += reduction / 2;
					if (new_depth - reduction > 3 && !see<me>(move, -SeeThreshold, SeeValue))
						reduction += 2;
					if (reduction == 1 && new_depth > 4)
						reduction = cnt > 3 ? 2 : 0;
					new_depth = Max(3, new_depth - reduction);
				}
			}
			if (!check)
			{
				if ((value = Current->score + DeltaM(move) + 10 * CP_SEARCH) < beta && depth <= 3)
				{
					score = Max(value, score);
					continue;
				}
				if (cnt > 7 && (value = margin + DeltaM(move) - 25 * CP_SEARCH * msb(cnt)) < beta && depth <= 19)
				{
					score = Max(value, score);
					continue;
				}
			}
			if (depth <= 9 && T(NonPawnKing(me)) && !see<me>(move, -SeeThreshold, SeeValue))
				continue;
		}
		else
		{
			if (Current->stage == r_cap)
			{
				if (!check && depth <= 9 && !see<me>(move, -SeeThreshold, SeeValue))
					continue;
			}
			else if (Current->stage == s_bad_cap && !check && depth <= 5)
				continue;
		}

		do_move<me>(move);
		value = -scout<opp, 0>(1 - beta, new_depth, FlagNeatSearch | ExtToFlag(ext));
		if (value >= beta && new_depth < depth - 2 + ext)
			value = -scout<opp, 0>(1 - beta, depth - 2 + ext, FlagNeatSearch | FlagDisableNull | ExtToFlag(ext));
		undo_move<me>(move);
		++played;
		if (value > score)
		{
			score = value;
			if (value >= beta)
				return cut_search<exclusion>(move, hash_move, score, beta, depth, flags);
		}
	}
	if (F(cnt) && F(flag))
	{
		hash_high(0, 127);
		hash_low(0, 0, 127);
		return 0;
	}
	if (F(exclusion))
		hash_high(score, depth);
	return score;
}

template<bool me, bool exclusion> int scout_evasion(int beta, int depth, int flags)
{
	int value, score, pext, move, cnt, hash_value = -MateValue, hash_depth, hash_move, new_depth, ext, moves_to_play;
	int height = (int)(Current - Data);

	if (depth <= 1)
		return q_evasion<me, 0>(beta - 1, beta, 1, flags);
	score = height - MateValue;
	if (flags & FlagHaltCheck)
	{
		if (score >= beta)
			return beta;
		if (MateValue - height < beta)
			return beta - 1;
		HALT_CHECK;
	}

	auto cut = [&](int move, int score)
	{
		if (exclusion)
			return score;
		Current->best = move;
		if (F(PieceAt(To(move))) && F(move & 0xE000))
		{
			UpdateCheckRef(move);
		}
		return hash_low(move, score, depth);
	};

	hash_depth = -1;
	hash_move = flags & 0xFFFF;
	if (exclusion)
	{
		cnt = pext = 0;
		score = beta - 1;
		(void)gen_evasions<me>(Current->moves);
		if (F(Current->moves[0]))
			return score;
	}
	else
	{
		Current->best = hash_move;
		if (GEntry* Entry = probe_hash())
		{
			if (Entry->high < beta && Entry->high_depth >= depth)
				return Entry->high;
			if (T(Entry->move) && Entry->low_depth > hash_depth)
			{
				Current->best = hash_move = Entry->move;
				hash_depth = Entry->low_depth;
			}
			if (Entry->low >= beta && Entry->low_depth >= depth)
			{
				if (Entry->move)
				{
					Current->best = Entry->move;
					if (F(PieceAt(To(Entry->move))) && F(Entry->move & 0xE000))
					{
						UpdateCheckRef(Entry->move);
					}
				}
				return Entry->low;
			}
			if (Entry->low_depth >= depth - 8 && Entry->low > hash_value)
				hash_value = Entry->low;
		}

		if (depth >= 20)
			if (GPVEntry* PVEntry = probe_pv_hash())
			{
				hash_low(PVEntry->move, PVEntry->value, PVEntry->depth);
				hash_high(PVEntry->value, PVEntry->depth);
				if (PVEntry->depth >= depth)
				{
					if (PVEntry->move)
						Current->best = PVEntry->move;
					return PVEntry->value;
				}
				if (T(PVEntry->move) && PVEntry->depth > hash_depth)
				{
					Current->best = hash_move = PVEntry->move;
					hash_depth = PVEntry->depth;
					hash_value = PVEntry->value;
				}
			}
#if TB
		if (hash_depth < NominalTbDepth && depth >= TBMinDepth && unsigned(popcnt(PieceAll())) <= TB_LARGEST) {
			auto res = TBProbe(tb_probe_wdl, me);
			if (res != TB_RESULT_FAILED) {
				++INFO->tbHits;
				hash_high(TbValues[res], TbDepth(depth));
				hash_low(0, TbValues[res], TbDepth(depth));
				return TbValues[res];
			}
		}
#endif

		if (hash_depth >= depth && hash_value > -EvalValue)
			score = Min(beta - 1, Max(score, hash_value));
		if (flags & FlagCallEvaluation)
			evaluate();

		Current->mask = Filled;
		if (Current->score - 10 * CP_SEARCH < beta && depth <= 3)
		{
			score = Current->score - 10 * CP_SEARCH;
			Current->mask = capture_margin_mask<me>(beta - 1, &score);
		}
		cnt = 0;
		(void)gen_evasions<me>(Current->moves);
		if (F(Current->moves[0]))
			return score;
		pext = 0;
		if (F(Current->moves[1]))
			pext = 2;

		if (T(hash_move))
		{
			move = hash_move;
			if (is_legal<me>(move) && !IsIllegal(me, move))
			{
				++cnt;
				ext = Max(pext, extension<me>(0, move, depth));
				if (depth >= 16 && hash_value >= beta && hash_depth >= (new_depth = depth - Min(12, depth / 2)))
				{
					int margin_one = beta - ExclSingle(depth);
					int margin_two = beta - ExclDouble(depth);
					int prev_ext = ExtFromFlag(flags);
					int singular = SingularExtension<me>(ext, prev_ext, margin_one, margin_two, new_depth, hash_move);
					if (singular)
						ext = Max(ext, singular + (prev_ext < 1) - (singular >= 2 && prev_ext >= 2));
				}
				new_depth = depth - 2 + ext;
				do_move<me>(move);
				evaluate();
				if (Current->att[opp] & King(me))
				{
					undo_move<me>(move);
					--cnt;
				}
				else
				{
					value = -scout<opp, 0>(1 - beta, new_depth,
						FlagHaltCheck | FlagHashCheck | ((hash_value >= beta && hash_depth >= depth - 12) ? FlagDisableNull : 0) | ExtToFlag(ext));
					undo_move<me>(move);
					if (value > score)
					{
						score = value;
						if (value >= beta)
							return cut(move, score);
					}
				}
			}
		}
	}

	// done with hash move
	moves_to_play = 3 + ((depth * depth) / 6);
	Current->ref[0] = RefM(Current->move).check_ref[0];
	Current->ref[1] = RefM(Current->move).check_ref[1];
	mark_evasions(Current->moves);
	Current->current = Current->moves;
	while (move = pick_move())
	{
		if (move == hash_move)
			continue;
		if (IsIllegal(me, move))
			continue;
		++cnt;
		if (IsRepetition(beta, move))
		{
			score = Max(0, score);
			continue;
		}
		bool check;
		ext = Max(pext, extension<me>(0, move, depth, &check));
		new_depth = depth - 2 + ext;
		if (F(PieceAt(To(move))) && F(move & 0xE000))
		{
			if (!check)
			{
				if (cnt > moves_to_play)
					continue;
				if ((value = Current->score + DeltaM(move) + 10 * CP_SEARCH) < beta && depth <= 3)
				{
					score = Max(value, score);
					continue;
				}
			}
			if (depth >= 6 && cnt > 3)
			{
				int reduction = reduction_n(depth, cnt);
				if (reduction >= 2 && !(Queen(White) | Queen(Black)) && popcnt(NonPawnKingAll()) <= 4)
					reduction += reduction / 2;
				new_depth = Max(3, new_depth - reduction);
			}
		}
		do_move<me>(move);
		value = -scout<opp, 0>(1 - beta, new_depth, FlagNeatSearch | ExtToFlag(ext));
		if (value >= beta && new_depth < depth - 2 + ext)
			value = -scout<opp, 0>(1 - beta, depth - 2 + ext, FlagNeatSearch | FlagDisableNull | ExtToFlag(ext));
		undo_move<me>(move);
		if (value > score)
		{
			score = value;
			if (value >= beta)
				return cut(move, score);
		}
	}
	if (F(exclusion))
		hash_high(score, depth);
	return score;
}

template<bool me, bool root> int pv_search(int alpha, int beta, int depth, int flags)
{
	int value, move, cnt, pext = 0, ext, hash_value = -MateValue, margin, singular = 0, played = 0, new_depth, hash_move,
		hash_depth, old_alpha = alpha, old_best, ex_depth = 0, ex_value = 0, start_knodes = (int)(INFO->nodes >> 10);
	int height = (int)(Current - Data);

	if (root)
	{
		depth = Max(depth, 2);
		flags |= ExtToFlag(1);
		if (F(RootList[0]))
			return 0;
		if (time_to_stop(CurrentSI, LastTime, false))
			stop();	// will break the outer loop

		auto p = &RootList[0];
		while (*p) ++p;
		sort_moves(&RootList[0], p);
		for (p = &RootList[0]; *p; ++p) *p &= 0xFFFF;
		SetScore(&RootList[0], 2);
	}
	else
	{
		if (depth <= 1)
			return q_search<me, 1>(alpha, beta, 1, FlagNeatSearch);
		if (static_cast<int>(Current - Data) - MateValue >= beta)
			return beta;
		if (MateValue - static_cast<int>(Current - Data) <= alpha)
			return alpha;
		HALT_CHECK;
	}

	// check hash
	hash_depth = -1;
	Current->best = hash_move = 0;
	if (GPVEntry* PVEntry = probe_pv_hash())
	{
		hash_low(PVEntry->move, PVEntry->value, PVEntry->depth);
		hash_high(PVEntry->value, PVEntry->depth);
		if (PVEntry->depth >= depth && T(PVHashing))
		{
			if (PVEntry->move)
				Current->best = PVEntry->move;
			if ((Current->ply <= PliesToEvalCut && PVEntry->ply <= PliesToEvalCut) || (Current->ply >= PliesToEvalCut && PVEntry->ply >= PliesToEvalCut))
				if (!PVEntry->value || !draw_in_pv<me>())
					return PVEntry->value;
		}
		if (T(PVEntry->move) && PVEntry->depth > hash_depth)
		{
			Current->best = hash_move = PVEntry->move;
			hash_depth = PVEntry->depth;
			hash_value = PVEntry->value;
		}
	}
	if (GEntry* Entry = probe_hash())
	{
		if (T(Entry->move) && Entry->low_depth > hash_depth)
		{
			Current->best = hash_move = Entry->move;
			hash_depth = Entry->low_depth;
			if (Entry->low_depth)
				hash_value = Entry->low;
		}
	}
#if TB
	if (!root && hash_depth < NominalTbDepth && depth >= TBMinDepth && unsigned(popcnt(PieceAll())) <= TB_LARGEST)
	{
		auto res = TBProbe(tb_probe_wdl, me);
		if (res != TB_RESULT_FAILED) {
			++INFO->tbHits;
			hash_high(TbValues[res], TbDepth(depth));
			hash_low(0, TbValues[res], TbDepth(depth));
		}
	}
#endif

	if (root)
	{
		hash_move = RootList[0];
		hash_value = Previous;
		hash_depth = Max(0, depth - 2);
	}

	evaluate();

	if (F(root) && depth >= 6 && (F(hash_move) || hash_value <= alpha || hash_depth < depth - 8))
	{
		new_depth = depth - (T(hash_move) ? 4 : 2);
		value = pv_search<me, 0>(alpha, beta, new_depth, hash_move);
		bool accept = value > alpha || alpha < -EvalValue;
		if (!accept)
		{
			new_depth = depth - 8;
			for (int i = 0; i < 5 && !accept; ++i)
			{
				margin = alpha - (CP_SEARCH << (i + 3));
				if (T(hash_move) && hash_depth >= new_depth && hash_value >= margin)
					break;
				value = scout<me, 0>(margin, new_depth, FlagHashCheck | FlagNoKillerUpdate | FlagDisableNull | FlagReturnBestMove | hash_move);
				accept = value >= margin;
			}
		}
		if (accept)
		{
			if (Current->best)
				hash_move = Current->best;
			hash_depth = new_depth;
			hash_value = value;
		}
	}
	if (F(root) && IsCheck(me))
	{
		Current->mask = Filled;
		(void)gen_evasions<me>(Current->moves);
		if (F(Current->moves[0]))
			return static_cast<int>(Current - Data) - MateValue;
		alpha = Max(static_cast<int>(Current - Data) - MateValue, alpha);
		if (F(Current->moves[1]))
			pext = 2;
	}

	cnt = 0;
	if (hash_move && is_legal<me>(move = hash_move))
	{
		++cnt;
		if (root)
		{
			memset(Data + 1, 0, 127 * sizeof(GData));
			if (INFO->id == 0)
				send_curr_move(move, cnt);
		}
		ext = Max(pext, extension<me>(1, move, depth));
		if (depth >= 12 && hash_value > alpha && hash_depth >= (new_depth = depth - Min(12, depth / 2)))
		{
			int margin_one = hash_value - ExclSingle(depth);
			int margin_two = hash_value - ExclDouble(depth);
			int prev_ext = ExtFromFlag(flags);
			singular = SingularExtension<me>(root ? 0 : ext, root ? 0 : prev_ext, margin_one, margin_two, new_depth, hash_move);
			if (singular)
			{
				ext = Max(ext, singular + (prev_ext < 1) - (singular >= 2 && prev_ext >= 2));
				if (root)
					CurrentSI->Singular = singular;
				ex_depth = new_depth;
				ex_value = (singular >= 2 ? margin_two : margin_one) - 1;
			}
		}
		new_depth = depth - 2 + ext;
		do_move<me>(move);
		evaluate();
		if (Current->att[opp] & King(me))
		{
			undo_move<me>(move);	// we will detect whether move has been undone, below
			--cnt;
		}

		if (!MY_TURN)	// move has not been undone, i.e., don't skip hash move
		{
			value = -pv_search<opp, 0>(-beta, -alpha, new_depth, ExtToFlag(ext));
			undo_move<me>(move);
			++played;
			if (value > alpha)
			{
				if (root)
				{
					CurrentSI->FailLow = 0;
					hash_low(move, value, depth);
					INFO->bestMove = move;
					INFO->bestScore = value;
					if (depth >= 8)
						send_pv(depth, old_alpha, beta);
				}
				Current->best = move;
				if (value >= beta)
					return hash_low(move, value, depth);
				alpha = value;
			}
			else if (root)
			{
				CurrentSI->FailLow = 1;
				CurrentSI->FailHigh = 0;
				CurrentSI->Singular = 0;
				INFO->bestScore = value;
				Say("Fail low at depth " + Str(depth/2) + "\n");
				if (depth >= 8)
					send_pv(depth, old_alpha, beta, true);
			}
		}
	}

	Current->gen_flags = 0;
	if (F(IsCheck(me)))
	{
		Current->stage = stage_search;
		Current->ref[0] = RefM(Current->move).ref[0];
		Current->ref[1] = RefM(Current->move).ref[1];
	}
	else
	{
		Current->stage = stage_evasion;
		Current->ref[0] = RefM(Current->move).check_ref[0];
		Current->ref[1] = RefM(Current->move).check_ref[1];
	}
	Current->killer[0] = 0;
	Current->moves[0] = 0;
	if (root)
		Current->current = &RootList[1];
	else
		Current->current = Current->moves;

	while (move = get_move<me, root>(depth))
	{
		if (move == hash_move)
			continue;
		if (IsIllegal(me, move))
			continue;
		++cnt;
		if (root)
		{
			memset(Data + 1, 0, 127 * sizeof(GData));
			send_curr_move(move, cnt);
		}
		if (IsRepetition(alpha + 1, move))
			continue;
		ext = Max(pext, extension<me>(1, move, depth));
		new_depth = depth - 2 + ext;
		if (depth >= 6 && F(move & 0xE000) && F(PieceAt(To(move))) && (T(root) || !is_killer(move) || T(IsCheck(me))) && cnt > 3)
		{
			int reduction = reduction_n(depth, cnt) - 1;
			if (move == Current->ref[0] || move == Current->ref[1])
				reduction = Max(0, reduction - 1);
			if (reduction >= 2 && !(Queen(White) | Queen(Black)) && popcnt(NonPawnKingAll()) <= 4)
				reduction += reduction / 2;
			new_depth = Max(3, new_depth - reduction);
		}

		do_move<me>(move);  // now Current points at child, until we undo_move, below
		if (new_depth <= 1)
			value = -pv_search<opp, 0>(-beta, -alpha, new_depth, ExtToFlag(ext));
		else
			value = -scout<opp, 0>(-alpha, new_depth, FlagNeatSearch | ExtToFlag(ext));
		if (value > alpha && new_depth > 1)
		{
			if (root)
			{
				SetScore(&RootList[cnt - 1], 1);
				CurrentSI->Early = 0;
				old_best = INFO->bestMove;
				INFO->bestMove = move;
			}
			new_depth = depth - 2 + ext;
			value = -pv_search<opp, 0>(-beta, -alpha, new_depth, ExtToFlag(ext));
			if (root)
			{
				if (value <= alpha)
					INFO->bestMove = old_best;
			}
		}
		undo_move<me>(move);
		++played;
		if (value > alpha)
		{
			if (root)
			{
				SetScore(&RootList[cnt - 1], cnt + 3);
				CurrentSI->Change = 1;
				CurrentSI->FailLow = 0;
				hash_low(move, value, depth);
				INFO->bestMove = move;
				INFO->bestScore = value;
				if (depth >= 8)
					send_pv(depth, old_alpha, beta);
			}
			Current->best = move;
			if (value >= beta)
				return hash_low(move, value, depth);
			alpha = value;
		}
	}
	if (F(cnt) && F(IsCheck(me)))
	{
		hash_high(0, 127);
		hash_low(0, 0, 127);
		hash_exact(0, 0, 127, 0, 0, 0);
		return 0;
	}
	if (F(root) || F(SearchMoves))
		hash_high(alpha, depth);
	if (alpha > old_alpha)
	{
		hash_low(Current->best, alpha, depth);
		if (Current->best != hash_move)
			ex_depth = 0;
		if (F(root) || F(SearchMoves))
			hash_exact(Current->best, alpha, depth, ex_value, ex_depth, static_cast<int>(INFO->nodes >> 10) - start_knodes);
	}
	return alpha;
}

template<bool me> void root()
{HERE
	int& depth = INFO->depth;
	int value, alpha, beta, start_depth = 2, hash_depth = 0, hash_value, store_time = 0, time_est, ex_depth = 0, ex_value, prev_time = 0, knodes = 0;
	int64_t time;
	HERE
	evaluate();
	gen_root_moves<me>();
	if (PVN > 1) {
		//        memset(MultiPV,0,128 * sizeof(int));
		//        for (i = 0; MultiPV[i] = RootList[i]; i++);
		fprintf(stderr, "MPV NYI...\n");
		abort();
	}HERE
	INFO->bestMove = RootList[0];
	if (F(INFO->bestMove))
	{
		stop();
		return;
	}HERE
	if (F(RootList[1]))
	{
		if (F(SHARED->best.move))
		{
			LOCK_SHARED;
			if (F(SHARED->best.move))
			{
				ScopedMove_ next(RootList[0]);
				const score_t* score = nullptr;
				if (GEntry* Entry = probe_hash())
				{
					if (Entry->low_depth)
						score = &Entry->low;
					else if (Entry->high_depth)
						score = &Entry->high;
				}
				if (!score)
				{
					evaluate();
					score = &Current->score;
				}
				SHARED->best = { 1, -*score, RootList[0], INFO->id };
				stop();
			}
		}
		return;
	}HERE

	memset(CurrentSI, 0, sizeof(GSearchInfo));
	memset(BaseSI, 0, sizeof(GSearchInfo));
	Previous = -MateValue;
	if (GPVEntry* PVEntry = probe_pv_hash())
	{
		if (is_legal<me>(PVEntry->move) && PVEntry->move == INFO->bestMove && PVEntry->depth > hash_depth)
		{
			hash_depth = PVEntry->depth;
			hash_value = PVEntry->value;
			ex_depth = PVEntry->ex_depth;
			ex_value = PVEntry->exclusion;
			knodes = PVEntry->knodes;
		}
	}HERE
	LastTime = 0;
	if (hash_depth > 0 && PVN == 1)
	{
		Previous = INFO->bestScore = hash_value;
		depth = hash_depth;
		if (PVHashing)
		{
			send_pv(depth, -MateValue, MateValue);
			start_depth = (depth + 2) & (~1);
		}
		if ((depth >= LastDepth - 8 || T(store_time)) && LastValue >= LastExactValue && hash_value >= LastExactValue && T(LastTime) && T(LastSpeed))
		{
			time = SHARED->softTimeLimit;
			if (ex_depth >= depth - Min(12, depth / 2) && ex_value <= hash_value - ExclSingle(depth))
			{
				BaseSI->Early = 1;
				BaseSI->Singular = 1;
				if (ex_value <= hash_value - ExclDouble(depth))
				{
					time = (time * TimeSingTwoMargin) / 100;
					BaseSI->Singular = 2;
				}
				else time = (time * TimeSingOneMargin) / 100;
			}
			time_est = Min(LastTime, int((knodes << 10) / LastSpeed));
			time_est = Max(time_est, store_time);
			//		set_prev_time:
			LastTime = prev_time = time_est;
		}
	}HERE

	memcpy(SaveBoard, Board, sizeof(GBoard));
	memcpy(SaveData, Data, sizeof(GData));
	save_sp = sp;
	int skipped = 0;
	for (depth = start_depth; !SHARED->stopAll && depth < SHARED->depthLimit && depth < 126; depth += 2)
	{
		if (INFO->id == 0)
			Say("info depth " + Str(depth / 2) + "\n");
		if (SHARED->best.depth > depth)	// we have fallen too far behind
			continue;
		HERE
		memset(Data + 1, 0, 127 * sizeof(GData));
		CurrentSI->Early = 1;
		CurrentSI->Change = CurrentSI->FailHigh = CurrentSI->FailLow = CurrentSI->Singular = 0;
		if (PVN > 1)
			value = multipv<me>(depth);
		else if ((depth / 2) < 7 || F(Aspiration) || INFO->id < 0)
		{
			LastValue = LastExactValue = value = pv_search<me, 1>(-MateValue, MateValue, depth, FlagNeatSearch);
			send_pv(depth, -MateValue, MateValue);
		}
		else
		{
			int deltaLo = FailLoInit, deltaHi = FailHiInit;
			if (SHARED->best.depth == depth)
			{
				Previous = SHARED->best.value;
				deltaLo /= 2;
				deltaHi /= 2;
			}HERE
			alpha = Previous - deltaLo;
			beta = Previous + deltaHi;
			if (SHARED->best.failLow)
			{
				alpha -= 1 + deltaHi;
				beta -= 1 + deltaHi;
				if (INFO->id & 1)
				{
					// try to escape fail-low state quickly
					alpha -= (alpha * INFO->id) % 256;
					beta = alpha + 1;
				}
			}HERE
			for ( ; ; )
			{
				HERE
				if (SHARED->best.depth > depth)	// search has moved beyond us
				{
					Previous = value = SHARED->best.value;
					break;
				}
				if (Max(deltaLo, deltaHi) >= 1300)
				{
					LastValue = LastExactValue = value = pv_search<me, 1>(-MateValue, MateValue, depth, FlagNeatSearch);
					break;
				}HERE
				value = pv_search<me, 1>(alpha, beta, depth, FlagNeatSearch);
				HERE
				if (value <= alpha)
				{
					CurrentSI->FailHigh = 0;
					CurrentSI->FailLow = 1;
					alpha -= deltaLo;
					deltaLo += (deltaLo * FailLoGrowth) / 64 + FailLoDelta;
					LastValue = value;
					memcpy(BaseSI, CurrentSI, sizeof(GSearchInfo));
					if (time_to_stop(CurrentSI, LastTime, false))
					{
						stop();	// will break the outer loop
						break;
					}
					continue;
				}
				else if (value >= beta)
				{
					CurrentSI->FailHigh = 1;
					CurrentSI->FailLow = 0;
					CurrentSI->Early = 1;
					CurrentSI->Change = 0;
					CurrentSI->Singular = Max(CurrentSI->Singular, 1);
					beta += deltaHi;
					deltaHi += (deltaHi * FailHiGrowth) / 64 + FailHiDelta;
					LastDepth = depth;
					LastTime = Max<int>(prev_time, millisecs(SHARED->startTime, now()));
					LastSpeed = INFO->nodes / Max(LastTime, 1);
					if (depth + 2 < SHARED->depthLimit)
						depth += 2;
					InstCnt = 0;
					if (time_to_stop(CurrentSI, LastTime, false))
					{
						stop();	// will break the outer loop
						break;
					}
					memset(Data + 1, 0, 127 * sizeof(GData));
					LastValue = value;
					memcpy(BaseSI, CurrentSI, sizeof(GSearchInfo));
					continue;
				}
				else
				{
					LastValue = LastExactValue = value;
					break;
				}HERE
			}HERE
		}HERE

		if (!SHARED->stopAll)
		{
			HERE
			CurrentSI->Bad = value < Previous - 12 * CP_SEARCH;
			memcpy(BaseSI, CurrentSI, sizeof(GSearchInfo));
			LastDepth = depth;
			LastTime = Max<int>(prev_time, millisecs(SHARED->startTime, now()));
			LastSpeed = INFO->nodes / Max(LastTime, 1);
			Previous = value;
			InstCnt = 0;
			if (INFO->id == 0 && time_to_stop(CurrentSI, LastTime, false))
			{
				stop();
				break;
			}
		}
	}

	Current = Data;
	memcpy(Board, SaveBoard, sizeof(GBoard));
	memcpy(Data, SaveData, sizeof(GData));
	sp = save_sp;
	if (!SHARED->stopAll)
	{
		LOCK_SHARED;
		SHARED->stopAll = true;
	}
}

template<bool me> int multipv(int depth)
{
	int move, low = MateValue, value, i, cnt, ext, new_depth = depth;
	Say("info depth " + Str(depth / 2) + "\n");
	for (cnt = 0; cnt < PVN && T(move = (MultiPV[cnt] & 0xFFFF)); ++cnt)
	{
		MultiPV[cnt] = move;
		send_curr_move(move, cnt);
		new_depth = depth - 2 + (ext = extension<me>(1, move, depth));
		do_move<me>(move);
		value = -pv_search<opp, 0>(-MateValue, MateValue, new_depth, ExtToFlag(ext));
		MultiPV[cnt] |= value << 16;
		if (value < low)
			low = value;
		undo_move<me>(move);
		for (i = cnt - 1; i >= 0; --i)
		{
			if ((MultiPV[i] >> 16) < value)
			{
				MultiPV[i + 1] = MultiPV[i];
				MultiPV[i] = move | (value << 16);
			}
		}
		INFO->bestMove = MultiPV[0] & 0xFFFF;
		Current->score = MultiPV[0] >> 16;
		send_multipv((depth / 2), cnt);
	}
	for (; T(move = (MultiPV[cnt] & 0xFFFF)); ++cnt)
	{
		MultiPV[cnt] = move;
		send_curr_move(move, cnt);
		new_depth = depth - 2 + (ext = extension<me>(1, move, depth));
		do_move<me>(move);
		value = -scout<opp, 0>(-low, new_depth, FlagNeatSearch | ExtToFlag(ext));
		if (value > low)
			value = -pv_search<opp, 0>(-MateValue, -low, new_depth, ExtToFlag(ext));
		MultiPV[cnt] |= value << 16;
		undo_move<me>(move);
		if (value > low)
		{
			for (i = cnt; i >= PVN; --i) MultiPV[i] = MultiPV[i - 1];
			MultiPV[PVN - 1] = move | (value << 16);
			for (i = PVN - 2; i >= 0; --i)
			{
				if ((MultiPV[i] >> 16) < value)
				{
					MultiPV[i + 1] = MultiPV[i];
					MultiPV[i] = move | (value << 16);
				}
			}
			INFO->bestMove = MultiPV[0] & 0xFFFF;
			Current->score = MultiPV[0] >> 16;
			low = MultiPV[PVN - 1] >> 16;
			send_multipv((depth / 2), cnt);
		}
	}
	return Current->score;
}

static void send_pv
	(const int* PV,
	 size_t nodes,
	 size_t tbHits,
	 int depth,
	 int selDepth,
	 int bestScore,
	 int bestMove,
	 bool fail_low,
	 const std::chrono::high_resolution_clock::time_point& startTime)
{
	const char *scoreType = "mate";
	if (bestScore > EvalValue)
		bestScore = (MateValue - bestScore + 1) / 2;
	else if (bestScore < -EvalValue)
		bestScore = -(bestScore + MateValue + 1) / 2;
	else
	{
		scoreType = fail_low ? "cp<" : "cp";
		bestScore /= CP_SEARCH;
	}

	auto currTime = now();
	auto elapsedTime = millisecs(startTime, currTime);
	if (elapsedTime == 0)
		elapsedTime = 1;

	size_t nps = (nodes * 1000) / elapsedTime;

	char pvStr[PIPE_BUF];
	unsigned pvPos = 0;
	for (unsigned i = 0; i < MAX_PV_LEN && PV[i] != 0; i++)
	{
		if (pvPos >= sizeof(pvStr) - 32)
			break;
		int move = PV[i];
		pvStr[pvPos++] = ' ';
		pvStr[pvPos++] = ((move >> 6) & 7) + 'a';
		pvStr[pvPos++] = ((move >> 9) & 7) + '1';
		pvStr[pvPos++] = (move & 7) + 'a';
		pvStr[pvPos++] = ((move >> 3) & 7) + '1';
		if (IsPromotion(move))
		{
			if ((move & 0xF000) == FlagPQueen)
				pvStr[pvPos++] = 'q';
			else if ((move & 0xF000) == FlagPRook)
				pvStr[pvPos++] = 'r';
			else if ((move & 0xF000) == FlagPBishop)
				pvStr[pvPos++] = 'b';
			else if ((move & 0xF000) == FlagPKnight)
				pvStr[pvPos++] = 'n';
		}
	}
	pvStr[pvPos++] = '\0';

	Say("info depth " + Str(depth / 2) + 
		" seldepth " + Str(selDepth) + 
		" score " + string(scoreType) + " " + Str(bestScore) + 
		" nodes " + Str(nodes) + " nps " + Str(nps) + " tbhits " + Str(tbHits) + 
		" time " + Str(millisecs(startTime, currTime)) + " pv" + string(pvStr) + "\n");
}

void send_pv(int depth, int alpha, int beta, bool fail_low = false)
{
	int sel_depth = 0;
	while (sel_depth < 126 && T((Data + sel_depth + 1)->att[0]))
		++sel_depth;
	int move = (INFO->bestMove == 0 ? RootList[0] : INFO->bestMove);
	bool isBest = false;
	INFO->selDepth = sel_depth;
	INFO->PV[0] = move;
	do_move(T(Current->turn), move);
	unsigned pvPtr = 1, pvLen = 64;
	pick_pv(pvPtr, pvLen);
	undo_move(F(Current->turn), move);
	// find out whether this is the best candidate move so far
	{
		LOCK_SHARED;
		isBest = depth > SHARED->best.depth;
		if (depth == SHARED->best.depth && move != SHARED->best.move)
		{
			double delta = INFO->bestScore - SHARED->best.value + (SHARED->best.failLow ? 65536 : 0) - (fail_low ? 65536 : 0) + (INFO->id ? 0 : 0.5);
			isBest = delta > 0;
		}
		if (isBest)
			SHARED->best = { depth, INFO->bestScore, move, INFO->id, fail_low };
	}
	if (!isBest)
		return;

	size_t nodes = 0, tbHits = 0;
	for (int i = 0; i < SETTINGS->nThreads; i++)
	{
		nodes += THREADS[i]->nodes;
		tbHits += THREADS[i]->tbHits;
	}
	const ThreadOwn_ my = *THREADS[INFO->id];
	send_pv(&my.PV[0], nodes, tbHits, my.depth, my.selDepth, my.bestScore, my.bestMove, fail_low, SHARED->startTime);
}

void send_multipv(int depth, int curr_number)
{
	abort();	// not implemented
}

void send_curr_move(int move, int cnt)
{
	if (INFO->id != 0)
		return;
	auto currTime = now();
	auto diffTime = millisecs(SHARED->startTime, currTime);
	if (diffTime <= InfoLag || millisecs(InfoTime, currTime) <= InfoDelay)
		return;
	InfoTime = currTime;
	char moveStr[16];
	move_to_string(move, moveStr);
	Say("info currmove " + string(moveStr) + " currmovenumber " + Str(cnt) + "\n");
}

void send_move_info(int bestScore)
{
	size_t nodes = 1, tbHits = 0;
	for (int i = 0; i < SETTINGS->nThreads; i++)
	{
		nodes += THREADS[i]->nodes;
		tbHits += THREADS[i]->tbHits;
	}
	auto stopTime = now();
	auto time = millisecs(SHARED->startTime, stopTime);
	uint64_t nps = (nodes / Max(time / 1000, 1u));
	const char *scoreType = "mate";
	if (bestScore > EvalValue)
		bestScore = (MateValue - bestScore + 1) / 2;
	else if (bestScore < -EvalValue)
		bestScore = -(bestScore + MateValue + 1) / 2;
	else
	{
		scoreType = "cp";
		bestScore /= CP_SEARCH;
	}

	Say("info nodes " + Str(nodes) + " tbhits " + Str(tbHits) + " time " + Str(time) + " nps " + Str(nps) + " score " + string(scoreType) + " " + Str(bestScore) + "\n");
}

template<class T_> void send_best_move(T_ best)
{
	char moveStr[16];
	move_to_string(best.move, moveStr);
	const int* PV = nullptr;
	int pvd = 0;
	for (int i = 0; i < THREADS.size(); ++i)
		if (THREADS[i]->PV[0] == best.move && THREADS[i]->depth > pvd)
			PV = &THREADS[i]->PV[0];
	if (PV && PV[0] && PV[1])
	{
		char ponderStr[16];
		move_to_string(PV[1], ponderStr);
		Say("bestmove " + string(moveStr) + " ponder " + string(ponderStr) + "\n");
	}
	else
		Say("bestmove " + string(moveStr) + "\n");
}

void send_best_move(bool have_lock = false)
{
	assert(SHARED->best.move);
	if (have_lock)
	{
		send_move_info(SHARED->best.value);
		send_best_move(SHARED->best);
		SHARED->best = { 0, 0, 0, 0 };
	}
	else
	{
		LOCK_SHARED;
		send_move_info(SHARED->best.value);
		send_best_move(SHARED->best);
		SHARED->best = { 0, 0, 0, 0 };
	}
}

void get_position(char string[])
{
	const char* fen;
	char* moves;
	const char* ptr;
	int move, move1 = 0;

	fen = strstr(string, "fen ");
	moves = strstr(string, "moves ");
	if (fen != nullptr)
		get_board(fen + 4);
	else
		get_board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	PrevMove = 0;
	if (moves != nullptr)
	{
		ptr = moves + 6;
		while (*ptr != 0)
		{
			pv_string[0] = *ptr++;
			pv_string[1] = *ptr++;
			pv_string[2] = *ptr++;
			pv_string[3] = *ptr++;
			if (*ptr == 0 || *ptr == ' ')
				pv_string[4] = 0;
			else
			{
				pv_string[4] = *ptr++;
				pv_string[5] = 0;
			}
			evaluate();
			move = move_from_string(pv_string);
			PrevMove = move1;
			move1 = move;
			if (Current->turn)
				do_move<1>(move);
			else
				do_move<0>(move);
			memcpy(Data, Current, sizeof(GData));
			Current = Data;
			while (*ptr == ' ') ++ptr;
		}
	}
	copy(Stack.begin() + sp - Current->ply, Stack.begin() + sp + 1, Stack.begin());
	//memcpy(Stack, Stack + sp - Current->ply, (Current->ply + 1) * sizeof(uint64));
	sp = Current->ply;
}

int time_to_stop(GSearchInfo* SI, int time, int searching)
{
	if (time > SHARED->hardTimeLimit)
		return 1;
	if (searching)
		return 0;
	if (2 * time > SHARED->hardTimeLimit)
		return 1;
	if (SI->Bad)
		return 0;
	if (time > SHARED->softTimeLimit)
		return 1;
	if (T(SI->Change) || T(SI->FailLow))
		return 0;
	if (time * 100 > SHARED->softTimeLimit * TimeNoChangeMargin)
		return 1;
	if (F(SI->Early))
		return 0;
	if (time * 100 > SHARED->softTimeLimit * TimeNoPVSCOMargin)
		return 1;
	if (SI->Singular < 1)
		return 0;
	if (time * 100 > SHARED->softTimeLimit * TimeSingOneMargin)
		return 1;
	if (SI->Singular < 2)
		return 0;
	if (time * 100 > SHARED->softTimeLimit * TimeSingTwoMargin)
		return 1;
	return 0;
}

inline int get_number(const char *token)
{
	if (token == NULL)
		return 0;
	return atoi(token);
}
inline size_t get_size(const char *token)
{
	if (token == NULL)
		return 0;
	return size_t(atoll(token));
}

namespace Version
{
	const char* mdy = __DATE__;
	const char now[10] = { mdy[7], mdy[8], mdy[9], mdy[10], mdy[0], mdy[1], mdy[2], mdy[4], mdy[5], 0 };
}


void uci(void)
{
	char line[4 * PIPE_BUF];
	auto currTime = now();

	while (true)
	{
		line[0] = '\0';
		while (true)
		{
			DWORD timeout = numeric_limits<DWORD>::max();
			//			if (!SHARED->working.Empty())
			//				timeout = 100;          // 100ms

			bool timedout = get_line(line, sizeof(line) - 1, timeout);
			currTime = now();

			if (mutex_try_lock(&SHARED->mutex, 500))
				mutex_unlock(&SHARED->mutex);
			else
				emergency_stop();
			if (!SHARED->working.Empty() && millisecs(SHARED->startTime, currTime) >= SHARED->hardTimeLimit / 2)
			{
				stop();
				wait_for_stop();
			}
			if (!timedout)
				break;
		}

		if (line[0] == EOF)
		{
			nuke_children();
			exit(EXIT_SUCCESS);
		}
		char *saveptr = NULL;
		char *token = strtok_s(line, " ", &saveptr);
		if (token == NULL)
			/* NOP */;
		else if (strcmp(token, "go") == 0)
		{
			if (!SHARED->working.Empty())
				continue;
			int binc = 0, btime = 0, depth = 256, movestogo = 0,
				winc = 0, wtime = 0, movetime = 0;
			bool infinite = false, ponder = false;
			while ((token = strtok_s(NULL, " ", &saveptr)) != NULL)
			{
				if (strcmp(token, "binc") == 0)
					binc = get_number(strtok_s(NULL, " ", &saveptr));
				else if (strcmp(token, "btime") == 0)
					btime = get_number(strtok_s(NULL, " ", &saveptr));
				else if (strcmp(token, "winc") == 0)
					winc = get_number(strtok_s(NULL, " ", &saveptr));
				else if (strcmp(token, "wtime") == 0)
					wtime = get_number(strtok_s(NULL, " ", &saveptr));
				else if (strcmp(token, "movetime") == 0)
					movetime = get_number(strtok_s(NULL, " ", &saveptr));
				else if (strcmp(token, "movetogo") == 0)
					movestogo = get_number(strtok_s(NULL, " ", &saveptr));
				else if (strcmp(token, "depth") == 0)
					depth = get_number(strtok_s(NULL, " ", &saveptr));
				else if (strcmp(token, "infinite") == 0)
					infinite = true;
				else if (strcmp(token, "ponder") == 0)
					ponder = true;
			}
			int time = (Current->turn == White ? wtime : btime);
			int inc = (Current->turn == White ? winc : binc);
			if (movetime == 0 && time == 0 && inc == 0)
				infinite = true;
			if (movestogo != 0)
				movestogo = Max(movestogo - 1, 1);
			int time_max = Max(time - Min(1000, (3 * time) / 4), 0);
			int nmoves;
			int exp_moves = MovesTg - 1;
			if (movestogo != 0)
				nmoves = movestogo;
			else
			{
				nmoves = MovesTg - 1;
				if (Current->ply > 40)
					nmoves += Min(Current->ply - 40, (100 - Current->ply) / 2);
				exp_moves = nmoves;
			}
			int softTimeLimit = Min(time_max, (time_max + (Min(exp_moves, nmoves) * inc)) / Min(exp_moves, nmoves));
			int hardTimeLimit = Min(time_max, (time_max + (Min(exp_moves, nmoves) * inc)) / Min(5, Min(exp_moves, nmoves)));
			softTimeLimit = Min(time_max, (softTimeLimit * TimeRatio) / 100);

			if (movetime != 0)
			{
				hardTimeLimit = movetime;
				softTimeLimit = numeric_limits<decltype(softTimeLimit)>::max();
			}
			else if (infinite)
				hardTimeLimit = softTimeLimit = numeric_limits<int>::max();
			{
				LOCK_SHARED;
				SHARED->ponder = ponder;
				SHARED->softTimeLimit = softTimeLimit;
				SHARED->hardTimeLimit = hardTimeLimit;
				SHARED->depthLimit = 2 * depth + 2;
				SHARED->startTime = currTime;
			}
			SHARED->rootProgress = Progress();
			go();
			// children start working; we wait until they are done
			for (int i = 1; ; ++i)
			{
				Sleep(i);
				if (SHARED->working.Empty())
					break;
				if (millisecs(SHARED->startTime, now()) > SHARED->hardTimeLimit)
				{
					VSAY("Hard stop\n");
					SHARED->stopAll = true;
				}
			}
		}
		else if (strcmp(token, "isready") == 0)
		{
			Say("readyok\n");
		}
		else if (strcmp(token, "stop") == 0)
		{
			Say("id name stop requested\n");
			{
				LOCK_SHARED;
				if (!SHARED->working.Empty())
				{
					for (int ii = 0; ii < SETTINGS->nThreads; ++ii)
					{
						if (SHARED->working.vals_[ii].next_ != ii)
							Say("id name worker " + Str(ii) + " still active\n");
					}
				}
				//if (SHARED->stopAll)
				//	VSAY("info " + Str(INFO->id) + "received stop request\n");
				//if (SHARED->best.move)
				//	VSAY("info stored candidate is move " + Str(SHARED->best.move) + " depth " + Str(SHARED->best.depth) + " score " + Str(SHARED->best.value) + " worker " + Str(SHARED->best.worker) + "\n");
				//else
				//	VSAY("info no stored candidate\n");
			}
			if (SHARED->working.Empty())
				continue;
			stop();
			wait_for_stop();
		}
		else if (strcmp(token, "ponderhit") == 0)
		{
			SHARED->ponder = false;	// don't bother with mutex, workers will not write to ponder
			if (SHARED->working.Empty())
				send_best_move(true);
		}
		else if (strcmp(token, "position") == 0)
		{
			token = strtok_s(NULL, " ", &saveptr);
			if (token == NULL)
				goto bad_command;
			char *moves;
			if (strcmp(token, "fen") == 0)
			{
				char *fen = token + strlen(token) + 1;
				moves = (char*)get_board(fen);
			}
			else if (strcmp(token, "startpos") == 0)
			{
				moves = saveptr;
				get_board(
					"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
			}
			else
				goto bad_command;
			token = strtok_s(moves, " ", &saveptr);
			if (token != NULL && strcmp(token, "moves") == 0)
			{
				LOCK_SHARED;
				while ((token = strtok_s(NULL, " ", &saveptr)) != NULL)
				{
					int move = move_from_string(token);
					if (Current->turn) do_move<1>(move); else do_move<0>(move);
					memcpy(Data, Current, sizeof(GData));
					Current = Data;
					SHARED->rootDepth++;
				}
			}
			copy(Stack.begin() + sp - Current->ply, Stack.begin() + sp + 1, Stack.begin());
			sp = Current->ply;
		}
		else if (strcmp(token, "setoption") == 0)
		{
			token = strtok_s(NULL, " ", &saveptr);
			if (token == NULL || strcmp(token, "name") != 0)
				goto bad_command;
			const char *name = strtok_s(NULL, " ", &saveptr);
			if (name == NULL)
				goto bad_command;
			token = strtok_s(NULL, " ", &saveptr);
			if (token == NULL || strcmp(token, "value") != 0)
				goto bad_command;
			if (token == NULL)
				goto bad_command;
			auto settings = *SETTINGS;
			if (strcmp(name, "Hash") == 0)
			{
				size_t n = get_size(strtok_s(NULL, " ", &saveptr));
				size_t nc = (n << 20) / (HASH_CLUSTER * sizeof(GEntry));
				if (nc == 0) nc = 1;	// don't want to deal with degenerate case of no hash
				settings.nHash = Bit(msb(nc)) * HASH_CLUSTER;
				settings.hashMask = settings.nHash - HASH_CLUSTER;
			}
			else if (strcmp(name, "Threads") == 0)
				settings.nThreads = get_number(strtok_s(NULL, " ", &saveptr));
			else if (strcmp(name, "SyzygyPath") == 0)
			{
				if (saveptr != NULL && strlen(saveptr) < sizeof(settings.tbPath) - 1)
					memcpy(settings.tbPath, saveptr, strlen(saveptr) + 1);
			}
			else if (strcmp(name, "Contempt") == 0)
				settings.contempt = get_number(strtok_s(NULL, " ", &saveptr));
			else if (strcmp(name, "Wobble") == 0)
				settings.wobble = get_number(strtok_s(NULL, " ", &saveptr));
			else if (strcmp(name, "SyzygyProbeDepth") == 0)
				settings.tbMinDepth = get_number(strtok_s(NULL, " ", &saveptr));
			else
				goto bad_command;
			reset(settings);
		}
		else if (strcmp(token, "ucinewgame") == 0)
		{
			if (!SHARED->working.Empty())
				continue;
			init_search(true);
			for (int i = 0; i < SETTINGS->nThreads; i++)
				THREADS[i]->newGame = true;
		}
		else if (strcmp(token, "uci") == 0)
		{
			char reply[] =
				"id name Roc            \n"
				"id author Demichev/Falcinelli/Hyer\n"
				"option name Hash type spin min 1 max 8388608 default 128\n"
				"option name Threads type spin min 1 max 64 default 4\n"
				"option name SyzygyPath type string default <empty>\n"
				"option name SyzygyProbeDepth type spin min 0 max 64 "
				"default 1\n"
				"uciok\n";
			char* dst = &reply[12];
			for (const char* src = &Version::now[0]; *src; ++src, ++dst) *dst = *src;
			Say(reply);
		}
		else if (strcmp(token, "quit") == 0)
		{
			nuke_children();
			exit(EXIT_SUCCESS);
		}
		else
		{
		bad_command:
			log_msg("warning: unknown command\n");
		}
	}
}

void worker()
{
	while (true)
	{
		Current = Data;
		wait_for_go();

		try
		{
			bool newGame = INFO->newGame;
			INFO->newGame = false;

			{
				LOCK_SHARED;
				SHARED->working.Insert(INFO->id);
			}

			if (newGame)
				init_search(false);

			memcpy(Board, &SHARED->rootBoard, sizeof(GBoard));
			memcpy(Current, &SHARED->rootData, sizeof(GData));
			memcpy(&Stack[0], &SHARED->rootStack, SHARED->rootSp * sizeof(uint64_t));
			sp = SHARED->rootSp;
			Stack[sp] = Current->key;
			check_node = INFO->nodes + 1024;	// should cause a check within a millisecond

			if (Current->turn == White)
				root<0>();
			else
				root<1>();
		}
		catch (...) 	// reserve the right to use exceptions to halt work
		{
			VSAY("Hard stop of worker " + Str(INFO->id) + " after line " + Str(debug_loc) + "\n");
		}

		LOCK_SHARED;
		SHARED->working.Remove(INFO->id);
		if (SHARED->working.Empty())
			send_best_move(true);
	}
}

void init_os()
{
	HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode;
	if (GetConsoleMode(handle, &mode))
	{
		mode &= ~ENABLE_MOUSE_INPUT;
		mode &= ~ENABLE_WINDOW_INPUT;
		mode |= ENABLE_LINE_INPUT;
		mode |= ENABLE_ECHO_INPUT;
		SetConsoleMode(handle, mode);
		FlushConsoleInputBuffer(handle);
	}

	HANDLE job = CreateJobObject(NULL, NULL);
	if (job == NULL)
		error_msg("failed to create job object (%d)", GetLastError());
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
	memset(&info, 0, sizeof(info));
	info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info,
		sizeof(info));
	AssignProcessToJobObject(job, GetCurrentProcess());     // Allowed to fail
}

#ifndef REGRESSION
int main(int argc, char **argv)
{
	if (argc > 1 && strcmp(argv[1], "child") == 0)
	{
		if (argc != 9)
		{
		usage:
			fprintf(stderr, "usage: %s\n", argv[0]);
			fprintf(stderr, "       %s \"bench\" <depth> FEN1 [FEN2 ...]\n",
				argv[0]);
			exit(EXIT_FAILURE);
		}

		// CHILD:
		const char
			*hashStr = argv[2],
			*pvHashStr = argv[3],
			*pawnHashStr = argv[4],
			*dataStr = argv[5],
			*settingsStr = argv[6],
			*sharedStr = argv[7],
			*infoStr = argv[8];
		RO = DATA = share_object(DATA, dataStr, true);
		SETTINGS = share_object(SETTINGS, settingsStr, true);
		SHARED = share_object(SHARED, sharedStr, false);
		HASH = share_object(HASH, hashStr, false);
		PVHASH = share_object(PVHASH, pvHashStr, false);
		PAWNHASH = share_object(PAWNHASH, pawnHashStr, false);
		INFO = share_object(INFO, infoStr, false);
		INFO->pid = GetCurrentProcessId();
#if TB
		tb_init_fwd(SETTINGS->tbPath);
#endif
		THREADS.resize(SETTINGS->nThreads);
		for (int i = 0; i < SETTINGS->nThreads; i++)
		{
			if (i == INFO->id)
			{
				THREADS[i] = INFO;
				continue;
			}
			string infoName = object_name("INFO", SETTINGS->parentPid, i);
			THREADS[i] = init_object<ThreadOwn_>(nullptr, infoName.c_str(), false, false);
		}
		init_search(false);

		// Signal that we are initialized.
		{
			LOCK_SHARED;
			SHARED->working.Remove(INFO->id);
		}
		try
		{
			worker();   // Wait for work from parent.
		}
		catch (...)
		{
			Say("Unexpected death\n");
			Say("At line " + Str(debugLine) + "\n");
			Say("Worker " + Str(INFO->id) + "\n");
			Say("*********************************************************************************\n");
		}
	}
	else if (argc > 2 && strcmp(argv[1], "bench") == 0)
	{
		const int benchDepth = atoi(argv[2]);
		RO = DATA = init_object(DATA, nullptr, true, false);
		init_data(DATA);
		Settings_ settings;
		settings.nThreads = 1;
		settings.nHash = (1 << 23) / sizeof(GEntry);        // 8MB
		settings.tbPath[0] = 0;
		SETTINGS = init_object(SETTINGS, nullptr, true, true, 1, &settings);
		SHARED = init_object(SHARED, nullptr, true, false);
		SHARED->mutex = mutex_init;
		SHARED->goEvent = event_init();

		HASH = init_object(HASH, nullptr, true, false, SETTINGS->nHash);
		PVHASH = init_object(PVHASH, nullptr, true, false, N_PV_HASH);
		PAWNHASH = init_object(PAWNHASH, nullptr, true, false, N_PAWN_HASH);
		INFO = init_object(INFO, nullptr, true, false);
		INFO->pid = GetCurrentProcessId();
		THREADS[0] = INFO;

		auto t0 = now();
		uint64_t nodes = 0;
		for (int i = 3; i < argc; i++)
		{
			init_search(true);
			if (strcmp(argv[i], "startpos") != 0)
				get_board(argv[i]);
			SHARED->stopAll = false;
			SHARED->depthLimit = 2 * benchDepth + 2;
			SHARED->softTimeLimit = UINT32_MAX;
			SHARED->hardTimeLimit = UINT32_MAX;
			SHARED->startTime = t0;
			if (Current->turn == White) root<0>(); else root<1>();
			send_best_move();
			nodes += INFO->nodes;
		}
		Say("TIME : " + Str(millisecs(t0, now())) + "\nNODES: " + Str(nodes) + "\n");
		exit(EXIT_SUCCESS);
	}
	else if (argc > 1)
		goto usage;

	// PARENT:
	Say("Roc " + string(Version::now) + "\n");
	init_os();

	// Read override parameters from the environment (useful for debugging)
	Settings_ startInfo;
	startInfo.parentPid = GetCurrentProcessId();
	const char *val;
	if ((val = getenv("ROC_HASH")) != NULL)
		startInfo.nHash = atoll(val) / sizeof(GEntry);
	else
		startInfo.nHash = (1 << 27) / sizeof(GEntry);     // 128MB
	startInfo.nHash = Bit(msb(startInfo.nHash));
	startInfo.hashMask = startInfo.nHash - HASH_CLUSTER;
	if ((val = getenv("ROC_THREADS")) != NULL)
		startInfo.nThreads = atoi(val);
	else
		startInfo.nThreads = get_num_cpus();
	if ((val = getenv("ROC_SYZYGY_PATH")) != NULL)
		strncpy(startInfo.tbPath, val, sizeof(startInfo.tbPath) - 1);
	else
		startInfo.tbPath[0] = 0;
	// other settings are fine at defaults
	if ((val = getenv("ROC_TB_MIN_DEPTH")) != NULL)
		startInfo.tbMinDepth = atoi(val);
	if ((val = getenv("ROC_CONTEMPT")) != NULL)
		startInfo.contempt = atoi(val);
	if ((val = getenv("ROC_WOBBLE")) != NULL)
		startInfo.wobble = atoi(val);

	PVN = 1;        // XXX NYI

	create_children(startInfo);
	init_search(false);

	while (true)
		uci();
	return 0;
}
#else
// regression tester
struct WriteMove_
{
	int m_;
	WriteMove_(int m) : m_(m) { HI BYE }
};
ostream& operator<<(ostream& dst, const WriteMove_& m)
{
	dst << static_cast<char>('a' + ((m.m_ >> 6) & 7));
	dst << static_cast<char>('1' + ((m.m_ >> 9) & 7));
	dst << '-';
	dst << static_cast<char>('a' + ((m.m_ >> 0) & 7));
	dst << static_cast<char>('1' + ((m.m_ >> 3) & 7));
	return dst;
}

void Test1(const char* fen, int max_depth, const char* solution)
{
	constexpr int DEPTH_LIMIT = 24;
	max_depth = Min(max_depth, DEPTH_LIMIT);
	init_search(1);
	get_board(fen);
	auto cmd = _strdup("go infinite");
	free(cmd);
	char moveStr[16];
	for (int depth = Min(4, max_depth); depth <= max_depth; ++depth)
	{
		auto score = Current->turn
			? pv_search<true, true>(-MateValue, MateValue, depth, FlagNeatSearch)
			: pv_search<false, true>(-MateValue, MateValue, depth, FlagNeatSearch);
		move_to_string(INFO->bestMove, moveStr);
		Say(string(moveStr) + ":  " + Str(score) + ", " + Str(INFO->nodes) + " nodes\n");
	}
	cin.ignore();
}

int main(int argc, char *argv[])
{
	int CPUInfo[4] = { -1, 0, 0, 0 };
	__cpuid(CPUInfo, 1);
	HardwarePopCnt = (CPUInfo[2] >> 23) & 1;

	init_os();

	// Read override parameters from the environment (useful for debugging)
	Settings_ startInfo;
	startInfo.parentPid = GetCurrentProcessId();
	startInfo.nHash = (1 << 24) / sizeof(GEntry);     // 16MB
	startInfo.nHash = Bit(msb(startInfo.nHash));
	startInfo.hashMask = startInfo.nHash - HASH_CLUSTER;
	startInfo.nThreads = get_num_cpus();
	startInfo.tbPath[0] = 0;
	PVN = 1;        // XXX NYI

	create_children(startInfo);
	init_search(false);

	fprintf(stdout, "Roc (regression mode)\n");

	Test1("R7/6k1/8/8/6P1/6K1/8/7r w - - 0 1", 24, "g4-g5");
	Test1("kr6/p7/8/8/8/8/8/BBK5 w - - 0 1", 24, "g4-g5");

	Test1("4kbnr/2pr1ppp/p1Q5/4p3/4P3/2Pq1b1P/PP1P1PP1/RNB2RK1 w - - 0 1", 20, "f1-e1");	// why didn't Roc keep the rook pinned?

	Test1("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1", 20, "a7-a8");
	Test1("4r1k1/4ppbp/r5p1/3Np3/2PnP3/3P2Pq/1R3P2/2BQ1RK1 w - - 0 1", 20, "b2-b1");

	Test1("8/4p3/8/3P3p/P2pK3/6P1/7b/3k4 w - - 0 1", 24, "d5-d6");
	Test1("rq2r1k1/5pp1/p7/4bNP1/1p2P2P/5Q2/PP4K1/5R1R w - - 0 1", 4, "f5-g7");
	Test1("6k1/2b2p1p/ppP3p1/4p3/PP1B4/5PP1/7P/7K w - - 0 1", 31, "d4-b6");			// Slizzard fails
	Test1("5r1k/p1q2pp1/1pb4p/n3R1NQ/7P/3B1P2/2P3P1/7K w - - 0 1", 26, "e5-e6");	// Slizzard finds at depth 21
	Test1("5r1k/1P4pp/3P1p2/4p3/1P5P/3q2P1/Q2b2K1/B3R3 w - - 0 1", 36, "a2-f7");	// Slizzard finds at depth 35
	Test1("3B4/8/2B5/1K6/8/8/3p4/3k4 w - - 0 1", 15, "b5-a6");						// Slizzard fails until depth 17
	Test1("1k1r4/1pp4p/2n5/P6R/2R1p1r1/2P2p2/1PP2B1P/4K3 b - - 0 1", 18, "e4-e3");
	Test1("6k1/p3q2p/1nr3pB/8/3Q1P2/6P1/PP5P/3R2K1 b - - 0 1", 12, "c6-d6");
	Test1("2krr3/1p4pp/p1bRpp1n/2p5/P1B1PP2/8/1PP3PP/R1K3B1 w - - 0 1", 15, "d6-c6");
	Test1("r5k1/pp2p1bp/6p1/n1p1P3/2qP1NP1/2PQB3/P5PP/R4K2 b - - 0 1", 18, "g6-g5");
	Test1("2r3k1/1qr1b1p1/p2pPn2/nppPp3/8/1PP1B2P/P1BQ1P2/5KRR w - - 0 1", 25, "g1-g7");
	Test1("1br3k1/p4p2/2p1r3/3p1b2/3Bn1p1/1P2P1Pq/P3Q1BP/2R1NRK1 b - - 0 1", 20, "h3-h2");
	Test1("8/pp3k2/2p1qp2/2P5/5P2/1R2p1rp/PP2R3/4K2Q b - - 0 1", 11, "e6-e4");
	Test1("3b2k1/1pp2rpp/r2n1p1B/p2N1q2/3Q4/6R1/PPP2PPP/4R1K1 w - - 0 1", 15, "d5-b4");
	Test1("3r1rk1/1p3pnp/p3pBp1/1qPpP3/1P1P2R1/P2Q3R/6PP/6K1 w - - 0 1", 24, "h3-h7");
	Test1("4k1rr/ppp5/3b1p1p/4pP1P/3pP2N/3P3P/PPP5/2KR2R1 w kq - 0 1", 16, "g1-g6");
	Test1("r1b3k1/ppp3pp/2qpp3/2r3N1/2R5/8/P1Q2PPP/2B3K1 b - - 0 1", 4, "g7-g6");
	Test1("4r1k1/p1qr1p2/2pb1Bp1/1p5p/3P1n1R/3B1P2/PP3PK1/2Q4R w - - 0 1", 20, "c1-f4");
	Test1("3r2k1/pp4B1/6pp/PP1Np2n/2Pp1p2/3P2Pq/3QPPbP/R4RK1 b - - 0 1", 42, "g3-f3");	// fails to find f3
	Test1("r4rk1/5p2/1n4pQ/2p5/p5P1/P4N2/1qb1BP1P/R3R1K1 w - - 0 1", 23, "a1-a2");
	Test1("r4rk1/pb3p2/1pp4p/2qn2p1/2B5/6BP/PPQ2PP1/3RR1K1 w - - 0 1", 15, "e1-e6");
	Test1("rnb1k2r/pp2qppp/3p1n2/2pp2B1/1bP5/2N1P3/PP2NPPP/R2QKB1R w KQkq - 0 1", 4, "a2-a3");
	Test1("r1b2rk1/pp1p1pBp/6p1/8/2PQ4/8/PP1KBP1P/q7 w - - 0 1", 30, "d4-f6");
	Test1("R7/3p3p/8/3P2P1/3k4/1p5p/1P1NKP1P/7q w - - 0 1", 31, "g5-g6");
	Test1("8/8/3k1p2/p2BnP2/4PN2/1P2K1p1/8/5b2 b - - 0 1", 57, "f4-d3");	// fails to find Nd3 (finds at 58 in about 1800 seconds)
	Test1("2r3k1/pbr1q2p/1p2pnp1/3p4/3P1P2/1P1BR3/PB1Q2PP/5RK1 w - - 0 1", 31, "f4-f5");
	Test1("3r2k1/p2r2p1/1p1B2Pp/4PQ1P/2b1p3/P3P3/7K/8 w - - 0 1", 39, "d6-b4");
	Test1("b2r1rk1/2q2ppp/p1nbpn2/1p6/1P6/P1N1PN2/1B2QPPP/1BR2RK1 w - - 0 1", 4, "c3-e4");
	Test1("r1b4Q/p4k1p/1pp1ppqn/8/1nP5/8/PP1KBPPP/3R2NR w - - 0 1", 4, "e5-e6");
	Test1("2k5/2p3Rp/p1pb4/1p2p3/4P3/PN1P1P2/1P2KP1r/8 w - - 0 1", 25, "f3-f4");

	cin.ignore();
	return 0;
}
#endif

#if TB
#pragma optimize("gy", off)
#define Say(x)	// mute TB query
// Fathom/Syzygy code at end where its #defines cannot screw us up
#undef LOCK
#undef UNLOCK
#include "src\tbconfig.h"
#include "src\tbcore.h"
#include "src\tbprobe.c"
#pragma optimize("", on)

int GetTBMove(unsigned res, int* best_score)
{
	*best_score = TbValues[TB_GET_WDL(res)];
	int retval = (TB_GET_FROM(res) << 6) | TB_GET_TO(res);
	switch (TB_GET_PROMOTES(res))
	{
	case TB_PROMOTES_QUEEN:
		return retval | FlagPQueen;
	case TB_PROMOTES_ROOK:
		return retval | FlagPRook;
	case TB_PROMOTES_BISHOP:
		return retval | FlagPBishop;
	case TB_PROMOTES_KNIGHT:
		return retval | FlagPKnight;
	}
	return retval;
}
#undef LOCK
#undef UNLOCK

unsigned tb_probe_root_fwd(
	uint64_t _white,
	uint64_t _black,
	uint64_t _kings,
	uint64_t _queens,
	uint64_t _rooks,
	uint64_t _bishops,
	uint64_t _knights,
	uint64_t _pawns,
	unsigned _rule50,
	unsigned _ep,
	bool     _turn)
{
	return tb_probe_root_impl(_white, _black, _kings, _queens, _rooks, _bishops, _knights, _pawns, _rule50, _ep, _turn, nullptr);
}

bool tb_init_fwd(const char* path)
{
	return tb_init(path);
}

#endif
#pragma optimize("", on)
