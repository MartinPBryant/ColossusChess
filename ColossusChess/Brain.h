#pragma once

#include "GlobalTypes.h"
#include "Evaluate.h"

class Brain
{
public:
	Brain();
	~Brain();
	void CopyFrom(Brain* sourceBrain);
	void ClearGameRecord();

	bool AnyChecks(int sideToMove);
	MoveWithScore_Struct* GenerateCapturesAndPromotions(int sideToMove, MoveWithScore_Struct *MLP);
	MoveWithScore_Struct* GenerateCapturesAndNonCaptures(int sideToMove, MoveWithScore_Struct* MLP);
	uint32_t CountCapturesAndNonCaptures(int sideToMove);
	bool AnyCapturesAndNonCaptures(int sideToMove);
	MoveWithScore_Struct* GenerateAllMovesOutOfCheck(int sideToMove, MoveWithScore_Struct* MLP, bool GenerateUnderPromotions);
	uint32_t CountAllMovesOutOfCheck(int sideToMove);
	bool AnyMovesOutOfCheck(int sideToMove);
	MoveWithScore_Struct* Brain::GenerateNonCaptureNonPromotionDirectChecks(int sideToMove, MoveWithScore_Struct* MLP);
	MoveWithScore_Struct* Brain::GenerateAllChecks(int sideToMove, MoveWithScore_Struct* MLP);
	MoveWithScore_Struct* Brain::GenerateAllNonCaptureNonPromotionChecks(int sideToMove, MoveWithScore_Struct* mlp);
	uint32_t CountKingMoves(int sideToMove);

	void CalculatePinnedPieces(int sideToMove);
	void CalculateDiscovererPieces(int sideToMove);

	uint32_t GenerateAllMoves(int sideToMove, int isInCheck, MoveWithScore_Struct* initialMLP);
	uint32_t GenerateMovesQuiescence(int sideToMove, int isInCheck, MoveWithScore_Struct* initialMLP, int depthRemaining);
	uint32_t CountAllMoves(int sideToMove, int isInCheck);
	bool AnyMoves(int sideToMove, int isInCheck);
	int CountAllQueenMovesMM(int sideToMove);
	int CountAllMovesMM(int sideToMove);

	void MakeMove(int sideToMove);
	void UnMakeMove(int sideToMove);

	int IsAttacked(int square, int sideToMove);
	int IsEnemyKingAttacked(int square, int sideToMove);

	uint64_t RankFilePinnersBB(int kingSquare, int sideToMove);
	uint64_t DiagonalPinnersBB(int kingSquare, int sideToMove);
	uint64_t RankFileDiscovereesBB(int enemyKingSquare, int sideToMove);
	uint64_t DiagonalDiscovereesBB(int enemyKingSquare, int sideToMove);

	void ScoreMoves(MoveWithScore_Struct* mlp, int movesCount, int tteBestMove, int ply, TwoGoodMoves_Struct* killerMoves, TwoGoodMoves_Struct* cms, TwoGoodMoves_Struct* fums);
	void ScoreMovesMVVLVA(MoveWithScore_Struct* mlp, int movesCount);
	void ScoreMovesMateMode(MoveWithScore_Struct* mlp, int movesCount, int tteBestMove, int ply, TwoGoodMoves_Struct* killerMoves, TwoGoodMoves_Struct* cms, TwoGoodMoves_Struct* fums, int enemyKingSquare, Move_Struct MatingMove);

	void SavePrincipalVariation(uint32_t move);

	int SEE(int fromSquare, int toSquare, int sideToMove);
	int SEE2(int fromSquare, int toSquare, int sideToMove, int threshold);
	bool SEETargetPieceUnsafe(int toSquare, int sideToMove, int offset);

	int KnownLowMaterialDraws(int sideToMove);
	bool KnownLowMaterialWins();
	bool KingCanLegallyMove(int sideToMove);
	bool ForcingLine(int ply, int offset);
	int SafePawnMoves(int sideToMove);
	bool HasOpposition(int sideToMove);
	std::string CurrentLine(int ply);

	Move_Struct ThreateningMateInOneWithNull(int sideToMove, int &checksCount);
	Move_Struct ThreateningMateInOne(int sideToMove, int &checksCount);
	
	uint32_t SYZYGYPYRRHICMoveToColossusMove(uint16_t SYZYGYmove, uint32_t epSquare);

public:
	alignas(64) int8_t mailboxBoard64[64];
	uint64_t piecesBB[Sides][King + 2]; // Making the size of the second dimension a power of 2 (in this case 8) gives about a 5% speed improvement!
	//static const int gameRecordSize = 800; // This used to be 600 but crashed when it played a game to move 288 without GUI EGTB adjudication (it got to move 239 before getting into the EGTBs anyway!)
	static const int gameRecordSize = 1000; // This used to be 800 but crashed when it played a game to move 344 in an opposite coloured bishop ending! But I can't be certain this was the problem!
	GameRecordEntry_Struct* gameRecord; // Created and initialised in the constructor.
	GameRecordEntry_Struct* gameRecordPointer; // Gets incremented in MakeMove and decremented in UnMakeMove
	int GameRecordIndexRoot; // Set to 2 when the game record is cleared. Set to the index of the position in the game record from where a search is made e.g. after e4 e5 it will be 4
};
