#pragma once

#include "Bit.h"
// true constants, not tuned parameters

typedef sint64 packed_t;
typedef sint16 score_t;

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

constexpr uint64 Filled = 0xFFFFFFFFFFFFFFFF;
constexpr uint64 Interior = 0x007E7E7E7E7E7E00;
constexpr uint64 Boundary = ~Interior;
constexpr uint64 LightArea = 0x55AA55AA55AA55AA;
constexpr uint64 DarkArea = ~LightArea;

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

constexpr uint16 FlagCastling = 0x1000;  // numerically equal to FlagPriority
constexpr uint16 FlagPriority = 0x1000;  // flag desirable quiet moves
constexpr uint16 FlagEP = 0x2000;
constexpr uint16 FlagPKnight = 0x4000;
constexpr uint16 FlagPBishop = 0x6000;
constexpr uint16 FlagPRook = 0xA000;
constexpr uint16 FlagPQueen = 0xC000;

INLINE bool IsPromotion(uint16 move)
{
	return T(move & 0xC000);
}
INLINE bool IsEP(uint16 move)
{
	return (move & 0xF000) == FlagEP;
}
template<bool me> INLINE uint8 Promotion(uint16 move)
{
	uint8 retval = (move & 0xF000) >> 12;
	//	if (retval == WhiteLight && HasBit(DarkArea, To(move)))
	//		retval = WhiteDark;
	return (me ? 1 : 0) + retval;
}

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
constexpr array<int, 16> MatCode = { 0, 0, MatWP, MatBP, MatWN, MatBN, MatWL, MatBL, MatWD, MatBD, MatWR, MatBR, MatWQ, MatBQ, 0, 0 };
constexpr int TotalMat = 2 * (MatWQ + MatBQ) + MatWL + MatBL + MatWD + MatBD + 2 * (MatWR + MatBR + MatWN + MatBN) + 8 * (MatWP + MatBP) + 1;
constexpr int FlagUnusualMaterial = 1 << 30;

constexpr uint64 FileA = 0x0101010101010101;
constexpr array<uint64, 8> File = { FileA, FileA << 1, FileA << 2, FileA << 3, FileA << 4, FileA << 5, FileA << 6, FileA << 7 };
constexpr array<uint64, 8> West = { 0, FileA, FileA * 3, FileA * 7, FileA * 15, FileA * 31, FileA * 63, FileA * 127 };
constexpr array<uint64, 8> East = { FileA * 254, FileA * 252, FileA * 248, FileA * 240, FileA * 224, FileA * 192, FileA * 128, 0 };
constexpr array<uint64, 8> PIsolated = { File[1], File[0] | File[2], File[1] | File[3], File[2] | File[4], File[3] | File[5], File[4] | File[6], File[5] | File[7], File[6] };
constexpr uint64 Line0 = 0x00000000000000FF;
constexpr array<uint64, 8> Line = { Line0, (Line0 << 8), (Line0 << 16), (Line0 << 24), (Line0 << 32), (Line0 << 40), (Line0 << 48), (Line0 << 56) };
