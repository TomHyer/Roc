// This code is public domain, as defined by the "CC0" Creative Commons license

//#define REGRESSION
//#define W32_BUILD
#define _CRT_SECURE_NO_WARNINGS
//#define CPU_TIMING
#define LARGE_PAGES
#define MP_NPS
//#define TIME_TO_DEPTH
#define TB 1
//#define HNI

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
#include <thread>
#include <cmath>
#include <algorithm>
#include <bitset>
#include "setjmp.h"
#include <windows.h>
#undef min
#undef max
#include <intrin.h>
#include <assert.h>


//#include "TunerParams.inc"

using namespace std;
#define INLINE __forceinline

#include "Bit.h"
#include "Immutable.h"

#define TEMPLATE_ME(f) (me ? f<1> : f<0>)
template<class T> INLINE int Sgn(const T& x)
{
	return x == 0 ? 0 : (x > 0 ? 1 : -1);
}
template<class T> INLINE const T& Min(const T& x, const T& y)
{
	return x < y ? x : y;
}
template<class T> INLINE const T& Max(const T& x, const T& y)
{
	return x < y ? y : x;
}
template<class T> INLINE T Square(const T& x)
{
	return x * x;
}
template<class T> INLINE bool Even(const T& x)
{
	return F(x & 1);
}
template<class C> INLINE bool Odd(const C& x)
{
	return T(x & 1);
}

INLINE int FileOf(int loc)
{
	return loc & 7;
}
INLINE int RankOf(int loc)
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
INLINE int Dist(int x, int y)
{
	return Max(abs(RankOf(x) - RankOf(y)), abs(FileOf(x) - FileOf(y)));
}


INLINE int From(uint16 move)
{
	return (move >> 6) & 0x3f;
}
INLINE int To(uint16 move)
{
	return move & 0x3f;
}
INLINE bool IsCastling(int piece, uint16 move)
{
	return piece >= WhiteKing && abs(To(move) - From(move)) == 2;
}

inline void SetScore(int* move, uint16 score)
{
	*move = (*move & 0xFFFF) | (score << 16);
}  // notice this operates on long-form (int) moves


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



struct GMaterial;
struct GPawnEntry;
struct GEvalInfo
{
	uint64 occ, area[2], free[2], klocus[2];
	GPawnEntry* PawnEntry;
	const GMaterial* material;
	packed_t score;
	uint32 king_att[2];
	packed_t king_att_val[2];
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
	bitset<2> pawnsForPiece, contempt;
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
	array<uint8, 256> SpanWidth, SpanGap;
	array<array<uint64, 64>, 2> BishopForward, PAtt, PMove, PWay, PCone, PSupport;
	array<uint64, 64> BMagic, BMagicMask, RMagic, RMagicMask;
	array<uint64, 64> VLine, RMask, BMask, QMask, NAtt, KAtt, KAttAtt, NAttAtt, OneIn;
	array<uint64, 64> KingFrontal, KingFlank;
	array<array<packed_t, 32>, 2> MobQueen;
	array<uint8, 256> PieceFromChar;
	array<int, 64> ROffset;
	array<array<packed_t, 18>, 2> MobBishop, MobRook;
	array<array<packed_t, 12>, 2> MobKnight;
	array<short, 64> BShift, BOffset, RShift;
	array<uint64, 16> CastleKey;
	array<array<uint64, 8>, 2> Forward;
	array<uint64, 8> EPKey;
	array<packed_t, 8> PasserGeneral, PasserBlocked, PasserFree, PasserSupported, PasserProtected, PasserConnected, PasserOutside, PasserCandidate, PasserClear;
	array<uint8, 64> UpdateCastling;
	array<array<sint16, 8>, 3> Shelter;
	array<uint8, 16> LogDist;
	array<sint16, 8> PasserAtt, PasserDef, PasserAttLog, PasserDefLog;
	array<sint16, 4> StormBlocked, StormShelterAtt, StormConnected, StormOpen, StormFree;
	uint64 TurnKey;
};
HANDLE DATA = NULL;
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
constexpr int DrawCap = 100 * CP_EVAL;
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
constexpr int AspirationEpsilon = 10;
constexpr int InitiativeConst = 3 * CP_SEARCH;
constexpr int InitiativePhase = 3 * CP_SEARCH;
constexpr int FutilityThreshold = 50 * CP_SEARCH;

#define IncV(var, x) (me ? (var -= (x)) : (var += (x)))
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
constexpr int MvvLvaVictim[16] = { 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 3, 3, 3, 3 };
constexpr int MvvLvaAttacker[16] = { 0, 0, 5, 5, 4, 4, 3, 3, 3, 3, 2, 2, 1, 1, 6, 6 };
constexpr int MvvLvaAttackerKB[16] = { 0, 0, 9, 9, 7, 7, 5, 5, 5, 5, 3, 3, 1, 1, 11, 11 };

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

#define HALT_CHECK \
    if (Current->ply >= 100) return 0; \
    for (int i = 4; i <= Current->ply; i += 2) if (Stack[sp - i] == Current->key) return 0; \
	if ((Current - Data) >= 126) {evaluate(); return Current->score; }

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
	packed_t pst;
	int material;
	uint16 move;
	uint8 turn, castle_flags, ply, ep_square, piece, capture;
} GPosData;
typedef struct
{
	array<uint64, 2> att, patt, xray;
	uint64 key, pawn_key, eval_key, passer, threat, mask;
	packed_t pst;
	int material;
	int *start, *current;
	int best;
	score_t score;
	array<uint16, N_KILLER + 1> killer;
	array<uint16, 2> ref;
	uint16 move;
	uint8 turn, castle_flags, ply, ep_square, capture, gen_flags, piece, stage;
	sint32 moves[144];
} GData;
__declspec(align(64)) GData Data[MAX_HEIGHT];
GData* Current = Data;
constexpr uint8 FlagSort = 1 << 0;
constexpr uint8 FlagNoBcSort = 1 << 1;
GData SaveData[1];

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

extern "C" unsigned tb_probe_wdl(
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
	bool     _turn);

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
static GEntry NullEntry = { 0, 1, 0, 0, 0, 0, 0, 0 };
constexpr int initial_hash_size = 1024 * 1024;
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
constexpr int PAWN_HASH_SIZE = 1 << 20;
__declspec(align(64)) array<GPawnEntry, PAWN_HASH_SIZE> PawnHash;
constexpr int PAWN_HASH_MASK = PAWN_HASH_SIZE - 1;

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
constexpr int pv_hash_size = 1 << 20;
constexpr int pv_cluster_size = 1 << 2;
constexpr int pv_hash_mask = pv_hash_size - pv_cluster_size;
GPVEntry* PVHash = nullptr;

array<int, 256> RootList;

void Prefetch1(const char* p)
{
	_mm_prefetch(p, _MM_HINT_T0);
}
template<int N> void Prefetch(const char* p)
{
	//p -= reinterpret_cast<size_t>(p) & 63;
	for (int ii = 0; ii < N; ++ii)
		Prefetch1(p + 64 * ii);
}
INLINE void Prefetch(const GMaterial* pe)
{
	Prefetch1(reinterpret_cast<const char*>(pe));
}
INLINE void Prefetch(const GEntry* pe)
{
	Prefetch<2>(reinterpret_cast<const char*>(pe));
}
INLINE void Prefetch(const GPawnEntry* pe)
{
	Prefetch<2>(reinterpret_cast<const char*>(pe));
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

uint16 date;
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
	return T(move & FlagPriority);
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

// EVAL

const sint8 DistC[8] = { 3, 2, 1, 0, 0, 1, 2, 3 };
const sint8 RankR[8] = { -3, -2, -1, 0, 1, 2, 3, 4 };

constexpr uint16 SeeValue[16] = { 0, 0, 360, 360, 1300, 1300, 1300, 1300, 1300, 1300, 2040, 2040, 3900, 3900, 30000, 30000 };
constexpr int PieceType[16] = { 0, 0, 0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 4, 4, 5, 5 };
constexpr array<int, 5> Phase = { 0, SeeValue[4], SeeValue[6], SeeValue[10], SeeValue[12] };
constexpr int PhaseMin = 2 * Phase[3] + Phase[1] + Phase[2];
constexpr int PhaseMax = 16 * Phase[0] + 3 * Phase[1] + 3 * Phase[2] + 4 * Phase[3] + 2 * Phase[4];

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
constexpr array<int, 6> MatLinear = { 21, -20, -8, 76, -15, -1 };

// T(pawn), pawn, knight, bishop, rook, queen
constexpr array<int, 21> MatQuadMe = { // tuner: type=array, var=1000, active=0
	NULL, 0, 0, 0, 0, 0,
	-33, 117, -23, -155, -247,
	15, 296, -105, -83,
	-162, 327, 315,
	-861, -1013,
	NULL
};
constexpr array<int, 15> MatQuadOpp = { // tuner: type=array, var=1000, active=0
	0, 0, 0, 0, 0,
	-114, -96, -20, -278,
	35, 39, 49,
	9, -2,
	75
};
constexpr array<int, 9> BishopPairQuad = { // tuner: type=array, var=1000, active=0
	-38, 164, 99, 246, -84, -57, -184, 88, -186
};
constexpr array<int, 6> MatClosed = { -20, 22, -33, 18, -2, 26 };

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
	constexpr array<int, 44> MatSpecial = {  // tuner: type=array, var=120, active=0
		52, 0, -52, -10,
		40, 2, -36, -10,
		32, 40, 48, 0,
		16, 20, 24, 0,
		20, 28, 36, 0,
		-12, -22, -32, 0,
		-10, 20, 64, 0,
		6, 21, 21, 0,
		0, -12, -24, 0,
		4, 8, 12, 0,
		0, 0, 0, -100 };
}
namespace Values
{
#define VALUE(name) constexpr packed_t name = Ca4(Params::MatSpecial, Params::name)
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

	constexpr Weights_ Pawn = {
		{ { -48, -275, 165, 0 },{ -460, -357, -359, 437 },{ 69, -28 } },
		{ {-85, -171, 27, 400},{ -160, -133, 93, 1079 },{ 13, -6 }},
		{ {-80, -41, -85, 782},{ 336, 303, 295, 1667 },{ -35, 13 }},
		{ {2, 13, 11, 23},{ 6, 14, 37, -88 },{ 14, -2 }} };
	constexpr Weights_ Knight = {
		{ { -134, 6, -12, -72 },{ -680, -343, -557, 1128 },{ -32, 14 } },
		{ { -315, -123, -12, -90 },{ -449, -257, -390, 777 },{ -24, -3 } },
		{ { -501, -246, -12, -107 },{ 61, -274, -357, 469 },{ -1, -16 } },
		{ { -12, -5, -2, -22 },{ 96, 69, -64, -23 },{ -5, -8 } } };
	constexpr Weights_ Bishop = {
		{ { -123, -62, 54, -116 },{ 24, -486, -350, -510 },{ 8, -58 } },
		{ { -168, -49, 24, -48 },{ -323, -289, -305, -254 },{ -7, -21 } },
		{ { -249, -33, 4, -14 },{ -529, -232, -135, 31 },{ -32, 0 } },
		{ { 4, -10, 9, -13 },{ 91, -43, -34, 29 },{ -13, -10 } } };
	constexpr Weights_ Rook = {
		{ { -260, 12, -49, 324 },{ -777, -223, 245, 670 },{ -7, -25 } },
		{ { -148, -88, -9, 165 },{ -448, -278, -63, 580 },{ -7, 0 } },
		{ { 13, -149, 14, 46 },{ -153, -225, -246, 578 },{ -6, 16 } },
		{ { 0, 8, -15, 8 },{ -32, -29, 10, -51 },{ -6, -23 } } };
	constexpr Weights_ Queen = {
		{ { -270, -18, -19, -68 },{ -520, 444, 474, -186 },{ 18, -6 } },
		{ { -114, -209, 21, -103 },{ -224, -300, 73, 529 },{ -13, 1 } },
		{ { 2, -341, 58, -160 },{ 40, -943, -171, 1328 },{ -34, 27 } },
		{ { -3, -26, 9, 5 },{ -43, -18, -107, 60 },{ 5, 12 } } };
	constexpr Weights_ King = {
		{ { -266, -694, -12, 170 },{ 1077, 3258, 20, -186 },{ -18, 3 } },
		{ { -284, -451, -31, 43 },{ 230, 1219, -425, 577 },{ -1, 5 } },
		{ { -334, -157, -67, -93 },{ -510, -701, -863, 1402 },{ 37, -8 } },
		{ { 22, 14, -16, 0 },{ 7, 70, 40,  78 } ,{ 9, -3 } } };
}

// coefficient (Linear, Log, Locus) * phase (4)
constexpr array<int, 12> MobCoeffsKnight = { 86, 46, 19, -5, 57, 36, 24, -1, 1, 10, -5, 12 };
constexpr array<int, 12> MobCoeffsBishop = { 85, 60, 48, -15, 59, 32, 25, -2, 1, 5, -1, 40 };
constexpr array<int, 12> MobCoeffsRook = { 22, 31, 44, 2, 41, 33, 28, 2, -1, 1, -1, 7 };
constexpr array<int, 12> MobCoeffsQueen = { 53, 30, 17, 2, 24, 31, 45, 1, 1, 1, -1, 10 };

constexpr int N_LOCUS = 22;

// file type (3) * distance from 2d rank/open (5)
constexpr array<int, 15> ShelterValue = {  // tuner: type=array, var=26, active=0
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

constexpr array<int, 3> PasserOffset = { -2, 2, 4 };
namespace PasserWeights
{
	constexpr array<int, 12> General = { -5,73,17, 14,19,91, 17,-9,60, -5,27,-16 };
	constexpr array<int, 12> Blocked = { -1,140,49, 30,79,175, 14,87,38, 19,57,-50 };
	constexpr array<int, 12> Free = { 29,239,-100, 67,130,403, 110,179,594, 3,1,13 };
	constexpr array<int, 12> Supported = { -2,188,17, 27,110,154, 53,65,280, -4,18,-16 };
	constexpr array<int, 12> Protected = { 69,-113,334, 53,-24,294, 37,52,177, -55,157,-176 };
	constexpr array<int, 12> Connected = { 11,134,76, 66,-28,328, 37,97,151, 107,-256,366 };
	constexpr array<int, 12> Outside = { -18,169,-14, 11,87,73, 0,75,-33, 13,-61,44 };
	constexpr array<int, 12> Candidate = { -22,75,5, 0,29,63, 27,12,38, -8,78,-26 };
	constexpr array<int, 12> Clear = { -3,-13,-12, 5,-30,13, -2,-10,-2, -1,-1,-1 };
	constexpr array<int, 12> QPGame = { 0,0,0, 0,0,0, -40,60,-20, 0,0,0 };
}

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
	26, 52, 0
};
namespace Values
{
	constexpr packed_t PasserOpRookBlock = 
			Pack4(0, Av(PasserSpecial, 3, ::PasserOpRookBlock, 0), Av(PasserSpecial, 3, ::PasserOpRookBlock, 1), Av(PasserSpecial, 3, ::PasserOpRookBlock, 2));
}

namespace Params
{
	enum
	{
		IsolatedOpen,
		IsolatedDoubledOpen,
		IsolatedClosed,
		IsolatedDoubledClosed,
		IsolatedBlocked,
		IsolatedForPiece,
		JoinedForPiece
	};
	constexpr array<int, 28> Isolated = {
		16, 13, 24, -57,
		1, 17, 46, -37,
		21, 16, 5, 1,
		33, 27, 23, -14,
		-24, -8, -8, -16,
		40, 40, 40, 10,
		5, 20, 40, 0
	};
}
namespace Values
{
#define VALUE(name) constexpr packed_t name = Ca4(Params::Isolated, Params::name)
	VALUE(IsolatedOpen);
	VALUE(IsolatedDoubledOpen);
	VALUE(IsolatedClosed);
	VALUE(IsolatedDoubledClosed);
	VALUE(IsolatedBlocked);
	VALUE(IsolatedForPiece);
	VALUE(JoinedForPiece);
#undef VALUE
}

namespace Params
{
	enum
	{
		DoubledOpen,
		DoubledClosed
	};
	constexpr array<int, 8> Doubled = {  // tuner: type=array, var=26, active=0
		63, 22, 18, -68,
		17, -5, 10, 12 };
}
namespace Values
{
#define VALUE(name) constexpr packed_t name = Ca4(Params::Doubled, Params::name)
	VALUE(DoubledOpen);
	VALUE(DoubledClosed);
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
	constexpr array<int, 12> Unprotected = {  // tuner: type=array, var=26, active=0
		20, 50, 19, -19,
		-9, 0, -30, 23,
		-5, -11, 10, -1 };
}
namespace Values
{
#define VALUE(name) constexpr packed_t name = Ca4(Params::Unprotected, Params::name)
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
	constexpr array<int, 8> Backward = {  // tuner: type=array, var=26, active=0
		68, 54, 40, 0,
		16, 10, 4, 0 };
}
namespace Values
{
#define VALUE(name) constexpr packed_t name = Ca4(Params::Backward, Params::name)
	VALUE(BackwardOpen);
	VALUE(BackwardClosed);
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
	constexpr array<int, 40> RookSpecial = {  // tuner: type=array, var=26, active=0
		32, 16, 0, 0,
		8, 4, 0, 0,
		44, 38, 32, 0,
		-4, 2, 8, 0,
		-4, -4, -4, 0,
		56, 26, -4, 0,
		20, 0, -20, 0,
		0, 30, 10, 0,
		0, 28, 32, 0,
		0, 76, 124, 0 };
}
namespace Values
{
#define VALUE(name) constexpr packed_t name = Ca4(Params::RookSpecial, Params::name)
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
	constexpr array<int, 32> Tactical = {  // tuner: type=array, var=51, active=0
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
#define VALUE(name) constexpr packed_t name = Ca4(Params::Tactical, Params::name)
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
	constexpr array<int, 12> KingDefence = {  // tuner: type=array, var=13, active=0
		8, 4, 0, 0,
		0, 2, 4, 0,
		16, 8, 0, 0 };
}
namespace Values
{
#define VALUE(name) constexpr packed_t name = Ca4(Params::KingDefence, Params::name)
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
		PawnFileSpan,
		PawnConnected
	};
	constexpr array<int, 20> PawnSpecial = {  // tuner: type=array, var=26, active=0
		44, 40, 36, 0,
		36, 26, 16, 0,
		0, 18, 36, 0,
		4, 4, 4, 0,
		15, 9, -2, 4
	};
}
namespace Values
{
#define VALUE(name) constexpr packed_t name = Ca4(Params::PawnSpecial, Params::name)
	VALUE(PawnChainLinear);
	VALUE(PawnChain);
	VALUE(PawnBlocked);
	VALUE(PawnFileSpan);
	VALUE(PawnConnected);
#undef VALUE
}

constexpr array<uint64, 2> BOutpost = { 0x00003C7E7E000000ull, 0x0000007E7E3C0000ull };
namespace Params
{
	enum 
	{ 
		BishopPawnBlock, 
		BishopOutpostInsideOut,
		BishopOutpostNoMinor
	};
	constexpr array<int, 12> BishopSpecial = { // tuner: type=array, var=20, active=0
		-1, 12, 12, 12,
		100, 33, 1, -25,
		130, 44, 1, -25
	};
}
namespace Values
{
#define VALUE(name) constexpr packed_t name = Ca4(Params::BishopSpecial, Params::name)
	VALUE(BishopPawnBlock);
	VALUE(BishopOutpostInsideOut);
	VALUE(BishopOutpostNoMinor);
#undef VALUE
}

constexpr array<uint64, 2> NOutpost = { 0x00187E7E3C000000ull, 0x0000003C7E7E1800ull };
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
	constexpr array<int, 24> KnightSpecial = {  // tuner: type=array, var=26, active=0
		42, 30, 5, -21,
		61, 43, 4, -29,
		28, 15, 28, 3,
		51, 13, 13, 82,
		0, 4, 15, -10,
		0, 2, 5, 0
	};
}
namespace Values
{
#define VALUE(name) constexpr packed_t name = Ca4(Params::KnightSpecial, Params::name)
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
	constexpr array<int, 20> Pin = {  // tuner: type=array, var=51, active=0
		84, 120, 156, 0,
		24, 172, 320, 0,
		180, 148, 116, 0,
		32, 34, 36, 0,
		245, 191, 138, 0 };
}
namespace Values
{
#define VALUE(name) constexpr packed_t name = Ca4(Params::Pin, Params::name)
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
	constexpr array<int, 12> KingRay = {  // tuner: type=array, var=51, active=0
		17, 26, 33, -2,
		0, 15, 42, 0,
		46, 8, 0, -1 };
}
namespace Values
{
#define VALUE(name) constexpr packed_t name = Ca4(Params::KingRay, Params::name)
	VALUE(QKingRay);
	VALUE(RKingRay);
	VALUE(BKingRay);
#undef VALUE
}

constexpr array<int, 2> PushW = { 7, -9 };
constexpr array<int, 2> Push = { 8, -8 };
constexpr array<int, 2> PushE = { 9, -7 };

constexpr array<int, 12> KingAttackWeight = {  // tuner: type=array, var=51, active=0
	56, 88, 44, 64, 60, 104, 116, 212, 16, 192, 256, 64 };
constexpr uint32 KingNAttack1 = UPack(1, KingAttackWeight[0]);
constexpr uint32 KingNAttack = UPack(2, KingAttackWeight[1]);
constexpr uint32 KingBAttack1 = UPack(1, KingAttackWeight[2]);
constexpr uint32 KingBAttack = UPack(2, KingAttackWeight[3]);
constexpr uint32 KingRAttack1 = UPack(1, KingAttackWeight[4]);
constexpr uint32 KingRAttack = UPack(2, KingAttackWeight[5]);
constexpr uint32 KingQAttack1 = UPack(1, KingAttackWeight[6]);
constexpr uint32 KingQAttack = UPack(2, KingAttackWeight[7]);
constexpr uint32 KingAttack = UPack(1, 0);
constexpr uint32 KingPAttackInc = UPack(0, KingAttackWeight[8]);
constexpr uint32 KingAttackSquare = KingAttackWeight[9];
constexpr uint32 KingNoMoves = KingAttackWeight[10];
constexpr uint32 KingShelterQuad = KingAttackWeight[11];	// a scale factor, not a score amount

template<int N> array<uint16, N> CoerceUnsigned(const array<int, N>& src)
{
	array<uint16, N> retval;
	for (int ii = 0; ii < N; ++ii)
		retval[ii] = static_cast<uint16>(max(0, src[ii]));
	return retval;
}

// tuner: stop

// END EVAL WEIGHTS

// SMP

#ifdef DEBUG
constexpr int MaxPrN = 1;
#else
#ifdef W32_BUILD
constexpr int MaxPrN = 32;  // mustn't exceed 32
#else
constexpr int MaxPrN = 64;  // mustn't exceed 64
#endif
#endif

int PrN = 1, CPUs = 1, parent = 1, child = 0, WinParId, Id = 0, ResetHash = 1, NewPrN = 0;
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

constexpr int SharedPVHashOffset = sizeof(GSMPI);

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
		&& F((move)& 0xF000);
};

inline uint64 XBMagicAttacks(const CommonData_* data, int i, uint64 occ)
{
	uint64 att = 0;
	for (uint64 u = data->BMask[i]; T(u); Cut(u))
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
	for (uint64 u = data->RMask[i]; T(u); Cut(u))
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

void CheckMaterialIndex(int theo)
{
	theo &= ~FlagUnusualMaterial;
	for (int ii = 0; ii < 64; ++ii)
		theo -= MatCode[PieceAt(ii)];
	assert(!theo);
}

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

void init_misc(CommonData_* data)
{
	array<uint64, 64> HLine;
	array<uint64, 64> NDiag;
	array<uint64, 64> SDiag;

	for (int i = 0; i < 64; ++i)
	{
		HLine[i] = data->VLine[i] = NDiag[i] = SDiag[i] = data->RMask[i] = data->BMask[i] = data->QMask[i] = 0;
		data->BMagicMask[i] = data->RMagicMask[i] = data->NAtt[i] = data->KAtt[i] = data->KAttAtt[i] = data->NAttAtt[i] = data->OneIn[i] = 0;
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
			if (FileOf(i) == FileOf(j))
				data->VLine[i] |= u;
			if (RankOf(i) == RankOf(j))
				HLine[i] |= u;  // thus HLine[i] = Rank[RankOf(i)] ^ Bit(i)
			if (NDiagOf(i) == NDiagOf(j))
				NDiag[i] |= u;
			if (SDiagOf(i) == SDiagOf(j))
				SDiag[i] |= u;
			if (Dist(i, j) <= 2)
			{
				data->KAttAtt[i] |= u;
				if (Dist(i, j) <= 1)
					data->KAtt[i] |= u;
				if (abs(RankOf(i) - RankOf(j)) + abs(FileOf(i) - FileOf(j)) == 3)
					data->NAtt[i] |= u;
			}
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
		data->RMask[i] = HLine[i] | data->VLine[i];
		data->BMask[i] = NDiag[i] | SDiag[i];
		data->QMask[i] = data->RMask[i] | data->BMask[i];
		data->BMagicMask[i] = data->BMask[i] & Interior;
		data->RMagicMask[i] = data->RMask[i];
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
	for (int i = 0; i < 64; ++i)
		for (int j = 0; j < 64; ++j)
			if (data->NAtt[i] & data->NAtt[j])
				data->NAttAtt[i] |= Bit(j);

	for (int i = 0; i < 8; ++i)
	{
		data->Forward[0][i] = data->Forward[1][i] = 0;
		for (int j = 0; j < 8; ++j)
		{
			if (i < j)
				data->Forward[0][i] |= Line[j];
			else if (i > j)
				data->Forward[1][i] |= Line[j];
		}
	}
	for (int i = 0; i < 64; ++i)
	{
		for (uint64 u = data->QMask[i]; T(u); Cut(u))
		{
			int j = lsb(u);
			int k = Sgn(RankOf(j) - RankOf(i));
			int l = Sgn(FileOf(j) - FileOf(i));
			for (int n = i + 8 * k + l; n != j; n += (8 * k + l))
				data->Between[i][j] |= Bit(n);
		}
		for (uint64 u = data->BMask[i]; T(u); Cut(u))
		{
			int j = lsb(u);
			data->FullLine[i][j] = data->BMask[i] & data->BMask[j];
		}
		for (uint64 u = data->RMask[i]; T(u); Cut(u))
		{
			int j = lsb(u);
			data->FullLine[i][j] = data->RMask[i] & data->RMask[j];
		}
		data->BishopForward[0][i] |= data->PWay[0][i];
		data->BishopForward[1][i] |= data->PWay[1][i];
		for (int j = 0; j < 64; j++)
		{
			if ((data->PWay[1][j] | Bit(j)) & data->BMask[i] & data->Forward[0][RankOf(i)])
				data->BishopForward[0][i] |= Bit(j);
			if ((data->PWay[0][j] | Bit(j)) & data->BMask[i] & data->Forward[1][RankOf(i)])
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
		uint8& gap = data->SpanGap[i] = 0;
		for (int j = 0, last = 9; j < 8; ++j)
		{
			if (i & Bit(j))
				last = j;
			else if (j > last)
				gap = max<uint8>(gap, j - 1 - last);
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
						else if (F(data->KAtt[wk] & bwp))
							goto set_draw;
					}
					un = 0;
					if (turn == Black)
					{
						u = data->KAtt[bk] & (~(data->KAtt[wk] | data->PAtt[White][wp]));
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
						for (u = data->KAtt[wk] & (~(data->KAtt[bk] | bwp)); T(u); Cut(u))
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
								if (F(data->KAtt[to] & bbk))
									goto set_win;
								if (data->KAtt[to] & bwk)
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
	const size_t n = (*mob)[1].size() - 1;
	const double c1 = n * log(n) - (n - 1) * log(n - 1);
	const double c2 = 1.0 - 1.0 / c1;
	// ordering of coeffs is (d(first)*4, d(last)*4, locus*4)
	auto m1 = [&](int phase, int pop)->sint16
	{
		double d1 = coeffs[phase], d2 = coeffs[phase + 4], dLocus = coeffs[phase + 8];
		double p = pow(1 + log(d2 / d1) / (c1 * c2), c2);	// reproduces the desired ratio
		double val = d1 * pow(pop, p) - N_LOCUS * pop * dLocus / 64;
		return static_cast<sint16>(0.4 * val);	// coeffs are in millipawns
	};
	auto m2 = [&](int pop)->sint64
	{
		return Pack4(m1(0, pop), m1(1, pop), m1(2, pop), m1(3, pop));
	};
	auto l1 = [&](int phase, int pop)->sint16
	{
		return static_cast<sint16>(0.4 * pop * coeffs[phase + 8]);	// coeffs are in millipawns
	};
	auto l2 = [&](int pop)->sint64
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

double passer_basis(int excess)
{
	return sqrt(Square(excess) + 0.5) + excess - 0.5;
}
double passer_basis(int rank, int offset)
{
	return passer_basis(rank - offset) / passer_basis(6 - offset);
}
sint16 passer_val(const int* weight, int rank)
{
	double retval = *weight * passer_basis(rank, PasserOffset[0])
		+ *(weight + 1) * passer_basis(rank, PasserOffset[1])
		+ *(weight + 2) * passer_basis(rank, PasserOffset[2]);
	return static_cast<sint16>(retval);
}
array<sint64, 8> init_passer(const array<int, 12>& weights)
{
	array<sint64, 8> retval = { 0ll,0ll,0ll,0ll,0ll,0ll,0ll,0ll };
	auto phased = [&](int which, int rank) { return passer_val(&weights[3 * which], rank); };
	for (int rank = 1; rank < 7; ++rank)
		retval[rank] = Pack4(phased(0, rank), phased(1, rank), phased(2, rank), phased(3, rank));
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
		data->KingFrontal[i] = make_klocus(i, 1.0, -0.25);
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

	data->PasserGeneral = init_passer(PasserWeights::General);
	data->PasserBlocked = init_passer(PasserWeights::Blocked);
	data->PasserFree = init_passer(PasserWeights::Free);
	data->PasserSupported = init_passer(PasserWeights::Supported);
	data->PasserProtected = init_passer(PasserWeights::Protected);
	data->PasserConnected = init_passer(PasserWeights::Connected);
	data->PasserOutside = init_passer(PasserWeights::Outside);
	data->PasserCandidate = init_passer(PasserWeights::Candidate);
	data->PasserClear = init_passer(PasserWeights::Clear);
	//data->PasserQPGame = init_passer(PasserWeights::QPGame);

	for (int i = 0; i < 8; ++i)
	{
		int im2 = Max(i - 2, 0);
		auto attdef = [&](int k) { return PasserAttDefQuad[k] * im2*im2 + PasserAttDefLinear[k] * im2 + PasserAttDefConst[k]; };
		data->PasserAtt[i] = attdef(0);
		data->PasserDef[i] = attdef(2);
		data->PasserAttLog[i] = attdef(1);
		data->PasserDefLog[i] = attdef(3);
	}
}

// all these special-purpose endgame evaluators

template<bool me> int krbkrx()
{
	if (King(opp) & Interior)
		return 1;
	return 16;
}

template<bool me> int kpkx()
{
	uint64 u = me == White ? RO->Kpk[Current->turn][lsb(Pawn(White))][lsb(King(White))] & Bit(lsb(King(Black)))
		: RO->Kpk[Current->turn ^ 1][63 - lsb(Pawn(Black))][63 - lsb(King(Black))] & Bit(63 - lsb(King(White)));
	return T(u) ? 32 : T(Piece(opp) ^ King(opp));
}

template<bool me> int knpkx()
{
	if (Pawn(me) & OwnLine(me, 6) & (File[0] | File[7]))
	{
		int sq = lsb(Pawn(me));
		if (RO->KAtt[sq] & King(opp) & (OwnLine(me, 6) | OwnLine(me, 7)))
			return 0;
		if (PieceAt(sq + Push[me]) == IKing[me] && (RO->KAtt[lsb(King(me))] & RO->KAtt[lsb(King(opp))] & OwnLine(me, 7)))
			return 0;
	}
	else if (Pawn(me) & OwnLine(me, 5) & (File[0] | File[7]))
	{
		int sq = lsb(Pawn(me)), to = sq + Push[me];
		if (PieceAt(sq + Push[me]) == IPawn[opp])
		{
			if (RO->KAtt[to] & King(opp) & OwnLine(me, 7))
				return 0;
			if ((RO->KAtt[to] & RO->KAtt[lsb(King(opp))] & OwnLine(me, 7)) && (!(RO->NAtt[to] & Knight(me)) || Current->turn == opp))
				return 0;
		}
	}
	return 32;
}

template<bool me> int krpkrx()
{
	int mul = 32;
	int sq = lsb(Pawn(me));
	int rrank = OwnRank<me>(sq);
	int o_king = lsb(King(opp));
	int o_rook = lsb(Rook(opp));
	int m_king = lsb(King(me));
	int add_mat = T(Piece(opp) ^ King(opp) ^ Rook(opp));
	int clear = F(add_mat) || F((RO->PWay[opp][sq] | PIsolated[FileOf(sq)]) & RO->Forward[opp][RankOf(sq + Push[me])] & (Piece(opp) ^ King(opp) ^ Rook(opp)));

	if (!clear)
		return 32;
	if (!add_mat && !(Pawn(me) & (File[0] | File[7])))
	{
		int m_rook = lsb(Rook(me));
		if (OwnRank<me>(o_king) < OwnRank<me>(m_rook) && OwnRank<me>(m_rook) < rrank && OwnRank<me>(m_king) >= rrank - 1 &&
			OwnRank<me>(m_king) > OwnRank<me>(m_rook) &&
			((RO->KAtt[m_king] & Pawn(me)) || (MY_TURN && abs(FileOf(sq) - FileOf(m_king)) <= 1 && abs(rrank - OwnRank<me>(m_king)) <= 2)))
			return 127;
		if (RO->KAtt[m_king] & Pawn(me))
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
			(!MY_TURN || F(RO->PAtt[me][sq] & RookAttacks(lsb(Rook(me)), PieceAll()) & (~RO->KAtt[o_king]))))
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

	if (RO->KAtt[o_king] & RO->PWay[me][sq] & OwnLine(me, 7))
	{
		if (rrank <= 4 && OwnRank<me>(m_king) <= 4 && OwnRank<me>(o_rook) == 5)
			mul = Min(mul, add_mat << 3);
		if (rrank == 5 && OwnRank<me>(o_rook) <= 1 && !MY_TURN || (F(RO->KAtt[m_king] & RO->PAtt[me][sq] & (~RO->KAtt[o_king])) && (IsCheck(me) || Dist(m_king, sq) >= 2)))
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

template<bool me> int krpkbx()
{
	if (!(Pawn(me) & OwnLine(me, 5)))
		return 32;
	int sq = lsb(Pawn(me));
	if (!(RO->PWay[me][sq] & King(opp)))
		return 32;
	int diag_sq = NB<me>(RO->BMask[sq + Push[me]]);
	if (OwnRank<me>(diag_sq) > 1)
		return 32;
	uint64 mdiag = RO->FullLine[sq + Push[me]][diag_sq] | Bit(sq + Push[me]) | Bit(diag_sq);
	int check_sq = NB<me>(RO->BMask[sq - Push[me]]);
	uint64 cdiag = RO->FullLine[sq - Push[me]][check_sq] | Bit(sq - Push[me]) | Bit(check_sq);
	if ((mdiag | cdiag) & (Piece(opp) ^ King(opp) ^ Bishop(opp)))
		return 32;
	if (cdiag & Bishop(opp))
		return 0;
	if ((mdiag & Bishop(opp)) && (Current->turn == opp || !(King(me) & RO->PAtt[opp][sq + Push[me]])))
		return 0;
	return 32;
}

template<bool me> int kqkp()
{
	if (F(RO->KAtt[lsb(King(opp))] & Pawn(opp) & OwnLine(me, 1) & (File[0] | File[2] | File[5] | File[7])))
		return 32;
	if (RO->PWay[opp][lsb(Pawn(opp))] & (King(me) | Queen(me)))
		return 32;
	if (Pawn(opp) & (File[0] | File[7]))
		return 1;
	else
		return 4;
}

template<bool me> int kqkrpx()
{
	int rsq = lsb(Rook(opp));
	uint64 pawns = RO->KAtt[lsb(King(opp))] & RO->PAtt[me][rsq] & Pawn(opp) & Interior & OwnLine(me, 6);
	if (pawns && OwnRank<me>(lsb(King(me))) <= 4)
		return 0;
	return 32;
}

template<bool me> int krkpx()
{
	if (T(RO->KAtt[lsb(King(opp))] & Pawn(opp) & OwnLine(me, 1)) & F(RO->PWay[opp][NB<me>(Pawn(opp))] & King(me)))
		return 0;
	return 32;
}

template<bool me> int krppkrpx()
{
	if (uint64 mine = Current->passer & Pawn(me))
	{
		if (Single(mine))
		{
			int sq = lsb(mine);
			if (RO->PWay[me][sq] & King(opp) & (File[0] | File[1] | File[6] | File[7]))
			{
				int opp_king = lsb(King(opp));
				if (RO->KAtt[opp_king] & Pawn(opp))
				{
					int king_file = FileOf(opp_king);
					if (!((~(File[king_file] | PIsolated[king_file])) & Pawn(me)))
						return 1;
				}
			}
		}
		if (mine & 0x8e8e8e8e8e8e8e8e)
			return 32;
	}
	if (F((~(RO->PWay[opp][lsb(King(opp))] | RO->PSupport[me][lsb(King(opp))])) & Pawn(me)))
		return 0;
	return 16;
}

template<bool me> int krpppkrppx()
{
	if (T(Current->passer & Pawn(me)) || F((RO->KAtt[lsb(Pawn(opp))] | RO->KAtt[msb(Pawn(opp))]) & Pawn(opp)))
		return 32;
	if (F((~(RO->PWay[opp][lsb(King(opp))] | RO->PSupport[me][lsb(King(opp))])) & Pawn(me)))
		return 0;
	return 32;
}

template<bool me> int kbpkbx()
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

template<bool me> int kbpknx()
{
	uint64 u;
	if (T(RO->PWay[me][lsb(Pawn(me))] & King(opp)) && T(King(opp) & LightArea) != T(Bishop(me) & LightArea))
		return 0;
	if (Current->turn == opp)
		for (u = Knight(opp); T(u); Cut(u))
			if (RO->NAtt[lsb(u)] & Pawn(me))
				return 0;
	return 32;
}

template<bool me> int kbppkbx()
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
	else if (PIsolated[FileOf(sq1)] & Pawn(me))
	{
		if (T(King(opp) & LightArea) != T(Bishop(me) & LightArea))
		{
			if (HasBit(RO->KAtt[o_king] | King(opp), sq2 + Push[me]) && HasBit(BishopAttacks(o_bishop, PieceAll()), sq2 + Push[me]) &&
				HasBit(RO->KAtt[o_king] | King(opp), (sq2 & 0xF8) | FileOf(sq1)) && HasBit(BishopAttacks(o_bishop, PieceAll()), (sq2 & 0xFFFFFFF8) | FileOf(sq1)))
				return 0;
		}
	}
	return 32;
}

template<bool me> int krppkrx()
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
	else if (T(PIsolated[FileOf(sq2)] & Pawn(me)) && T((File[0] | File[7]) & Pawn(me)) && T(King(opp) & Shift<me>(Pawn(me))))
	{
		if (OwnRank<me>(sq2) == 5 && OwnRank<me>(sq1) == 4 && T(Rook(opp) & (OwnLine(me, 5) | OwnLine(me, 6))))
			return 10;
		else if (OwnRank<me>(sq2) < 5)
			return 16;
	}
	int r2 = lsb(Rook(opp)), rf = FileOf(r2);
	const uint64 mask = West[rf] & King(me) ? West[rf] : East[rf];
	return mask & (Rook(me) | Pawn(me)) ? 32 : 16;
}


template<bool me> bool eval_stalemate(GEvalInfo& EI)
{
	bool retval = (F(NonPawnKing(opp)) && Current->turn == opp && F(Current->att[me] & King(opp)) && F(RO->KAtt[EI.king[opp]] & (~(Current->att[me] | Piece(opp)))) &&
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
		if ((RO->KAtt[sq] | Bit(sq)) & King(opp))
			EI.mul = 0;
		else if ((RO->KAtt[sq] & RO->KAtt[lsb(King(opp))] & OwnLine(me, 7)) && PieceAt(sq - Push[me]) == IPawn[opp] && PieceAt(sq - 2 * Push[me]) == IPawn[me])
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
	if (F(Current->passer))
	{
		int blocks = pop(FileOcc((Pawn(me) | PAtts<opp>(Pawn(opp))) & 0x0000FFFFFFFF0000));
		if (EI.mul <= 32)
			EI.mul = min(EI.mul, 43 - 3 * blocks);
	}
}

template<bool me> void eval_single_bishop(GEvalInfo& EI, pop_func_t pop)
{
	int number = pop(Pawn(me));
	int sq = Piece(ILight[me]) ? (me ? 0 : 63) : (me ? 7 : 56);
	if (F(Pawn(me) & (~RO->PWay[opp][sq])))
	{
		if ((RO->KAtt[sq] | Bit(sq)) & King(opp))
			EI.mul = 0;
		else if ((RO->KAtt[sq] & RO->KAtt[lsb(King(opp))] & OwnLine(me, 7)) && PieceAt(sq - Push[me]) == IPawn[opp] && PieceAt(sq - 2 * Push[me]) == IPawn[me])
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
		if ((RO->KAtt[EI.king[opp]] | Current->patt[opp]) & Bishop(opp))
		{
			uint64 push = Shift<me>(Pawn(me));
			if (!(push & (~(Piece(opp) | Current->att[opp]))) && (King(opp) & (Board->bb[ILight[opp]] ? LightArea : DarkArea)))
			{
				EI.mul = Min(EI.mul, 8);
				int bsq = lsb(Bishop(opp));
				uint64 att = BishopAttacks(bsq, EI.occ);
				uint64 prp = (att | RO->KAtt[EI.king[opp]]) & Pawn(opp) & (Board->bb[ILight[opp]] ? LightArea : DarkArea);
				uint64 patt = ShiftW<opp>(prp) | ShiftE<opp>(prp);
				if ((RO->KAtt[EI.king[opp]] | patt) & Bishop(opp))
				{
					uint64 double_att = (RO->KAtt[EI.king[opp]] & patt) | (patt & att) | (RO->KAtt[EI.king[opp]] & att);
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
			EI.mul = Min(EI.mul, T(King(me) & AB & ~back) ? 24 : 8);
	}
	if (F(Pawn(me) & ~GH) && T(King(opp) & FGH))
	{
		uint64 back = RO->Forward[opp][RankOf(lsb(King(opp)))];
		if (T(back & Pawn(me)))
			EI.mul = Min(EI.mul, T(King(me) & GH & ~back) ? 24 : 8);
	}
}

template<bool me> inline void check_forced_stalemate(int* mul, int kloc)
{
	if (F(RO->KAtt[kloc] & ~Current->att[me])
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
//	if (T(Minor(opp)))
//		EI.mul = Min(EI.mul, RO->OneIn[lsb(King(opp))] & Rook(me) ? 6 : 0);
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

	int phase = Phase[PieceType[WhitePawn]] * (pawns[White] + pawns[Black])
		+ Phase[PieceType[WhiteKnight]] * (knights[White] + knights[Black])
		+ Phase[PieceType[WhiteLight]] * (bishops[White] + bishops[Black])
		+ Phase[PieceType[WhiteRook]] * (rooks[White] + rooks[Black])
		+ Phase[PieceType[WhiteQueen]] * (queens[White] + queens[Black]);
	material.phase = Min((Max(phase - PhaseMin, 0) * MAX_PHASE) / (PhaseMax - PhaseMin), MAX_PHASE);
	material.contempt[White] = material.contempt[Black] = true;

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
			if (pawns[me] > pawns[opp] && 2 * rooks[me] + minor[me] == 2 * rooks[opp] + minor[opp] + 1 && rooks[opp] + minor[opp] > 1)
				material.contempt[me] = false;	// don't avoid trading down to a won position

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
	material.closed = Closed(special) + closed[White] - closed[Black]; // *mat[score > 0 ? White : Black]) / 32;

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
				if (tot[opp] == 0 && pawns[opp] == 1)
					material.eval[me] = TEMPLATE_ME(eval_kqkpx);
				else if (rooks[opp] == 1)
					material.eval[me] = TEMPLATE_ME(eval_kqkrpx);
			}
		}
		else if (major[opp] + minor[opp] == 0)
			material.eval[me] = eval_null;	// just force the stalemate check

		material.pawnsForPiece[me] = tot[me] < tot[opp] && major[me] + minor[me] < major[opp] + minor[opp] &&  pawns[me] > pawns[opp];
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

void init_hash()
{
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

void init_data()
{
	if (parent && DATA != NULL)
	{
		UnmapViewOfFile(RO);
		CloseHandle(DATA);
	}
	char name[256];
	sprintf_s(name, "ROC_DATA_%d", WinParId);
	HANDLE RW = NULL;
	DWORD size = sizeof(CommonData_);
	if (parent)
	{
		RW = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, size, name);
		auto data = reinterpret_cast<CommonData_*>(MapViewOfFile(RW, FILE_MAP_ALL_ACCESS, 0, 0, size));
		init_data(data);
		UnmapViewOfFile(data);
	}
	DATA = OpenFileMapping(FILE_MAP_READ, 0, name);
	RO = reinterpret_cast<CommonData_*>(MapViewOfFile(DATA, FILE_MAP_READ, 0, 0, size));
	if (parent)
	{
		CloseHandle(RW);
	}
}

void init_shared()
{
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
	PVHash = (GPVEntry*)(((char*)Smpi) + SharedPVHashOffset);
	if (parent)
		memset(PVHash, 0, pv_hash_size * sizeof(GPVEntry));
}

void init()
{
	init_shared();
	init_data();
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
	fill(Current->killer.begin() + 1, Current->killer.end(), 0);
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
}

void init_search(int clear_hash)
{
	for (int ip = 0; ip < 16; ++ip)
		for (int it = 0; it < 64; ++it)
			fill(HistoryVals[ip][it].begin(), HistoryVals[ip][it].end(), (1 << 8) | 2);	

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
				Next->key ^= pKey[to] ^ RO->PieceKey[piece][to];
				Next->pawn_key ^= pKey[from];
			}
			else
				Next->pawn_key ^= pKey[from] ^ pKey[to];

			PawnEntry = &PawnHash[Next->pawn_key & PAWN_HASH_MASK];
			Prefetch(PawnEntry);
		}
		else if (piece >= WhiteKing)
		{
			Next->pawn_key ^= pKey[from] ^ pKey[to];
			PawnEntry = &PawnHash[Next->pawn_key & PAWN_HASH_MASK];
			Prefetch(PawnEntry);
		}
		else if (capture < WhiteKnight)
		{
			PawnEntry = &PawnHash[Next->pawn_key & PAWN_HASH_MASK];
			Prefetch(PawnEntry);
		}

		if (Current->castle_flags && (piece >= WhiteRook || capture >= WhiteRook))
		{
			Next->castle_flags &= RO->UpdateCastling[to] & RO->UpdateCastling[from];
			Next->key ^= RO->CastleKey[Current->castle_flags] ^ RO->CastleKey[Next->castle_flags];
			Next->pawn_key ^= RO->CastleKey[Current->castle_flags] ^ RO->CastleKey[Next->castle_flags];
		}
		if (F(Next->material & FlagUnusualMaterial))
			Prefetch(&RO->Material[Next->material]);
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
			Prefetch(PawnEntry);
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
					Prefetch(PawnEntry);
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
	Entry = Hash + (High32(Next->key) & hash_mask);
	Prefetch(Entry);
	++sp;
	Stack[sp] = Next->key;
	Next->move = move;
	Next->gen_flags = 0;
	++Current;
	++nodes;
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
	Entry = Hash + (High32(Next->key) & hash_mask);
	Prefetch(Entry);
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
	++nodes;
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

template<bool me, class POP> INLINE void eval_pawns(GPawnEntry* PawnEntry, GPawnEvalInfo& PEI, bool pawns_for_piece)
{
	constexpr array<array<uint64, 2>, 2> RFileBlockMask = { array<uint64, 2>({ 0x0202000000000000, 0x8080000000000000 }), array<uint64, 2>({ 0x0202, 0x8080 }) };
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
				if (uint64 u = (PIsolated[FileOf(oppP)] & RO->Forward[opp][RankOf(oppP)] & Pawn(me)))
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

		int isolated = !(Pawn(me) & PIsolated[file]);
		int doubled = T(Pawn(me) & (File[file] ^ b));
		int open = !(PawnAll() & way);

		if (isolated)
		{
			DecV(PEI.score, open ? Values::IsolatedOpen : Values::IsolatedClosed);
			if (F(open) && next == IPawn[opp])
				DecV(PEI.score, Values::IsolatedBlocked);
			if (doubled)
				DecV(PEI.score, open ? Values::IsolatedDoubledOpen : Values::IsolatedDoubledClosed);
			if (pawns_for_piece)
				DecV(PEI.score, Values::IsolatedForPiece);
		}
		else
		{
			if (doubled)
				DecV(PEI.score, open ? Values::DoubledOpen : Values::DoubledClosed);
			if (rrank >= 3 && (b & (File[2] | File[3] | File[4] | File[5])) && next != IPawn[opp] && (PIsolated[file] & Line[rank] & Pawn(me)))
				IncV(PEI.score, Values::PawnChainLinear * (rrank - 3) + Values::PawnChain);
			if (pawns_for_piece)
				IncV(PEI.score, Values::JoinedForPiece);
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
			if (open && (F(Pawn(opp) & PIsolated[file]) || pop(Pawn(me) & PIsolated[file]) >= pop(Pawn(opp) & PIsolated[file])))
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
					DecV(PEI.score, Values::PasserTarget * (3 - rrank));
				}	// Gull 3 was thinking of removing this term, because fitted weight is negative

				for (uint64 v = RO->PAtt[me][sq] & Pawn(me); v; Cut(v)) if ((RO->PSupport[me][lsb(v)] & Pawn(me)) == b)
				{
					DecV(PEI.score, Values::ChainRoot);
					break;
				}
			}
		}
		if (open && F(PIsolated[file] & RO->Forward[me][rank] & Pawn(opp)))
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
			if (F(Pawn(opp) & West[file]) || F(Pawn(opp) & East[file]))
				IncV(PEI.score, RO->PasserOutside[rrank]);
		}
	}

	uint8 files = FileOcc(Pawn(me));
	int file_span = RO->SpanWidth[files];
	IncV(PEI.score, Values::PawnFileSpan * file_span);
	IncV(PEI.score, Values::PawnConnected* pop(Pawn(me)& ((Pawn(me) & 0x7f7f7f7f7f7f7f7f) << 1)));
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

	eval_pawns<White, POP>(PawnEntry, PEI, EI.material && EI.material->pawnsForPiece[White]);
	eval_pawns<Black, POP>(PawnEntry, PEI, EI.material && EI.material->pawnsForPiece[Black]);

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
		if (RO->QMask[sq] & King(opp))
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
					else if (piece != IPawn[opp] && !(((RO->BMask[sq] & Bishop(opp)) | (RO->RMask[sq] & Rook(opp)) | Queen(opp)) & v))
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

		uint64 control = EI.free[me];
		if (uint64 dbl = att & RO->PWay[me][sq] & Rook(me))  // we are supporting a rook
			control &= QueenAttacks(sq, EI.occ & ~dbl);
		else
			control &= att;
		IncV(EI.score, RO->MobQueen[0][pop(control) + Regions24(pop, control) / 2]);
		IncV(EI.score, RO->MobQueen[1][pop(control & RO->KingFlank[EI.king[opp]])]);
		if (control & Pawn(opp))
			IncV(EI.score, Values::TacticalMajorPawn);
		if (control & Minor(opp))
			IncV(EI.score, Values::TacticalMajorMinor);
		if (uint64 a = att & EI.area[opp])
		{
			EI.king_att[me] += (Single(a) && F(a & control)) ? KingQAttack1 : KingQAttack;
			for (uint64 v = att & EI.area[opp]; T(v); Cut(v))
				if (RO->FullLine[sq][lsb(v)] & att & ((Rook(me) & RO->RMask[sq]) | (Bishop(me) & RO->BMask[sq])))
					EI.king_att[me]++;
		}
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
		if (RO->RMask[sq] & King(opp))
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
		IncV(EI.score, RO->MobRook[0][pop(control) + Regions24(pop, control) / 2]);
		IncV(EI.score, RO->MobRook[1][pop(control & RO->KingFlank[EI.king[opp]])]);
		if (control & Pawn(opp))
			IncV(EI.score, Values::TacticalMajorPawn);
		if (control & Minor(opp))
			IncV(EI.score, Values::TacticalMajorMinor);
		if (uint64 a = att & EI.area[opp])
		{
			EI.king_att[me] += (Single(a) && F(a & control)) ? KingRAttack1 : KingRAttack;
			for (uint64 v = att & EI.area[opp]; T(v); Cut(v))
				if (RO->FullLine[sq][lsb(v)] & att & Major(me))
					EI.king_att[me]++;
		}
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
		if (RO->BMask[sq] & King(opp))
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

		uint64 control = att & EI.free[me];
		IncV(EI.score, RO->MobBishop[0][pop(control) + Regions24(pop, control) / 2]);
		IncV(EI.score, RO->MobBishop[1][pop(control & RO->KingFrontal[EI.king[opp]])]);
		if (control & Pawn(opp))
			IncV(EI.score, Values::TacticalMinorPawn);
		if (control & Knight(opp))
			IncV(EI.score, Values::TacticalMinorMinor);
		if (uint64 a = att & EI.area[opp])
			EI.king_att[me] += (Single(a) && F(a & control)) ? KingBAttack1 : KingBAttack;
		if (att & EI.area[me])
			IncV(EI.score, Values::KingDefBishop);
		Current->threat |= att & Major(opp);
		const uint64& myArea = (b & LightArea) ? LightArea : DarkArea;
		uint64 v = RO->BishopForward[me][sq] & Pawn(me) & myArea;
		v |= (v & (File[2] | File[3] | File[4] | File[5] | RO->BMask[sq])) >> 8;	// the ">>8" is just a trick to double-count these without two calls to pop()
		DecV(EI.score, Values::BishopPawnBlock * pop(v));
		if (int f = FileOf(sq);  T(b & BOutpost[me])
			&& F(Knight(opp))
			&& T(Current->patt[me] & b)
			&& F((Pawn(opp) | (Pawn(me) & Current->patt[opp])) & PIsolated[f] & RO->Forward[me][RankOf(sq)])
			&& F(Piece((T(b & LightArea) ? WhiteLight : WhiteDark) | opp)))
		{
			uint64 central = FileOf(sq) < 4 ? West[sq] : East[sq];
			// check inside-outness

			uint64 p = RO->PAtt[opp][sq] & Pawn(me);
			if ((f < 3 && F(p & East[f])) || (f > 4 && F(p & West[f])))
				IncV(EI.score, Values::BishopOutpostInsideOut);
			else
				IncV(EI.score, Values::BishopOutpostNoMinor);
		}
	}
}

template<bool me, class POP> INLINE void eval_knights(GEvalInfo& EI)
{
	POP pop;
	uint64 b;
	for (uint64 u = Knight(me); T(u); u ^= b)
	{
		int sq = lsb(u);
		b = Bit(sq);
		uint64 att = RO->NAtt[sq];
		Current->att[me] |= att;
		Current->threat |= att & Major(opp);
		uint64 control = att & EI.free[me];
		IncV(EI.score, RO->MobKnight[0][pop(control) + Regions24(pop, control) / 2]);
		IncV(EI.score, RO->MobKnight[1][pop(control & RO->KingFrontal[EI.king[opp]])]);
		if (control & Pawn(opp))
			IncV(EI.score, Values::TacticalMinorPawn);
		if (control & Bishop(opp))
			IncV(EI.score, Values::TacticalMinorMinor);
		if (uint64 a = att & EI.area[opp])
			EI.king_att[me] += (Single(att & (EI.area[opp] | RO->KAtt[EI.king[opp]]))) ? KingNAttack1 : KingNAttack;
		if (att & EI.area[me])
			IncV(EI.score, Values::KingDefKnight);
		if (T(b & NOutpost[me]) && F((Pawn(opp) | (Pawn(me) & Current->patt[opp])) & PIsolated[FileOf(sq)] & RO->Forward[me][RankOf(sq)]))
		{
			IncV(EI.score, Values::KnightOutpost);
			if (Current->patt[me] & b)
			{
				IncV(EI.score, Values::KnightOutpostProtected);
				if (att & EI.free[me] & Pawn(opp))
					IncV(EI.score, Values::KnightOutpostPawnAtt);
				if (F(Knight(opp) | Piece((T(b & LightArea) ? WhiteLight : WhiteDark) | opp)))
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

static double KA_E = 0, KA_N = 0;

template<bool me, class POP> INLINE void eval_king(GEvalInfo& EI)
{
	constexpr array<int, 4> PhaseScale = { 14, 11, 3, -3 };
	constexpr array<uint16, 16> KingAttackScale = { 0, 1, 2, 6, 8, 10, 14, 19, 25, 31, 39, 47, 46, 65, 65, 65 };
	constexpr array<int, 4> KingCenterScale = { 62, 61, 70, 68 };
	POP pop;
	uint16 cnt = Min<uint16>(15, UUnpack1(EI.king_att[me]));
	uint16 score = UUnpack2(EI.king_att[me]);
	if (cnt >= 2 && T(Queen(me)))
	{
		score += (EI.PawnEntry->shelter[opp] * KingShelterQuad) / 64;
		if (uint64 u = Current->att[me] & EI.area[opp] & (~Current->att[opp]))
			score += pop(u) * KingAttackSquare;
		if (!(RO->KAtt[EI.king[opp]] & (~(Piece(opp) | Current->att[me]))))
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
		uint64 holes = RO->KingFlank[EI.king[opp]] & ~Current->att[opp];
		int nHoles = pop(holes);
		int nIncursions = pop(holes & Current->att[me]);
		uint64 personnel = NonPawnKing(opp);
		uint64 guards = RO->KingFlank[EI.king[opp]] & personnel;
		uint64 awol = personnel ^ guards;
		int nGuards = pop(guards) + pop(guards & Queen(opp));
		int nAwol = pop(awol) + pop(awol & Queen(opp));
		adjusted += (adjusted * (max(0, nAwol - nGuards) + max(0, 3 * nIncursions + nHoles - 10))) / 32;
	}

	int op = ((PhaseScale[0] + OwnRank<opp>(EI.king[opp])) * adjusted) / 32;
	int md = (PhaseScale[1] * adjusted) / 32;
	int eg = (PhaseScale[2] * adjusted) / 32;
	int cl = (PhaseScale[3] * adjusted) / 32;
	IncV(EI.score, 0 * cnt + Pack4(op, md, eg, cl));
}

template<bool me, class POP> INLINE void eval_passer(GEvalInfo& EI)
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
				if (F(Pawn(opp) & (RO->VLine[square] | PIsolated[FileOf(square)]) & RO->Forward[me][RankOf(square)]))
					++connected;
			}
			if (connected)
				IncV(EI.score, RO->PasserConnected[rank]);
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
	constexpr array<int, 2> now = { 30, 10 };
	constexpr array<int, 2> later = { 15, 5 };
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
	EI.king_att[me] = Multiple(Current->patt[me] & EI.area[opp]) ? KingPAttackInc : 0;
	DecV(EI.score, pop(Shift<opp>(EI.occ) & Pawn(me)) * Values::PawnBlocked);
	EI.free[me] = Queen(opp) | King(opp) | (~(Current->patt[opp] | Pawn(me) | King(me)));
	eval_queens<me, POP>(EI);
	EI.free[me] |= Rook(opp);
	eval_rooks<me, POP>(EI);
	EI.free[me] |= Minor(opp);
	eval_bishops<me, POP>(EI);
	eval_knights<me, POP>(EI);
}

template<class POP> struct UnpackScore_
{
	int clx_;
	sint16 mat_, closed_;
	uint8 phase_;
	array<uint8, 2> mul_;
	UnpackScore_(const GMaterial* material)  
	{
		if (material)
		{
			phase_ = material->phase;
			mat_ = material->score;
			mul_ = material->mul;
			closed_ = material->closed;
		}
		else
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

			int phase = Phase[PieceType[WhitePawn]] * (wp + bp)
				+ Phase[PieceType[WhiteKnight]] * (wminor + bminor)
				+ Phase[PieceType[WhiteRook]] * (wr + br)
				+ Phase[PieceType[WhiteQueen]] * (wq + bq);
			phase_ = Min((Max(phase - PhaseMin, 0) * MAX_PHASE) / (PhaseMax - PhaseMin), MAX_PHASE);
			mat_ = SeeValue[WhitePawn] * (wp - bp) + SeeValue[WhiteKnight] * (wminor - bminor) + SeeValue[WhiteRook] * (wr - br) +
				SeeValue[WhiteQueen] * (wq - bq);
			mul_ = { 33, 33 };
			closed_ = 0;
		}
		clx_ = closure<POP>(); 
	}
	sint16 operator()(packed_t score) const
	{
		int md = Middle(score), cl = Closed(score);
		sint16 clVal = static_cast<sint16>((clx_ * (Min<int>(phase_, MIDDLE_PHASE) * cl + MIDDLE_PHASE * closed_)) / 8192);	// closure is capped at 128, phase at 64
		if (phase_ > MIDDLE_PHASE)
			return clVal + (Opening(score) * (phase_ - MIDDLE_PHASE) + md * (MAX_PHASE - phase_)) / PHASE_M2M;
		else
			return clVal + (md * phase_ + Endgame(score) * (MIDDLE_PHASE - phase_)) / MIDDLE_PHASE;
	}
};

sint16 depreciate_katt(sint16 score, sint16 katt, sint16 katt_opp)
{
	constexpr sint16 CUT = 130 * CP_EVAL;
	return score - min<sint16>(score - CUT, max<sint16>(katt, katt_opp / 3)) / 4;
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
	EI.area[White] = (RO->KAtt[EI.king[White]] | King(White)) & ((~Current->patt[White]) | Current->patt[Black]);
	EI.area[Black] = (RO->KAtt[EI.king[Black]] | King(Black)) & ((~Current->patt[Black]) | Current->patt[White]);
	Current->att[White] = Current->patt[White];
	Current->att[Black] = Current->patt[Black];
	Current->passer = 0;
	Current->threat = (Current->patt[White] & NonPawn(Black)) | (Current->patt[Black] & NonPawn(White));
	EI.score = Current->pst;
	if (F(Current->material & FlagUnusualMaterial))
		EI.material = &RO->Material[Current->material];
	else
		EI.material = 0;

	eval_sequential<White, POP>(EI);
	eval_sequential<Black, POP>(EI);

	EI.PawnEntry = &PawnHash[Current->pawn_key & PAWN_HASH_MASK];
	if (Current->pawn_key != EI.PawnEntry->key)
		eval_pawn_structure<POP>(EI, EI.PawnEntry);
	EI.score += EI.PawnEntry->score;

	eval_king<White, POP>(EI);
	eval_king<Black, POP>(EI);
	Current->att[White] |= RO->KAtt[EI.king[White]];
	Current->att[Black] |= RO->KAtt[EI.king[Black]];

	eval_passer<White, POP>(EI);
	eval_pieces<White, POP>(EI);
	eval_passer<Black, POP>(EI);
	eval_pieces<Black, POP>(EI);

	UnpackScore_<POP> value(EI.material);
	Current->score = value.mat_ + value(EI.score);
	// apply contempt before drawishness
	if (Contempt > 0 && (!EI.material || EI.material->contempt[Data->turn]))
	{
		int mySign = F(Data->turn) ? 1 : -1;
		int maxContempt = (value.phase_ * Contempt * CP_EVAL) / 64;
		if (Current->score * mySign > 2 * maxContempt)
			Current->score += mySign * maxContempt;
		else if (Current->score * mySign > 0)
			Current->score += Current->score / 2;
	}

	if (Current->ply >= PliesToEvalCut)
		Current->score /= 2;
	if (Current->score > 0)
	{
		//Current->score = depreciate_katt(Current->score, value(EI.king_att_val[White]), value(EI.king_att_val[Black]));
		EI.mul = value.mul_[White];
		if (EI.material && EI.material->eval[White] && !eval_stalemate<White>(EI))
			EI.material->eval[White](EI, pop.Imp());
		else if (EI.mul <= 32)
			EI.mul = Min(EI.mul, 37 - value.clx_ / 8);
		Current->score -= (Min<int>(Current->score, DrawCap) * EI.PawnEntry->draw[White]) / 64;
	}
	else if (Current->score < 0)
	{
		//Current->score = -depreciate_katt(-Current->score, value(EI.king_att_val[Black]), value(EI.king_att_val[White]));
		EI.mul = value.mul_[Black];
		if (EI.material && EI.material->eval[Black] && !eval_stalemate<Black>(EI))
			EI.material->eval[Black](EI, pop.Imp());
		else if (EI.mul <= 32)
			EI.mul = Min(EI.mul, 37 - value.clx_ / 8);
		Current->score += (Min<int>(-Current->score, DrawCap) * EI.PawnEntry->draw[Black]) / 64;
	}
	else
		EI.mul = Min(value.mul_[White], value.mul_[Black]);
	Current->score = (Current->score * EI.mul * CP_SEARCH) / (32 * CP_EVAL);

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
{
	HI
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
		if ((RO->QMask[from] & u) == 0)
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
		if (F(RO->KAtt[from] & u))
			return 0;
		if (Current->att[opp] & u)
			return 0;
		return 1;
	}
	piece = (piece >> 1) - 2;
	if (piece == 0)
	{
		if (u & RO->NAtt[from])
			return 1;
		else
			return 0;
	}
	else
	{
		if (piece <= 2)
		{
			if (RO->BMask[from] & u)
				return 1;
		}
		else if (piece == 3)
		{
			if (RO->RMask[from] & u)
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
		if (RO->NAtt[to] & king)
			return true;
	}
	else if (piece < WhiteRook)
	{
		if (RO->BMask[to] & king)
			if (F(RO->Between[king_sq][to] & PieceAll()))
				return true;
	}
	else if (piece < WhiteQueen)
	{
		if (RO->RMask[to] & king)
			if (F(RO->Between[king_sq][to] & PieceAll()))
				return true;
	}
	else if (piece < WhiteKing)
	{
		if (RO->QMask[to] & king)
			if (F(RO->Between[king_sq][to] & PieceAll()))
				return true;
	}
	return false;
}

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
	else if (move && is_legal(T(Current->turn), move))
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

template<bool me> int extension(int pv, int move, int depth, bool* check = nullptr)
{
	if (is_check<me>(move))
	{
		if (check) *check = true;
		return see<me>(move, -SeeThreshold, SeeValue) + T(pv || depth < 16);
	}
	if (check) *check = false;
	int from = From(move);
	if (HasBit(Current->passer, from))
	{
		if (T(Current->material & FlagUnusualMaterial) || Current->material >= TotalMat || RO->Material[Current->material].phase > MIDDLE_PHASE)
		{
			int rank = OwnRank(me, from);
			if (rank >= 5 && depth < 16)
				return pv ? 2 : 1;
		}
	}
	int to = To(move);
	if (T(PieceAt(to)) && depth < 13)
	{
		if ((T(Current->material & FlagUnusualMaterial) || Current->material >= TotalMat || RO->Material[Current->material].phase > MIDDLE_PHASE)
			&& HasBit(RO->KAttAtt[lsb(King(me))] | RO->KAttAtt[lsb(King(opp))], to))
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
	int move = *(Current->current);
	if (F(move))
		return 0;
	int* best = Current->current;
	for (int* p = Current->current + 1; T(*p); ++p)
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

void mark_evasions(int* list)
{
	for (; T(*list); ++list)
	{
		int move = (*list) & 0xFFFF;
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

template<bool me, bool root> int get_move(int depth)
{
	int move;

	if (root)
	{
		move = (*Current->current) & 0xFFFF;
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
	att |= RO->NAtt[to] & Knight(opp);
	if (T(att) && delta > mat_value[WhiteDark] - margin)
		return 0;
	occ = PieceAll() & clear;
	b_area = BishopAttacks(to, occ);
	opp_bishop = Bishop(opp);
	if (delta > mat_value[IDark[me]] - margin)
		if (b_area & opp_bishop)
			return 0;
	my_bishop = Bishop(me);
	b_slider_att = RO->BMask[to] & (opp_bishop | Queen(opp));
	r_slider_att = RO->RMask[to] & Major(opp);
	b_slider_def = RO->BMask[to] & (my_bishop | Queen(me)) & clear;
	r_slider_def = RO->RMask[to] & Major(me) & clear;
	att |= (b_slider_att & b_area);
	def |= RO->NAtt[to] & Knight(me) & clear;
	r_area = RookAttacks(to, occ);
	att |= (r_slider_att & r_area);
	def |= (b_slider_def & b_area);
	def |= (r_slider_def & r_area);
	att |= RO->KAtt[to] & King(opp);
	def |= RO->KAtt[to] & King(me) & clear;
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
	if (RO->NAttAtt[dst] & King(me))
	{
		for (uint64 nn = Knight(opp) & RO->NAttAtt[dst]; nn; Cut(nn))
		{
			if (T(RO->NAtt[dst] & RO->NAtt[lsb(nn)] & RO->NAtt[lsb(King(me))]))
				return true;
		}
	}
	return false;
}

template<class T_> T_* NullTerminate(T_* list)
{
	*list = 0;
	return list;
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
			list = AddMove(list, from, to, FlagPKnight, T(RO->NAtt[to] & King(opp)) ? MvvLvaPromotionKnight : MvvLvaPromotionBad);
			list = AddMove(list, from, to, FlagPRook, MvvLvaPromotionBad);
			list = AddMove(list, from, to, FlagPBishop, MvvLvaPromotionBad);
		}
	}
	for (v = ShiftW<opp>(Current->mask) & Pawn(me) & OwnLine(me, 6); T(v); Cut(v))
	{
		list = AddMove(list, lsb(v), lsb(v) + PushE[me], FlagPQueen, MvvLvaPromotionCap(PieceAt(lsb(v) + PushE[me])));
		if (HasBit(RO->NAtt[lsb(King(opp))], lsb(v) + PushE[me]))
			list = AddMove(list, lsb(v), lsb(v) + PushE[me], FlagPKnight, MvvLvaPromotionKnightCap(PieceAt(lsb(v) + PushE[me])));
	}
	for (v = ShiftE<opp>(Current->mask) & Pawn(me) & OwnLine(me, 6); T(v); Cut(v))
	{
		list = AddMove(list, lsb(v), lsb(v) + PushW[me], FlagPQueen, MvvLvaPromotionCap(PieceAt(lsb(v) + PushW[me])));
		if (HasBit(RO->NAtt[lsb(King(opp))], lsb(v) + PushW[me]))
			list = AddMove(list, lsb(v), lsb(v) + PushW[me], FlagPKnight, MvvLvaPromotionKnightCap(PieceAt(lsb(v) + PushW[me])));
	}
	if (T(Current->att[me] & Current->mask))
	{
		for (v = ShiftW<opp>(Current->mask) & Pawn(me) & (~OwnLine(me, 6)); T(v); Cut(v))
			list = AddCaptureP(list, IPawn[me], lsb(v), lsb(v) + PushE[me], 0);
		for (v = ShiftE<opp>(Current->mask) & Pawn(me) & (~OwnLine(me, 6)); T(v); Cut(v))
			list = AddCaptureP(list, IPawn[me], lsb(v), lsb(v) + PushW[me], 0);
		for (v = RO->KAtt[lsb(King(me))] & Current->mask & (~Current->att[opp]); T(v); Cut(v))
			list = AddCaptureP(list, IKing[me], lsb(King(me)), lsb(v), 0);
		for (u = Knight(me); T(u); Cut(u))
			for (v = RO->NAtt[lsb(u)] & Current->mask; T(v); Cut(v))
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

template<bool me> int* gen_evasions(int* list)
{
	static const int MvvLvaPromotion = RO->MvvLva[WhiteQueen][BlackQueen];

	int from;
	uint64 b, u;
	//	pair<uint64, uint64> pJoins = pawn_joins(me, Pawn(me));

	int king = lsb(King(me));
	uint64 att = (RO->NAtt[king] & Knight(opp)) | (RO->PAtt[me][king] & Pawn(opp));
	for (u = (RO->BMask[king] & (Bishop(opp) | Queen(opp))) | (RO->RMask[king] & (Rook(opp) | Queen(opp))); T(u); u ^= b)
	{
		b = Bit(lsb(u));
		if (F(RO->Between[king][lsb(u)] & PieceAll()))
			att |= b;
	}
	int att_sq = lsb(att);  // generally the only attacker
	uint64 esc = RO->KAtt[king] & (~(Piece(me) | Current->att[opp])) & Current->mask;
	if (PieceAt(att_sq) >= WhiteLight)
		esc &= ~RO->FullLine[king][att_sq];
	else if (PieceAt(att_sq) >= WhiteKnight)
		esc &= ~RO->NAtt[att_sq];

	Cut(att);
	if (att)
	{  // second attacker (double check)
		att_sq = lsb(att);
		if (PieceAt(att_sq) >= WhiteLight)
			esc &= ~RO->FullLine[king][att_sq];
		else if (PieceAt(att_sq) >= WhiteKnight)
			esc &= ~RO->NAtt[att_sq];

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
		from = lsb(u);
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
		for (esc = RO->NAtt[lsb(u)] & inter; T(esc); Cut(esc))
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

	uint64 occ = PieceAll();
	if (me == White)
	{
		if (can_castle(occ, me, true))
			list = AddHistoryP(list, IKing[White], 4, 6, FlagPriority);
		if (can_castle(occ, me, false))
			list = AddHistoryP(list, IKing[White], 4, 2, FlagPriority);
	}
	else
	{
		if (can_castle(occ, me, true))
			list = AddHistoryP(list, IKing[Black], 60, 62, FlagPriority);
		if (can_castle(occ, me, false))
			list = AddHistoryP(list, IKing[Black], 60, 58, FlagPriority);
	}

	uint64 free = ~occ;
	auto pTarget = PawnJoins<me>();
	auto pFlag = [&](int to) {return HasBit(pTarget, to) ? FlagPriority : 0; };
	for (v = Shift<me>(Pawn(me)) & free & (~OwnLine(me, 7)); T(v); Cut(v))
	{
		int to = lsb(v);
		int passer = T(HasBit(Current->passer, to - Push[me]));
		int leading = passer && F(Current->passer & Pawn(me) & RO->Forward[me][RankOf(to - Push[me])]);
		if (HasBit(OwnLine(me, 2), to) && F(PieceAt(to + Push[me])))
			list = AddHistoryP(list, IPawn[me], to - Push[me], to + Push[me], passer ? FlagPriority : pFlag(to + Push[me]));
		list = AddHistoryP(list, IPawn[me], to - Push[me], to, passer ? FlagPriority : pFlag(to));
	}

	// for all other pieces, we distinguish threat moves
	for (u = Knight(me); T(u); Cut(u))
	{
		int from = lsb(u);
		for (v = free & RO->NAtt[from]; T(v); Cut(v))
		{
			int to = lsb(v);
			int flag = RO->NAtt[to] & Major(opp) ? FlagPriority : 0;
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
			int flag = RO->BMask[to] & (RO->PAtt[opp][to] & Pawn(me) ? Major(opp) : Rook(opp)) ? FlagCastling : 0;
			list = AddHistoryP(list, which, from, to, flag);
		}
	}

	for (u = Rook(me); T(u); Cut(u))
	{
		int from = lsb(u);
		for (v = free & RookAttacks(from, occ); T(v); Cut(v))
		{
			int to = lsb(v);
			int flag = (RO->PAtt[opp][to] & Pawn(me)) && (RO->RMask[to] & Queen(opp)) ? FlagPriority : 0;
			list = AddHistoryP(list, IRook[me], from, to, flag);
		}
	}
	for (u = Queen(me); T(u); Cut(u))
	{
		//uint64 qTarget = RO->NAtt[lsb(King(opp))];	// try to get next to this
		int from = lsb(u);
		for (v = free & QueenAttacks(from, occ); T(v); Cut(v))
		{
			int to = lsb(v);
			list = AddHistoryP(list, IQueen[me], from, to, HasBit(Current->att[opp], to) ? 0 : FlagPriority);
		}
	}

	int from = lsb(King(me));
	auto kTarget = Shift<opp>(Current->passer);	// behind ours, ahead of his
	auto kFlag = [&](int to) {return HasBit(kTarget, to) ? FlagPriority : 0; };
	for (v = RO->KAtt[lsb(King(me))] & free & (~Current->att[opp]); T(v); Cut(v))
	{
		int to = lsb(v);
		list = AddHistoryP(list, IKing[me], from, to, kFlag(to));
	}

	return NullTerminate(list);
}

template<bool me> int* gen_checks(int* list)
{
	static const int MvvLvaXray = RO->MvvLva[WhiteQueen][WhitePawn];
	uint64 clear = ~(Piece(me) | Current->mask);
	int king = lsb(King(opp));
	// discovered checks
	for (uint64 u = Current->xray[me] & Piece(me); T(u); Cut(u))
	{
		int from = lsb(u);
		uint64 target = clear & (~RO->FullLine[king][from]);
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

				for (uint64 v = RO->PAtt[me][from] & target & Piece(opp); T(v); Cut(v))
					list = AddMove(list, from, lsb(v), 0, MvvLvaXrayCap(PieceAt(lsb(v))));
			}
		}
		else
		{
			uint64 v;
			if (PieceAt(from) < WhiteLight)
				v = RO->NAtt[from] & target;
			else if (PieceAt(from) < WhiteRook)
				v = BishopAttacks(from, PieceAll()) & target;
			else if (PieceAt(from) < WhiteQueen)
				v = RookAttacks(from, PieceAll()) & target;
			else if (PieceAt(from) < WhiteKing)
				v = QueenAttacks(from, PieceAll()) & target;
			else
				v = RO->KAtt[from] & target & (~Current->att[opp]);
			for (; T(v); Cut(v))
				list = AddMove(list, from, lsb(v), 0, MvvLvaXrayCap(PieceAt(lsb(v))));
		}
	}

	const uint64 nonDiscover = ~(Current->xray[me] & Piece(me));  // exclude pieces already checked
	for (uint64 u = Knight(me) & RO->NAttAtt[king] & nonDiscover; T(u); Cut(u))
		for (uint64 v = RO->NAtt[king] & RO->NAtt[lsb(u)] & clear; T(v); Cut(v))
			list = AddCaptureP(list, IKnight[me], lsb(u), lsb(v), 0);

	for (uint64 u = RO->KAttAtt[king] & Pawn(me) & (~OwnLine(me, 6)) & nonDiscover; T(u); Cut(u))
	{
		int from = lsb(u);
		for (uint64 v = RO->PAtt[me][from] & RO->PAtt[opp][king] & clear & Piece(opp); T(v); Cut(v))
			list = AddCaptureP(list, IPawn[me], from, lsb(v), 0);
		if (F(PieceAt(from + Push[me])) && HasBit(RO->PAtt[opp][king], from + Push[me]))
			list = AddMove(list, from, from + Push[me], 0, 0);
	}

	uint64 b_target = BishopAttacks(king, PieceAll()) & clear;
	uint64 r_target = RookAttacks(king, PieceAll()) & clear;
	for (uint64 u = Board->bb[(T(King(opp) & LightArea) ? WhiteLight : WhiteDark) | me] & nonDiscover; T(u); Cut(u))
		for (uint64 v = BishopAttacks(lsb(u), PieceAll()) & b_target; T(v); Cut(v))
			list = AddCapture(list, lsb(u), lsb(v), 0);
	for (uint64 u = Rook(me) & nonDiscover; T(u); Cut(u))
		for (uint64 v = RookAttacks(lsb(u), PieceAll()) & r_target; T(v); Cut(v))
			list = AddCaptureP(list, IRook[me], lsb(u), lsb(v), 0);
	for (uint64 u = Queen(me) & nonDiscover; T(u); Cut(u))
	{
		uint64 contact = RO->KAtt[king];
		int from = lsb(u);
		for (uint64 v = QueenAttacks(from, PieceAll()) & (b_target | r_target); T(v); Cut(v))
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
		for (uint64 u = Pawn(me) & OwnLine(me, 1) & nonDiscover & RO->PAtt[opp][king - 2 * Push[me]]; T(u); Cut(u))
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
		for (uint64 v = free & RO->NAtt[lsb(u)]; T(v); Cut(v))
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
	for (uint64 v = RO->KAtt[lsb(King(me))] & free & (~Current->att[opp]); T(v); Cut(v))
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

namespace Futility
{
	constexpr array<sint16, 10> PieceThreshold = { 12, 18, 22, 24, 25, 26, 27, 26, 40, 40 };	// in CP
	constexpr array<sint16, 8> PasserThreshold = { 0, 0, 0, 0, 0, 20, 40, 0 };

	template<bool me> inline sint16 x()
	{
		sint16 retval = PieceThreshold[pop0(NonPawnKing(me))];
		if (uint64 passer = Current->passer & Pawn(me))
			retval = Max(retval, PasserThreshold[OwnRank<me>(NB<opp>(passer))]);
		return retval;
	}

	template<bool me> inline sint16 HashCut(bool did_delta_moves)
	{
		return (did_delta_moves ? 4 : 8) * x<me>();
	}
	template<bool me> inline sint16 CheckCut()
	{
		return 11 * x<me>();
	}
	template<bool me> inline sint16 DeltaCut()
	{
		return HashCut<me>(false);
	}
	template<bool me> inline sint16 ScoutCut(int depth)
	{
		return (depth > 3 ? 4 : 7) * x<me>();
	}
};
template<bool me> inline int MinZugzwangDepth()
{
	return 16;	// below this depth, no point checking for Zugzwang
}

template<bool me, bool pv> int q_search(int alpha, int beta, int depth, int flags)
{
	int i, value, score, move, hash_move, hash_depth;
	GEntry* Entry;
	auto finish = [&](const score_t& score, bool did_delta_moves)
	{
		if (depth >= 0)
			hash_high(score, 1);
		else if (depth >= -2)
		{
			if (auto fut = Futility::HashCut<me>(did_delta_moves); Current->score + fut >= alpha)
				hash_high(alpha, 1);
		}
		return score;
	};

	if (flags & FlagHaltCheck)
	{
		HALT_CHECK;
	}
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
					&& (depth < -2
						|| depth <= -1 && Current->score + Futility::HashCut<me>(false) < alpha))
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

	if (depth < -2 || (depth < 0 && Current->score + Futility::CheckCut<me>() < alpha))
		return score;	// never hash this
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

	if (T(nTried) || Current->score + Futility::DeltaCut<me>() < alpha || T(Current->threat & Piece(me)) || T(Current->xray[opp] & NonPawn(opp)) ||
		T(Pawn(opp) & OwnLine(me, 1) & Shift<me>(~PieceAll())))
		return finish(score, false);
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
	return finish(score, true);
}

template<bool me, bool pv> int q_evasion(int alpha, int beta, int depth, int flags)
{
	int i, value, pext, score, move, cnt, hash_move, hash_depth;
	int* p;
	GEntry* Entry;

	score = static_cast<int>(Current - Data) - MateValue;
	if (flags & FlagHaltCheck)
	{
		HALT_CHECK;
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

template<bool me> int smp_search(GSP* Sp)
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
			value = -scout<opp, 0>(-alpha, M->reduced_depth, FlagNeatSearch | ExtToFlag(M->ext));
			if (value > alpha && (Sp->pv || M->reduced_depth < M->research_depth))
			{
				if (Sp->pv)
					value = -pv_search<opp, 0>(-Sp->beta, -Sp->alpha, M->research_depth, FlagNeatSearch | ExtToFlag(M->ext));
				else
					value = -scout<opp, 0>(-alpha, M->research_depth, FlagNeatSearch | FlagDisableNull | ExtToFlag(M->ext));
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

template<bool exclusion> int cut_search(int move, int hash_move, int score, int beta, int depth, int flags, int sp_init)
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
		if (move != hash_move && Current->stage == s_quiet && !sp_init)
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

template<int PV = 0> struct LMR_
{
	const double scale_;
	LMR_(int depth, bool no_hash) : scale_((no_hash ? 0.14 : 0.1) + 0.0015 * depth) {}
	INLINE int operator()(int cnt) const
	{
		return cnt > 2 ? int(scale_ * msb(Square(Square(Square(uint64(cnt)))))) - PV : 0;
	}
};

template<int principal> INLINE void check_recapture(int to, int depth, int* ext)
{
	if (principal == 1 && *ext < 2 && depth < 16 && T(PieceAt(to)))
	{
		if (to == To(Current->move))
			*ext = 2;	// recapture extension
		else if (principal > 100)
		{
			if (Current - Data >= 2 && to == To((Current - 2)->move))
				*ext = 1;
			else if (Current - Data >= 4 && to == To((Current - 4)->move))
				*ext = 1;
		}
	}
}

template<bool me, bool exclusion> int scout(int beta, int depth, int flags);
template<bool me, bool root> int pv_search(int alpha, int beta, int depth, int flags);

void check_state()
{
	GSP* Sp, * Spc;
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
			value = -scout<1, 0>(-alpha, new_depth, FlagNeatSearch | ExtToFlag(ext));
			if (value > alpha)
				value = -pv_search<1, 0>(-beta, -alpha, r_depth, ExtToFlag(ext));
		}
		else
		{
			value = -scout<1, 0>(1 - beta, new_depth, FlagNeatSearch | ExtToFlag(ext));
			if (value >= beta && new_depth < r_depth)
				value = -scout<1, 0>(1 - beta, r_depth, FlagNeatSearch | FlagDisableNull | ExtToFlag(ext));
		}
		undo_move<0>(move);
	}
	else
	{
		do_move<1>(move);
		if (pv)
		{
			value = -scout<0, 0>(-alpha, new_depth, FlagNeatSearch | ExtToFlag(ext));
			if (value > alpha)
				value = -pv_search<0, 0>(-beta, -alpha, r_depth, ExtToFlag(ext));
		}
		else
		{
			value = -scout<0, 0>(1 - beta, new_depth, FlagNeatSearch | ExtToFlag(ext));
			if (value >= beta && new_depth < r_depth)
				value = -scout<0, 0>(1 - beta, r_depth, FlagNeatSearch | FlagDisableNull | ExtToFlag(ext));
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

	return GetTickCount64();
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

void send_pv(int depth, int alpha, int beta, int score)
{
	int i, pos, move, mate = 0, mate_score, sel_depth;
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
	sint64 tbhits = 0, elapsed = get_time() - StartTime;
#ifdef MP_NPS
	sint64 snodes = Smpi->nodes;
#ifdef TB
	tbhits = Smpi->tb_hits;
#endif
#else
	sint64 snodes = nodes;
#endif
	sint64 nps = elapsed ? (snodes * 1000ll) / elapsed : 12345;
	if (score < beta)
	{
		if (score <= alpha)
			fprintf(stdout, "info depth %d seldepth %d score %s%d upperbound time %I64d nodes %I64d nps %I64d tbhits %I64d pv %s\n", depth, sel_depth,
				score_string, (mate ? mate_score : score / CP_SEARCH), elapsed, snodes, nps, tbhits, pv_string);
		else
			fprintf(stdout, "info depth %d seldepth %d score %s%d time %I64d nodes %I64d nps %I64d tbhits %I64d pv %s\n", depth, sel_depth, score_string,
				(mate ? mate_score : score / CP_SEARCH), elapsed, snodes, nps, tbhits, pv_string);
	}
	else
		fprintf(stdout, "info depth %d seldepth %d score %s%d lowerbound nodes %I64d nps %I64d tbhits %I64d pv %s\n", depth, sel_depth, score_string,
			(mate ? mate_score : score / CP_SEARCH), snodes, nps, tbhits, pv_string);
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
			j + 1, (j <= curr_number ? depth : depth - 1), score_string, score, snodes, nps, tbhits, pv_string);
		fflush(stdout);
	}
}

void check_time(const int* time, int searching);

template<bool me> void root()
{
	int i, depth, value, alpha, beta, start_depth = 2, hash_depth = 0, hash_value = 0;
	int store_time = 0, time_est, ex_depth = 0, ex_value, prev_time = 0, knodes = 0;
	sint64 time;
	GPVEntry* PVEntry;

	++date;
	nodes = check_node = check_node_smp = 0;
	if (parent)
		Smpi->nodes = 0;
	memcpy(Data, Current, sizeof(GData));
	Current = Data;

#if TB
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
			if (ex_depth >= depth - Min(12, depth / 2) && ex_value <= hash_value - ExclSingle(depth))
			{
				BaseSI->Early = 1;
				BaseSI->Singular = 1;
				if (ex_value <= hash_value - ExclDouble(depth))
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
		memset(Data + 1, 0, 127 * sizeof(GData));
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
			if (alpha >= 0 && alpha < AspirationEpsilon)
				alpha = -1;
			beta = Previous + deltaHi;
			if (beta <= 0 && -beta < AspirationEpsilon)
				beta = 1;
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
					memset(Data + 1, 0, 127 * sizeof(GData));
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

void epd_test(const char filename[], int time_limit)
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
			stop_searching :
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
	const char now[10] = { mdy[7], mdy[8], mdy[9], mdy[10], mdy[0], mdy[1], mdy[2], mdy[4], mdy[5], 0 };
	char* ptr = nullptr;
	char* strtok_context = nullptr;
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

void check_time(const int* time, int searching)
{
#ifdef CPU_TIMING
	if (!time && CpuTiming && UciMaxKNodes && nodes > UciMaxKNodes * 1024)
		Stop = 1;
#endif
	while (!Stop && input())
		uci();
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

template<bool me, bool exclusion> int scout(int beta, int depth, int flags)
{
	int i, value, cnt, flag, moves_to_play, score, move, ext, hash_move, do_split, sp_init, singular, played, high_depth, high_value, hash_value,
		new_depth, move_back, hash_depth;
	int height = (int)(Current - Data);
	GSP* Sp = nullptr;

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
		cnt = high_depth = do_split = sp_init = singular = played = 0;
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

		if (Current->score - 90 * CP_SEARCH > beta
			&& F(flags & (FlagReturnBestMove | FlagDisableNull))
			&& F(Pawn(opp) & OwnLine(me, 1) & Shift<me>(~PieceAll()))
			&& depth < MinZugzwangDepth<me>())
		{
			int offset = (90 + depth * 8 + Max(depth - 5, 0) * 32 + Max(depth - 11, 0) * 128) * CP_SEARCH;
			if (beta < -200 * CP_SEARCH)
				offset += ((+beta + 100 * CP_SEARCH) * height) / 128;
			if (T(Pawn(opp) & OwnLine(me, 1) & Shift<me>(~PieceAll())))
				offset += SeeValue[WhiteQueen] - offset / 3;
			if (F(Queen(White) | Queen(Black)))
			{
				offset -= offset / 4;
				if (F(Rook(White) | Rook(Black)))
					offset -= offset / 4;
			}
			if ((value = Current->score - offset) >= beta)
				return value;
		}

		value = Current->score + Futility::ScoutCut<me>(depth);
		if (depth <= 3 && value < beta)
			return Max(value, q_search<me, 0>(beta - 1, beta, 1, FlagHashCheck | (flags & 0xFFFF)));

		high_depth = 0;
		high_value = MateValue;
		hash_value = -MateValue;
		hash_depth = -1;
		Current->best = hash_move = flags & 0xFFFF;
		if (GEntry* Entry = probe_hash())
		{
			if (Entry->high_depth > high_depth)
			{
				high_depth = Entry->high_depth;
				high_value = Entry->high;
			}
			if (Entry->high < beta && Entry->high_depth >= depth)
				return Entry->high;
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
				check_recapture<1>(To(move), depth, &ext);
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
						return cut_search<exclusion>(move, hash_move, score, beta, depth, flags, 0);
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

	do_split = sp_init = 0;
	if (depth >= SplitDepth && PrN > 1 && parent && !exclusion)
		do_split = 1;

	LMR_<0> lmr(depth, F(hash_move));
	while (move = get_move<me, 0>(Odd(depth)))
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
		check_recapture<0>(To(move), depth, &ext);

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
					int reduction = lmr(cnt);
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
		value = -scout<opp, 0>(1 - beta, new_depth, FlagNeatSearch | ExtToFlag(ext));
		if (value >= beta && new_depth < depth - 2 + ext)
			value = -scout<opp, 0>(1 - beta, depth - 2 + ext, FlagNeatSearch | FlagDisableNull | ExtToFlag(ext));
		undo_move<me>(move);
		++played;
		if (value > score)
		{
			score = value;
			if (value >= beta)
				return cut_search<exclusion>(move, hash_move, score, beta, depth, flags, sp_init);
		}
	}
	if (do_split && sp_init)
	{
		value = smp_search<me>(Sp);
		if (value >= beta && Sp->best_move)
		{
			score = beta;
			Current->best = move = Sp->best_move;
			for (i = 0; i < Sp->move_number; ++i)
			{
				GMove* M = &Sp->move[i];
				if ((M->flags & FlagFinished) && M->stage == s_quiet && M->move != move)
					HistoryBad(M->move, depth);
			}
		}
		if (value >= beta)
			return cut_search<exclusion>(move, hash_move, score, beta, depth, flags, sp_init);
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
				tb_hits++;
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
				check_recapture<2>(To(move), depth, &ext);
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
					int flags = FlagHaltCheck | FlagHashCheck | ((hash_value >= beta && hash_depth >= depth - 12) ? FlagDisableNull : 0) | ExtToFlag(ext);
					value = -scout<opp, 0>(1 - beta, new_depth, flags);
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
	LMR_<0> lmr(depth, false);
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
		check_recapture<0>(To(move), depth, &ext);
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
				int reduction = lmr(cnt);
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
			move_to_string(move, score_string);
			if (Print)
				sprintf_s(info_string, "info currmove %s currmovenumber %d\n", score_string, cnt);
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
		check_recapture<2>(To(move), depth, &ext);
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

	LMR_<1> lmr(depth, false);
	while (move = get_move<me, root>(Odd(depth)))
	{
		if (move == hash_move)
			continue;
		if (IsIllegal(me, move))
			continue;
		++cnt;
		if (root)
		{
			memset(Data + 1, 0, 127 * sizeof(GData));
			move_to_string(move, score_string);
			if (Print)
				sprintf_s(info_string, "info currmove %s currmovenumber %d\n", score_string, cnt);
		}
		if (IsRepetition(alpha + 1, move))
			continue;
		bool check = is_check<me>(move);
		ext = Max(pext, extension<me>(1, move, depth));
		check_recapture<0>(To(move), depth, &ext);
		new_depth = depth - 2 + ext;
		if (depth >= 6 && F(move & 0xE000) && F(PieceAt(To(move))) && (T(root) || !is_killer(move) || T(IsCheck(me))) && cnt > 3)
		{
			int reduction = lmr(cnt);
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
			value = -scout<opp, 0>(-alpha, new_depth, FlagNeatSearch | ExtToFlag(ext));
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

template<bool me> int multipv(int depth)
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
			best_move = MultiPV[0] & 0xFFFF;
			Current->score = MultiPV[0] >> 16;
			low = MultiPV[PVN - 1] >> 16;
			send_multipv((depth / 2), cnt);
		}
	}
	return Current->score;
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
int main(int argc, char* argv[])
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

	while (true) uci();
	return 0;
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
	constexpr int DEPTH_LIMIT = 124;
	max_depth = Min(max_depth, DEPTH_LIMIT);
	init_search(1);
	get_board(fen);
	auto cmd = _strdup("go infinite");
	get_time_limit(cmd);
	free(cmd);
	evaluate(); 
	cout << "Position eval = " << Current->score << "\n";
	for (int depth = Min(4, max_depth); depth <= max_depth; ++depth)
	{
		auto score = Current->turn
			? pv_search<true, true>(-MateValue, MateValue, depth, FlagNeatSearch)
			: pv_search<false, true>(-MateValue, MateValue, depth, FlagNeatSearch);
		cout << WriteMove_(best_move) << ":  " << score << ", " << nodes << " nodes\n";
	}
	cout << KA_N << " samples; mean " << KA_E << "\n";
	KA_N = 0;
	cin.ignore();
}

int main(int argc, char *argv[])
{
	int CPUInfo[4] = { -1, 0, 0, 0};
	__cpuid(CPUInfo, 1);
	HardwarePopCnt = (CPUInfo[2] >> 23) & 1;

	init();

	fprintf(stdout, "Roc (regression mode)\n");

	init_hash();

	Console = true;

	Test1("1r2b1k1/5pp1/q3p1p1/b2pP3/p2P4/Pr2RN1P/1PR2PP1/2QN2K1 w - - 0 1", 30, "a1-a1");	// Roc finds 0.49, Gull 0.14
	//Test1("5q2/7k/6b1/Q1nN4/P1Bbp1P1/1P4B1/7K/8 b - - 0 1", 30, "a1-a1");	// appears to have crashed in this position, but cannot reproduce
	//Test1("rnbqk2r/pp3ppp/2pb1n2/3p4/2PPp3/4P3/PPB1NPPP/RNBQ1RK1 b - - 0 1", 40, "d6-h2");

	//Test1("R7/6k1/8/8/6P1/6K1/8/7r w - - 0 1", 24, "g4-g5");
	//Test1("kr6/p7/8/8/8/8/8/BBK5 w - - 0 1", 24, "g4-g5");

	//Test1("4kbnr/2pr1ppp/p1Q5/4p3/4P3/2Pq1b1P/PP1P1PP1/RNB2RK1 w - - 0 1", 20, "f1-e1");	// why didn't Roc keep the rook pinned?

	//Test1("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1", 20, "a7-a8");
	//Test1("4r1k1/4ppbp/r5p1/3Np3/2PnP3/3P2Pq/1R3P2/2BQ1RK1 w - - 0 1", 20, "b2-b1");

	//Test1("8/4p3/8/3P3p/P2pK3/6P1/7b/3k4 w - - 0 1", 24, "d5-d6");					// Roc finds at depth 23
	//Test1("rq2r1k1/5pp1/p7/4bNP1/1p2P2P/5Q2/PP4K1/5R1R w - - 0 1", 4, "f5-g7");
	//Test1("6k1/2b2p1p/ppP3p1/4p3/PP1B4/5PP1/7P/7K w - - 0 1", 31, "d4-b6");			// Roc fails
	//Test1("5r1k/p1q2pp1/1pb4p/n3R1NQ/7P/3B1P2/2P3P1/7K w - - 0 1", 26, "e5-e6");	// Roc finds at depth 26
	//Test1("5r1k/1P4pp/3P1p2/4p3/1P5P/3q2P1/Q2b2K1/B3R3 w - - 0 1", 36, "a2-f7");	// Roc fails
	//Test1("3B4/8/2B5/1K6/8/8/3p4/3k4 w - - 0 1", 15, "b5-a6");						// Roc fails
	//Test1("1k1r4/1pp4p/2n5/P6R/2R1p1r1/2P2p2/1PP2B1P/4K3 b - - 0 1", 18, "e4-e3");	// Roc finds at depth 16
	//Test1("6k1/p3q2p/1nr3pB/8/3Q1P2/6P1/PP5P/3R2K1 b - - 0 1", 12, "c6-d6");		// Roc finds at depth 11
	//Test1("2krr3/1p4pp/p1bRpp1n/2p5/P1B1PP2/8/1PP3PP/R1K3B1 w - - 0 1", 15, "d6-c6");	// Roc finds at depth 14
	//Test1("r5k1/pp2p1bp/6p1/n1p1P3/2qP1NP1/2PQB3/P5PP/R4K2 b - - 0 1", 18, "g6-g5");	// Roc fails (finds at depth 16 then switches away)
	//Test1("2r3k1/1qr1b1p1/p2pPn2/nppPp3/8/1PP1B2P/P1BQ1P2/5KRR w - - 0 1", 25, "g1-g7");	// Roc finds at depth 18
	//Test1("1br3k1/p4p2/2p1r3/3p1b2/3Bn1p1/1P2P1Pq/P3Q1BP/2R1NRK1 b - - 0 1", 20, "h3-h2");	// Roc finds at depth 20
	//Test1("8/pp3k2/2p1qp2/2P5/5P2/1R2p1rp/PP2R3/4K2Q b - - 0 1", 11, "e6-e4");				// Roc finds at depth 11
	//Test1("3b2k1/1pp2rpp/r2n1p1B/p2N1q2/3Q4/6R1/PPP2PPP/4R1K1 w - - 0 1", 15, "d5-b4");		// Roc finds at depth 15
	//Test1("3r1rk1/1p3pnp/p3pBp1/1qPpP3/1P1P2R1/P2Q3R/6PP/6K1 w - - 0 1", 24, "h3-h7");		// Roc finds at depth 24
	Test1("4k1rr/ppp5/3b1p1p/4pP1P/3pP2N/3P3P/PPP5/2KR2R1 w kq - 0 1", 16, "g1-g6");		
	Test1("r1b3k1/ppp3pp/2qpp3/2r3N1/2R5/8/P1Q2PPP/2B3K1 b - - 0 1", 4, "g7-g6");
	Test1("4r1k1/p1qr1p2/2pb1Bp1/1p5p/3P1n1R/3B1P2/PP3PK1/2Q4R w - - 0 1", 20, "c1-f4");	// Roc finds at depth 19
	Test1("3r2k1/pp4B1/6pp/PP1Np2n/2Pp1p2/3P2Pq/3QPPbP/R4RK1 b - - 0 1", 32, "g3-f3");		// Roc finds at depth 28
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

#pragma optimize("gy", off)
#define Say(x)	// mute TB query
#pragma warning (disable: 4244)
#pragma warning (disable: 4334)
// Fathom/Syzygy code at end where its #defines cannot screw us up
#if TB
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
