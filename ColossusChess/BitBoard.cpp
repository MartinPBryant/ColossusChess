#include <assert.h>

#include "GlobalTypes.h"
#include "BitBoard.h"

//----------------------------------------------------------------------------------------------------

// Basic population count and bitscan routines

__forceinline uint32_t BitScanForwardPOPCNT(uint64_t bb)
{
	assert(bb != 0);
	return (uint32_t)__popcnt64((bb & (0 - bb)) - 1);
}

__forceinline uint32_t BitScanForwardBSF(uint64_t bb)
{
	assert(bb != 0);
	unsigned long _Index;
	_BitScanForward64(&_Index, bb);
	return (uint32_t)_Index;
}

__forceinline uint32_t BitScanForwardTZCNT(uint64_t bb)
{
	assert(bb != 0);
	return (uint32_t)_tzcnt_u64(bb);
}

__forceinline uint32_t BitScanReverseBSR(uint64_t bb)
{
	assert(bb != 0);
	unsigned long _Index;
	_BitScanReverse64(&_Index, bb);
	return (uint32_t)_Index;
}

__forceinline uint32_t BitScanReverseLZCNT(uint64_t bb)
{
	assert(bb != 0);
	return (uint32_t)_lzcnt_u64(bb);
}

__forceinline int poplsb(uint64_t *bb) {
	int lsb = GetLS1BIndex(*bb);
	*bb &= *bb - 1;
	return lsb;
}

//int getlsb(uint64_t bb) {
//	assert(bb);  // lsb(0) is undefined
//	return __builtin_ctzll(bb);
//}


//----------------------------------------------------------------------------------------------------

// Bitboard manipulation routines (compass points are from white's perspective)
inline uint64_t Rank(int square) { return  0xffULL << (square & 56); }
inline uint64_t File(int square) { return 0x0101010101010101ULL << (square & 7); }
inline uint64_t LeftDiagonal(int square) { const uint64_t longDiagonal = 0x0102040810204080ULL; int diagonal = 56 - 8 * (square & 7) - (square & 56); int north = -diagonal & (diagonal >> 31); int south = diagonal & (-diagonal >> 31); return (longDiagonal >> south) << north; }
inline uint64_t RightDiagonal(int square) { const uint64_t longDiagonal = 0x8040201008040201ULL; int diagonal = 8 * (square & 7) - (square & 56); int north = -diagonal & (diagonal >> 31); int south = diagonal & (-diagonal >> 31); return (longDiagonal >> south) << north; }

inline uint64_t North(uint64_t bb) { return  bb << 8; }
inline uint64_t South(uint64_t bb) { return  bb >> 8; }
inline uint64_t East(uint64_t bb) { return (bb << 1) & NotFileABB; }
inline uint64_t West(uint64_t bb) { return (bb >> 1) & NotFileHBB; }
inline uint64_t NorthEast(uint64_t bb) { return (bb << 9) & NotFileABB; }
inline uint64_t NorthWest(uint64_t bb) { return (bb << 7) & NotFileHBB; }
inline uint64_t SouthEast(uint64_t bb) { return (bb >> 7) & NotFileABB; }
inline uint64_t SouthWest(uint64_t bb) { return (bb >> 9) & NotFileHBB; }

inline uint64_t NorthFill(uint64_t bb) { bb |= (bb << 8); bb |= (bb << 16); bb |= (bb << 32); return bb; }
inline uint64_t SouthFill(uint64_t bb) { bb |= (bb >> 8); bb |= (bb >> 16); bb |= (bb >> 32); return bb; }
inline uint64_t EastFill(uint64_t bb) { const uint64_t pr0 = ~FileABB; const uint64_t pr1 = pr0 & (pr0 << 1); const uint64_t pr2 = pr1 & (pr1 << 2); bb |= pr0 & (bb << 1); bb |= pr1 & (bb << 2); bb |= pr2 & (bb << 4); return bb; }
inline uint64_t WestFill(uint64_t bb) { const uint64_t pr0 = ~FileHBB; const uint64_t pr1 = pr0 & (pr0 >> 1); const uint64_t pr2 = pr1 & (pr1 >> 2); bb |= pr0 & (bb >> 1); bb |= pr1 & (bb >> 2); bb |= pr2 & (bb >> 4); return bb; }
inline uint64_t NorthEastFill(uint64_t bb) { const uint64_t pr0 = ~FileABB; const uint64_t pr1 = pr0 & (pr0 << 9);  const uint64_t pr2 = pr1 & (pr1 << 18); bb |= pr0 & (bb << 9); bb |= pr1 & (bb << 18); bb |= pr2 & (bb << 36); return bb; }
inline uint64_t NorthWestFill(uint64_t bb) { const uint64_t pr0 = ~FileHBB; const uint64_t pr1 = pr0 & (pr0 << 7); const uint64_t pr2 = pr1 & (pr1 << 14); bb |= pr0 & (bb << 7); bb |= pr1 & (bb << 14); bb |= pr2 & (bb << 28); return bb; }
inline uint64_t SouthEastFill(uint64_t bb) { const uint64_t pr0 = ~FileABB; const uint64_t pr1 = pr0 & (pr0 >> 7);  const uint64_t pr2 = pr1 & (pr1 >> 14); bb |= pr0 & (bb >> 7); bb |= pr1 & (bb >> 14); bb |= pr2 & (bb >> 28); return bb; }
inline uint64_t SouthWestFill(uint64_t bb) { const uint64_t pr0 = ~FileHBB; const uint64_t pr1 = pr0 & (pr0 >> 9); const uint64_t pr2 = pr1 & (pr1 >> 18); bb |= pr0 & (bb >> 9); bb |= pr1 & (bb >> 18); bb |= pr2 & (bb >> 36); return bb; }

inline uint64_t EastNorthFill(uint64_t bb) { return East(NorthFill(bb)); }
inline uint64_t WestNorthFill(uint64_t bb) { return West(NorthFill(bb)); }
inline uint64_t EastSouthFill(uint64_t bb) { return East(SouthFill(bb)); }
inline uint64_t WestSouthFill(uint64_t bb) { return West(SouthFill(bb)); }

inline uint64_t NorthSpan(uint64_t bb) { return North(NorthFill(bb)); }
inline uint64_t SouthSpan(uint64_t bb) { return South(SouthFill(bb)); }
inline uint64_t EastNorthSpan(uint64_t bb) { return East(NorthSpan(bb)); }
inline uint64_t WestNorthSpan(uint64_t bb) { return West(NorthSpan(bb)); }
inline uint64_t EastSouthSpan(uint64_t bb) { return East(SouthSpan(bb)); }
inline uint64_t WestSouthSpan(uint64_t bb) { return West(SouthSpan(bb)); }

inline uint64_t FileFill(uint64_t bb) { return NorthFill(bb) | SouthFill(bb); }
inline uint64_t EastFileFill(uint64_t bb) { return East(FileFill(bb)); }
inline uint64_t WestFileFill(uint64_t bb) { return West(FileFill(bb)); }

inline uint64_t noNeighborOnEastFile(uint64_t bb) { return bb & ~WestFileFill(bb); }
inline uint64_t noNeighborOnWestFile(uint64_t bb) { return bb & ~EastFileFill(bb); }

inline uint64_t isolanis(uint64_t bb) { return noNeighborOnEastFile(bb) & noNeighborOnWestFile(bb); }
inline uint64_t halfIsolanis(uint64_t bb) { return noNeighborOnEastFile(bb) ^ noNeighborOnWestFile(bb); }
inline uint64_t openSide1(uint64_t side1Pawns, uint64_t side2Pawns) { return side1Pawns & ~SouthSpan(side2Pawns); }
inline uint64_t openSide2(uint64_t side2Pawns, uint64_t side1Pawns) { return side2Pawns & ~NorthSpan(side1Pawns); }
inline uint64_t backwardSide1(uint64_t side1Pawns, uint64_t side2Pawns) {
	uint64_t stops = North(side1Pawns);
	uint64_t side1AttackSpans = EastNorthSpan(side1Pawns) | WestNorthSpan(side1Pawns);
	uint64_t side2Attacks = SouthEast(side2Pawns) | SouthWest(side2Pawns);
	return South(stops & side2Attacks & ~side1AttackSpans);
}
inline uint64_t backwardSide2(uint64_t side2Pawns, uint64_t side1Pawns) {
	uint64_t stops = South(side2Pawns);
	uint64_t side2AttackSpans = EastSouthSpan(side2Pawns) | WestSouthSpan(side2Pawns);
	uint64_t side1Attacks = NorthEast(side1Pawns) | NorthWest(side1Pawns);
	return North(stops & side1Attacks & ~side2AttackSpans);
}
inline uint64_t stragglerSide1(uint64_t side1Pawns, uint64_t side2Pawns) { return backwardSide1(side1Pawns, side2Pawns) & openSide1(side1Pawns, side2Pawns) & (Rank2BB | Rank3BB); }
inline uint64_t stragglerSide2(uint64_t side2Pawns, uint64_t side1Pawns) { return backwardSide2(side2Pawns, side1Pawns) & openSide2(side2Pawns, side1Pawns) & (Rank7BB | Rank6BB); }

inline uint64_t passedSide1(uint64_t wpawns, uint64_t bpawns) { // Modified so that doubled pawns don't count as two passed pawns
	uint64_t allFrontSpans = SouthSpan(bpawns);
	allFrontSpans |= East(allFrontSpans) | West(allFrontSpans);
	return wpawns & ~allFrontSpans & ~SouthSpan(wpawns);
}
inline uint64_t passedSide2(uint64_t bpawns, uint64_t wpawns) {
	uint64_t allFrontSpans = NorthSpan(wpawns);
	allFrontSpans |= East(allFrontSpans) | West(allFrontSpans);
	return bpawns & ~allFrontSpans & ~NorthSpan(bpawns);
}

inline uint64_t closedFiles(uint64_t bb1, uint64_t bb2) { return FileFill(bb1) & FileFill(bb2); }
inline uint64_t openFiles(uint64_t bb1, uint64_t bb2) { return ~FileFill(bb1) & ~FileFill(bb2); }
inline uint64_t halfOpenOrOpenFiles(uint64_t bb) { return ~FileFill(bb); }
inline uint64_t halfOpenFile(uint64_t bb1, uint64_t bb2) { return halfOpenOrOpenFiles(bb1) ^ openFiles(bb1, bb2); }

inline uint8_t fileSet(uint64_t bb) { return (uint8_t)SouthFill(bb); }

inline bool Aligned(int square1, int square2, int square3) { return LineListBB[square1][square2] & CreateBitboardFromSquare(square3); }

//----------------------------------------------------------------------------------------------------

alignas(64) const uint64_t KingSafetyFiles[8] = { (FileABB | FileBBB | FileCBB), (FileABB | FileBBB | FileCBB), (FileBBB | FileCBB | FileDBB), (FileCBB | FileDBB | FileEBB), (FileDBB | FileEBB | FileFBB), (FileEBB | FileFBB | FileGBB), (FileFBB | FileGBB | FileHBB), (FileFBB | FileGBB | FileHBB) };
alignas(64) const uint64_t FilesBB[8] = { FileABB, FileBBB, FileCBB, FileDBB, FileEBB, FileFBB, FileGBB, FileHBB };

alignas(64) uint64_t RanksListBB[64];
alignas(64) uint64_t FilesListBB[64];
alignas(64) uint64_t LeftDiagonalsListBB[64];
alignas(64) uint64_t RightDiagonalsListBB[64];
alignas(64) uint64_t LineListBB[64][64]; // e.g. LineListBB[D2][D6] gives the squares D1 - D8
alignas(64) uint64_t BetweenListBB[64][64]; // e.g. BetweenListBB[D2][D6] gives the squares D3 - D5

alignas(64) uint64_t KingAttacksBBList[64];
alignas(64) uint64_t KnightAttacksBBList[64];
alignas(64) uint64_t PawnAttacksBBList[Sides][64];

alignas(64) uint64_t AttacksByPieceBBList[King + 2][64];
alignas(64) int8_t DirectAttacksByPiece[8][64][64];

alignas(64) uint64_t PassedPawnCatchableByKing[2][2][64]; // side to move, king colour, king square

#if !defined _WIN64 || defined TB_NO_HW_POP_COUNT
int Ms1b[256];
void init_Ms1b()
{
	int i;
	for (i = 0; i < 256; i++) {
		Ms1b[i] = (
			(i > 127) ? 7 :
			(i > 63) ? 6 :
			(i > 31) ? 5 :
			(i > 15) ? 4 :
			(i > 7) ? 3 :
			(i > 3) ? 2 :
			(i > 1) ? 1 :
			0
			);
	};
}

uint8_t PopulationCount16[1 << 16];
void InitialisePopulationCount16()
{
	for (int index = 0; index < (1 << 16); index++)
		PopulationCount16[index] = uint8_t(std::bitset<16>(index).count());
}

#endif

// Return a bitboard containing all the squares attacked by the king in the provided bitboard
// This is only used here to initialise the KingAttacksBBList array
uint64_t KingAttacksBB(uint64_t kingSquareBB)
{
	uint64_t attacksBB = East(kingSquareBB) | West(kingSquareBB);
	kingSquareBB |= attacksBB;
	attacksBB |= North(kingSquareBB) | South(kingSquareBB);
	return attacksBB;
}

// Return a bitboard containing all the squares attacked by the knight(s) in the provided bitboard
// This is only used here to initialise the KnightAttacksBBList array
uint64_t KnightAttacksBB(uint64_t knightSquareBB)
{
	uint64_t attacksBB, eastBB, westBB;
	eastBB = East(knightSquareBB);
	westBB = West(knightSquareBB);
	attacksBB = (eastBB | westBB) << 16;
	attacksBB |= (eastBB | westBB) >> 16;
	eastBB = East(eastBB);
	westBB = West(westBB);
	attacksBB |= (eastBB | westBB) << 8;
	attacksBB |= (eastBB | westBB) >> 8;
	return attacksBB;
}

// Return a bitboard containing all the squares attacked by the pawn(s) in the provided bitboard
inline uint64_t Side0PawnAttacksBB(uint64_t pawnSquareBB)
{
	return NorthEast(pawnSquareBB) | NorthWest(pawnSquareBB);
}

// Return a bitboard containing all the squares attacked by the pawn(s) in the provided bitboard
inline uint64_t Side1PawnAttacksBB(uint64_t pawnSquareBB)
{
	return SouthEast(pawnSquareBB) | SouthWest(pawnSquareBB);
}

// 'Fancy' magic bitboards (862208 bytes : About 2.74 times smaller and about 2.5% faster than 'plain')
// I tried putting the AttacksPointer, InnerRay, MagicMultiplier and BlockerPermutationBitsPreAdjusted into a struct (to try to save array indexing) but it was slower! :O
alignas(64) uint64_t RookAttacksFancyBB[102400]; // 819200 = 800KB
alignas(64) uint64_t BishopAttacksFancyBB[5248]; // 41984 = 41KB
alignas(64) uint64_t* RookAttacksFancyPointer[64]; // 512 // 64 pointers to the relevant part of the RookAttacksFancyBB array above
alignas(64) uint64_t* BishopAttacksFancyPointer[64]; // 512 // 64 pointers to the relevant part of the BishopAttacksFancyBB array above

alignas(64) uint64_t RookInnerRays[64];
alignas(64) uint64_t BishopInnerRays[64];

// The sum of moves in all the rays (not counting the last step of the ray)
// 2^12 = 4096 (i.e. the number of blocker permutations for a rook on e.g. A1) x 64 squares x 8 bytes (i.e. 1 uint64_t) = 2MB
alignas(64)
const int RookBlockerPermutationBits[64] = {
  12, 11, 11, 11, 11, 11, 11, 12,
  11, 10, 10, 10, 10, 10, 10, 11,
  11, 10, 10, 10, 10, 10, 10, 11,
  11, 10, 10, 10, 10, 10, 10, 11,
  11, 10, 10, 10, 10, 10, 10, 11,
  11, 10, 10, 10, 10, 10, 10, 11,
  11, 10, 10, 10, 10, 10, 10, 11,
  12, 11, 11, 11, 11, 11, 11, 12
};
alignas(64)
const int RookBlockerPermutationBitsPreAdjusted[64] = {
  64 - 12, 64 - 11, 64 - 11, 64 - 11, 64 - 11, 64 - 11, 64 - 11, 64 - 12,
  64 - 11, 64 - 10, 64 - 10, 64 - 10, 64 - 10, 64 - 10, 64 - 10, 64 - 11,
  64 - 11, 64 - 10, 64 - 10, 64 - 10, 64 - 10, 64 - 10, 64 - 10, 64 - 11,
  64 - 11, 64 - 10, 64 - 10, 64 - 10, 64 - 10, 64 - 10, 64 - 10, 64 - 11,
  64 - 11, 64 - 10, 64 - 10, 64 - 10, 64 - 10, 64 - 10, 64 - 10, 64 - 11,
  64 - 11, 64 - 10, 64 - 10, 64 - 10, 64 - 10, 64 - 10, 64 - 10, 64 - 11,
  64 - 11, 64 - 10, 64 - 10, 64 - 10, 64 - 10, 64 - 10, 64 - 10, 64 - 11,
  64 - 12, 64 - 11, 64 - 11, 64 - 11, 64 - 11, 64 - 11, 64 - 11, 64 - 12
};

// 2^9 = 512 (i.e. the number of blocker permutations for a bishop on e.g. D4) x 64 squares x 8 bytes (i.e. 1 uint64_t) = 256KB
alignas(64)
const int BishopBlockerPermutationBits[64] = {
  6, 5, 5, 5, 5, 5, 5, 6,
  5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 7, 7, 7, 7, 5, 5,
  5, 5, 7, 9, 9, 7, 5, 5,
  5, 5, 7, 9, 9, 7, 5, 5,
  5, 5, 7, 7, 7, 7, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5,
  6, 5, 5, 5, 5, 5, 5, 6
};
alignas(64)
const int BishopBlockerPermutationBitsPreAdjusted[64] = {
  64 - 6, 64 - 5, 64 - 5, 64 - 5, 64 - 5, 64 - 5, 64 - 5, 64 - 6,
  64 - 5, 64 - 5, 64 - 5, 64 - 5, 64 - 5, 64 - 5, 64 - 5, 64 - 5,
  64 - 5, 64 - 5, 64 - 7, 64 - 7, 64 - 7, 64 - 7, 64 - 5, 64 - 5,
  64 - 5, 64 - 5, 64 - 7, 64 - 9, 64 - 9, 64 - 7, 64 - 5, 64 - 5,
  64 - 5, 64 - 5, 64 - 7, 64 - 9, 64 - 9, 64 - 7, 64 - 5, 64 - 5,
  64 - 5, 64 - 5, 64 - 7, 64 - 7, 64 - 7, 64 - 7, 64 - 5, 64 - 5,
  64 - 5, 64 - 5, 64 - 5, 64 - 5, 64 - 5, 64 - 5, 64 - 5, 64 - 5,
  64 - 6, 64 - 5, 64 - 5, 64 - 5, 64 - 5, 64 - 5, 64 - 5, 64 - 6
};

alignas(64)
const uint64_t RookMagicMultipliers[64] = {
  0x80004000976080ULL,
  0x1040400010002000ULL,
  0x4880200210000980ULL,
  0x5280080010000482ULL,
  0x200040200081020ULL,
  0x2100080100020400ULL,
  0x4280008001000200ULL,
  0x1000a4425820300ULL,
  0x29002100800040ULL,
  0x4503400040201004ULL,
  0x209002001004018ULL,
  0x1131000a10002100ULL,
  0x9000800120500ULL,
  0x10e001804820010ULL,
  0x29000402000100ULL,
  0x2002000d01c40292ULL,
  0x80084000200c40ULL,
  0x10004040002002ULL,
  0x201030020004014ULL,
  0x80012000a420020ULL,
  0x129010008001204ULL,
  0x6109010008040002ULL,
  0x950010100020004ULL,
  0x803a0000c50284ULL,
  0x80004100210080ULL,
  0x200240100140ULL,
  0x20004040100800ULL,
  0x4018090300201000ULL,
  0x4802010a00102004ULL,
  0x2001000900040002ULL,
  0x4a02104001002a8ULL,
  0x2188108200204401ULL,
  0x40400020800080ULL,
  0x880402000401004ULL,
  0x10040800202000ULL,
  0x604410a02001020ULL,
  0x200200206a001410ULL,
  0x86000400810080ULL,
  0x428200040600080bULL,
  0x2001000041000082ULL,
  0x80002000484000ULL,
  0x210002002c24000ULL,
  0x401a200100410014ULL,
  0x5021000a30009ULL,
  0x218000509010010ULL,
  0x4000400410080120ULL,
  0x20801040010ULL,
  0x29040040820011ULL,
  0x4080400024800280ULL,
  0x500200040100440ULL,
  0x2880142001004100ULL,
  0x412020400a001200ULL,
  0x18c028004080080ULL,
  0x884001020080401ULL,
  0x210810420400ULL,
  0x801048745040200ULL,
  0x4401002040120082ULL,
  0x408200210012ULL,
  0x110008200441ULL,
  0x2010002004100901ULL,
  0x801000800040211ULL,
  0x480d000400820801ULL,
  0x820104201280084ULL,
  0x1001040311802142ULL,
};

alignas(64)
const uint64_t BishopMagicMultipliers[64] = {
  0x1024b002420160ULL,
  0x1008080140420021ULL,
  0x2012080041080024ULL,
  0xc282601408c0802ULL,
  0x2004042000000002ULL,
  0x12021004022080ULL,
  0x880414820100000ULL,
  0x4501002211044000ULL,
  0x20402222121600ULL,
  0x1081088a28022020ULL,
  0x1004c2810851064ULL,
  0x2040080841004918ULL,
  0x1448020210201017ULL,
  0x4808110108400025ULL,
  0x10504404054004ULL,
  0x800010422092400ULL,
  0x40000870450250ULL,
  0x402040408080518ULL,
  0x1000980a404108ULL,
  0x1020804110080ULL,
  0x8200c02082005ULL,
  0x40802009a0800ULL,
  0x1000201012100ULL,
  0x111080200820180ULL,
  0x904122104101024ULL,
  0x4008200405244084ULL,
  0x44040002182400ULL,
  0x4804080004021002ULL,
  0x6401004024004040ULL,
  0x404010001300a20ULL,
  0x428020200a20100ULL,
  0x300460100420200ULL,
  0x404200c062000ULL,
  0x22101400510141ULL,
  0x104044400180031ULL,
  0x2040040400280211ULL,
  0x8020400401010ULL,
  0x20100110401a0040ULL,
  0x100101005a2080ULL,
  0x1a008300042411ULL,
  0x120a025004504000ULL,
  0x4001084242101000ULL,
  0xa020202010a4200ULL,
  0x4000002018000100ULL,
  0x80104000044ULL,
  0x1004009806004043ULL,
  0x100401080a000112ULL,
  0x1041012101000608ULL,
  0x40400c250100140ULL,
  0x80a10460a100002ULL,
  0x2210030401240002ULL,
  0x6040aa108481b20ULL,
  0x4009004050410002ULL,
  0x8106003420200e0ULL,
  0x1410500a08206000ULL,
  0x92548802004000ULL,
  0x1040041241028ULL,
  0x120042025011ULL,
  0x8060104054400ULL,
  0x20004404020a0a01ULL,
  0x40008010020214ULL,
  0x4000050209802c1ULL,
  0x208244210400ULL,
  0x10140848044010ULL,
};

// Return a bitboard containing all the squares attacked by the rook on the provided square with the provided blockers
// This is used by the move generators
uint64_t RookAttacksBB(int square, uint64_t occupiedSquaresBB)
{
	occupiedSquaresBB &= RookInnerRays[square]; // Get the relevant blockers for the rook on 'square'
	occupiedSquaresBB *= RookMagicMultipliers[square]; // The multiplication and shift give us an index into the table of pre-calculated moves from the square with the relevant blockers
	occupiedSquaresBB >>= RookBlockerPermutationBitsPreAdjusted[square];
	return *(RookAttacksFancyPointer[square] + occupiedSquaresBB);

	//uint64_t blockers, index1, index2,index3;
	//blockers = occupiedSquaresBB & RookInnerRays[square]; // Get the relevant blockers for the rook on 'square'
	//index1 = blockers * RookMagicMultipliers[square] >> RookBlockerPermutationBitsPreAdjusted[square];
	////return *(RookAttacksFancyPointer[square] + index);

	//index2 = _pext_u64(occupiedSquaresBB, 64 - 12);
	//index3 = _pext_u64(occupiedSquaresBB, RookInnerRays[square]);
	//if (index1 != index2)
	//	occupiedSquaresBB = 0;
	//if (index1 != index3)
	//	occupiedSquaresBB = 0;
	//return *(RookAttacksFancyPointer[square] + index1);
}

// Return a bitboard containing all the squares attacked by the bishop on the provided square with the provided blockers
// This is used by the move generators
uint64_t BishopAttacksBB(int square, uint64_t occupiedSquaresBB)
{
	occupiedSquaresBB &= BishopInnerRays[square]; // Get the relevant blockers for the bishop on 'square'
	occupiedSquaresBB *= BishopMagicMultipliers[square];
	occupiedSquaresBB >>= BishopBlockerPermutationBitsPreAdjusted[square];
	return *(BishopAttacksFancyPointer[square] + occupiedSquaresBB);
}

// Generate the inner rays bitboard for a rook on the given square, e.g. for f6 ...
// . . . . . . . .
// . . . . . 1 . .
// . 1 1 1 1 . 1 .
// . . . . . 1 . .
// . . . . . 1 . .
// . . . . . 1 . .
// . . . . . 1 . .
// . . . . . . . .
// This is only used here to initialise the RookInnerRays array
uint64_t GenerateRookInnerRay(int square)
{
	uint64_t resultBB = 0ULL;
	int rank = square / 8, file = square % 8, r, f;
	for (r = rank + 1; r <= 6; r++) resultBB |= (1ULL << (file + r * 8));
	for (r = rank - 1; r >= 1; r--) resultBB |= (1ULL << (file + r * 8));
	for (f = file + 1; f <= 6; f++) resultBB |= (1ULL << (f + rank * 8));
	for (f = file - 1; f >= 1; f--) resultBB |= (1ULL << (f + rank * 8));
	return resultBB;
}

// Generate the inner rays bitboard for a bishop on the given square
// This is only used here to initialise the BishopInnerRays array
uint64_t GenerateBishopInnerRay(int square)
{
	uint64_t resultBB = 0ULL;
	int rank = square / 8, file = square % 8, r, f;
	for (r = rank + 1, f = file + 1; r <= 6 && f <= 6; r++, f++) resultBB |= (1ULL << (f + r * 8));
	for (r = rank + 1, f = file - 1; r <= 6 && f >= 1; r++, f--) resultBB |= (1ULL << (f + r * 8));
	for (r = rank - 1, f = file + 1; r >= 1 && f <= 6; r--, f++) resultBB |= (1ULL << (f + r * 8));
	for (r = rank - 1, f = file - 1; r >= 1 && f >= 1; r--, f--) resultBB |= (1ULL << (f + r * 8));
	return resultBB;
}

uint64_t GenerateRookAttacks(int square, uint64_t blockersBB) {
	uint64_t resultBB = 0ULL;
	int rank = square / 8, file = square % 8, r, f;
	for (r = rank + 1; r <= 7; r++) {
		resultBB |= (1ULL << (file + r * 8));
		if (blockersBB & (1ULL << (file + r * 8))) break;
	}
	for (r = rank - 1; r >= 0; r--) {
		resultBB |= (1ULL << (file + r * 8));
		if (blockersBB & (1ULL << (file + r * 8))) break;
	}
	for (f = file + 1; f <= 7; f++) {
		resultBB |= (1ULL << (f + rank * 8));
		if (blockersBB & (1ULL << (f + rank * 8))) break;
	}
	for (f = file - 1; f >= 0; f--) {
		resultBB |= (1ULL << (f + rank * 8));
		if (blockersBB & (1ULL << (f + rank * 8))) break;
	}
	return resultBB;
}

uint64_t GenerateBishopAttacks(int square, uint64_t blockersBB) {
	uint64_t resultBB = 0ULL;
	int rank = square / 8, file = square % 8, r, f;
	for (r = rank + 1, f = file + 1; r <= 7 && f <= 7; r++, f++) {
		resultBB |= (1ULL << (f + r * 8));
		if (blockersBB & (1ULL << (f + r * 8))) break;
	}
	for (r = rank + 1, f = file - 1; r <= 7 && f >= 0; r++, f--) {
		resultBB |= (1ULL << (f + r * 8));
		if (blockersBB & (1ULL << (f + r * 8))) break;
	}
	for (r = rank - 1, f = file + 1; r >= 0 && f <= 7; r--, f++) {
		resultBB |= (1ULL << (f + r * 8));
		if (blockersBB & (1ULL << (f + r * 8))) break;
	}
	for (r = rank - 1, f = file - 1; r >= 0 && f >= 0; r--, f--) {
		resultBB |= (1ULL << (f + r * 8));
		if (blockersBB & (1ULL << (f + r * 8))) break;
	}
	return resultBB;
}

uint64_t PermutationIndexToBB(int permutationIndex, int blockerPermutationBits, uint64_t innerRays) { // index: 0 .. 4095 (12 bits)
	int i, j;
	uint64_t result = 0ULL;
	for (i = 0; i < blockerPermutationBits; i++) { // e.g. 0 .. 11
		j = GetLS1BIndex(innerRays);
		ClearLS1B(innerRays);
		if (permutationIndex & (1 << i)) result |= (1ULL << j);
	}
	return result;
}

inline uint64_t FlipVertical(uint64_t bb) {
	const uint64_t k1 = 0x00FF00FF00FF00FFULL;
	const uint64_t k2 = 0x0000FFFF0000FFFFULL;
	bb = ((bb >> 8) & k1) | ((bb & k1) << 8);
	bb = ((bb >> 16) & k2) | ((bb & k2) << 16);
	bb = (bb >> 32) | (bb << 32);
	return bb;
}

bool PassedPawnCatchableByKingMirrored()
{
	bool result = true;

	for (int square = A1; square <= H8; square++)
	{
		if (PassedPawnCatchableByKing[0][0][square] != FlipVertical(PassedPawnCatchableByKing[1][1][square ^ 56]))
			result = false;
		if (PassedPawnCatchableByKing[0][1][square] != FlipVertical(PassedPawnCatchableByKing[1][0][square ^ 56]))
			result = false;
	}

	return result;
}

void InitialiseBitBoardLists()
{
	// Initialise attack lists (one off)
	std::memset(AttacksByPieceBBList, 0, sizeof(AttacksByPieceBBList));

	int rookFancyIndex = 0;
	int bishopFancyIndex = 0;
	for (int square = A1; square <= H8; square++)
	{
		uint64_t squareBB = CreateBitboardFromSquare(square);

		if (square < A8)
			PawnAttacksBBList[0][square] = Side0PawnAttacksBB(squareBB);
		else
			PawnAttacksBBList[0][square] = 0;
		if (square > H1)
			PawnAttacksBBList[1][square] = Side1PawnAttacksBB(squareBB);
		else
			PawnAttacksBBList[1][square] = 0;

		KnightAttacksBBList[square] = KnightAttacksBB(squareBB);
		AttacksByPieceBBList[Knight][square] = KnightAttacksBBList[square];

		KingAttacksBBList[square] = KingAttacksBB(squareBB);
		AttacksByPieceBBList[King][square] = KingAttacksBBList[square];

		RookInnerRays[square] = GenerateRookInnerRay(square);
		RookAttacksFancyPointer[square] = &RookAttacksFancyBB[rookFancyIndex];
		for (int permutationIndex = 0; permutationIndex < (1 << RookBlockerPermutationBits[square]); permutationIndex++) // 1 << (max)12 = 4096
		{
			uint64_t blockersBB;
			blockersBB = PermutationIndexToBB(permutationIndex, RookBlockerPermutationBits[square], RookInnerRays[square]);
			int keyIndex;
			keyIndex = (int)((blockersBB * RookMagicMultipliers[square]) >> (64 - RookBlockerPermutationBits[square]));
			*(RookAttacksFancyPointer[square] + keyIndex) = GenerateRookAttacks(square, blockersBB);
			rookFancyIndex++;
		}

		BishopInnerRays[square] = GenerateBishopInnerRay(square);
		BishopAttacksFancyPointer[square] = &BishopAttacksFancyBB[bishopFancyIndex];
		for (int permutationIndex = 0; permutationIndex < (1 << BishopBlockerPermutationBits[square]); permutationIndex++) // 1 << (max)9 = 512
		{
			uint64_t blockersBB;
			blockersBB = PermutationIndexToBB(permutationIndex, BishopBlockerPermutationBits[square], BishopInnerRays[square]);
			int keyIndex;
			keyIndex = (int)((blockersBB * BishopMagicMultipliers[square]) >> (64 - BishopBlockerPermutationBits[square]));
			*(BishopAttacksFancyPointer[square] + keyIndex) = GenerateBishopAttacks(square, blockersBB);
			bishopFancyIndex++;
		}

		AttacksByPieceBBList[Bishop][square] = BishopAttacksBB(square, 0);
		AttacksByPieceBBList[Rook][square] = RookAttacksBB(square, 0);
		AttacksByPieceBBList[Queen][square] = BishopAttacksBB(square, 0) | RookAttacksBB(square, 0);

		// White to move, black king catching white pawns
		// The black king can't catch any pawns on a higher rank
		// The black king can't catch any pawns if it is on rank 0 or 1

		int kingRank = square >> 3;
		int movesToPromotionRank = 7 - kingRank;
		uint64_t bb = CreateBitboardFromSquare(square);
		for (int i = 0; i < movesToPromotionRank; i++)
			bb |= West(bb) | East(bb); // The king can catch pawns to either side by the same number of moves to the promotion rank
		if (kingRank < 2) // If the king is on the 1st or 2nd rank it can't catch any pawns so clear the bitboard BUG FIX!
			bb = 0;
		for (int i = 0; i < kingRank - 2; i++) // Fill in a pyramid down the board to the 3rd rank
			bb |= South(West(bb) | bb | East(bb));
		bb |= South(bb);// | 0xff; // Fill in the 2nd rank the same as the 3rd rank and fill in the 1st rank for completeness WHY???


		PassedPawnCatchableByKing[0][1][square] = bb;

		// Black to move, white king catching black pawns is just a reflection
		PassedPawnCatchableByKing[1][0][square ^ 56] = FlipVertical(bb);
	}
	
	// Passed pawn runners
	for (int square = A1; square <= H8; square++)
	{
		int square2;

		// Black to move, black king catching white pawns
		
		square2 = square;
		if (square < 56)
			square2 += 8; // If it's not already on the promotion rank then it can advance
		// Now it can capture the union of the current square and the squares to each side
		PassedPawnCatchableByKing[1][1][square] = PassedPawnCatchableByKing[0][1][square2] | CreateBitboardFromSquare(square2); // Also have to 'or' in the square itself to handle when the king is on the 1st rank
		if (square % 8 > 0)
			PassedPawnCatchableByKing[1][1][square] |= PassedPawnCatchableByKing[0][1][square2 - 1] | CreateBitboardFromSquare(square2 - 1);
		if (square % 8 < 7)
			PassedPawnCatchableByKing[1][1][square] |= PassedPawnCatchableByKing[0][1][square2 + 1] | CreateBitboardFromSquare(square2 + 1);

		PassedPawnCatchableByKing[0][0][square ^ 56] = FlipVertical(PassedPawnCatchableByKing[1][1][square]);
	}
	assert(PassedPawnCatchableByKingMirrored());

	// Initialise helper bitmap lists
	for (int squareIndex1 = A1; squareIndex1 <= H8; squareIndex1++)
	{
		RanksListBB[squareIndex1] = Rank(squareIndex1);
		FilesListBB[squareIndex1] = File(squareIndex1);
		LeftDiagonalsListBB[squareIndex1] = LeftDiagonal(squareIndex1);
		RightDiagonalsListBB[squareIndex1]= RightDiagonal(squareIndex1);

		for (int squareIndex2 = A1; squareIndex2 <= H8; squareIndex2++)
		{
			LineListBB[squareIndex1][squareIndex2] = 0;
			BetweenListBB[squareIndex1][squareIndex2] = 0;
			if ((squareIndex1 != squareIndex2) && ((LeftDiagonalsListBB[squareIndex1] | RightDiagonalsListBB[squareIndex1]) & CreateBitboardFromSquare(squareIndex2)))
			{
				LineListBB[squareIndex1][squareIndex2] = (BishopAttacksBB(squareIndex1, 0) & BishopAttacksBB(squareIndex2, 0)) | CreateBitboardFromSquare(squareIndex1) | CreateBitboardFromSquare(squareIndex2);
				BetweenListBB[squareIndex1][squareIndex2] = BishopAttacksBB(squareIndex1, CreateBitboardFromSquare(squareIndex2)) & BishopAttacksBB(squareIndex2, CreateBitboardFromSquare(squareIndex1));
			}
			if ((squareIndex1 != squareIndex2) && ((RanksListBB[squareIndex1] | FilesListBB[squareIndex1]) & CreateBitboardFromSquare(squareIndex2)))
			{
				LineListBB[squareIndex1][squareIndex2] = RookAttacksBB(squareIndex1, 0) & RookAttacksBB(squareIndex2, 0) | CreateBitboardFromSquare(squareIndex1) | CreateBitboardFromSquare(squareIndex2);
				BetweenListBB[squareIndex1][squareIndex2] = RookAttacksBB(squareIndex1, CreateBitboardFromSquare(squareIndex2)) & RookAttacksBB(squareIndex2, CreateBitboardFromSquare(squareIndex1));
			}
		}
	}

	// Possible direct checks NOT FINISHED!
	for (int piece = Pawn; piece < King; piece++)
	{
		for (int attackedSquare = A1; attackedSquare <= H8; attackedSquare++)
		{
			for (int square = A1; square <= H8; square++)
			{
				int8_t v = 0;
				switch (piece)
				{
				case Pawn:
					break;
				case Knight:
					if (KnightAttacksBBList[square] & CreateBitboardFromSquare(attackedSquare))
						v = 2;
					break;
				case Bishop:
					break;
				case Rook:
					break;
				case Queen:
					break;
				case King:
					if (KingAttacksBBList[square] & CreateBitboardFromSquare(attackedSquare))
						v = 2;
					break;
				}
				DirectAttacksByPiece[piece][attackedSquare][square] = v;
			}
		}
	}


#if !defined _WIN64 || defined TB_NO_HW_POP_COUNT
	// Initialise array for software bit scan reverse
	init_Ms1b();
	InitialisePopulationCount16();
#endif
}
