#pragma once

#include "GlobalTypes.h"

//----------------------------------------------------------------------------------------------------

extern uint64_t RanksListBB[64];
extern uint64_t FilesListBB[64];
extern uint64_t LeftDiagonalsListBB[64];
extern uint64_t RightDiagonalsListBB[64];
extern uint64_t LineListBB[64][64];
extern uint64_t BetweenListBB[64][64];

// Piece attack arrays and routines
extern uint64_t KingAttacksBBList[64];
extern uint64_t KnightAttacksBBList[64];
extern uint64_t PawnAttacksBBList[Sides][64];

extern uint64_t AttacksByPieceBBList[King + 2][64];

extern uint64_t PassedPawnCatchableByKing[2][2][64];

extern uint64_t Side0PawnAttacksBB(uint64_t pawnSquareBB);
extern uint64_t Side1PawnAttacksBB(uint64_t pawnSquareBB);

extern uint64_t RookAttacksBB(int square, uint64_t occupiedSquaresBB);
extern uint64_t BishopAttacksBB(int square, uint64_t occupiedSquaresBB);

extern void InitialiseBitBoardLists();

extern uint64_t RankFilePinnersBB(int square, int sideToMove);
extern uint64_t DiagonalPinnersBB(int square, int sideToMove);
extern uint64_t RankFileDiscovereesBB(int enemyKingSquare, int sideToMove);
extern uint64_t DiagonalDiscovereesBB(int enemyKingSquare, int sideToMove);

extern const uint64_t FilesBB[8];

//----------------------------------------------------------------------------------------------------

// Various board set masks
#define	Rank1BB	0x00000000000000FFULL
#define	Rank2BB	0x000000000000FF00ULL
#define	Rank3BB	0x0000000000FF0000ULL
#define	Rank4BB	0x00000000FF000000ULL
#define	Rank5BB	0x000000FF00000000ULL
#define	Rank6BB	0x0000FF0000000000ULL
#define	Rank7BB	0x00FF000000000000ULL
#define	Rank8BB	0xFF00000000000000ULL
#define	Rank1AndRank8BB	0xFF000000000000FFULL
#define	FileABB	0x0101010101010101ULL
#define	FileBBB	0x0202020202020202ULL
#define	FileCBB	0x0404040404040404ULL
#define	FileDBB	0x0808080808080808ULL
#define	FileEBB	0x1010101010101010ULL
#define	FileFBB	0x2020202020202020ULL
#define	FileGBB	0x4040404040404040ULL
#define	FileHBB	0x8080808080808080ULL
#define	NotFileABB	0xfefefefefefefefeULL
#define	NotFileHBB	0x7f7f7f7f7f7f7f7fULL
#define	diagonalA1H8BB	0x8040201008040201
#define	diagonalB1H7BB	0x0080402010080402
#define	diagonalH1A8BB	0x0102040810204080
#define	diagonalG1A7BB	0x0001020408102040
#define	LightBB	0x55AA55AA55AA55AA
#define	DarkBB	0xAA55AA55AA55AA55
#define	CentreBB	0x0000001818000000
#define	OuterCentreBB	0x00003C24243C0000
#define	InnerEdgesBB	0x007E424242427E00
#define	EdgesBB	0xFF818181818181FF
#define	CornersBB	0x8100000000000081
#define	InnerCornersBB	0x0042000000004200
#define	BottomLeft4CornerBB	0x0000000000000303
#define	BottomRight4CornerBB	0x000000000000C0C0
#define	TopLeft4CornerBB	0x0303000000000000
#define	TopRight4CornerBB	0xC0C0000000000000
#define A2B2C2BB 0x0000000000000700
#define A3B3C3BB 0x0000000000070000
#define F2G2H2BB 0x000000000000E000
#define F3G3H3BB 0x0000000000E00000
#define A7B7C7BB 0x0007000000000000
#define A6B6C6BB 0x0000070000000000
#define F7G7H7BB 0x00E0000000000000
#define F6G6H6BB 0x0000E00000000000
#define A1B1A2BB 0x0000000000000103
#define G1H1H2BB 0x00000000000080C0
#define A8B8A7BB 0x0301000000000000
#define G8H8H7BB 0xC080000000000000
#define B1C1BB 0x0000000000000006
#define F1G1BB 0x0000000000000060
#define B8C8BB 0x0600000000000000
#define F8G8BB 0x6000000000000000
#define A7H7A6H6BB 0x0081810000000000
#define A2H2A3H3BB 0x0000000000818100
#define Side0HalfBB 0x00000000FFFFFFFF
#define Side1HalfBB 0xFFFFFFFF00000000

//----------------------------------------------------------------------------------------------------

//#define x64

#if defined _WIN64 && !defined TB_NO_HW_POP_COUNT

// 64-bit hardware based routines
#define PopulationCountX(ui64) PopulationCountHardware(ui64)
#define BitScanForwardX(ui64) BitScanForwardHardwarePOPCNT(ui64)
#define BitScanReverseX(ui64) BitScanReverseBSR(ui64)

extern uint32_t PopulationCountHardware(uint64_t ui64); // Fastest

extern unsigned long BitScanForwardHardwareBSF(uint64_t ui64); // Marginally slower than BitScanForwardHardwarePOPCNT

extern uint32_t BitScanForwardHardwarePOPCNT(uint64_t ui64); // Fastest

extern uint32_t BitScanReverseBSR(uint64_t ui64); // Fastest

extern int poplsb(uint64_t *bb);

//extern int getlsb(uint64_t bb);

#else

// Software based routines (used for 32-bit or very old 64-bit processors)
#define PopulationCountX(ui64) PopulationCountByArray(ui64)
#define BitScanForwardX(ui64) BitScanForwardDeBruijn(ui64)
#define BitScanReverseX(ui64) BitScanReverseMs1b(ui64)

extern int PopulationCountBrianKernighan(uint64_t ui64); // Slowest but simplest s/w

extern uint64_t PopulationCountSWAR(uint64_t ui64); // Nearly fastest s/w

extern uint64_t PopulationCountByArray(uint64_t ui64); // Fastest (just) s/w

extern int BitScanForwardDeBruijn(uint64_t ui64); // Fastest s/w

extern int BitScanReverseDeBruijn(uint64_t ui64);

extern int BitScanReverseMs1b(uint64_t ui64); // Fastest s/w

#endif

//----------------------------------------------------------------------------------------------------

// Commonly used 64-bit macros
#define CUINT64(constantUINT64) constantUINT64##ULL
#define UINT64SetBit(i) (1ULL << (i))
#define ClearLS1B(bb) (bb &= (bb - 1))
//#define ClearLS1B(bb) (bb = _blsr_u64(bb)) // This BMI instruction is no better than the above

//----------------------------------------------------------------------------------------------------

// Bitboard manipulation routines (compass points are from white's perspective)
extern uint64_t Rank(int square);
extern uint64_t File(int square);
extern uint64_t LeftDiagonal(int square);
extern uint64_t RightDiagonal(int square);

extern uint64_t North(uint64_t bb);
extern uint64_t South(uint64_t bb);
extern uint64_t East(uint64_t bb);
extern uint64_t West(uint64_t bb);
extern uint64_t NorthEast(uint64_t bb);
extern uint64_t NorthWest(uint64_t bb);
extern uint64_t SouthEast(uint64_t bb);
extern uint64_t SouthWest(uint64_t bb);

extern uint64_t NorthFill(uint64_t bb);
extern uint64_t SouthFill(uint64_t bb);
extern uint64_t EastFill(uint64_t bb);
extern uint64_t WestFill(uint64_t bb);
extern uint64_t NorthEastFill(uint64_t bb);
extern uint64_t NorthWestFill(uint64_t bb);
extern uint64_t SouthEastFill(uint64_t bb);
extern uint64_t SouthWestFill(uint64_t bb);

extern uint64_t EastNorthFill(uint64_t bb);
extern uint64_t WestNorthFill(uint64_t bb);
extern uint64_t EastSouthFill(uint64_t bb);
extern uint64_t WestSouthFill(uint64_t bb);

extern uint64_t NorthSpan(uint64_t bb);
extern uint64_t SouthSpan(uint64_t bb);
extern uint64_t EastNorthSpan(uint64_t bb);
extern uint64_t WestNorthSpan(uint64_t bb);
extern uint64_t EastSouthSpan(uint64_t bb);
extern uint64_t WestSouthSpan(uint64_t bb);

extern uint64_t FileFill(uint64_t bb);
extern uint64_t EastFileFill(uint64_t bb);
extern uint64_t WestFileFill(uint64_t bb);

extern uint64_t noNeighborOnEastFile(uint64_t bb);
extern uint64_t noNeighborOnWestFile(uint64_t bb);

extern uint64_t isolanis(uint64_t bb);
extern uint64_t halfIsolanis(uint64_t bb);
extern uint64_t openSide1(uint64_t side1Pawns, uint64_t side2Pawns);
extern uint64_t openSide2(uint64_t side2Pawns, uint64_t side1Pawns);
extern uint64_t backwardSide1(uint64_t side1Pawns, uint64_t side2Pawns);
extern uint64_t backwardSide2(uint64_t side2Pawns, uint64_t side1Pawns);
extern uint64_t stragglerSide1(uint64_t side1Pawns, uint64_t side2Pawns);
extern uint64_t stragglerSide2(uint64_t side2Pawns, uint64_t side1Pawns);

extern uint64_t passedSide1(uint64_t wpawns, uint64_t bpawns);
extern uint64_t passedSide2(uint64_t bpawns, uint64_t wpawns);

extern uint64_t closedFiles(uint64_t bb1, uint64_t bb2);
extern uint64_t openFiles(uint64_t bb1, uint64_t bb2);
extern uint64_t halfOpenOrOpenFiles(uint64_t bb);
extern uint64_t halfOpenFile(uint64_t bb1, uint64_t bb2);
