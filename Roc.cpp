//#define REGRESSION
//#define W32_BUILD
#define _CRT_SECURE_NO_WARNINGS
//#define CPU_TIMING
//#define TUNER
//#define EXPLAIN_EVAL
//#define TWO_PHASE
//#define THREE_PHASE
#define LARGE_PAGES
#define MP_NPS
//#define TIME_TO_DEPTH
#define TB 1
//#define HNI

#ifdef W32_BUILD
#define NTDDI_VERSION 0x05010200
#define _WIN32_WINNT 0x0501
#endif

#include <iostream>
#include <fstream>
#include <array>
#include <numeric>
#include <string>
#include <thread>
#include "setjmp.h"
#include <windows.h>
#include <intrin.h>
#include <assert.h>

//#include "TunerParams.inc"


#ifdef TUNER
#include "time.h"
//#define PGN
#define RANDOM_SPHERICAL
//#define WIN_PR
//#define TIMING
//#define RECORD_GAMES
#endif

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
template <class T> INLINE int Sgn(const T& x)
{
	return x == 0 ? 0 : (x > 0 ? 1 : -1);
}
template <class T> INLINE const T& Min(const T& x, const T& y)
{
	return x < y ? x : y;
}
template <class T> INLINE const T& Max(const T& x, const T& y)
{
	return x < y ? y : x;
}
template <class T> INLINE T Square(const T& x)
{
	return x * x;
}
template <class C> INLINE bool T(const C& x)
{
	return x != 0;
}
template <class T> INLINE bool F(const T& x)
{
	return x == 0;
}
template <class T> INLINE bool Even(const T& x)
{
	return F(x & 1);
}
template <class C> INLINE bool Odd(const C& x)
{
	return T(x & 1);
}

#if TB
constexpr sint16 TBMateValue = 31380;
constexpr sint16 TBCursedMateValue = 13;
constexpr array<int, 5> TbValues = { -TBMateValue, -TBCursedMateValue, 0, TBCursedMateValue, TBMateValue };
constexpr int NominalTbDepth = 33;
INLINE int TbDepth(int depth) { return Min(depth + NominalTbDepth, 117); }

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

inline unsigned tb_probe_root_checked(
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

INLINE constexpr packed_t Pack4(sint16 op, sint16 mid, sint16 eg, sint16 asym)
{
	return op + (static_cast<sint64>(mid) << 16) + (static_cast<sint64>(eg) << 32) + (static_cast<sint64>(asym) << 48);
}
INLINE constexpr packed_t Pack2(sint16 x, sint16 y)
{
	return Pack4(x, (x + y) / 2, y, 0);
}

INLINE sint16 Opening(packed_t x) { return static_cast<sint16>(x & 0xFFFF); }
INLINE sint16 Middle(packed_t x) { return static_cast<sint16>((x >> 16) + ((x >> 15) & 1)); }
INLINE sint16 Endgame(packed_t x) { return static_cast<sint16>((x >> 32) + ((x >> 31) & 1)); }
INLINE sint16 Closed(packed_t x) { return static_cast<sint16>((x >> 48) + ((x >> 47) & 1)); }

#endif


// unsigned for king_att
INLINE constexpr uint32 UPack(uint16 x, uint16 y)
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


INLINE int FileOf(int loc)
{
	return loc & 7;
}
INLINE int RankOf(int loc)
{
	return loc >> 3;
}
template <bool me> INLINE int OwnRank(int loc)
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
INLINE int Dist(int x, int y)
{
	return Max(abs(RankOf(x) - RankOf(y)), abs(FileOf(x) - FileOf(y)));
}

INLINE uint64 Bit(int loc)
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
template<int DEN, int SINK> struct Div_
{
	int operator()(int x) const
	{
		constexpr int shift = std::numeric_limits<int>::max / (2 * DEN);
		constexpr int shrink = (SINK - DEN) / 2;
		x = x > 0 ? max(0, x - shrink) : min(0, x + shrink);
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
template <bool me> INLINE uint8 Promotion(uint16 move)
{
	return (me ? 1 : 0) + ((move & 0xF000) >> 12);
}

constexpr array<uint64, 64> BMagic = { 0x0048610528020080, 0x00c4100212410004, 0x0004180181002010, 0x0004040188108502, 0x0012021008003040, 0x0002900420228000,
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

constexpr array<uint64, 64> RMagic = { 0x00800011400080a6, 0x004000100120004e, 0x0080100008600082, 0x0080080016500080, 0x0080040008000280, 0x0080020005040080,
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

constexpr array<short, 64> BShift = { 58, 59, 59, 59, 59, 59, 59, 58, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 57, 57, 57, 57, 59, 59, 59, 59, 57, 55, 55, 57, 59, 59,
59, 59, 57, 55, 55, 57, 59, 59, 59, 59, 57, 57, 57, 57, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 58, 59, 59, 59, 59, 59, 59, 58 };

constexpr array<short, 64> BOffset = { 0,    64,   96,   128,  160,  192,  224,  256,  320,  352,  384,  416,  448,  480,  512,  544,  576,  608,  640,  768,  896,  1024,
1152, 1184, 1216, 1248, 1280, 1408, 1920, 2432, 2560, 2592, 2624, 2656, 2688, 2816, 3328, 3840, 3968, 4000, 4032, 4064, 4096, 4224,
4352, 4480, 4608, 4640, 4672, 4704, 4736, 4768, 4800, 4832, 4864, 4896, 4928, 4992, 5024, 5056, 5088, 5120, 5152, 5184 };

constexpr array<short, 64> RShift = { 52, 53, 53, 53, 53, 53, 53, 52, 53, 54, 54, 54, 54, 54, 54, 53, 53, 54, 54, 54, 54, 54, 54, 53, 53, 54, 54, 54, 54, 54, 54, 53,
53, 54, 54, 54, 54, 54, 54, 53, 53, 54, 54, 54, 54, 54, 54, 53, 53, 54, 54, 54, 54, 54, 54, 53, 52, 53, 53, 53, 53, 53, 53, 52 };

constexpr array<int, 64> ROffset = { 5248,  9344,  11392, 13440, 15488, 17536, 19584, 21632, 25728, 27776, 28800, 29824, 30848, 31872, 32896,  33920,
35968, 38016, 39040, 40064, 41088, 42112, 43136, 44160, 46208, 48256, 49280, 50304, 51328, 52352, 53376,  54400,
56448, 58496, 59520, 60544, 61568, 62592, 63616, 64640, 66688, 68736, 69760, 70784, 71808, 72832, 73856,  74880,
76928, 78976, 80000, 81024, 82048, 83072, 84096, 85120, 87168, 91264, 93312, 95360, 97408, 99456, 101504, 103552 };
uint64* BOffsetPointer[64];
uint64* ROffsetPointer[64];

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
constexpr uint64 FileA = 0x0101010101010101;
constexpr array<uint64, 8> File = { FileA, FileA << 1, FileA << 2, FileA << 3, FileA << 4, FileA << 5, FileA << 6, FileA << 7 };
constexpr uint64 Line0 = 0x00000000000000FF;
constexpr array<uint64, 8> Line = { Line0, (Line0 << 8), (Line0 << 16), (Line0 << 24), (Line0 << 32), (Line0 << 40), (Line0 << 48), (Line0 << 56) };

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
template <bool me> INLINE uint64 ShiftW(const uint64& target)
{
	return me ? ShiftSW(target) : ShiftNW(target);
}
template <bool me> INLINE uint64 ShiftE(const uint64& target)
{
	return me ? ShiftSE(target) : ShiftNE(target);
}
template <bool me> INLINE uint64 Shift(const uint64& target)
{
	return me ? target >> 8 : target << 8;
}
constexpr array<int, 2> PushW = { 7, -9 };
constexpr array<int, 2> Push = { 8, -8 };
constexpr array<int, 2> PushE = { 9, -7 };

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
constexpr int InitiativeConst = 2 * CP_SEARCH;
constexpr int InitiativePhase = 2 * CP_SEARCH;
constexpr int FutilityThreshold = 50 * CP_SEARCH;

#ifdef EXPLAIN_EVAL
FILE* fexplain;
int explain = 0, cpp_length;
char GullCppFile[16384][256];
#define IncV(var, x)                                                                                                                                         \
    (explain ? (me ? ((var -= (x)) &&                                                                                                                        \
                      fprintf(fexplain, "(%d, %d): %s [Black]: %s", Opening(-(x)), Endgame(-(x)), __FUNCTION__, GullCppFile[Min(__LINE__ - 1, cpp_length)])) \
                   : ((var += (x)) &&                                                                                                                        \
                      fprintf(fexplain, "(%d, %d): %s [White]: %s", Opening(x), Endgame(x), __FUNCTION__, GullCppFile[Min(__LINE__ - 1, cpp_length)])))      \
             : (me ? (var -= (x)) : (var += (x))))
#else
#define IncV(var, x) (me ? (var -= (x)) : (var += (x)))
#endif
#define DecV(var, x) IncV(var, -(x))

constexpr sint16 KpkValue = 300 * CP_EVAL;
constexpr sint16 EvalValue = 30000;
constexpr sint16 MateValue = 32760 - 8 * (CP_SEARCH - 1);


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
const int MvvLvaVictim[16] = { 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 3, 3, 3, 3 };
const int MvvLvaAttacker[16] = { 0, 0, 5, 5, 4, 4, 3, 3, 3, 3, 2, 2, 1, 1, 6, 6 };
const int MvvLvaAttackerKB[16] = { 0, 0, 9, 9, 7, 7, 5, 5, 5, 5, 3, 3, 1, 1, 11, 11 };

#define PawnCaptureMvvLva(attacker) (MvvLvaAttacker[attacker])
#define MaxPawnCaptureMvvLva (MvvLvaAttacker[15])  // 6
#define KnightCaptureMvvLva(attacker) (MaxPawnCaptureMvvLva + MvvLvaAttackerKB[attacker])
#define MaxKnightCaptureMvvLva (MaxPawnCaptureMvvLva + MvvLvaAttackerKB[15])  // 17
#define BishopCaptureMvvLva(attacker) (MaxPawnCaptureMvvLva + MvvLvaAttackerKB[attacker] + 1)
#define MaxBishopCaptureMvvLva (MaxPawnCaptureMvvLva + MvvLvaAttackerKB[15] + 1)  // usually 18
#define RookCaptureMvvLva(attacker) (MaxBishopCaptureMvvLva + MvvLvaAttacker[attacker])
#define MaxRookCaptureMvvLva (MaxBishopCaptureMvvLva + MvvLvaAttacker[15])  // usually 24
#define QueenCaptureMvvLva(attacker) (MaxRookCaptureMvvLva + MvvLvaAttacker[attacker])

#define MvvLvaPromotion (MvvLva[WhiteQueen][BlackQueen])
#define MvvLvaPromotionKnight (MvvLva[WhiteKnight][BlackKnight])
#define MvvLvaPromotionCap(capture) (MvvLva[((capture) < WhiteRook) ? WhiteRook : ((capture) >= WhiteQueen ? WhiteKing : WhiteKnight)][BlackQueen])
#define MvvLvaPromotionKnightCap(capture) (MvvLva[WhiteKing][capture])
#define MvvLvaXray (MvvLva[WhiteQueen][WhitePawn])
#define MvvLvaXrayCap(capture) (MvvLva[WhiteKing][capture])
constexpr int RefOneScore = (0xFF << 16) | (3 << 24);
constexpr int RefTwoScore = (0xFF << 16) | (2 << 24);

#define halt_check                         \
    if ((Current - Data) >= 126)           \
    {                                      \
        evaluate();                        \
        return Current->score;             \
    }                                      \
    if (Current->ply >= 100)               \
        return 0;                          \
    for (int i = 4; i <= Current->ply; i += 2) \
        if (Stack[sp - i] == Current->key) \
    return 0
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
uint64 nodes, tb_hits, check_node, check_node_smp;
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
	uint8 turn, castle_flags, ply, ep_square, capture, gen_flags, piece, stage, mul, dummy;
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

typedef struct
{
	uint32 key;
	uint16 date;
	uint16 move;
	score_t low;
	score_t high;
	uint16 flags;
	uint8 low_depth;
	uint8 high_depth;
} GEntry;
constexpr GEntry NullEntry = { 0, 1, 0, 0, 0, 0, 0, 0 };
#ifndef TUNER
constexpr int initial_hash_size = 1024 * 1024;
#else
constexpr int initial_hash_size = 64 * 1024;
GEntry HashOne[initial_hash_size];
GEntry HashTwo[initial_hash_size];
#endif
sint64 hash_size = initial_hash_size;
uint64 hash_mask = (initial_hash_size - 4);
GEntry* Hash;

struct GPawnEntry
{
	uint64 key;
	packed_t score;
	array<sint16, 2> shelter;
	array<uint8, 2> passer, draw;
};
constexpr GPawnEntry NullPawnEntry = { 0, 0, {0, 0}, {0, 0}, {0, 0} };
#ifndef TUNER
constexpr int pawn_hash_size = 1 << 20;
__declspec(align(64)) array<GPawnEntry, pawn_hash_size> PawnHash;
#else
constexpr int pawn_hash_size = 1 << 15;
__declspec(align(64)) GPawnEntry PawnHashOne[pawn_hash_size];
__declspec(align(64)) GPawnEntry PawnHashTwo[pawn_hash_size];
GPawnEntry* PawnHash = PawnHashOne;
#endif
constexpr int pawn_hash_mask = pawn_hash_size - 1;

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
#ifndef TUNER
constexpr int pv_hash_size = 1 << 20;
#else
constexpr int pv_hash_size = 1 << 14;
GPVEntry PVHashOne[pv_hash_size];
GPVEntry PVHashTwo[pv_hash_size];
#endif
constexpr int pv_cluster_size = 1 << 2;
constexpr int pv_hash_mask = pv_hash_size - pv_cluster_size;
GPVEntry* PVHash = nullptr;

array<int, 256> RootList;

template<class T> void prefetch(T* p)
{
	_mm_prefetch(reinterpret_cast<char*>(p), _MM_HINT_NTA);
}

uint64 Forward[2][8];
uint64 West[8];
uint64 East[8];
uint64 PIsolated[8];
uint64 VLine[64];
uint64 RMask[64];
uint64 BMask[64];
uint64 QMask[64];
uint64 BMagicMask[64];
uint64 RMagicMask[64];
uint64 NAtt[64];
uint64 KAtt[64];
uint64 KAttAtt[64];
uint64 NAttAtt[64];
uint64 BishopForward[2][64];
uint64 PAtt[2][64];
uint64 PSupport[2][64];
uint64 PMove[2][64];
uint64 PWay[2][64];
uint64 PCone[2][64];	// wider than PWay
uint64 Between[64][64];
uint64 FullLine[64][64];


#ifndef HNI
uint64 BishopAttacks(int sq, const uint64& occ)
{
	return *(BOffsetPointer[sq] + (((BMagicMask[sq] & occ) * BMagic[sq]) >> BShift[sq]));
}
uint64 RookAttacks(int sq, const uint64& occ)
{
	return *(ROffsetPointer[sq] + (((RMagicMask[sq] & occ) * RMagic[sq]) >> RShift[sq]));
}
#else
#define BishopAttacks(sq, occ) (*(BOffsetPointer[sq] + _pext_u64(occ, BMagicMask[sq])))
#define RookAttacks(sq, occ) (*(ROffsetPointer[sq] + _pext_u64(occ, RMagicMask[sq])))
#endif
uint64 QueenAttacks(int sq, const uint64& occ)
{
	return BishopAttacks(sq, occ) | RookAttacks(sq, occ);
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

struct GMaterial;
struct GPawnEntry;
struct GEvalInfo
{
	uint64 occ, area[2], free[2], klocus[2];
	GPawnEntry* PawnEntry;
	GMaterial* material;
	packed_t score;
	uint32 king_att[2];
	int king[2], mul;
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
#ifdef TUNER
	uint32 generation;
#endif
};
GMaterial* Material;
constexpr int FlagSingleBishop[2] = { 1, 2 };
constexpr int FlagCallEvalEndgame[2] = { 4, 8 };
#ifndef TUNER
array<packed_t, 16 * 64> PstVals;
#else
packed_t PstOne[16 * 64];
packed_t PstTwo[16 * 64];
packed_t* PstVals = PstOne;
#endif
packed_t& Pst(int piece, int sq)
{
	return PstVals[(piece << 6) | sq];
};
// #define Pst(piece,sq) PstVals[((piece) << 6) | (sq)]
int MvvLva[16][16];  // [piece][capture]
uint64 TurnKey;
uint64 PieceKey[16][64];
uint64 CastleKey[16];
uint64 EPKey[8];
uint16 date;

uint64 Kpk[2][64][64];

#ifndef TUNER
uint16 HistoryVals[2 * 16 * 64];
#else
uint16 HistoryOne[2 * 16 * 64];
uint16 HistoryTwo[2 * 16 * 64];
uint16* HistoryVals = HistoryOne;
#endif

INLINE int* AddMove(int* list, int from, int to, int flags, int score)
{
	*list = ((from) << 6) | (to) | (flags) | (score);
	return ++list;
}
INLINE int* AddCapturePP(int* list, int att, int vic, int from, int to, int flags)
{
	return AddMove(list, from, to, flags, MvvLva[att][vic]);
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
	return HistoryVals[((join) << 10) | ((piece) << 6) | (to)];
}
INLINE int HistoryMerit(int hs)
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

#ifndef TUNER
sint16 DeltaVals[16 * 4096];
#else
sint16 DeltaOne[16 * 4096];
sint16 DeltaTwo[16 * 4096];
sint16* DeltaVals = DeltaOne;
#endif
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
#ifndef TUNER
GRef Ref[16 * 64];
#else
GRef RefOne[16 * 64];
GRef RefTwo[16 * 64];
GRef* Ref = RefOne;
#endif
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

uint8 PieceFromChar[256];
uint16 PV[MAX_HEIGHT];
char info_string[1024];
char pv_string[1024];
char score_string[16];
char mstring[65536];
int MultiPV[256];
int pvp;
int pv_length;
int best_move, best_score;
int TimeLimit1, TimeLimit2, Console, HardwarePopCnt;
int DepthLimit, LastDepth, LastTime, LastValue, LastExactValue, PrevMove, InstCnt;
sint64 LastSpeed;
int PVN, Contempt, Wobble, Stop, Print, Input = 1, PVHashing = 1, Infinite, MoveTime, SearchMoves, SMPointer, Ponder, Searching, Previous;
typedef struct
{
	int Bad, Change, Singular, Early, FailLow, FailHigh;
} GSearchInfo;
GSearchInfo CurrentSI[1], BaseSI[1];
#ifdef CPU_TIMING
int CpuTiming = 0, UciMaxDepth = 0, UciMaxKNodes = 0, UciBaseTime = 1000, UciIncTime = 5;
int GlobalTime[2] = { 0, 0 };
int GlobalInc[2] = { 0, 0 };
int GlobalTurn = 0;
constexpr sint64 CyclesPerMSec = 3400000;
#endif
int Aspiration = 1, LargePages = 1;
constexpr int TimeSingTwoMargin = 20;
constexpr int TimeSingOneMargin = 30;
constexpr int TimeNoPVSCOMargin = 60;
constexpr int TimeNoChangeMargin = 70;
constexpr int TimeRatio = 120;
constexpr int PonderRatio = 120;
constexpr int MovesTg = 30;
constexpr int InfoLag = 5000;
constexpr int InfoDelay = 1000;
sint64 StartTime, InfoTime, CurrTime;
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
INLINE int ExclSinglePV(int depth)
{
	return 8 * CP_SEARCH;
}
INLINE int ExclDoublePV(int depth)
{
	return 16 * CP_SEARCH;
}

// EVAL

constexpr array<sint8, 8> DistC = { 3, 2, 1, 0, 0, 1, 2, 3 };
constexpr array<sint8, 8> RankR = { -3, -2, -1, 0, 1, 2, 3, 4 };

constexpr array<uint16, 16> SeeValue = { 0, 0, 360, 360, 1300, 1300, 1300, 1300, 1300, 1300, 2040, 2040, 3900, 3900, 30000, 30000 };
constexpr array<int, 16> PieceType = { 0, 0, 0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 4, 4, 5, 5 };
constexpr array<int, 5> Phase = { 0, SeeValue[4], SeeValue[6], SeeValue[10], SeeValue[12] };
constexpr int PhaseMin = 2 * Phase[3] + Phase[1] + Phase[2];
constexpr int PhaseMax = 16 * Phase[0] + 3 * Phase[1] + 3 * Phase[2] + 4 * Phase[3] + 2 * Phase[4];

#ifndef TUNER
#define V(x) (x)
#else
constexpr int MaxVariables = 1024;
int var_number, active_vars;
typedef struct
{
	char line[256];
} GString;
GString SourceFile[1000], VarName[1000];
int VarIndex[1000];
int src_str_num = 0, var_name_num = 0;
int Variables[MaxVariables];
uint8 Active[MaxVariables];
double Var[MaxVariables], Base[MaxVariables], FE[MaxVariables], SE[MaxVariables], Grad[MaxVariables];
#define V(x) Variables[x]
double EvalOne[MaxVariables], EvalTwo[MaxVariables];
int RecordGames = 0;
char RecordString[65536], PosStr[256], *Buffer;
FILE* frec;
#endif

#define ArrayIndex(width, row, column) (((row) * (width)) + (column))
#ifndef TUNER
#define Av(x, width, row, column) (x)[ArrayIndex(width, row, column)]
#else
#define Av(x, width, row, column) V((I##x) + ArrayIndex(width, row, column))
#endif
#define TrAv(x, w, r, c) Av(x, 0, 0, (((r) * (2 * (w) - (r) + 1)) / 2) + (c))

#define Sa(x, y) Av(x, 0, 0, y)
template<class C_> INLINE packed_t Ca4(const C_& x, int y)
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

constexpr array<int, 6> MatLinear = { 29, -5, -12, 88, -19, -3 };
// pawn, knight, bishop, rook, queen, pair
constexpr array<int, 14> MatQuadMe = { // tuner: type=array, var=1000, active=0
	-33, 17, -23, -155, -247,
	15, 296, -105, -83,
	-162, 327, 315,
	-861, -1013
};
constexpr array<int, 10> MatQuadOpp = { // tuner: type=array, var=1000, active=0
	-14, 47, -20, -278,
	35, 39, 49,
	9, -2,
	75
};
constexpr array<int, 9> BishopPairQuad = { // tuner: type=array, var=1000, active=0
	-38, 164, 99, 246, -84, -57, -184, 88, -186
};
constexpr array<int, 6> MatClosed = { -17, 26, -27, 17, -4, 20 };

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
constexpr array<int, 44> MatSpecial = {  // tuner: type=array, var=120, active=0
	52, 0, -52, 0,
	40, 2, -36, 0,
	32, 40, 48, 0,
	16, 20, 24, 0,
	20, 28, 36, 0,
	-12, -22, -32, 0,
	-16, 6, 28, 0,
	8, 4, 0, 0,
	0, -12, -24, 0,
	4, 8, 12, 0,
	0, 0, 0, -100};

// piece type (6) * direction (4: h center dist, v center dist, diag dist, rank) * phase (3)
constexpr array<int, 96> PstQuadWeights = {  // tuner: type=array, var=256, active=0
	-60, -68, -76, 0,
	-280, -166, -52, 0,
	132, 26, -80, 0,
	0, 394, 788, 0,
	-144, -316, -488, 0,
	0, -120, -240, 0,
	-32, -22, -12, 0,
	-68, -90, -112, 0,
	-108, -180, -252, 0,
	-68, -48, -28, 0,
	56, 28, 0, 0,
	-96, -58, -20, 0,
	-256, -132, -8, 0,
	0, -76, -152, 0,
	-32, -16, 0, 0,
	308, 176, 44, 0,
	-268, -128, 12, 0,
	-16, -192, -368, 0,
	-8, 20, 48, 0,
	-52, -110, -168, 0,
	-252, -294, -336, 0,
	-700, -434, -168, 0,
	-8, -38, -68, 0,
	160, 42, -76, 0
};

constexpr array<int, 96> PstLinearWeights = {  // tuner: type=array, var=1280, active=0
	-428, -80, 268, 0,
	-460, -64, 332, 0,
	-220, 24, 268,  0,
	368, 1070, 1772,  0,
	-708, -344, 20,  0,
	-328, -286, -244,  0,
	-424, -420, -416,  0,
	1092, 806, 520, 0,
	0, -290, -580,  0,
	-420, -326, -232, 0,
	-396, -272, -148,  0,
	-532, -188, 56,  0,
	-740, -456, -172,  0,
	-268, -240, -212,  0,
	212, -24, -260,  0,
	696, 616, 536, 0,
	-516, -244, 28,  0,
	392, -266, -924,  0,
	428, 134, -160,  0,
	-108, 568, 1244,  0,
	1024, 278, -468,  0,
	3252, 1264, -724,  0,
	8, -426, -860,  0,
	-176, 600, 1376, 0 };

// piece type (6) * type (2: h * v, h * rank) * phase (3)
constexpr array<int, 48> PstQuadMixedWeights = {  // tuner: type=array, var=256, active=0
	56, 16, -24,  0,
	4, -6, -16,  0,
	-32, -20, -8,  0,
	16, 0, -16, 0,
	4, -12, -28,  0,
	-48, -24, 0,  0,
	-8, -6, -4,  0,
	-20, -2, 16, 0,
	20, -10, -40,  0,
	0, 8, 16,  0,
	-8, 6, 20,  0,
	16, 4, -8, 0
};

// coefficient (Linear, Log, Locus) * phase (4)
constexpr array<int, 12> MobCoeffsKnight = { 1281, 857, 650, 18, 2000, 891, 89, -215, 257, 289, -47, 178 };
constexpr array<int, 12> MobCoeffsBishop = { 1484, 748, 558, 137, 1687, 1644, 1594, -580, -96, 437, 136, 502 };
constexpr array<int, 12> MobCoeffsRook = { 1096, 887, 678, 22, -565, 248, 1251, 7, 64, 59, 53, -15 };
constexpr array<int, 12> MobCoeffsQueen = { 597, 876, 1152, 16, 1755, 324, -1091, 8, 65, 89, 20, -18 };

constexpr int N_LOCUS = 22;

array<array<packed_t, 9>, 2> MobKnight;
array<array<packed_t, 15>, 2> MobBishop, MobRook;
array<array<packed_t, 28>, 2> MobQueen;
array<uint64, 64> KingLocus;

// file type (3) * distance from 2d rank/open (5)
constexpr array<int, 15> ShelterValue = {  // tuner: type=array, var=26, active=0
	8, 36, 44, 0, 0,	// h-pawns
	48, 72, 44, 0, 8,	// g
	96, 28, 32, 0, 0	// f
};
array<array<sint16, 8>, 3> Shelter;

enum
{
	StormHofValue,
	StormHofScale,
	StormOfValue,
	StormOfScale
};
constexpr array<int, 4> ShelterMod = { 0, 0, 88, 0 };

enum
{
	StormBlockedMul,
	StormShelterAttMul,
	StormConnectedMul,
	StormOpenMul,
	StormFreeMul
};
constexpr array<int, 5> StormQuad = {  // tuner: type=array, var=640, active=0
	504, 1312, 1852, 860, 356
};
constexpr array<int, 5> StormLinear = {  // tuner: type=array, var=1280, active=0
	332, 624, 1752, 1284, 48
};
array<sint16, 4> StormBlocked;
array<sint16, 4> StormShelterAtt;
array<sint16, 4> StormConnected;
array<sint16, 4> StormOpen;
array<sint16, 4> StormFree;

// type (9: general, blocked, free, supported, protected, connected, outside, candidate, clear) * phase (4)
constexpr array<int, 36> PasserQuad = {  // tuner: type=array, var=128, active=0
	76, 64, 52, 0,
	84, 48, 12, 0,
	-96, 204, 504, 0,
	0, 130, 260,  0,
	128, 176, 224,  0,
	108, 44, -20,  0,
	128, 32, -64, 0,
	52, 34, 16,  0,
	4, 4, 4, 0 };
constexpr array<int, 36> PasserLinear = {  // tuner: type=array, var=512, active=0
	164, 86, 8, 0,
	444, 394, 344, 0,
	712, 582, 452, 0,
	808, 434, 60, 0,
	-244, -80, 84, 0,
	372, 518, 664, 0,
	344, 356, 368, 0,
	108, 122, 136, 0,
	-72, -50, -28, 0 };
constexpr array<int, 36> PasserConstant = {  // tuner: type=array, var=2048, active=0
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
constexpr array<int, 4> PasserAttDefQuad = { // tuner: type=array, var=500, active=0
	764, 204, 332, 76
};
constexpr array<int, 4> PasserAttDefLinear = { // tuner: type=array, var=500, active=0
	2536, 16, 932, 264
};
constexpr array<int, 4> PasserAttDefConst = { // tuner: type=array, var=500, active=0
	0, 0, 0, 0
};
enum { PasserOnePiece, PasserOpKingControl, PasserOpMinorControl, PasserOpRookBlock };
// case(4) * phase(3 -- no opening)
constexpr array<int, 12> PasserSpecial = { // tuner: type=array, var=100, active=0
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	26, 52, 0
};
namespace Values
{
	constexpr packed_t PasserOpRookBlock = 
			Pack4(0, Av(PasserSpecial, 3, ::PasserOpRookBlock, 0), Av(PasserSpecial, 3, ::PasserOpRookBlock, 1), Av(PasserSpecial, 3, ::PasserOpRookBlock, 2));
}

array<uint8, 16> LogDist;
array<packed_t, 8> PasserGeneral;
array<packed_t, 8> PasserBlocked;
array<packed_t, 8> PasserFree;
array<packed_t, 8> PasserSupported;
array<packed_t, 8> PasserProtected;
array<packed_t, 8> PasserConnected;
array<packed_t, 8> PasserOutside;
array<packed_t, 8> PasserCandidate;
array<packed_t, 8> PasserClear;
array<sint16, 8> PasserAtt;
array<sint16, 8> PasserDef;
array<sint16, 8> PasserAttLog;
array<sint16, 8> PasserDefLog;

enum
{
	IsolatedOpen,
	IsolatedClosed,
	IsolatedBlocked,
	IsolatedDoubledOpen,
	IsolatedDoubledClosed
};
constexpr array<int, 20> Isolated = {	
	36, 28, 19, 1,
	40, 21, 1, 12,
	-40, -20, -3, -3,
	0, 10, 45, 3,
	27, 27, 36, 8 };

enum
{
	UpBlocked,
	PasserTarget,
	ChainRoot
};
constexpr array<int, 12> Unprotected = {  // tuner: type=array, var=26, active=0
	16, 18, 20, 0,
	-20, -12, -4, 0,
	36, 16, -4, 0 };
enum
{
	BackwardOpen,
	BackwardClosed
};
constexpr array<int, 8> Backward = {  // tuner: type=array, var=26, active=0
	68, 54, 40, 0,
	16, 10, 4, 0 };
enum
{
	DoubledOpen,
	DoubledClosed
};
constexpr array<int, 8> Doubled = {  // tuner: type=array, var=26, active=0
	12, 6, 0, 0,
	4, 2, 0, 0 };

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
constexpr array<int, 40> RookSpecial = {  // tuner: type=array, var=26, active=0
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
constexpr array<int, 28> Tactical = {  // tuner: type=array, var=51, active=0
	-4, 8, 20, 0,
	0, 10, 20, 0,
	44, 80, 116, 0,
	92, 110, 128, 0,
	76, 60, 44, 0,
	164, 106, 48, 0,
	0,	10,	40,	-10
};

enum
{
	KingDefKnight,
	KingDefBishop,
	KingDefRook,
	KingDefQueen
};
constexpr array<int, 16> KingDefence = {  // tuner: type=array, var=13, active=0
	8, 4, 0, 0,
	0, 2, 4, 0,
	0, 0, 0, 0,
	16, 8, 0, 0 };

enum
{
	PawnChainLinear,
	PawnChain,
	PawnBlocked,
	PawnFileSpan,
	PawnRestrictsK
};
constexpr array<int, 20> PawnSpecial = {  // tuner: type=array, var=26, active=0
	44, 40, 36, 0, 
	36, 26, 16, 0, 
	0, 18, 36, 0, 
	4, 4, 4, 0, 
	0, 0, 0, 0
};

enum { BishopNonForwardPawn, BishopPawnBlock, BishopOutpostNoMinor };
constexpr array<int, 12> BishopSpecial = { // tuner: type=array, var=20, active=0
	0, 0, 0, 0,
	0, 6, 12, 0,
	60, 60, 45, 0
};

constexpr array<uint64, 2> Outpost = { 0x00007E7E3C000000ull, 0x0000003C7E7E0000ull };
enum
{
	KnightOutpost,
	KnightOutpostProtected,
	KnightOutpostPawnAtt,
	KnightOutpostNoMinor
};
constexpr array<int, 16> KnightSpecial = {  // tuner: type=array, var=26, active=0
	40, 40, 24, 0,
	41, 40, 0, 0,
	44, 44, 18, 0,
	41, 40, 0, 0 };

enum
{
	WeakPin,
	StrongPin,
	ThreatPin,
	SelfPawnPin,
	SelfPiecePin
};
constexpr array<int, 20> Pin = {  // tuner: type=array, var=51, active=0
	84, 120, 156, 0,
	24, 172, 320, 0,
	180, 148, 116, 0,
	32, 34, 36, 0,
	192, 150, 108, 0 };

enum
{
	QKingRay,
	RKingRay,
	BKingRay
};
constexpr array<int, 12> KingRay = {  // tuner: type=array, var=51, active=0
	17, 26, 33, -2,
	-14, 15, 42, 0,
	43, 14, -9, -1 };

constexpr array<int, 11> KingAttackWeight = {  // tuner: type=array, var=51, active=0
	56, 88, 44, 64, 60, 104, 116, 212, 192, 256, 64 };
constexpr uint32 KingNAttack1 = UPack(1, KingAttackWeight[0]);
constexpr uint32 KingNAttack = UPack(2, KingAttackWeight[1]);
constexpr uint32 KingBAttack1 = UPack(1, KingAttackWeight[2]);
constexpr uint32 KingBAttack = UPack(2, KingAttackWeight[3]);
constexpr uint32 KingRAttack1 = UPack(1, KingAttackWeight[4]);
constexpr uint32 KingRAttack = UPack(2, KingAttackWeight[5]);
constexpr uint32 KingQAttack1 = UPack(1, KingAttackWeight[6]);
constexpr uint32 KingQAttack = UPack(2, KingAttackWeight[7]);
constexpr uint32 KingAttack = UPack(1, 0);
constexpr uint32 KingAttackSquare = KingAttackWeight[8];
constexpr uint32 KingNoMoves = KingAttackWeight[9];
constexpr uint32 KingShelterQuad = KingAttackWeight[10];	// a scale factor, not a score amount

template<int N> array<uint16, N> CoerceUnsigned(const array<int, N>& src)
{
	array<uint16, N> retval;
	for (int ii = 0; ii < N; ++ii)
		retval[ii] = static_cast<uint16>(max(0, src[ii]));
	return retval;
}
constexpr array<uint16, 16> KingAttackScale = { 0, 1, 2, 4, 6, 9, 14, 19, 25, 31, 39, 47, 46, 65, 65, 65 };
constexpr array<int, 4> KingCenterScale = { 62, 61, 70, 68 };

// tuner: stop

// END EVAL WEIGHTS

// SMP

#define MaxPrN 1
#ifndef DEBUG
#ifndef TUNER
#undef MaxPrN
#ifndef W32_BUILD
#define MaxPrN 64  // mustn't exceed 64
#else
#define MaxPrN 32  // mustn't exceed 32
#endif
#endif
#endif

int PrN = 1, CPUs = 1, HT = 0, parent = 1, child = 0, WinParId, Id = 0, ResetHash = 1, NewPrN = 0;
HANDLE ChildPr[MaxPrN];
constexpr int SplitDepth = 10;
constexpr int SplitDepthPV = 4;
constexpr int MaxSplitPoints = 64;  // mustn't exceed 64

typedef struct
{
	GPosData Position[1];
	array<uint64, 104> stack;
	int sp, date;
	array<array<uint16, N_KILLER>, 16> killer;
} GPos;

constexpr int FlagClaimed = 1 << 1;
constexpr int FlagFinished = 1 << 2;

typedef struct
{
	volatile uint16 move;
	volatile uint8 reduced_depth, research_depth, stage, ext, id, flags;
} GMove;

typedef struct
{
	array<GMove, MAX_HEIGHT> move;
	GPos Pos[1];
	volatile LONG lock;
	volatile int claimed, active, finished, pv, move_number, current, depth, alpha, beta, singular, split, best_move, height;
	jmp_buf jump;
} GSP;

typedef struct
{
	volatile long long nodes, active_sp, searching;
#ifdef TB
	volatile long long tb_hits;
#endif
#ifndef W32_BUILD
	volatile long long stop, fail_high;
#else
	volatile long stop, fail_high;
#endif
	volatile sint64 hash_size;
	volatile int PrN;
	GSP Sp[MaxSplitPoints];
#ifdef TB
	char tb_path[1024];
#endif
} GSMPI;

constexpr int MAGIC_SIZE = 107648;
uint64* MagicAttacks;

#define SharedMaterialOffset (sizeof(GSMPI))
#define SharedMagicOffset (SharedMaterialOffset + TotalMat * sizeof(GMaterial))
#define SharedPVHashOffset (SharedMagicOffset + MAGIC_SIZE * sizeof(uint64))

GSMPI* Smpi;

jmp_buf CheckJump;

HANDLE SHARED = nullptr, HASH = nullptr;

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
template <bool me> int NB(const uint64& x)
{
	return me ? msb(x) : lsb(x);
}
template <bool me> int NBZ(const uint64& x)
{
	return me ? MSBZ(x) : LSBZ(x);
}

array<int, 256> SpanWidth;
inline int FileSpan(uint64* occ)
{
	*occ |= *occ >> 32;
	*occ |= *occ >> 16;
	*occ |= *occ >> 8;
	*occ &= 0xFF;	// now it is the file population
	return SpanWidth[static_cast<size_t>(*occ)];
}
inline int FileSpan(const uint64& occ)
{
	uint64 temp = occ;
	return FileSpan(&temp);
}

uint64 BMagicAttacks(int i, uint64 occ);
uint64 RMagicAttacks(int i, uint64 occ);
uint16 rand16();
uint64 rand64();
void init_pst();
void init_eval();
void init();
void init_search(int clear_hash);
void setup_board();
void get_board(const char fen[]);
void init_hash();
void move_to_string(int move, char string[]);
int move_from_string(char string[]);
void pick_pv();
template <bool me> void do_move(int move);
INLINE void do_move(bool me, int move)
{
	if (me)
		do_move<true>(move);
	else
		do_move<false>(move);
}
template <bool me> void undo_move(int move);
INLINE void undo_move(bool me, int move)
{
	if (me)
		undo_move<true>(move);
	else
		undo_move<false>(move);
}
void do_null();
void undo_null();
INLINE void evaluate();
template <bool me> bool is_legal(int move);
template <bool me> bool is_check(int move);
void hash_high(int value, int depth);
int hash_low(int move, int value, int depth);	// returns input value
void hash_exact(int move, int value, int depth, int exclusion, int ex_depth, int knodes);
INLINE int pick_move();
template <bool me, bool root> int get_move(int depth);
template <bool me> bool see(int move, int margin, const uint16* mat_value);
template <bool me> void gen_root_moves();
template <bool me> int* gen_captures(int* list);
template <bool me> int* gen_evasions(int* list);
void mark_evasions(int* list);
template <bool me> int* gen_quiet_moves(int* list);
template <bool me> int* gen_checks(int* list);
template <bool me> int* gen_delta_moves(int* list);
template <bool me, bool pv> int q_search(int alpha, int beta, int depth, int flags);
template <bool me, bool pv> int q_evasion(int alpha, int beta, int depth, int flags);
template <bool me, bool exclusion, bool evasion> int scout(int beta, int depth, int flags);
template <bool me, bool root> int pv_search(int alpha, int beta, int depth, int flags);
template <bool me> void root();
template <bool me> int multipv(int depth);
void send_pv(int depth, int alpha, int beta, int score);
void send_multipv(int depth, int curr_number);
void send_best_move();
void get_position(char string[]);
void get_time_limit(char string[]);
sint64 get_time();
int time_to_stop(GSearchInfo* SI, int time, int searching);
void check_time(const int* time, int searching);
int input();
void uci();

bool IsIllegal(bool me, int move)
{
	return ((HasBit(Current->xray[opp], From(move)) && F(Bit(To(move)) & FullLine[lsb(King(me))][From(move)])) ||
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
		&& F((move)& 0xF000);
};

#ifdef TUNER
#ifndef RECORD_GAMES
int ResignThreshold = 150;
#else
int ResignThreshold = 1500;
#endif

typedef struct
{
	int wins, draws, losses;
} GMatchInfo;
GMatchInfo MatchInfo[1] = { (0, 0, 0) };

char Fen[65536][MAX_HEIGHT];
int opening_positions = 0;

int Client = 0, Server = 0, Local = 1, cmd_number = 0;
int generation = 0;

#ifdef PGN
typedef struct
{
	uint64 bb[6];  // white, black, pawns, minor, major, queens and knights
	uint8 ep_square, turn, ply, castle_flags;
} GPos;
GPos Pos[65536];
int pgn_positions = 0;

void position_to_pos(GPos* pos)
{
	pos->bb[0] = Piece(White);
	pos->bb[1] = Piece(Black);
	pos->bb[2] = PawnAll();
	pos->bb[3] = Minor(White) | Minor(Black);
	pos->bb[4] = Major(White) | Major(Black);
	pos->bb[5] = Queen(White) | Queen(Black) | Knight(White) | Knight(Black);
	pos->ep_square = Current->ep_square;
	pos->turn = Current->turn;
	pos->castle_flags = Current->castle_flags;
}

void position_from_pos(GPos* pos)
{
	Current = Data;
	memset(Board, 0, sizeof(GBoard));
	memset(Current, 0, sizeof(GData));
	Piece(White) = pos->bb[0];
	Piece(Black) = pos->bb[1];
	for (int me = 0; me < 2; ++me)
	{
		Piece(me) = pos->bb[me];
		Piece(IPawn[me]) = pos->bb[2] & Piece(me);
		Piece(IKnight[me]) = pos->bb[3] & pos->bb[5] & Piece(me);
		Piece(ILight[me]) = pos->bb[3] & (~pos->bb[5]) & LightArea & Piece(me);
		Piece(IDark[me]) = pos->bb[3] & (~pos->bb[5]) & DarkArea & Piece(me);
		Piece(IRook[me]) = pos->bb[4] & (~pos->bb[5]) & Piece(me);
		Piece(IQueen[me]) = pos->bb[4] & pos->bb[5] & Piece(me);
		Piece(IKing[me]) = Piece(me) & (~(pos->bb[2] | pos->bb[3] | pos->bb[4]));
	}
	for (int i = 2; i < 16; ++i)
		for (uint64 u = Piece(i); u; Cut(u)) PieceAt(lsb(u)) = i;
	Current->ep_square = pos->ep_square;
	Current->ply = pos->ply;
	Current->castle_flags = pos->castle_flags;
	Current->turn = pos->turn;
	setup_board();
}

int pos_shelter_tune()
{
	if (!Queen(White) || !Queen(Black))
		return 0;
	if (popcnt(NonPawnKingAll()) < 10)
		return 0;
	if (Current->castle_flags)
		return 0;
	if (FileOf(lsb(King(White))) <= 2 && FileOf(lsb(King(Black))) >= 5)
		return 1;
	if (FileOf(lsb(King(White))) >= 5 && FileOf(lsb(King(Black))) <= 2)
		return 1;
	return 0;
}
int pos_passer_tune()
{
	if (Current->passer)
		return 1;
	return 0;
}
#endif

void init_openings()
{
	FILE* ffen = nullptr;
	ffen = fopen("8moves.epd", "r");
	if (ffen != nullptr)
	{
		for (int i = 0; i < 65536; ++i)
		{
			fgets(Fen[i], 128, ffen);
			if (feof(ffen))
			{
				opening_positions = Max(opening_positions - 1, 0);
				break;
			}
			else
				++opening_positions;
		}
	}
	else
	{
		fprintf(stdout, "File '8moves.epd' not found\n");
		exit(0);
		goto no_fen;
	}
	fclose(ffen);
no_fen:
	if (opening_positions == 0)
	{
		sprintf(Fen[0], "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1\n");
		opening_positions = 1;
	}
#ifdef PGN
	FILE* fpgn = fopen("uci_games.pgn", "r");
	if (fpgn == nullptr)
	{
		fprintf(stdout, "File 'uci_games.pgn' not found\n");
		exit(0);
	}
	while (pgn_positions < 65536)
	{
		fgets(mstring, 65536, fpgn);
		if (feof(fpgn))
		{
			pgn_positions = Max(pgn_positions - 1, 0);
			break;
		}
		if (strstr(mstring, "FEN"))
			get_board(mstring + 6);
		if (strchr(mstring, '['))
			continue;
		if (strlen(mstring) < 100)
			continue;
		char* ptr = mstring;
		while (*ptr != 0)
		{
			evaluate();
			if (pos_passer_tune())
			{
				position_to_pos(&Pos[pgn_positions++]);
				break;
			}
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
			if (pv_string[0] == '1' || pv_string[0] == '0')
				break;
			int move = move_from_string(pv_string);
			if (Current->turn)
			{
				if (!is_legal<1>(move))
					break;
				do_move<1>(move);
			}
			else
			{
				if (!is_legal<0>(move))
					break;
				do_move<0>(move);
			}
			memcpy(Data, Current, sizeof(GData));
			Current = Data;
			while (*ptr == ' ') ++ptr;
		}
	}
	fclose(fpgn);
	fprintf(stdout, "%d PGN positions\n", pgn_positions);
#endif
}
void init_variables()
{
	int i, j, k, start = 0;
	FILE* f;

	if (Local)
		f = fopen(GullCpp, "r");
	else if (Server)
		f = fopen("./Serverl.cpp", "r");
	else
		f = fopen("./Client/Gull.cpp", "r");
	while (!feof(f))
	{
		(void)fgets(mstring, 256, f);
		if (!start && memcmp(mstring, "// tuner: start", 15))
			continue;
		start = 1;
		if (!memcmp(mstring, "// tuner: stop", 14))
			break;
		memcpy(SourceFile[src_str_num].line, mstring, 256);
		++src_str_num;
	}
	fclose(f);

	var_number = 0;
	active_vars = 0;

	int curr_ind = -1, active, indexed[MaxVariables];
	double var;
	char* p, *q;
	memset(VarName, 0, 1000 * sizeof(GString));
	memset(indexed, 0, MaxVariables * sizeof(int));
	for (i = 0; i < src_str_num; ++i)
	{
		if (!strstr(SourceFile[i].line, "tuner: enum"))
			continue;
		for (++i; !strstr(SourceFile[i].line, "};"); ++i)
		{
			p = strchr(SourceFile[i].line, 'I') + 1;
			strcpy(VarName[var_name_num].line, p);
			for (j = 0; VarName[var_name_num].line[j] >= '0' && VarName[var_name_num].line[j] <= 'z'; ++j)
				;
			VarName[var_name_num].line[j] = '\n';
			for (k = j + 1; k < 1000; ++k) VarName[var_name_num].line[k] = 0;
			q = strchr(p, '+');
			if (q != nullptr)
				curr_ind += atoi(q + 1);
			else
				++curr_ind;
			VarIndex[var_name_num] = curr_ind;
			++var_name_num;
		}
		break;
	}
	for (i = 0; i < src_str_num; ++i)
	{
		if (!(p = strstr(SourceFile[i].line, "tuner:")))
			continue;
		q = strstr(p, "type=");
		if (q == nullptr)
			continue;
		q += 5;
		p = strstr(q, "active=");
		if (p == nullptr)
			active = 1;
		else
			active = atoi(p + 7);
		uint8 active_mask[1024];
		memset(active_mask, 1, 1024);
		if (p = strstr(q, "mask="))
		{
			p += 5;
			j = 0;
			while (p[0] != ' ')
			{
				int value = 0;
				if (p[0] == 's')
					value = 1;
				if (p[1] == '&')
				{
					if (value == 1)
						break;
					for (k = j; k < 1024; ++k) active_mask[k] = 0;
					break;
				}
				for (k = 0; k < p[1] - '0'; ++k) active_mask[j + k] = value;
				j += p[1] - '0';
				p += 2;
			}
		}
		p = strstr(q, "var=");
		if (p == nullptr)
			var = 1000.0;
		else
			var = (double)atoi(p + 4);
		if (!memcmp(q, "array", 5))
		{
			p = strstr(SourceFile[i].line, "int ");
			p += 4;
			q = strchr(p, '[');
			for (j = 0; j < var_name_num; ++j)
				if (!memcmp(p, VarName[j].line, (int)(q - p)))
					break;
			curr_ind = VarIndex[j];
			fprintf(stdout, "Array (%d) active=%d var=%.2lf: %s", curr_ind, active, var, VarName[j].line);
		}
		++i;
		memset(mstring, 0, strlen(mstring));
		while (!strstr(SourceFile[i].line, "};"))
		{
			strcat(mstring, SourceFile[i].line);
			++i;
		}
		--i;
		p = mstring - 1;
		int cnt = 0;
		do
		{
			++p;
			Variables[curr_ind] = atoi(p);
			++var_number;
			if (indexed[curr_ind])
			{
				fprintf(stdout, "index mismatch: %d (%s)\n", curr_ind, VarName[j].line);
				exit(0);
			}
			++indexed[curr_ind];
			int activate = 0;
			if (active && active_mask[cnt])
				activate = 1;
			if (activate)
				Var[active_vars] = var;
			Active[curr_ind++] = activate;
			active_vars += activate;
			++cnt;
		} while (p = strchr(p, ','));
	}
	for (i = 0; i < curr_ind; ++i)
		if (!indexed[i])
		{
			fprintf(stdout, "index skipped %d\n", i);
			exit(0);
		}
	fprintf(stdout, "%d variables, %d active\n", var_number, active_vars);
}
void eval_to_cpp(const char* filename, double* list)
{
	FILE* f = fopen(filename, "w");
	for (int i = 0; i < var_name_num; ++i) VarName[i].line[strlen(VarName[i].line) - 1] = 0;
	for (int i = 0; i < src_str_num; ++i)
	{
		fprintf(f, "%s", SourceFile[i].line);
		if (!strstr(SourceFile[i].line, "type=array") || strstr(SourceFile[i].line, "active=0"))
			continue;
		for (int j = 0; j < var_name_num; ++j)
			if (strstr(SourceFile[i].line, VarName[j].line))
			{
				int n = 0;
			start:
				++i;
				fprintf(f, "    ");
				int cnt = 0, index = 0;
				for (int k = 0; k < VarIndex[j]; ++k)
					if (Active[k])
						++index;
				char* p = SourceFile[i].line, *end;
				while ((p = strchr(p + 1, ',')) != nullptr) ++cnt;
				if (end = strstr(SourceFile[i + 1].line, "};"))
					++cnt;
				for (int k = 0; k < cnt; ++k)
				{
					fprintf(f, "%d", (int)list[index + (n++)]);
					if (k + 1 < cnt)
						fprintf(f, ", ");
					else if (end == nullptr)
						fprintf(f, ",\n");
					else
						fprintf(f, "\n");
				}
				if (end == nullptr)
					goto start;
			}
	}
	fclose(f);
}
void print_eval()
{
	int i, j;
	FILE* f = fopen("eval.txt", "w");
	fprintf(f, "Pst\n");
	for (j = 2; j < 16; j += 2)
	{
		if (j == 8)
			continue;
		fprintf(f, "%d:\n", j);
		for (i = 0; i < 64; ++i)
		{
			fprintf(f, "(%d,%d), ", Opening(Pst(j, i)), Endgame(Pst(j, i)));
			if ((i + 1) % 8 == 0)
				fprintf(f, "\n");
		}
	}
	fprintf(f, "Mobility\n");
	for (j = 0; j < 4; ++j)
	{
		fprintf(f, "%d:\n", j);
		for (i = 0; i < 32; ++i) fprintf(f, "(%d,%d), ", Opening(Mobility[j][i]), Endgame(Mobility[j][i]));
		fprintf(f, "\n");
	}
	fprintf(f, "PasserGeneral\n");
	for (i = 0; i < 8; ++i) fprintf(f, "(%d,%d), ", Opening(PasserGeneral[i]), Endgame(PasserGeneral[i]));
	fprintf(f, "\n");

	fprintf(f, "PasserBlocked\n");
	for (i = 0; i < 8; ++i) fprintf(f, "(%d,%d), ", Opening(PasserBlocked[i]), Endgame(PasserBlocked[i]));
	fprintf(f, "\n");

	fprintf(f, "PasserFree\n");
	for (i = 0; i < 8; ++i) fprintf(f, "(%d,%d), ", Opening(PasserFree[i]), Endgame(PasserFree[i]));
	fprintf(f, "\n");

	fprintf(f, "PasserSupported\n");
	for (i = 0; i < 8; ++i) fprintf(f, "(%d,%d), ", Opening(PasserSupported[i]), Endgame(PasserSupported[i]));
	fprintf(f, "\n");

	fprintf(f, "PasserProtected\n");
	for (i = 0; i < 8; ++i) fprintf(f, "(%d,%d), ", Opening(PasserProtected[i]), Endgame(PasserProtected[i]));
	fprintf(f, "\n");

	fprintf(f, "PasserConnected\n");
	for (i = 0; i < 8; ++i) fprintf(f, "(%d,%d), ", Opening(PasserConnected[i]), Endgame(PasserConnected[i]));
	fprintf(f, "\n");

	fprintf(f, "PasserOutside\n");
	for (i = 0; i < 8; ++i) fprintf(f, "(%d,%d), ", Opening(PasserOutside[i]), Endgame(PasserOutside[i]));
	fprintf(f, "\n");

	fprintf(f, "PasserCandidate\n");
	for (i = 0; i < 8; ++i) fprintf(f, "(%d,%d), ", Opening(PasserCandidate[i]), Endgame(PasserCandidate[i]));
	fprintf(f, "\n");

	fprintf(f, "PasserClear\n");
	for (i = 0; i < 8; ++i) fprintf(f, "(%d,%d), ", Opening(PasserClear[i]), Endgame(PasserClear[i]));
	fprintf(f, "\n");

	fprintf(f, "PasserAtt\n");
	for (i = 0; i < 8; ++i) fprintf(f, "%d, ", PasserAtt[i]);
	fprintf(f, "\n");

	fprintf(f, "PasserDef\n");
	for (i = 0; i < 8; ++i) fprintf(f, "%d, ", PasserDef[i]);
	fprintf(f, "\n");

	fprintf(f, "PasserAttLog\n");
	for (i = 0; i < 8; ++i) fprintf(f, "%d, ", PasserAttLog[i]);
	fprintf(f, "\n");

	fprintf(f, "PasserDefLog\n");
	for (i = 0; i < 8; ++i) fprintf(f, "%d, ", PasserDefLog[i]);
	fprintf(f, "\n");

	fprintf(f, "StormBlocked\n");
	for (i = 0; i < 4; ++i) fprintf(f, "%d, ", StormBlocked[i]);
	fprintf(f, "\n");

	fprintf(f, "StormShelterAtt\n");
	for (i = 0; i < 4; ++i) fprintf(f, "%d, ", StormShelterAtt[i]);
	fprintf(f, "\n");

	fprintf(f, "StormConnected\n");
	for (i = 0; i < 4; ++i) fprintf(f, "%d, ", StormConnected[i]);
	fprintf(f, "\n");

	fprintf(f, "StormOpen\n");
	for (i = 0; i < 4; ++i) fprintf(f, "%d, ", StormOpen[i]);
	fprintf(f, "\n");

	fprintf(f, "StormFree\n");
	for (i = 0; i < 4; ++i) fprintf(f, "%d, ", StormFree[i]);
	fprintf(f, "\n");

	fclose(f);
}

double ratio_from_elo(double elo)
{
	return 1.0 / (1.0 + exp(((-elo) / 400.0) * log(10.0)));
}
double elo_from_ratio(double ratio)
{
	return -(log((1.0 / Min(0.99999, Max(ratio, 0.00001))) - 1.0) / log(10.0)) * 400.0;
}
double rand_u()
{
	return Min(1.0, Max(0.0, ((double)((rand() << 15) | rand())) / (32768.0 * 32768.0)));
}
double gaussian(double mean, double sigma)
{
	return sqrt(Max(0.0000001, -2.0 * log(Max(0.0000001, rand_u())))) * sin(2.0 * 3.14159265358979323846 * rand_u()) * sigma + mean;
}
void int_to_double(double* dst, int* src, int n)
{
	for (int i = 0; i < n; ++i) dst[i] = (double)src[i];
}
void double_to_int(int* dst, double* src, int n)
{
	for (int i = 0; i < n; ++i) dst[i] = (int)src[i];
}
void double_to_double(double* dst, double* src, int n)
{
	for (int i = 0; i < n; ++i) dst[i] = src[i];
}
void int_to_int(int* dst, int* src, int n)
{
	for (int i = 0; i < n; ++i) dst[i] = src[i];
}
double scalar(double* one, double* two, int n)
{
	double result = 0.0;
	for (int i = 0; i < n; ++i) result += one[i] * two[i];
	return result;
}
void load_list(double* list)
{
	int i, j = 0;
	for (i = 0; i < var_number; ++i)
		if (Active[i])
			Variables[i] = (int)list[j++];
}
void save_list(double* list)
{
	int i, j = 0;
	for (i = 0; i < var_number; ++i)
		if (Active[i])
			list[j++] = (double)Variables[i];
}
void log_list(FILE* f, double* list, int n)
{
	fprintf(f, "(");
	for (int i = 0; i < n; ++i)
	{
		fprintf(f, "%.2lf", list[i]);
		if (i < n - 1)
			fprintf(f, ",");
	}
	fprintf(f, ")\n");
}
void log_list(const char* file_name, double* list, int n)
{
	FILE* f = fopen(file_name, "a");
	log_list(f, list, n);
	fclose(f);
}
void log_list(char* s, double* list, int n, bool precision)
{
	sprintf(s + strlen(s), "(");
	for (int i = 0; i < n; ++i)
	{
		if (!precision)
			sprintf(s + strlen(s), "%.2lf", list[i]);
		else
			sprintf(s + strlen(s), "%lf", list[i]);
		if (i < n - 1)
			sprintf(s + strlen(s), ",");
	}
	sprintf(s + strlen(s), ") ");
}
void read_list(char* string, double* list, int n)
{
	int i = 0;
	char* p = strchr(string, '(');
	do
	{
		list[i++] = atof(++p);
		if (i >= n)
			break;
	} while (p = strchr(p, ','));
}
void init_eval_data(double* one, double* two)
{
	if (one != EvalOne)
		double_to_double(EvalOne, one, var_number);
	if (two != EvalTwo)
		double_to_double(EvalTwo, two, var_number);
	load_list(one);
	PstVals = PstOne;
	init_pst();
	load_list(two);
	PstVals = PstTwo;
	init_pst();
}
void load_eval(int first)
{
	int i;
	++generation;
	for (i = 1; i < MAX_HEIGHT; ++i) Data[i].eval_key = 0;
	if (first)
	{
		load_list(EvalOne);
		Hash = HashOne;
		PawnHash = PawnHashOne;
		PVHash = PVHashOne;
		PstVals = PstOne;
		HistoryVals = HistoryOne;
		DeltaVals = DeltaOne;
		Ref = RefOne;
	}
	else
	{
		load_list(EvalTwo);
		Hash = HashTwo;
		PawnHash = PawnHashTwo;
		PVHash = PVHashTwo;
		PstVals = PstTwo;
		HistoryVals = HistoryTwo;
		DeltaVals = DeltaTwo;
		Ref = RefTwo;
	}
	Current->pst = 0;
	for (i = 0; i < 64; ++i)
		if (PieceAt(i))
			Current->pst += Pst(PieceAt(i), i);
	init_eval();
}
void compute_list(double* dst, double* base, double* dir, double* var, double a)
{
	for (int i = 0; i < active_vars; ++i) dst[i] = base[i] + dir[i] * var[i] * a;
}
void scale_list(double* list, double r)
{
	int i;
	double x = 0.0;
	for (i = 0; i < active_vars; ++i) x += list[i] * list[i];
	x = r / sqrt(x);
	for (i = 0; i < active_vars; ++i) list[i] *= x;
}
int play(int depth)
{
	LastDepth = TimeLimit1 = TimeLimit2 = 0;
#ifdef TIMING
	Infinite = 0;
	int nmoves = MovesTg - 1;
	if (Current->ply > 40)
		nmoves += Min(Current->ply - 40, (100 - Current->ply) / 2);
	TimeLimit1 = Min(GlobalTime[GlobalTurn], (GlobalTime[GlobalTurn] + nmoves * GlobalInc[GlobalTurn]) / nmoves);
	TimeLimit2 = Min(GlobalTime[GlobalTurn], (GlobalTime[GlobalTurn] + nmoves * GlobalInc[GlobalTurn]) / 3);
	TimeLimit1 = Min(GlobalTime[GlobalTurn], (TimeLimit1 * TimeRatio) / 100);
	DepthLimit = MAX_HEIGHT;
	Searching = 1;
	nodes = Stop = 0;
	StartTime = get_time();
#else
	DepthLimit = 2 * depth + 2;
	Infinite = 1;
#endif
	best_score = best_move = 0;
	Print = 0;
	if (Current->turn == White)
		root<0>();
	else
		root<1>();
	return best_score;
}
double play_game(double* one, double* two, int depth, char* fen)
{
	int i, cnt, sdepth, value, previous = 0, im = 0;
	load_eval(0);
	init_search(1);
	load_eval(1);
	init_search(1);
#ifndef PGN
	get_board(fen);
	if (RecordGames)
	{
		RecordString[0] = 0;
		for (cnt = 0; fen[cnt] != '\n'; ++cnt) PosStr[cnt] = fen[cnt];
		PosStr[cnt] = 0;
	}
#else
	position_from_pos((GPos*)fen);
#endif
	init_eval_data(one, two);

#ifdef TIMING
	GlobalTime[0] = GlobalTime[1] = static_cast<int>((1.0 + rand_u()) * (double)(1000 << (int)(depth)));
	GlobalInc[0] = GlobalInc[1] = GlobalTime[0] / 200;
#endif

	for (cnt = 0; cnt < 200 + (RecordGames ? 200 : 0); ++cnt)
	{
		GlobalTurn = Even(cnt);
		load_eval(GlobalTurn);
		memcpy(Data, Current, sizeof(GData));
		Current = Data;
		if (Even(cnt))
			sdepth = depth + Odd(rand16());
		value = play(sdepth);
		if (!best_move)
			goto loss;
		if (value < -ResignThreshold && previous > ResignThreshold)
			goto loss;
		if (!RecordGames)
		{
			if (value == 0 && previous == 0 && cnt >= 60)
				goto draw;
			if (abs(value) <= 3 && abs(previous) <= 3 && cnt >= 120)
				goto draw;
		}
		if (Current->ply >= 100)
			goto draw;
		for (i = 4; i <= Current->ply; i += 2)
			if (Stack[sp - i] == Current->key)
				goto draw;
		int me = 0;
		if (!PawnAll())
		{
			int my_score = 3 * popcnt(Minor(me)) + 5 * popcnt(Rook(me)) + 9 * popcnt(Queen(me));
			int opp_score = 3 * popcnt(Minor(opp)) + 5 * popcnt(Rook(opp)) + 9 * popcnt(Queen(opp));
			if (abs(my_score - opp_score) <= 3 && Max(popcnt(NonPawnKing(me)), popcnt(NonPawnKing(opp))) <= 2)
			{
				++im;
				if (im >= 10 && abs(value) < 128 && abs(previous) < 128)
					goto draw;
			}
		}
#ifdef WIN_PR
		if (cnt >= 6 && ((!Queen(White) && !Queen(Black)) || (popcnt(NonPawnKing(White)) <= 2 && popcnt(NonPawnKing(Black)) <= 2) ||
			(!Current->castle_flags && ((VLine[lsb(King(White))] | PIsolated[FileOf(lsb(King(White)))]) & King(Black)))))
			return ratio_from_elo(3.0 * (double)value) + ratio_from_elo(-3.0 * (double)previous);
#endif
		previous = value;
		if (!Current->turn)
			do_move<0>(best_move);
		else
			do_move<1>(best_move);
		if (RecordGames)
		{
			move_to_string(best_move, pv_string);
			sprintf(RecordString + strlen(RecordString), "%s { %.2lf / %d } ", pv_string, (double)(Current->turn ? value : (-value)) / 100.0, LastDepth / 2);
		}
	}
draw:
	if (RecordGames)
		sprintf(Buffer + strlen(Buffer), "[FEN \"%s\"]\n[Result \"1/2-1/2\"]\n%s\n", PosStr, RecordString);
	return 1.0;
loss:
	if (Even(cnt))
	{
		if (RecordGames)
		{
			if (Current->turn)
				sprintf(Buffer + strlen(Buffer), "[FEN \"%s\"]\n[Result \"1-0\"]\n%s\n", PosStr, RecordString);
			else
				sprintf(Buffer + strlen(Buffer), "[FEN \"%s\"]\n[Result \"0-1\"]\n%s\n", PosStr, RecordString);
		}
		return 0.0;
	}
	else
	{
		if (RecordGames)
		{
			if (Current->turn)
				sprintf(Buffer + strlen(Buffer), "[FEN \"%s\"]\n[Result \"1-0\"]\n%s\n", PosStr, RecordString);
			else
				sprintf(Buffer + strlen(Buffer), "[FEN \"%s\"]\n[Result \"0-1\"]\n%s\n", PosStr, RecordString);
		}
		return 2.0;
	}
}
double play_position(double* one, double* two, int depth, char* fen, GMatchInfo* MI)
{
	double result, score = 0.0;
	result = play_game(one, two, depth, fen);
	if (result >= 1.98)
		MI->wins++;
	else if (result <= 0.02)
		MI->losses++;
	else
		MI->draws++;
	score += result;
	result = play_game(two, one, depth, fen);
	if (result >= 1.98)
		MI->losses++;
	else if (result <= 0.02)
		MI->wins++;
	else
		MI->draws++;
	score += 2.0 - result;
	return score;
}
double match(double* one, double* two, int positions, int depth, GMatchInfo* MI)
{
	double score = 0.0;
	memset(MI, 0, sizeof(GMatchInfo));
	for (int i = 0; i < positions; ++i)
	{
#ifndef PGN
		score += play_position(one, two, depth, Fen[rand64() % (uint64)opening_positions], MI);
#else
		score += play_position(one, two, depth, (char*)&Pos[rand64() % (uint64)pgn_positions], MI);
#endif
	}
	return (25.0 * (double)score) / (double)(positions);
}
int match_los(double* one, double* two, int positions, int chunk_size, int depth, double high, double low, double uh, double ul, GMatchInfo* MI, bool print)
{
	int pos = 0;
	double score, ratio, stdev, wins, draws, losses, total, tot_score = 0.0;
	++cmd_number;

	memset(mstring, 0, strlen(mstring));
	sprintf(mstring, "$ Number=%d Command=match Depth=%d Positions=%d", cmd_number, depth, chunk_size);
	sprintf(mstring + strlen(mstring), " First=");
	log_list(mstring, one, active_vars, false);
	sprintf(mstring + strlen(mstring), " Second=");
	log_list(mstring, two, active_vars, false);
	fseek(stdin, 0, SEEK_END);
	fprintf(stdout, "%s\n", mstring);

	memset(MI, 0, sizeof(GMatchInfo));
	while (pos < positions)
	{
		pos += chunk_size;
	start:
		fgets(mstring, 65536, stdin);
		char* p = strstr(mstring, "Number=");
		if (p == nullptr)
			goto start;
		if (atoi(p + 7) != cmd_number)
			goto start;
		p = strstr(mstring, "Wins=");
		MI->wins += atoi(p + 5);
		p = strstr(mstring, "Draws=");
		MI->draws += atoi(p + 6);
		p = strstr(mstring, "Losses=");
		MI->losses += atoi(p + 7);
		p = strstr(mstring, "Result=");
		tot_score += atof(p + 7);

		wins = (double)MI->wins;
		draws = (double)MI->draws;
		losses = (double)MI->losses;
		total = Max(wins + losses, 1.0);
#ifndef WIN_PR
		score = (100.0 * wins + 50.0 * draws) / (total + draws);
#else
		score = tot_score / (double)(pos / chunk_size);
#endif
		if (print)
			fprintf(stdout, "%.2lf (%d positions played): %d-%d-%d\n", score, pos, MI->wins, MI->draws, MI->losses);
		if (total <= 0.99)
			continue;
		ratio = wins / total;
		stdev = 0.5 / sqrt(total);
		if (high > 0.01)
		{
			if (ratio >= 0.5 + stdev * high)
				return 1;
#ifdef WIN_PR
			if (score / 100.0 >= 0.5 + stdev * high)
				return 1;
#endif
		}
		if (low > 0.01)
		{
			if (ratio <= 0.5 - stdev * low)
				return -1;
#ifdef WIN_PR
			if (score / 100.0 <= 0.5 - stdev * low)
				return -1;
#endif
		}
		if (pos >= positions)
			break;
		double remaining = ((2.0 * (double)positions - total - draws) * (wins + losses)) / (total + draws);
		double target_high = 0.5 * (1.0 + (high / sqrt(total + remaining)));
		double target_low = 0.5 * (1.0 - (low / sqrt(total + remaining)));
		double ratio_high = target_high + 0.5 * (uh / sqrt(remaining));
		double ratio_low = target_low - 0.5 * (ul / sqrt(remaining));
		if (uh > 0.01)
			if ((wins + ratio_high * remaining) / (total + remaining) < target_high)
				return -1;
		if (ul > 0.01)
			if ((wins + ratio_low * remaining) / (total + remaining) > target_low)
				return 1;
	}
	return 0;
}
void gradient(double* base, double* var, int iter, int pos_per_iter, int depth, double radius, double* grad)
{
	int i, j;
	double dir[MaxVariables], A[MaxVariables], B[MaxVariables], r;
	memset(grad, 0, active_vars * sizeof(double));
	for (i = 0; i < iter; ++i)
	{
#ifndef RANDOM_SPHERICAL
		for (j = 0; j < active_vars; ++j) dir[j] = (Odd(rand()) ? 1.0 : (-1.0)) / sqrt(active_vars);
#else
		for (j = 0, r = 0.0; j < active_vars; ++j)
		{
			dir[j] = gaussian(0.0, 1.0);
			r += dir[j] * dir[j];
		}
		r = 1.0 / sqrt(Max(r, 0.0000001));
		for (j = 0; j < active_vars; ++j) dir[j] *= r;
#endif
		compute_list(A, base, dir, Var, -radius);
		compute_list(B, base, dir, Var, radius);
		r = 50.0 - match(A, B, pos_per_iter, depth, MatchInfo);
		for (j = 0; j < active_vars; ++j) grad[j] += r * dir[j];
	}
	for (i = 0; i < active_vars; ++i) grad[i] /= (double)iter;
}
void NormalizeVar(double* base, double* base_var, int depth, int positions, double radius, double target, double* var)
{
	int i, j;
	double A[MaxVariables], r, value, curr_var;

	fprintf(stdout, "NormalizeVar(): depth=%d, positions=%d, radius=%.2lf, target=%.2lf\n", depth, positions, radius, target);
	for (i = 0; i < active_vars; ++i)
	{
		double_to_double(A, base, active_vars);
		curr_var = base_var[i];
		fprintf(stdout, "Variable %d (%.2lf):\n", i, curr_var);
		for (j = 0; j < 10; ++j)
		{
			A[i] = base[i] + (radius * curr_var);
			match_los(base, A, positions, 16, depth, 0.0, 0.0, 0.0, 0.0, MatchInfo, false);
			r = (100 * MatchInfo->wins + 50 * MatchInfo->draws) / static_cast<double>(MatchInfo->wins + MatchInfo->draws + MatchInfo->losses);
			value = elo_from_ratio(r * 0.01);
			if (value < target)
				break;
			curr_var = curr_var * Min(sqrt(target / Max(value, 1.0)), 1.5);
			fprintf(stdout, "(%.2lf,%.2lf)\n", value, curr_var);
			if (curr_var > base_var[i])
			{
				curr_var = base_var[i];
				break;
			}
		}
		var[i] = curr_var;
		fprintf(stdout, "(%.2lf,%.2lf)\n", value, curr_var);
	}
	log_list("var.txt", var, active_vars);
}

void Gradient(double* base, double* var, int depth, int iter, int pos_per_iter, int max_positions, double radius, double angle_target, double* grad)
{
	typedef struct
	{
		double grad[MaxVariables];
	} GGradient;
	GGradient A[4], N[4];
	double list[MaxVariables], av, angle;
	int i, j, cnt = 0;
	++cmd_number;

	fprintf(stdout, "Gradient(): depth=%d, iter=%d, pos_per_iter=%d, max_positions=%d, radius=%.2lf\n", depth, iter, pos_per_iter, max_positions, radius);
	memset(A, 0, 4 * sizeof(GGradient));
	memset(grad, 0, active_vars * sizeof(double));

	memset(mstring, 0, strlen(mstring));
	sprintf(mstring, "$ Number=%d Command=gradient Depth=%d Iter=%d Positions=%d Radius=%lf Var=", cmd_number, depth, iter, pos_per_iter, radius);
	log_list(mstring, Var, active_vars, false);
	sprintf(mstring + strlen(mstring), " Base=");
	log_list(mstring, Base, active_vars, false);
	fseek(stdin, 0, SEEK_END);
	fprintf(stdout, "%s\n", mstring);

	while (cnt < max_positions)
	{
		for (j = 0; j < 4; ++j)
		{
		start:
			fgets(mstring, 65536, stdin);
			char* p = strstr(mstring, "Number=");
			if (p == nullptr)
				goto start;
			if (atoi(p + 7) != cmd_number)
				goto start;
			p = strstr(mstring, "Grad=");
			read_list(p, list, active_vars);

			for (i = 0; i < active_vars; ++i)
			{
				A[j].grad[i] += list[i];
				N[j].grad[i] = A[j].grad[i];
			}
			scale_list(N[j].grad, 1.0);
		}
		for (i = 0; i < active_vars; ++i) grad[i] = A[0].grad[i] + A[1].grad[i] + A[2].grad[i] + A[3].grad[i];
		scale_list(grad, 1.0);
		av = 0.0;
		for (i = 0; i < 4; ++i)
			for (j = i + 1; j < 4; ++j) av += scalar(N[i].grad, N[j].grad, active_vars);
		av /= 6.0;
		av = Min(0.99999, Max(-0.99999, av));
		angle = (acos(av) * 180.0) / 3.1415926535;
		cnt += 4 * pos_per_iter * iter;
		fprintf(stdout, "%d positions: angle = %.2lf, gradient = ", cnt, angle);
		log_list(stdout, grad, active_vars);
		if (angle < angle_target)
			break;
		FILE* fgrad = fopen("gradient.txt", "w");
		log_list(fgrad, grad, active_vars);
		fprintf(fgrad, "%d\n", cnt);
		fclose(fgrad);
	}
}
void GD(double* base,
	double* var,
	int depth,
	double radius,
	double min_radius,
	double angle_target,
	int max_grad_positions,
	int max_line_positions,
	double high,
	double low,
	double uh,
	double ul)
{
	double Grad[MaxVariables], a, br, A[MaxVariables], B[MaxVariables];
	FILE* fbest = fopen("gd.txt", "w");
	fclose(fbest);

	fprintf(stdout, "GD()\n");
	while (true)
	{
	start:
		fbest = fopen("gd.txt", "a");
		fprintf(fbest, "radius = %.2lf:\n", radius);
		log_list(fbest, base, active_vars);
		fclose(fbest);
		log_list(stdout, base, active_vars);
		// radius = 2.0;
		// read_list("(0.05,-0.04,-0.00,0.04,0.09,-0.10,-0.00,0.06,-0.14,-0.08,-0.06,0.05,-0.21,-0.10,-0.03,0.04,0.06,-0.01,-0.04,0.06,0.01,-0.05,-0.02,-0.06,-0.05,0.14,0.18,-0.01,-0.01,0.02,-0.11,0.05,-0.00,0.18,-0.15,-0.02,0.03,0.01,-0.06,-0.07,-0.03,0.11,0.13,-0.07,0.06,0.02,-0.01,0.06,-0.07,-0.09,0.01,-0.09,0.13,-0.03,0.04,0.03,-0.04,0.16,0.03,-0.21,-0.01,0.04,-0.03,-0.11,0.00,-0.03,-0.03,-0.11,-0.00,-0.06,0.04,-0.05,0.00,-0.03,-0.12,0.00,-0.07,-0.13,-0.08,0.10,0.11,0.03,0.08,0.12,-0.05,-0.07,-0.01,-0.02,0.08,-0.12,-0.05,0.02,0.03,0.13,-0.08,0.05,0.04,0.02,-0.00,0.06,-0.06,-0.07,-0.00,0.05,-0.09,-0.16,-0.02,-0.07,0.16,-0.24,0.09,0.04,-0.09,0.03,-0.06,0.01,-0.05,0.00,-0.10,-0.02,-0.12,-0.05,-0.05,0.07,0.14,0.16,-0.07,0.03,-0.06,-0.16,-0.03,0.04,-0.04,0.02,-0.12,-0.18,0.01,-0.04,-0.04,-0.18,0.08,0.09,-0.06,-0.00,0.02,-0.03,0.10,0.04,-0.02)",
		// Grad, active_vars);
		Gradient(base, var, depth, 32, 1, max_grad_positions, radius, angle_target, Grad);
		min_radius = Min(radius * 0.45, min_radius);
		a = radius;
		while (a >= min_radius)
		{
			fprintf(stdout, "Verification %.2lf:\n", a);
			compute_list(A, base, Grad, var, a);
			// eval_to_cpp("gd.cpp", A);
			if (match_los(A, base, max_line_positions, 32, depth, high, low, uh, ul, MatchInfo, true) == 1)
			{
				br = a;
				a *= 0.6;
				compute_list(B, base, Grad, var, a);
				double_to_double(base, A, active_vars);
				log_list("gd.txt", base, active_vars);
				eval_to_cpp("gd.cpp", base);
				fprintf(stdout, "New best: ");
				log_list(stdout, base, active_vars);
				fprintf(stdout, "Try %.2lf:\n", a);
				if (match_los(B, A, max_line_positions, 32, depth, 2.0, 2.0, 2.0, 0.0, MatchInfo, true) == 1)
				{
					br = a;
					double_to_double(base, B, active_vars);
					log_list("gd.txt", base, active_vars);
					eval_to_cpp("gd.cpp", base);
					fprintf(stdout, "New best: ");
					log_list(stdout, base, active_vars);
				}
				if (br < radius * 0.29)
					radius *= 0.7;
				goto start;
			}
			a *= 0.7;
		}
		radius *= 0.7;
	}
}
void get_command()
{
	enum
	{
		mode_grad,
		mode_match
	};
	int mode, depth, positions, number;
	char* p;

	if (RecordGames)
		Buffer[0] = 0;
	fgets(mstring, 65536, stdin);
	fseek(stdin, 0, SEEK_END);
	p = strstr(mstring, "Command=");
	if (p == nullptr)
		return;
	if (!memcmp(p + 8, "gradient", 8))
		mode = mode_grad;
	else if (!memcmp(p + 8, "match", 5))
		mode = mode_match;
	else
		return;
	p = strstr(mstring, "Number=");
	number = atoi(p + 7);
	p = strstr(mstring, "Depth=");
	depth = atoi(p + 6);
	p = strstr(mstring, "Positions=");
	positions = atoi(p + 10);
	if (mode == mode_grad)
	{
		p = strstr(mstring, "Iter=");
		int iter = atoi(p + 5);
		p = strstr(mstring, "Radius=");
		int radius = atof(p + 7);
		p = strstr(mstring, "Var=");
		read_list(p, Var, active_vars);
		p = strstr(mstring, "Base=");
		read_list(p, Base, active_vars);
		gradient(Base, Var, iter, positions, depth, radius, Grad);
		memset(mstring, 0, strlen(mstring));
		sprintf(mstring, "$ Number=%d Grad=", number);
		log_list(mstring, Grad, active_vars, true);
		fprintf(stdout, "%s\n", mstring);
	}
	else if (mode == mode_match)
	{
		p = strstr(mstring, "First=");
		read_list(p, FE, active_vars);
		p = strstr(mstring, "Second=");
		read_list(p, SE, active_vars);
		double r = match(FE, SE, positions, depth, MatchInfo);
		if (RecordGames)
		{
			frec = fopen("games.pgn", "a");
			fprintf(frec, "%s\n", Buffer);
			fclose(frec);
		}
		memset(mstring, 0, strlen(mstring));
		sprintf(mstring, "$ Number=%d Result=%lf Wins=%d Draws=%d Losses=%d", number, r, MatchInfo->wins, MatchInfo->draws, MatchInfo->losses);
		fprintf(stdout, "%s\n", mstring);
	}
	else
		nodes /= 0;
}
int get_mat_index(int wq, int bq, int wr, int br, int wl, int bl, int wd, int bd, int wn, int bn, int wp, int bp)
{
	if (wq > 2 || bq > 2 || wr > 2 || br > 2 || wl > 1 || bl > 1 || wd > 1 || bd > 1 || wn > 2 || bn > 2 || wp > 8 || bp > 8)
		return -1;
	return wp * MatWP + bp * MatBP + wn * MatWN + bn * MatBN + wl * MatWL + bl * MatBL + wd * MatWD + bd * MatBD + wr * MatWR + br * MatBR + wq * MatWQ +
		bq * MatBQ;
}
int conj_mat_index(int index, int* conj_symm, int* conj_ld, int* conj_ld_symm)
{
	int wq = index % 3;
	index /= 3;
	int bq = index % 3;
	index /= 3;
	int wr = index % 3;
	index /= 3;
	int br = index % 3;
	index /= 3;
	int wl = index % 2;
	index /= 2;
	int bl = index % 2;
	index /= 2;
	int wd = index % 2;
	index /= 2;
	int bd = index % 2;
	index /= 2;
	int wn = index % 3;
	index /= 3;
	int bn = index % 3;
	index /= 3;
	int wp = index % 9;
	index /= 9;
	int bp = index;
	*conj_symm = -1;
	*conj_ld = -1;
	*conj_ld_symm = -1;
	if (wq != bq || wr != br || wl != bd || wd != bl || wn != bn || wp != bp)
	{
		*conj_symm = get_mat_index(bq, wq, br, wr, bd, wd, bl, wl, bn, wn, bp, wp);
		if (wl != wd || bl != bd)
		{
			*conj_ld = get_mat_index(wq, bq, wr, br, wd, bd, wl, bl, wn, bn, wp, bp);
			*conj_ld_symm = get_mat_index(bq, wq, br, wr, bl, wl, bd, wd, bn, wn, bp, wp);
		}
	}
	return *conj_symm;
}
void pgn_stat()
{
#define elo_eval_ratio 1.0
#define PosInRow 6
#define ratio_from_eval(x) ratio_from_elo(elo_eval_ratio*(x))
#define ind_from_eval(x)                                                                               \
    (ratio_from_eval((double)(x)) >= 0.5 ? Max(0, (int)((ratio_from_eval((double)(x)) - 0.5) * 100.0)) \
                                         : Max(0, (int)((0.5 - ratio_from_eval((double)(x))) * 100.0)))
#define est_from_ind(x) (Eval[x].score / Max(1.0, (double)Eval[x].cnt))
#define est_from_eval(x) ((x) >= 0 ? est_from_ind(ind_from_eval(x)) : (1.0 - est_from_ind(ind_from_eval(x))))
	typedef struct
	{
		double score;
		double est;
		int cnt;
	} GStat;
	GStat Eval[64];
	for (int i = 0; i < 64; ++i)
	{
		Eval[i].cnt = 0;
		Eval[i].score = 1.0;
	}
	GStat* Mat = (GStat*)malloc(TotalMat * sizeof(GStat));
	memset(Mat, 0, TotalMat * sizeof(GStat));
	FILE* fpgn;
	int iter = 0;
loop:
	fpgn = fopen("D:/Development/G3T/games.pgn", "r");
	if (fpgn == nullptr)
	{
		fprintf(stdout, "File 'games.pgn' not found\n");
		getchar();
		exit(0);
	}
	double stat = 0.0, est = 0.0;
	int cnt = 0;
	while (true)
	{
		fgets(mstring, 65536, fpgn);
		if (feof(fpgn))
			break;
		if (strstr(mstring, "FEN"))
			get_board(mstring + 6);
		double result;
		if (strstr(mstring, "Result"))
		{
			if (strstr(mstring, "1-0"))
				result = 1.0;
			else if (strstr(mstring, "0-1"))
				result = 0.0;
			else
				result = 0.5;
		}
		if (strchr(mstring, '['))
			continue;
		if (strlen(mstring) < 100)
			continue;
		char* ptr = mstring;
		int eval[20], nc = 0;
		memset(eval, 0, 20 * sizeof(int));
		while (*ptr != 0)
		{
			if (!Current->capture && !(Current->move & 0xE000))
				++nc;
			else
				nc = 0;
			evaluate();
			if (nc == PosInRow)
			{
				double ratio = ratio_from_eval((double)eval[PosInRow - 2]);
				int r_index;
				if (ratio >= 0.5)
					r_index = Max(0, (int)((ratio - 0.5) * 100.0));
				else
					r_index = Max(0, (int)((0.5 - ratio) * 100.0));
				if (Even(iter))
				{
					Eval[r_index].cnt++;
					Eval[r_index].score += (ratio >= 0.5 ? result : (1.0 - result));
				}

				if (!(Current->material & FlagUnusualMaterial) && Odd(iter))
				{
					int index = Current->material, conj_symm, conj_ld, conj_ld_symm;
					conj_mat_index(index, &conj_symm, &conj_ld, &conj_ld_symm);
					Mat[index].cnt++;
					Mat[index].score += result;
					Mat[index].est += est_from_eval(eval[PosInRow - 1]);
					if (conj_symm >= 0)
						Mat[conj_symm].cnt++;
					Mat[conj_symm].score += 1.0 - result;
					Mat[conj_symm].est += 1.0 - est_from_eval(eval[PosInRow - 1]);
					if (conj_ld >= 0)
						Mat[conj_ld].cnt++;
					Mat[conj_ld].score += result;
					Mat[conj_ld].est += est_from_eval(eval[PosInRow - 1]);
					if (conj_ld_symm >= 0)
						Mat[conj_ld_symm].cnt++;
					Mat[conj_ld_symm].score += 1.0 - result;
					Mat[conj_ld_symm].est += 1.0 - est_from_eval(eval[PosInRow - 1]);
				}
			}
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
			int move = move_from_string(pv_string);
			if (Current->turn)
			{
				if (!is_legal<1>(move))
					break;
				do_move<1>(move);
			}
			else
			{
				if (!is_legal<0>(move))
					break;
				do_move<0>(move);
			}
			memcpy(Data, Current, sizeof(GData));
			Current = Data;
			while (*ptr == ' ') ++ptr;
			for (int i = 19; i >= 1; --i) eval[i] = eval[i - 1];
			eval[0] = (int)(atof(ptr + 2) * 100.0);
			ptr = strchr(ptr, '}') + 2;
		}
	}
	fclose(fpgn);
	if (!iter)
	{
		for (int i = 0; i < 64; ++i)
			fprintf(stdout, "ratio(eval x %.2lf) in (%.2lf, %.2lf), score = %.2lf\n", elo_eval_ratio, 50.0 + (double)i, 50.0 + (double)(i + 1),
				(Eval[i].score * 100.0) / Max(1.0, (double)Eval[i].cnt));
		++iter;
		goto loop;
	}
	FILE* fmat = fopen("material.txt", "w");
	fprintf(fmat, "const int MaterialShift[MaterialShiftSize] = {\n");
	int mat_cnt = 0;
	for (int index = 0; index < TotalMat; ++index)
	{
		int cnt = Mat[index].cnt;
		if (cnt < 64)
			continue;
		double ratio = Mat[index].score / (double)cnt;
		double est = Mat[index].est / (double)cnt;
		bool flag = (ratio < est);
		double error = (sqrt(2.0) * 0.5) / sqrt((double)cnt);
		if (abs(ratio - est) <= error + 0.01)
			continue;
		ratio = ((ratio >= est) ? (ratio - error) : (ratio + error));
		if (est <= 0.5 && ratio > 0.5)
			ratio = 0.5000001;
		if (est >= 0.5 && ratio < 0.5)
			ratio = 0.4999999;
		double abs_ratio = ((ratio >= 0.5) ? ratio : (1.0 - ratio));
		double abs_est = ((est >= 0.5) ? est : (1.0 - est));
		int curr_ind = 0, new_ind = 0;
		for (int i = 0; i < 64; ++i)
		{
			if (Eval[i].score / Max(1.0, (double)Eval[i].cnt) < abs_ratio)
				new_ind = i;
			if (Eval[i].score / Max(1.0, (double)Eval[i].cnt) < abs_est)
				curr_ind = i;
		}
		if (abs(curr_ind - new_ind) <= 1)
			continue;
		if (new_ind > curr_ind)
			--new_ind;
		else
			++new_ind;
		double curr_eval = elo_from_ratio(Eval[curr_ind].score / Max(1.0, (double)Eval[curr_ind].cnt)) / elo_eval_ratio;
		double new_eval = elo_from_ratio(Eval[new_ind].score / Max(1.0, (double)Eval[new_ind].cnt)) / elo_eval_ratio;
		int score = (int)abs(curr_eval - new_eval);
		if (flag)
			score = -score;
		if (score * Material[index].score < 0)
			score = Sgn(score) * Min(abs(Material[index].score), abs(score));
		if (abs(score) < 5)
			continue;
		++mat_cnt;
		fprintf(fmat, "%d, %d, ", index, score);
		if ((mat_cnt % 8) == 0)
			fprintf(fmat, "\n");
	}
	fprintf(fmat, "}; %d\n", mat_cnt * 2);
	fclose(fmat);
	fprintf(stdout, "Press any key...\n");
}
#endif

uint64 BMagicAttacks(int i, uint64 occ)
{
	uint64 att = 0;
	for (uint64 u = BMask[i]; T(u); Cut(u))
		if (F(Between[i][lsb(u)] & occ))
			att |= Between[i][lsb(u)] | Bit(lsb(u));
	return att;
}

uint64 RMagicAttacks(int i, uint64 occ)
{
	uint64 att = 0;
	for (uint64 u = RMask[i]; T(u); Cut(u))
		if (F(Between[i][lsb(u)] & occ))
			att |= Between[i][lsb(u)] | Bit(lsb(u));
	return att;
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
	cout << Current->patt[(seed >> 32) | (seed << 32)];
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

void init_misc()
{
	array<uint64, 64> HLine;
	array<uint64, 64> NDiag;
	array<uint64, 64> SDiag;

	for (int i = 0; i < 64; ++i)
	{
		HLine[i] = VLine[i] = NDiag[i] = SDiag[i] = RMask[i] = BMask[i] = QMask[i] = 0;
		BMagicMask[i] = RMagicMask[i] = NAtt[i] = KAtt[i] = KAttAtt[i] = NAttAtt[i] = 0;
		PAtt[0][i] = PAtt[1][i] = PMove[0][i] = PMove[1][i] = PWay[0][i] = PWay[1][i] = PCone[0][i] = PCone[1][i] 
				= PSupport[0][i] = PSupport[1][i] = BishopForward[0][i] = BishopForward[1][i] = 0;
		for (int j = 0; j < 64; ++j) 
			Between[i][j] = FullLine[i][j] = 0;
	}

	for (int i = 0; i < 64; ++i)
	{
		for (int j = 0; j < 64; ++j)
		{
			if (i == j)
				continue;
			uint64 u = Bit(j);
			if (FileOf(i) == FileOf(j))
				VLine[i] |= u;
			if (RankOf(i) == RankOf(j))
				HLine[i] |= u;  // thus HLine[i] = Rank[RankOf(i)] ^ Bit(i)
			if (NDiagOf(i) == NDiagOf(j))
				NDiag[i] |= u;
			if (SDiagOf(i) == SDiagOf(j))
				SDiag[i] |= u;
			if (Dist(i, j) <= 2)
			{
				KAttAtt[i] |= u;
				if (Dist(i, j) <= 1)
					KAtt[i] |= u;
				if (abs(RankOf(i) - RankOf(j)) + abs(FileOf(i) - FileOf(j)) == 3)
					NAtt[i] |= u;
			}
			if (j == i + 8)
				PMove[0][i] |= u;
			if (j == i - 8)
				PMove[1][i] |= u;
			if (abs(FileOf(i) - FileOf(j)) == 1)
			{
				if (RankOf(j) >= RankOf(i))
				{
					PSupport[1][i] |= u;
					if (RankOf(j) - RankOf(i) == 1)
						PAtt[0][i] |= u;
				}
				if (RankOf(j) <= RankOf(i))
				{
					PSupport[0][i] |= u;
					if (RankOf(i) - RankOf(j) == 1)
						PAtt[1][i] |= u;
				}
			}
			else if (FileOf(i) == FileOf(j))
			{
				if (RankOf(j) > RankOf(i))
					PWay[0][i] |= u;
				else
					PWay[1][i] |= u;
			}
		}

		RMask[i] = HLine[i] | VLine[i];
		BMask[i] = NDiag[i] | SDiag[i];
		QMask[i] = RMask[i] | BMask[i];
		BMagicMask[i] = BMask[i] & Interior;
		RMagicMask[i] = RMask[i];
		PCone[0][i] = PWay[0][i];
		PCone[1][i] = PWay[1][i];
		if (FileOf(i) > 0)
		{
			RMagicMask[i] &= ~File[0];
			PCone[0][i] |= PWay[0][i - 1];
			PCone[1][i] |= PWay[1][i - 1];
		}
		if (RankOf(i) > 0)
			RMagicMask[i] &= ~Line[0];
		if (FileOf(i) < 7)
		{
			RMagicMask[i] &= ~File[7];
			PCone[0][i] |= PWay[0][i + 1];
			PCone[1][i] |= PWay[1][i + 1];
		}
		if (RankOf(i) < 7)
			RMagicMask[i] &= ~Line[7];
	}
	for (int i = 0; i < 64; ++i)
		for (int j = 0; j < 64; ++j)
			if (NAtt[i] & NAtt[j])
				NAttAtt[i] |= Bit(j);

	for (int i = 0; i < 8; ++i)
	{
		West[i] = 0;
		East[i] = 0;
		Forward[0][i] = Forward[1][i] = 0;
		PIsolated[i] = 0;
		for (int j = 0; j < 8; ++j)
		{
			if (i < j)
				Forward[0][i] |= Line[j];
			else if (i > j)
				Forward[1][i] |= Line[j];
			if (i < j)
				East[i] |= File[j];
			else if (i > j)
				West[i] |= File[j];
		}
		if (i > 0)
			PIsolated[i] |= File[i - 1];
		if (i < 7)
			PIsolated[i] |= File[i + 1];
	}
	for (int i = 0; i < 64; ++i)
	{
		for (uint64 u = QMask[i]; T(u); Cut(u))
		{
			int j = lsb(u);
			int k = Sgn(RankOf(j) - RankOf(i));
			int l = Sgn(FileOf(j) - FileOf(i));
			for (int n = i + 8 * k + l; n != j; n += (8 * k + l)) 
				Between[i][j] |= Bit(n);
		}
		for (uint64 u = BMask[i]; T(u); Cut(u))
		{
			int j = lsb(u);
			FullLine[i][j] = BMask[i] & BMask[j];
		}
		for (uint64 u = RMask[i]; T(u); Cut(u))
		{
			int j = lsb(u);
			FullLine[i][j] = RMask[i] & RMask[j];
		}
		BishopForward[0][i] |= PWay[0][i];
		BishopForward[1][i] |= PWay[1][i];
		for (int j = 0; j < 64; j++)
		{
			if ((PWay[1][j] | Bit(j)) & BMask[i] & Forward[0][RankOf(i)])
				BishopForward[0][i] |= Bit(j);
			if ((PWay[0][j] | Bit(j)) & BMask[i] & Forward[1][RankOf(i)])
				BishopForward[1][i] |= Bit(j);
		}
	}

	for (int i = 0; i < 16; ++i)
		for (int j = 0; j < 16; ++j)
		{
			if (j < WhitePawn)
				MvvLva[i][j] = 0;
			else if (j < WhiteKnight)
				MvvLva[i][j] = PawnCaptureMvvLva(i) << 26;
			else if (j < WhiteLight)
				MvvLva[i][j] = KnightCaptureMvvLva(i) << 26;
			else if (j < WhiteRook)
				MvvLva[i][j] = BishopCaptureMvvLva(i) << 26;
			else if (j < WhiteQueen)
				MvvLva[i][j] = RookCaptureMvvLva(i) << 26;
			else
				MvvLva[i][j] = QueenCaptureMvvLva(i) << 26;
		}

	for (int i = 0; i < 256; ++i) 
		PieceFromChar[i] = 0;
	PieceFromChar[66] = 6;
	PieceFromChar[75] = 14;
	PieceFromChar[78] = 4;
	PieceFromChar[80] = 2;
	PieceFromChar[81] = 12;
	PieceFromChar[82] = 10;
	PieceFromChar[98] = 7;
	PieceFromChar[107] = 15;
	PieceFromChar[110] = 5;
	PieceFromChar[112] = 3;
	PieceFromChar[113] = 13;
	PieceFromChar[114] = 11;

	TurnKey = rand64();
	for (int i = 0; i < 8; ++i) EPKey[i] = rand64();
	for (int i = 0; i < 16; ++i) CastleKey[i] = rand64();
	for (int i = 0; i < 16; ++i)
		for (int j = 0; j < 64; ++j)
			PieceKey[i][j] = i ? rand64() : 0;
	for (int i = 0; i < 16; ++i)
		LogDist[i] = (int)(10.0 * log(1.01 + i));
	for (int i = 1; i < 256; ++i)
		SpanWidth[i] = 1 + msb(i) - lsb(i);
	SpanWidth[0] = 0;
}

void init_magic()
{
#ifdef TUNER
	MagicAttacks = (uint64*)malloc(MAGIC_SIZE * sizeof(uint64));
#endif
	for (int i = 0; i < 64; ++i)
	{
		int bits = 64 - BShift[i];
		array<int, 16> bit_list = { 0 };
		uint64 u = BMagicMask[i];
		for (int j = 0; T(u); Cut(u), ++j) 
			bit_list[j] = lsb(u);
		for (int j = 0; j < Bit(bits); ++j)
		{
			u = 0;
			for (int k = 0; k < bits; ++k)
				if (Odd(j >> k))
					u |= Bit(bit_list[k]);
#ifndef HNI
			int index = static_cast<int>(BOffset[i] + ((BMagic[i] * u) >> BShift[i]));
#else
			int index = static_cast<int>(BOffset[i] + _pext_u64(u, BMagicMask[i]));
#endif
			MagicAttacks[index] = BMagicAttacks(i, u);
		}
		bits = 64 - RShift[i];
		u = RMagicMask[i];
		for (int j = 0; T(u); Cut(u), ++j) 
			bit_list[j] = lsb(u);
		for (int j = 0; j < Bit(bits); ++j)
		{
			u = 0;
			for (int k = 0; k < bits; ++k)
				if (Odd(j >> k))
					u |= Bit(bit_list[k]);
#ifndef HNI
			int index = static_cast<int>(ROffset[i] + ((RMagic[i] * u) >> RShift[i]));
#else
			int index = static_cast<int>(ROffset[i] + _pext_u64(u, RMagicMask[i]));
#endif
			MagicAttacks[index] = RMagicAttacks(i, u);
		}
	}
}

void gen_kpk()
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
					if (PAtt[White][wp] & bbk)
					{
						if (turn == White)
							goto set_draw;
						else if (F(KAtt[wk] & bwp))
							goto set_draw;
					}
					un = 0;
					if (turn == Black)
					{
						u = KAtt[bk] & (~(KAtt[wk] | PAtt[White][wp]));
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
				Kpk[turn][wp][wk] = 0;
				for (bk = 0; bk < 64; ++bk)
				{
					if (Kpk_gen[turn][wp][wk][bk] == 2)
						Kpk[turn][wp][wk] |= Bit(bk);
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

void init_pst()
{
	fill(PstVals.begin(), PstVals.end(), 0);

	for (int i = 0; i < 64; ++i)
	{
		int r = RankOf(i);
		int f = FileOf(i);
		int d = abs(f - r);
		int e = abs(f + r - 7);
		array<int, 4> distL = { DistC[f], DistC[r],  RankR[d] + RankR[e], RankR[r] };
		array<int, 4> distQ = { DistC[f] * DistC[f], DistC[r] * DistC[r], RankR[d] * RankR[d] + RankR[e] * RankR[e], RankR[r] * RankR[r] };
		array<int, 2> distM = { DistC[f] * DistC[r], DistC[f] * RankR[r] };
		for (int j = 2; j < 16; j += 2)
		{
			int index = PieceType[j];
			int op = 0, md = 0, eg = 0, cl = 0;
			for (int k = 0; k < 2; ++k)
			{
				op += Av(PstQuadMixedWeights, 8, index, (k * 4)) * distM[k];
				md += Av(PstQuadMixedWeights, 8, index, (k * 4) + 1) * distM[k];
				eg += Av(PstQuadMixedWeights, 8, index, (k * 4) + 2) * distM[k];
				cl += Av(PstQuadMixedWeights, 8, index, (k * 4) + 3) * distM[k];
			}
			for (int k = 0; k < 4; ++k)
			{
				op += Av(PstQuadWeights, 16, index, (k * 4)) * distQ[k];
				md += Av(PstQuadWeights, 16, index, (k * 4) + 1) * distQ[k];
				eg += Av(PstQuadWeights, 16, index, (k * 4) + 2) * distQ[k];
				cl += Av(PstQuadWeights, 16, index, (k * 4) + 3) * distQ[k];
				op += Av(PstLinearWeights, 16, index, (k * 4)) * distL[k];
				md += Av(PstLinearWeights, 16, index, (k * 4) + 1) * distL[k];
				eg += Av(PstLinearWeights, 16, index, (k * 4) + 2) * distL[k];
				cl += Av(PstLinearWeights, 16, index, (k * 4) + 3) * distL[k];
			}
			// Regularize(&op, &md, &eg);
			Pst(j, i) = Pack4(op / 64, md / 64, eg / 64, cl / 64);
		}
	}

	Pst(WhiteKnight, 56) -= Pack2(100 * CP_EVAL, 0);
	Pst(WhiteKnight, 63) -= Pack2(100 * CP_EVAL, 0);
	// now for black
	for (int i = 0; i < 64; ++i)
		for (int j = 3; j < 16; j += 2)
		{
			auto src = Pst(j - 1, 63 - i);
			Pst(j, i) = Pack4(-Opening(src), -Middle(src), -Endgame(src), -Closed(src));
		}

	Current->pst = 0;
	for (int i = 0; i < 64; ++i)
		if (PieceAt(i))
			Current->pst += Pst(PieceAt(i), i);
}

double KingLocusDist(int x, int y)
{
	return sqrt(1.0 * Square(RankOf(x) - RankOf(y)) + Square(FileOf(x) - FileOf(y)));
}
uint64 make_klocus(int k_loc)
{
	constexpr double CENTER_WEIGHT = 1.0;	// relative importance of center vis-a-vis king
	if (N_LOCUS <= 0)
		return 0ull;
	array<pair<double, int>, 64> temp;
	for (int ii = 0; ii < 64; ++ii)
	{
		auto kDist = KingLocusDist(k_loc, ii);
		auto centerDist = sqrt(Square(3.5 - RankOf(ii)) + Square(3.5 - FileOf(ii)));
		auto useDist = CENTER_WEIGHT * centerDist + kDist;
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


void init_eval()
{
	init_mobility(MobCoeffsKnight, &MobKnight);
	init_mobility(MobCoeffsBishop, &MobBishop);
	init_mobility(MobCoeffsRook, &MobRook);
	init_mobility(MobCoeffsQueen, &MobQueen);
	for (int i = 0; i < 64; ++i)
		KingLocus[i] = make_klocus(i);

	for (int i = 0; i < 3; ++i)
		for (int j = 7; j >= 0; --j)
		{
			Shelter[i][j] = 0;
			for (int k = 1; k < Min(j, 5); ++k)
				Shelter[i][j] += Av(ShelterValue, 5, i, k - 1);
			if (!j)  // no pawn in file
				Shelter[i][j] = Shelter[i][7] + Av(ShelterValue, 5, i, 4);
		}

	for (int i = 0; i < 4; ++i)
	{
		StormBlocked[i] = ((Sa(StormQuad, StormBlockedMul) * i * i) + (Sa(StormLinear, StormBlockedMul) * (i + 1))) / 100;
		StormShelterAtt[i] = ((Sa(StormQuad, StormShelterAttMul) * i * i) + (Sa(StormLinear, StormShelterAttMul) * (i + 1))) / 100;
		StormConnected[i] = ((Sa(StormQuad, StormConnectedMul) * i * i) + (Sa(StormLinear, StormConnectedMul) * (i + 1))) / 100;
		StormOpen[i] = ((Sa(StormQuad, StormOpenMul) * i * i) + (Sa(StormLinear, StormOpenMul) * (i + 1))) / 100;
		StormFree[i] = ((Sa(StormQuad, StormFreeMul) * i * i) + (Sa(StormLinear, StormFreeMul) * (i + 1))) / 100;
	}

	for (int i = 0; i < 8; ++i)
	{
		int im2 = Max(i - 2, 0);
		auto quad1 = [&](int row, int col, int rr) { return (Av(PasserQuad, 4, row, col) * rr + Av(PasserLinear, 4, row, col)) * rr + Av(PasserConstant, 4, row, col); };
		auto quad = [&](int row, int col) { return quad1(row, col, 5) > 0 ? Max(0, quad1(row, col, im2)) : Min(0, quad1(row, col, im2)); }; // no sign changes please
		auto pack16ths = [&](int which) { return Pack4(quad(which, 0) / 16, quad(which, 1) / 16, quad(which, 2) / 16, quad(which, 3) / 16); };
		PasserGeneral[i] = pack16ths(0);
		PasserBlocked[i] = pack16ths(1);
		PasserFree[i] = pack16ths(2);
		PasserSupported[i] = pack16ths(3);
		PasserProtected[i] = pack16ths(4);
		PasserConnected[i] = pack16ths(5);
		PasserOutside[i] = pack16ths(6);
		PasserCandidate[i] = pack16ths(7);
		PasserClear[i] = pack16ths(8);

		auto attdef = [&](int k) { return PasserAttDefQuad[k] * im2*im2 + PasserAttDefLinear[k] * im2 + PasserAttDefConst[k]; };
		PasserAtt[i] = attdef(0);
		PasserDef[i] = attdef(2);
		PasserAttLog[i] = attdef(1);
		PasserDefLog[i] = attdef(3);
	}
}

// all these special-purpose endgame evaluators

template <bool me> int krbkrx()
{
	if (King(opp) & Interior)
		return 1;
	return 16;
}

template <bool me> int kpkx()
{
	uint64 u = me == White ? Kpk[Current->turn][lsb(Pawn(White))][lsb(King(White))] & Bit(lsb(King(Black)))
		: Kpk[Current->turn ^ 1][63 - lsb(Pawn(Black))][63 - lsb(King(Black))] & Bit(63 - lsb(King(White)));
	return T(u) ? 32 : T(Piece(opp) ^ King(opp));
}

template <bool me> int knpkx()
{
	if (Pawn(me) & OwnLine(me, 6) & (File[0] | File[7]))
	{
		int sq = lsb(Pawn(me));
		if (KAtt[sq] & King(opp) & (OwnLine(me, 6) | OwnLine(me, 7)))
			return 0;
		if (PieceAt(sq + Push[me]) == IKing[me] && (KAtt[lsb(King(me))] && KAtt[lsb(King(opp))] & OwnLine(me, 7)))
			return 0;
	}
	else if (Pawn(me) & OwnLine(me, 5) & (File[0] | File[7]))
	{
		int sq = lsb(Pawn(me));
		if (PieceAt(sq + Push[me]) == IPawn[opp])
		{
			if (KAtt[sq + Push[me]] & King(opp) & OwnLine(me, 7))
				return 0;
			if ((KAtt[sq + Push[me]] & KAtt[lsb(King(opp))] & OwnLine(me, 7)) && (!(NAtt[sq + Push[me]] & Knight(me)) || Current->turn == opp))
				return 0;
		}
	}
	return 32;
}

template <bool me> int krpkrx()
{
	int mul = 32;
	int sq = lsb(Pawn(me));
	int rrank = OwnRank<me>(sq);
	int o_king = lsb(King(opp));
	int o_rook = lsb(Rook(opp));
	int m_king = lsb(King(me));
	int add_mat = T(Piece(opp) ^ King(opp) ^ Rook(opp));
	int clear = F(add_mat) || F((PWay[opp][sq] | PIsolated[FileOf(sq)]) & Forward[opp][RankOf(sq + Push[me])] & (Piece(opp) ^ King(opp) ^ Rook(opp)));

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

	if (PWay[me][sq] & King(opp))
	{
		if (Pawn(me) & (File[0] | File[7]))
			mul = Min(mul, add_mat << 3);
		if (rrank <= 3)
			mul = Min(mul, add_mat << 3);
		if (rrank == 4 && OwnRank<me>(m_king) <= 4 && OwnRank<me>(o_rook) == 5 && T(King(opp) & (OwnLine(me, 6) | OwnLine(me, 7))) &&
			(!MY_TURN || F(PAtt[me][sq] & RookAttacks(lsb(Rook(me)), PieceAll()) & (~KAtt[o_king]))))
			mul = Min(mul, add_mat << 3);
		if (rrank >= 5 && OwnRank<me>(o_rook) <= 1 && (!MY_TURN || IsCheck(me) || Dist(m_king, sq) >= 2))
			mul = Min(mul, add_mat << 3);
		if (T(King(opp) & (File[1] | File[2] | File[6] | File[7])) && T(Rook(opp) & OwnLine(me, 7)) && T(Between[o_king][o_rook] & (File[3] | File[4])) &&
			F(Rook(me) & OwnLine(me, 7)))
			mul = Min(mul, add_mat << 3);
		return mul;
	}
	else if (rrank == 6 && (Pawn(me) & (File[0] | File[7])) && ((PSupport[me][sq] | PWay[opp][sq]) & Rook(opp)) && OwnRank<me>(o_king) >= 6)
	{
		int dist = abs(FileOf(sq) - FileOf(o_king));
		if (dist <= 3)
			mul = Min(mul, add_mat << 3);
		if (dist == 4 && ((PSupport[me][o_king] & Rook(me)) || Current->turn == opp))
			mul = Min(mul, add_mat << 3);
	}

	if (KAtt[o_king] & PWay[me][sq] & OwnLine(me, 7))
	{
		if (rrank <= 4 && OwnRank<me>(m_king) <= 4 && OwnRank<me>(o_rook) == 5)
			mul = Min(mul, add_mat << 3);
		if (rrank == 5 && OwnRank<me>(o_rook) <= 1 && !MY_TURN || (F(KAtt[m_king] & PAtt[me][sq] & (~KAtt[o_king])) && (IsCheck(me) || Dist(m_king, sq) >= 2)))
			mul = Min(mul, add_mat << 3);
	}

	if (T(PWay[me][sq] & Rook(me)) && T(PWay[opp][sq] & Rook(opp)))
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

template <bool me> int krpkbx()
{
	if (!(Pawn(me) & OwnLine(me, 5)))
		return 32;
	int sq = lsb(Pawn(me));
	if (!(PWay[me][sq] & King(opp)))
		return 32;
	int diag_sq = NB<me>(BMask[sq + Push[me]]);
	if (OwnRank<me>(diag_sq) > 1)
		return 32;
	uint64 mdiag = FullLine[sq + Push[me]][diag_sq] | Bit(sq + Push[me]) | Bit(diag_sq);
	int check_sq = NB<me>(BMask[sq - Push[me]]);
	uint64 cdiag = FullLine[sq - Push[me]][check_sq] | Bit(sq - Push[me]) | Bit(check_sq);
	if ((mdiag | cdiag) & (Piece(opp) ^ King(opp) ^ Bishop(opp)))
		return 32;
	if (cdiag & Bishop(opp))
		return 0;
	if ((mdiag & Bishop(opp)) && (Current->turn == opp || !(King(me) & PAtt[opp][sq + Push[me]])))
		return 0;
	return 32;
}

template <bool me> int kqkp()
{
	if (F(KAtt[lsb(King(opp))] & Pawn(opp) & OwnLine(me, 1) & (File[0] | File[2] | File[5] | File[7])))
		return 32;
	if (PWay[opp][lsb(Pawn(opp))] & (King(me) | Queen(me)))
		return 32;
	if (Pawn(opp) & (File[0] | File[7]))
		return 1;
	else
		return 4;
}

template <bool me> int kqkrpx()
{
	int rsq = lsb(Rook(opp));
	uint64 pawns = KAtt[lsb(King(opp))] & PAtt[me][rsq] & Pawn(opp) & Interior & OwnLine(me, 6);
	if (pawns && OwnRank<me>(lsb(King(me))) <= 4)
		return 0;
	return 32;
}

template <bool me> int krkpx()
{
	if (T(KAtt[lsb(King(opp))] & Pawn(opp) & OwnLine(me, 1)) & F(PWay[opp][NB<me>(Pawn(opp))] & King(me)))
		return 0;
	return 32;
}

template <bool me> int krppkrpx()
{
	if (Current->passer & Pawn(me))
	{
		if (Single(Current->passer & Pawn(me)))
		{
			int sq = lsb(Current->passer & Pawn(me));
			if (PWay[me][sq] & King(opp) & (File[0] | File[1] | File[6] | File[7]))
			{
				int opp_king = lsb(King(opp));
				if (KAtt[opp_king] & Pawn(opp))
				{
					int king_file = FileOf(opp_king);
					if (!((~(File[king_file] | PIsolated[king_file])) & Pawn(me)))
						return 1;
				}
			}
		}
		return 32;
	}
	if (F((~(PWay[opp][lsb(King(opp))] | PSupport[me][lsb(King(opp))])) & Pawn(me)))
		return 0;
	return 32;
}

template <bool me> int krpppkrppx()
{
	if (T(Current->passer & Pawn(me)) || F((KAtt[lsb(Pawn(opp))] | KAtt[msb(Pawn(opp))]) & Pawn(opp)))
		return 32;
	if (F((~(PWay[opp][lsb(King(opp))] | PSupport[me][lsb(King(opp))])) & Pawn(me)))
		return 0;
	return 32;
}

template <bool me> int kbpkbx()
{
	int sq = lsb(Pawn(me));
	uint64 u;
	if ((T(Board->bb[ILight[me]]) && T(Board->bb[IDark[opp]])) || (T(Board->bb[IDark[me]]) && T(Board->bb[ILight[opp]])))
	{
		if (OwnRank<me>(sq) <= 4)
			return 0;
		if (T(PWay[me][sq] & King(opp)) && OwnRank<me>(sq) <= 5)
			return 0;
		for (u = Bishop(opp); T(u); Cut(u))
		{
			if (OwnRank<me>(lsb(u)) <= 4 && T(BishopAttacks(lsb(u), PieceAll()) & PWay[me][sq]))
				return 0;
			if (Current->turn == opp && T(BishopAttacks(lsb(u), PieceAll()) & Pawn(me)))
				return 0;
		}
	}
	else if (T(PWay[me][sq] & King(opp)) && T(King(opp) & LightArea) != T(Bishop(me) & LightArea))
		return 0;
	return 32;
}

template <bool me> int kbpknx()
{
	uint64 u;
	if (T(PWay[me][lsb(Pawn(me))] & King(opp)) && T(King(opp) & LightArea) != T(Bishop(me) & LightArea))
		return 0;
	if (Current->turn == opp)
		for (u = Knight(opp); T(u); Cut(u))
			if (NAtt[lsb(u)] & Pawn(me))
				return 0;
	return 32;
}

template <bool me> int kbppkbx()
{
	int sq1 = NB<me>(Pawn(me));
	int sq2 = NB<opp>(Pawn(me));
	int o_king = lsb(King(opp));
	int o_bishop = lsb(Bishop(opp));

	if (FileOf(sq1) == FileOf(sq2))
	{
		if (OwnRank<me>(sq2) <= 3)
			return 0;
		if (T(PWay[me][sq2] & King(opp)) && OwnRank<me>(sq2) <= 5)
			return 0;
	}
	else if (PIsolated[FileOf(sq1)] & Pawn(me))
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

template <bool me> int krppkrx()
{
	int sq1 = NB<me>(Pawn(me));
	int sq2 = NB<opp>(Pawn(me));

	if ((Piece(opp) ^ King(opp) ^ Rook(opp)) & Forward[me][RankOf(sq1 - Push[me])])
		return 32;
	if (FileOf(sq1) == FileOf(sq2))
	{
		if (T(PWay[me][sq2] & King(opp)))
			return 16;
	}
	else if (T(PIsolated[FileOf(sq2)] & Pawn(me)) && T((File[0] | File[7]) & Pawn(me)) && T(King(opp) & Shift<me>(Pawn(me))))
	{
		if (OwnRank<me>(sq2) == 5 && OwnRank<me>(sq1) == 4 && T(Rook(opp) & (OwnLine(me, 5) | OwnLine(me, 6))))
			return 10;
		else if (OwnRank<me>(sq2) < 5)
			return 16;
	}
	int r2 = lsb(Rook(opp)), rf = FileOf(r2);
	const uint64 mask = West[rf] & King(me) ? West[rf] : East[rf];
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

	if (F(Pawn(me) & (~PWay[opp][sq])))
	{
		if ((KAtt[sq] | Bit(sq)) & King(opp))
			EI.mul = 0;
		else if ((KAtt[sq] & KAtt[lsb(King(opp))] & OwnLine(me, 7)) && PieceAt(sq - Push[me]) == IPawn[opp] && PieceAt(sq - 2 * Push[me]) == IPawn[me])
			EI.mul = 0;
	}
	else if ((King(opp) & OwnLine(me, 6) | OwnLine(me, 7)) && abs(FileOf(sq) - FileOf(lsb(King(opp)))) <= 3 && !(Pawn(me) & (~PSupport[me][sq])) &&
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
	if (F(Pawn(me) & (~PWay[opp][sq])))
	{
		if ((KAtt[sq] | Bit(sq)) & King(opp))
			EI.mul = 0;
		else if ((KAtt[sq] & KAtt[lsb(King(opp))] & OwnLine(me, 7)) && PieceAt(sq - Push[me]) == IPawn[opp] && PieceAt(sq - 2 * Push[me]) == IPawn[me])
			EI.mul = 0;
	}
	else if ((King(opp) & OwnLine(me, 6) | OwnLine(me, 7)) && abs(FileOf(sq) - FileOf(lsb(King(opp)))) <= 3 && !(Pawn(me) & (~PSupport[me][sq])) &&
		(Pawn(me) & OwnLine(me, 5) & Shift<opp>(Pawn(opp))))
		EI.mul = 0;

	if (number == 1)
	{
		sq = lsb(Pawn(me));
		if ((Pawn(me) & (File[1] | File[6]) & OwnLine(me, 5)) && PieceAt(sq + Push[me]) == IPawn[opp] &&
			((PAtt[me][sq + Push[me]] | PWay[me][sq + Push[me]]) & King(opp)))
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
				if (!(PWay[me][sq] & (PAtt[me][EI.king[opp]] | PAtt[opp][EI.king[opp]])))
				{
					if (!(PWay[me][sq] & Pawn(opp)))
						break;
					int bsq = lsb(Bishop(opp));
					uint64 att = BishopAttacks(bsq, EI.occ);
					if (!(att & PWay[me][sq] & Pawn(opp)))
						break;
					if (!(BishopForward[me][bsq] & att & PWay[me][sq] & Pawn(opp)) && pop(FullLine[lsb(att & PWay[me][sq] & Pawn(opp))][bsq] & att) <= 2)
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
				EI.mul = Min(EI.mul, 8);
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
	constexpr uint64 AB = File[0] | File[1], ABC = AB | File[2];
	constexpr uint64 GH = File[6] | File[7], FGH = GH | File[5];
	if (F(Pawn(me) & ~AB) && T(King(opp) & ABC))
	{
		uint64 back = Forward[opp][RankOf(lsb(King(opp)))];
		if (T(back & Pawn(me)))
			EI.mul = Min(EI.mul, T(King(me) & AB & ~back) ? 24 : 8);
	}
	else if (F(Pawn(me) & ~GH) && T(King(opp) & FGH))
	{
		uint64 back = Forward[opp][RankOf(lsb(King(opp)))];
		if (T(back & Pawn(me)))
			EI.mul = Min(EI.mul, T(King(me) & GH & ~back) ? 24 : 8);
	}
}

template<bool me> inline void check_forced_stalemate(int* mul, int kloc)
{
	if (F(KAtt[kloc] & ~Current->att[me])
		&& F(Shift<opp>(Pawn(opp)) & ~PieceAll()))
		*mul -= (3 * *mul) / 4;
}
template<bool me> INLINE void check_forced_stalemate(int* mul)
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
}
template<bool me> void eval_krpkrx(GEvalInfo& EI, pop_func_t pop)
{
	assert(Rook(me) && Single(Rook(me)) && Pawn(me) && Single(Pawn(me)) && Rook(opp) && Single(Rook(opp)));
	int new_mul = krpkrx<me>();
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
//		EI.mul = Min(EI.mul, OneIn[lsb(King(opp))] & Queen(me) ? 4 : 0);
	check_forced_stalemate<me>(&EI.mul);
}


void calc_material(int index)
{
	array<int, 2> pawns, knights, light, dark, rooks, queens, bishops, major, minor, tot, count, mat, mul, closed;
	int i = index;
#ifdef TUNER
	Material[index].generation = generation;
#endif
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

	int phase = Phase[PieceType[WhitePawn]] * (pawns[White] + pawns[Black]) 
			+ Phase[PieceType[WhiteKnight]] * (knights[White] + knights[Black]) 
			+ Phase[PieceType[WhiteLight]] * (bishops[White] + bishops[Black]) 
			+ Phase[PieceType[WhiteRook]] * (rooks[White] + rooks[Black]) 
			+ Phase[PieceType[WhiteQueen]] * (queens[White] + queens[Black]);
	Material[index].phase = Min((Max(phase - PhaseMin, 0) * MAX_PHASE) / (PhaseMax - PhaseMin), MAX_PHASE);

	packed_t special = 0;
	for (int me = 0; me < 2; ++me)
	{
		if (queens[me] == queens[opp])
		{
			if (rooks[me] - rooks[opp] == 1)
			{
				if (knights[me] == knights[opp] && bishops[opp] - bishops[me] == 1)
					IncV(special, Ca4(MatSpecial, MatRB));
				else if (bishops[me] == bishops[opp] && knights[opp] - knights[me] == 1)
					IncV(special, Ca4(MatSpecial, MatRN));
				else if (knights[me] == knights[opp] && bishops[opp] - bishops[me] == 2)
					DecV(special, Ca4(MatSpecial, MatBBR));
				else if (bishops[me] == bishops[opp] && knights[opp] - knights[me] == 2)
					DecV(special, Ca4(MatSpecial, MatNNR));
				else if (bishops[opp] - bishops[me] == 1 && knights[opp] - knights[me] == 1)
					DecV(special, Ca4(MatSpecial, MatBNR));
			}
			else if (rooks[me] == rooks[opp])
			{
				if (minor[me] - minor[opp] == 1)
					IncV(special, Ca4(MatSpecial, MatM));
				else if (minor[me] == minor[opp] && pawns[me] > pawns[opp])
					IncV(special, Ca4(MatSpecial, MatPawnOnly));
			}
		}
		else if (queens[me] - queens[opp] == 1)
		{
			if (rooks[opp] - rooks[me] == 2 && minor[opp] - minor[me] == 0)
				IncV(special, Ca4(MatSpecial, MatQRR));
			else if (rooks[opp] - rooks[me] == 1 && knights[opp] == knights[me] && bishops[opp] - bishops[me] == 1)
				IncV(special, Ca4(MatSpecial, MatQRB));
			else if (rooks[opp] - rooks[me] == 1 && knights[opp] - knights[me] == 1 && bishops[opp] == bishops[me])
				IncV(special, Ca4(MatSpecial, MatQRN));
			else if ((major[opp] + minor[opp]) - (major[me] + minor[me]) >= 2)
				IncV(special, Ca4(MatSpecial, MatQ3));
		}
	}
	score += (Opening(special) * Material[index].phase + Endgame(special) * (MAX_PHASE - (int)Material[index].phase)) / MAX_PHASE;

	array<int, 2> quad = { 0, 0 };
	for (int me = 0; me < 2; me++) 
	{
		quad[me] += pawns[me] * (pawns[me] * TrAv(MatQuadMe, 5, 0, 0) 
				+ knights[me] * TrAv(MatQuadMe, 5, 0, 1)
				+ bishops[me] * TrAv(MatQuadMe, 5, 0, 2) 
				+ rooks[me] * TrAv(MatQuadMe, 5, 0, 3) 
				+ queens[me] * TrAv(MatQuadMe, 5, 0, 4));
		quad[me] += knights[me] * (knights[me] * TrAv(MatQuadMe, 5, 1, 0)
				+ bishops[me] * TrAv(MatQuadMe, 5, 1, 1) 
				+ rooks[me] * TrAv(MatQuadMe, 5, 1, 2) 
				+ queens[me] * TrAv(MatQuadMe, 5, 1, 3));
		quad[me] += bishops[me] * (bishops[me] * TrAv(MatQuadMe, 5, 2, 0) 
				+ rooks[me] * TrAv(MatQuadMe, 5, 2, 1) 
				+ queens[me] * TrAv(MatQuadMe, 5, 2, 2));
		quad[me] += rooks[me] * (rooks[me] * TrAv(MatQuadMe, 5, 3, 0) 
				+ queens[me] * TrAv(MatQuadMe, 5, 3, 1));

		quad[me] += pawns[me] * (knights[opp] * TrAv(MatQuadOpp, 4, 0, 0)
				+ bishops[opp] * TrAv(MatQuadOpp, 4, 0, 1) 
				+ rooks[opp] * TrAv(MatQuadOpp, 4, 0, 2) 
				+ queens[opp] * TrAv(MatQuadOpp, 4, 0, 3));
		quad[me] += knights[me] * (bishops[opp] * TrAv(MatQuadOpp, 4, 1, 0) 
				+ rooks[opp] * TrAv(MatQuadOpp, 4, 1, 1) 
				+ queens[opp] * TrAv(MatQuadOpp, 4, 1, 2));
		quad[me] += bishops[me] * (rooks[opp] * TrAv(MatQuadOpp, 4, 2, 0) 
				+ queens[opp] * TrAv(MatQuadOpp, 4, 2, 1));
		quad[me] += rooks[me] * queens[opp] * TrAv(MatQuadOpp, 4, 3, 0);

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
		else if (F(queens[me] + queens[opp] + minor[opp] + pawns[opp]) && rooks[me] == rooks[opp] && minor[me] == 1 && T(pawns[me]))	// RNP or RBP vs R
			mat[me] += 4 / (pawns[me] + rooks[me]);
		else if (F(queens[me] + minor[me] + major[opp] + pawns[opp]) && rooks[me] == minor[opp] && T(pawns[me]))	// RP vs minor
			mat[me] += 2;
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
		Material[index].mul[me] = mul[me];
	Material[index].score = (score * mat[score > 0 ? White : Black]) / 32;
	Material[index].closed = Closed(special) + closed[White] - closed[Black]; // *mat[score > 0 ? White : Black]) / 32;
	Material[index].eval = { nullptr, nullptr };
	for (int me = 0; me < 2; ++me)
	{
		if (F(major[me] + minor[me]))
			Material[index].eval[me] = TEMPLATE_ME(eval_pawns_only);
		else if (F(major[me]) && minor[me] == 1)
		{
			if (bishops[me])
				Material[index].eval[me] = pawns[me] ? TEMPLATE_ME(eval_single_bishop) : eval_unwinnable;
			else if (pawns[me] == 2 && bishops[opp] == 1)
				Material[index].eval[me] = TEMPLATE_ME(eval_knppkbx);
			else if (pawns[me] <= 1)
				Material[index].eval[me] = pawns[me] ? TEMPLATE_ME(eval_np) : eval_unwinnable;
		}
		else if (!pawns[me] && !queens[me] && rooks[me] == 1 && !knights[me] && bishops[me] == 1 && rooks[opp] == 1 && !minor[opp] && !queens[opp])
			Material[index].eval[me] = TEMPLATE_ME(eval_krbkrx);
		else if (F(minor[me]) && major[me] == 1)
		{
			if (rooks[me])
			{
				if (F(pawns[me]) && T(pawns[opp]))
					Material[index].eval[me] = TEMPLATE_ME(eval_krkpx);
				else if (rooks[opp] == 1)
				{
					if (pawns[me] == 1)
						Material[index].eval[me] = TEMPLATE_ME(eval_krpkrx);
					else if (pawns[me] == 2)
						Material[index].eval[me] = F(pawns[opp]) ? TEMPLATE_ME(eval_krppkrx) : TEMPLATE_ME(eval_krppkrpx);
					else if (pawns[me] == 3 && T(pawns[opp]))
						Material[index].eval[me] = TEMPLATE_ME(eval_krpppkrppx);
				}
				else if (pawns[me] == 1 && T(bishops[opp]))
					Material[index].eval[me] = TEMPLATE_ME(eval_krpkbx);
			}
			else if (F(pawns[me]) && T(pawns[opp]))
			{
				if (tot[opp] == 0 && pawns[opp] == 1)
					Material[index].eval[me] = TEMPLATE_ME(eval_kqkpx);
				else if (rooks[opp] == 1)
					Material[index].eval[me] = TEMPLATE_ME(eval_kqkrpx);
			}
		}
		else if (major[opp] + minor[opp] == 0)
			Material[index].eval[me] = eval_null;	// just force the stalemate check
	}
}

void init_material()
{
#ifdef TUNER
	Material = (GMaterial*)malloc(TotalMat * sizeof(GMaterial));
#endif
	memset(Material, 0, TotalMat * sizeof(GMaterial));
	for (int index = 0; index < TotalMat; ++index) 
		calc_material(index);

}

void init_hash()
{
#ifdef TUNER
	return;
#endif
	string name = "ROC_HASH_" + to_string(WinParId);
	sint64 size = hash_size * sizeof(GEntry);
	int initialized = 0;
	if (parent && HASH != nullptr)
	{
		initialized = 1;
		UnmapViewOfFile(Hash);
		CloseHandle(HASH);
	}
	if (parent)
	{
		HASH = nullptr;
#ifdef LARGE_PAGES
		if (LargePages)
		{
			if (HINSTANCE hDll = LoadLibrary(TEXT("kernel32.dll")))
			{
				typedef int(*GETLARGEPAGEMINIMUM)(void);
				if (GETLARGEPAGEMINIMUM pGetLargePageMinimum = (GETLARGEPAGEMINIMUM)GetProcAddress(hDll, "GetLargePageMinimum"))
				{
					int min_page_size = (*pGetLargePageMinimum)();
					if (size < min_page_size)
						size = min_page_size;
					if (!initialized)
					{
						TOKEN_PRIVILEGES tp;
						HANDLE hToken;
						OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken);
						LookupPrivilegeValue(nullptr, "SeLockMemoryPrivilege", &tp.Privileges[0].Luid);
						tp.PrivilegeCount = 1;
						tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
						AdjustTokenPrivileges(hToken, FALSE, &tp, 0, (PTOKEN_PRIVILEGES) nullptr, 0);
					}
					HASH = CreateFileMapping
							(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE | SEC_COMMIT | SEC_LARGE_PAGES, size >> 32, size & 0xFFFFFFFF, name.c_str());
					if (HASH)
						fprintf(stdout, "Large page hash\n");
				}
			}
		}
#endif
		if (!HASH)
			HASH = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, size >> 32, size & 0xFFFFFFFF, name.c_str());
	}
	else
		HASH = OpenFileMapping(FILE_MAP_ALL_ACCESS, 0, name.c_str());

	Hash = (GEntry*)MapViewOfFile(HASH, FILE_MAP_ALL_ACCESS, 0, 0, size);
	if (parent)
		memset(Hash, 0, size);
	hash_mask = hash_size - 4;
}

void init_shared()
{
#ifdef TUNER
	return;
#endif
	char name[256];
	DWORD size = SharedPVHashOffset + pv_hash_size * sizeof(GPVEntry);
	sprintf_s(name, "ROC_SHARED_%d", WinParId);
	if (parent && SHARED != NULL)
	{
		UnmapViewOfFile(Smpi);
		CloseHandle(SHARED);
	}
	if (parent)
		SHARED = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, size, name);
	else
		SHARED = OpenFileMapping(FILE_MAP_ALL_ACCESS, 0, name);
	Smpi = (GSMPI*)MapViewOfFile(SHARED, FILE_MAP_ALL_ACCESS, 0, 0, size);
	if (parent)
		memset(Smpi, 0, size);
	Material = (GMaterial*)(((char*)Smpi) + SharedMaterialOffset);
	MagicAttacks = (uint64*)(((char*)Smpi) + SharedMagicOffset);
	PVHash = (GPVEntry*)(((char*)Smpi) + SharedPVHashOffset);
	if (parent)
		memset(PVHash, 0, pv_hash_size * sizeof(GPVEntry));
}

void init()
{
	init_shared();
	init_misc();
	if (parent)
		init_magic();
	for (int i = 0; i < 64; ++i)
	{
		BOffsetPointer[i] = MagicAttacks + BOffset[i];
		ROffsetPointer[i] = MagicAttacks + ROffset[i];
	}
	gen_kpk();
	init_pst();
	init_eval();
	if (parent)
		init_material();
#ifdef EXPLAIN_EVAL
	memset(GullCppFile, 0, 16384 * 256);
	FILE* fcpp;
	fcpp = fopen(GullCpp, "r");
	for (cpp_length = 0; cpp_length < 16384; ++cpp_length)
	{
		if (feof(fcpp))
			break;
		fgets(mstring, 65536, fcpp);
		memcpy(GullCppFile[cpp_length], mstring, 256);
		if (!strchr(GullCppFile[cpp_length], '\n'))
			GullCppFile[cpp_length][255] = '\n';
		char* p;
		for (p = GullCppFile[cpp_length]; (*p) == '\t'; ++p)
			;
		strcpy(mstring, p);
		strcpy(GullCppFile[cpp_length], mstring);
	}
	--cpp_length;
	fclose(fcpp);
#endif
}

inline int HistInitPst(int piece, int to, bool good)
{
	// we don't know "from", only "to"
	// work around this by taking the RMS value of possible "from" PSTs (RMS because more-desirable squares are also
	// more-likely)
	if (piece < WhiteKing || piece > BlackQueen)
		return 1;
	uint64 att = 0;
	switch (piece & ~1)
	{
	case WhiteKnight:
		att = NAtt[to];
		break;
	case WhiteLight:
	case WhiteDark:
		att = BishopAttacks(to, att);
		break;
	case WhiteRook:
		att = RookAttacks(to, att);
		break;
	case WhiteQueen:
		att = QueenAttacks(to, att);
		break;
	}
	int nf = 0, sf = 0;
	for (; T(att); Cut(att))
	{
		int from = lsb(att);
		++nf;
		sf += Square(Middle(Pst(piece, from)));
	}
	if (nf == 0)
		return 1;
	int delta = Middle(Pst(piece, to)) - static_cast<int>(sqrt(double(sf) / nf));
	return Max(1, good ? delta : -delta);
}

void init_search(int clear_hash)
{
	for (int ih = 0; ih < 16 * 64; ++ih)
		HistoryVals[ih] = HistoryVals[ih + 16 * 64] = (1 << 8) | 2;	// Leave memory for joins, etc, in History

	memset(DeltaVals, 0, 16 * 4096 * sizeof(sint16));
	memset(Ref, 0, 16 * 64 * sizeof(GRef));
	memset(Data + 1, 0, 127 * sizeof(GData));
	if (clear_hash)
	{
		date = 0;
		date = 1;
		memset(Hash, 0, hash_size * sizeof(GEntry));
		memset(PVHash, 0, pv_hash_size * sizeof(GPVEntry));
	}
	get_board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	nodes = tb_hits = 0;
	best_move = best_score = 0;
	LastTime = LastValue = LastExactValue = InstCnt = 0;
	LastSpeed = 0;
	PVN = 1;
	Contempt = 8;
	Wobble = 0;
	Infinite = 1;
	SearchMoves = 0;
	TimeLimit1 = TimeLimit2 = 0;
	Stop = Searching = 0;
	if (MaxPrN > 1)
		ZERO_BIT_64(Smpi->searching, 0);
	DepthLimit = MAX_HEIGHT;
	LastDepth = MAX_HEIGHT;
	Print = 1;
	memset(CurrentSI, 0, sizeof(GSearchInfo));
	memset(BaseSI, 0, sizeof(GSearchInfo));
#ifdef CPU_TIMING
	GlobalTime[GlobalTurn] = UciBaseTime;
	GlobalInc[GlobalTurn] = UciIncTime;
#endif
}

void setup_board()
{
	int i;
	uint64 occ;

	occ = 0;
	sp = 0;
	++date;
	if (date > 0x8000)
	{  // mustn't ever happen
		date = 2;
		// now GUI must wait for readyok... we have plenty of time :)
		std::fill(Hash, Hash + hash_size, NullEntry);
		std::fill(PVHash, PVHash + pv_hash_size, NullPVEntry);
		//std::fill(PawnHash.begin(), PawnHash.end(), NullPawnEntry);
	}
	Current->material = 0;
	Current->pst = 0;
	Current->key = PieceKey[0][0];
	if (Current->turn)
		Current->key ^= TurnKey;
	Current->key ^= CastleKey[Current->castle_flags];
	if (Current->ep_square)
		Current->key ^= EPKey[FileOf(Current->ep_square)];
	Current->pawn_key = 0;
	Current->pawn_key ^= CastleKey[Current->castle_flags];
	for (i = 0; i < 16; ++i) Piece(i) = 0;
	for (i = 0; i < 64; ++i)
	{
		if (PieceAt(i))
		{
			Piece(PieceAt(i)) |= Bit(i);
			Piece(PieceAt(i) & 1) |= Bit(i);
			occ |= Bit(i);
			Current->key ^= PieceKey[PieceAt(i)][i];
			if (PieceAt(i) < WhiteKnight)
				Current->pawn_key ^= PieceKey[PieceAt(i)][i];
			if (PieceAt(i) < WhiteKing)
				Current->material += MatCode[PieceAt(i)];
			else
				Current->pawn_key ^= PieceKey[PieceAt(i)][i];
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

void get_board(const char fen[])
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
				PieceAt(i + j) = PieceFromChar[c];
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
}

INLINE GEntry* probe_hash()
{
	GEntry* start = Hash + (High32(Current->key) & hash_mask);
	for (GEntry* Entry = start; Entry < start + 4; ++Entry)
		if (Low32(Current->key) == Entry->key)
		{
			Entry->date = date;
			return Entry;
		}
	return nullptr;
}

INLINE GPVEntry* probe_pv_hash()
{
	GPVEntry* start = PVHash + (High32(Current->key) & pv_hash_mask);
	for (GPVEntry* PVEntry = start; PVEntry < start + pv_cluster_size; ++PVEntry)
		if (Low32(Current->key) == PVEntry->key)
		{
			PVEntry->date = date;
			return PVEntry;
		}
	return nullptr;
}

void move_to_string(int move, char string[])
{
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

void pick_pv()
{
	GEntry* Entry;
	GPVEntry* PVEntry;
	int i, depth, move;
	if (pvp >= Min(pv_length, 64))
	{
		PV[pvp] = 0;
		return;
	}
	move = 0;
	depth = -256;
	if (Entry = probe_hash())
		if (T(Entry->move) && Entry->low_depth > depth)
		{
			depth = Entry->low_depth;
			move = Entry->move;
		}
	if (PVEntry = probe_pv_hash())
		if (T(PVEntry->move) && PVEntry->depth > depth)
		{
			depth = PVEntry->depth;
			move = PVEntry->move;
		}
	evaluate();
	if (Current->att[Current->turn] & King(Current->turn ^ 1))
		PV[pvp] = 0;
	else if (move && (Current->turn ? is_legal<1>(move) : is_legal<0>(move)))
	{
		PV[pvp] = move;
		++pvp;
		ScopedMove_ raii(move);
		if (Current->ply >= 100)
			return;
		for (i = 4; i <= Current->ply; i += 2)
			if (Stack[sp - i] == Current->key)
			{
				PV[pvp] = 0;
				return;
			}
		pick_pv();
	}
	else
		PV[pvp] = 0;
}

template <bool me> bool draw_in_pv()
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

template <bool me> void do_move(int move)
{MOVING 
	// clang-format off
	constexpr array<uint8, 64> UpdateCastling = { 0xFF ^ CanCastle_OOO, 0xFF, 0xFF, 0xFF,
		0xFF ^ (CanCastle_OO | CanCastle_OOO), 0xFF, 0xFF, 0xFF ^ CanCastle_OO,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF ^ CanCastle_ooo, 0xFF, 0xFF, 0xFF,
		0xFF ^ (CanCastle_oo | CanCastle_ooo), 0xFF, 0xFF, 0xFF ^ CanCastle_oo };
	// clang-format on

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
	Next->castle_flags = Current->castle_flags & UpdateCastling[to] & UpdateCastling[from];
	Next->turn = opp;
	const auto& pKey = PieceKey[piece];
	Next->capture = capture;
	Next->pst = Current->pst + Pst(piece, to) - Pst(piece, from);
	Next->key = Current->key ^ pKey[to] ^ pKey[from] ^ CastleKey[Current->castle_flags] ^ CastleKey[Next->castle_flags];
	Next->pawn_key = Current->pawn_key ^ CastleKey[Current->castle_flags] ^ CastleKey[Next->castle_flags];

	if (T(capture))
	{
		Piece(capture) ^= mask_to;
		Piece(opp) ^= mask_to;
		Next->pst -= Pst(capture, to);
		Next->key ^= PieceKey[capture][to];
		if (capture == IPawn[opp])
			Next->pawn_key ^= PieceKey[IPawn[opp]][to];
		Next->material = Current->material - MatCode[capture];
		if (T(Current->material & FlagUnusualMaterial) && capture >= WhiteKnight)
		{
			if (popcnt(Piece(WhiteQueen)) <= 2 && popcnt(Piece(BlackQueen)) <= 2 && popcnt(Piece(WhiteLight)) <= 1 && popcnt(Piece(BlackLight)) <= 1 &&
				popcnt(Piece(WhiteKnight)) <= 2 && popcnt(Piece(BlackKnight)) <= 2 && popcnt(Piece(WhiteRook)) <= 2 && popcnt(Piece(BlackRook)) <= 2)
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
				Next->key ^= pKey[to] ^ PieceKey[piece][to];
				Next->pawn_key ^= pKey[from];
			}
			else
				Next->pawn_key ^= pKey[from] ^ pKey[to];

			PawnEntry = &PawnHash[Next->pawn_key & pawn_hash_mask];
			prefetch(PawnEntry);
		}
		else if (piece >= WhiteKing)
		{
			Next->pawn_key ^= pKey[from] ^ pKey[to];
			PawnEntry = &PawnHash[Next->pawn_key & pawn_hash_mask];
			prefetch(PawnEntry);
		}
		else if (capture < WhiteKnight)
		{
			PawnEntry = &PawnHash[Next->pawn_key & pawn_hash_mask];
			prefetch(PawnEntry);
		}
		if (F(Next->material & FlagUnusualMaterial))
			prefetch(Material + Next->material);
		if (Current->ep_square)
			Next->key ^= EPKey[FileOf(Current->ep_square)];
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
				Next->key ^= PieceKey[IPawn[opp]][to ^ 8];
				Next->pawn_key ^= PieceKey[IPawn[opp]][to ^ 8];
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
				Next->key ^= PieceKey[piece][to] ^ pKey[to];
				Next->pawn_key ^= pKey[to];
			}
			else if ((to ^ from) == 16)
			{
				if (PAtt[me][(to + from) >> 1] & Pawn(opp))
				{
					Next->ep_square = (to + from) >> 1;
					Next->key ^= EPKey[FileOf(Next->ep_square)];
				}
			}
			PawnEntry = &PawnHash[Next->pawn_key & pawn_hash_mask];
			prefetch(PawnEntry);
		}
		else
		{
			if (piece >= WhiteKing)
			{
				Next->pawn_key ^= pKey[to] ^ pKey[from];
				PawnEntry = &PawnHash[Next->pawn_key & pawn_hash_mask];
				prefetch(PawnEntry);
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
				Next->key ^= PieceKey[IRook[me]][rnew] ^ PieceKey[IRook[me]][rold];
			}
		}

		if (Current->ep_square)
			Next->key ^= EPKey[FileOf(Current->ep_square)];
	}	// end F(Capture)

	Next->key ^= TurnKey;
	Entry = Hash + (High32(Next->key) & hash_mask);
	prefetch(Entry);
	++sp;
	Stack[sp] = Next->key;
	Next->move = move;
	Next->gen_flags = 0;
	++Current;
	++nodes;
	assert(King(me) && King(opp));
BYE}

template <bool me> void undo_move(int move)
{MOVING 
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
BYE}

void do_null()
{
	GData* Next;
	GEntry* Entry;

	Next = Current + 1;
	Next->key = Current->key ^ TurnKey;
	Entry = Hash + (High32(Next->key) & hash_mask);
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
		Next->key ^= EPKey[FileOf(Current->ep_square)];
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
	++nodes;
}

void undo_null()
{
	--Current;
	--sp;
}

typedef struct
{
	uint64 patt[2], double_att[2];
	int king[2];
	packed_t score;
} GPawnEvalInfo;

constexpr array<array<uint64, 2>, 2> RFileBlockMask =
		{ array<uint64, 2>({0x0202000000000000, 0x8080000000000000}), array<uint64, 2>({0x0202, 0x8080}) };

template <bool me, class POP> INLINE void eval_pawns(GPawnEntry* PawnEntry, GPawnEvalInfo& PEI)
{
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
	uint64 mpawns = Pawn(me) & Forward[me][me ? Min(kr + 1, 7) : Max(kr - 1, 0)];
	for (int file = start, i = 0; i < 3; file += inc, ++i)
	{
		shelter += Shelter[i][OwnRank<me>(NBZ<me>(mpawns & File[file]))];
		if (Pawn(opp) & File[file])
		{
			int oppP = NB<me>(Pawn(opp) & File[file]);
			int rank = OwnRank<opp>(oppP);
			if (rank < 6)
			{
				if (rank >= 3)
					shelter += StormBlocked[rank - 3];
				if (uint64 u = (PIsolated[FileOf(oppP)] & Forward[opp][RankOf(oppP)] & Pawn(me)))
				{
					int meP = NB<opp>(u);
					uint64 att_sq = PAtt[me][meP] & PWay[opp][oppP];  // may be zero
					if (abs(kf - FileOf(meP)) <= 1
						&& (!(PEI.double_att[me] & att_sq) || (Current->patt[opp] & att_sq))
						&& F(PWay[opp][meP] & Pawn(me))
						&& (!(PawnAll() & PWay[opp][oppP] & Forward[me][RankOf(meP)])))
					{
						if (rank >= 3)
						{
							shelter += StormShelterAtt[rank - 3];
							if (HasBit(PEI.patt[opp], oppP + Push[opp]))
								shelter += StormConnected[rank - 3];
							if (!(PWay[opp][oppP] & PawnAll()))
								shelter += StormOpen[rank - 3];
						}
						if (rank <= 4 && !(PCone[me][oppP] & King(opp)))
							shelter += StormFree[rank - 1];
					}
				}
			}
		}
		else
		{
			if (i > 0 || T((File[file]|File[file+inc]) & (Rook(opp)|Queen(opp))) || T(RFileBlockMask[me][inc > 0] & ~(Pawn(opp) | King(opp))))
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
		uint64 way = PWay[me][sq];
		int next = PieceAt(sq + Push[me]);

		int isolated = !(Pawn(me) & PIsolated[file]);
		int doubled = T(Pawn(me) & (File[file] ^ b));
		int open = !(PawnAll() & way);

		if (isolated)
		{
			DecV(PEI.score, open ? Ca4(Isolated, IsolatedOpen) : Ca4(Isolated, IsolatedClosed));
			if (F(open) && next == IPawn[opp])
				DecV(PEI.score, Ca4(Isolated, IsolatedBlocked));
			if (doubled)
				DecV(PEI.score, open ? Ca4(Isolated, IsolatedDoubledOpen) : Ca4(Isolated, IsolatedDoubledClosed));
		}
		else
		{
			if (doubled)
				DecV(PEI.score, open ? Ca4(Doubled, DoubledOpen) : Ca4(Doubled, DoubledClosed));
			if (rrank >= 3 && (b & (File[2] | File[3] | File[4] | File[5])) && next != IPawn[opp] && (PIsolated[file] & Line[rank] & Pawn(me)))
				IncV(PEI.score, Ca4(PawnSpecial, PawnChainLinear) * (rrank - 3) + Ca4(PawnSpecial, PawnChain));
		}
		int backward = 0;
		if (!(PSupport[me][sq] & Pawn(me)))
		{
			if (isolated)
				backward = 1;
			else if (uint64 v = (PawnAll() | PEI.patt[opp]) & way)
				if (OwnRank<me>(NB<me>(PEI.patt[me] & way)) > OwnRank<me>(NB<me>(v)))
					backward = 1;
		}
		if (backward)
			DecV(PEI.score, open ? Ca4(Backward, BackwardOpen) : Ca4(Backward, BackwardClosed));
		else
		{
			if (open && (F(Pawn(opp) & PIsolated[file]) || pop(Pawn(me) & PIsolated[file]) >= pop(Pawn(opp) & PIsolated[file])))
				IncV(PEI.score, PasserCandidate[rrank]);  // IDEA: more precise pawn counting for the case of, say,
														  // white e5 candidate with black pawn on f5 or f4...
		}

		if (F(PEI.patt[me] & b) && next == IPawn[opp])  // unprotected and can't advance
		{
			DecV(PEI.score, Ca4(Unprotected, UpBlocked));
			if (backward)
			{
				if (rrank <= 2)	
				{	
					DecV(PEI.score, Ca4(Unprotected, PasserTarget));	
					if (rrank <= 1) DecV(PEI.score, Ca4(Unprotected, PasserTarget));	
				}	// Gull 3 was thinking of removing this term, because fitted weight is negative

				for (uint64 v = PAtt[me][sq] & Pawn(me); v; Cut(v)) if ((PSupport[me][lsb(v)] & Pawn(me)) == b)
				{
					DecV(PEI.score, Ca4(Unprotected, ChainRoot));
					break;
				}
			}
		}
		if (open && F(PIsolated[file] & Forward[me][rank] & Pawn(opp)))
		{
			PawnEntry->passer[me] |= (uint8)(1 << file);
			if (rrank <= 2)
				continue;
			IncV(PEI.score, PasserGeneral[rrank]);
			// IDEA: average the distance with the distance to the promotion square? or just use the latter?
			int dist_att = Dist(PEI.king[opp], sq + Push[me]);
			int dist_def = Dist(PEI.king[me], sq + Push[me]);
			int value = dist_att * PasserAtt[rrank] + LogDist[dist_att] * PasserAttLog[rrank]
				- dist_def * PasserDef[rrank] - LogDist[dist_def] * PasserDefLog[rrank];  // IDEA -- incorporate side-to-move in closer-king check?
			// IDEA -- scale down rook pawns?
			IncV(PEI.score, Pack2(0, value / 256));
			if (PEI.patt[me] & b)
				IncV(PEI.score, PasserProtected[rrank]);
			if (F(Pawn(opp) & West[file]) || F(Pawn(opp) & East[file]))
				IncV(PEI.score, PasserOutside[rrank]);
		}
	}
	if (!((kf * kr) % 7))
	{
		const uint64 kAdj = KAtt[PEI.king[me]];
		// look for opp pawns restricting K mobility
		if (PEI.patt[opp] & kAdj)
		{
			// find out which one it is
			for (uint64 u = Pawn(opp); T(u); u ^= b)
			{
				int sq = lsb(u);
				b = Bit(sq);
				if ((PAtt[opp][sq] & kAdj) && HasBit(Pawn(me), sq + Push[opp]))
					DecV(PEI.score, Ca4(PawnSpecial, PawnRestrictsK));
			}
		}
	}

	uint64 files = Pawn(me) | (Pawn(me) >> 32);
	files |= files >> 16;
	files = (files | (files >> 8)) & 0xFF;
	int file_span = (files ? (msb(files) - lsb(files)) : 0);
	IncV(PEI.score, Ca4(PawnSpecial, PawnFileSpan) * file_span);
	PawnEntry->draw[me] = (7 - file_span) * Max(5 - pop(files), 0);
}

template <class POP> INLINE void eval_pawn_structure(const GEvalInfo& EI, GPawnEntry* PawnEntry)
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

template <bool me, class POP> INLINE void eval_queens(GEvalInfo& EI)
{
	POP pop;
	for (uint64 b, u = Queen(me); T(u); u ^= b)
	{
		int sq = lsb(u);
		b = Bit(sq);
		uint64 att = QueenAttacks(sq, EI.occ);
		Current->att[me] |= att;

		uint64 control = att & EI.free[me];
		IncV(EI.score, MobQueen[0][pop(control)]);
		IncV(EI.score, MobQueen[1][pop(control & KingLocus[EI.king[opp]])]);
		if (control & Pawn(opp))
			IncV(EI.score, Ca4(Tactical, TacticalMajorPawn));
		if (control & Minor(opp))
			IncV(EI.score, Ca4(Tactical, TacticalMajorMinor));
		if (att & EI.area[me])
			IncV(EI.score, Ca4(KingDefence, KingDefQueen));
		uint64 att_opp = att & EI.area[opp];
		if (att_opp)
		{
			EI.king_att[me] += Single(att_opp) ? KingQAttack1 : KingQAttack;
			for (uint64 v = att_opp; T(v); Cut(v))
				if (FullLine[sq][lsb(v)] & att & ((Rook(me) & RMask[sq]) | (Bishop(me) & BMask[sq])))
					EI.king_att[me]++;
		}

		uint64 in_ray;
		if (T(QMask[sq] & King(opp)) && T(in_ray = Between[EI.king[opp]][sq] & EI.occ))
		{
			if (Single(in_ray))
			{
				Current->xray[me] |= in_ray;
				int square = lsb(in_ray), katt = F(att_opp);
				int piece = PieceAt(square);
				if ((piece & 1) == me)
				{
					if (piece == IPawn[me])
					{
						if (T(PieceAt(square + Push[me])))
							katt = 0;
						else
							IncV(EI.score, Ca4(Pin, SelfPawnPin));
					}
					else
						IncV(EI.score, Ca4(Pin, SelfPiecePin));
				}
				else if (piece != IPawn[opp] && F(((BMask[sq] & Bishop(opp)) | (RMask[sq] & Rook(opp)) | Queen(opp)) & in_ray))
				{
					IncV(EI.score, Ca4(Pin, WeakPin));
					katt *= F(Current->patt[opp] & in_ray);
				}
				else
					katt = 0;
				EI.king_att[me] += katt * KingAttack;
			}
			else
			{
				uint64 pinnable = (RMask[sq] & Bishop(opp)) | (BMask[sq] & Rook(opp)) | Knight(opp);
				if (F(in_ray & ~pinnable))
					IncV(EI.score, Ca4(KingRay, QKingRay));
			}
		}
	}
}

template <bool me, class POP> INLINE void eval_rooks(GEvalInfo& EI)
{
	POP pop;
	for (uint64 b, u = Rook(me); T(u); u ^= b)
	{
		int sq = lsb(u);
		b = Bit(sq);
		uint64 att = RookAttacks(sq, EI.occ);
		Current->att[me] |= att;
		Current->threat |= att & Queen(opp);
		uint64 control = att & EI.free[me];
		IncV(EI.score, MobRook[0][pop(control)]);
		IncV(EI.score, MobRook[1][pop(control & KingLocus[EI.king[opp]])]);
		if (control & Pawn(opp))
			IncV(EI.score, Ca4(Tactical, TacticalMajorPawn));
		if (control & Minor(opp))
			IncV(EI.score, Ca4(Tactical, TacticalMajorMinor));
		uint64 att_opp = att & EI.area[opp];
		if (att_opp)
		{
			EI.king_att[me] += Single(att_opp) ? KingRAttack1 : KingRAttack;
			for (uint64 v = att_opp; T(v); Cut(v))
				if (FullLine[sq][lsb(v)] & att & Major(me))
					EI.king_att[me]++;
		}

		uint64 in_ray;
		if (T(RMask[sq] & King(opp)) && T(in_ray = Between[EI.king[opp]][sq] & EI.occ))
		{
			if (Single(in_ray))
			{
				Current->xray[me] |= in_ray;
				int square = lsb(in_ray), katt = F(att_opp);
				int piece = PieceAt(square);
				if ((piece & 1) == me)
				{
					if (piece == IPawn[me])
					{
						if (!PieceAt(square + Push[me]))
							IncV(EI.score, Ca4(Pin, SelfPawnPin));
						else
							katt = 0;
					}
					else
						IncV(EI.score, Ca4(Pin, SelfPiecePin));
				}
				else if (piece != IPawn[opp])
				{
					if (piece < WhiteRook)
					{
						IncV(EI.score, Ca4(Pin, WeakPin));
						katt *= F(Current->patt[opp] & in_ray);
					}
					else if (piece >= WhiteQueen)
					{
						IncV(EI.score, Ca4(Pin, ThreatPin));
						katt = 0;
					}
				}
				else
					katt = 0;
				EI.king_att[me] += katt * KingAttack;
			}
			else if (F(in_ray & ~Minor(opp) & ~Queen(opp)))
				IncV(EI.score, Ca4(KingRay, RKingRay));
		}

		if (F(PWay[me][sq] & Pawn(me)))
		{
			IncV(EI.score, Ca4(RookSpecial, RookHof));
			packed_t hof_score = 0;
			if (F(PWay[me][sq] & Pawn(opp)))
			{
				IncV(EI.score, Ca4(RookSpecial, RookOf));
				if (att & OwnLine(me, 7))
					hof_score += Ca4(RookSpecial, RookOfOpen);
				else if (uint64 target = att & PWay[me][sq] & Minor(opp))
				{
					if (T(Current->patt[opp] & target))
						hof_score += Ca4(RookSpecial, RookOfMinorFixed);
					else
					{
						hof_score += Ca4(RookSpecial, RookOfMinorHanging);
						if (PWay[me][sq] & King(opp))
							hof_score += Ca4(RookSpecial, RookOfKingAtt);
					}
				}
			}
			else if (att & PWay[me][sq] & Pawn(opp))
			{
				uint64 square = lsb(att & PWay[me][sq] & Pawn(opp));
				if (F(PSupport[opp][square] & Pawn(opp)))
					hof_score += Ca4(RookSpecial, RookHofWeakPAtt);
			}
			IncV(EI.score, hof_score);
			if (PWay[opp][sq] & att & Major(me))
				IncV(EI.score, hof_score);
		}
		if ((b & OwnLine(me, 6)) && ((King(opp) | Pawn(opp)) & (OwnLine(me, 6) | OwnLine(me, 7))))
		{
			IncV(EI.score, Ca4(RookSpecial, Rook7th));
			if (King(opp) & OwnLine(me, 7))
				IncV(EI.score, Ca4(RookSpecial, Rook7thK8th));
			if (Major(me) & att & OwnLine(me, 6))
				IncV(EI.score, Ca4(RookSpecial, Rook7thDoubled));
		}
	}
}

template <bool me, class POP> INLINE void eval_bishops(GEvalInfo& EI)
{
	POP pop;
	uint64 b;
	for (uint64 u = Bishop(me); T(u); u ^= b)
	{
		int sq = lsb(u);
		b = Bit(sq);
		uint64 att = BishopAttacks(sq, EI.occ);
		Current->att[me] |= att;
		uint64 att_opp = att & EI.area[opp];
		if (att_opp)
			EI.king_att[me] += Single(att_opp) ? KingBAttack1 : KingBAttack;
		uint64 control = att & EI.free[me];
		IncV(EI.score, MobBishop[0][pop(control)]);
		IncV(EI.score, MobBishop[1][pop(control & KingLocus[EI.king[opp]])]);
		if (control & Pawn(opp))
			IncV(EI.score, Ca4(Tactical, TacticalMinorPawn));
		if (control & Knight(opp))
			IncV(EI.score, Ca4(Tactical, TacticalMinorMinor));
		if (att & EI.area[me])
			IncV(EI.score, Ca4(KingDefence, KingDefBishop));
		Current->threat |= att & Major(opp);
		const uint64& myArea = (b & LightArea) ? LightArea : DarkArea;
		uint64 v = BishopForward[me][sq] & Pawn(me) & myArea;
		v |= (v & (File[2] | File[3] | File[4] | File[5] | BMask[sq])) >> 8;
		DecV(EI.score, Ca4(BishopSpecial, BishopPawnBlock) * pop(v));
		if (T(b & Outpost[me])
			&& F(Knight(opp))
			&& T(Current->patt[me] & b)
			&& F(Pawn(opp) & PIsolated[FileOf(sq)] & Forward[me][RankOf(sq)])
			&& F(Piece((T(b & LightArea) ? WhiteLight : WhiteDark) | opp)))
			IncV(EI.score, Ca4(BishopSpecial, BishopOutpostNoMinor));

		uint64 in_ray;
		if (BMask[sq] & King(opp) && T(in_ray = Between[EI.king[opp]][sq] & EI.occ))
		{
			if (Single(in_ray))  // pin or discovery threat
			{
				Current->xray[me] |= in_ray;
				int square = lsb(in_ray), katt = F(att_opp);
				int piece = PieceAt(square);
				if ((piece & 1) == me)
				{
					if (piece == IPawn[me])
					{
						if (T(PieceAt(square + Push[me])))
							katt = 0;
						else
							IncV(EI.score, Ca4(Pin, SelfPawnPin));
					}
					else if ((piece & 1) == me)
						IncV(EI.score, Ca4(Pin, SelfPiecePin));
				}
				else if (piece != IPawn[opp])
				{
					if (piece == IKnight[opp])
					{
						IncV(EI.score, Ca4(Pin, StrongPin));
						katt *= F(Current->patt[opp] & in_ray);
					}
					else if (piece >= WhiteRook)
					{
						IncV(EI.score, Ca4(Pin, ThreatPin));
						katt = 0;
					}
				}
				else
					katt = 0;
				EI.king_att[me] += katt * KingAttack;
			}
			else if (F(in_ray & ~Knight(opp) & ~Major(opp)))
				IncV(EI.score, Ca4(KingRay, BKingRay));
		}
	}
}

template <bool me, class POP> INLINE void eval_knights(GEvalInfo& EI)
{
	POP pop;
	for (uint64 b, u = Knight(me); T(u); u ^= b)
	{
		int sq = lsb(u);
		b = Bit(sq);
		uint64 att = NAtt[sq];
		Current->att[me] |= att;
		if (uint64 a = att & EI.area[opp])
			EI.king_att[me] += Single(a) ? KingNAttack1 : KingNAttack;
		Current->threat |= att & Major(opp);
		uint64 control = att & EI.free[me];
		IncV(EI.score, MobKnight[0][pop(control)]);
		IncV(EI.score, MobKnight[1][pop(control & KingLocus[EI.king[opp]])]);
		if (control & Pawn(opp))
			IncV(EI.score, Ca4(Tactical, TacticalMinorPawn));
		if (control & Bishop(opp))
			IncV(EI.score, Ca4(Tactical, TacticalMinorMinor));
		if (att & EI.area[me])
			IncV(EI.score, Ca4(KingDefence, KingDefKnight));
		if ((b & Outpost[me]) && !(Pawn(opp) & PIsolated[FileOf(sq)] & Forward[me][RankOf(sq)]))
		{
			IncV(EI.score, Ca4(KnightSpecial, KnightOutpost));
			if (Current->patt[me] & b)
			{
				IncV(EI.score, Ca4(KnightSpecial, KnightOutpostProtected));
				if (att & EI.free[me] & Pawn(opp))
					IncV(EI.score, Ca4(KnightSpecial, KnightOutpostPawnAtt));
				if (F(Knight(opp) & Piece((T(b & LightArea) ? WhiteLight : WhiteDark) | opp)))
					IncV(EI.score, Ca4(KnightSpecial, KnightOutpostNoMinor));
			}
		}
	}
}

template <bool me, class POP> INLINE void eval_king(GEvalInfo& EI)
{
	POP pop;
	uint16 cnt = Min<uint16>(10, UUnpack1(EI.king_att[me]));
	uint16 score = UUnpack2(EI.king_att[me]);
	if (cnt >= 2 && T(Queen(me)))
	{
		score += (EI.PawnEntry->shelter[opp] * KingShelterQuad) / 64;
		if (uint64 u = Current->att[me] & EI.area[opp] & (~Current->att[opp]))
			score += pop(u) * KingAttackSquare;
		if (!(KAtt[EI.king[opp]] & (~(Piece(opp) | Current->att[me]))))
			score += KingNoMoves;
	}
	int adjusted = ((score * KingAttackScale[cnt]) >> 3) + EI.PawnEntry->shelter[opp];
	int kf = FileOf(EI.king[opp]);
	if (kf > 3)
		kf = 7 - kf;
	adjusted = (adjusted * KingCenterScale[kf]) / 64;
	if (!Queen(me))
		adjusted = (adjusted * KingSafetyNoQueen) / 16;
	// add a correction for defense-in-depth
	if (adjusted > 1)
	{
		uint64 holes = KingLocus[EI.king[opp]] & ~Current->att[opp];
		int nHoles = pop(holes);
		int nIncursions = pop(holes & Current->att[me]);
		uint64 personnel = NonPawnKing(opp);
		uint64 guards = KingLocus[EI.king[opp]] & personnel;
		uint64 awol = personnel ^ guards;
		int nGuards = pop(guards) + pop(guards & Queen(opp));
		int nAwol = pop(awol) + pop(awol & Queen(opp));
		adjusted += (adjusted * (max(0, nAwol - nGuards) + max(0, 3 * nIncursions + nHoles - 10))) / 32;
	}

	static constexpr array<int, 4> PHASE = { 13, 10, 1, 0 };
	int op = (PHASE[0] * adjusted) / 16;
	int md = (PHASE[1] * adjusted) / 16;
	int eg = (PHASE[2] * adjusted) / 16;
	int cl = (PHASE[3] * adjusted) / 16;
	IncV(EI.score, Pack4(op, md, eg, cl));
}

template <bool me, class POP> INLINE void eval_passer(GEvalInfo& EI)
{
	bool sr_me = Rook(me) && !Minor(me) && !Queen(me) && Single(Rook(me));
	bool sr_opp = Rook(opp) && !Minor(opp) && !Queen(opp) && Single(Rook(opp));
	int ksq = lsb(King(me));

	for (uint8 u = EI.PawnEntry->passer[me]; T(u); u &= (u - 1))
	{
		int file = lsb(u);
		int sq = NB<opp>(File[file] & Pawn(me));  // most advanced in this file
		int rank = OwnRank<me>(sq);
		Current->passer |= Bit(sq);
		if (rank <= 2)
			continue;
		if (!PieceAt(sq + Push[me]))
			IncV(EI.score, PasserBlocked[rank]);
		uint64 way = PWay[me][sq];
		int connected = 0, supported = 0, hooked = 0, unsupported = 0, free = 0;
		if (!(way & Piece(opp)))
		{
			IncV(EI.score, PasserClear[rank]);
			if (PWay[opp][sq] & Major(me))
			{
				int square = NB<opp>(PWay[opp][sq] & Major(me));
				if (F(Between[sq][square] & EI.occ))
					supported = 1;
			}
			if (PWay[opp][sq] & Major(opp))
			{
				int square = NB<opp>(PWay[opp][sq] & Major(opp));
				if (F(Between[sq][square] & EI.occ))
					hooked = 1;
			}
			for (uint64 v = PAtt[me][sq - Push[me]] & Pawn(me); T(v); Cut(v))
			{
				int square = lsb(v);
				if (F(Pawn(opp) & (VLine[square] | PIsolated[FileOf(square)]) & Forward[me][RankOf(square)]))
					++connected;
			}
			if (connected)
				IncV(EI.score, PasserConnected[rank]);
			if (!hooked && !(Current->att[opp] & way))
			{
				IncV(EI.score, PasserFree[rank]);
				free = 1;
			}
			else
			{
				uint64 attacked = Current->att[opp] | (hooked ? way : 0);
				if (supported || (!hooked && connected) || (!(Major(me) & way) && !(attacked & (~Current->att[me]))))
					IncV(EI.score, PasserSupported[rank]);
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
		return threat ? Ca4(Tactical, TacticalThreat) : 0;
	// according to Gull, second threat is extra DoubleThreat, third and after are simple Threat again
	return Ca4(Tactical, TacticalDoubleThreat) + pop(threat) * Ca4(Tactical, TacticalThreat);
}

template <bool me, class POP> INLINE void eval_pieces(GEvalInfo& EI)
{
	POP pop;
	Current->threat |= Current->att[opp] & (~Current->att[me]) & Piece(me);
	DecV(EI.score, eval_threat<POP>(Current->threat & Piece(me)));
	if (T(Queen(me)) && F(Queen(opp)))
		IncV(EI.score, pop((Piece(opp) ^ King(opp)) & ~Current->att[opp]) * Ca4(Tactical, TacticalUnguardedQ));
}


template <class POP> void eval_unusual_material(GEvalInfo& EI)
{
	POP pop;
	int wp, bp, wminor, bminor, wr, br, wq, bq;
	wp = pop(Pawn(White));
	bp = pop(Pawn(Black));
	wminor = pop(Minor(White));
	bminor = pop(Minor(Black));
	wr = pop(Rook(White));
	br = pop(Rook(Black));
	wq = pop(Queen(White));
	bq = pop(Queen(Black));
	int phase = Min(24, (wminor + bminor) + 2 * (wr + br) + 4 * (wq + bq));
	sint16 temp = SeeValue[WhitePawn] * (wp - bp) + SeeValue[WhiteKnight] * (wminor - bminor) + SeeValue[WhiteRook] * (wr - br) +
		SeeValue[WhiteQueen] * (wq - bq);
	packed_t mat_score = Pack2(temp, temp);
	Current->score = (((Opening(mat_score + EI.score) * phase) + (Endgame(mat_score + EI.score) * (24 - phase))) / 24);
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

template<bool me, class POP> void eval_sequential(GEvalInfo& EI)
{
	POP pop;
	Current->xray[me] = 0;
	EI.king_att[me] = T(Current->patt[me] & EI.area[opp]) ? KingPAttack : 0;
	DecV(EI.score, pop(Shift<opp>(EI.occ) & Pawn(me)) * PawnSpecial[PawnBlocked]);
	EI.free[me] = Queen(opp) | King(opp) | (~(Current->patt[opp] | Pawn(me) | King(me)));
	eval_queens<me, POP>(EI);
	EI.free[me] |= Rook(opp);
	eval_rooks<me, POP>(EI);
	EI.free[me] |= Minor(opp);
	eval_bishops<me, POP>(EI);
	eval_knights<me, POP>(EI);
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
		EI.material = &Material[Current->material];
	else
		EI.material = 0;

#define me White
	Current->xray[me] = 0;
	EI.free[me] = Queen(opp) | King(opp) | (~(Current->patt[opp] | Pawn(me) | King(me)));
	DecV(EI.score, pop(Shift<opp>(EI.occ) & Pawn(me)) * Ca4(PawnSpecial, PawnBlocked));
	EI.king_att[me] = T(Current->patt[me] & EI.area[opp]) ? KingAttack : 0;
	eval_queens<me, POP>(EI);
	EI.free[me] |= Rook(opp);
	eval_rooks<me, POP>(EI);
	EI.free[me] |= Minor(opp);
	eval_bishops<me, POP>(EI);
	eval_knights<me, POP>(EI);
#undef me
#define me Black
	Current->xray[me] = 0;
	EI.free[me] = Queen(opp) | King(opp) | (~(Current->patt[opp] | Pawn(me) | King(me)));
	DecV(EI.score, pop(Shift<opp>(EI.occ) & Pawn(me)) * Ca4(PawnSpecial, PawnBlocked));
	EI.king_att[me] = T(Current->patt[me] & EI.area[opp]) ? KingAttack : 0;
	eval_queens<me, POP>(EI);
	EI.free[me] |= Rook(opp);
	eval_rooks<me, POP>(EI);
	EI.free[me] |= Minor(opp);
	eval_bishops<me, POP>(EI);
	eval_knights<me, POP>(EI);
#undef me

	EI.PawnEntry = &PawnHash[Current->pawn_key & pawn_hash_mask];
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
#ifdef TUNER
		if (EI.material->generation != generation)
			calc_material(Current->material);
#endif
		const uint8& phase = EI.material->phase;
#ifdef TWO_PHASE
		int op = Opening(EI.score), eg = Endgame(EI.score);
		Current->score = EI.material->score;
		Current->score += (op * phase + eg * (MAX_PHASE - phase)) / MAX_PHASE;
#else
		int op = Opening(EI.score), eg = Endgame(EI.score), md = Middle(EI.score), cl = Closed(EI.score);
		Current->score = EI.material->score;
		if (EI.material->phase > MIDDLE_PHASE)
			Current->score += (op * (phase - MIDDLE_PHASE) + md * (MAX_PHASE - phase)) / PHASE_M2M;
		else
			Current->score += (md * phase + eg * (MIDDLE_PHASE - phase)) / MIDDLE_PHASE;
#ifndef THREE_PHASE
		int clx = closure<POP>();
		Current->score += static_cast<sint16>((clx * (Min<int>(phase, MIDDLE_PHASE) * cl + MIDDLE_PHASE * EI.material->closed)) / 8192);	// closure is capped at 128, phase at 64
#endif
#endif
		// apply contempt before drawishness
		if (Contempt > 0)
		{
			int maxContempt = (phase * Contempt * CP_EVAL) / 64;
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
			EI.mul = EI.material->mul[White];
			if (EI.material->eval[White] && !eval_stalemate<White>(EI))
				EI.material->eval[White](EI, pop.Imp());
			Current->score -= (Min<int>(Current->score, drawCap) * EI.PawnEntry->draw[White]) / 64;
		}
		else if (Current->score < 0)
		{
			EI.mul = EI.material->mul[Black];
			if (EI.material->eval[Black] && !eval_stalemate<Black>(EI))
				EI.material->eval[Black](EI, pop.Imp());
			Current->score += (Min<int>(-Current->score, drawCap) * EI.PawnEntry->draw[Black]) / 64;
		}
		else
			EI.mul = Min(EI.material->mul[White], EI.material->mul[Black]);
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

INLINE void evaluate()
{HI
	HardwarePopCnt ? evaluation<pop1_>() : evaluation<pop0_>();
BYE}

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
		if (Between[from][to] & occ)
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
		if (u & PMove[me][from])
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
		else if (u & PAtt[me][from])
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

template <bool me> bool is_check(int move)
{  // doesn't detect castling and ep checks
	uint64 king;
	int from, to, piece, king_sq;

	from = From(move);
	to = To(move);
	king = King(opp);
	king_sq = lsb(king);
	piece = PieceAt(from);
	if (HasBit(Current->xray[me], from) && !HasBit(FullLine[king_sq][from], to))
		return true;
	if (piece < WhiteKnight)
	{
		if (PAtt[me][to] & king)
			return true;
		if (HasBit(OwnLine(me, 7), to) && T(king & OwnLine(me, 7)) && F(Between[to][king_sq] & PieceAll()))
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
			if (F(Between[king_sq][to] & PieceAll()))
				return true;
	}
	else if (piece < WhiteQueen)
	{
		if (RMask[to] & king)
			if (F(Between[king_sq][to] & PieceAll()))
				return true;
	}
	else if (piece < WhiteKing)
	{
		if (QMask[to] & king)
			if (F(Between[king_sq][to] & PieceAll()))
				return true;
	}
	return false;
}

void hash_high(int value, int depth)
{
	int i;
	GEntry* best, *Entry;

	// search for an old entry to overwrite
	int minMerit = 0x70000000;
	for (i = 0, best = Entry = Hash + (High32(Current->key) & hash_mask); i < 4; ++i, ++Entry)
	{
		if (Entry->key == Low32(Current->key))
		{
			Entry->date = date;
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
		int merit = 8 * static_cast<int>(Entry->date)  + static_cast<int>(Max(Entry->high_depth, Entry->low_depth));
		if (merit < minMerit)
		{
			minMerit = merit;
			best = Entry;
		}
	}
	best->date = date;
	best->key = Low32(Current->key);
	best->high = value;
	best->high_depth = depth;
	best->low = 0;
	best->low_depth = 0;
	best->move = 0;
	best->flags = 0;
	return;
}

// POSTPONED -- can hash_low return something better than its input?
int hash_low(int move, int value, int depth)
{
	int i;
	GEntry* best, *Entry;

	int min_score = 0x70000000;
	move &= 0xFFFF;
	for (i = 0, best = Entry = Hash + (High32(Current->key) & hash_mask); i < 4; ++i, ++Entry)
	{
		if (Entry->key == Low32(Current->key))
		{
			Entry->date = date;
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
		int score = (static_cast<int>(Entry->date) << 3) + static_cast<int>(Max(Entry->high_depth, Entry->low_depth));
		if (score < min_score)
		{
			min_score = score;
			best = Entry;
		}
	}
	best->date = date;
	best->key = Low32(Current->key);
	best->high = 0;
	best->high_depth = 0;
	best->low = value;
	best->low_depth = depth;
	best->move = move;
	best->flags = 0;
	return value;
}

void hash_exact(int move, int value, int depth, int exclusion, int ex_depth, int knodes)
{
	int i, score, min_score;
	GPVEntry* best;
	GPVEntry* PVEntry;

	min_score = 0x70000000;
	for (i = 0, best = PVEntry = PVHash + (High32(Current->key) & pv_hash_mask); i < pv_cluster_size; ++i, ++PVEntry)
	{
		if (PVEntry->key == Low32(Current->key))
		{
			PVEntry->date = date;
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
		score = (static_cast<int>(PVEntry->date) << 3) + static_cast<int>(PVEntry->depth);
		if (score < min_score)
		{
			min_score = score;
			best = PVEntry;
		}
	}
	best->key = Low32(Current->key);
	best->date = date;
	best->value = value;
	best->depth = depth;
	best->move = move;
	best->exclusion = exclusion;
	best->ex_depth = ex_depth;
	best->knodes = knodes;
	best->ply = Current->ply;
}

template<bool me, bool pv> INLINE int extension(int move, int depth)
{
	int from = From(move);
	if (HasBit(Current->passer, from))
	{
		int rank = OwnRank(me, from);
		if (rank >= 5 && depth < 16)
			return pv ? 2 : 1;
	}
	if (HasBit(Piece(opp), To(move)) && (pv || Current->score > 120 + 30 * depth))
		return 1;
	return 0;
}

template<bool me, bool pv> INLINE int check_extension(int move, int depth)
{
	return pv ? 2 : T(depth < 12) + T(depth < 24);
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
	*move += (mp + depth) % (Wobble + 1);	// (minimal) bonus for right parity
}

INLINE bool is_killer(uint16 move)
{
	for (int ik = 1; ik <= N_KILLER; ++ik)
		if (move == Current->killer[ik])
			return true;
	return false;
}

template <bool me> void gen_next_moves(int depth)
{
	int* p, *q, *r;
	Current->gen_flags &= ~FlagSort;
	switch (Current->stage)
	{
	case s_hash_move:
	case r_hash_move:
	case e_hash_move:
		Current->moves[0] = Current->killer[0];	// POSTPONED -- Current->killer[0] is always 0 except at root, so this is a waste
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
		for (auto q = Current->start; *q; ++q)
			apply_wobble(&*q, depth % (Wobble + 1));
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

template <bool me, bool root> int get_move(int depth)
{
	if (root)
	{
		int move = (*Current->current) & 0xFFFF;
		Current->current++;
		return move;
	}
	for ( ; ; )
	{
		if (F(*Current->current))
		{
			Current->stage++;
			if ((1 << Current->stage) & StageNone)
				return 0;
			gen_next_moves<me>(depth);
			continue;
		}
		int move = Current->gen_flags & FlagSort ? pick_move() : (*Current->current++) & 0xFFFF;

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

template<bool me> bool see(int move, int margin, const array<uint16, 16>& mat_value)
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
	att = PAtt[me][to] & Pawn(opp);
	if (T(att) && delta + margin > mat_value[WhitePawn])
		return 0;
	clear = ~Bit(from);
	def = PAtt[opp][to] & Pawn(me) & clear;
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
			for (new_att = FullLine[to][sq] & b_slider_att & occ & (~att); T(new_att); Cut(new_att))
			{
				pos = lsb(new_att);
				if (F(Between[to][pos] & occ))
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
			for (new_att = FullLine[to][sq] & b_slider_att & occ & (~att); T(new_att); Cut(new_att))
			{
				pos = lsb(new_att);
				if (F(Between[to][pos] & occ))
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
			for (new_att = FullLine[to][sq] & r_slider_att & occ & (~att); T(new_att); Cut(new_att))
			{
				pos = lsb(new_att);
				if (F(Between[to][pos] & occ))
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
			for (new_att = FullLine[to][sq] & (r_slider_att | b_slider_att) & occ & (~att); T(new_att); Cut(new_att))
			{
				pos = lsb(new_att);
				if (F(Between[to][pos] & occ))
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
			for (new_att = FullLine[to][sq] & b_slider_def & occ & (~att); T(new_att); Cut(new_att))
			{
				pos = lsb(new_att);
				if (F(Between[to][pos] & occ))
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
			for (new_att = FullLine[to][sq] & b_slider_def & occ & (~att); T(new_att); Cut(new_att))
			{
				pos = lsb(new_att);
				if (F(Between[to][pos] & occ))
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
			for (new_att = FullLine[to][sq] & r_slider_def & occ & (~att); T(new_att); Cut(new_att))
			{
				pos = lsb(new_att);
				if (F(Between[to][pos] & occ))
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
			for (new_att = FullLine[to][sq] & (r_slider_def | b_slider_def) & occ & (~att); T(new_att); Cut(new_att))
			{
				pos = lsb(new_att);
				if (F(Between[to][pos] & occ))
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

template <bool me> void gen_root_moves()
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

template <bool me> int* gen_captures(int* list)
{
	uint64 u, v;

	if (Current->ep_square)
		for (v = PAtt[opp][Current->ep_square] & Pawn(me); T(v); Cut(v)) 
			list = AddMove(list, lsb(v), Current->ep_square, FlagEP, MvvLva[IPawn[me]][IPawn[opp]]);
	for (u = Pawn(me) & OwnLine(me, 6); T(u); Cut(u))
		if (F(PieceAt(lsb(u) + Push[me])))
		{
			int from = lsb(u), to = from + Push[me];
			list = AddMove(list, from, to, FlagPQueen, MvvLvaPromotion);
			if (T(NAtt[to] & King(opp)) || forkable<me>(to))	// Roc v Hannibal, 64th amateur series A round 2, proved the need for this second test
				list = AddMove(list, from, to, FlagPKnight, MvvLvaPromotionKnight);
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

INLINE pair<uint64, uint64> pawn_joins(bool me, const uint64& pawns)  // returns (pawn attacks, reversed attacks)
{
	pair<uint64, uint64> retval(0, 0);
	for (uint64 u = pawns; T(u); Cut(u))
	{
		int loc = lsb(u);
		retval.first |= PAtt[me][loc];
		retval.second |= PAtt[opp][loc];
	}
	return retval;
}

template<class T_> T_* NullTerminate(T_* list)
{
	*list = 0;
	return list;
}

template <bool me> int* gen_evasions(int* list)
{
	int from;
	uint64 b, u;
	//	pair<uint64, uint64> pJoins = pawn_joins(me, Pawn(me));

	int king = lsb(King(me));
	uint64 att = (NAtt[king] & Knight(opp)) | (PAtt[me][king] & Pawn(opp));
	for (u = (BMask[king] & (Bishop(opp) | Queen(opp))) | (RMask[king] & (Rook(opp) | Queen(opp))); T(u); u ^= b)
	{
		b = Bit(lsb(u));
		if (F(Between[king][lsb(u)] & PieceAll()))
			att |= b;
	}
	int att_sq = lsb(att);  // generally the only attacker
	uint64 esc = KAtt[king] & (~(Piece(me) | Current->att[opp])) & Current->mask;
	if (PieceAt(att_sq) >= WhiteLight)
		esc &= ~FullLine[king][att_sq];
	else if (PieceAt(att_sq) >= WhiteKnight)
		esc &= ~NAtt[att_sq];

	Cut(att);
	if (att)
	{  // second attacker (double check)
		att_sq = lsb(att);
		if (PieceAt(att_sq) >= WhiteLight)
			esc &= ~FullLine[king][att_sq];
		else if (PieceAt(att_sq) >= WhiteKnight)
			esc &= ~NAtt[att_sq];

		for (; T(esc); Cut(esc)) 
			list = AddCaptureP(list, IKing[me], king, lsb(esc), 0);
		return NullTerminate(list);
	}

	// not double check
	if (T(Current->ep_square) && Current->ep_square == att_sq + Push[me] && HasBit(Current->mask, att_sq))
	{
		for (u = PAtt[opp][Current->ep_square] & Pawn(me); T(u); Cut(u)) 
			list = AddMove(list, lsb(u), att_sq + Push[me], FlagEP, MvvLva[IPawn[me]][IPawn[opp]]);
	}
	for (u = PAtt[opp][att_sq] & Pawn(me); T(u); Cut(u))
	{
		from = lsb(u);
		if (HasBit(OwnLine(me, 7), att_sq))
			list = AddMove(list, from, att_sq, FlagPQueen, MvvLvaPromotionCap(PieceAt(att_sq)));
		else if (HasBit(Current->mask, att_sq))
			list = AddCaptureP(list, IPawn[me], from, att_sq, 0);
	}
	for (; T(esc); Cut(esc)) 
		list = AddCaptureP(list, IKing[me], king, lsb(esc), 0);
	// now check interpositions
	uint64 inter = Between[king][att_sq];
	for (u = Shift<opp>(inter) & Pawn(me); T(u); Cut(u))
	{
		from = lsb(u);
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
				auto ik = find(Current->killer.begin() + 1, Current->killer.end(), move) - Current->killer.begin();
				*list |= (0xFF >> max(0, ik - 2)) << 16;
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
	return Shift<me>(Current->passer | Current->threat) | ShiftW<opp>(Current->threat) | ShiftE<opp>(Current->threat);
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
			int flag = BMask[to] & (PAtt[opp][to] & Pawn(me) ? Major(opp) : Rook(opp)) ? FlagCastling : 0;
			list = AddHistoryP(list, which, from, to, flag);
		}
	}

	for (u = Rook(me); T(u); Cut(u))
	{
		int from = lsb(u);
		for (v = free & RookAttacks(from, occ); T(v); Cut(v))
		{
			int to = lsb(v);
			int flag = (PAtt[opp][to] & Pawn(me)) && (RMask[to] & Queen(opp)) ? FlagCastling : 0;
			list = AddHistoryP(list, IRook[me], from, to, flag);
		}
	}
	for (u = Queen(me); T(u); Cut(u))
	{
		//uint64 qTarget = NAtt[lsb(King(opp))];	// try to get next to this
		int from = lsb(u);
		for (v = free & QueenAttacks(from, occ); T(v); Cut(v))
		{
			int to = lsb(v);
			list = AddHistoryP(list, IQueen[me], from, to, 0);	// KAtt[to] & qTarget ? FlagCastling : 0);
		}
	}
	for (v = KAtt[lsb(King(me))] & free & (~Current->att[opp]); T(v); Cut(v)) 
		list = AddHistoryP(list, IKing[me], lsb(King(me)), lsb(v), 0);

	return NullTerminate(list);
}

template <bool me> int* gen_checks(int* list)
{
	int king, from;
	uint64 u, v, target, b_target, r_target, clear;

	clear = ~(Piece(me) | Current->mask);
	king = lsb(King(opp));
	// discovered checks
	for (u = Current->xray[me] & Piece(me); T(u); Cut(u))
	{
		from = lsb(u);
		target = clear & (~FullLine[king][from]);
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

				for (v = PAtt[me][from] & target & Piece(opp); T(v); Cut(v))
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
		from = lsb(u);
		for (v = PAtt[me][from] & PAtt[opp][king] & clear & Piece(opp); T(v); Cut(v))
			list = AddCaptureP(list, IPawn[me], from, lsb(v), 0);
		if (F(PieceAt(from + Push[me])) && HasBit(PAtt[opp][king], from + Push[me]))
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
		//uint64 contact = KAtt[king];
		int from = lsb(u);
		for (v = QueenAttacks(from, PieceAll()) & (b_target | r_target); T(v); Cut(v))
		{
			int to = lsb(v);
			//if (HasBit(contact, to))
			//	list = AddCaptureP(list, IQueen[me], from, to, 0, T(Boundary & King(opp)) || OwnRank<me>(to) == 7 ? IPawn[opp] : IRook[opp]);
			//else
			list = AddCaptureP(list, IQueen[me], from, to, 0);
		}
	}

	if (OwnRank<me>(king) == 4)
	{	  // check for double-push checks	
		for (u = Pawn(me) & OwnLine(me, 1) & nonDiscover & PAtt[opp][king - 2 * Push[me]]; T(u); Cut(u))
		{
			from = lsb(u);
			int to = from + 2 * Push[me];
			if (F(PieceAt(from + Push[me])) && F(PieceAt(to)))
				list = AddMove(list, from, to, 0, 0);
		}
	}
	return NullTerminate(list);
}

template <bool me> int* gen_delta_moves(int margin, int* list)
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

template<bool me> int singular_extension(int ext, int prev_ext, int margin_one, int margin_two, int depth, int killer)
{
	int value = -MateValue;
	int singular = 0;
	if (ext < (prev_ext ? 1 : 2))
	{
		value = (IsCheck(me) ? scout<me, 1, 1> : scout<me, 1, 0>)(margin_one, depth, killer);
		if (value < margin_one)
			singular = 1;
	}
	if (value < margin_one && ext < (prev_ext ? (prev_ext >= 2 ? 1 : 2) : 3))
	{
		value = (IsCheck(me) ? scout<me, 1, 1> : scout<me, 1, 0>)(margin_two, depth, killer);
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

template <bool me, bool pv> int q_search(int alpha, int beta, int depth, int flags)
{
	int i, value, score, move, hash_move, hash_depth, cnt;
	GEntry* Entry;
	auto finish = [&](int score)
	{
		if (depth >= -2 && (depth >= 0 || Current->score + FutilityThreshold >= alpha))
			hash_high(score, 1);
		return score;
	};

	if (flags & FlagHaltCheck)
		halt_check;
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
#ifdef TUNER
		if (nodes > 64 * 1024 * 1024)
			longjmp(Jump, 1);
#endif
	}
#endif
	if (flags & FlagCallEvaluation)
		evaluate();
	if (IsCheck(me))
		return q_evasion<me, pv>(alpha, beta, depth, FlagHashCheck);

	int initiative = InitiativeConst;
	if (F(NonPawnKing(me) | (Current->passer & Pawn(me))))
		initiative = 0;
	else if (F(Current->material & FlagUnusualMaterial) && Current->material < TotalMat)
		initiative += (InitiativePhase * Material[Current->material].phase) / MAX_PHASE;
	score = Current->score + initiative;
	if (score > alpha)
	{
		if (score >= beta)
			return score;
		alpha = score;
	}

	hash_move = hash_depth = 0;
	if (flags & FlagHashCheck)
	{
		for (i = 0, Entry = Hash + (High32(Current->key) & hash_mask); i < 4; ++Entry, ++i)
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

	cnt = 0;
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
					++cnt;
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
				++cnt;
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

	if (T(cnt) || Current->score + 30 * CP_SEARCH < alpha || T(Current->threat & Piece(me)) || T(Current->xray[opp] & NonPawn(opp)) ||
		T(Pawn(opp) & OwnLine(me, 1) & Shift<me>(~PieceAll())))
		return finish(score);
	int margin = alpha - Current->score + 6 * CP_SEARCH;
	gen_delta_moves<me>(margin, Current->moves);
	Current->current = Current->moves;
	while (move = pick_move())
	{
		if (move != hash_move && !IsIllegal(me, move) && !IsRepetition(alpha + 1, move) && see<me>(move, -SeeThreshold, SeeValue))
		{
			++cnt;
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
							for (int jk = N_KILLER; jk > 1; --jk) Current->killer[jk] = Current->killer[jk - 1];
							Current->killer[1] = move;
						}
						return hash_low(move, Max(score, beta), 1);
					}
					alpha = score;
				}
			}
			if (cnt >= 3)
				break;
		}
	}
	return finish(score);
}

template <bool me, bool pv> int q_evasion(int alpha, int beta, int depth, int flags)
{
	int i, value, pext, score, move, cnt, hash_move, hash_depth;
	int* p;
	GEntry* Entry;

	score = static_cast<int>(Current - Data) - MateValue;
	if (flags & FlagHaltCheck)
		halt_check;

	hash_move = hash_depth = 0;
	if (flags & FlagHashCheck)
	{
		for (i = 0, Entry = Hash + (High32(Current->key) & hash_mask); i < 4; ++Entry, ++i)
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

void send_position(GPos* Pos)
{
	Pos->Position->key = Current->key;
	Pos->Position->pawn_key = Current->pawn_key;
	Pos->Position->move = Current->move;
	Pos->Position->capture = Current->capture;
	Pos->Position->turn = Current->turn;
	Pos->Position->castle_flags = Current->castle_flags;
	Pos->Position->ply = Current->ply;
	Pos->Position->ep_square = Current->ep_square;
	Pos->Position->piece = Current->piece;
	Pos->Position->pst = Current->pst;
	Pos->Position->material = Current->material;
	for (int i = 0; i < 64; ++i) Pos->Position->square[i] = Board->square[i];
	Pos->date = date;
	Pos->sp = sp;
	for (int i = 0; i <= Current->ply; ++i) Pos->stack[i] = Stack[sp - i];
	for (int i = 0; i < Min(16, 126 - (int)(Current - Data)); ++i)
		for (int ik = 0; ik < N_KILLER; ++ik) Pos->killer[i][ik] = (Current + i + 1)->killer[ik + 1];
	for (int i = Min(16, 126 - (int)(Current - Data)); i < 16; ++i)
		for (int ik = 0; ik < N_KILLER; ++ik) Pos->killer[i][ik] = 0;
}

void retrieve_board(GPos* Pos)
{
	for (int i = 0; i < 16; ++i) Board->bb[i] = 0;
	for (int i = 0; i < 64; ++i)
	{
		int piece = Pos->Position->square[i];
		Board->square[i] = piece;
		if (piece)
		{
			Board->bb[piece & 1] |= Bit(i);
			Board->bb[piece] |= Bit(i);
		}
	}
}

void retrieve_position(GPos* Pos, int copy_stack)
{
	Current->key = Pos->Position->key;
	Current->pawn_key = Pos->Position->pawn_key;
	Current->move = Pos->Position->move;
	Current->capture = Pos->Position->capture;
	Current->turn = Pos->Position->turn;
	Current->castle_flags = Pos->Position->castle_flags;
	Current->ply = Pos->Position->ply;
	Current->ep_square = Pos->Position->ep_square;
	Current->piece = Pos->Position->piece;
	Current->pst = Pos->Position->pst;
	Current->material = Pos->Position->material;
	retrieve_board(Pos);
	date = Pos->date;
	if (copy_stack)
	{
		sp = Current->ply;
		for (int i = 0; i <= sp; ++i) Stack[sp - i] = Pos->stack[i];
	}
	else
		sp = Pos->sp;

	for (int i = 0; i < 16; ++i)
		for (int ik = 0; ik < N_KILLER; ++ik) (Current + i + 1)->killer[ik + 1] = Pos->killer[i][ik];
}

void halt_all(GSP* Sp, int locked)
{
	GMove* M;
	if (!locked)
		LOCK(Sp->lock);
	if (Sp->active)
	{
		for (int i = 0; i < Sp->move_number; ++i)
		{
			M = &Sp->move[i];
			if (T(M->flags & FlagClaimed) && F(M->flags & FlagFinished) && M->id != Id)
				SET_BIT_64(Smpi->stop, M->id);
		}
		Sp->active = Sp->claimed = 0;
		ZERO_BIT_64(Smpi->active_sp, (int)(Sp - Smpi->Sp));
	}
	if (!locked)
		UNLOCK(Sp->lock);
}

void halt_all(int from, int to)
{
	for (uint64 u = Smpi->active_sp; u; Cut(u))
	{
		GSP* Sp = &Smpi->Sp[lsb(u)];
		LOCK(Sp->lock);
		if (Sp->height >= from && Sp->height <= to)
			halt_all(Sp, 1);
		UNLOCK(Sp->lock);
	}
}

void init_sp(GSP* Sp, int alpha, int beta, int depth, int pv, int singular, int height)
{
	Sp->claimed = 1;
	Sp->active = Sp->finished = 0;
	Sp->best_move = 0;
	Sp->alpha = alpha;
	Sp->beta = beta;
	Sp->depth = depth;
	Sp->split = 0;
	Sp->singular = singular;
	Sp->height = height;
	Sp->move_number = 0;
	Sp->pv = pv;
}

int CutSearch(GSP* Sp)
{
	halt_all(Sp, 1);
	UNLOCK(Sp->lock);
	return Sp->beta;
}

template <bool me> int smp_search(GSP* Sp)
{
	int i, value, move, alpha;
	if (!Sp->move_number)
		return Sp->alpha;
	send_position(Sp->Pos);
	if (setjmp(Sp->jump))
	{
		LOCK(Sp->lock);
		halt_all(Sp, 1);
		UNLOCK(Sp->lock);
		halt_all(Sp->height + 1, 127);
		Current = Data + Sp->height;
		sp = Sp->Pos->sp;
		retrieve_board(Sp->Pos);
		return Sp->beta;
	}
	LOCK(Sp->lock);
	SET_BIT_64(Smpi->active_sp, (int)(Sp - Smpi->Sp));
	Sp->active = 1;
	Sp->claimed = Sp->finished = 0;
	
	for (int iter = 0; iter < 2; ++iter)
	{
		for (i = 0; i < Sp->move_number; ++i)
		{
			GMove* M = &Sp->move[i];
			if (!iter)
				Sp->current = i;
			if (M->flags & FlagFinished)
				continue;
			if (!iter)
			{
				if (M->flags & FlagClaimed)
					continue;
				M->flags |= FlagClaimed;
				M->id = Id;
			}
			else if (M->flags & FlagClaimed)
			{
				SET_BIT_64(Smpi->stop, M->id);
				M->id = Id;
			}
			move = M->move;
			alpha = Sp->alpha;
			UNLOCK(Sp->lock);
			do_move<me>(move);
			value = -scout<opp, 0, 0>(-alpha, M->reduced_depth, FlagNeatSearch | ExtToFlag(M->ext));
			if (value > alpha && (Sp->pv || M->reduced_depth < M->research_depth))
			{
				if (Sp->pv)
					value = -pv_search<opp, 0>(-Sp->beta, -Sp->alpha, M->research_depth, FlagNeatSearch | ExtToFlag(M->ext));
				else
					value = -scout<opp, 0, 0>(-alpha, M->research_depth, FlagNeatSearch | FlagDisableNull | ExtToFlag(M->ext));
			}
			undo_move<me>(move);
			LOCK(Sp->lock);
			if (Sp->finished)
				return CutSearch(Sp);
			M->flags |= FlagFinished;
			if (value > Sp->alpha)
			{
				Sp->best_move = move;
				Sp->alpha = Min<volatile int>(value, Sp->beta);
				if (value >= Sp->beta)
					return CutSearch(Sp);
			}
		}
	}
	halt_all(Sp, 1);
	UNLOCK(Sp->lock);
	return Sp->alpha;
}


template<bool exclusion, bool evasion> int cut_search(int move, int hash_move, int score, int beta, int depth, int flags, int sp_init)
{
	if (exclusion)
		return score;
	Current->best = move;
	if (!evasion && depth >= 10)
		score = Min(beta, score);
	if (F(PieceAt(To(move))) && F(move & 0xE000))
	{
		if (evasion)
			UpdateCheckRef(move);
		else
		{
			if (Current->killer[1] != move && F(flags & FlagNoKillerUpdate))
			{
				for (int jk = N_KILLER; jk > 1; --jk) Current->killer[jk] = Current->killer[jk - 1];
				Current->killer[1] = move;
			}
			if (Current->stage == s_quiet && (move & 0xFFFF) == (*(Current->current - 1) & 0xFFFF))
				HistoryGood(*(Current->current - 1), depth);	// restore history information
			else
				HistoryGood(move, depth);
			if (move != hash_move && Current->stage == s_quiet && !sp_init)
				for (auto p = Current->start; p < (Current->current - 1); ++p)
					HistoryBad(*p, depth);
			UpdateRef(move);
		}
	}
	return hash_low(move, score, depth);
};

INLINE int RazoringThreshold(int score, int depth, int height)
{
	return score + (70 + 8 * Max(height, depth) + 3 * Square(Max(0, depth - 7))) * CP_SEARCH;
}

template<bool me> bool IsThreat(int move)
{
	int to = To(move);
	switch (PieceAt(From(move)) >> 1)
	{
	case WhitePawn >> 1:
		return T(PAtt[me][to] & NonPawnKing(opp));
	case WhiteKnight >> 1:
		return T(NAtt[to] & Major(opp));
	case WhiteLight >> 1:
	case WhiteDark >> 1:
		return T(BishopAttacks(to, PieceAll()) & Major(opp));
	case WhiteRook >> 1:
		return T(RookAttacks(to, PieceAll()) & Queen(opp));
	}
	return false;
}

template<bool me, bool exclusion, bool evasion> int scout(int beta, int depth, int flags)
{
	int height = (int)(Current - Data);
	GSP* Sp = nullptr;

	if (!evasion)
	{
#ifndef TUNER
		if (nodes > check_node_smp + 0x10)
		{
#ifndef W32_BUILD
			InterlockedAdd64(&Smpi->nodes, (long long)(nodes)-(long long)(check_node_smp));
#else
			Smpi->nodes += (long long)(nodes)-(long long)(check_node_smp);
#endif
			check_node_smp = nodes;
			check_state();
			if (nodes > check_node + 0x4000 && parent)
			{
#ifdef TB
				InterlockedAdd64(&Smpi->tb_hits, tb_hits);
				tb_hits = 0;
#endif
				check_node = nodes;
#ifndef REGRESSION
				check_time(nullptr, 1);
#endif
				if (Searching)
					SET_BIT_64(Smpi->searching, Id);  // BUG, don't know why this is necessary
			}
		}
#endif
	}

	if (depth <= 1)
		return (evasion ? q_evasion<me, 0> : q_search<me, 0>)(beta - 1, beta, 1, flags);
	int score = height - MateValue;
	if (flags & FlagHaltCheck)
	{
		if (score >= beta)
			return beta;
		if (MateValue - height < beta)
			return beta - 1;
		if (!evasion)
		{
			halt_check;
		}
	}

	int hash_move = flags & 0xFFFF, cnt = 0, played = 0;
	int do_split = 0, sp_init = 0;
	int singular = 0;
	int high_depth = 0, hash_depth = -1;
	bool can_hash = true;
	if (exclusion)
	{
		can_hash = false;
		score = beta - 1;
		if (evasion)
		{
			(void) gen_evasions<me>(Current->moves);
			if (F(Current->moves[0]))
				return score;
		}
	}
	else
	{
		if (flags & FlagCallEvaluation)
			evaluate();
		if (!evasion && IsCheck(me))
			return scout<me, 0, 1>(beta, depth, flags & (~(FlagHaltCheck | FlagCallEvaluation)));

		if (!evasion)
		{
			int value = Current->score - (90 + depth * 8 + Max(depth - 5, 0) * 32) * CP_SEARCH;
			if (value >= beta && depth <= 13 && T(NonPawnKing(me)) && F(Pawn(opp) & OwnLine(me, 1) & Shift<me>(~PieceAll())) && F(flags & (FlagReturnBestMove | FlagDisableNull)))
				return value;

			value = Current->score + FutilityThreshold;
			if (value < beta && depth <= 3)
				return Max(value, q_search<me, 0>(beta - 1, beta, 1, FlagHashCheck | (flags & 0xFFFF)));
		}

		Current->best = hash_move;
		int high_value = MateValue, hash_value = -MateValue;
		if (GEntry* Entry = probe_hash())
		{
			if (Entry->high < beta && Entry->high_depth >= depth)
				return Entry->high;
			if (!evasion && Entry->high_depth > high_depth)
			{
				high_depth = Entry->high_depth;
				high_value = Entry->high;
			}
			if (T(Entry->move) && Entry->low_depth > hash_depth)
			{
				Current->best = hash_move = Entry->move;
				hash_depth = Entry->low_depth;
				if (!evasion && Entry->low_depth)
					hash_value = Entry->low;
			}
			if (Entry->low >= beta && Entry->low_depth >= depth)
			{
				if (Entry->move)
				{
					Current->best = Entry->move;
					if (F(PieceAt(To(Entry->move))) && F(Entry->move & 0xE000))
					{
						if (evasion)
							UpdateCheckRef(Entry->move);
						else
						{
							if (Current->killer[1] != Entry->move && F(flags & FlagNoKillerUpdate))
							{
								for (int jk = N_KILLER; jk > 1; --jk)
									Current->killer[jk] = Current->killer[jk - 1];
								Current->killer[1] = Entry->move;
							}
							UpdateRef(Entry->move);
						}
					}
				}
				if (F(flags & FlagReturnBestMove))	
					return Entry->low;
			}
			if (evasion && Entry->low_depth >= depth - 8 && Entry->low > hash_value)
				hash_value = Entry->low;
		}

#if TB
		if (hash_depth < NominalTbDepth && TB_LARGEST > 0 && depth >= TBMinDepth && unsigned(popcnt(PieceAll())) <= TB_LARGEST) {
			auto res = TBProbe(tb_probe_wdl, me);
			if (res != TB_RESULT_FAILED) {
				tb_hits++;
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
				if (F(flags & FlagReturnBestMove) && (Current->ply <= PliesToEvalCut) == (PVEntry->ply <= PliesToEvalCut))
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
		if (evasion && hash_depth >= depth && hash_value > -EvalValue)
			score = Min(beta - 1, Max(score, hash_value));
		if (evasion && T(flags & FlagCallEvaluation))
			evaluate();

		if (!evasion && depth >= 12 && (F(hash_move) || hash_value < beta || hash_depth < depth - 12) && (high_value >= beta || high_depth < depth - 12) &&
			F(flags & FlagDisableNull))
		{
			int new_depth = depth - 8;
			int value = scout<me, 0, 0>(beta, new_depth, FlagHashCheck | FlagNoKillerUpdate | FlagDisableNull | FlagReturnBestMove | hash_move);
			if (value >= beta)
			{
				if (Current->best)
					hash_move = Current->best;
				hash_depth = new_depth;
				hash_value = beta;
			}
		}
		if (!evasion && depth >= 4 && Current->score + 3 * CP_SEARCH >= beta && F(flags & (FlagDisableNull | FlagReturnBestMove)) && (high_value >= beta || high_depth < depth - 10) &&
			(depth < 12 || (hash_value >= beta && hash_depth >= depth - 12)) && beta > -EvalValue && T(NonPawnKing(me)))
		{
			int new_depth = depth - 8;
			do_null();
			int value = -scout<opp, 0, 0>(1 - beta, new_depth, FlagHashCheck);
			undo_null();
			if (value >= beta)
			{
				if (depth < 12)
					hash_low(0, value, depth);
				return value;
			}
		}

		if (evasion)
		{
			Current->mask = Filled;
			if (depth < 4 && Current->score - 10 * CP_SEARCH < beta)
			{
				score = Current->score - 10 * CP_SEARCH;
				Current->mask = capture_margin_mask<me>(beta - 1, &score);
			}
			(void)gen_evasions<me>(Current->moves);
			if (F(Current->moves[0]))
				return score;
		}


		cnt = singular = played = 0;
		if (T(hash_move))
		{
			int move = hash_move;
			if (is_legal<me>(move) && !IsIllegal(me, move))
			{
				++cnt;
				int ext = evasion && F(Current->moves[1]) 
						? 2 
						: (is_check<me>(move) ? check_extension<me, 0> : extension<me, 0>)(move, depth);
				if (ext < 2 && depth >= 16 && hash_value >= beta)
				{
					int test_depth = depth - Min(12, depth / 2);
					if (hash_depth >= test_depth)
					{
						int margin_one = beta - ExclSingle(depth);
						int margin_two = beta - ExclDouble(depth);
						int prev_ext = ExtFromFlag(flags);
						if (int singular = singular_extension<me>(ext, prev_ext, margin_one, margin_two, test_depth, hash_move))
							ext = Max(ext, singular + (prev_ext < 1) - (singular >= 2 && prev_ext >= 2));
					}
				}
				int to = To(move);
				if (depth < 16 && to == To(Current->move) && T(PieceAt(to)))	// recapture extension
					ext = Max(ext, 2);
				int new_depth = depth - 2 + ext;
				do_move<me>(move);
				if (evasion)
					evaluate();
				if (evasion && Current->att[opp] & King(me))
				{
					undo_move<me>(move);
					--cnt;
				}
				else
				{
					int new_flags = (evasion ? FlagNeatSearch ^ FlagCallEvaluation : FlagNeatSearch)
						| ((hash_value >= beta && hash_depth >= depth - 12) ? FlagDisableNull : 0)
						| ExtToFlag(ext);


					int value = -scout<opp, 0, 0>(1 - beta, new_depth, new_flags);
					undo_move<me>(move);
					++played;
					if (value > score)
					{
						score = value;
						if (value >= beta)
							return cut_search<exclusion, evasion>(move, hash_move, score, beta, depth, flags, 0);
					}
				}
			}
		}
	}

	// done with hash 
	int margin = 0;
	if (evasion)
	{
		Current->ref[0] = RefM(Current->move).check_ref[0];
		Current->ref[1] = RefM(Current->move).check_ref[1];
		mark_evasions(Current->moves);
	}
	else
	{
		Current->killer[0] = 0;
		Current->stage = stage_search;
		Current->gen_flags = 0;
		Current->ref[0] = RefM(Current->move).ref[0];
		Current->ref[1] = RefM(Current->move).ref[1];
		margin = RazoringThreshold(Current->score, depth, height);
		if (margin < beta)
		{
			can_hash = false;
			score = Max(margin, score);
			Current->stage = stage_razoring;
			Current->mask = Piece(opp);
			int value = margin + 200 * CP_SEARCH;
			if (value < beta)
			{
				score = Max(value, score);
				Current->mask ^= Pawn(opp);
			}
		}
		Current->moves[0] = 0;
		if (depth <= 5)
			Current->gen_flags |= FlagNoBcSort;
	}
	do_split = sp_init = 0;
	if (depth >= SplitDepth && PrN > 1 && parent && !exclusion)
		do_split = 1;
	int moves_to_play = 3 + Square(depth) / 6;
	Current->current = Current->moves;

	bool forced = evasion && F(Current->moves[1]);
	int move_back = 0;
	if (beta > 0 && Current->ply >= 2 && F((Current - 1)->move & 0xF000))
	{
		move_back = (To((Current - 1)->move) << 6) | From((Current - 1)->move);
		if (PieceAt(To(move_back)))
			move_back = 0;
	}

	while (int move = evasion ? pick_move() : get_move<me, 0>(depth))
	{
		if (move == hash_move)
			continue;
		if (IsIllegal(me, move))
			continue;
		++cnt;
		if ((move & 0xFFFF) == move_back && IsRepetition(beta, move))
		{
			score = Max(0, score);
			continue;
		}
		bool check = (!evasion && Current->stage == r_checks) || is_check<me>(move);
		int ext;
		if (evasion && forced)
			ext = 2;
		else if (check && see<me>(move, 0, SeeValue))
			ext = check_extension<me, 0>(move, depth);
		else
			ext = extension<me, 0>(move, depth);
		int new_depth = depth - 2 + ext;
		if (F(PieceAt(To(move))) && F(move & 0xE000))
		{
			if (evasion || !is_killer(move))
			{
				if (!check && cnt > moves_to_play)
				{
					Current->gen_flags &= ~FlagSort;
					continue;
				}
				if (depth >= 6 && (!evasion || cnt > 3))
				{
					int reduction = msb(cnt);
					if (!evasion && move == Current->ref[0] || move == Current->ref[1])
						reduction = Max(0, reduction - 1);
					if (reduction >= 2 && !(Queen(White) | Queen(Black)) && popcnt(NonPawnKingAll()) <= 4)
						reduction += reduction / 2;
					if (!evasion && new_depth - reduction > 3 && !see<me>(move, -SeeThreshold, SeeValue))
						reduction += 2;
					if (!evasion && reduction == 1 && new_depth > 4)
						reduction = cnt > 3 ? 2 : 0;
					new_depth = Max(3, new_depth - reduction);
				}
			}
			if (!check)
			{
				int value = Current->score + DeltaM(move) + 10 * CP_SEARCH;
				if (value < beta && depth <= 3)
				{
					score = Max(value, score);
					continue;
				}
				if (!evasion && cnt > 7 && (value = margin + DeltaM(move) - 25 * CP_SEARCH * msb(cnt)) < beta && depth <= 19)
				{
					score = Max(value, score);
					continue;
				}
			}
			if (!evasion && depth <= 9 && T(NonPawnKing(me)) && !see<me>(move, -SeeThreshold, SeeValue))
				continue;
		}
		else if (!evasion && !check && depth <= 9)
		{
			if ((Current->stage == s_bad_cap && depth <= 5)
				|| (Current->stage == r_cap && !see<me>(move, -SeeThreshold, SeeValue)))
					continue;
		}
		if (do_split && played >= 1)
		{
			uint64 u = 1;	// used to flag need for setup
			if (!sp_init)
			{
				sp_init = 1;
				u = ~Smpi->active_sp;
				if (!u)
					do_split = 0;
				else
				{
					Sp = &Smpi->Sp[lsb(u)];
					init_sp(Sp, beta - 1, beta, depth, 0, singular, height);
				}
			}
			if (u)	// already initialized, or have active sp
			{
				GMove* M = &Sp->move[Sp->move_number++];
				M->ext = ext;
				M->flags = 0;
				M->move = move;
				M->reduced_depth = new_depth;
				M->research_depth = depth - 2 + ext;
				M->stage = Current->stage;
				continue;
			}
		}

		// done splitting
		do_move<me>(move);
		int value = -scout<opp, 0, 0>(1 - beta, new_depth, FlagNeatSearch | ExtToFlag(ext));	// POSTPONED -- call scout_evasion here if check?
		if (value >= beta && new_depth < depth - 2 + ext)
			value = -scout<opp, 0, 0>(1 - beta, depth - 2 + ext, FlagNeatSearch | FlagDisableNull | ExtToFlag(ext));
		undo_move<me>(move);
		++played;
		if (value > score)
		{
			score = value;
			if (value >= beta)
				return cut_search<exclusion, evasion>(move, hash_move, score, beta, depth, flags, sp_init);
		}
	}
	if (do_split && sp_init)
	{
		int value = smp_search<me>(Sp);
		if (value >= beta && Sp->best_move)
		{
			score = beta;
			int move = Current->best = Sp->best_move;
			for (int i = 0; i < Sp->move_number; ++i)
			{
				GMove* M = &Sp->move[i];
				if ((M->flags & FlagFinished) && M->stage == s_quiet && M->move != move)
					HistoryBad(M->move, depth);
			}
		}
		if (value >= beta)
			return cut_search<exclusion, evasion>(Sp->best_move, hash_move, score, beta, depth, flags, sp_init);
	}
	if (!evasion && F(cnt) && can_hash)
	{
		hash_high(0, 127);
		hash_low(0, 0, 127);
		return 0;
	}
	if (F(exclusion))
		hash_high(score, depth);
	return score;
}


template <bool me, bool root> int pv_search(int alpha, int beta, int depth, int flags)
{
	int value, move, cnt, pext = 0, ext, hash_value = -MateValue, margin, do_split = 0, sp_init = 0, singular = 0, played = 0, new_depth, hash_move,
		hash_depth, old_alpha = alpha, old_best, ex_depth = 0, ex_value = 0, start_knodes = (int)(nodes >> 10);
	GSP* Sp = nullptr;
	int height = (int)(Current - Data);

	if (root)
	{
		depth = Max(depth, 2);
		flags |= ExtToFlag(1);
		if (F(RootList[0]))
			return 0;
		if (Print)
		{
			fprintf(stdout, "info depth %d\n", (depth / 2));
			fflush(stdout);
		}
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
		halt_check;
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
	if (!root && hash_depth < NominalTbDepth && TB_LARGEST > 0 && depth >= TBMinDepth && unsigned(popcnt(PieceAll())) <= TB_LARGEST)
	{
		auto res = TBProbe(tb_probe_wdl, me);
		if (res != TB_RESULT_FAILED) {
			tb_hits++;
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
				value = scout<me, 0, 0>(margin, new_depth, FlagHashCheck | FlagNoKillerUpdate | FlagDisableNull | FlagReturnBestMove | hash_move);
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
#ifndef TUNER
			memset(Data + 1, 0, 127 * sizeof(GData));
#endif
			move_to_string(move, score_string);
			if (Print)
				sprintf_s(info_string, "info currmove %s currmovenumber %d\n", score_string, cnt);
		}
		ext = is_check<me>(move) ? check_extension<me, 1>(move, depth) : Max(pext, extension<me, 1>(move, depth));
		if (depth >= 12 && hash_value > alpha && hash_depth >= (new_depth = depth - Min(12, depth / 2)))
		{
			int margin_one = hash_value - ExclSinglePV(depth);
			int margin_two = hash_value - ExclDoublePV(depth);
			int prev_ext = ExtFromFlag(flags);
			singular = singular_extension<me>(root ? 0 : ext, root ? 0 : prev_ext, margin_one, margin_two, new_depth, hash_move);
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
		if (PrN > 1) 
		{
			evaluate();
			if (Current->att[opp] & King(me))
			{
				undo_move<me>(move);	// we will detect whether move has been undone, below
				--cnt;
			}
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
					best_move = move;
					best_score = value;
					hash_low(best_move, value, depth);
					if (depth >= 14 || T(Console))
						send_pv(depth / 2, old_alpha, beta, value);
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
				if (depth >= 14 || T(Console))
					send_pv(depth / 2, old_alpha, beta, value);
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

	if (PrN > 1 && !root && parent && depth >= SplitDepthPV)
		do_split = 1;

	while (move = get_move<me, root>(depth))
	{
		if (move == hash_move)
			continue;
		if (IsIllegal(me, move))
			continue;
		++cnt;
		if (root)
		{
#ifndef TUNER
			memset(Data + 1, 0, 127 * sizeof(GData));
#endif
			move_to_string(move, score_string);
			if (Print)
				sprintf_s(info_string, "info currmove %s currmovenumber %d\n", score_string, cnt);
		}
		if (IsRepetition(alpha + 1, move))
			continue;
		bool check = is_check<me>(move);
		ext = check ? check_extension<me, 1>(move, depth) : Max(pext, extension<me, 1>(move, depth));
		new_depth = depth - 2 + ext;
		if (depth >= 6 && F(move & 0xE000) && F(PieceAt(To(move))) && (T(root) || !is_killer(move) || T(IsCheck(me))) && cnt > 3)
		{
			int reduction = msb(cnt) - 1;
			if (move == Current->ref[0] || move == Current->ref[1])
				reduction = Max(0, reduction - 1);
			if (reduction >= 2 && !(Queen(White) | Queen(Black)) && popcnt(NonPawnKingAll()) <= 4)
				reduction += reduction / 2;
			new_depth = Max(3, new_depth - reduction);
		}
		if (do_split && played >= 1)
		{
			if (!sp_init)
			{
				sp_init = 1;
				if (uint64 u = ~Smpi->active_sp)
				{
					Sp = &Smpi->Sp[lsb(u)];
					init_sp(Sp, alpha, beta, depth, 1, singular, height);
				}
				else
					do_split = 0;
			}
			if (do_split)
			{
				GMove* M = &Sp->move[Sp->move_number++];
				M->ext = ext;
				M->flags = 0;
				M->move = move;
				M->reduced_depth = new_depth;
				M->research_depth = depth - 2 + ext;
				M->stage = Current->stage;
				continue;
			}
		}

		do_move<me>(move);  // now Current points at child, until we undo_move, below
		if (new_depth <= 1)
			value = -pv_search<opp, 0>(-beta, -alpha, new_depth, ExtToFlag(ext));
		else
			value = -scout<opp, 0, 0>(-alpha, new_depth, FlagNeatSearch | ExtToFlag(ext));
		if (value > alpha && new_depth > 1)
		{
			if (root)
			{
				SetScore(&RootList[cnt - 1], 1);
				CurrentSI->Early = 0;
				old_best = best_move;
				best_move = move;
			}
			new_depth = depth - 2 + ext;
			value = -pv_search<opp, 0>(-beta, -alpha, new_depth, ExtToFlag(ext));
			if (root)
			{
				if (value <= alpha) 
					best_move = old_best;
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
				best_move = move;
				best_score = value;
				hash_low(best_move, value, depth);
				if (depth >= 14 || T(Console))
					send_pv(depth / 2, old_alpha, beta, value);
			}
			Current->best = move;
			if (value >= beta)
				return hash_low(move, value, depth);
			alpha = value;
		}
	}
	if (do_split && sp_init)
	{
		value = smp_search<me>(Sp);
		if (value > alpha && Sp->best_move)
		{
			alpha = value;
			Current->best = move = Sp->best_move;
		}
		if (value >= beta)
			return hash_low(move, alpha, depth);
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
			hash_exact(Current->best, alpha, depth, ex_value, ex_depth, static_cast<int>(nodes >> 10) - start_knodes);
	}
	return alpha;
}

template <bool me> void root()
{
	int i, depth, value, alpha, beta, start_depth = 2, hash_depth = 0, hash_value = 0; 
	int store_time = 0, time_est, ex_depth = 0, ex_value, prev_time = 0, knodes = 0;
	sint64 time;
	GPVEntry* PVEntry;

	++date;
	nodes = check_node = check_node_smp = 0;
#ifndef TUNER
	if (parent)
		Smpi->nodes = 0;
#endif
	memcpy(Data, Current, sizeof(GData));
	Current = Data;

#ifdef TB
	if (popcnt(PieceAll()) <= int(TB_LARGEST)) 
	{
		auto res = TBProbe(tb_probe_root_checked, me);
		if (res != TB_RESULT_FAILED) {
			int bestScore;
			best_move = GetTBMove(res, &bestScore);
			char movStr[16];
			move_to_string(best_move, movStr);
			// TODO -- if TB mate, find # of moves to mate
			printf("info depth 1 seldepth 1 score cp %d nodes 1 nps 0 tbhits 1 pv %s\n", best_score / CP_SEARCH, movStr);   // Fake PV
			send_best_move();
			Searching = 0;
			if (MaxPrN > 1) ZERO_BIT_64(Smpi->searching, 0);
			return;
		}
	}
#endif

	evaluate();
	gen_root_moves<me>();
	if (PVN > 1)
	{
		memset(MultiPV, 0, MAX_HEIGHT * sizeof(int));
		for (i = 0; MultiPV[i] = RootList[i]; ++i)
			;
	}
	best_move = RootList[0];
#ifdef REGRESSION
	return;
#endif
	if (F(best_move))
		return;
	if (F(Infinite) && !RootList[1])
	{
		Infinite = 1;
		value = pv_search<me, 1>(-MateValue, MateValue, 4, FlagNeatSearch);
		Infinite = 0;
		LastDepth = MAX_HEIGHT;
		send_pv(6, -MateValue, MateValue, value);
		send_best_move();
		Searching = 0;
		if (MaxPrN > 1)
			ZERO_BIT_64(Smpi->searching, 0);
		return;
	}

	memset(CurrentSI, 0, sizeof(GSearchInfo));
	memset(BaseSI, 0, sizeof(GSearchInfo));
	Previous = -MateValue;
	if (PVEntry = probe_pv_hash())
	{
		if (is_legal<me>(PVEntry->move) && PVEntry->move == best_move && PVEntry->depth > hash_depth)
		{
			hash_depth = PVEntry->depth;
			hash_value = PVEntry->value;
			ex_depth = PVEntry->ex_depth;
			ex_value = PVEntry->exclusion;
			knodes = PVEntry->knodes;
		}
	}
	if (T(hash_depth) && PVN == 1)
	{
		Previous = best_score = hash_value;
		depth = hash_depth;
		if (PVHashing)
		{
			send_pv(depth / 2, -MateValue, MateValue, best_score);
			start_depth = (depth + 2) & (~1);
		}
		if ((depth >= LastDepth - 8 || T(store_time)) && LastValue >= LastExactValue && hash_value >= LastExactValue && T(LastTime) && T(LastSpeed))
		{
			time = TimeLimit1;
			if (ex_depth >= depth - Min(12, depth / 2) && ex_value <= hash_value - ExclSinglePV(depth))
			{
				BaseSI->Early = 1;
				BaseSI->Singular = 1;
				if (ex_value <= hash_value - ExclDoublePV(depth))
				{
					time = (time * TimeSingTwoMargin) / 100;
					BaseSI->Singular = 2;
				}
				else
					time = (time * TimeSingOneMargin) / 100;
			}
			time_est = Min<int>(LastTime, (int)((sint64(knodes) << 10) / LastSpeed));
			time_est = Max(time_est, store_time);
			for (; ; )	// loop from set_prev_time
			{
				LastTime = prev_time = time_est;
				if (prev_time >= time && F(Infinite))
				{
					++InstCnt;
					if (time_est <= store_time)
						InstCnt = 0;
					if (InstCnt > 2)
					{
						if (T(store_time) && store_time < time_est)
						{
							time_est = store_time;
							continue;
						}
						LastSpeed = 0;
						LastTime = 0;
						prev_time = 0;
						break;
					}
					if (hash_value > 0 && Current->ply >= 2 && F(PieceAt(To(best_move))) && F(best_move & 0xF000) &&
						PrevMove == ((To(best_move) << 6) | From(best_move)))
						break;
					do_move<me>(best_move);
					if (Current->ply >= 100)
					{
						undo_move<me>(best_move);
						break;
					}
					for (i = 4; i <= Current->ply; i += 2)
						if (Stack[sp - i] == Current->key)
						{
							undo_move<me>(best_move);
							break;
						}
					undo_move<me>(best_move);
					LastDepth = depth;
					LastTime = prev_time;
					LastValue = LastExactValue = hash_value;
					send_best_move();
					Searching = 0;
					if (MaxPrN > 1)
						ZERO_BIT_64(Smpi->searching, 0);
					return;
				}
				else
					break;
			}
		}
	}
	else
		LastTime = 0;
	
	// set_jmp
	memcpy(SaveBoard, Board, sizeof(GBoard));
	memcpy(SaveData, Data, sizeof(GData));
	save_sp = sp;
	if (setjmp(Jump))
	{
		Current = Data;
		Searching = 0;
		if (MaxPrN > 1)
		{
			halt_all(0, 127);
			ZERO_BIT_64(Smpi->searching, 0);
		}
		memcpy(Board, SaveBoard, sizeof(GBoard));
		memcpy(Data, SaveData, sizeof(GData));
		sp = save_sp;
		send_best_move();
		return;
	}
	for (depth = start_depth; depth < DepthLimit; depth += 2)
	{
#ifndef TUNER
		memset(Data + 1, 0, 127 * sizeof(GData));
#endif
		CurrentSI->Early = 1;
		CurrentSI->Change = CurrentSI->FailHigh = CurrentSI->FailLow = CurrentSI->Singular = 0;
		if (PVN > 1)
			value = multipv<me>(depth);
		else if ((depth / 2) < 7 || F(Aspiration))
			LastValue = LastExactValue = value = pv_search<me, 1>(-MateValue, MateValue, depth, FlagNeatSearch);
		else
		{
			int deltaLo = FailLoInit, deltaHi = FailHiInit;
			alpha = Previous - deltaLo;
			beta = Previous + deltaHi;
			for (; ; )	// loop:
			{
				if (Max(deltaLo, deltaHi) >= 1300)
				{
					LastValue = LastExactValue = value = pv_search<me, 1>(-MateValue, MateValue, depth, FlagNeatSearch);
					break;
				}
				value = pv_search<me, 1>(alpha, beta, depth, FlagNeatSearch);
				if (value <= alpha)
				{
					CurrentSI->FailHigh = 0;
					CurrentSI->FailLow = 1;
					alpha -= deltaLo;
					deltaLo += (deltaLo * FailLoGrowth) / 64 + FailLoDelta;
					LastValue = value;
					memcpy(BaseSI, CurrentSI, sizeof(GSearchInfo));
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
					LastTime = Max(prev_time, (int)(get_time() - StartTime));
					LastSpeed = nodes / Max(LastTime, 1);
					if (depth + 2 < DepthLimit)
						depth += 2;
					InstCnt = 0;
#ifdef TIMING
					if (depth >= 6)
#endif
						check_time(&LastTime, 0);
#ifndef TUNER
					memset(Data + 1, 0, 127 * sizeof(GData));
#endif
					LastValue = value;
					memcpy(BaseSI, CurrentSI, sizeof(GSearchInfo));
					continue;
				}
				else
				{
					LastValue = LastExactValue = value;
					break;
				}
			}
		}

		CurrentSI->Bad = (value < Previous - 50 * CP_SEARCH);
		memcpy(BaseSI, CurrentSI, sizeof(GSearchInfo));
		LastDepth = depth;
		LastTime = Max(prev_time, (int)(get_time() - StartTime));
		LastSpeed = nodes / Max(LastTime, 1);
		Previous = value;
		InstCnt = 0;
#ifdef TIMING
		if (depth >= 6)
#endif
			check_time(&LastTime, 0);
	}
	Searching = 0;
	if (MaxPrN > 1)
		ZERO_BIT_64(Smpi->searching, 0);
	if (F(Infinite) || DepthLimit < MAX_HEIGHT)
		send_best_move();
}

template <bool me> int multipv(int depth)
{
	int move, low = MateValue, value, i, cnt, ext, new_depth = depth;
	fprintf(stdout, "info depth %d\n", (depth / 2));
	fflush(stdout);
	for (cnt = 0; cnt < PVN && T(move = (MultiPV[cnt] & 0xFFFF)); ++cnt)
	{
		MultiPV[cnt] = move;
		move_to_string(move, score_string);
		if (T(Print))
			sprintf_s(info_string, "info currmove %s currmovenumber %d\n", score_string, cnt + 1);
		new_depth = depth - 2 + (ext = extension<me, 1>(move, depth));
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
		best_move = MultiPV[0] & 0xFFFF;
		Current->score = MultiPV[0] >> 16;
		send_multipv((depth / 2), cnt);
	}
	for (; T(move = (MultiPV[cnt] & 0xFFFF)); ++cnt)
	{
		MultiPV[cnt] = move;
		move_to_string(move, score_string);
		if (T(Print))
			sprintf_s(info_string, "info currmove %s currmovenumber %d\n", score_string, cnt + 1);
		new_depth = depth - 2 + (ext = extension<me, 1>(move, depth));
		do_move<me>(move);
		value = -scout<opp, 0, 0>(-low, new_depth, FlagNeatSearch | ExtToFlag(ext));
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
			best_move = MultiPV[0] & 0xFFFF;
			Current->score = MultiPV[0] >> 16;
			low = MultiPV[PVN - 1] >> 16;
			send_multipv((depth / 2), cnt);
		}
	}
	return Current->score;
}

void send_pv(int depth, int alpha, int beta, int score)
{
	int i, pos, move, mate = 0, mate_score, sel_depth;
	sint64 nps, snodes, tbhits = 0;
	if (F(Print))
		return;
	for (sel_depth = 1; sel_depth < 127 && T((Data + sel_depth)->att[0]); ++sel_depth)
		;
	--sel_depth;
	pv_length = 64;
	if (F(move = best_move))
		move = RootList[0];
	if (F(move))
		return;
	PV[0] = move;
	if (Current->turn)
		do_move<1>(move);
	else
		do_move<0>(move);
	pvp = 1;
	pick_pv();
	if (Current->turn ^ 1)
		undo_move<1>(move);
	else
		undo_move<0>(move);
	pos = 0;
	for (i = 0; i < 64 && T(PV[i]); ++i)
	{
		if (pos > 0)
		{
			pv_string[pos] = ' ';
			++pos;
		}
		move = PV[i];
		pv_string[pos++] = ((move >> 6) & 7) + 'a';
		pv_string[pos++] = ((move >> 9) & 7) + '1';
		pv_string[pos++] = (move & 7) + 'a';
		pv_string[pos++] = ((move >> 3) & 7) + '1';
		if (IsPromotion(move))
		{
			if ((move & 0xF000) == FlagPQueen)
				pv_string[pos++] = 'q';
			else if ((move & 0xF000) == FlagPRook)
				pv_string[pos++] = 'r';
			else if ((move & 0xF000) == FlagPBishop)
				pv_string[pos++] = 'b';
			else if ((move & 0xF000) == FlagPKnight)
				pv_string[pos++] = 'n';
		}
		pv_string[pos] = 0;
	}
	score_string[0] = 'c';
	score_string[1] = 'p';
	if (score > EvalValue)
	{
		mate = 1;
		strcpy_s(score_string, "mate ");
		mate_score = (MateValue - score + 1) / 2;
		score_string[6] = 0;
	}
	else if (score < -EvalValue)
	{
		mate = 1;
		strcpy_s(score_string, "mate ");
		mate_score = -(score + MateValue + 1) / 2;
		score_string[6] = 0;
	}
	else
	{
		score_string[0] = 'c';
		score_string[1] = 'p';
		score_string[2] = ' ';
		score_string[3] = 0;
	}
	sint64 elapsed = get_time() - StartTime;
#ifdef MP_NPS
	snodes = Smpi->nodes;
#ifdef TB
	tbhits = Smpi->tb_hits;
#endif
#else
	snodes = nodes;
#endif
	nps = elapsed ? (snodes * 1000) / elapsed : 12345;
	if (score < beta)
	{
		if (score <= alpha)
			fprintf(stdout, "info depth %d seldepth %d score %s%d upperbound time %I64d nodes %I64d nps %I64d tbhits %I64d pv %s\n", depth, sel_depth, score_string,
					(mate ? mate_score : score / CP_SEARCH), elapsed, snodes, nps, tbhits, pv_string);
		else
			fprintf(stdout, "info depth %d seldepth %d score %s%d time %I64d nodes %I64d nps %I64d tbhits %I64d pv %s\n", depth, sel_depth, score_string, 
					(mate ? mate_score : score / CP_SEARCH), elapsed, snodes, nps, tbhits, pv_string);
	}
	else
		fprintf(stdout, "info depth %d seldepth %d score %s%d lowerbound time %I64d nodes %I64d nps %I64d tbhits %I64d pv %s\n", depth, sel_depth, score_string,
				(mate ? mate_score : score / CP_SEARCH), elapsed, snodes, nps, tbhits, pv_string);
	fflush(stdout);
}

void send_multipv(int depth, int curr_number)
{
	int i, j, pos, move, score;
	sint64 nps, snodes, tbhits = 0;
	if (F(Print))
		return;
	for (j = 0; j < PVN && T(MultiPV[j]); ++j)
	{
		pv_length = 63;
		pvp = 0;
		move = MultiPV[j] & 0xFFFF;
		score = MultiPV[j] >> 16;
		memset(PV, 0, 64 * sizeof(uint16));
		if (Current->turn)
			do_move<1>(move);
		else
			do_move<0>(move);
		pick_pv();
		if (Current->turn ^ 1)
			undo_move<1>(move);
		else
			undo_move<0>(move);
		for (i = 63; i > 0; --i) PV[i] = PV[i - 1];
		PV[0] = move;
		pos = 0;
		for (i = 0; i < 64 && T(PV[i]); ++i)
		{
			if (pos > 0)
			{
				pv_string[pos] = ' ';
				++pos;
			}
			move = PV[i];
			pv_string[pos++] = ((move >> 6) & 7) + 'a';
			pv_string[pos++] = ((move >> 9) & 7) + '1';
			pv_string[pos++] = (move & 7) + 'a';
			pv_string[pos++] = ((move >> 3) & 7) + '1';
			if (IsPromotion(move))
			{
				if ((move & 0xF000) == FlagPQueen)
					pv_string[pos++] = 'q';
				else if ((move & 0xF000) == FlagPRook)
					pv_string[pos++] = 'r';
				else if ((move & 0xF000) == FlagPBishop)
					pv_string[pos++] = 'b';
				else if ((move & 0xF000) == FlagPKnight)
					pv_string[pos++] = 'n';
			}
			pv_string[pos] = 0;
		}
		score_string[0] = 'c';
		score_string[1] = 'p';
		if (score > EvalValue)
		{
			strcpy_s(score_string, "mate ");
			score = (MateValue - score + 1) / 2;
			score_string[6] = 0;
		}
		else if (score < -EvalValue)
		{
			strcpy_s(score_string, "mate ");
			score = -(score + MateValue + 1) / 2;
			score_string[6] = 0;
		}
		else
		{
			score_string[0] = 'c';
			score_string[1] = 'p';
			score_string[2] = ' ';
			score_string[3] = 0;
		}
		nps = get_time() - StartTime;
#ifdef MP_NPS
		snodes = Smpi->nodes;
#ifdef TB
		tbhits = Smpi->tb_hits;
#endif
#else
		snodes = nodes;
#endif
		if (nps)
			nps = (snodes * 1000) / nps;
		fprintf(stdout, "info multipv %d depth %d score %s%d nodes %I64d nps %I64d tbhits %I64d pv %s\n", 
					j + 1, (j <= curr_number ? depth : depth - 1), score_string, score,	snodes, nps, tbhits, pv_string);
		fflush(stdout);
	}
}

void send_best_move()
{
	uint64 snodes;
	int ponder;
#ifdef CPU_TIMING
	GlobalTime[GlobalTurn] -= static_cast<int>(get_time() - StartTime) - GlobalInc[GlobalTurn];
	if (GlobalTime[GlobalTurn] < GlobalInc[GlobalTurn])
		GlobalTime[GlobalTurn] = GlobalInc[GlobalTurn];
#endif
	if (F(Print))
		return;
#ifdef MP_NPS
	snodes = Smpi->nodes;
#else
	snodes = nodes;
#endif
	fprintf(stdout, "info nodes %I64d score cp %d\n", snodes, best_score / CP_SEARCH);
	if (!best_move)
		return;
	Current = Data;
	evaluate();
	if (Current->turn)
		do_move<1>(best_move);
	else
		do_move<0>(best_move);
	pv_length = 1;
	pvp = 0;
	pick_pv();
	ponder = PV[0];
	if (Current->turn ^ 1)
		undo_move<1>(best_move);
	else
		undo_move<0>(best_move);
	move_to_string(best_move, pv_string);
	if (ponder)
	{
		move_to_string(ponder, score_string);
		fprintf(stdout, "bestmove %s ponder %s\n", pv_string, score_string);
	}
	else
		fprintf(stdout, "bestmove %s\n", pv_string);
	fflush(stdout);
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

void get_time_limit(char string[])
{
	const char* ptr;
	int i, time, inc, wtime, btime, winc, binc, moves, pondering, movetime = 0;
	double exp_moves = MovesTg - 1;
	char* strtok_context = nullptr;

	Infinite = 1;
	MoveTime = 0;
	SearchMoves = 0;
	SMPointer = 0;
	pondering = 0;
	TimeLimit1 = 0;
	TimeLimit2 = 0;
	wtime = btime = 0;
	winc = binc = 0;
	moves = 0;
	Stop = 0;
	DepthLimit = MAX_HEIGHT;
	ptr = strtok_s(string, " ", &strtok_context);
	for (ptr = strtok_s(nullptr, " ", &strtok_context); ptr != nullptr; ptr = strtok_s(nullptr, " ", &strtok_context))
	{
		if (!strcmp(ptr, "binc"))
		{
			ptr = strtok_s(nullptr, " ", &strtok_context);
			binc = atoi(ptr);
			Infinite = 0;
		}
		else if (!strcmp(ptr, "btime"))
		{
			ptr = strtok_s(nullptr, " ", &strtok_context);
			btime = atoi(ptr);
			Infinite = 0;
		}
		else if (!strcmp(ptr, "depth"))
		{
			ptr = strtok_s(nullptr, " ", &strtok_context);
			DepthLimit = 2 * atoi(ptr) + 2;
			Infinite = 1;
		}
		else if (!strcmp(ptr, "infinite"))
		{
			Infinite = 1;
		}
		else if (!strcmp(ptr, "movestogo"))
		{
			ptr = strtok_s(nullptr, " ", &strtok_context);
			moves = atoi(ptr);
			Infinite = 0;
		}
		else if (!strcmp(ptr, "winc"))
		{
			ptr = strtok_s(nullptr, " ", &strtok_context);
			winc = atoi(ptr);
			Infinite = 0;
		}
		else if (!strcmp(ptr, "wtime"))
		{
			ptr = strtok_s(nullptr, " ", &strtok_context);
			wtime = atoi(ptr);
			Infinite = 0;
		}
		else if (!strcmp(ptr, "movetime"))
		{
			ptr = strtok_s(nullptr, " ", &strtok_context);
			movetime = atoi(ptr);
			MoveTime = 1;
			Infinite = 0;
		}
		else if (!strcmp(ptr, "searchmoves"))
		{
			if (F(SearchMoves))
			{
				for (i = 0; i < 256; ++i) SMoves[i] = 0;
			}
			SearchMoves = 1;
			ptr += 12;
			while (ptr != nullptr && ptr[0] >= 'a' && ptr[0] <= 'h' && ptr[1] >= '1' && ptr[1] <= '8')
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
				SMoves[SMPointer] = move_from_string(pv_string);
				SMPointer++;
				ptr = strtok_s(nullptr, " ", &strtok_context);
			}
		}
		else if (!strcmp(ptr, "ponder"))
			pondering = 1;
	}
	if (pondering)
		Infinite = 1;
	if (Current->turn == White)
	{
		time = wtime;
		inc = winc;
	}
	else
	{
		time = btime;
		inc = binc;
	}
#ifdef CPU_TIMING
	if (CpuTiming)
	{
		time = GlobalTime[GlobalTurn];
		inc = GlobalInc[GlobalTurn];
		if (UciMaxDepth)
			DepthLimit = 2 * UciMaxDepth + 2;
	}
#endif
	if (moves)
		moves = Max(moves - 1, 1);
	int time_max = Max(time - Min(1000, time / 2), 0);
	double nmoves;
	if (moves)
		nmoves = moves;
	else
	{
		nmoves = (MovesTg - 1);
		if (Current->ply > 40)
			nmoves += Min(Current->ply - 40, (100 - Current->ply) / 2);
		exp_moves = nmoves;
	}
	TimeLimit1 = Min(time_max, static_cast<int>((time_max + (Min(exp_moves, nmoves) * inc)) / Min(exp_moves, nmoves)));
	TimeLimit2 = Min(time_max, static_cast<int>((time_max + (Min(exp_moves, nmoves) * inc)) / Min(3.0, Min(exp_moves, nmoves))));
	TimeLimit1 = Min(time_max, (TimeLimit1 * TimeRatio) / 100);
	if (Ponder)
		TimeLimit1 = (TimeLimit1 * PonderRatio) / 100;
	if (MoveTime)
	{
		TimeLimit2 = movetime;
		TimeLimit1 = TimeLimit2 * 100;
	}
	InfoTime = StartTime = get_time();
	Searching = 1;
	if (MaxPrN > 1)
		SET_BIT_64(Smpi->searching, 0);
	if (F(Infinite))
		PVN = 1;
	if (Current->turn == White)
		root<0>();
	else
		root<1>();
}

sint64 get_time()
{
#ifdef CPU_TIMING
#ifndef TIMING
	if (CpuTiming)
	{
#endif
		uint64 ctime;
		QueryProcessCycleTime(GetCurrentProcess(), &ctime);
#ifdef TIMING
		return ctime / (CyclesPerMSec / 1000);
#endif
		return (ctime / CyclesPerMSec);
#ifndef TIMING
	}
#endif
#endif

#ifdef W32_BUILD
	return GetTickCount();
#else
	return GetTickCount64();
#endif
}

int time_to_stop(GSearchInfo* SI, int time, int searching)
{
	if (Infinite)
		return 0;
	if (time > TimeLimit2)
		return 1;
	if (searching)
		return 0;
	if (2 * time > TimeLimit2 && F(MoveTime))
		return 1;
	if (SI->Bad)
		return 0;
	if (time > TimeLimit1)
		return 1;
	if (T(SI->Change) || T(SI->FailLow))
		return 0;
	if (time * 100 > TimeLimit1 * TimeNoChangeMargin)
		return 1;
	if (F(SI->Early))
		return 0;
	if (time * 100 > TimeLimit1 * TimeNoPVSCOMargin)
		return 1;
	if (SI->Singular < 1)
		return 0;
	if (time * 100 > TimeLimit1 * TimeSingOneMargin)
		return 1;
	if (SI->Singular < 2)
		return 0;
	if (time * 100 > TimeLimit1 * TimeSingTwoMargin)
		return 1;
	return 0;
}

void check_time(const int* time, int searching)
{
#ifdef CPU_TIMING
	if (!time && CpuTiming && UciMaxKNodes && nodes > UciMaxKNodes * 1024)
		Stop = 1;
#endif
#ifdef TUNER
#ifndef TIMING
	return;
#endif
#else
	while (!Stop && input()) uci();
#endif
	if (!Stop)
	{
		CurrTime = get_time();
		int Time = static_cast<int>(CurrTime - StartTime);
		if (T(Print) && Time > InfoLag && CurrTime - InfoTime > InfoDelay)
		{
			InfoTime = CurrTime;
			if (info_string[0])
			{
				fprintf(stdout, "%s", info_string);
				info_string[0] = 0;
				fflush(stdout);
			}
		}
		if (F(time_to_stop(CurrentSI, time ? *time : Time, searching)))
			return;
	}

	Stop = 1;
	longjmp(Jump, 1);
}

void check_state()
{
	GSP* Sp, *Spc;
	int n, nc, score, best, pv, alpha, beta, new_depth, r_depth, ext, move, value;
	GMove* M;

	if (parent)
	{
		for (uint64 u = TEST_RESET(Smpi->fail_high); u; Cut(u))
		{
			Sp = &Smpi->Sp[lsb(u)];
			LOCK(Sp->lock);
			if (Sp->active && Sp->finished)
			{
				UNLOCK(Sp->lock);
				longjmp(Sp->jump, 1);
			}
			UNLOCK(Sp->lock);
		}
		return;
	}

	for (; ; ) // start:
	{
		if (TEST_RESET_BIT(Smpi->stop, Id))
			longjmp(CheckJump, 1);
		if (HasBit(Smpi->searching, Id))
			return;
		if (!(Smpi->searching & 1))
		{
			Sleep(1);
			return;
		}
		while ((Smpi->searching & 1) && !Smpi->active_sp) _mm_pause();
		while ((Smpi->searching & 1) && !HasBit(Smpi->searching, Id - 1)) _mm_pause();

		Sp = nullptr;
		best = -0x7FFFFFFF;
		for (uint64 u = Smpi->active_sp; u; Cut(u))
		{
			Spc = &Smpi->Sp[lsb(u)];
			if (!Spc->active || Spc->finished || Spc->lock)
				continue;
			for (nc = Spc->current + 1; nc < Spc->move_number; ++nc)
				if (!(Spc->move[nc].flags & FlagClaimed))
					break;
			if (nc < Spc->move_number)
				score = 1024 * 1024 + 512 * 1024 * (Spc->depth >= 20) + 128 * 1024 * (!(Spc->split)) + ((Spc->depth + 2 * Spc->singular) * 1024) -
				(((16 * 1024) * (nc - Spc->current)) / nc);
			else
				continue;
			if (score > best)
			{
				best = score;
				Sp = Spc;
				n = nc;
			}
		}

		if (Sp == nullptr)
			continue;
		if (!Sp->active || Sp->finished || (Sp->move[n].flags & FlagClaimed) || n <= Sp->current || n >= Sp->move_number)
			continue;
		if (Sp->lock)
			continue;

		LOCK(Sp->lock);
		if (!Sp->active || Sp->finished || (Sp->move[n].flags & FlagClaimed) || n <= Sp->current || n >= Sp->move_number)
		{
			UNLOCK(Sp->lock);
			continue;
		}
		break;
	}

	M = &Sp->move[n];
	M->flags |= FlagClaimed;
	M->id = Id;
	Sp->split |= Bit(Id);
	pv = Sp->pv;
	alpha = Sp->alpha;
	beta = Sp->beta;
	new_depth = M->reduced_depth;
	r_depth = M->research_depth;
	ext = M->ext;
	move = M->move;

	Current = Data;
	retrieve_position(Sp->Pos, 1);
	evaluate();
	SET_BIT_64(Smpi->searching, Id);
	UNLOCK(Sp->lock);

	if (setjmp(CheckJump))
	{
		ZERO_BIT_64(Smpi->searching, Id);
		return;
	}
	if (Current->turn == White)
	{
		do_move<0>(move);
		if (pv)
		{
			value = -scout<1, 0, 0>(-alpha, new_depth, FlagNeatSearch | ExtToFlag(ext));
			if (value > alpha)
				value = -pv_search<1, 0>(-beta, -alpha, r_depth, ExtToFlag(ext));
		}
		else
		{
			value = -scout<1, 0, 0>(1 - beta, new_depth, FlagNeatSearch | ExtToFlag(ext));
			if (value >= beta && new_depth < r_depth)
				value = -scout<1, 0, 0>(1 - beta, r_depth, FlagNeatSearch | FlagDisableNull | ExtToFlag(ext));
		}
		undo_move<0>(move);
	}
	else
	{
		do_move<1>(move);
		if (pv)
		{
			value = -scout<0, 0, 0>(-alpha, new_depth, FlagNeatSearch | ExtToFlag(ext));
			if (value > alpha)
				value = -pv_search<0, 0>(-beta, -alpha, r_depth, ExtToFlag(ext));
		}
		else
		{
			value = -scout<0, 0, 0>(1 - beta, new_depth, FlagNeatSearch | ExtToFlag(ext));
			if (value >= beta && new_depth < r_depth)
				value = -scout<0, 0, 0>(1 - beta, r_depth, FlagNeatSearch | FlagDisableNull | ExtToFlag(ext));
		}
		undo_move<1>(move);
	}

	LOCK(Sp->lock);
	ZERO_BIT_64(Smpi->searching, Id);
	if (TEST_RESET_BIT(Smpi->stop, Id))
	{
		UNLOCK(Sp->lock);
		return;
	}
	M->flags |= FlagFinished;
	if (value > Sp->alpha)
	{
		Sp->alpha = Min(value, beta);
		Sp->best_move = move;
		if (value >= beta)
		{
			Sp->finished = 1;
			SET_BIT_64(Smpi->fail_high, (int)(Sp - Smpi->Sp));
		}
	}
	UNLOCK(Sp->lock);
}

int input()
{
	if (child)
		return 0;
	DWORD p;
	if (F(Input))
		return 0;
	if (F(Console))
	{
		if (PeekNamedPipe(StreamHandle, nullptr, 0, nullptr, &p, nullptr))
			return (p > 0);
		else
			return 1;
	}
	else
		return 0;
}

void epd_test(char filename[], int time_limit)
{
	int n = 0, positions = 4000;
	uint64 Time, all_nodes = 0, new_time, total_time;
	double prod = 0.0;
	char* ptr;
	FILE* fh = nullptr;
	if (fopen_s(&fh, filename, "r") || fh == nullptr)
	{
		fprintf(stdout, "File not found\n");
		return;
	}
	Infinite = 1;
	Time = get_time();
	int total_depth = 0;
	Print = 0;
	Input = 0;
	total_time = 1;
	while (!feof(fh) && n < positions)
	{
	new_position:
		(void)fgets(mstring, 65536, fh);
		ptr = strchr(mstring, '\n');
		if (ptr != nullptr)
			*ptr = 0;
		get_board(mstring);
		evaluate();
		if (Current->turn == White)
		{
			gen_root_moves<0>();
		}
		else
		{
			gen_root_moves<1>();
		}
		Infinite = 0;
		MoveTime = TimeLimit1 = 100000000;
#ifndef TIME_TO_DEPTH
		TimeLimit2 = time_limit;
#else
		TimeLimit2 = TimeLimit1;
#endif
		DepthLimit = 127;
		++n;
		Stop = 0;
		Smpi->nodes = nodes = check_node = check_node_smp = 0;
		StartTime = get_time();
		if (setjmp(Jump))
		{
			halt_all(0, 127);
#ifdef TIME_TO_DEPTH
		stop_searching:
#endif
			ZERO_BIT_64(Smpi->searching, Id);
			Searching = 0;
			Current = Data;
			new_time = Max(get_time() - StartTime, 1ll);
			total_time += new_time;
#ifdef MP_NPS
			all_nodes += Smpi->nodes;
#else
			all_nodes += nodes;
#endif
			total_depth += LastDepth / 2;
#ifndef TIME_TO_DEPTH
			fprintf(stdout, "Position %d: %d [%lf, %lld]\n", n, LastDepth / 2, ((double)total_depth) / ((double)n), (all_nodes * 1000ull) / total_time);
#else
			prod += log((double)new_time);
			fprintf(stdout, "Position %d: %lld [%.0lf, %lld]\n", n, new_time, exp(prod / (double)n), (all_nodes * 1000ull) / total_time);
#endif
			goto new_position;
		}
		for (int d = 4; d < MAX_HEIGHT; d += 2)
		{
			LastDepth = d;
			Searching = 1;
			SET_BIT_64(Smpi->searching, Id);
			if (Current->turn == White)
			{
				pv_search<0, 1>(-MateValue, MateValue, d, FlagNeatSearch);
			}
			else
			{
				pv_search<1, 1>(-MateValue, MateValue, d, FlagNeatSearch);
			}
#ifdef TIME_TO_DEPTH
			if (d >= (time_limit * 2))
				goto stop_searching;
#endif
		}
	}
	if (n == 0)
	{
		fprintf(stdout, "Empty file\n");
		return;
	}
	fclose(fh);
}

void uci()
{
	const char* mdy = __DATE__;
	const char now[10] = { mdy[7], mdy[8], mdy[9], mdy[10], mdy[0], mdy[1], mdy[2], mdy[4] == ' ' ? '0' : mdy[4], mdy[5], 0 };
	char* ptr = nullptr;
	char * strtok_context = nullptr;
	int i;

	(void)fgets(mstring, 65536, stdin);
	if (feof(stdin))
		exit(0);
	ptr = strchr(mstring, '\n');
	if (ptr != nullptr)
		*ptr = 0;
	if (!strcmp(mstring, "uci"))
	{
		fprintf(stdout, "id name Roc ");
		fprintf(stdout, now);
		fprintf(stdout, "\n");
		fprintf(stdout, "id author Demichev/Hyer\n");
#ifndef W32_BUILD
		fprintf(stdout, "option name Hash type spin min 1 max 65536 default 16\n");
#else
		fprintf(stdout, "option name Hash type spin min 1 max 1024 default 16\n");
#endif
		fprintf(stdout, "option name Ponder type check default false\n");
		fprintf(stdout, "option name MultiPV type spin min 1 max 64 default 1\n");
		fprintf(stdout, "option name Clear Hash type button\n");
		fprintf(stdout, "option name PV Hash type check default true\n");
		fprintf(stdout, "option name Contempt type spin min 0 max 64 default 8\n");
		fprintf(stdout, "option name Wobble type spin min 0 max 3 default 0\n");
		fprintf(stdout, "option name Aspiration window type check default true\n");
#ifdef TB
		fprintf(stdout, "option name SyzygyPath type string default <empty>\n");
#endif
#ifdef CPU_TIMING
		fprintf(stdout, "option name CPUTiming type check default false\n");
		fprintf(stdout, "option name MaxDepth type spin min 0 max 128 default 0\n");
		fprintf(stdout, "option name MaxKNodes type spin min 0 max 65536 default 0\n");
		fprintf(stdout, "option name BaseTime type spin min 0 max 1000000 default 1000\n");
		fprintf(stdout, "option name IncTime type spin min 0 max 1000000 default 5\n");
#endif
		fprintf(stdout, "option name Threads type spin min 1 max %d default %d\n", Min(CPUs, MaxPrN), PrN);
#ifdef LARGE_PAGES
		fprintf(stdout, "option name Large memory pages type check default true\n");
#endif
		fprintf(stdout, "uciok\n");
		if (F(Searching))
			init_search(1);
	}
	else if (!strcmp(mstring, "ucinewgame"))
	{
		Stop = 0;
		init_search(1);
	}
	else if (!strcmp(mstring, "isready"))
	{
		fprintf(stdout, "readyok\n");
		fflush(stdout);
	}
	else if (!memcmp(mstring, "position", 8))
	{
		if (F(Searching))
			get_position(mstring);
	}
	else if (!memcmp(mstring, "go", 2))
	{
		if (F(Searching))
			get_time_limit(mstring);
	}
	else if (!memcmp(mstring, "setoption", 9))
	{
		ptr = strtok_s(mstring, " ", &strtok_context);
		for (ptr = strtok_s(nullptr, " ", &strtok_context); ptr != nullptr; ptr = strtok_s(nullptr, " ", &strtok_context))
		{
			if (!memcmp(ptr, "Hash", 4) && !Searching)
			{
				ptr += 11;
				int value = Max(1, atoi(ptr));
#ifdef W32_BUILD
				value = Min(value, 1024);
#else
				value = Min(value, 65536);
#endif
				value = static_cast<int>(Bit(msb(value) + 20) / static_cast<uint64>(sizeof(GEntry)));
				if (value != hash_size)
				{
					ResetHash = 1;
					hash_size = value;
					longjmp(ResetJump, 1);
				}
			}
			else if (!memcmp(ptr, "Threads", 7) && !Searching)
			{
				ptr += 14;
				int value = atoi(ptr);
				if (value != PrN)
				{
					NewPrN = Max(1, Min(MaxPrN, value));
					ResetHash = 0;
					longjmp(ResetJump, 1);
				}
			}
			else if (!memcmp(ptr, "MultiPV", 7))
			{
				ptr += 14;
				PVN = atoi(ptr);
				Stop = 1;
			}
			else if (!memcmp(ptr, "Contempt", 8))
			{
				ptr += 15;
				Contempt = atoi(ptr);
				Stop = 1;
			}
			else if (!memcmp(ptr, "Wobble", 6))
			{
				ptr += 13;
				Wobble = atoi(ptr);
				Stop = 1;
			}
			else if (!memcmp(ptr, "Ponder", 6))
			{
				ptr += 13;
				if (ptr[0] == 't')
					Ponder = 1;
				else
					Ponder = 0;
			}
			else if (!memcmp(ptr, "Clear", 5))
			{
				init_search(1);
				break;
			}
			else if (!memcmp(ptr, "PV", 2))
			{
				ptr += 14;
				if (ptr[0] == 't')
					PVHashing = 1;
				else
					PVHashing = 0;
			}
			else if (!memcmp(ptr, "Large", 5) && !Searching)
			{
				ptr += 25;
				if (ptr[0] == 't')
				{
					if (LargePages)
						return;
					LargePages = 1;
				}
				else
				{
					if (!LargePages)
						return;
					LargePages = 0;
				}
				ResetHash = 1;
				longjmp(ResetJump, 1);
			}
			else if (!memcmp(ptr, "Aspiration", 10))
			{
				ptr += 24;
				if (ptr[0] == 't')
					Aspiration = 1;
				else
					Aspiration = 0;
			}
#ifdef TB
			else if (!memcmp(ptr, "SyzygyPath", 10))
			{
				ptr += 17;
				strncpy(Smpi->tb_path, ptr, sizeof(Smpi->tb_path) - 1);
				ResetHash = 0;
				longjmp(ResetJump, 1);
			}
#endif
#ifdef CPU_TIMING
			else if (!memcmp(ptr, "CPUTiming", 9))
			{
				ptr += 16;
				if (ptr[0] == 't')
					CpuTiming = 1;
				else
					CpuTiming = 0;
			}
			else if (!memcmp(ptr, "MaxDepth", 8))
				UciMaxDepth = atoi(ptr + 15);
			else if (!memcmp(ptr, "MaxKNodes", 9))
				UciMaxKNodes = atoi(ptr + 16);
			else if (!memcmp(ptr, "BaseTime", 8))
				UciBaseTime = atoi(ptr + 15);
			else if (!memcmp(ptr, "IncTime", 7))
				UciIncTime = atoi(ptr + 14);
#endif
		}
	}
	else if (!strcmp(mstring, "stop"))
	{
		Stop = 1;
		if (F(Searching))
			send_best_move();
	}
	else if (!strcmp(mstring, "ponderhit"))
	{
		Infinite = 0;
		if (!RootList[1])
			Stop = 1;
		if (F(CurrentSI->Bad) && F(CurrentSI->FailLow) && time_to_stop(BaseSI, LastTime, 0))
			Stop = 1;
		if (F(Searching))
			send_best_move();
	}
	else if (!strcmp(mstring, "quit"))
	{
		for (i = 1; i < PrN; ++i)
		{
			TerminateProcess(ChildPr[i], 0);
			CloseHandle(ChildPr[i]);
		}
		exit(0);
	}
	else if (!memcmp(mstring, "epd", 3))
	{
		ptr = mstring + 4;
		epd_test("op.epd", atoi(ptr));
	}
}

HANDLE CreateChildProcess(int child_id)
{
	char name[1024];
	TCHAR szCmdline[1024];
	PROCESS_INFORMATION piProcInfo;
	STARTUPINFO siStartInfo;
	BOOL bSuccess = FALSE;

	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	ZeroMemory(szCmdline, 1024 * sizeof(TCHAR));
	ZeroMemory(name, 1024);

	siStartInfo.cb = sizeof(STARTUPINFO);
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	GetModuleFileName(nullptr, name, 1024);
	sprintf_s(szCmdline, " child %d %d", WinParId, child_id);

	bSuccess = CreateProcess(TEXT(name), TEXT(szCmdline), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &siStartInfo, &piProcInfo);

	if (bSuccess)
	{
		CloseHandle(piProcInfo.hThread);
		return piProcInfo.hProcess;
	}
	else
	{
		fprintf(stdout, "Error %d\n", GetLastError());
		return nullptr;
	}
}

#ifndef REGRESSION
void main(int argc, char* argv[])
{
	DWORD p;
	int i, HT = 0;
	SYSTEM_INFO sysinfo;

	if (argc >= 2)
		if (!memcmp(argv[1], "child", 5))
		{
			child = 1;
			parent = 0;
			WinParId = atoi(argv[2]);
			Id = atoi(argv[3]);
		}

	int CPUInfo[4] = { -1, 0, 0, 0 };
	__cpuid(CPUInfo, 1);
	HardwarePopCnt = (CPUInfo[2] >> 23) & 1;

	if (parent)
	{
		if (((CPUInfo[3] >> 28) & 1) && GetProcAddress(GetModuleHandle(TEXT("kernel32")), "GetLogicalProcessorInformation") != nullptr)
		{
			p = sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
#ifndef W32_BUILD
			SYSTEM_LOGICAL_PROCESSOR_INFORMATION syslogprocinfo[1];
			GetLogicalProcessorInformation(syslogprocinfo, &p);
			if (syslogprocinfo->ProcessorCore.Flags == 1)
				HT = 1;
#endif
		}
		WinParId = GetProcessId(GetCurrentProcess());
		HANDLE JOB = CreateJobObject(nullptr, nullptr);
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
		jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		SetInformationJobObject(JOB, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
		AssignProcessToJobObject(JOB, GetCurrentProcess());
		if (MaxPrN > 1)
		{
			GetSystemInfo(&sysinfo);
			CPUs = sysinfo.dwNumberOfProcessors;
			PrN = Min(CPUs, MaxPrN);
			if (HT)
				PrN = Max(1, Min(PrN, CPUs / 2));
		}
	}

#ifdef CPU_TIMING
	SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
#endif

	init();

#ifdef TB
	if (parent)
		if (auto val = getenv("GULL_SYZYGY_PATH"))
			strncpy(Smpi->tb_path, val, sizeof(Smpi->tb_path) - 1);
#endif

	StreamHandle = GetStdHandle(STD_INPUT_HANDLE);
	Console = GetConsoleMode(StreamHandle, &p);
	if (Console)
	{
		SetConsoleMode(StreamHandle, p & (~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT)));
		FlushConsoleInputBuffer(StreamHandle);
	}

	setvbuf(stdout, nullptr, _IONBF, 0);
	setvbuf(stdin, nullptr, _IONBF, 0);
	fflush(nullptr);

	fprintf(stdout, "Roc\n");

reset_jump:
#ifndef TUNER
	if (parent)
	{
		if (setjmp(ResetJump))
		{
			for (i = 1; i < PrN; ++i) TerminateProcess(ChildPr[i], 0);
			for (i = 1; i < PrN; ++i)
			{
				WaitForSingleObject(ChildPr[i], INFINITE);
				CloseHandle(ChildPr[i]);
			}
			Smpi->searching = Smpi->active_sp = Smpi->stop = 0;
			for (i = 0; i < MaxSplitPoints; ++i) Smpi->Sp->active = Smpi->Sp->claimed = 0;

			Smpi->hash_size = hash_size;
			if (NewPrN)
				Smpi->PrN = PrN = NewPrN;
			goto reset_jump;
		}
		Smpi->hash_size = hash_size;
		Smpi->PrN = PrN;
	}
	else
	{
		hash_size = Smpi->hash_size;
		PrN = Smpi->PrN;
	}
#endif
	if (ResetHash)
		init_hash();
	init_search(0);
#ifdef TB
	tb_init_fwd(Smpi->tb_path);
#endif

	if (child)
		while (true) check_state();
	if (parent)
		for (i = 1; i < PrN; ++i) ChildPr[i] = CreateChildProcess(i);

#ifdef EXPLAIN_EVAL
	get_board("3rr1k1/1pp2qpp/pnn1pp2/8/3PPB2/PQ3N1P/1PR2PP1/2R3K1 b - - 0 28");
	fexplain = fopen("evaluation.txt", "w");
	explain = 1;
	evaluate();
	fclose(fexplain);
	fprintf(stdout, "Press any key...\n");
	getchar();
	exit(0);
#endif

#ifdef TUNER
	if (argc >= 2)
	{
		if (!memcmp(argv[1], "client", 6))
			Client = 1;
		else if (!memcmp(argv[1], "server", 6))
			Server = 1;
		if (Client || Server)
			Local = 0;
	}
	fprintf(stdout, Client ? "Client\n" : (Server ? "Server\n" : "Local\n"));

	uint64 ctime;
	QueryProcessCycleTime(GetCurrentProcess(), &ctime);
	srand(time(nullptr) + 123 * GetProcessId(GetCurrentProcess()) + ctime);
	QueryProcessCycleTime(GetCurrentProcess(), &ctime);
	seed = (uint64)(time(nullptr) + 345 * GetProcessId(GetCurrentProcess()) + ctime);
	init_openings();
	init_variables();

	if (Client)
	{
#ifdef RECORD_GAMES
		RecordGames = 1;
		Buffer = (char*)malloc(16 * 1024 * 1024);
#endif
		while (true) get_command();
	}

	init_pst();
	init_eval();
	print_eval();

	// read_list("(-0.24,0.69,-3.56,14.38,-18.97,-9.43,31.93,-42.58,-84.76,-239.60,62.93,83.44,-124.95,25.59,-22.50,152.24,472.44,-652.13,-903.63,-16.63,11.50,-0.02,-202.44,29.65,-2.27,-62.69,-81.95,61.32,-492.11,-51.01,-23.03,-15.79,283.90,-116.64,-4.38,-92.49,-30.59,-48.53,-35.85,15.25,-83.44,-32.20,33.31,-14.71,27.13,215.48,-48.91,-107.82,5.28,-59.32,-9.16,-16.93,-21.26,-21.12,-35.52,-41.67,-35.52,-16.59,21.48,-1.20,-26.27,-23.81,-58.82,-9.36,38.87,-34.02,-10.33,0.07,101.64,11.30,-66.04,-4.39,10.43,-60.66,-6.41,0.68,-15.18,-69.89,-41.54,-84.48,-143.38,-46.16,-3.12,-13.96,31.00,-16.14,-89.96,100.44,-137.64,97.51,-85.03,62.93,78.39,444.37,-143.70,25.65,-74.57,-143.94,-106.03,-128.86,285.08,111.90,-24.94,-104.36,-142.29,-59.11,-92.95,-32.91,-153.55,15.40,-181.39,-35.76,14.98,-5.08,76.49,-80.38,177.51,132.39,-134.36,-6.67,49.81,-260.99,101.53,-41.31,-26.30,418.42,220.09,-127.18,762.99,-117.88,246.62,-203.99,18.52,266.32,290.73,112.16,292.84,127.11,277.25,189.46,214.95,304.06,399.54,-195.77,280.34,351.89,-485.96,-2.82,251.09,38.25,82.39,152.04,53.11,8.04,7.61,-21.45,10.43,-0.53,4.19,-9.26,13.89,14.56,19.18,7.64,-2.16,138.97,6.71,57.43,0.28,56.89,0.92,-9.14,35.31,1.05,8.57,10.12,34.71,0.23,71.71,76.05,153.65,114.23,85.39,1.34,-12.79,26.11,48.42,125.83,147.73,148.27,41.60,42.53,-14.37,6.87,-6.88,-2.23,130.20,22.09,45.46,15.40,13.11,8.80,2.28,2.99,-0.83,-3.11,-0.81,4.40,6.09,6.27,5.79,5.24,-2.88,-0.26,16.45,-2.67,11.20,7.72,6.17,1.23,3.61,0.08,-0.51,-0.25,9.09,2.08,0.69,0.35,13.18,6.69,0.52,1.58,1.56,-0.95,11.40,0.81,-6.78,3.32,-4.89,8.87,-5.50,31.67,0.30,2.94,0.18,5.42,14.11,33.51,28.03,32.65,21.20,11.16,48.32,14.90,4.31,2.41,2.18,2.69,0.78,0.05,4.27,1.51,17.77,7.82,5.21,1.29,0.15,4.35,-0.12,-0.06,-0.25,3.24,5.37,5.85,14.36,-1.62,9.45,0.47,4.07,5.19,26.33,2.20,20.31,37.81,1.02,82.85,56.61,23.77,19.82,-3.83,47.50,25.50)",
	// Base, active_vars);
	// eval_to_cpp("gd.cpp", Base);

	save_list(Base);

	// pgn_stat();
#ifdef RECORD_GAMES
	match_los(Base, Base, 4 * 64 * 1024, 512, 7, 0.0, 0.0, 0.0, 0.0, MatchInfo, 1);
#endif

	double NormalizedVar[MaxVariables];
	NormalizeVar(Base, Var, 2, 256, 10.0, 20.0, NormalizedVar);
	double_to_double(Var, NormalizedVar, active_vars);
	// read_list("(4.10,3.41,7.24,11.13,44.59,41.51,67.17,126.38,183.25,328.40,95.55,442.95,110.04,439.85,199.82,506.49,1000.00,531.27,1000.00,94.70,40.64,96.88,182.65,154.15,152.52,490.96,231.09,605.53,1000.00,36.49,55.68,30.62,35.70,21.40,18.08,38.61,48.23,17.96,33.78,25.56,31.86,16.40,22.84,18.78,35.36,26.99,27.03,27.50,41.10,24.01,21.96,28.26,24.41,19.37,21.28,49.80,30.27,10.66,12.25,43.65,28.65,35.98,75.89,26.88,47.37,8.62,37.29,22.31,60.45,28.59,18.53,100.00,54.22,9.86,10.63,83.68,25.20,124.05,121.47,93.76,81.23,48.30,56.78,56.15,67.16,78.24,169.91,68.80,114.34,43.30,55.89,95.85,122.56,102.36,77.96,112.11,88.70,53.74,76.44,47.93,46.64,70.83,57.70,137.88,108.52,125.92,79.97,33.71,49.84,44.58,192.99,129.64,271.09,79.00,145.01,69.40,193.09,156.78,186.59,391.22,150.93,346.72,80.75,230.85,128.81,46.74,49.53,19.18,39.71,27.84,39.56,60.18,55.76,40.46,31.10,34.48,41.23,25.69,22.04,14.65,27.90,31.79,85.75,49.45,100.00,48.27,25.91,60.07,62.46)",
	// Var, active_vars);

	GD(Base, Var, 7, 5.0, 1.0, 50.0, 16 * 1024, 16 * 1024, 3.0, 2.0, 2.0, 0.0);

	double New[1024];
	read_list(
		"(5.07,27.02,27.37,15.16,28.60,14.62,40.93,8.61,14.02,172.58,178.09,180.83,457.03,128.24,172.66,178.21,343."
		"44,1281.53,45.85)",
		New, active_vars);
	for (i = 7; i < 64; ++i)
	{
		fprintf(stdout, "\ndepth = %d/%d: \n", i, i + 1);
		match_los(New, Base, 4 * 1024, 128, i, 3.0, 3.0, 0.0, 0.0, MatchInfo, 1);
	}
#endif

	while (true) uci();
}
#else
// regression tester
struct WriteMove_
{
	int m_;
	WriteMove_(int m) : m_(m) {HI BYE}
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
	constexpr int DEPTH_LIMIT = 52;
	max_depth = Min(max_depth, DEPTH_LIMIT);
	init_search(1);
	get_board(fen);
	auto cmd = _strdup("go infinite");
	get_time_limit(cmd);
	free(cmd);
	for (int depth = Min(4, max_depth); depth <= max_depth; ++depth)
	{
		auto score = (Current->turn ? pv_search<true, true> : pv_search<false, true>)(-MateValue, MateValue, depth, FlagNeatSearch);
		cout << WriteMove_(best_move) << ":  " << score << ", " << nodes << " nodes\n";
	}
}

void main(int argc, char *argv[])
{
	int CPUInfo[4] = { -1, 0, 0, 0};
	__cpuid(CPUInfo, 1);
	HardwarePopCnt = (CPUInfo[2] >> 23) & 1;

	init();

	fprintf(stdout, "Roc (regression mode)\n");

	init_hash();

	init_pst();
	init_eval();
	Console = true;

	//Test1("kr6/p7/8/8/8/8/8/BBK5 w - - 0 1", 24, "g4-g5");

	//Test1("4kbnr/2pr1ppp/p1Q5/4p3/4P3/2Pq1b1P/PP1P1PP1/RNB2RK1 w - - 0 1", 20, "f1-e1");	// why didn't Roc keep the rook pinned?

	//Test1("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1", 20, "a7-a8");
	//Test1("4r1k1/4ppbp/r5p1/3Np3/2PnP3/3P2Pq/1R3P2/2BQ1RK1 w - - 0 1", 20, "b2-b1");
	//Test1("R7/6k1/8/8/6P1/6K1/8/7r w - - 0 1", 24, "g4-g5");

	Test1("rnb1k2r/pp2qppp/3p1n2/2pp2B1/1bP5/2N1P3/PP2NPPP/R2QKB1R w KQkq - 0 1", 20, "a2-a3");	// Roc fails until depth 9
	//Test1("r4rk1/pb3p2/1pp4p/2qn2p1/2B5/6BP/PPQ2PP1/3RR1K1 w - - 0 1", 15, "e1-e6");	// Roc fails until depth 19
	//Test1("3B4/8/2B5/1K6/8/8/3p4/3k4 w - - 0 1", 15, "b5-a6");						// Roc fails until depth 18
	//Test1("2krr3/1p4pp/p1bRpp1n/2p5/P1B1PP2/8/1PP3PP/R1K3B1 w - - 0 1", 15, "d6-c6");	// Roc fails until depth 17
	//Test1("r5k1/pp2p1bp/6p1/n1p1P3/2qP1NP1/2PQB3/P5PP/R4K2 b - - 0 1", 18, "g6-g5");	// Roc fails until depth 20
	//Test1("1br3k1/p4p2/2p1r3/3p1b2/3Bn1p1/1P2P1Pq/P3Q1BP/2R1NRK1 b - - 0 1", 20, "h3-h2");	// Roc fails until depth 21
	//Test1("8/4p3/8/3P3p/P2pK3/6P1/7b/3k4 w - - 0 1", 24, "d5-d6");					// Roc fails until depth 25
	//Test1("6k1/2b2p1p/ppP3p1/4p3/PP1B4/5PP1/7P/7K w - - 0 1", 31, "d4-b6");			// Roc fails until depth 37
	//Test1("R7/3p3p/8/3P2P1/3k4/1p5p/1P1NKP1P/7q w - - 0 1", 31, "g5-g6");				// Roc fails until depth 32
	//Test1("5r1k/1P4pp/3P1p2/4p3/1P5P/3q2P1/Q2b2K1/B3R3 w - - 0 1", 36, "a2-f7");		// Roc fails until depth 37
	//Test1("3r2k1/p2r2p1/1p1B2Pp/4PQ1P/2b1p3/P3P3/7K/8 w - - 0 1", 43, "d6-b4");			// Roc fails until depth 47
	//Test1("3r2k1/pp4B1/6pp/PP1Np2n/2Pp1p2/3P2Pq/3QPPbP/R4RK1 b - - 0 1", 42, "g3-f3");	// Slizzard fails to find f3
	//Test1("8/8/3k1p2/p2BnP2/4PN2/1P2K1p1/8/5b2 b - - 0 1", 57, "f4-d3");	// Slizzard fails to find Nd3 (finds at 58 in about 1800 seconds)
	//Test1("rq2r1k1/5pp1/p7/4bNP1/1p2P2P/5Q2/PP4K1/5R1R w - - 0 1", 4, "f5-g7");
	//Test1("b2r1rk1/2q2ppp/p1nbpn2/1p6/1P6/P1N1PN2/1B2QPPP/1BR2RK1 w - - 0 1", 4, "c3-e4");
	//Test1("8/pp3k2/2p1qp2/2P5/5P2/1R2p1rp/PP2R3/4K2Q b - - 0 1", 11, "e6-e4");
	//Test1("r1b4Q/p4k1p/1pp1ppqn/8/1nP5/8/PP1KBPPP/3R2NR w - - 0 1", 4, "d2-e1");
	//Test1("r1b2rk1/pp1p1pBp/6p1/8/2PQ4/8/PP1KBP1P/q7 w - - 0 1", 30, "d4-f6");
	//Test1("r1b3k1/ppp3pp/2qpp3/2r3N1/2R5/8/P1Q2PPP/2B3K1 b - - 0 1", 4, "g7-g6");
	//Test1("4r1k1/p1qr1p2/2pb1Bp1/1p5p/3P1n1R/3B1P2/PP3PK1/2Q4R w - - 0 1", 20, "c1-f4");
	//Test1("4k1rr/ppp5/3b1p1p/4pP1P/3pP2N/3P3P/PPP5/2KR2R1 w kq - 0 1", 16, "g1-g6");	// Roc finds at depth 9
	//Test1("6k1/p3q2p/1nr3pB/8/3Q1P2/6P1/PP5P/3R2K1 b - - 0 1", 12, "c6-d6");			// Roc finds at depth 11
	//Test1("3b2k1/1pp2rpp/r2n1p1B/p2N1q2/3Q4/6R1/PPP2PPP/4R1K1 w - - 0 1", 15, "d5-b4");	// Roc finds at depth 14
	//Test1("1k1r4/1pp4p/2n5/P6R/2R1p1r1/2P2p2/1PP2B1P/4K3 b - - 0 1", 18, "e4-e3");	// Roc finds at depth 16
	//Test1("2k5/2p3Rp/p1pb4/1p2p3/4P3/PN1P1P2/1P2KP1r/8 w - - 0 1", 25, "f3-f4");		// Roc finds at depth 21
	//Test1("r4rk1/5p2/1n4pQ/2p5/p5P1/P4N2/1qb1BP1P/R3R1K1 w - - 0 1", 23, "a1-a2");		// Roc finds at depth 22
	//Test1("2r3k1/1qr1b1p1/p2pPn2/nppPp3/8/1PP1B2P/P1BQ1P2/5KRR w - - 0 1", 25, "g1-g7");	// Roc finds at depth 23
	//Test1("3r1rk1/1p3pnp/p3pBp1/1qPpP3/1P1P2R1/P2Q3R/6PP/6K1 w - - 0 1", 24, "h3-h7");	// Roc finds at depth 23
	//Test1("2r3k1/pbr1q2p/1p2pnp1/3p4/3P1P2/1P1BR3/PB1Q2PP/5RK1 w - - 0 1", 31, "f4-f5");	// Roc finds at depth 23
	//Test1("5r1k/p1q2pp1/1pb4p/n3R1NQ/7P/3B1P2/2P3P1/7K w - - 0 1", 26, "e5-e6");		// Roc finds at depth 25

	cout << "Done\n";
	cin.ignore();
}
#endif

#pragma optimize("gy", off)
#pragma warning(push)
#pragma warning(disable: 4334)
#pragma warning(disable: 4244)
#define Say(x)	// mute TB query
// Fathom/Syzygy code at end where its #defines cannot screw us up
#if TB
#undef LOCK
#undef UNLOCK
#include "src\tbconfig.h"
#include "src\tbcore.h"
#include "src\tbprobe.c"
#pragma warning(pop)
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
