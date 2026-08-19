#pragma once

#include <string>

#include "GlobalConstants.h"

//----------------------------------------------------------------------------------------------------

// Square names corresponding to 0 - 63 indeces (a1=0, h8=63)
enum SquaresEnum : int8_t {
	A1, B1, C1, D1, E1, F1, G1, H1,
	A2, B2, C2, D2, E2, F2, G2, H2,
	A3, B3, C3, D3, E3, F3, G3, H3,
	A4, B4, C4, D4, E4, F4, G4, H4,
	A5, B5, C5, D5, E5, F5, G5, H5,
	A6, B6, C6, D6, E6, F6, G6, H6,
	A7, B7, C7, D7, E7, F7, G7, H7,
	A8, B8, C8, D8, E8, F8, G8, H8
};

// File names corresponding to 0 - 7 indeces (a-file=0, h-file=7)
enum FilesEnum {
	A, B, C, D, E, F, G, H
};

struct MoveFields_Struct
{
	SquaresEnum fromSquare;
	SquaresEnum toSquare;
	uint16_t flag; // See 'Move flags' in GlobalConstants.h
};
struct alignas(4) Move_Struct // 4
{
	union {
		uint32_t ui32;
		MoveFields_Struct mf;
	};
};
struct alignas(8) MoveWithScore_Struct // 8
{
	union {
		uint32_t ui32;
		MoveFields_Struct mf;
	};
	int score; // Must be signed int as is used for actual scores at the root as well as ordering scores deeper in the tree NO LONGER TRUE???
};
struct alignas(8) MoveWithPiece_Struct // 8
{
	union {
		uint32_t ui32;
		MoveFields_Struct mf;
	};
	int piece;
};
struct MoveUndo_Struct
{
	union {
		uint32_t ui32;
		MoveFields_Struct mf;
	};
	uint64_t fromToXor;
	int8_t fromSquarePiece;
	int8_t toSquarePiece;
};
struct alignas(16) TwoGoodMoves_Struct // 16
{
	MoveWithPiece_Struct m1;
	MoveWithPiece_Struct m2;
};

struct RootMoveList_Struct
{
	MoveWithScore_Struct mws;
	uint64_t nodes;
	int priority;
	int EGTBWDL;
	int EGTBDTZ;
	int EGTBRank;
};

struct History_Struct
{
	int History[6][64]; // 1536
};

struct CounterMoveHistory_Struct
{
	History_Struct CMH[6][64]; // 589824
};

// Castling statuses
union GameRecordCastlingStatusUnion
{
	uint8_t ui8[Sides][2];
	uint32_t ui32; // 0 = all castling possible, 0x01010101 = no castling possible
};

struct GameRecordEntry_Struct
{
	// N.B. Microsoft pack structures to be a multiple of the largest field
	uint64_t transpositionTableHash64; // N.B. This is the hash BEFORE any move has been played at this ply
	uint64_t transpositionTableHash64WithEP;
	//uint64_t epRandom;
	//uint64_t pinnersRankFileBB;
	//uint64_t pinnersDiagonalBB;
	uint64_t pinnedRankFileBB;
	uint64_t pinnedDiagonalBB;
	uint64_t pinnedAllBB;
	uint64_t discoverersRankFileBB;
	uint64_t discoverersDiagonalBB;
	uint64_t discoverersAllBB;
	uint32_t* principalVariationPointer; // Initialised in ComputeNormal
	History_Struct* historyPointer;

	// About the position before any move is played at this ply
	GameRecordCastlingStatusUnion castlingStatus; // Four entries set to 0 at the start of a game to signify all castling possible
	int isInCheck;
	int isTWM; // Threatened with mate
	int isO1M; // Only one move
	int isFMTP; // Fewer moves than pieces
	int isZLKM; // Zero legal king moves
	int isO1PCM; // Only one piece can move
	int isOKCM; // Only king can move

	int sideToMove; // N.B. NOT updated during tree search
	int moveNumber; // N.B. NOT updated during tree search

	uint64_t zLMPiecesBB; // Bitboard indicating the defenders pieces that have zero moves at the root

	int gamePhase[2]; // Keeps a running total of pieces (not pawns) for each side. At the start of the game this is 31 (3x4 + 5x2 + 9x1) but can be higher after promotions to a maximum of 103. The evaluation function limits the sum of both sides to a total of 64
	short staticEvaluation;
	short totalMaterial[2];
	short totalOpeningPST[2];
	short totalEndgamePST[2];

	uint8_t epSquare;
	uint8_t pliesSinceIrreversible;

	bool forcingLine;
	bool forcingLineTWM;
	int DefenderKingMovesBefore; // Updated at even plies with the number of defender king moves at this ply
	int TotalDefenderKingMovesBefore;
	int lineExpense;

	// About each move that is played at this ply
	int givesCheck;
	Move_Struct isThreateningMateInOne;
	int isAttacking;
	//int expense;
	bool forcingMove;
	int DefenderKingMovesAfter;
	int TotalDefenderKingMovesAfter;
	int moveExpense;

	MoveUndo_Struct move;
	int excludedMove;

	int SEEResult;
	int checks;

	uint64_t fixedPiecesAttackerBB;
	uint64_t fixedPiecesDefenderBB;
};
