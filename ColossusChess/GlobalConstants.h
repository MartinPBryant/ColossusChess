#pragma once

#include <string>

//----------------------------------------------------------------------------------------------------

// Version
#define VersionX "20NNx80"
//#define EXPERIMENTAL
//#define TB_NO_HW_POP_COUNT // Use the same directive name as defined in Ronald de Man's probing code!

#define Sides 2

// Material values
#define MVPawn 100
#define MVKnight 324 // N.B. do NOT make knight and bishop have exactly the same value else the 'KnownLowMaterialDraws' code may fail!
#define MVBishop 325
#define MVRook 500
#define MVQueen 975
#define MVKing 1000
// The maximum number of 'points' of material for one side in a legal position is 9(Q)+10(Rs)+12(B+Ns)+8*9(promoted Ps)=103 ... so a score of ~10300cp or 103.00

enum PiecesEnum {
	AllPieces = 0, // For bitboards
	Empty = 0, Pawn, Knight, Bishop, Rook, Queen, King
};

const short MaterialValue[7] = { 0, MVPawn, MVKnight, MVBishop, MVRook, MVQueen, MVKing };
const int SimplePieceValues[7] = { 0, 1, 3, 3, 5, 9, 999 };

// Searching
#define MaximumPly 256

// Miscellaneous scoring values
// Mating values 16000 - 15000, EGTB win values 15000-14000
// All EGTB and mating wins will be >= EGTBWinningScore
// At the root: #1=15998, #-1=-15997, #2=15996, #-2=-15995, #3=15994... i.e. 16000-2*mateInN or -16000+2*matedInN+1
#define MatingIn0Score 16000
#define MatingScore (MatingIn0Score - 1000)
#define MatedScore (-MatingScore)
#define EGTBWinningIn0Score 15000
#define EGTBWinningScore (EGTBWinningIn0Score - 1000)
#define EGTBLosingScore (-EGTBWinningScore)
#define AspirationWindowDelta 50

// Move flags
// Four bits (maximum possible when we compress the move into 16 bits in the TT {from-sq:6 bits, to-sq:6 bits, flag:4 bits})
// 0-1: 0=normal move, 1=EP, 2=castling, 3=pawn promotion
// 2-3: if pawn promotion: 0=Q, 1=R, 2=B, 3=N
#define MFNormal 0
#define MFEnPassant 1
#define MFCastling 2
#define MFPromotion 3
#define MFPromoteToQueen ((0 << 2) + MFPromotion)
#define MFPromoteToRook ((1 << 2) + MFPromotion)
#define MFPromoteToBishop ((2 << 2) + MFPromotion)
#define MFPromoteToKnight ((3 << 2) + MFPromotion)
const int PromotedPieces[4] = { Queen, Rook, Bishop, Knight };

#define NullMove 0

// Principal variation terminators (all have the bottom 16-bits zero)
#define PVTUnknown (0 << 16)
#define PVTStandPat (1 << 16)
#define PVTDrawByRepetition (2 << 16)
#define PVTDrawBy50MoveRule (3 << 16)
#define PVTDrawMinimumMaterial (4 << 16)
#define PVTDrawImmediateRepetition (5 << 16)
#define PVTDrawPerpetual (6 << 16)
#define PVTDrawStalemate (7 << 16)
#define PVTCheckmate (8 << 16)
#define PVTEGTB (9 << 16)
#define PVTTTUpper (10 << 16)
#define PVTTTLower (11 << 16)
#define PVTTTExact (12 << 16)
#define PVTFailedMateCondition (13 << 16)

// Node types (not used!) - See https://www.chessprogramming.org/Node_Types
#define NTAll -1
#define NTPV 0
#define NTCut 1

// TT flag values
// Bit 7:		Only one piece can move
// Bit 6:		Threatened with mate
// Bit 5:		Only one legal move
// Bit 4:		Fewer moves than pieces
// Bits 3,2:	Exact/upper/lower flag
// Bits 1,0:	Age
#define TTFlagOnlyOnePieceCanMove 0x80
#define TTFlagThreatenedWithMate 0x40
#define TTFlagOnlyOneLegalMove 0x20
#define TTFlagFewerMovesThanPieces 0x10
#define TTFlagIsInDangerMask 0xF0
#define TTFlagExact 0x00 // PV
#define TTFlagLower 0x04 // Cut
#define TTFlagUpper 0x08 // All
#define TTFlagEULMask 0x0C
#define TTFlagAgeMask 0x03

const std::string Notation64[64] =
{
	"a1","b1","c1","d1","e1","f1","g1","h1",
	"a2","b2","c2","d2","e2","f2","g2","h2",
	"a3","b3","c3","d3","e3","f3","g3","h3",
	"a4","b4","c4","d4","e4","f4","g4","h4",
	"a5","b5","c5","d5","e5","f5","g5","h5",
	"a6","b6","c6","d6","e6","f6","g6","h6",
	"a7","b7","c7","d7","e7","f7","g7","h7",
	"a8","b8","c8","d8","e8","f8","g8","h8"
};

const std::string PieceToChar("kqrbnp.PNBRQK");

// Threads
#define ThreadsMin 1
#define ThreadsMax 64
#define ThreadsDefault 1

// Contempt
#define ContemptMin -150
#define ContemptMax 150
#define ContemptDefault 0

// EGTB probe limit
#define SyzygyProbeLimitMin 0
#define SyzygyProbeLimitMax 7
#define SyzygyProbeLimitDefault 6
