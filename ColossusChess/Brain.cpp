#include <algorithm>
#include <assert.h>

//#include "BitBoard.h"
#include "Engine.h"
#include "Utilities.h"
#include "Brain.h"
//#include "SearchNormal.h"
#include "SYZYGYPYRRHIC\tbprobe.h"

//----------------------------------------------------------------------------------------------------

Brain::Brain()
{
	GameRecord = new GameRecordEntry_Struct[GameRecordSize];
	ClearGameRecord();
}

Brain::~Brain()
{
	delete GameRecord;
}

//----------------------------------------------------------------------------------------------------

void Brain::CopyFrom(Brain* sourceBrain)
{
	// Copy the mailbox board and the game record from the source brain into this brain
	std::copy(sourceBrain->MailboxBoard64, sourceBrain->MailboxBoard64 + 64, this->MailboxBoard64); // N.B. "+64" is correct!
	for (int i = 0; i < this->GameRecordSize; i++)
		this->GameRecord[i] = sourceBrain->GameRecord[i];
	this->GameRecordIndexRoot = sourceBrain->GameRecordIndexRoot;
	this->GameRecordPointer = &GameRecord[GameRecordIndexRoot];
}

void Brain::ClearGameRecord()
{
	// The 0th and 1th entries are placeholders BEFORE the first move of the game. They may be referenced by 'improving' code/'follow-up-move' code which examines previous plies data
	GameRecord[0].move.ui32 = 0 | (0 << 8) | (MFCastling << 16); // Anything to flag it as irreversible
	GameRecord[0].move.fromSquarePiece = King;
	GameRecord[0].move.toSquarePiece = King;
	GameRecord[0].castlingStatus.ui32 = 0;
	GameRecord[0].sideToMove = 0;
	GameRecord[0].moveNumber = 0;
	GameRecord[0].transpositionTableHash64 = 0;
	GameRecord[0].transpositionTableHash64WithEP = 0;
	GameRecord[0].isInCheck = 0;
	//gameRecord[0].dangerConditions.dc.isTWM = TTFlagThreatenedWithMate;
	//gameRecord[0].dangerConditions.dc.isFMTP = TTFlagFewerMovesThanPieces;
	//gameRecord[0].dangerConditions.dc.isO1M = TTFlagOnlyOneMove;
	//gameRecord[0].dangerConditions.dc.isO1PCM = TTFlagOnlyOnePieceCanMove;
	//gameRecord[0].dangerConditions.dc.isZLKM = 1;
	//gameRecord[0].dangerConditions.dc.isOKCM = 1;
	GameRecord[0].dangerConditions = 0xFF;
	GameRecord[0].isThreateningMateInOne.ui32 = 0;
	GameRecord[0].epSquare = 0;
	GameRecord[0].pliesSinceIrreversible = 0;
	GameRecord[0].givesCheck = 0;
	GameRecord[0].isThreateningMateInOne.ui32 = 0;
	GameRecord[0].forcingLine = true;
	GameRecord[0].forcingLineTWM = true;
	GameRecord[0].forcingMove = true;
	GameRecord[0].DefenderKingMovesBefore = 0;
	GameRecord[0].TotalDefenderKingMovesBefore = 0;
	GameRecord[0].DefenderKingMovesAfter = 0;
	GameRecord[0].TotalDefenderKingMovesAfter = 0;
	GameRecord[0].SEEResult = 0;
	GameRecord[0].move.ui32 = 0;
	GameRecord[0].move.fromSquarePiece = 1;
	GameRecord[0].move.toSquarePiece = 0;

	GameRecord[1].move.ui32 = 0 | (0 << 8) | (MFCastling << 16); // Anything to flag it as irreversible
	GameRecord[1].move.fromSquarePiece = King;
	GameRecord[1].move.toSquarePiece = King;
	GameRecord[1].castlingStatus.ui32 = 0;
	GameRecord[1].sideToMove = 1;
	GameRecord[1].moveNumber = 0;
	GameRecord[1].transpositionTableHash64 = 0;
	GameRecord[1].transpositionTableHash64WithEP = 0;
	GameRecord[1].isInCheck = 0;
	//gameRecord[1].dangerConditions.dc.isTWM = TTFlagThreatenedWithMate;
	//gameRecord[1].dangerConditions.dc.isFMTP = TTFlagFewerMovesThanPieces;
	//gameRecord[1].dangerConditions.dc.isO1M = TTFlagOnlyOneMove;
	//gameRecord[1].dangerConditions.dc.isO1PCM = TTFlagOnlyOnePieceCanMove;
	//gameRecord[1].dangerConditions.dc.isZLKM = 1;
	//gameRecord[1].dangerConditions.dc.isOKCM = 1;
	GameRecord[0].dangerConditions = 0xFF;
	GameRecord[1].isThreateningMateInOne.ui32 = 0;
	GameRecord[1].epSquare = 0;
	GameRecord[1].pliesSinceIrreversible = 0;
	GameRecord[1].givesCheck = 0;
	GameRecord[1].isThreateningMateInOne.ui32 = 0;
	GameRecord[1].forcingLine = true;
	GameRecord[1].forcingLineTWM = true;
	GameRecord[1].forcingMove = true;
	GameRecord[1].DefenderKingMovesBefore = 0;
	GameRecord[0].TotalDefenderKingMovesBefore = 0;
	GameRecord[1].DefenderKingMovesAfter = 0;
	GameRecord[1].TotalDefenderKingMovesAfter = 0;
	GameRecord[1].SEEResult = 0;
	GameRecord[1].move.ui32 = 0;
	GameRecord[1].move.fromSquarePiece = 1;
	GameRecord[1].move.toSquarePiece = 0;

	// The 2th entry needs some fields initialising too (which may subsequently be updated if a FEN position is specified)
	GameRecord[2].sideToMove = 0;
	GameRecord[2].moveNumber = 1;
	GameRecord[2].castlingStatus.ui32 = 0;
	GameRecord[2].epSquare = 0;
	GameRecord[2].pliesSinceIrreversible = 0;

	// The first move of the game goes in the 2th entry
	GameRecordIndexRoot = 2;
	GameRecordPointer = &GameRecord[GameRecordIndexRoot];
}

//----------------------------------------------------------------------------------------------------

bool Brain::AnyChecks(int sideToMove)
{
	//DISCOVERED CHECKS VERY HARD???
	//ALSO EP AND CASTLING :(
	// AND P PROMS!
	//WOULD 'GENERATECHECKS' BE EASIER??? PIECE SPECIFIC TESTS POSSIBLE IN THE RIGHT PLACE THEN (already got GenerateNonCaptureDirectChecks)
	//IN THE NORMAL GEN ROUTINES, CAN CHECKS BE FLAGGED EASILY?




	return false;
}

MoveWithScore_Struct* Brain::GenerateCapturesAndPromotions(int sideToMove, MoveWithScore_Struct* mlp) // Only used in the QS
{
	// ~22% of calls generate zero captures
	// ~29% of calls generate one capture
	// ~78% of calls generate some captures
	// I tried adding a sizable 'ZeroCapturesAndPromotions' hash table but it was hit so few times it didn't outweigh the overhead

	uint32_t fromSquare, toSquare;
	uint64_t attacksBB;
	uint64_t occupiedBB = PiecesBB[0][AllPieces] | PiecesBB[1][AllPieces];
	uint32_t kingSquare = GetLS1BIndex(PiecesBB[sideToMove][King]);

	// Pawns
	int pmo = PawnMoveOffset[sideToMove];
	uint64_t pawnPromotionsBB = (((PiecesBB[sideToMove][Pawn] & SeventhRankBB[sideToMove] & ~GameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB;
	while (pawnPromotionsBB)
	{
		toSquare = GetLS1BIndex(pawnPromotionsBB);
		if (
			(!(CreateBitboardFromSquare(toSquare - pmo) & GameRecordPointer->pinnedRankFileBB)) || // Not pinned?
			(CreateBitboardFromSquare(toSquare) & LineListBB[toSquare - pmo][kingSquare])
			)
		{
			mlp++->ui32 = (toSquare - pmo) | (toSquare << 8) | (MFPromoteToQueen << 16);
			//if (GenerateUnderPromotions)
			//{
			//	mlp++->ui32 = (toSquare - pmo) | (toSquare << 8) | (MFPromoteToRookNew << 16);
			//	mlp++->ui32 = (toSquare - pmo) | (toSquare << 8) | (MFPromoteToBishopNew << 16);
			//	mlp++->ui32 = (toSquare - pmo) | (toSquare << 8) | (MFPromoteToKnightNew << 16);
			//}
			//else if (KnightAttacksBBList[toSquare] & piecesBB[sideToMove ^ 1][King])
			//	mlp++->i32 = (toSquare - pmo) | (toSquare << 8) | (MFPromoteToKnightNew << 16); // Knight promotions that give check (-3.0, +/-3.6, 20000)
		}
		ClearLS1B(pawnPromotionsBB);
	}
	uint64_t pawnsCapturesEastBB = (East(((PiecesBB[sideToMove][Pawn] & ~GameRecordPointer->pinnedRankFileBB) << 8) >> (sideToMove << 4))) & PiecesBB[sideToMove ^ 1][AllPieces];
	while (pawnsCapturesEastBB)
	{
		toSquare = GetLS1BIndex(pawnsCapturesEastBB);
		if (
			(!(CreateBitboardFromSquare(toSquare - (pmo + 1)) & GameRecordPointer->pinnedDiagonalBB)) || // Not pinned?
			(CreateBitboardFromSquare(toSquare) & LineListBB[toSquare - (pmo + 1)][kingSquare])
			)
		{
			if ((toSquare >> 3) == EigthRank[sideToMove])
			{
				mlp++->ui32 = (toSquare - (pmo + 1)) | (toSquare << 8) | (MFPromoteToQueen << 16);
				//if (GenerateUnderPromotions)
				//{
				//	mlp++->ui32 = (toSquare - (pmo + 1)) | (toSquare << 8) | (MFPromoteToRookNew << 16);
				//	mlp++->ui32 = (toSquare - (pmo + 1)) | (toSquare << 8) | (MFPromoteToBishopNew << 16);
				//	mlp++->ui32 = (toSquare - (pmo + 1)) | (toSquare << 8) | (MFPromoteToKnightNew << 16);
				//}
				//else if (KnightAttacksBBList[toSquare] & piecesBB[sideToMove ^ 1][King])
				//	mlp++->i32 = (toSquare - (pmo + 1)) | (toSquare << 8) | (MFPromoteToKnightNew << 16);
			}
			else
			{
				mlp++->ui32 = (toSquare - (pmo + 1)) | (toSquare << 8);
			}
		}
		ClearLS1B(pawnsCapturesEastBB);
	}
	uint64_t pawnsCapturesWestBB = (West(((PiecesBB[sideToMove][Pawn] & ~GameRecordPointer->pinnedRankFileBB) << 8) >> (sideToMove << 4))) & PiecesBB[sideToMove ^ 1][AllPieces];
	while (pawnsCapturesWestBB)
	{
		toSquare = GetLS1BIndex(pawnsCapturesWestBB);
		if (
			(!(CreateBitboardFromSquare(toSquare - (pmo - 1)) & GameRecordPointer->pinnedDiagonalBB)) || // Not pinned?
			(CreateBitboardFromSquare(toSquare) & LineListBB[toSquare - (pmo - 1)][kingSquare])
			)
		{
			if ((toSquare >> 3) == EigthRank[sideToMove])
			{
				mlp++->ui32 = (toSquare - (pmo - 1)) | (toSquare << 8) | (MFPromoteToQueen << 16);
				//if (GenerateUnderPromotions)
				//{
				//	mlp++->ui32 = (toSquare - (pmo - 1)) | (toSquare << 8) | (MFPromoteToRookNew << 16);
				//	mlp++->ui32 = (toSquare - (pmo - 1)) | (toSquare << 8) | (MFPromoteToBishopNew << 16);
				//	mlp++->ui32 = (toSquare - (pmo - 1)) | (toSquare << 8) | (MFPromoteToKnightNew << 16);
				//}
				//else if (KnightAttacksBBList[toSquare] & PiecesBB[sideToMove ^ 1][King])
				//	mlp++->i32 = (toSquare - (pmo - 1)) | (toSquare << 8) | (MFPromoteToKnight << 16);
			}
			else
			{
				mlp++->ui32 = (toSquare - (pmo - 1)) | (toSquare << 8);
			}
		}
		ClearLS1B(pawnsCapturesWestBB);
	}
	// En-passant
	if (GameRecordPointer->epSquare)
	{
		Move_Struct previousMove;
		previousMove.ui32 = (GameRecordPointer - 1)->move.ui32;
		if (West(CreateBitboardFromSquare(previousMove.mf.toSquare)) & PiecesBB[sideToMove][Pawn])
		{
			PiecesBB[sideToMove][Pawn] ^= (CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare));
			PiecesBB[sideToMove][AllPieces] ^= (CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare));
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			if (!IsAttacked(kingSquare, sideToMove ^ 1))
				mlp++->ui32 = (previousMove.mf.toSquare - 1) | ((previousMove.mf.toSquare) << 8) | (MFEnPassant << 16);
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
		}
		if (East(CreateBitboardFromSquare(previousMove.mf.toSquare)) & PiecesBB[sideToMove][Pawn])
		{
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			if (!IsAttacked(kingSquare, sideToMove ^ 1))
				mlp++->ui32 = (previousMove.mf.toSquare + 1) | ((previousMove.mf.toSquare) << 8) | (MFEnPassant << 16);
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
		}
	}

	// Knights
	uint64_t knightsBB = PiecesBB[sideToMove][Knight] & ~GameRecordPointer->pinnedAllBB;
	while (knightsBB)
	{
		fromSquare = GetLS1BIndex(knightsBB);
		attacksBB = KnightAttacksBBList[fromSquare] & PiecesBB[sideToMove ^ 1][AllPieces];
		while (attacksBB)
		{
			int toSquare = GetLS1BIndex(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(knightsBB);
	}

	// Bishops and queens
	uint64_t bishopsAndQueensBB = PiecesBB[sideToMove][Bishop] | PiecesBB[sideToMove][Queen];
	while (bishopsAndQueensBB)
	{
		fromSquare = GetLS1BIndex(bishopsAndQueensBB);
		attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & PiecesBB[sideToMove ^ 1][AllPieces];
		if (CreateBitboardFromSquare(fromSquare) & GameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			int toSquare = GetLS1BIndex(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(bishopsAndQueensBB);
	}

	// Rooks and queens
	uint64_t rooksAndQueensBB = PiecesBB[sideToMove][Rook] | PiecesBB[sideToMove][Queen];
	while (rooksAndQueensBB)
	{
		fromSquare = GetLS1BIndex(rooksAndQueensBB);
		attacksBB = RookAttacksBB(fromSquare, occupiedBB) & PiecesBB[sideToMove ^ 1][AllPieces];
		if (CreateBitboardFromSquare(fromSquare) & GameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			int toSquare = GetLS1BIndex(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(rooksAndQueensBB);
	}

	// King
	attacksBB = KingAttacksBBList[kingSquare] & PiecesBB[sideToMove ^ 1][AllPieces];
	while (attacksBB)
	{
		int toSquare = GetLS1BIndex(attacksBB);
		if (!IsAttacked(toSquare, sideToMove ^ 1))
			mlp++->ui32 = kingSquare | (toSquare << 8);
		ClearLS1B(attacksBB);
	}

	return mlp;
}

MoveWithScore_Struct* Brain::GenerateCapturesAndNonCaptures(int sideToMove, MoveWithScore_Struct* mlp)
{
	uint32_t fromSquare, toSquare;
	uint64_t attacksBB;
	uint64_t occupiedBB = PiecesBB[0][AllPieces] | PiecesBB[1][AllPieces];
	uint32_t kingSquare = GetLS1BIndex(PiecesBB[sideToMove][King]);

	// Pawns
	int pmo = PawnMoveOffset[sideToMove];
	uint64_t pawnMove1BB = (((PiecesBB[sideToMove][Pawn] & ~GameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB; // The '>> (sideToMove << 4)' clause flips north to south when the 2nd side is to move
	while (pawnMove1BB)
	{
		toSquare = GetLS1BIndex(pawnMove1BB);
		if (
			(!(CreateBitboardFromSquare(toSquare - pmo) & GameRecordPointer->pinnedRankFileBB)) || // Not pinned?
			(CreateBitboardFromSquare(toSquare) & LineListBB[toSquare - pmo][kingSquare])
			)
		{
			if ((toSquare >> 3) == EigthRank[sideToMove])
			{
				mlp++->ui32 = (toSquare - pmo) | (toSquare << 8) | (MFPromoteToQueen << 16);
				//if (GenerateUnderPromotions)
				{
					mlp++->ui32 = (toSquare - pmo) | (toSquare << 8) | (MFPromoteToRook << 16);
					mlp++->ui32 = (toSquare - pmo) | (toSquare << 8) | (MFPromoteToBishop << 16);
					mlp++->ui32 = (toSquare - pmo) | (toSquare << 8) | (MFPromoteToKnight << 16);
				}
			}
			else
			{
				mlp++->ui32 = (toSquare - pmo) | (toSquare << 8);
			}
		}
		ClearLS1B(pawnMove1BB);
	}
	uint64_t pawnMove2BB = ((((((PiecesBB[sideToMove][Pawn] & SecondRankBB[sideToMove] & ~GameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB) << 8) >> (sideToMove << 4)) & notOccupiedBB;
	while (pawnMove2BB)
	{
		toSquare = GetLS1BIndex(pawnMove2BB);
		if (
			(!(CreateBitboardFromSquare(toSquare - (pmo * 2)) & GameRecordPointer->pinnedRankFileBB)) ||
			(CreateBitboardFromSquare(toSquare) & LineListBB[toSquare - (pmo * 2)][kingSquare])
			)
			mlp++->ui32 = (toSquare - pmo * 2) | (toSquare << 8);
		ClearLS1B(pawnMove2BB);
	}
	uint64_t pawnsCapturesEastBB = (East(((PiecesBB[sideToMove][Pawn] & ~GameRecordPointer->pinnedRankFileBB) << 8) >> (sideToMove << 4))) & PiecesBB[sideToMove ^ 1][AllPieces];
	while (pawnsCapturesEastBB)
	{
		toSquare = GetLS1BIndex(pawnsCapturesEastBB);
		if (
			(!(CreateBitboardFromSquare(toSquare - (pmo + 1)) & GameRecordPointer->pinnedDiagonalBB)) || // Not pinned?
			(CreateBitboardFromSquare(toSquare) & LineListBB[toSquare - (pmo + 1)][kingSquare])
			)
		{
			if ((toSquare >> 3) == EigthRank[sideToMove])
			{
				mlp++->ui32 = (toSquare - (pmo + 1)) | (toSquare << 8) | (MFPromoteToQueen << 16);
				//if (GenerateUnderPromotions)
				{
					mlp++->ui32 = (toSquare - (pmo + 1)) | (toSquare << 8) | (MFPromoteToRook << 16);
					mlp++->ui32 = (toSquare - (pmo + 1)) | (toSquare << 8) | (MFPromoteToBishop << 16);
					mlp++->ui32 = (toSquare - (pmo + 1)) | (toSquare << 8) | (MFPromoteToKnight << 16);
				}
			}
			else
			{
				mlp++->ui32 = (toSquare - (pmo + 1)) | (toSquare << 8);
			}
		}
		ClearLS1B(pawnsCapturesEastBB);
	}
	uint64_t pawnsCapturesWestBB = (West(((PiecesBB[sideToMove][Pawn] & ~GameRecordPointer->pinnedRankFileBB) << 8) >> (sideToMove << 4))) & PiecesBB[sideToMove ^ 1][AllPieces];
	while (pawnsCapturesWestBB)
	{
		toSquare = GetLS1BIndex(pawnsCapturesWestBB);
		if (
			(!(CreateBitboardFromSquare(toSquare - (pmo - 1)) & GameRecordPointer->pinnedDiagonalBB)) || // Not pinned?
			(CreateBitboardFromSquare(toSquare) & LineListBB[toSquare - (pmo - 1)][kingSquare])
			)
		{
			if ((toSquare >> 3) == EigthRank[sideToMove])
			{
				mlp++->ui32 = (toSquare - (pmo - 1)) | (toSquare << 8) | (MFPromoteToQueen << 16);
				//if (GenerateUnderPromotions)
				{
					mlp++->ui32 = (toSquare - (pmo - 1)) | (toSquare << 8) | (MFPromoteToRook << 16);
					mlp++->ui32 = (toSquare - (pmo - 1)) | (toSquare << 8) | (MFPromoteToBishop << 16);
					mlp++->ui32 = (toSquare - (pmo - 1)) | (toSquare << 8) | (MFPromoteToKnight << 16);
				}
			}
			else
			{
				mlp++->ui32 = (toSquare - (pmo - 1)) | (toSquare << 8);
			}
		}
		ClearLS1B(pawnsCapturesWestBB);
	}
	// En-passant
	if (GameRecordPointer->epSquare)
	{
		Move_Struct previousMove;
		previousMove.ui32 = (GameRecordPointer - 1)->move.ui32;
		if (West(CreateBitboardFromSquare(previousMove.mf.toSquare)) & PiecesBB[sideToMove][Pawn])
		{
			PiecesBB[sideToMove][Pawn] ^= (CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare));
			PiecesBB[sideToMove][AllPieces] ^= (CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare));
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			if (!IsAttacked(kingSquare, sideToMove ^ 1))
				mlp++->ui32 = (previousMove.mf.toSquare - 1) | ((previousMove.mf.toSquare) << 8) | (MFEnPassant << 16);
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
		}
		if (East(CreateBitboardFromSquare(previousMove.mf.toSquare)) & PiecesBB[sideToMove][Pawn])
		{
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			if (!IsAttacked(kingSquare, sideToMove ^ 1))
				mlp++->ui32 = (previousMove.mf.toSquare + 1) | ((previousMove.mf.toSquare) << 8) | (MFEnPassant << 16);
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
		}
	}

	// Knights
	uint64_t knightsBB = PiecesBB[sideToMove][Knight] & ~GameRecordPointer->pinnedAllBB;
	while (knightsBB)
	{
		fromSquare = GetLS1BIndex(knightsBB);
		attacksBB = KnightAttacksBBList[fromSquare] & ~PiecesBB[sideToMove][AllPieces];
		while (attacksBB)
		{
			int toSquare = GetLS1BIndex(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(knightsBB);
	}

	// Bishops and queens
	uint64_t bishopsAndQueensBB = PiecesBB[sideToMove][Bishop] | PiecesBB[sideToMove][Queen];
	while (bishopsAndQueensBB)
	{
		fromSquare = GetLS1BIndex(bishopsAndQueensBB);
		attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & ~PiecesBB[sideToMove][AllPieces];
		if (CreateBitboardFromSquare(fromSquare) & GameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			int toSquare = GetLS1BIndex(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(bishopsAndQueensBB);
	}

	// Rooks and queens
	uint64_t rooksAndQueensBB = PiecesBB[sideToMove][Rook] | PiecesBB[sideToMove][Queen];
	while (rooksAndQueensBB)
	{
		fromSquare = GetLS1BIndex(rooksAndQueensBB);
		attacksBB = RookAttacksBB(fromSquare, occupiedBB) & ~PiecesBB[sideToMove][AllPieces];
		if (CreateBitboardFromSquare(fromSquare) & GameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			int toSquare = GetLS1BIndex(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(rooksAndQueensBB);
	}

	// King
	attacksBB = KingAttacksBBList[kingSquare] & ~PiecesBB[sideToMove][AllPieces];
	while (attacksBB)
	{
		int toSquare = GetLS1BIndex(attacksBB);
		if (!IsAttacked(toSquare, sideToMove ^ 1))
			mlp++->ui32 = kingSquare | (toSquare << 8);
		ClearLS1B(attacksBB);
	}

	// Generate castling moves
	if (UCI_Chess960)
	{
		// King side
		if (GameRecordPointer->castlingStatus.ui8[sideToMove][0] == 0)
		{
			// Check passed-over squares are empty and not attacked
			bool allEmpty;
			int offset;
			int square;

			allEmpty = true;

			square = kingSquare;
			offset = 1;
			//if (square > BackRankBaseSquareIndex[sideToMove] + G) // Not neccesary on king side
			//	offset = -1;
			while (true) // Scan king squares
			{
				if ((square != kingSquare) && ((square & 7) != InitialKingSideRookFile))
					if (MailboxBoard64[square] != Empty)
					{
						allEmpty = false;
						break;
					}
				if (IsAttacked(square, sideToMove ^ 1)) // Also check king squares for attacks
				{
					allEmpty = false;
					break;
				}
				if (square == BackRankBaseSquareIndex[sideToMove] + G)
					break;
				square += offset;
			}
			if (allEmpty)
			{
				square = kingSquare - InitialKingFile + InitialKingSideRookFile;
				offset = 1;
				if (square > BackRankBaseSquareIndex[sideToMove] + F)
					offset = -1;
				while (true) // Scan rook squares
				{
					if ((square != kingSquare) && ((square & 7) != InitialKingSideRookFile))
						if (MailboxBoard64[square] != Empty)
						{
							allEmpty = false;
							break;
						}
					if (square == BackRankBaseSquareIndex[sideToMove] + F)
						break;
					square += offset;
				}
				if (allEmpty)
				{
					if (((PiecesBB[sideToMove ^ 1][Rook] | PiecesBB[sideToMove ^ 1][Queen]) & CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + H)) == 0)
						mlp++->ui32 = kingSquare | ((BackRankBaseSquareIndex[sideToMove] + G) << 8) | (MFCastling << 16);
				}
			}
		}

		// Queen side
		if (GameRecordPointer->castlingStatus.ui8[sideToMove][1] == 0)
		{
			// Check passed-over squares are empty
			bool allEmpty;
			int offset;
			int square;

			allEmpty = true;

			square = kingSquare;
			offset = 1;
			if (square > BackRankBaseSquareIndex[sideToMove] + C)
				offset = -1;
			while (true) // Scan king squares
			{
				if ((square != kingSquare) && ((square & 7) != InitialQueenSideRookFile))
					if (MailboxBoard64[square] != Empty)
					{
						allEmpty = false;
						break;
					}
				if (IsAttacked(square, sideToMove ^ 1)) // Also check king squares for attacks
				{
					allEmpty = false;
					break;
				}
				if (square == BackRankBaseSquareIndex[sideToMove] + C)
					break;
				square += offset;
			}
			if (allEmpty)
			{
				square = kingSquare - InitialKingFile + InitialQueenSideRookFile;
				offset = 1;
				if (square > BackRankBaseSquareIndex[sideToMove] + D)
					offset = -1;
				while (true) // Scan rook squares
				{
					if ((square != kingSquare) && ((square & 7) != InitialQueenSideRookFile))
						if (MailboxBoard64[square] != Empty)
						{
							allEmpty = false;
							break;
						}
					if (square == BackRankBaseSquareIndex[sideToMove] + D)
						break;
					square += offset;
				}
				if (allEmpty)
				{
					uint64_t rooksAndQueensBB = PiecesBB[sideToMove ^ 1][Rook] | PiecesBB[sideToMove ^ 1][Queen];
					if ((rooksAndQueensBB & CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + B)) == 0)
						if (((rooksAndQueensBB & CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + A)) == 0) || ((MailboxBoard64[BackRankBaseSquareIndex[sideToMove] + B] != Empty) && (InitialQueenSideRookFile != B)))
							mlp++->ui32 = kingSquare | ((BackRankBaseSquareIndex[sideToMove] + C) << 8) | (MFCastling << 16);
				}
			}
		}
	}
	else
	{
		// King side
		if (GameRecordPointer->castlingStatus.ui8[sideToMove][0] == 0)
			if ((MailboxBoard64[kingSquare + 2] | MailboxBoard64[kingSquare + 1]) == Empty)
				if (!IsAttacked(kingSquare + 1, sideToMove ^ 1) && !IsAttacked(kingSquare + 2, sideToMove ^ 1))
					mlp++->ui32 = kingSquare | ((kingSquare + 2) << 8) | (MFCastling << 16);

		// Queen side
		if (GameRecordPointer->castlingStatus.ui8[sideToMove][1] == 0)
			if ((MailboxBoard64[kingSquare - 3] | MailboxBoard64[kingSquare - 2] | MailboxBoard64[kingSquare - 1]) == Empty)
				if (!IsAttacked(kingSquare - 1, sideToMove ^ 1) && !IsAttacked(kingSquare - 2, sideToMove ^ 1))
					mlp++->ui32 = kingSquare | ((kingSquare - 2) << 8) | (MFCastling << 16);
	}

	return mlp;
}

uint32_t Brain::CountCapturesAndNonCaptures(int sideToMove)
{
	uint32_t fromSquare, toSquare;
	uint64_t attacksBB;
	uint64_t occupiedBB = PiecesBB[0][AllPieces] | PiecesBB[1][AllPieces];
	uint32_t kingSquare = GetLS1BIndex(PiecesBB[sideToMove][King]);
	uint64_t moves = 0;

	// Pawns
	int pmo = PawnMoveOffset[sideToMove];
	uint64_t pawnMove1BB = (((PiecesBB[sideToMove][Pawn] & ~GameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB;
	while (pawnMove1BB)
	{
		toSquare = GetLS1BIndex(pawnMove1BB);
		if (
			(!(CreateBitboardFromSquare(toSquare - pmo) & GameRecordPointer->pinnedRankFileBB)) || // Not pinned?
			(CreateBitboardFromSquare(toSquare) & LineListBB[toSquare - pmo][kingSquare])
			)
		{
			if ((toSquare >> 3) == EigthRank[sideToMove])
			{
				moves += 4;
			}
			else
			{
				moves++;
			}
		}
		ClearLS1B(pawnMove1BB);
	}
	uint64_t pawnMove2BB = ((((((PiecesBB[sideToMove][Pawn] & SecondRankBB[sideToMove] & ~GameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB) << 8) >> (sideToMove << 4)) & notOccupiedBB;
	while (pawnMove2BB)
	{
		toSquare = GetLS1BIndex(pawnMove2BB);
		if (
			(!(CreateBitboardFromSquare(toSquare - (pmo * 2)) & GameRecordPointer->pinnedRankFileBB)) ||
			(CreateBitboardFromSquare(toSquare) & LineListBB[toSquare - (pmo * 2)][kingSquare])
			)
			moves++;
		ClearLS1B(pawnMove2BB);
	}
	uint64_t pawnsCapturesEastBB = (East(((PiecesBB[sideToMove][Pawn] & ~GameRecordPointer->pinnedRankFileBB) << 8) >> (sideToMove << 4))) & PiecesBB[sideToMove ^ 1][AllPieces];
	while (pawnsCapturesEastBB)
	{
		toSquare = GetLS1BIndex(pawnsCapturesEastBB);
		if (
			(!(CreateBitboardFromSquare(toSquare - (pmo + 1)) & GameRecordPointer->pinnedDiagonalBB)) || // Not pinned?
			(CreateBitboardFromSquare(toSquare) & LineListBB[toSquare - (pmo + 1)][kingSquare])
			)
		{
			if ((toSquare >> 3) == EigthRank[sideToMove])
			{
				moves += 4;
			}
			else
			{
				moves++;
			}
		}
		ClearLS1B(pawnsCapturesEastBB);
	}
	uint64_t pawnsCapturesWestBB = (West(((PiecesBB[sideToMove][Pawn] & ~GameRecordPointer->pinnedRankFileBB) << 8) >> (sideToMove << 4))) & PiecesBB[sideToMove ^ 1][AllPieces];
	while (pawnsCapturesWestBB)
	{
		toSquare = GetLS1BIndex(pawnsCapturesWestBB);
		if (
			(!(CreateBitboardFromSquare(toSquare - (pmo - 1)) & GameRecordPointer->pinnedDiagonalBB)) || // Not pinned?
			(CreateBitboardFromSquare(toSquare) & LineListBB[toSquare - (pmo - 1)][kingSquare])
			)
		{
			if ((toSquare >> 3) == EigthRank[sideToMove])
			{
				moves += 4;
			}
			else
			{
				moves++;
			}
		}
		ClearLS1B(pawnsCapturesWestBB);
	}
	// En-passant
	if (GameRecordPointer->epSquare)
	{
		Move_Struct previousMove;
		previousMove.ui32 = (GameRecordPointer - 1)->move.ui32;
		if (West(CreateBitboardFromSquare(previousMove.mf.toSquare)) & PiecesBB[sideToMove][Pawn])
		{
			PiecesBB[sideToMove][Pawn] ^= (CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare));
			PiecesBB[sideToMove][AllPieces] ^= (CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare));
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			if (!IsAttacked(kingSquare, sideToMove ^ 1))
				moves++;
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
		}
		if (East(CreateBitboardFromSquare(previousMove.mf.toSquare)) & PiecesBB[sideToMove][Pawn])
		{
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			if (!IsAttacked(kingSquare, sideToMove ^ 1))
				moves++;
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
		}
	}

	// Knights
	uint64_t knightsBB = PiecesBB[sideToMove][Knight] & ~GameRecordPointer->pinnedAllBB;
	while (knightsBB)
	{
		fromSquare = GetLS1BIndex(knightsBB);
		attacksBB = KnightAttacksBBList[fromSquare] & ~PiecesBB[sideToMove][AllPieces];
		moves += PopulationCountX(attacksBB);
		ClearLS1B(knightsBB);
	}

	// Bishops and queens
	uint64_t bishopsAndQueensBB = PiecesBB[sideToMove][Bishop] | PiecesBB[sideToMove][Queen];
	while (bishopsAndQueensBB)
	{
		fromSquare = GetLS1BIndex(bishopsAndQueensBB);
		attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & ~PiecesBB[sideToMove][AllPieces];
		if (CreateBitboardFromSquare(fromSquare) & GameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		moves += PopulationCountX(attacksBB);
		ClearLS1B(bishopsAndQueensBB);
	}

	// Rooks and queens
	uint64_t rooksAndQueensBB = PiecesBB[sideToMove][Rook] | PiecesBB[sideToMove][Queen];
	while (rooksAndQueensBB)
	{
		fromSquare = GetLS1BIndex(rooksAndQueensBB);
		attacksBB = RookAttacksBB(fromSquare, occupiedBB) & ~PiecesBB[sideToMove][AllPieces];
		if (CreateBitboardFromSquare(fromSquare) & GameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		moves += PopulationCountX(attacksBB);
		ClearLS1B(rooksAndQueensBB);
	}

	// King
	attacksBB = KingAttacksBBList[kingSquare] & ~PiecesBB[sideToMove][AllPieces];
	while (attacksBB)
	{
		int toSquare = GetLS1BIndex(attacksBB);
		if (!IsAttacked(toSquare, sideToMove ^ 1))
			moves++;
		ClearLS1B(attacksBB);
	}

	// Generate castling moves
	if (UCI_Chess960)
	{
		// King side
		if (GameRecordPointer->castlingStatus.ui8[sideToMove][0] == 0)
		{
			// Check passed-over squares are empty and not attacked
			bool allEmpty;
			int offset;
			int square;

			allEmpty = true;

			square = kingSquare;
			offset = 1;
			//if (square > BackRankBaseSquareIndex[sideToMove] + G) // Not neccesary on king side
			//	offset = -1;
			while (true) // Scan king squares
			{
				if ((square != kingSquare) && ((square & 7) != InitialKingSideRookFile))
					if (MailboxBoard64[square] != Empty)
					{
						allEmpty = false;
						break;
					}
				if (IsAttacked(square, sideToMove ^ 1)) // Also check king squares for attacks
				{
					allEmpty = false;
					break;
				}
				if (square == BackRankBaseSquareIndex[sideToMove] + G)
					break;
				square += offset;
			}
			if (allEmpty)
			{
				square = kingSquare - InitialKingFile + InitialKingSideRookFile;
				offset = 1;
				if (square > BackRankBaseSquareIndex[sideToMove] + F)
					offset = -1;
				while (true) // Scan rook squares
				{
					if ((square != kingSquare) && ((square & 7) != InitialKingSideRookFile))
						if (MailboxBoard64[square] != Empty)
						{
							allEmpty = false;
							break;
						}
					if (square == BackRankBaseSquareIndex[sideToMove] + F)
						break;
					square += offset;
				}
				if (allEmpty)
				{
					if (((PiecesBB[sideToMove ^ 1][Rook] | PiecesBB[sideToMove ^ 1][Queen]) & CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + H)) == 0)
						moves++;
				}
			}
		}

		// Queen side
		if (GameRecordPointer->castlingStatus.ui8[sideToMove][1] == 0)
		{
			// Check passed-over squares are empty
			bool allEmpty;
			int offset;
			int square;

			allEmpty = true;

			square = kingSquare;
			offset = 1;
			if (square > BackRankBaseSquareIndex[sideToMove] + C)
				offset = -1;
			while (true) // Scan king squares
			{
				if ((square != kingSquare) && ((square & 7) != InitialQueenSideRookFile))
					if (MailboxBoard64[square] != Empty)
					{
						allEmpty = false;
						break;
					}
				if (IsAttacked(square, sideToMove ^ 1)) // Also check king squares for attacks
				{
					allEmpty = false;
					break;
				}
				if (square == BackRankBaseSquareIndex[sideToMove] + C)
					break;
				square += offset;
			}
			if (allEmpty)
			{
				square = kingSquare - InitialKingFile + InitialQueenSideRookFile;
				offset = 1;
				if (square > BackRankBaseSquareIndex[sideToMove] + D)
					offset = -1;
				while (true) // Scan rook squares
				{
					if ((square != kingSquare) && ((square & 7) != InitialQueenSideRookFile))
						if (MailboxBoard64[square] != Empty)
						{
							allEmpty = false;
							break;
						}
					if (square == BackRankBaseSquareIndex[sideToMove] + D)
						break;
					square += offset;
				}
				if (allEmpty)
				{
					uint64_t rooksAndQueensBB = PiecesBB[sideToMove ^ 1][Rook] | PiecesBB[sideToMove ^ 1][Queen];
					if ((rooksAndQueensBB & CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + B)) == 0)
						if (((rooksAndQueensBB & CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + A)) == 0) || ((MailboxBoard64[BackRankBaseSquareIndex[sideToMove] + B] != Empty) && (InitialQueenSideRookFile != B)))
							moves++;
				}
			}
		}
	}
	else
	{
		// King side
		if (GameRecordPointer->castlingStatus.ui8[sideToMove][0] == 0)
			if ((MailboxBoard64[kingSquare + 2] | MailboxBoard64[kingSquare + 1]) == Empty)
				if (!IsAttacked(kingSquare + 1, sideToMove ^ 1) && !IsAttacked(kingSquare + 2, sideToMove ^ 1))
					moves++;

		// Queen side
		if (GameRecordPointer->castlingStatus.ui8[sideToMove][1] == 0)
			if ((MailboxBoard64[kingSquare - 3] | MailboxBoard64[kingSquare - 2] | MailboxBoard64[kingSquare - 1]) == Empty)
				if (!IsAttacked(kingSquare - 1, sideToMove ^ 1) && !IsAttacked(kingSquare - 2, sideToMove ^ 1))
					moves++;
	}

	return moves;
}

bool Brain::AnyCapturesAndNonCaptures(int sideToMove)
{
	uint32_t fromSquare, toSquare;
	uint64_t attacksBB;
	uint64_t occupiedBB = PiecesBB[0][AllPieces] | PiecesBB[1][AllPieces];
	uint32_t kingSquare = GetLS1BIndex(PiecesBB[sideToMove][King]);
	//uint64_t moves = 0;
	bool canMove;

	// Pawns
	int pmo = PawnMoveOffset[sideToMove];
	uint64_t pawnMove1BB = (((PiecesBB[sideToMove][Pawn] & ~GameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB;
	while (pawnMove1BB)
	{
		toSquare = GetLS1BIndex(pawnMove1BB);
		if (
			(!(CreateBitboardFromSquare(toSquare - pmo) & GameRecordPointer->pinnedRankFileBB)) || // Not pinned?
			(CreateBitboardFromSquare(toSquare) & LineListBB[toSquare - pmo][kingSquare])
			)
		{
			//if ((toSquare >> 3) == EigthRank[sideToMove])
			//{
			//	moves += 4;
			//}
			//else
			//{
			//	moves++;
			//}
			return true;
		}
		ClearLS1B(pawnMove1BB);
	}
	uint64_t pawnMove2BB = ((((((PiecesBB[sideToMove][Pawn] & SecondRankBB[sideToMove] & ~GameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB) << 8) >> (sideToMove << 4)) & notOccupiedBB;
	while (pawnMove2BB)
	{
		toSquare = GetLS1BIndex(pawnMove2BB);
		if (
			(!(CreateBitboardFromSquare(toSquare - (pmo * 2)) & GameRecordPointer->pinnedRankFileBB)) ||
			(CreateBitboardFromSquare(toSquare) & LineListBB[toSquare - (pmo * 2)][kingSquare])
			)
			//moves++;
			return true;
		ClearLS1B(pawnMove2BB);
	}
	uint64_t pawnsCapturesEastBB = (East(((PiecesBB[sideToMove][Pawn] & ~GameRecordPointer->pinnedRankFileBB) << 8) >> (sideToMove << 4))) & PiecesBB[sideToMove ^ 1][AllPieces];
	while (pawnsCapturesEastBB)
	{
		toSquare = GetLS1BIndex(pawnsCapturesEastBB);
		if (
			(!(CreateBitboardFromSquare(toSquare - (pmo + 1)) & GameRecordPointer->pinnedDiagonalBB)) || // Not pinned?
			(CreateBitboardFromSquare(toSquare) & LineListBB[toSquare - (pmo + 1)][kingSquare])
			)
		{
			//if ((toSquare >> 3) == EigthRank[sideToMove])
			//{
			//	moves += 4;
			//}
			//else
			//{
			//	moves++;
			//}
			return true;
		}
		ClearLS1B(pawnsCapturesEastBB);
	}
	uint64_t pawnsCapturesWestBB = (West(((PiecesBB[sideToMove][Pawn] & ~GameRecordPointer->pinnedRankFileBB) << 8) >> (sideToMove << 4))) & PiecesBB[sideToMove ^ 1][AllPieces];
	while (pawnsCapturesWestBB)
	{
		toSquare = GetLS1BIndex(pawnsCapturesWestBB);
		if (
			(!(CreateBitboardFromSquare(toSquare - (pmo - 1)) & GameRecordPointer->pinnedDiagonalBB)) || // Not pinned?
			(CreateBitboardFromSquare(toSquare) & LineListBB[toSquare - (pmo - 1)][kingSquare])
			)
		{
			//if ((toSquare >> 3) == EigthRank[sideToMove])
			//{
			//	moves += 4;
			//}
			//else
			//{
			//	moves++;
			//}
			return true;
		}
		ClearLS1B(pawnsCapturesWestBB);
	}
	// En-passant
	if (GameRecordPointer->epSquare)
	{
		Move_Struct previousMove;
		previousMove.ui32 = (GameRecordPointer - 1)->move.ui32;
		if (West(CreateBitboardFromSquare(previousMove.mf.toSquare)) & PiecesBB[sideToMove][Pawn])
		{
			PiecesBB[sideToMove][Pawn] ^= (CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare));
			PiecesBB[sideToMove][AllPieces] ^= (CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare));
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			//if (!IsAttacked(kingSquare, sideToMove ^ 1))
				//moves++;
			canMove = !IsAttacked(kingSquare, sideToMove ^ 1);
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			if (canMove)
				return true;
		}
		if (East(CreateBitboardFromSquare(previousMove.mf.toSquare)) & PiecesBB[sideToMove][Pawn])
		{
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			//if (!IsAttacked(kingSquare, sideToMove ^ 1))
				//moves++;
			canMove = !IsAttacked(kingSquare, sideToMove ^ 1);
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			if (canMove)
				return true;
		}
	}

	// Knights
	uint64_t knightsBB = PiecesBB[sideToMove][Knight] & ~GameRecordPointer->pinnedAllBB;
	while (knightsBB)
	{
		fromSquare = GetLS1BIndex(knightsBB);
		attacksBB = KnightAttacksBBList[fromSquare] & ~PiecesBB[sideToMove][AllPieces];
		//moves += PopulationCountX(attacksBB);
		if (PopulationCountX(attacksBB))
			return true;
		ClearLS1B(knightsBB);
	}

	// Bishops and queens
	uint64_t bishopsAndQueensBB = PiecesBB[sideToMove][Bishop] | PiecesBB[sideToMove][Queen];
	while (bishopsAndQueensBB)
	{
		fromSquare = GetLS1BIndex(bishopsAndQueensBB);
		attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & ~PiecesBB[sideToMove][AllPieces];
		if (CreateBitboardFromSquare(fromSquare) & GameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		//moves += PopulationCountX(attacksBB);
		if (PopulationCountX(attacksBB))
			return true;
		ClearLS1B(bishopsAndQueensBB);
	}

	// Rooks and queens
	uint64_t rooksAndQueensBB = PiecesBB[sideToMove][Rook] | PiecesBB[sideToMove][Queen];
	while (rooksAndQueensBB)
	{
		fromSquare = GetLS1BIndex(rooksAndQueensBB);
		attacksBB = RookAttacksBB(fromSquare, occupiedBB) & ~PiecesBB[sideToMove][AllPieces];
		if (CreateBitboardFromSquare(fromSquare) & GameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		//moves += PopulationCountX(attacksBB);
		if (PopulationCountX(attacksBB))
			return true;
		ClearLS1B(rooksAndQueensBB);
	}

	// King
	attacksBB = KingAttacksBBList[kingSquare] & ~PiecesBB[sideToMove][AllPieces];
	while (attacksBB)
	{
		int toSquare = GetLS1BIndex(attacksBB);
		if (!IsAttacked(toSquare, sideToMove ^ 1))
			//moves++;
			return true;
		ClearLS1B(attacksBB);
	}

	// THERE IS NO NEED TO CHECK FOR CASTLING MOVES AS THERE WOULD HAVE TO BE AT LEAST ONE LEGAL KING MOVE ABOVE
	//// Generate castling moves
	//if (UCI_Chess960)
	//{
	//	// King side
	//	if (gameRecordPointer->castlingStatus.ui8[sideToMove][0] == 0)
	//	{
	//		// Check passed-over squares are empty and not attacked
	//		bool allEmpty;
	//		int offset;
	//		int square;

	//		allEmpty = true;

	//		square = kingSquare;
	//		offset = 1;
	//		//if (square > BackRankBaseSquareIndex[sideToMove] + G) // Not neccesary on king side
	//		//	offset = -1;
	//		while (true) // Scan king squares
	//		{
	//			if ((square != kingSquare) && ((square & 7) != InitialKingSideRookFile))
	//				if (mailboxBoard64[square] != Empty)
	//				{
	//					allEmpty = false;
	//					break;
	//				}
	//			if (IsAttacked(square, sideToMove ^ 1)) // Also check king squares for attacks
	//			{
	//				allEmpty = false;
	//				break;
	//			}
	//			if (square == BackRankBaseSquareIndex[sideToMove] + G)
	//				break;
	//			square += offset;
	//		}
	//		if (allEmpty)
	//		{
	//			square = kingSquare - InitialKingFile + InitialKingSideRookFile;
	//			offset = 1;
	//			if (square > BackRankBaseSquareIndex[sideToMove] + F)
	//				offset = -1;
	//			while (true) // Scan rook squares
	//			{
	//				if ((square != kingSquare) && ((square & 7) != InitialKingSideRookFile))
	//					if (mailboxBoard64[square] != Empty)
	//					{
	//						allEmpty = false;
	//						break;
	//					}
	//				if (square == BackRankBaseSquareIndex[sideToMove] + F)
	//					break;
	//				square += offset;
	//			}
	//			if (allEmpty)
	//			{
	//				if (((piecesBB[sideToMove ^ 1][Rook] | piecesBB[sideToMove ^ 1][Queen]) & CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + H)) == 0)
	//					//moves++;
	//					return true;
	//			}
	//		}
	//	}

	//	// Queen side
	//	if (gameRecordPointer->castlingStatus.ui8[sideToMove][1] == 0)
	//	{
	//		// Check passed-over squares are empty
	//		bool allEmpty;
	//		int offset;
	//		int square;

	//		allEmpty = true;

	//		square = kingSquare;
	//		offset = 1;
	//		if (square > BackRankBaseSquareIndex[sideToMove] + C)
	//			offset = -1;
	//		while (true) // Scan king squares
	//		{
	//			if ((square != kingSquare) && ((square & 7) != InitialQueenSideRookFile))
	//				if (mailboxBoard64[square] != Empty)
	//				{
	//					allEmpty = false;
	//					break;
	//				}
	//			if (IsAttacked(square, sideToMove ^ 1)) // Also check king squares for attacks
	//			{
	//				allEmpty = false;
	//				break;
	//			}
	//			if (square == BackRankBaseSquareIndex[sideToMove] + C)
	//				break;
	//			square += offset;
	//		}
	//		if (allEmpty)
	//		{
	//			square = kingSquare - InitialKingFile + InitialQueenSideRookFile;
	//			offset = 1;
	//			if (square > BackRankBaseSquareIndex[sideToMove] + D)
	//				offset = -1;
	//			while (true) // Scan rook squares
	//			{
	//				if ((square != kingSquare) && ((square & 7) != InitialQueenSideRookFile))
	//					if (mailboxBoard64[square] != Empty)
	//					{
	//						allEmpty = false;
	//						break;
	//					}
	//				if (square == BackRankBaseSquareIndex[sideToMove] + D)
	//					break;
	//				square += offset;
	//			}
	//			if (allEmpty)
	//			{
	//				uint64_t rooksAndQueensBB = piecesBB[sideToMove ^ 1][Rook] | piecesBB[sideToMove ^ 1][Queen];
	//				if ((rooksAndQueensBB & CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + B)) == 0)
	//					if (((rooksAndQueensBB & CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + A)) == 0) || ((mailboxBoard64[BackRankBaseSquareIndex[sideToMove] + B] != Empty) && (InitialQueenSideRookFile != B)))
	//						//moves++;
	//						return true;
	//			}
	//		}
	//	}
	//}
	//else
	//{
	//	// King side
	//	if (gameRecordPointer->castlingStatus.ui8[sideToMove][0] == 0)
	//		if ((mailboxBoard64[kingSquare + 2] | mailboxBoard64[kingSquare + 1]) == Empty)
	//			if (!IsAttacked(kingSquare + 1, sideToMove ^ 1) && !IsAttacked(kingSquare + 2, sideToMove ^ 1))
	//				//moves++;
	//				return true;

	//	// Queen side
	//	if (gameRecordPointer->castlingStatus.ui8[sideToMove][1] == 0)
	//		if ((mailboxBoard64[kingSquare - 3] | mailboxBoard64[kingSquare - 2] | mailboxBoard64[kingSquare - 1]) == Empty)
	//			if (!IsAttacked(kingSquare - 1, sideToMove ^ 1) && !IsAttacked(kingSquare - 2, sideToMove ^ 1))
	//				//moves++;
	//				return true;
	//}

	return false;
}

MoveWithScore_Struct* Brain::GenerateAllMovesOutOfCheck(int sideToMove, MoveWithScore_Struct* mlp, bool generateUnderPromotions)
{
	uint32_t fromSquare, toSquare;
	uint64_t attacksBB;
	uint64_t occupiedBB = PiecesBB[0][AllPieces] | PiecesBB[1][AllPieces];
	uint32_t kingSquare = GetLS1BIndex(PiecesBB[sideToMove][King]);

	// Generate a bitboard containing all the checking pieces
	uint64_t enemyCheckersBB =
		(BishopAttacksBB(kingSquare, occupiedBB) & (PiecesBB[sideToMove ^ 1][Bishop] | PiecesBB[sideToMove ^ 1][Queen])) |
		(RookAttacksBB(kingSquare, occupiedBB) & (PiecesBB[sideToMove ^ 1][Rook] | PiecesBB[sideToMove ^ 1][Queen])) |
		(KnightAttacksBBList[kingSquare] & PiecesBB[sideToMove ^ 1][Knight]) |
		(PawnAttacksBBList[sideToMove][kingSquare] & PiecesBB[sideToMove ^ 1][Pawn])
		;
	assert(PopulationCountX(enemyCheckersBB) >= 1);
	if (PopulationCountX(enemyCheckersBB) > 1) // If in double-check then only king moves are possible
		goto king;

	// So we now know that enemyCheckersBB has only one bit set
	// N.B. If we are in check a pinned piece cannot move at all

	int checkerSquare = GetLS1BIndex(enemyCheckersBB);
	uint64_t enemyCheckersBetweenSquaresBB = BetweenListBB[checkerSquare][kingSquare];

	// Pawns
	int pmo = PawnMoveOffset[sideToMove];
	uint64_t pawnsCapturesEastBB = (East((PiecesBB[sideToMove][Pawn] << 8) >> (sideToMove << 4))) & enemyCheckersBB;
	if (pawnsCapturesEastBB) // At most one bit set
	{
		toSquare = GetLS1BIndex(pawnsCapturesEastBB);
		if (!(CreateBitboardFromSquare(toSquare - (pmo + 1)) & GameRecordPointer->pinnedAllBB)) // Not pinned?
		{
			if ((toSquare >> 3) == EigthRank[sideToMove])
			{
				mlp++->ui32 = (toSquare - (pmo + 1)) | (toSquare << 8) | (MFPromoteToQueen << 16);
				if (generateUnderPromotions)
				{
					mlp++->ui32 = (toSquare - (pmo + 1)) | (toSquare << 8) | (MFPromoteToRook << 16);
					mlp++->ui32 = (toSquare - (pmo + 1)) | (toSquare << 8) | (MFPromoteToBishop << 16);
					mlp++->ui32 = (toSquare - (pmo + 1)) | (toSquare << 8) | (MFPromoteToKnight << 16);
				}
			}
			else
				mlp++->ui32 = (toSquare - (pmo + 1)) | (toSquare << 8);
		}
	}
	uint64_t pawnsCapturesWestBB = (West((PiecesBB[sideToMove][Pawn] << 8) >> (sideToMove << 4))) & enemyCheckersBB;
	if (pawnsCapturesWestBB)
	{
		toSquare = GetLS1BIndex(pawnsCapturesWestBB);
		if (!(CreateBitboardFromSquare(toSquare - (pmo - 1)) & GameRecordPointer->pinnedAllBB)) // Not pinned?
		{
			if ((toSquare >> 3) == EigthRank[sideToMove])
			{
				mlp++->ui32 = (toSquare - (pmo - 1)) | (toSquare << 8) | (MFPromoteToQueen << 16);
				if (generateUnderPromotions)
				{
					mlp++->ui32 = (toSquare - (pmo - 1)) | (toSquare << 8) | (MFPromoteToRook << 16);
					mlp++->ui32 = (toSquare - (pmo - 1)) | (toSquare << 8) | (MFPromoteToBishop << 16);
					mlp++->ui32 = (toSquare - (pmo - 1)) | (toSquare << 8) | (MFPromoteToKnight << 16);
				}
			}
			else
				mlp++->ui32 = (toSquare - (pmo - 1)) | (toSquare << 8);
		}
	}
	// En-passant
	if (GameRecordPointer->epSquare)
	{
		Move_Struct previousMove;
		previousMove.ui32 = (GameRecordPointer - 1)->move.ui32;
		if (West(CreateBitboardFromSquare(previousMove.mf.toSquare)) & PiecesBB[sideToMove][Pawn])
		{
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			if (!IsAttacked(kingSquare, sideToMove ^ 1))
				mlp++->ui32 = (previousMove.mf.toSquare - 1) | ((previousMove.mf.toSquare) << 8) | (MFEnPassant << 16);
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
		}
		if (East(CreateBitboardFromSquare(previousMove.mf.toSquare)) & PiecesBB[sideToMove][Pawn])
		{
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			if (!IsAttacked(kingSquare, sideToMove ^ 1))
				mlp++->ui32 = (previousMove.mf.toSquare + 1) | ((previousMove.mf.toSquare) << 8) | (MFEnPassant << 16);
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
		}
	}
	uint64_t pawnMove1BB = ((PiecesBB[sideToMove][Pawn] << 8) >> (sideToMove << 4)) & enemyCheckersBetweenSquaresBB;
	while (pawnMove1BB)
	{
		toSquare = GetLS1BIndex(pawnMove1BB);
		if (!(CreateBitboardFromSquare(toSquare - pmo) & GameRecordPointer->pinnedAllBB)) // Not pinned?
		{
			if ((toSquare >> 3) == EigthRank[sideToMove])
			{
				mlp++->ui32 = (toSquare - pmo) | (toSquare << 8) | (MFPromoteToQueen << 16);
				if (generateUnderPromotions)
				{
					mlp++->ui32 = (toSquare - pmo) | (toSquare << 8) | (MFPromoteToRook << 16);
					mlp++->ui32 = (toSquare - pmo) | (toSquare << 8) | (MFPromoteToBishop << 16);
					mlp++->ui32 = (toSquare - pmo) | (toSquare << 8) | (MFPromoteToKnight << 16);
				}
			}
			else
				mlp++->ui32 = (toSquare - pmo) | (toSquare << 8);
		}
		ClearLS1B(pawnMove1BB);
	}
	uint64_t pawnMove2BB = ((((((PiecesBB[sideToMove][Pawn] & SecondRankBB[sideToMove]) << 8) >> (sideToMove << 4)) & notOccupiedBB) << 8) >> (sideToMove << 4)) & enemyCheckersBetweenSquaresBB;
	while (pawnMove2BB)
	{
		toSquare = GetLS1BIndex(pawnMove2BB);
		if (!(CreateBitboardFromSquare(toSquare - (pmo * 2)) & GameRecordPointer->pinnedAllBB)) // Not pinned?
			mlp++->ui32 = toSquare - (pmo * 2) | (toSquare << 8);
		ClearLS1B(pawnMove2BB);
	}

	// Knights
	uint64_t knightsBB = PiecesBB[sideToMove][Knight] & ~GameRecordPointer->pinnedAllBB;
	while (knightsBB)
	{
		fromSquare = GetLS1BIndex(knightsBB);
		attacksBB = KnightAttacksBBList[fromSquare] & (enemyCheckersBB | enemyCheckersBetweenSquaresBB);
		while (attacksBB)
		{
			int toSquare = GetLS1BIndex(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(knightsBB);
	}

	// Bishops and queens
	//uint64_t bishopsAndQueensBB = piecesBB[sideToMove][Bishop] | piecesBB[sideToMove][Queen];
	uint64_t bishopsAndQueensBB = (PiecesBB[sideToMove][Bishop] | PiecesBB[sideToMove][Queen]) & ~GameRecordPointer->pinnedAllBB;
	while (bishopsAndQueensBB)
	{
		fromSquare = GetLS1BIndex(bishopsAndQueensBB);
		//if (!(CreateBitboardFromSquare(fromSquare) & gameRecordPointer->pinnedAllBB)) // Not pinned?
		{
			attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & (enemyCheckersBB | enemyCheckersBetweenSquaresBB);
			while (attacksBB)
			{
				int toSquare = GetLS1BIndex(attacksBB);
				mlp++->ui32 = fromSquare | (toSquare << 8);
				ClearLS1B(attacksBB);
			}
		}
		ClearLS1B(bishopsAndQueensBB);
	}

	// Rooks and queens
	//uint64_t rooksAndQueensBB = piecesBB[sideToMove][Rook] | piecesBB[sideToMove][Queen];
	uint64_t rooksAndQueensBB = (PiecesBB[sideToMove][Rook] | PiecesBB[sideToMove][Queen]) & ~GameRecordPointer->pinnedAllBB;
	while (rooksAndQueensBB)
	{
		fromSquare = GetLS1BIndex(rooksAndQueensBB);
		//if (!(CreateBitboardFromSquare(fromSquare) & gameRecordPointer->pinnedAllBB)) // Not pinned?
		{
			attacksBB = RookAttacksBB(fromSquare, occupiedBB) & (enemyCheckersBB | enemyCheckersBetweenSquaresBB);
			while (attacksBB)
			{
				int toSquare = GetLS1BIndex(attacksBB);
				mlp++->ui32 = fromSquare | (toSquare << 8);
				ClearLS1B(attacksBB);
			}
		}
		ClearLS1B(rooksAndQueensBB);
	}

	// King
king:
	attacksBB = KingAttacksBBList[kingSquare] & ~PiecesBB[sideToMove][AllPieces];
	PiecesBB[sideToMove][AllPieces] ^= PiecesBB[sideToMove][King];
	while (attacksBB)
	{
		int toSquare = GetLS1BIndex(attacksBB);
		if (!IsAttacked(toSquare, sideToMove ^ 1))
			mlp++->ui32 = kingSquare | (toSquare << 8);
		ClearLS1B(attacksBB);
	}
	PiecesBB[sideToMove][AllPieces] ^= PiecesBB[sideToMove][King];

	return mlp;
}

uint32_t Brain::CountAllMovesOutOfCheck(int sideToMove)
{
	uint32_t fromSquare, toSquare;
	uint64_t attacksBB;
	uint64_t occupiedBB = PiecesBB[0][AllPieces] | PiecesBB[1][AllPieces];
	uint32_t kingSquare = GetLS1BIndex(PiecesBB[sideToMove][King]);
	uint64_t moves = 0;

	// Generate a bitboard containing all the checking pieces
	uint64_t enemyCheckersBB =
		(BishopAttacksBB(kingSquare, occupiedBB) & (PiecesBB[sideToMove ^ 1][Bishop] | PiecesBB[sideToMove ^ 1][Queen])) |
		(RookAttacksBB(kingSquare, occupiedBB) & (PiecesBB[sideToMove ^ 1][Rook] | PiecesBB[sideToMove ^ 1][Queen])) |
		(KnightAttacksBBList[kingSquare] & PiecesBB[sideToMove ^ 1][Knight]) |
		(PawnAttacksBBList[sideToMove][kingSquare] & PiecesBB[sideToMove ^ 1][Pawn])
		;
	assert(PopulationCountX(enemyCheckersBB) >= 1);
	if (PopulationCountX(enemyCheckersBB) > 1) // If in double-check then only King moves are possible
		goto king;

	// So we now know that enemyCheckersBB has only one bit set
	// N.B. If we are in check a pinned piece cannot move at all

	int checkerSquare = GetLS1BIndex(enemyCheckersBB);
	uint64_t enemyCheckersBetweenSquaresBB = BetweenListBB[checkerSquare][kingSquare];

	// Pawns
	int pmo = PawnMoveOffset[sideToMove];
	uint64_t pawnsCapturesEastBB = (East((PiecesBB[sideToMove][Pawn] << 8) >> (sideToMove << 4))) & enemyCheckersBB;
	if (pawnsCapturesEastBB) // At most one bit set
	{
		toSquare = GetLS1BIndex(pawnsCapturesEastBB);
		if (!(CreateBitboardFromSquare(toSquare - (pmo + 1)) & GameRecordPointer->pinnedAllBB)) // Not pinned?
		{
			if ((toSquare >> 3) == EigthRank[sideToMove])
			{
				moves += 4;
			}
			else
				moves++;
		}
	}
	uint64_t pawnsCapturesWestBB = (West((PiecesBB[sideToMove][Pawn] << 8) >> (sideToMove << 4))) & enemyCheckersBB;
	if (pawnsCapturesWestBB)
	{
		toSquare = GetLS1BIndex(pawnsCapturesWestBB);
		if (!(CreateBitboardFromSquare(toSquare - (pmo - 1)) & GameRecordPointer->pinnedAllBB)) // Not pinned?
		{
			if ((toSquare >> 3) == EigthRank[sideToMove])
			{
				moves += 4;
			}
			else
				moves++;
		}
	}
	// En-passant
	if (GameRecordPointer->epSquare)
	{
		Move_Struct previousMove;
		previousMove.ui32 = (GameRecordPointer - 1)->move.ui32;
		if (West(CreateBitboardFromSquare(previousMove.mf.toSquare)) & PiecesBB[sideToMove][Pawn])
		{
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			if (!IsAttacked(kingSquare, sideToMove ^ 1))
				moves++;
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
		}
		if (East(CreateBitboardFromSquare(previousMove.mf.toSquare)) & PiecesBB[sideToMove][Pawn])
		{
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			if (!IsAttacked(kingSquare, sideToMove ^ 1))
				moves++;
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
		}
	}
	uint64_t pawnMove1BB = ((PiecesBB[sideToMove][Pawn] << 8) >> (sideToMove << 4)) & enemyCheckersBetweenSquaresBB;
	while (pawnMove1BB)
	{
		toSquare = GetLS1BIndex(pawnMove1BB);
		if (!(CreateBitboardFromSquare(toSquare - pmo) & GameRecordPointer->pinnedAllBB)) // Not pinned?
		{
			if ((toSquare >> 3) == EigthRank[sideToMove])
			{
				moves += 4;
			}
			else
				moves++;
		}
		ClearLS1B(pawnMove1BB);
	}
	uint64_t pawnMove2BB = ((((((PiecesBB[sideToMove][Pawn] & SecondRankBB[sideToMove]) << 8) >> (sideToMove << 4)) & notOccupiedBB) << 8) >> (sideToMove << 4)) & enemyCheckersBetweenSquaresBB;
	while (pawnMove2BB)
	{
		toSquare = GetLS1BIndex(pawnMove2BB);
		if (!(CreateBitboardFromSquare(toSquare - (pmo * 2)) & GameRecordPointer->pinnedAllBB)) // Not pinned?
			moves++;
		ClearLS1B(pawnMove2BB);
	}

	// If we are in check then pinned pieces can't move at all as they couldn't capture the checker/interrupt the check without exposing the king

	// Knights
	uint64_t knightsBB = PiecesBB[sideToMove][Knight] & ~GameRecordPointer->pinnedAllBB;//CAN DO PINNED TEST HERE AS IN COUNTCPAS&NONCAPS!
	while (knightsBB)
	{
		fromSquare = GetLS1BIndex(knightsBB);
		//if (!(CreateBitboardFromSquare(fromSquare) & gameRecordPointer->pinnedAllBB)) // Not pinned?
		{
			attacksBB = KnightAttacksBBList[fromSquare] & (enemyCheckersBB | enemyCheckersBetweenSquaresBB);
			moves += PopulationCountX(attacksBB);
		}
		ClearLS1B(knightsBB);
	}

	// Bishops and queens
	uint64_t bishopsAndQueensBB = (PiecesBB[sideToMove][Bishop] | PiecesBB[sideToMove][Queen]) & ~GameRecordPointer->pinnedAllBB;
	while (bishopsAndQueensBB)
	{
		fromSquare = GetLS1BIndex(bishopsAndQueensBB);
		//if (!(CreateBitboardFromSquare(fromSquare) & gameRecordPointer->pinnedAllBB)) // Not pinned?
		{
			attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & (enemyCheckersBB | enemyCheckersBetweenSquaresBB);
			moves += PopulationCountX(attacksBB);
		}
		ClearLS1B(bishopsAndQueensBB);
	}

	// Rooks and queens
	uint64_t rooksAndQueensBB = (PiecesBB[sideToMove][Rook] | PiecesBB[sideToMove][Queen]) & ~GameRecordPointer->pinnedAllBB;
	while (rooksAndQueensBB)
	{
		fromSquare = GetLS1BIndex(rooksAndQueensBB);
		//if (!(CreateBitboardFromSquare(fromSquare) & gameRecordPointer->pinnedAllBB)) // Not pinned?
		{
			attacksBB = RookAttacksBB(fromSquare, occupiedBB) & (enemyCheckersBB | enemyCheckersBetweenSquaresBB);
			moves += PopulationCountX(attacksBB);
		}
		ClearLS1B(rooksAndQueensBB);
	}

	// King
king:
	attacksBB = KingAttacksBBList[kingSquare] & ~PiecesBB[sideToMove][AllPieces];
	PiecesBB[sideToMove][AllPieces] ^= PiecesBB[sideToMove][King];
	while (attacksBB)
	{
		int toSquare = GetLS1BIndex(attacksBB);
		if (!IsAttacked(toSquare, sideToMove ^ 1))
			moves++;
		ClearLS1B(attacksBB);
	}
	PiecesBB[sideToMove][AllPieces] ^= PiecesBB[sideToMove][King];

	return moves;
}

bool Brain::AnyMovesOutOfCheck(int sideToMove)
{
	uint32_t fromSquare, toSquare;
	uint64_t attacksBB;
	uint64_t occupiedBB = PiecesBB[0][AllPieces] | PiecesBB[1][AllPieces];
	uint32_t kingSquare = GetLS1BIndex(PiecesBB[sideToMove][King]);
	//uint64_t moves = 0;
	bool canMove;

	// Generate a bitboard containing all the checking pieces
	uint64_t enemyCheckersBB =
		(BishopAttacksBB(kingSquare, occupiedBB) & (PiecesBB[sideToMove ^ 1][Bishop] | PiecesBB[sideToMove ^ 1][Queen])) |
		(RookAttacksBB(kingSquare, occupiedBB) & (PiecesBB[sideToMove ^ 1][Rook] | PiecesBB[sideToMove ^ 1][Queen])) |
		(KnightAttacksBBList[kingSquare] & PiecesBB[sideToMove ^ 1][Knight]) |
		(PawnAttacksBBList[sideToMove][kingSquare] & PiecesBB[sideToMove ^ 1][Pawn])
		;
	assert(PopulationCountX(enemyCheckersBB) >= 1);
	if (PopulationCountX(enemyCheckersBB) > 1) // If in double-check then only King moves are possible
		goto king;

	// So we now know that enemyCheckersBB has only one bit set
	// N.B. If we are in check a pinned piece cannot move at all

	int checkerSquare = GetLS1BIndex(enemyCheckersBB);
	uint64_t enemyCheckersBetweenSquaresBB = BetweenListBB[checkerSquare][kingSquare];

	// Pawns
	int pmo = PawnMoveOffset[sideToMove];
	uint64_t pawnsCapturesEastBB = (East((PiecesBB[sideToMove][Pawn] << 8) >> (sideToMove << 4))) & enemyCheckersBB;
	if (pawnsCapturesEastBB) // At most one bit set
	{
		toSquare = GetLS1BIndex(pawnsCapturesEastBB);
		if (!(CreateBitboardFromSquare(toSquare - (pmo + 1)) & GameRecordPointer->pinnedAllBB)) // Not pinned?
		{
			//if ((toSquare >> 3) == EigthRank[sideToMove])
			//{
			//	moves += 4;
			//}
			//else
			//	moves++;
			return true;
		}
	}
	uint64_t pawnsCapturesWestBB = (West((PiecesBB[sideToMove][Pawn] << 8) >> (sideToMove << 4))) & enemyCheckersBB;
	if (pawnsCapturesWestBB)
	{
		toSquare = GetLS1BIndex(pawnsCapturesWestBB);
		if (!(CreateBitboardFromSquare(toSquare - (pmo - 1)) & GameRecordPointer->pinnedAllBB)) // Not pinned?
		{
			//if ((toSquare >> 3) == EigthRank[sideToMove])
			//{
			//	moves += 4;
			//}
			//else
			//	moves++;
			return true;
		}
	}
	// En-passant
	if (GameRecordPointer->epSquare)
	{
		Move_Struct previousMove;
		previousMove.ui32 = (GameRecordPointer - 1)->move.ui32;
		if (West(CreateBitboardFromSquare(previousMove.mf.toSquare)) & PiecesBB[sideToMove][Pawn])
		{
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			//if (!IsAttacked(kingSquare, sideToMove ^ 1))
				//moves++;
			canMove = !IsAttacked(kingSquare, sideToMove ^ 1);
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			if (canMove)
				return true;
		}
		if (East(CreateBitboardFromSquare(previousMove.mf.toSquare)) & PiecesBB[sideToMove][Pawn])
		{
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			//if (!IsAttacked(kingSquare, sideToMove ^ 1))
				//moves++;
			canMove = !IsAttacked(kingSquare, sideToMove ^ 1);
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			if (canMove)
				return true;
		}

	}
	uint64_t pawnMove1BB = ((PiecesBB[sideToMove][Pawn] << 8) >> (sideToMove << 4)) & enemyCheckersBetweenSquaresBB;
	while (pawnMove1BB)
	{
		toSquare = GetLS1BIndex(pawnMove1BB);
		if (!(CreateBitboardFromSquare(toSquare - pmo) & GameRecordPointer->pinnedAllBB)) // Not pinned?
		{
			//if ((toSquare >> 3) == EigthRank[sideToMove])
			//{
			//	moves += 4;
			//}
			//else
			//	moves++;
			return true;
		}
		ClearLS1B(pawnMove1BB);
	}
	uint64_t pawnMove2BB = ((((((PiecesBB[sideToMove][Pawn] & SecondRankBB[sideToMove]) << 8) >> (sideToMove << 4)) & notOccupiedBB) << 8) >> (sideToMove << 4)) & enemyCheckersBetweenSquaresBB;
	while (pawnMove2BB)
	{
		toSquare = GetLS1BIndex(pawnMove2BB);
		if (!(CreateBitboardFromSquare(toSquare - (pmo * 2)) & GameRecordPointer->pinnedAllBB)) // Not pinned?
			//moves++;
			return true;
		ClearLS1B(pawnMove2BB);
	}

	// If we are in check then pinned pieces can't move at all as they couldn't capture the checker/interrupt the check without exposing the king

	// Knights
	uint64_t knightsBB = PiecesBB[sideToMove][Knight] & ~GameRecordPointer->pinnedAllBB;//CAN DO PINNED TEST HERE AS IN COUNTCPAS&NONCAPS!
	while (knightsBB)
	{
		fromSquare = GetLS1BIndex(knightsBB);
		//if (!(CreateBitboardFromSquare(fromSquare) & gameRecordPointer->pinnedAllBB)) // Not pinned?
		{
			attacksBB = KnightAttacksBBList[fromSquare] & (enemyCheckersBB | enemyCheckersBetweenSquaresBB);
			//moves += PopulationCountX(attacksBB);
			if (PopulationCountX(attacksBB))
				return true;
		}
		ClearLS1B(knightsBB);
	}

	// Bishops and queens
	uint64_t bishopsAndQueensBB = (PiecesBB[sideToMove][Bishop] | PiecesBB[sideToMove][Queen]) & ~GameRecordPointer->pinnedAllBB;
	while (bishopsAndQueensBB)
	{
		fromSquare = GetLS1BIndex(bishopsAndQueensBB);
		//if (!(CreateBitboardFromSquare(fromSquare) & gameRecordPointer->pinnedAllBB)) // Not pinned?
		{
			attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & (enemyCheckersBB | enemyCheckersBetweenSquaresBB);
			//moves += PopulationCountX(attacksBB);
			if (PopulationCountX(attacksBB))
				return true;
		}
		ClearLS1B(bishopsAndQueensBB);
	}

	// Rooks and queens
	uint64_t rooksAndQueensBB = (PiecesBB[sideToMove][Rook] | PiecesBB[sideToMove][Queen]) & ~GameRecordPointer->pinnedAllBB;
	while (rooksAndQueensBB)
	{
		fromSquare = GetLS1BIndex(rooksAndQueensBB);
		//if (!(CreateBitboardFromSquare(fromSquare) & gameRecordPointer->pinnedAllBB)) // Not pinned?
		{
			attacksBB = RookAttacksBB(fromSquare, occupiedBB) & (enemyCheckersBB | enemyCheckersBetweenSquaresBB);
			//moves += PopulationCountX(attacksBB);
			if (PopulationCountX(attacksBB))
				return true;
		}
		ClearLS1B(rooksAndQueensBB);
	}

	// King
king:
	attacksBB = KingAttacksBBList[kingSquare] & ~PiecesBB[sideToMove][AllPieces];
	PiecesBB[sideToMove][AllPieces] ^= PiecesBB[sideToMove][King];
	canMove = false;
	while (attacksBB)
	{
		int toSquare = GetLS1BIndex(attacksBB);
		if (!IsAttacked(toSquare, sideToMove ^ 1))
			//moves++;
		{
			canMove = true;
			break;
		}
		ClearLS1B(attacksBB);
	}
	PiecesBB[sideToMove][AllPieces] ^= PiecesBB[sideToMove][King];
	if (canMove)
		return true;

	return false;
}

MoveWithScore_Struct* Brain::GenerateNonCaptureNonPromotionDirectChecks(int sideToMove, MoveWithScore_Struct* mlp)
{
	int fromSquare, toSquare;
	uint64_t attacksBB;
	uint64_t occupiedBB = PiecesBB[0][AllPieces] | PiecesBB[1][AllPieces];
	int kingSquare = GetLS1BIndex(PiecesBB[sideToMove][King]);
	int enemyKingSquare = GetLS1BIndex(PiecesBB[sideToMove ^ 1][King]);
	uint64_t enemyKingPawnAttacksBB = PawnAttacksBBList[sideToMove ^ 1][enemyKingSquare];
	uint64_t enemyKingKnightAttacksBB = KnightAttacksBBList[enemyKingSquare];
	uint64_t enemyKingBishopAttacksBB = BishopAttacksBB(enemyKingSquare, occupiedBB);
	uint64_t enemyKingRookAttacksBB = RookAttacksBB(enemyKingSquare, occupiedBB);
	//uint64_t fromSquareBB, validBB;

	// Pawns
	int pmo = PawnMoveOffset[sideToMove];
	uint64_t pawnMove1BB = (((PiecesBB[sideToMove][Pawn] & ~SeventhRankBB[sideToMove] & ~GameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB & enemyKingPawnAttacksBB;
	while (pawnMove1BB)
	{
		toSquare = GetLS1BIndex(pawnMove1BB);
		if (
			(!(CreateBitboardFromSquare(toSquare - pmo) & GameRecordPointer->pinnedRankFileBB)) || // Not pinned?
			(CreateBitboardFromSquare(toSquare) & LineListBB[toSquare - pmo][kingSquare]) // Is the to-square on the line between the from-square and the king?
			)
			mlp++->ui32 = (toSquare - pmo) | (toSquare << 8);
		ClearLS1B(pawnMove1BB);
	}
	uint64_t pawnMove2BB = ((((((PiecesBB[sideToMove][Pawn] & SecondRankBB[sideToMove] & ~GameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB) << 8) >> (sideToMove << 4)) & notOccupiedBB & enemyKingPawnAttacksBB;
	while (pawnMove2BB)
	{
		toSquare = GetLS1BIndex(pawnMove2BB);
		if (
			(!(CreateBitboardFromSquare(toSquare - (pmo * 2)) & GameRecordPointer->pinnedRankFileBB)) ||
			(CreateBitboardFromSquare(toSquare) & LineListBB[toSquare - (pmo * 2)][kingSquare])
			)
			mlp++->ui32 = (toSquare - (pmo * 2)) | (toSquare << 8);
		ClearLS1B(pawnMove2BB);
	}

	// Knights
	uint64_t knightsBB = PiecesBB[sideToMove][Knight] & ~GameRecordPointer->pinnedAllBB;
	while (knightsBB)
	{
		fromSquare = GetLS1BIndex(knightsBB);
		//validBB = enemyKingKnightAttacksBB; // Discovered check?
		//fromSquareBB = CreateBitboardFromSquare(fromSquare);
		//if ((fromSquareBB & enemyKingRookAttacksBB) && (RookAttacksBB(enemyKingSquare, occupiedBB ^ fromSquareBB) & (PiecesBB[sideToMove][Rook] | PiecesBB[sideToMove][Queen])))
		//	validBB = -1;
		//else if ((fromSquareBB & enemyKingBishopAttacksBB) && (BishopAttacksBB(enemyKingSquare, occupiedBB ^ fromSquareBB) & (PiecesBB[sideToMove][Bishop] | PiecesBB[sideToMove][Queen])))
		//	validBB = -1;
		//attacksBB = KnightAttacksBBList[fromSquare] & notOccupiedBB & validBB;
		attacksBB = KnightAttacksBBList[fromSquare] & notOccupiedBB & enemyKingKnightAttacksBB;
		while (attacksBB)
		{
			toSquare = GetLS1BIndex(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(knightsBB);
	}

	// Bishops
	uint64_t bishopsBB = PiecesBB[sideToMove][Bishop];
	while (bishopsBB)
	{
		fromSquare = GetLS1BIndex(bishopsBB);
		//validBB = enemyKingBishopAttacksBB; // Discovered check?
		//fromSquareBB = CreateBitboardFromSquare(fromSquare);
		//if (fromSquareBB & enemyKingRookAttacksBB)
		//	if (RookAttacksBB(enemyKingSquare, occupiedBB ^ fromSquareBB) & (PiecesBB[sideToMove][Rook] | PiecesBB[sideToMove][Queen]))
		//		validBB = -1;
		//attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & notOccupiedBB & validBB;
		attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & notOccupiedBB & enemyKingBishopAttacksBB;
		if (CreateBitboardFromSquare(fromSquare) & GameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			toSquare = GetLS1BIndex(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(bishopsBB);
	}

	// Rooks
	uint64_t rooksBB = PiecesBB[sideToMove][Rook];
	while (rooksBB)
	{
		fromSquare = GetLS1BIndex(rooksBB);
		//validBB = enemyKingRookAttacksBB; // Discovered check?
		//fromSquareBB = CreateBitboardFromSquare(fromSquare);
		//if (fromSquareBB & enemyKingBishopAttacksBB)
		//	if (BishopAttacksBB(enemyKingSquare, occupiedBB ^ fromSquareBB) & (PiecesBB[sideToMove][Bishop] | PiecesBB[sideToMove][Queen]))
		//		validBB = -1;
		//attacksBB = RookAttacksBB(fromSquare, occupiedBB) & notOccupiedBB & validBB;
		attacksBB = RookAttacksBB(fromSquare, occupiedBB) & notOccupiedBB & enemyKingRookAttacksBB;
		if (CreateBitboardFromSquare(fromSquare) & GameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			toSquare = GetLS1BIndex(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(rooksBB);
	}

	// Queens (have to be done separately from the rooks and bishops above as they can check along rank/files and diagonals)
	uint64_t queensBB = PiecesBB[sideToMove][Queen];
	while (queensBB)
	{
		fromSquare = GetLS1BIndex(queensBB);
		attacksBB = ((BishopAttacksBB(fromSquare, occupiedBB) | RookAttacksBB(fromSquare, occupiedBB)) & notOccupiedBB) & (enemyKingRookAttacksBB | enemyKingBishopAttacksBB);
		if (CreateBitboardFromSquare(fromSquare) & GameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			toSquare = GetLS1BIndex(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(queensBB);
	}

	return mlp;
}

MoveWithScore_Struct* Brain::GenerateAllChecks(int sideToMove, MoveWithScore_Struct* mlp)
{
	int fromSquare, toSquare;
	uint64_t attacksBB;
	uint64_t occupiedBB = PiecesBB[0][AllPieces] | PiecesBB[1][AllPieces];
	int kingSquare = GetLS1BIndex(PiecesBB[sideToMove][King]);
	int enemyKingSquare = GetLS1BIndex(PiecesBB[sideToMove ^ 1][King]);
	uint64_t enemyKingPawnAttacksBB = PawnAttacksBBList[sideToMove ^ 1][enemyKingSquare];
	uint64_t enemyKingKnightAttacksBB = KnightAttacksBBList[enemyKingSquare];
	uint64_t enemyKingBishopAttacksBB = BishopAttacksBB(enemyKingSquare, occupiedBB);
	uint64_t enemyKingRookAttacksBB = RookAttacksBB(enemyKingSquare, occupiedBB);
	uint64_t fromSquareBB, toSquareBB;

	// Pawns
	int pmo = PawnMoveOffset[sideToMove];
	uint64_t pawnMove1BB = (((PiecesBB[sideToMove][Pawn] & ~GameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB;
	while (pawnMove1BB)
	{
		toSquare = GetLS1BIndex(pawnMove1BB);
		toSquareBB = CreateBitboardFromSquare(toSquare);
		fromSquare = toSquare - pmo;
		fromSquareBB = CreateBitboardFromSquare(fromSquare);
		if (
			(!(fromSquareBB & GameRecordPointer->pinnedRankFileBB)) || // Not pinned?
			(toSquareBB & LineListBB[fromSquare][kingSquare])
			)
			if (
				(fromSquareBB & GameRecordPointer->discoverersAllBB) && ((toSquareBB & LineListBB[fromSquare][enemyKingSquare]) == 0) // Discovered check?
				|| (toSquareBB & enemyKingPawnAttacksBB) // Direct check?
				)
			{
				if ((toSquare >> 3) == EigthRank[sideToMove])
				{
					mlp++->ui32 = fromSquare | (toSquare << 8) | (MFPromoteToQueen << 16);
					//if (GenerateUnderPromotions)
					{
						mlp++->ui32 = fromSquare | (toSquare << 8) | (MFPromoteToRook << 16);
						mlp++->ui32 = fromSquare | (toSquare << 8) | (MFPromoteToBishop << 16);
						mlp++->ui32 = fromSquare | (toSquare << 8) | (MFPromoteToKnight << 16);
					}
				}
				else
				{
					mlp++->ui32 = fromSquare | (toSquare << 8);
				}
			}
			else
			{
				if ((toSquare >> 3) == EigthRank[sideToMove]) // Promotion check?
				{
					if (toSquareBB & (enemyKingBishopAttacksBB | RookAttacksBB(enemyKingSquare, occupiedBB & ~fromSquareBB & ~toSquareBB)))
						mlp++->ui32 = fromSquare | (toSquare << 8) | (MFPromoteToQueen << 16);
					//if (GenerateUnderPromotions)
					{
						if (toSquareBB & (RookAttacksBB(enemyKingSquare, occupiedBB & ~fromSquareBB & ~toSquareBB)))
							mlp++->ui32 = fromSquare | (toSquare << 8) | (MFPromoteToRook << 16);
						if (toSquareBB & (enemyKingBishopAttacksBB))
							mlp++->ui32 = fromSquare | (toSquare << 8) | (MFPromoteToBishop << 16);
						if (toSquareBB & (enemyKingKnightAttacksBB))
							mlp++->ui32 = fromSquare | (toSquare << 8) | (MFPromoteToKnight << 16);
					}
				}
			}
		ClearLS1B(pawnMove1BB);
	}
	uint64_t pawnMove2BB = ((((((PiecesBB[sideToMove][Pawn] & SecondRankBB[sideToMove] & ~GameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB) << 8) >> (sideToMove << 4)) & notOccupiedBB;
	while (pawnMove2BB)
	{
		toSquare = GetLS1BIndex(pawnMove2BB);
		toSquareBB = CreateBitboardFromSquare(toSquare);
		fromSquare = toSquare - pmo * 2;
		fromSquareBB = CreateBitboardFromSquare(fromSquare);
		if (
			(!(fromSquareBB & GameRecordPointer->pinnedRankFileBB)) ||
			(toSquareBB & LineListBB[fromSquare][kingSquare])
			)
			if (
				(fromSquareBB & GameRecordPointer->discoverersAllBB) && ((toSquareBB & LineListBB[fromSquare][enemyKingSquare]) == 0) // Discovered check?
				|| (toSquareBB & enemyKingPawnAttacksBB) // Direct check?
				)
				mlp++->ui32 = (toSquare - pmo * 2) | (toSquare << 8);
		ClearLS1B(pawnMove2BB);
	}
	uint64_t pawnsCapturesEastBB = (East(((PiecesBB[sideToMove][Pawn] & ~GameRecordPointer->pinnedRankFileBB) << 8) >> (sideToMove << 4))) & PiecesBB[sideToMove ^ 1][AllPieces];
	while (pawnsCapturesEastBB)
	{
		toSquare = GetLS1BIndex(pawnsCapturesEastBB);
		toSquareBB = CreateBitboardFromSquare(toSquare);
		fromSquare = toSquare - (pmo + 1);
		fromSquareBB = CreateBitboardFromSquare(fromSquare);
		if (
			(!(fromSquareBB & GameRecordPointer->pinnedDiagonalBB)) || // Not pinned?
			(toSquareBB & LineListBB[fromSquare][kingSquare])
			)
			if (
				(fromSquareBB & GameRecordPointer->discoverersAllBB) && ((toSquareBB & LineListBB[fromSquare][enemyKingSquare]) == 0) // Discovered check?
				|| (toSquareBB & enemyKingPawnAttacksBB) // Direct check?
				)
			{
				if ((toSquare >> 3) == EigthRank[sideToMove])
				{
					mlp++->ui32 = fromSquare | (toSquare << 8) | (MFPromoteToQueen << 16);
					//if (GenerateUnderPromotions)
					{
						mlp++->ui32 = fromSquare | (toSquare << 8) | (MFPromoteToRook << 16);
						mlp++->ui32 = fromSquare | (toSquare << 8) | (MFPromoteToBishop << 16);
						mlp++->ui32 = fromSquare | (toSquare << 8) | (MFPromoteToKnight << 16);
					}
				}
				else
				{
					mlp++->ui32 = fromSquare | (toSquare << 8);
				}
			}
			else
			{
				if ((toSquare >> 3) == EigthRank[sideToMove]) // Promotion check?
				{
					if (toSquareBB & (BishopAttacksBB(enemyKingSquare, occupiedBB & ~fromSquareBB & ~toSquareBB) | enemyKingRookAttacksBB))
						mlp++->ui32 = (toSquare - (pmo + 1)) | (toSquare << 8) | (MFPromoteToQueen << 16);
					//if (GenerateUnderPromotions)
					{
						if (toSquareBB & (enemyKingRookAttacksBB))
							mlp++->ui32 = (toSquare - (pmo + 1)) | (toSquare << 8) | (MFPromoteToRook << 16);
						if (toSquareBB & (BishopAttacksBB(enemyKingSquare, occupiedBB & ~fromSquareBB & ~toSquareBB)))
							mlp++->ui32 = (toSquare - (pmo + 1)) | (toSquare << 8) | (MFPromoteToBishop << 16);
						if (toSquareBB & (enemyKingKnightAttacksBB))
							mlp++->ui32 = (toSquare - (pmo + 1)) | (toSquare << 8) | (MFPromoteToKnight << 16);
					}
					//else if (KnightAttacksBBList[toSquare] & PiecesBB[sideToMove ^ 1][King])
					//	MLP++->i32 = (toSquare - (pmo + 1)) | (toSquare << 8) | (MFPromoteToKnight << 16);
				}
			}
		ClearLS1B(pawnsCapturesEastBB);
	}
	uint64_t pawnsCapturesWestBB = (West(((PiecesBB[sideToMove][Pawn] & ~GameRecordPointer->pinnedRankFileBB) << 8) >> (sideToMove << 4))) & PiecesBB[sideToMove ^ 1][AllPieces];
	while (pawnsCapturesWestBB)
	{
		toSquare = GetLS1BIndex(pawnsCapturesWestBB);
		toSquareBB = CreateBitboardFromSquare(toSquare);
		fromSquare = toSquare - (pmo - 1);
		fromSquareBB = CreateBitboardFromSquare(fromSquare);
		if (
			(!(fromSquareBB & GameRecordPointer->pinnedDiagonalBB)) || // Not pinned?
			(toSquareBB & LineListBB[fromSquare][kingSquare])
			)
			if (
				(fromSquareBB & GameRecordPointer->discoverersAllBB) && ((toSquareBB & LineListBB[fromSquare][enemyKingSquare]) == 0) // Discovered check?
				|| (toSquareBB & enemyKingPawnAttacksBB) // Direct check?
				)
			{
				if ((toSquare >> 3) == EigthRank[sideToMove])
				{
					mlp++->ui32 = fromSquare | (toSquare << 8) | (MFPromoteToQueen << 16);
					//if (GenerateUnderPromotions)
					{
						mlp++->ui32 = fromSquare | (toSquare << 8) | (MFPromoteToRook << 16);
						mlp++->ui32 = fromSquare | (toSquare << 8) | (MFPromoteToBishop << 16);
						mlp++->ui32 = fromSquare | (toSquare << 8) | (MFPromoteToKnight << 16);
					}
				}
				else
				{
					mlp++->ui32 = fromSquare | (toSquare << 8);
				}
			}
			else
			{
				if ((toSquare >> 3) == EigthRank[sideToMove])
				{
					if (toSquareBB & (BishopAttacksBB(enemyKingSquare, occupiedBB & ~fromSquareBB & ~toSquareBB) | enemyKingRookAttacksBB))
						mlp++->ui32 = (toSquare - (pmo - 1)) | (toSquare << 8) | (MFPromoteToQueen << 16);
					//if (GenerateUnderPromotions)
					{
						if (toSquareBB & (enemyKingRookAttacksBB))
							mlp++->ui32 = (toSquare - (pmo - 1)) | (toSquare << 8) | (MFPromoteToRook << 16);
						if (toSquareBB & (BishopAttacksBB(enemyKingSquare, occupiedBB & ~fromSquareBB & ~toSquareBB)))
							mlp++->ui32 = (toSquare - (pmo - 1)) | (toSquare << 8) | (MFPromoteToBishop << 16);
						if (toSquareBB & (enemyKingKnightAttacksBB))
							mlp++->ui32 = (toSquare - (pmo - 1)) | (toSquare << 8) | (MFPromoteToKnight << 16);
					}
					//else if (KnightAttacksBBList[toSquare] & PiecesBB[sideToMove ^ 1][King])
					//	MLP++->i32 = (toSquare - (pmo - 1)) | (toSquare << 8) | (MFPromoteToKnight << 16);
				}
			}
		ClearLS1B(pawnsCapturesWestBB);
	}
	// En-passant
	if (GameRecordPointer->epSquare)
	{
		Move_Struct previousMove;
		previousMove.ui32 = (GameRecordPointer - 1)->move.ui32;
		if (West(CreateBitboardFromSquare(previousMove.mf.toSquare)) & PiecesBB[sideToMove][Pawn])
		{
			PiecesBB[sideToMove][Pawn] ^= (CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare));
			PiecesBB[sideToMove][AllPieces] ^= (CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare));
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			if (!IsAttacked(kingSquare, sideToMove ^ 1))
				if (IsAttacked(enemyKingSquare, sideToMove))
					mlp++->ui32 = (previousMove.mf.toSquare - 1) | ((previousMove.mf.toSquare) << 8) | (MFEnPassant << 16);
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare - 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
		}
		if (East(CreateBitboardFromSquare(previousMove.mf.toSquare)) & PiecesBB[sideToMove][Pawn])
		{
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			if (!IsAttacked(kingSquare, sideToMove ^ 1))
				if (IsAttacked(enemyKingSquare, sideToMove))
					mlp++->ui32 = (previousMove.mf.toSquare + 1) | ((previousMove.mf.toSquare) << 8) | (MFEnPassant << 16);
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare + 1) ^ CreateBitboardFromSquare(GameRecordPointer->epSquare);
			PiecesBB[sideToMove ^ 1][Pawn] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(previousMove.mf.toSquare);
		}
	}

	// Knights (can give discovered checks along ranks/files and diagonals)
	uint64_t knightsBB = PiecesBB[sideToMove][Knight] & ~GameRecordPointer->pinnedAllBB;
	while (knightsBB)
	{
		fromSquare = GetLS1BIndex(knightsBB);
		fromSquareBB = CreateBitboardFromSquare(fromSquare);
		if (fromSquareBB & GameRecordPointer->discoverersAllBB)
			attacksBB = KnightAttacksBBList[fromSquare] & ~PiecesBB[sideToMove][AllPieces]; // All moves will give discovered checks
		else
			attacksBB = KnightAttacksBBList[fromSquare] & ~PiecesBB[sideToMove][AllPieces] & enemyKingKnightAttacksBB; // Some moves may directly give check
		while (attacksBB)
		{
			toSquare = GetLS1BIndex(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(knightsBB);
	}

	// Bishops (can give discovered checks along ranks/files)
	uint64_t bishopsBB = PiecesBB[sideToMove][Bishop];
	while (bishopsBB)
	{
		fromSquare = GetLS1BIndex(bishopsBB);
		fromSquareBB = CreateBitboardFromSquare(fromSquare);
		if (fromSquareBB & GameRecordPointer->discoverersAllBB)
			attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & ~PiecesBB[sideToMove][AllPieces];
		else
			attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & ~PiecesBB[sideToMove][AllPieces] & enemyKingBishopAttacksBB;
		if (fromSquareBB & GameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			toSquare = GetLS1BIndex(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(bishopsBB);
	}

	// Rooks (can give discovered checks along diagonals)
	uint64_t rooksBB = PiecesBB[sideToMove][Rook];
	while (rooksBB)
	{
		fromSquare = GetLS1BIndex(rooksBB);
		fromSquareBB = CreateBitboardFromSquare(fromSquare);
		if (fromSquareBB & GameRecordPointer->discoverersAllBB)
			attacksBB = RookAttacksBB(fromSquare, occupiedBB) & ~PiecesBB[sideToMove][AllPieces];
		else
			attacksBB = RookAttacksBB(fromSquare, occupiedBB) & ~PiecesBB[sideToMove][AllPieces] & enemyKingRookAttacksBB;
		if (fromSquareBB & GameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			toSquare = GetLS1BIndex(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(rooksBB);
	}

	// Queens (cannot give discovered checks as they would already be checking)
	uint64_t queensBB = PiecesBB[sideToMove][Queen];
	while (queensBB)
	{
		fromSquare = GetLS1BIndex(queensBB);
		fromSquareBB = CreateBitboardFromSquare(fromSquare);
		//if (fromSquareBB & gameRecordPointer->discoverersAllBB)
		//	attacksBB = ((BishopAttacksBB(fromSquare, occupiedBB) | RookAttacksBB(fromSquare, occupiedBB)) & ~piecesBB[sideToMove][AllPieces]);
		//else
			attacksBB = ((BishopAttacksBB(fromSquare, occupiedBB) | RookAttacksBB(fromSquare, occupiedBB)) & ~PiecesBB[sideToMove][AllPieces]) & (enemyKingRookAttacksBB | enemyKingBishopAttacksBB);
		if (CreateBitboardFromSquare(fromSquare) & GameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			toSquare = GetLS1BIndex(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(queensBB);
	}

	// King (can give discovered checks along ranks/files and diagonals but not along the ray to/fro the enemy king)
	fromSquareBB = CreateBitboardFromSquare(kingSquare);//SURELY THIS IS piecesBB[sideToMove][King] ?????
	if (fromSquareBB & GameRecordPointer->discoverersAllBB)
	{
		attacksBB = KingAttacksBBList[kingSquare] & ~PiecesBB[sideToMove][AllPieces] & ~LineListBB[enemyKingSquare][kingSquare];
		while (attacksBB)
		{
			int toSquare = GetLS1BIndex(attacksBB);
			if (!IsAttacked(toSquare, sideToMove ^ 1))
				mlp++->ui32 = kingSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
	}

	// Generate castling moves
	// DOESN'T SUPPORT CHESS960 CASTLING!
	//if (UCI_Chess960)
	//{

	//}
	//else
	{
		// King side
		if (GameRecordPointer->castlingStatus.ui8[sideToMove][0] == 0)
			if ((MailboxBoard64[kingSquare + 2] | MailboxBoard64[kingSquare + 1]) == Empty)
				if (!IsAttacked(kingSquare + 1, sideToMove ^ 1) && !IsAttacked(kingSquare + 2, sideToMove ^ 1))
					if (BetweenListBB[enemyKingSquare][BackRankBaseSquareIndex[sideToMove] + F] != 0)
						if ((BetweenListBB[enemyKingSquare][BackRankBaseSquareIndex[sideToMove] + F] & ~(occupiedBB & ~PiecesBB[sideToMove][King]) & (FirstRankBB[sideToMove] | FileFBB)) == BetweenListBB[enemyKingSquare][BackRankBaseSquareIndex[sideToMove] + F]) // Are all the squares between the enemy king and the rook empty?
							mlp++->ui32 = kingSquare | ((kingSquare + 2) << 8) | (MFCastling << 16);

		// Queen side
		if (GameRecordPointer->castlingStatus.ui8[sideToMove][1] == 0)
			if ((MailboxBoard64[kingSquare - 3] | MailboxBoard64[kingSquare - 2] | MailboxBoard64[kingSquare - 1]) == Empty)
				if (!IsAttacked(kingSquare - 1, sideToMove ^ 1) && !IsAttacked(kingSquare - 2, sideToMove ^ 1))
					if (BetweenListBB[enemyKingSquare][BackRankBaseSquareIndex[sideToMove] + D] != 0)
						if ((BetweenListBB[enemyKingSquare][BackRankBaseSquareIndex[sideToMove] + D] & ~(occupiedBB & ~PiecesBB[sideToMove][King]) & (FirstRankBB[sideToMove] | FileDBB)) == BetweenListBB[enemyKingSquare][BackRankBaseSquareIndex[sideToMove] + D]) // Are all the squares between the enemy king and the rook empty?
							mlp++->ui32 = kingSquare | ((kingSquare - 2) << 8) | (MFCastling << 16);
	}

	return mlp;
}

MoveWithScore_Struct* Brain::GenerateAllNonCaptureNonPromotionChecks(int sideToMove, MoveWithScore_Struct* mlp)
{
	int fromSquare, toSquare;
	uint64_t attacksBB;
	uint64_t occupiedBB = PiecesBB[0][AllPieces] | PiecesBB[1][AllPieces];
	int kingSquare = GetLS1BIndex(PiecesBB[sideToMove][King]);
	int enemyKingSquare = GetLS1BIndex(PiecesBB[sideToMove ^ 1][King]);
	uint64_t enemyKingPawnAttacksBB = PawnAttacksBBList[sideToMove ^ 1][enemyKingSquare];
	uint64_t enemyKingKnightAttacksBB = KnightAttacksBBList[enemyKingSquare];
	uint64_t enemyKingBishopAttacksBB = BishopAttacksBB(enemyKingSquare, occupiedBB);
	uint64_t enemyKingRookAttacksBB = RookAttacksBB(enemyKingSquare, occupiedBB);
	uint64_t fromSquareBB, toSquareBB;

	// Pawns
	int pmo = PawnMoveOffset[sideToMove];
	uint64_t pawnMove1BB = (((PiecesBB[sideToMove][Pawn] & ~SeventhRankBB[sideToMove] & ~GameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB;
	while (pawnMove1BB)
	{
		toSquare = GetLS1BIndex(pawnMove1BB);
		toSquareBB = CreateBitboardFromSquare(toSquare);
		fromSquare = toSquare - pmo;
		fromSquareBB = CreateBitboardFromSquare(fromSquare);
		if (
			(!(fromSquareBB & GameRecordPointer->pinnedRankFileBB)) || // Not pinned?
			(toSquareBB & LineListBB[fromSquare][kingSquare])
			)
			if (
				(fromSquareBB & GameRecordPointer->discoverersAllBB) && ((toSquareBB & LineListBB[fromSquare][enemyKingSquare]) == 0) // Discovered check?
				|| (toSquareBB & enemyKingPawnAttacksBB) // Direct check?
				)
				mlp++->ui32 = fromSquare | (toSquare << 8);
		ClearLS1B(pawnMove1BB);
	}
	uint64_t pawnMove2BB = ((((((PiecesBB[sideToMove][Pawn] & SecondRankBB[sideToMove] & ~GameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB) << 8) >> (sideToMove << 4)) & notOccupiedBB;
	while (pawnMove2BB)
	{
		toSquare = GetLS1BIndex(pawnMove2BB);
		toSquareBB = CreateBitboardFromSquare(toSquare);
		fromSquare = toSquare - pmo * 2;
		fromSquareBB = CreateBitboardFromSquare(fromSquare);
		if (
			(!(fromSquareBB & GameRecordPointer->pinnedRankFileBB)) ||
			(toSquareBB & LineListBB[fromSquare][kingSquare])
			)
			if (
				(fromSquareBB & GameRecordPointer->discoverersAllBB) && ((toSquareBB & LineListBB[fromSquare][enemyKingSquare]) == 0) // Discovered check?
				|| (toSquareBB & enemyKingPawnAttacksBB) // Direct check?
				)
				mlp++->ui32 = fromSquare | (toSquare << 8);
		ClearLS1B(pawnMove2BB);
	}

	// Knights (can give discovered checks along ranks/files and diagonals)
	uint64_t knightsBB = PiecesBB[sideToMove][Knight] & ~GameRecordPointer->pinnedAllBB;
	while (knightsBB)
	{
		fromSquare = GetLS1BIndex(knightsBB);
		fromSquareBB = CreateBitboardFromSquare(fromSquare);
		if (fromSquareBB & GameRecordPointer->discoverersAllBB)
			attacksBB = KnightAttacksBBList[fromSquare] & notOccupiedBB; // All moves will give discovered checks
		else
			attacksBB = KnightAttacksBBList[fromSquare] & notOccupiedBB & enemyKingKnightAttacksBB; // Some moves may directly give check
		while (attacksBB)
		{
			toSquare = GetLS1BIndex(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(knightsBB);
	}

	// Bishops (can give discovered checks along ranks/files)
	uint64_t bishopsBB = PiecesBB[sideToMove][Bishop];
	while (bishopsBB)
	{
		fromSquare = GetLS1BIndex(bishopsBB);
		fromSquareBB = CreateBitboardFromSquare(fromSquare);
		if (fromSquareBB & GameRecordPointer->discoverersAllBB)
			attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & notOccupiedBB;
		else
			attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & notOccupiedBB & enemyKingBishopAttacksBB;
		if (fromSquareBB & GameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			toSquare = GetLS1BIndex(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(bishopsBB);
	}

	// Rooks (can give discovered checks along diagonals)
	uint64_t rooksBB = PiecesBB[sideToMove][Rook];
	while (rooksBB)
	{
		fromSquare = GetLS1BIndex(rooksBB);
		fromSquareBB = CreateBitboardFromSquare(fromSquare);
		if (fromSquareBB & GameRecordPointer->discoverersAllBB)
			attacksBB = RookAttacksBB(fromSquare, occupiedBB) & notOccupiedBB;
		else
			attacksBB = RookAttacksBB(fromSquare, occupiedBB) & notOccupiedBB & enemyKingRookAttacksBB;
		if (fromSquareBB & GameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			toSquare = GetLS1BIndex(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(rooksBB);
	}

	// Queens (cannot give discovered checks as they would already be checking)
	uint64_t queensBB = PiecesBB[sideToMove][Queen];
	while (queensBB)
	{
		fromSquare = GetLS1BIndex(queensBB);
		fromSquareBB = CreateBitboardFromSquare(fromSquare);
		//if (fromSquareBB & gameRecordPointer->discoverersAllBB)
		//	attacksBB = ((BishopAttacksBB(fromSquare, occupiedBB) | RookAttacksBB(fromSquare, occupiedBB)) & ~piecesBB[sideToMove][AllPieces]);
		//else
		attacksBB = ((BishopAttacksBB(fromSquare, occupiedBB) | RookAttacksBB(fromSquare, occupiedBB)) & notOccupiedBB) & (enemyKingRookAttacksBB | enemyKingBishopAttacksBB);
		if (CreateBitboardFromSquare(fromSquare) & GameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			toSquare = GetLS1BIndex(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(queensBB);
	}

	// King (can give discovered checks along ranks/files and diagonals but not along the ray to/fro the enemy king)
	fromSquareBB = CreateBitboardFromSquare(kingSquare);//SURELY THIS IS piecesBB[sideToMove][King] ?????
	if (fromSquareBB & GameRecordPointer->discoverersAllBB)
	{
		attacksBB = KingAttacksBBList[kingSquare] & notOccupiedBB & ~LineListBB[enemyKingSquare][kingSquare];
		while (attacksBB)
		{
			int toSquare = GetLS1BIndex(attacksBB);
			if (!IsAttacked(toSquare, sideToMove ^ 1))
				mlp++->ui32 = kingSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
	}

	// Generate castling moves
	// DOESN'T SUPPORT CHESS960 CASTLING!
	//if (UCI_Chess960)
	//{

	//}
	//else
	{
		// King side
		if (GameRecordPointer->castlingStatus.ui8[sideToMove][0] == 0)
			if ((MailboxBoard64[kingSquare + 2] | MailboxBoard64[kingSquare + 1]) == Empty)
				if (!IsAttacked(kingSquare + 1, sideToMove ^ 1) && !IsAttacked(kingSquare + 2, sideToMove ^ 1))
					if (BetweenListBB[enemyKingSquare][BackRankBaseSquareIndex[sideToMove] + F] != 0)
						if ((BetweenListBB[enemyKingSquare][BackRankBaseSquareIndex[sideToMove] + F] & ~(occupiedBB & ~PiecesBB[sideToMove][King]) & (FirstRankBB[sideToMove] | FileFBB)) == BetweenListBB[enemyKingSquare][BackRankBaseSquareIndex[sideToMove] + F]) // Are all the squares between the enemy king and the rook empty?
							mlp++->ui32 = kingSquare | ((kingSquare + 2) << 8) | (MFCastling << 16);

		// Queen side
		if (GameRecordPointer->castlingStatus.ui8[sideToMove][1] == 0)
			if ((MailboxBoard64[kingSquare - 3] | MailboxBoard64[kingSquare - 2] | MailboxBoard64[kingSquare - 1]) == Empty)
				if (!IsAttacked(kingSquare - 1, sideToMove ^ 1) && !IsAttacked(kingSquare - 2, sideToMove ^ 1))
					if (BetweenListBB[enemyKingSquare][BackRankBaseSquareIndex[sideToMove] + D] != 0)
						if ((BetweenListBB[enemyKingSquare][BackRankBaseSquareIndex[sideToMove] + D] & ~(occupiedBB & ~PiecesBB[sideToMove][King]) & (FirstRankBB[sideToMove] | FileDBB)) == BetweenListBB[enemyKingSquare][BackRankBaseSquareIndex[sideToMove] + D]) // Are all the squares between the enemy king and the rook empty?
							mlp++->ui32 = kingSquare | ((kingSquare - 2) << 8) | (MFCastling << 16);
	}

	return mlp;
}

uint32_t Brain::CountKingMoves(int sideToMove)
{
	int fromSquare;
	uint64_t attacksBB;
	uint32_t moves = 0;

	fromSquare = GetLS1BIndex(PiecesBB[sideToMove][King]);
	attacksBB = KingAttacksBBList[fromSquare] & ~PiecesBB[sideToMove][AllPieces];
	while (attacksBB)
	{
		int toSquare = GetLS1BIndex(attacksBB);
		if (!IsAttacked(toSquare, sideToMove ^ 1))
			moves++;
		ClearLS1B(attacksBB);
	}

	return moves;
}

//----------------------------------------------------------------------------------------------------

void Brain::CalculatePinnedPieces(int sideToMove)
{
	// Find pieces of the side to move that are pinned to their own king
	int kingSquare = GetLS1BIndex(PiecesBB[sideToMove][King]);
	uint64_t pinnersBB;

	GameRecordPointer->pinnedRankFileBB = 0;
	pinnersBB = RankFilePinnersBB(kingSquare, sideToMove);
	//gameRecordPointer->pinnersRankFileBB = pinnersBB;
	while (pinnersBB)
	{
		int pinnerSquare = GetLS1BIndex(pinnersBB);
		GameRecordPointer->pinnedRankFileBB |= PiecesBB[sideToMove][AllPieces] & BetweenListBB[pinnerSquare][kingSquare];
		ClearLS1B(pinnersBB);
	}

	GameRecordPointer->pinnedDiagonalBB = 0;
	pinnersBB = DiagonalPinnersBB(kingSquare, sideToMove);
	//gameRecordPointer->pinnersDiagonalBB = pinnersBB;
	while (pinnersBB)
	{
		int pinnerSquare = GetLS1BIndex(pinnersBB);
		GameRecordPointer->pinnedDiagonalBB |= PiecesBB[sideToMove][AllPieces] & BetweenListBB[pinnerSquare][kingSquare];
		ClearLS1B(pinnersBB);
	}

	GameRecordPointer->pinnedAllBB = GameRecordPointer->pinnedRankFileBB | GameRecordPointer->pinnedDiagonalBB;
}

void Brain::CalculateDiscovererPieces(int sideToMove)
{
	// Find pieces of the side to move that are hiding a discovered check to the enemy king
	int enemyKingSquare = GetLS1BIndex(PiecesBB[sideToMove ^ 1][King]);
	uint64_t discovereesBB;

	GameRecordPointer->discoverersRankFileBB = 0;
	discovereesBB = RankFileDiscovereesBB(enemyKingSquare, sideToMove);
	while (discovereesBB)
	{
		int discovereeSquare = GetLS1BIndex(discovereesBB);
		GameRecordPointer->discoverersRankFileBB |= PiecesBB[sideToMove][AllPieces] & BetweenListBB[discovereeSquare][enemyKingSquare];
		ClearLS1B(discovereesBB);
	}

	GameRecordPointer->discoverersDiagonalBB = 0;
	discovereesBB = DiagonalDiscovereesBB(enemyKingSquare, sideToMove);
	while (discovereesBB)
	{
		int discovereeSquare = GetLS1BIndex(discovereesBB);
		GameRecordPointer->discoverersDiagonalBB |= PiecesBB[sideToMove][AllPieces] & BetweenListBB[discovereeSquare][enemyKingSquare];
		ClearLS1B(discovereesBB);
	}

	GameRecordPointer->discoverersAllBB = GameRecordPointer->discoverersRankFileBB | GameRecordPointer->discoverersDiagonalBB;
}

//----------------------------------------------------------------------------------------------------

uint32_t Brain::GenerateAllMoves(int sideToMove, int isInCheck, MoveWithScore_Struct* initialMLP)
{
	//GenerateUnderPromotions = true;

	if (!isInCheck)
	{
		// About 90% of calls are not in check
		// About 30 moves are generated
		//GenerateCapturesAndPromotions(sideToMove);
		return (uint32_t)(GenerateCapturesAndNonCaptures(sideToMove, initialMLP) - initialMLP);
	}

	// About 10% of calls are in check
	// About 4.5 moves are generated
	//GenerateCapturesOutOfCheck(sideToMove);
	//GenerateNonCapturesOutOfCheck(sideToMove);
	return (uint32_t)(GenerateAllMovesOutOfCheck(sideToMove, initialMLP, true) - initialMLP);
}

//uint32_t GenerateMovesQuiescence(int sideToMove, int isInCheck, MoveWithScore_Struct* initialMLP, uint64_t passedBB, int depthRemaining)
uint32_t Brain::GenerateMovesQuiescence(int sideToMove, int isInCheck, MoveWithScore_Struct* initialMLP, int depthRemaining)
{
	//GenerateUnderPromotions = false;

	if (!isInCheck)
	{
		MoveWithScore_Struct* MLP;

		MLP = GenerateCapturesAndPromotions(sideToMove, initialMLP);

		if (depthRemaining == 0)
		//if (depthRemaining >= -1)
		{
			MLP = GenerateNonCaptureNonPromotionDirectChecks(sideToMove, MLP); // Also generate SOME checks at the first ply of the QS
			
			//CalculateDiscovererPieces(sideToMove); // Required for legal move generation
			//MLP = GenerateAllNonCaptureNonPromotionChecks(sideToMove, MLP); // Also generate ALL checks at the first ply of the QS
		}

		//if (passedBB)
		//{
		//	int fromSquare, toSquare;
		//	uint64_t occupiedBB = PiecesBB[0][AllPieces] | PiecesBB[1][AllPieces];
		//	int kingSquare = BitScanForwardX(PiecesBB[sideToMove][King]);
		//	int pmo = PawnMoveOffset[sideToMove];
		//	uint64_t pawnMove1BB = (((passedBB & ~gameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB;
		//	while (pawnMove1BB)
		//	{
		//		if (sideToMove == 0)
		//			int ii = 99;
		//		if (sideToMove == 1)
		//			int ii = 99;
		//		toSquare = BitScanForwardX(pawnMove1BB);
		//		if (
		//			(!(CreateBitboardFromSquare(toSquare - pmo) & gameRecordPointer->pinnedRankFileBB)) || // Not pinned?
		//			(CreateBitboardFromSquare(toSquare) & LineListBB[toSquare - pmo][kingSquare]) // Is the to-square on the line between the from-square and the king?
		//			)
		//			MLP++->i32 = (toSquare - pmo) | (toSquare << 8);
		//		ClearLS1B(pawnMove1BB);
		//	}
		//	uint64_t pawnMove2BB = ((((((passedBB & SecondRankBB[sideToMove] & ~gameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB) << 8) >> (sideToMove << 4)) & notOccupiedBB;
		//	while (pawnMove2BB)
		//	{
		//		toSquare = BitScanForwardX(pawnMove2BB);
		//		if (
		//			(!(CreateBitboardFromSquare(toSquare - (pmo * 2)) & gameRecordPointer->pinnedRankFileBB)) ||
		//			(CreateBitboardFromSquare(toSquare) & LineListBB[toSquare - (pmo * 2)][kingSquare])
		//			)
		//			MLP++->i32 = (toSquare - pmo * 2) | (toSquare << 8);
		//		ClearLS1B(pawnMove2BB);
		//	}
		//}

		return (uint32_t)(MLP - initialMLP);
	}

	return (uint32_t)(GenerateAllMovesOutOfCheck(sideToMove, initialMLP, false) - initialMLP);
}

uint32_t Brain::CountAllMoves(int sideToMove, int isInCheck)
{
	if (!isInCheck)
		return CountCapturesAndNonCaptures(sideToMove);

	return CountAllMovesOutOfCheck(sideToMove);
}

bool Brain::AnyMoves(int sideToMove, int isInCheck)
{
	if (!isInCheck)
		return AnyCapturesAndNonCaptures(sideToMove);

	return AnyMovesOutOfCheck(sideToMove);
}

int Brain::CountAllQueenMovesMM(int sideToMove)
{
	int fromSquare;
	uint64_t attacksBB;
	uint64_t occupiedBB = PiecesBB[0][AllPieces] | PiecesBB[1][AllPieces];
	int moves = 0;

	// Queens
	uint64_t queensBB = PiecesBB[sideToMove][Queen];
	while (queensBB)
	{
		fromSquare = GetLS1BIndex(queensBB);
		attacksBB = RookAttacksBB(fromSquare, occupiedBB) & ~PiecesBB[sideToMove][AllPieces];
		moves += PopulationCountX(attacksBB);
		attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & ~PiecesBB[sideToMove][AllPieces];
		moves += PopulationCountX(attacksBB);
		ClearLS1B(queensBB);
	}

	return moves;
}

int Brain::CountAllMovesMM(int sideToMove)
{
	int fromSquare;
	uint64_t attacksBB;
	uint64_t occupiedBB = PiecesBB[0][AllPieces] | PiecesBB[1][AllPieces];
	int moves = 0;

	// Rooks and queens
	// The maximum number of 'rook' moves that 9 queens and 2 rooks can contribute is 11*14=154
	uint64_t rooksAndQueensBB = PiecesBB[sideToMove][Rook] | PiecesBB[sideToMove][Queen];
	while (rooksAndQueensBB)
	{
		fromSquare = GetLS1BIndex(rooksAndQueensBB);
		attacksBB = RookAttacksBB(fromSquare, occupiedBB) & ~PiecesBB[sideToMove][AllPieces];
		moves += PopulationCountX(attacksBB);
		ClearLS1B(rooksAndQueensBB);
	}

	if (moves < 218 - 143 - 16 - 8)
		return 0;

	// Bishops and queens
	// The maximum number of 'bishop' moves that 9 queens and 2 bishops can contribute is 11*13=143
	uint64_t bishopsAndQueensBB = PiecesBB[sideToMove][Bishop] | PiecesBB[sideToMove][Queen];
	while (bishopsAndQueensBB)
	{
		fromSquare = GetLS1BIndex(bishopsAndQueensBB);
		attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & ~PiecesBB[sideToMove][AllPieces];
		moves += PopulationCountX(attacksBB);
		ClearLS1B(bishopsAndQueensBB);
	}

	if (moves < 218 - 16 - 8)
		return 0;

	// Knights
	// The maximum number of moves that 2 knights can contribute is 16
	uint64_t knightsBB = PiecesBB[sideToMove][Knight];
	while (knightsBB)
	{
		fromSquare = GetLS1BIndex(knightsBB);
		attacksBB = KnightAttacksBBList[fromSquare] & ~PiecesBB[sideToMove][AllPieces];
		moves += PopulationCountX(attacksBB);
		ClearLS1B(knightsBB);
	}

	if (moves < 218 - 8)
		return 0;

	// King
	// The maximum number of moves that the king can contribute is 8
	int kingSquare = GetLS1BIndex(PiecesBB[sideToMove][King]);
	uint64_t restrictedBB = CreateBitboardFromSquare(H8) | CreateBitboardFromSquare(G8) | CreateBitboardFromSquare(H7) | CreateBitboardFromSquare(G7);
	attacksBB = KingAttacksBBList[kingSquare] & ~PiecesBB[sideToMove][AllPieces] & ~restrictedBB;
	moves += PopulationCountX(attacksBB);

	//// Castling
	//if (kingSquare == E1)
	//{
	//	if ((PiecesBB[sideToMove][Rook] & CreateBitboardFromSquare(H1)) && ((PiecesBB[sideToMove][AllPieces] & (CreateBitboardFromSquare(F1) | CreateBitboardFromSquare(G1))) == 0))
	//		moves++;
	//	if ((PiecesBB[sideToMove][Rook] & CreateBitboardFromSquare(A1)) && ((PiecesBB[sideToMove][AllPieces] & (CreateBitboardFromSquare(D1) | CreateBitboardFromSquare(C1) | CreateBitboardFromSquare(B1))) == 0))
	//		moves++;
	//}

	return moves;
}

//----------------------------------------------------------------------------------------------------

void Brain::MakeMove(int sideToMove)
{
	MoveUndo_Struct* currentMove = &GameRecordPointer->move;

	assert((currentMove->mf.fromSquare >= A1) && (currentMove->mf.fromSquare <= H8) && (currentMove->mf.toSquare >= A1) && (currentMove->mf.toSquare <= H8)); // Can't assert fromSquare!=toSquare because of FRC castling! Also can't assert that the toSquare is empty or contains opponent's piece
	assert(currentMove->mf.flag <= 15);
	assert(currentMove->ui32 != 0);
	assert(MailboxBoard64[currentMove->mf.fromSquare] != Empty);

	uint64_t hash64 = GameRecordPointer->transpositionTableHash64;
	(GameRecordPointer + 1)->castlingStatus = GameRecordPointer->castlingStatus;
	*(uint32_t*)(&(GameRecordPointer + 1)->totalMaterial[0]) = *(uint32_t*)(&GameRecordPointer->totalMaterial[0]);
	*(uint64_t*)(&(GameRecordPointer + 1)->gamePhase[0]) = *(uint64_t*)(&GameRecordPointer->gamePhase[0]);
	*(uint32_t*)(&(GameRecordPointer + 1)->totalOpeningPST[0]) = *(uint32_t*)(&GameRecordPointer->totalOpeningPST[0]);
	*(uint32_t*)(&(GameRecordPointer + 1)->totalEndgamePST[0]) = *(uint32_t*)(&GameRecordPointer->totalEndgamePST[0]);

	GameRecordPointer++;

	// Save pieces
	currentMove->fromSquarePiece = MailboxBoard64[currentMove->mf.fromSquare];
	currentMove->toSquarePiece = MailboxBoard64[currentMove->mf.toSquare];
	if (currentMove->toSquarePiece)
		if ((currentMove->fromSquarePiece > 0) == (currentMove->toSquarePiece > 0)) // Chess960 castling where K stays on same square or takes own rook?
			currentMove->toSquarePiece = Empty;

	currentMove->fromToXor = CreateBitboardFromSquare(currentMove->mf.fromSquare) ^ CreateBitboardFromSquare(currentMove->mf.toSquare);
	PiecesBB[sideToMove][abs(currentMove->fromSquarePiece)] ^= currentMove->fromToXor;
	PiecesBB[sideToMove][AllPieces] ^= currentMove->fromToXor;

	// Update the mailbox board
	MailboxBoard64[currentMove->mf.fromSquare] = Empty; // N.B. must update the from-square BEFORE the to-square for Chess960 castling as they might be the same square!
	MailboxBoard64[currentMove->mf.toSquare] = currentMove->fromSquarePiece;
	hash64 ^= TranspositionTableRandoms[sideToMove][abs(currentMove->fromSquarePiece)][currentMove->mf.toSquare] ^ TranspositionTableRandoms[sideToMove][abs(currentMove->fromSquarePiece)][currentMove->mf.fromSquare];

	int pstXOR = (sideToMove ? 0 : 56);
	GameRecordPointer->totalOpeningPST[sideToMove] += OpeningPSTs[abs(currentMove->fromSquarePiece) - 1][currentMove->mf.toSquare ^ pstXOR] - OpeningPSTs[abs(currentMove->fromSquarePiece) - 1][currentMove->mf.fromSquare ^ pstXOR];
	GameRecordPointer->totalEndgamePST[sideToMove] += EndgamePSTs[abs(currentMove->fromSquarePiece) - 1][currentMove->mf.toSquare ^ pstXOR] - EndgamePSTs[abs(currentMove->fromSquarePiece) - 1][currentMove->mf.fromSquare ^ pstXOR];


	// Piece specific updates
	GameRecordPointer->epSquare = 0;
	switch (abs(currentMove->fromSquarePiece))
	{
	case Pawn:
		// Pawn promotion?
		if (currentMove->mf.flag >= MFPromotion)
		{
			int8_t promotionPiece;
			promotionPiece = PromotedPieces[currentMove->mf.flag >> 2];

			// Update the mailbox board, bitboards and transposition table hash
			MailboxBoard64[currentMove->mf.toSquare] = (sideToMove ? -promotionPiece : promotionPiece);
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(currentMove->mf.toSquare);
			PiecesBB[sideToMove][promotionPiece] ^= CreateBitboardFromSquare(currentMove->mf.toSquare);
			hash64 ^= TranspositionTableRandoms[sideToMove][Pawn][currentMove->mf.toSquare];
			hash64 ^= TranspositionTableRandoms[sideToMove][promotionPiece][currentMove->mf.toSquare];

			// Update the material etc
			GameRecordPointer->totalMaterial[sideToMove] += MaterialValue[promotionPiece] - MVPawn;
			GameRecordPointer->gamePhase[sideToMove] += GamePhaseIncrement[promotionPiece];// -GamePhaseIncrement[Pawn];
			GameRecordPointer->totalOpeningPST[sideToMove] += OpeningPSTs[promotionPiece - 1][currentMove->mf.toSquare ^ pstXOR] - OpeningPSTs[Pawn - 1][currentMove->mf.toSquare ^ pstXOR];
			GameRecordPointer->totalEndgamePST[sideToMove] += EndgamePSTs[promotionPiece - 1][currentMove->mf.toSquare ^ pstXOR] - EndgamePSTs[Pawn - 1][currentMove->mf.toSquare ^ pstXOR];
		}
		else if (
			(abs(currentMove->mf.toSquare - currentMove->mf.fromSquare) == 16)
			&& ((West(CreateBitboardFromSquare(currentMove->mf.toSquare)) | East(CreateBitboardFromSquare(currentMove->mf.toSquare))) & PiecesBB[sideToMove ^ 1][Pawn])
			)
			GameRecordPointer->epSquare = currentMove->mf.toSquare + PawnMoveOffset[sideToMove ^ 1];

		break;
	case Rook:
		// Update castling statuses
		if (currentMove->mf.fromSquare == BackRankBaseSquareIndex[sideToMove] + InitialKingSideRookFile)
		{
			if (GameRecordPointer->castlingStatus.ui8[sideToMove][0] == 0)
				hash64 ^= TranspositionTableRandomKingSideCastling[sideToMove];
			GameRecordPointer->castlingStatus.ui8[sideToMove][0] = 1;
		}
		else if (currentMove->mf.fromSquare == BackRankBaseSquareIndex[sideToMove] + InitialQueenSideRookFile)
		{
			if (GameRecordPointer->castlingStatus.ui8[sideToMove][1] == 0)
				hash64 ^= TranspositionTableRandomQueenSideCastling[sideToMove];
			GameRecordPointer->castlingStatus.ui8[sideToMove][1] = 1;
		}
		break;
	case King:
		// Update castling statuses
		if (GameRecordPointer->castlingStatus.ui8[sideToMove][0] == 0)
			hash64 ^= TranspositionTableRandomKingSideCastling[sideToMove];
		//gameRecordPointer->castlingStatus.ui8[sideToMove][0] = 1;
		if (GameRecordPointer->castlingStatus.ui8[sideToMove][1] == 0)
			hash64 ^= TranspositionTableRandomQueenSideCastling[sideToMove];
		//gameRecordPointer->castlingStatus.ui8[sideToMove][1] = 1;
		GameRecordPointer->castlingStatus.ui16[sideToMove] = 0x0101;
		// Was it a castling move?
		if (currentMove->mf.flag == MFCastling)
		{
			if (UCI_Chess960)
			{
				if (currentMove->mf.toSquare == BackRankBaseSquareIndex[sideToMove] + G) // King-side?
				{
					if (abs(MailboxBoard64[BackRankBaseSquareIndex[sideToMove] + InitialKingSideRookFile]) != King)
						MailboxBoard64[BackRankBaseSquareIndex[sideToMove] + InitialKingSideRookFile] = Empty;
					MailboxBoard64[BackRankBaseSquareIndex[sideToMove] + F] = (sideToMove ? -Rook : Rook);
					PiecesBB[sideToMove][Rook] ^= CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + InitialKingSideRookFile) ^ CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + F);
					PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + InitialKingSideRookFile) ^ CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + F);
					hash64 ^= TranspositionTableRandoms[sideToMove][Rook][BackRankBaseSquareIndex[sideToMove] + F] ^ TranspositionTableRandoms[sideToMove][Rook][BackRankBaseSquareIndex[sideToMove] + InitialKingSideRookFile];
					GameRecordPointer->totalOpeningPST[sideToMove] += OpeningPSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + F) ^ pstXOR] - OpeningPSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + InitialKingSideRookFile) ^ pstXOR];
					GameRecordPointer->totalEndgamePST[sideToMove] += EndgamePSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + F) ^ pstXOR] - EndgamePSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + InitialKingSideRookFile) ^ pstXOR];
				}
				else
				{
					if (abs(MailboxBoard64[BackRankBaseSquareIndex[sideToMove] + InitialQueenSideRookFile]) != King)
						MailboxBoard64[BackRankBaseSquareIndex[sideToMove] + InitialQueenSideRookFile] = Empty;
					MailboxBoard64[BackRankBaseSquareIndex[sideToMove] + D] = (sideToMove ? -Rook : Rook);
					PiecesBB[sideToMove][Rook] ^= CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + InitialQueenSideRookFile) ^ CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + D);
					PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + InitialQueenSideRookFile) ^ CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + D);
					hash64 ^= TranspositionTableRandoms[sideToMove][Rook][BackRankBaseSquareIndex[sideToMove] + D] ^ TranspositionTableRandoms[sideToMove][Rook][BackRankBaseSquareIndex[sideToMove] + InitialQueenSideRookFile];
					GameRecordPointer->totalOpeningPST[sideToMove] += OpeningPSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + D) ^ pstXOR] - OpeningPSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + InitialQueenSideRookFile) ^ pstXOR];
					GameRecordPointer->totalEndgamePST[sideToMove] += EndgamePSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + D) ^ pstXOR] - EndgamePSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + InitialQueenSideRookFile) ^ pstXOR];
				}
			}
			else
			{
				if (currentMove->mf.toSquare == BackRankBaseSquareIndex[sideToMove] + G) // King-side?
				{
					MailboxBoard64[BackRankBaseSquareIndex[sideToMove] + F] = (sideToMove ? -Rook : Rook);
					MailboxBoard64[BackRankBaseSquareIndex[sideToMove] + H] = Empty;
					PiecesBB[sideToMove][Rook] ^= CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + H) ^ CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + F);
					PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + H) ^ CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + F);
					hash64 ^= TranspositionTableRandoms[sideToMove][Rook][BackRankBaseSquareIndex[sideToMove] + F] ^ TranspositionTableRandoms[sideToMove][Rook][BackRankBaseSquareIndex[sideToMove] + H];
					GameRecordPointer->totalOpeningPST[sideToMove] += OpeningPSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + F) ^ pstXOR] - OpeningPSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + H) ^ pstXOR];
					GameRecordPointer->totalEndgamePST[sideToMove] += EndgamePSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + F) ^ pstXOR] - EndgamePSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + H) ^ pstXOR];
				}
				else
				{
					MailboxBoard64[BackRankBaseSquareIndex[sideToMove] + D] = (sideToMove ? -Rook : Rook);
					MailboxBoard64[BackRankBaseSquareIndex[sideToMove] + A] = Empty;
					PiecesBB[sideToMove][Rook] ^= CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + A) ^ CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + D);
					PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + A) ^ CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + D);
					hash64 ^= TranspositionTableRandoms[sideToMove][Rook][BackRankBaseSquareIndex[sideToMove] + D] ^ TranspositionTableRandoms[sideToMove][Rook][BackRankBaseSquareIndex[sideToMove] + A];
					GameRecordPointer->totalOpeningPST[sideToMove] += OpeningPSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + D) ^ pstXOR] - OpeningPSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + A) ^ pstXOR];
					GameRecordPointer->totalEndgamePST[sideToMove] += EndgamePSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + D) ^ pstXOR] - EndgamePSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + A) ^ pstXOR];
				}
			}
		}
		//break;
	}

	// Was it a capture?
	if (currentMove->toSquarePiece)
	{
		PiecesBB[sideToMove ^ 1][abs(currentMove->toSquarePiece)] ^= CreateBitboardFromSquare(currentMove->mf.toSquare);
		PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(currentMove->mf.toSquare);
		GameRecordPointer->totalMaterial[sideToMove ^ 1] -= MaterialValue[abs(currentMove->toSquarePiece)];
		GameRecordPointer->gamePhase[sideToMove ^ 1] -= GamePhaseIncrement[abs(currentMove->toSquarePiece)];
		GameRecordPointer->totalOpeningPST[sideToMove ^ 1] -= OpeningPSTs[abs(currentMove->toSquarePiece) - 1][currentMove->mf.toSquare ^ pstXOR ^ 56];
		GameRecordPointer->totalEndgamePST[sideToMove ^ 1] -= EndgamePSTs[abs(currentMove->toSquarePiece) - 1][currentMove->mf.toSquare ^ pstXOR ^ 56];

		// Remove the to square piece from the to square
		hash64 ^= TranspositionTableRandoms[sideToMove ^ 1][abs(currentMove->toSquarePiece)][currentMove->mf.toSquare];

		if (abs(currentMove->toSquarePiece) == Pawn)
		{
			// Was it an en-passant capture?
			if (currentMove->mf.flag == MFEnPassant)
			{ // An EP move is stored as e.g. fromSquare=d5, toSquare = e5 (not e6), so we have to move the capturing pawn forward one square
				PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(currentMove->mf.toSquare + PawnMoveOffset[sideToMove]) ^ CreateBitboardFromSquare(currentMove->mf.toSquare);
				PiecesBB[sideToMove][AllPieces] ^= CreateBitboardFromSquare(currentMove->mf.toSquare + PawnMoveOffset[sideToMove]) ^ CreateBitboardFromSquare(currentMove->mf.toSquare);
				MailboxBoard64[currentMove->mf.toSquare + PawnMoveOffset[sideToMove]] = -currentMove->toSquarePiece;
				MailboxBoard64[currentMove->mf.toSquare] = Empty;
				hash64 ^= TranspositionTableRandoms[sideToMove][Pawn][currentMove->mf.toSquare + PawnMoveOffset[sideToMove]] ^ TranspositionTableRandoms[sideToMove][Pawn][currentMove->mf.toSquare];

				GameRecordPointer->totalOpeningPST[sideToMove] += OpeningPSTs[Pawn - 1][(currentMove->mf.toSquare + PawnMoveOffset[sideToMove]) ^ pstXOR] - OpeningPSTs[Pawn - 1][(currentMove->mf.toSquare) ^ pstXOR];
				GameRecordPointer->totalEndgamePST[sideToMove] += EndgamePSTs[Pawn - 1][(currentMove->mf.toSquare + PawnMoveOffset[sideToMove]) ^ pstXOR] - EndgamePSTs[Pawn - 1][(currentMove->mf.toSquare) ^ pstXOR];
			}
		}
		else
		{
			if (abs(currentMove->toSquarePiece) == Rook)
			{
				// Update castling statuses
				if (currentMove->mf.toSquare == BackRankBaseSquareIndex[sideToMove ^ 1] + InitialKingSideRookFile)
				{
					if (GameRecordPointer->castlingStatus.ui8[sideToMove ^ 1][0] == 0)
						hash64 ^= TranspositionTableRandomKingSideCastling[sideToMove ^ 1];
					GameRecordPointer->castlingStatus.ui8[sideToMove ^ 1][0] = 1;
				}
				else if (currentMove->mf.toSquare == BackRankBaseSquareIndex[sideToMove ^ 1] + InitialQueenSideRookFile)
				{
					if (GameRecordPointer->castlingStatus.ui8[sideToMove ^ 1][1] == 0)
						hash64 ^= TranspositionTableRandomQueenSideCastling[sideToMove ^ 1];
					GameRecordPointer->castlingStatus.ui8[sideToMove ^ 1][1] = 1;
				}
			}
		}
	}

	// Update '50-move' counter
	if (
		(abs(currentMove->fromSquarePiece) == Pawn) // Pawn move?
		|| (currentMove->toSquarePiece) // Capture?
		|| (GameRecordPointer->castlingStatus.ui32 != (GameRecordPointer - 1)->castlingStatus.ui32) // Move by king (including castling) or rook that changes castling status?
		)
		GameRecordPointer->pliesSinceIrreversible = 0;
	else
		GameRecordPointer->pliesSinceIrreversible = (GameRecordPointer - 1)->pliesSinceIrreversible + 1;
	assert((GameRecordPointer->pliesSinceIrreversible >= 0) && (GameRecordPointer->pliesSinceIrreversible <= 100));

	// Update hash for side to move
	hash64 = ~hash64;

	// Update hashes
	GameRecordPointer->transpositionTableHash64 = hash64;
	GameRecordPointer->transpositionTableHash64WithEP = hash64 ^ TranspositionTableRandomsEnPassant[GameRecordPointer->epSquare]; // N.B. TranspositionTableRandomsEnPassant[0] = 0

	assert(!IsAttacked(GetLS1BIndex(PiecesBB[sideToMove][King]), sideToMove ^ 1));
}

void Brain::UnMakeMove(int sideToMove)
{
	GameRecordPointer--;
	MoveUndo_Struct* currentMove = &GameRecordPointer->move;
	assert((currentMove->mf.fromSquare >= A1) && (currentMove->mf.fromSquare <= H8) && (currentMove->mf.toSquare >= A1) && (currentMove->mf.toSquare <= H8));

	MailboxBoard64[currentMove->mf.toSquare] = currentMove->toSquarePiece; // N.B. must un-update the to-square BEFORE the from-square for Chess960 castling as they might be the same square!
	MailboxBoard64[currentMove->mf.fromSquare] = currentMove->fromSquarePiece;
	PiecesBB[sideToMove][abs(currentMove->fromSquarePiece)] ^= currentMove->fromToXor;
	PiecesBB[sideToMove][AllPieces] ^= currentMove->fromToXor;

	if (currentMove->mf.flag == MFCastling)
	{
		if (UCI_Chess960)
		{
			if (currentMove->mf.toSquare == BackRankBaseSquareIndex[sideToMove] + G)
			{
				if (abs(MailboxBoard64[BackRankBaseSquareIndex[sideToMove] + F]) != King)
					MailboxBoard64[BackRankBaseSquareIndex[sideToMove] + F] = Empty;
				MailboxBoard64[BackRankBaseSquareIndex[sideToMove] + InitialKingSideRookFile] = (sideToMove ? -Rook : Rook);
				PiecesBB[sideToMove][Rook] ^= (CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + InitialKingSideRookFile) ^ CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + F));
				PiecesBB[sideToMove][AllPieces] ^= (CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + InitialKingSideRookFile) ^ CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + F));
			}
			else
			{
				if (abs(MailboxBoard64[BackRankBaseSquareIndex[sideToMove] + D]) != King)
					MailboxBoard64[BackRankBaseSquareIndex[sideToMove] + D] = Empty;
				MailboxBoard64[BackRankBaseSquareIndex[sideToMove] + InitialQueenSideRookFile] = (sideToMove ? -Rook : Rook);
				PiecesBB[sideToMove][Rook] ^= (CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + InitialQueenSideRookFile) ^ CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + D));
				PiecesBB[sideToMove][AllPieces] ^= (CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + InitialQueenSideRookFile) ^ CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + D));
			}
		}
		else
		{
			if (currentMove->mf.toSquare == BackRankBaseSquareIndex[sideToMove] + G)
			{
				MailboxBoard64[BackRankBaseSquareIndex[sideToMove] + F] = Empty;
				MailboxBoard64[BackRankBaseSquareIndex[sideToMove] + H] = (sideToMove ? -Rook : Rook);
				PiecesBB[sideToMove][Rook] ^= (CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + H) ^ CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + F));
				PiecesBB[sideToMove][AllPieces] ^= (CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + H) ^ CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + F));
			}
			else
			{
				MailboxBoard64[BackRankBaseSquareIndex[sideToMove] + D] = Empty;
				MailboxBoard64[BackRankBaseSquareIndex[sideToMove] + A] = (sideToMove ? -Rook : Rook);
				PiecesBB[sideToMove][Rook] ^= (CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + A) ^ CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + D));
				PiecesBB[sideToMove][AllPieces] ^= (CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + A) ^ CreateBitboardFromSquare(BackRankBaseSquareIndex[sideToMove] + D));
			}
		}
	}
	else
	{
		if (currentMove->mf.flag >= MFPromotion) // Promotion
		{
			PiecesBB[sideToMove][Pawn] ^= CreateBitboardFromSquare(currentMove->mf.toSquare);
			PiecesBB[sideToMove][PromotedPieces[currentMove->mf.flag >> 2]] ^= CreateBitboardFromSquare(currentMove->mf.toSquare);
		}

		if (currentMove->toSquarePiece) // Capture?
		{
			// Restore the captured pieces bitboard
			PiecesBB[sideToMove ^ 1][abs(currentMove->toSquarePiece)] ^= CreateBitboardFromSquare(currentMove->mf.toSquare);
			PiecesBB[sideToMove ^ 1][AllPieces] ^= CreateBitboardFromSquare(currentMove->mf.toSquare);

			// Was it an en-passant capture?
			if (currentMove->mf.flag == MFEnPassant)
			{
				MailboxBoard64[currentMove->mf.toSquare + PawnMoveOffset[sideToMove]] = Empty;
				PiecesBB[sideToMove][Pawn] ^= (CreateBitboardFromSquare(currentMove->mf.toSquare + PawnMoveOffset[sideToMove]) ^ CreateBitboardFromSquare(currentMove->mf.toSquare));
				PiecesBB[sideToMove][AllPieces] ^= (CreateBitboardFromSquare(currentMove->mf.toSquare + PawnMoveOffset[sideToMove]) ^ CreateBitboardFromSquare(currentMove->mf.toSquare));
			}
		}
	}
}

int Brain::IsAttacked(int square, int sideToMove)
{
	return
		(KnightAttacksBBList[square] & PiecesBB[sideToMove][Knight]) ||
		((PiecesBB[sideToMove][Bishop] | PiecesBB[sideToMove][Queen]) && (BishopAttacksBB(square, PiecesBB[sideToMove][AllPieces] | PiecesBB[sideToMove ^ 1][AllPieces]) & (PiecesBB[sideToMove][Bishop] | PiecesBB[sideToMove][Queen]))) ||
		(PawnAttacksBBList[sideToMove ^ 1][square] & PiecesBB[sideToMove][Pawn]) ||
		((PiecesBB[sideToMove][Rook] | PiecesBB[sideToMove][Queen]) && (RookAttacksBB(square, PiecesBB[sideToMove][AllPieces] | PiecesBB[sideToMove ^ 1][AllPieces]) & (PiecesBB[sideToMove][Rook] | PiecesBB[sideToMove][Queen]))) ||
		(KingAttacksBBList[square] & PiecesBB[sideToMove][King])
		;
}

int Brain::IsEnemyKingAttacked(int square, int sideToMove)
{
	// This routine is identical to IsAttacked except that it skips attacks by the king as it would be an illegal position!
	// N.B. The inclusion of the extra clause in the slider tests is intentional to allow short-circuiting
	// The king is attacked about 3% of the time
	return
		(KnightAttacksBBList[square] & PiecesBB[sideToMove][Knight]) ||
		((PiecesBB[sideToMove][Bishop] | PiecesBB[sideToMove][Queen]) && (BishopAttacksBB(square, PiecesBB[sideToMove][AllPieces] | PiecesBB[sideToMove ^ 1][AllPieces]) & (PiecesBB[sideToMove][Bishop] | PiecesBB[sideToMove][Queen]))) ||
		(PawnAttacksBBList[sideToMove ^ 1][square] & PiecesBB[sideToMove][Pawn]) ||
		((PiecesBB[sideToMove][Rook] | PiecesBB[sideToMove][Queen]) && (RookAttacksBB(square, PiecesBB[sideToMove][AllPieces] | PiecesBB[sideToMove ^ 1][AllPieces]) & (PiecesBB[sideToMove][Rook] | PiecesBB[sideToMove][Queen])))
		;
}

uint64_t Brain::RankFilePinnersBB(int kingSquare, int sideToMove)
{
	uint64_t occupiedBB = PiecesBB[0][AllPieces] | PiecesBB[1][AllPieces];
	uint64_t attacksBB = RookAttacksBB(kingSquare, occupiedBB);
	uint64_t potentiallyPinnedBB = attacksBB & PiecesBB[sideToMove][AllPieces]; // Find friendly pieces that may be pinned
	return (attacksBB ^ RookAttacksBB(kingSquare, occupiedBB ^ potentiallyPinnedBB)) & (PiecesBB[sideToMove ^ 1][Rook] | PiecesBB[sideToMove ^ 1][Queen]);
}

uint64_t Brain::DiagonalPinnersBB(int kingSquare, int sideToMove)
{
	uint64_t occupiedBB = PiecesBB[0][AllPieces] | PiecesBB[1][AllPieces];
	uint64_t attacksBB = BishopAttacksBB(kingSquare, occupiedBB);
	uint64_t potentiallyPinnedBB = attacksBB & PiecesBB[sideToMove][AllPieces]; // Find friendly pieces that may be pinned
	return (attacksBB ^ BishopAttacksBB(kingSquare, occupiedBB ^ potentiallyPinnedBB)) & (PiecesBB[sideToMove ^ 1][Bishop] | PiecesBB[sideToMove ^ 1][Queen]);
}

uint64_t Brain::RankFileDiscovereesBB(int enemyKingSquare, int sideToMove)
{
	uint64_t occupiedBB = PiecesBB[0][AllPieces] | PiecesBB[1][AllPieces];
	uint64_t attacksBB = RookAttacksBB(enemyKingSquare, occupiedBB);
	uint64_t potentiallyDiscovererBB = attacksBB & PiecesBB[sideToMove][AllPieces]; // Find friendly pieces that may be discoverers
	return (attacksBB ^ RookAttacksBB(enemyKingSquare, occupiedBB ^ potentiallyDiscovererBB)) & (PiecesBB[sideToMove][Rook] | PiecesBB[sideToMove][Queen]);
}

uint64_t Brain::DiagonalDiscovereesBB(int enemyKingSquare, int sideToMove)
{
	uint64_t occupiedBB = PiecesBB[0][AllPieces] | PiecesBB[1][AllPieces];
	uint64_t attacksBB = BishopAttacksBB(enemyKingSquare, occupiedBB);
	uint64_t potentiallyDiscovererBB = attacksBB & PiecesBB[sideToMove][AllPieces]; // Find friendly pieces that may be discoverers
	return (attacksBB ^ BishopAttacksBB(enemyKingSquare, occupiedBB ^ potentiallyDiscovererBB)) & (PiecesBB[sideToMove][Bishop] | PiecesBB[sideToMove][Queen]);
}

//----------------------------------------------------------------------------------------------------

// Changed the MVVLVA table to make the king the 'lowest' valued attacker as its legal captures must all be SEE 'winning'. (+6.4 +/-8.6 LOS 92.7%, 3438)
// The largest entry in here (i.e. (30 << 26) + 1 = 2,013,265,921) is less than INT_MAX (2^31-1 = 2,147,483,647) which is used for the transposition table best move
// The smallest entry in here (i.e. (25 << 26) - 3 = 1,677,721,597) must be greater than the values used for killers, history etc
// Indexed by [toSquarePiece][fromSquarePiece]
alignas(64) const int MVVLVA[8][8] =
{
	//{0,  5 << 26,  4 << 26,  4 << 26,  3 << 26,  2 << 26,  (5 << 26) + 1, 0}, // Capture of empty square: not used
	//{0, 10 << 26,  9 << 26,  9 << 26,  8 << 26,  7 << 26, (10 << 26) + 1, 0},
	//{0, 15 << 26, 14 << 26, 14 << 26, 13 << 26, 12 << 26, (15 << 26) + 1, 0},
	//{0, 15 << 26, 14 << 26, 14 << 26, 13 << 26, 12 << 26, (15 << 26) + 1, 0},
	//{0, 20 << 26, 19 << 26, 19 << 26, 18 << 26, 17 << 26, (20 << 26) + 1, 0},
	//{0, 25 << 26, 24 << 26, 24 << 26, 23 << 26, 22 << 26, (25 << 26) + 1, 0},
	//{0, 30 << 26, 29 << 26, 29 << 26, 28 << 26, 27 << 26, (30 << 26) + 1, 0}, // Capture of king: used for capture promotions
	//{0, 0, 0, 0, 0, 0, 0, 0}

	//{0, 25 << 26, (25 << 26) - 1, (25 << 26) - 1, (25 << 26) - 2, (25 << 26) - 3, (25 << 26) + 1, 0}, // Capture of empty square: not used
	//{0, 26 << 26, (26 << 26) - 1, (26 << 26) - 1, (26 << 26) - 2, (26 << 26) - 3, (26 << 26) + 1, 0},
	//{0, 27 << 26, (27 << 26) - 1, (27 << 26) - 1, (27 << 26) - 2, (27 << 26) - 3, (27 << 26) + 1, 0},
	//{0, 27 << 26, (27 << 26) - 1, (27 << 26) - 1, (27 << 26) - 2, (27 << 26) - 3, (27 << 26) + 1, 0},
	//{0, 28 << 26, (28 << 26) - 1, (28 << 26) - 1, (28 << 26) - 2, (28 << 26) - 3, (28 << 26) + 1, 0},
	//{0, 29 << 26, (29 << 26) - 1, (29 << 26) - 1, (29 << 26) - 2, (29 << 26) - 3, (29 << 26) + 1, 0},
	//{0, 30 << 26, (30 << 26) - 1, (30 << 26) - 1, (30 << 26) - 2, (30 << 26) - 3, (30 << 26) + 1, 0}, // Capture of king: used for capture promotions
	//{0, 0, 0, 0, 0, 0, 0, 0}

	//{0, 25 << 26, (25 << 26) - 1, (25 << 26) - 2, (25 << 26) - 3, (25 << 26) - 4, (25 << 26) + 1, 0}, // Capture of empty square: not used
	//{0, 26 << 26, (26 << 26) - 1, (26 << 26) - 2, (26 << 26) - 3, (26 << 26) - 4, (26 << 26) + 1, 0},
	//{0, (27 << 26) - 10, (27 << 26) - 11, (27 << 26) - 12, (27 << 26) - 13, (27 << 26) - 14, (27 << 26) -10 + 1, 0},
	//{0, 27 << 26, (27 << 26) - 1, (27 << 26) - 2, (27 << 26) - 3, (27 << 26) - 4, (27 << 26) + 1, 0},
	//{0, 28 << 26, (28 << 26) - 1, (28 << 26) - 2, (28 << 26) - 3, (28 << 26) - 4, (28 << 26) + 1, 0},
	//{0, 29 << 26, (29 << 26) - 1, (29 << 26) - 2, (29 << 26) - 3, (29 << 26) - 4, (29 << 26) + 1, 0},
	//{0, 30 << 26, (30 << 26) - 1, (30 << 26) - 2, (30 << 26) - 3, (30 << 26) - 4, (30 << 26) + 1, 0}, // Capture of king: used for capture promotions
	//{0, 0, 0, 0, 0, 0, 0, 0}

	// N.B. INT_MAX is odd, so subtracting an odd amount leaves an even number and subtracting an even amount leaves an odd number
	// Odd numbers indicate LxH or ExE, even numbers indicate HxL THIS WAS AN IDEA THAT DIDN'T COME TO FRUITION :D
	{0, INT_MAX - 61, INT_MAX - 63, INT_MAX - 63, INT_MAX - 65, INT_MAX - 67, INT_MAX - 60, 0}, // Empty : Capture of empty square: not currently used
	{0, INT_MAX - 52, INT_MAX - 53, INT_MAX - 53, INT_MAX - 55, INT_MAX - 57, INT_MAX - 50, 0}, // P
	{0, INT_MAX - 32, INT_MAX - 34, INT_MAX - 34, INT_MAX - 35, INT_MAX - 37, INT_MAX - 30, 0}, // N
	{0, INT_MAX - 32, INT_MAX - 34, INT_MAX - 34, INT_MAX - 35, INT_MAX - 37, INT_MAX - 30, 0}, // B
	{0, INT_MAX - 22, INT_MAX - 24, INT_MAX - 24, INT_MAX - 26, INT_MAX - 27, INT_MAX - 20, 0}, // R
	{0, INT_MAX - 12, INT_MAX - 14, INT_MAX - 14, INT_MAX - 16, INT_MAX - 18, INT_MAX - 10, 0}, // Q
	{0, 0, 0, 0, 0, 0, 0, 0}, // K
	{0, 0, 0, 0, 0, 0, 0, 0}
};

void Brain::ScoreMoves(MoveWithScore_Struct* mlp, int movesCount, int tteBestMove, int ply, TwoGoodMoves_Struct* killerMoves, TwoGoodMoves_Struct* cms, TwoGoodMoves_Struct* fums)
{
	// N.B. All scores for special moves should be > 1<<30. History uses the range +/-1<<30
	assert(movesCount > 0);
	int score;

	for (int i = 0; i < movesCount; i++)
	{
		if (mlp->ui32 == tteBestMove)
			score = INT_MAX; // TT move first (2^31-1 or 1<<31-1)
		else
		{
			uint16_t flag = mlp->mf.flag;
			int fromSquare = mlp->mf.fromSquare;
			int toSquare = mlp->mf.toSquare;
			int fromSquarePiece = std::abs(MailboxBoard64[fromSquare]);
			int toSquarePiece = std::abs(MailboxBoard64[toSquare]);

			if (flag >= MFPromotion)//NOT WORTH TESTING FOR AS SO RARE???
			{
				if (flag == MFPromoteToQueen)
					toSquarePiece = Queen; // All promotions to queen (capture or non-capture) are treated as a capture of a queen
				else
					toSquarePiece = std::max((int)Knight, toSquarePiece); // Underpromotions are treated as at least the capture of a knight
			}

			if (toSquarePiece) // Capture? (or promotion!)
			{
				// MVV/LVA				
				score = MVVLVA[toSquarePiece][fromSquarePiece];
				assert(score > 0);
			}
			else
			{
				if ((mlp->ui32 == killerMoves[ply].m1.ui32) && (killerMoves[ply].m1.piece == fromSquarePiece)) // Killer moves
					score = INT_MAX - 101;
				else if ((mlp->ui32 == killerMoves[ply].m2.ui32) && (killerMoves[ply].m2.piece == fromSquarePiece))
					score = INT_MAX - 102;

				else if (mlp->ui32 == cms->m1.ui32) // Counter moves
					score = INT_MAX - 103;
				else if (mlp->ui32 == cms->m2.ui32)
					score = INT_MAX - 104;

				else if (mlp->ui32 == fums->m1.ui32) // Follow-up moves
					score = INT_MAX - 105;
				else if (mlp->ui32 == fums->m2.ui32)
					score = INT_MAX - 106;

				else if ((ply > 2) && (mlp->ui32 == killerMoves[ply - 2].m1.ui32) && (killerMoves[ply - 2].m1.piece == fromSquarePiece)) // Killer moves from 2-ply earlier
					score = INT_MAX - 109;
				else if ((ply > 2) && (mlp->ui32 == killerMoves[ply - 2].m2.ui32) && (killerMoves[ply - 2].m2.piece == fromSquarePiece))
					score = INT_MAX - 110;

				else
					score = GameRecordPointer->historyPointer->History[fromSquarePiece - 1][toSquare];
			}
		}

		mlp++->score = score;
	}
}

void Brain::ScoreMovesMVVLVA(MoveWithScore_Struct* mlp, int movesCount)
{
	// In the QS when not in check the simple MVVLVA works best.
	// I tried small enhancements for the TT move and for promotions but they gave no significant ELO change
	assert(movesCount > 0);
	int score;

	for (int i = 0; i < movesCount; i++)
	{
		int fromSquare = mlp->mf.fromSquare;
		int toSquare = mlp->mf.toSquare;
		int fromSquarePiece = std::abs(MailboxBoard64[fromSquare]);
		int toSquarePiece = std::abs(MailboxBoard64[toSquare]);

		// MVV/LVA				
		score = MVVLVA[toSquarePiece][fromSquarePiece];
		assert(score > 0);
		
		mlp++->score = score;
	}
}

void Brain::ScoreMovesMateMode(MoveWithScore_Struct* mlp, int movesCount, int tteBestMove, int ply, TwoGoodMoves_Struct* killerMoves, TwoGoodMoves_Struct* cms, TwoGoodMoves_Struct* fums, int enemyKingSquare, Move_Struct MatingMove)
{
	assert(movesCount > 0);
	int score;

	for (int i = 0; i < movesCount; i++)
	{
		if (mlp->ui32 == tteBestMove)
			score = INT_MAX; // TT move first (2^31-1)
		else if (mlp->ui32 == MatingMove.ui32)
			score = INT_MAX - 1;
		else
		{
			uint16_t flag = mlp->mf.flag;
			int fromSquare = mlp->mf.fromSquare;
			int toSquare = mlp->mf.toSquare;
			int fromSquarePiece = std::abs(MailboxBoard64[fromSquare]);
			int toSquarePiece = std::abs(MailboxBoard64[toSquare]);

			if (toSquarePiece) // Capture?
			{
				if (flag == MFPromoteToQueen) // Treat capturing promotions to queen (at least +Q+N-P=11) as capture of king (highest entry in MVVLVA table)
					toSquarePiece = King;
				//else if (((ply & 1) == 0) & (TMI1move.ui32 != 0) && (toSquare == TMI1move.mf.fromSquare))
				//	toSquarePiece = King;

				// MVV/LVA				
				score = MVVLVA[toSquarePiece][fromSquarePiece];
			}
			else
			{
				if ((mlp->ui32 == killerMoves[ply].m1.ui32) && (killerMoves[ply].m1.piece == fromSquarePiece)) // Killer moves
					score = INT_MAX - 101;
				else if ((mlp->ui32 == killerMoves[ply].m2.ui32) && (killerMoves[ply].m2.piece == fromSquarePiece))
					score = INT_MAX - 102;

				else if (mlp->ui32 == cms->m1.ui32) // Counter moves
					score = INT_MAX - 103;
				else if (mlp->ui32 == cms->m2.ui32)
					score = INT_MAX - 104;

				else if (mlp->ui32 == fums->m1.ui32) // Follow-up moves
					score = INT_MAX - 105;
				else if (mlp->ui32 == fums->m2.ui32)
					score = INT_MAX - 106;

				else if ((ply > 2) && (mlp->ui32 == killerMoves[ply - 2].m1.ui32) && (killerMoves[ply - 2].m1.piece == fromSquarePiece)) // Killer-2 moves
					score = INT_MAX - 109;
				else if ((ply > 2) && (mlp->ui32 == killerMoves[ply - 2].m2.ui32) && (killerMoves[ply - 2].m2.piece == fromSquarePiece))
					score = INT_MAX - 110;

				//else if ((mlp->ui32 == KillerMoves[ply + 2].m1.ui32) && (KillerMoves[ply + 2].m1.piece == fromSquarePiece)) // Killer+2 moves
				//	score = (1 << 23) - 9;

				else
				{
					if (flag == MFPromoteToQueen) // Treat non-capturing promotions to queen (+Q-P=8) as slightly better than capture of rook
						score = MVVLVA[Rook][Pawn] + 1;
					else
					{
						//score = CounterMoveHistory
						//	//score = CounterMoveHistory[sideToMove]
						//	[abs((gameRecordPointer - 1)->move.fromSquarePiece) - 1]
						//[(gameRecordPointer - 1)->move.mf.toSquare]
						////[abs(fromSquarePiece) - 1]
						//.History[abs(fromSquarePiece) - 1]
						//	[toSquare];
						score = GameRecordPointer->historyPointer->History[fromSquarePiece - 1][toSquare];
						//if (score == 0)
						//	score += Random64() & 15;//TEMP

						//assert((score >= 0) && (score <= 255));
						//score += Centre[toSquare] - Centre[fromSquare] + 3;//0 - 6
						//score = HashHistory[gameRecordPointer->transpositionTableHash64WithEP & 1023].History[abs(fromSquarePiece) - 1][toSquare];

						//if (ManhattanDistance[fromSquare][enemyKingSquare] > ManhattanDistance[toSquare][enemyKingSquare])
						//	score++;

					}
				}
			}
		}

		assert(score >= 0);
		mlp++->score = score;
	}
}

// Saves the current subtree principal variation in the triangular array.
void Brain::SavePrincipalVariation(uint32_t move)
{
	uint32_t* p1 = GameRecordPointer->principalVariationPointer;
	uint32_t* p2 = p1 + MaximumPly;

	// Save move at this ply
	*p1 = move;

#ifdef _DEBUG
	int count = 0;
#endif

	// Save variation backed down
	do
	{
		p1++;
		p2++;
		*p1 = *p2;
#ifdef _DEBUG
		count++;
		assert(count < MaximumPly);
#endif
	} while ((uint16_t)*p1); // All the PVT* terminators have the bottom 16 bits set to 0
}

const int SeeLowHighValues[7] = { 0, 100, 300, 300, 500, 1000, 0 }; // N.B. king is lowest so that all its captures are immediately counted as winning (as they wouldn't be legal otherwise)
const int SeeValues[7] = { 0, 100, 300, 300, 500, 1000, 3000 };

int Brain::SEE(int fromSquare, int toSquare, int sideToMove)
{
	// Returns 1 (a winning capture), 0 (an equal capture/move) or -1 (a losing capture/move)
	// A non-capture promotion to an undefended square returns 1
	// Castling moves return 0
	// The fromSquare must contain a piece of the sideToMove.

	int sideNotToMove = sideToMove ^ 1;
	uint64_t attackersBB, occupiedBB;
	int latestToSquarePiece = abs(MailboxBoard64[fromSquare]);
	int sideToMoveTotalGain = SeeValues[abs(MailboxBoard64[toSquare])];
	int sideNotToMoveTotalGain = 0;
	int piece;

	occupiedBB = PiecesBB[0][AllPieces] | PiecesBB[1][AllPieces];
	occupiedBB ^= CreateBitboardFromSquare(fromSquare); // Remove the initial capturing piece

	if (latestToSquarePiece == Pawn)
	{
		if (abs(toSquare - fromSquare) == 1) // Handle en-passant
		{
			occupiedBB ^= CreateBitboardFromSquare(toSquare); // Remove the ep captured pawn
			toSquare += PawnMoveOffset[sideToMove]; // Adjust the to-square
		}
		else if ((toSquare >> 3) == EigthRank[sideToMove]) // Handle promotion
		{
			sideToMoveTotalGain += SeeValues[Queen] - SeeValues[Pawn];
			latestToSquarePiece = Queen;
		}
	}

	// Get all the attackers for both sides
	attackersBB =
		(RookAttacksBB(toSquare, occupiedBB) & (PiecesBB[0][Rook] | PiecesBB[1][Rook] | PiecesBB[0][Queen] | PiecesBB[1][Queen])) |
		(BishopAttacksBB(toSquare, occupiedBB) & (PiecesBB[0][Bishop] | PiecesBB[1][Bishop] | PiecesBB[0][Queen] | PiecesBB[1][Queen])) |
		(KnightAttacksBBList[toSquare] & (PiecesBB[0][Knight] | PiecesBB[1][Knight])) |
		(KingAttacksBBList[toSquare] & (PiecesBB[0][King] | PiecesBB[1][King])) |
		(PawnAttacksBBList[0][toSquare] & PiecesBB[1][Pawn]) |
		(PawnAttacksBBList[1][toSquare] & PiecesBB[0][Pawn]);
	attackersBB &= occupiedBB; // Remove all the empty squares along the rays

	int floor = -1, ceiling = 1;

sideNotToMove:
	if (sideNotToMoveTotalGain >= sideToMoveTotalGain)
	{
		if (sideNotToMoveTotalGain > sideToMoveTotalGain)
			goto exit;
		ceiling = 0;
	}

	if (attackersBB & PiecesBB[sideNotToMove][AllPieces]) // Any more defenders?
	{
		// Find the lowest valued piece type
		sideNotToMoveTotalGain += SeeValues[latestToSquarePiece];
		if (latestToSquarePiece == King)
			goto exit;
		piece = Pawn;
		while (!(attackersBB & PiecesBB[sideNotToMove][piece]))
			piece++;
		latestToSquarePiece = piece;

		if (latestToSquarePiece == Pawn)
			if ((toSquare >> 3) == EigthRank[sideNotToMove]) // Handle promotion
			{
				sideNotToMoveTotalGain += SeeValues[Queen] - SeeValues[Pawn];
				latestToSquarePiece = Queen;
			}

		uint64_t bb = attackersBB & PiecesBB[sideNotToMove][piece];
		occupiedBB ^= (bb & -bb); // Remove the latest attacker(x & -x gives you the LS1B)
		// Add any new x-ray attacks by either side behind the just removed piece
		attackersBB |=
			(RookAttacksBB(toSquare, occupiedBB) & (PiecesBB[0][Rook] | PiecesBB[1][Rook] | PiecesBB[0][Queen] | PiecesBB[1][Queen])) |
			(BishopAttacksBB(toSquare, occupiedBB) & (PiecesBB[0][Bishop] | PiecesBB[1][Bishop] | PiecesBB[0][Queen] | PiecesBB[1][Queen]));
		attackersBB &= occupiedBB; // Remove all the empty squares and the discarded attacker(s)
	}
	else
		goto exit;

	//sideToMove:
	if (sideToMoveTotalGain >= sideNotToMoveTotalGain)
	{
		if (sideToMoveTotalGain > sideNotToMoveTotalGain)
			goto exit;
		floor = 0;
	}

	if (attackersBB & PiecesBB[sideToMove][AllPieces]) // Any more attackers?
	{
		// Find the lowest valued piece type
		sideToMoveTotalGain += SeeValues[latestToSquarePiece];
		if (latestToSquarePiece == King)
			goto exit;
		piece = Pawn;
		while (!(attackersBB & PiecesBB[sideToMove][piece]))
			piece++;
		latestToSquarePiece = piece;

		if (latestToSquarePiece == Pawn)
			if ((toSquare >> 3) == EigthRank[sideToMove]) // Handle promotion
			{
				sideToMoveTotalGain += SeeValues[Queen] - SeeValues[Pawn];
				latestToSquarePiece = Queen;
			}

		uint64_t bb = attackersBB & PiecesBB[sideToMove][piece];
		occupiedBB ^= (bb & -bb); // Remove the latest attacker(x & -x gives you the LS1B)
		// Add any new x-ray attacks by either side behind the just removed piece
		attackersBB |=
			(RookAttacksBB(toSquare, occupiedBB) & (PiecesBB[0][Rook] | PiecesBB[1][Rook] | PiecesBB[0][Queen] | PiecesBB[1][Queen])) |
			(BishopAttacksBB(toSquare, occupiedBB) & (PiecesBB[0][Bishop] | PiecesBB[1][Bishop] | PiecesBB[0][Queen] | PiecesBB[1][Queen]));
		attackersBB &= occupiedBB; // Remove all the empty squares and the discarded attacker(s)

		goto sideNotToMove;
	}

exit:
	if (sideToMoveTotalGain < sideNotToMoveTotalGain)
		return std::max(-1, floor);
	if (sideToMoveTotalGain == sideNotToMoveTotalGain)
		return 0;
	return std::min(1, ceiling);
}

int Brain::SEE2(int fromSquare, int toSquare, int sideToMove, int threshold)
{
	// Returns 1 (a winning capture), 0 (an equal capture/move) or -1 (a losing capture/move)
	// The fromSquare must contain a piece of the sideToMove.
	// Castling moves return 0

	int sideNotToMove = sideToMove ^ 1;
	uint64_t attackersBB, occupiedBB;
	int latestToSquarePiece = abs(MailboxBoard64[fromSquare]);
	int sideToMoveTotalGain = threshold;
	int sideNotToMoveTotalGain = 0;
	int piece;

	occupiedBB = PiecesBB[0][AllPieces] | PiecesBB[1][AllPieces];
	occupiedBB ^= CreateBitboardFromSquare(fromSquare); // Remove the initial capturing piece

	if (latestToSquarePiece == Pawn)
	{
		if (abs(toSquare - fromSquare) == 1) // Handle en-passant
		{
			occupiedBB ^= CreateBitboardFromSquare(toSquare); // Remove the ep captured pawn
			toSquare += PawnMoveOffset[sideToMove]; // Adjust the to-square
		}
		else if ((toSquare >> 3) == EigthRank[sideToMove]) // Handle promotion
		{
			sideToMoveTotalGain += SeeValues[Queen] - SeeValues[Pawn];
			latestToSquarePiece = Queen;
		}
	}

	// Get all the attackers for both sides
	attackersBB =
		(RookAttacksBB(toSquare, occupiedBB) & (PiecesBB[0][Rook] | PiecesBB[1][Rook] | PiecesBB[0][Queen] | PiecesBB[1][Queen])) |
		(BishopAttacksBB(toSquare, occupiedBB) & (PiecesBB[0][Bishop] | PiecesBB[1][Bishop] | PiecesBB[0][Queen] | PiecesBB[1][Queen])) |
		(KnightAttacksBBList[toSquare] & (PiecesBB[0][Knight] | PiecesBB[1][Knight])) |
		(KingAttacksBBList[toSquare] & (PiecesBB[0][King] | PiecesBB[1][King])) |
		(PawnAttacksBBList[0][toSquare] & PiecesBB[1][Pawn]) |
		(PawnAttacksBBList[1][toSquare] & PiecesBB[0][Pawn]);
	attackersBB &= occupiedBB; // Remove all the empty squares along the rays

	int floor = -1, ceiling = 1;

sideNotToMove:
	if (sideNotToMoveTotalGain >= sideToMoveTotalGain)
	{
		if (sideNotToMoveTotalGain > sideToMoveTotalGain)
			goto exit;
		ceiling = 0;
	}

	if (attackersBB & PiecesBB[sideNotToMove][AllPieces]) // Any more defenders?
	{
		// Find the lowest valued piece type
		sideNotToMoveTotalGain += SeeValues[latestToSquarePiece];
		if (latestToSquarePiece == King)
			goto exit;
		piece = Pawn;
		while (!(attackersBB & PiecesBB[sideNotToMove][piece]))
			piece++;
		latestToSquarePiece = piece;
		uint64_t bb = attackersBB & PiecesBB[sideNotToMove][piece];
		occupiedBB ^= (bb & -bb); // Remove the latest attacker(x & -x gives you the LS1B)
		// Add any new x-ray attacks by either side behind the just removed piece
		attackersBB |=
			(RookAttacksBB(toSquare, occupiedBB) & (PiecesBB[0][Rook] | PiecesBB[1][Rook] | PiecesBB[0][Queen] | PiecesBB[1][Queen])) |
			(BishopAttacksBB(toSquare, occupiedBB) & (PiecesBB[0][Bishop] | PiecesBB[1][Bishop] | PiecesBB[0][Queen] | PiecesBB[1][Queen]));
		attackersBB &= occupiedBB; // Remove all the empty squares and the discarded attacker(s)
	}
	else
		goto exit;

	//sideToMove:
	if (sideToMoveTotalGain >= sideNotToMoveTotalGain)
	{
		if (sideToMoveTotalGain > sideNotToMoveTotalGain)
			goto exit;
		floor = 0;
	}

	if (attackersBB & PiecesBB[sideToMove][AllPieces]) // Any more attackers?
	{
		// Find the lowest valued piece type
		sideToMoveTotalGain += SeeValues[latestToSquarePiece];
		if (latestToSquarePiece == King)
			goto exit;
		piece = Pawn;
		while (!(attackersBB & PiecesBB[sideToMove][piece]))
			piece++;
		latestToSquarePiece = piece;
		uint64_t bb = attackersBB & PiecesBB[sideToMove][piece];
		occupiedBB ^= (bb & -bb); // Remove the latest attacker(x & -x gives you the LS1B)
		// Add any new x-ray attacks by either side behind the just removed piece
		attackersBB |=
			(RookAttacksBB(toSquare, occupiedBB) & (PiecesBB[0][Rook] | PiecesBB[1][Rook] | PiecesBB[0][Queen] | PiecesBB[1][Queen])) |
			(BishopAttacksBB(toSquare, occupiedBB) & (PiecesBB[0][Bishop] | PiecesBB[1][Bishop] | PiecesBB[0][Queen] | PiecesBB[1][Queen]));
		attackersBB &= occupiedBB; // Remove all the empty squares and the discarded attacker(s)

		goto sideNotToMove;
	}

exit:
	if (sideToMoveTotalGain < sideNotToMoveTotalGain)
		return std::max(-1, floor);
	if (sideToMoveTotalGain == sideNotToMoveTotalGain)
		return 0;
	return std::min(1, ceiling);
}

bool Brain::SEETargetPieceUnsafe(int toSquare, int sideToMove, int offset)
{
	// The toSquare must contain a piece of the sideNotToMove. The sideToMove is asking "can I WIN material on toSquare?"

	int sideNotToMove = sideToMove ^ 1;
	uint64_t attackersBB, occupiedBB;

	occupiedBB = PiecesBB[0][AllPieces] | PiecesBB[1][AllPieces];

	// Get all the attackers for both sides
	attackersBB =
		(RookAttacksBB(toSquare, occupiedBB) & (PiecesBB[0][Rook] | PiecesBB[1][Rook] | PiecesBB[0][Queen] | PiecesBB[1][Queen])) |
		(BishopAttacksBB(toSquare, occupiedBB) & (PiecesBB[0][Bishop] | PiecesBB[1][Bishop] | PiecesBB[0][Queen] | PiecesBB[1][Queen])) |
		(KnightAttacksBBList[toSquare] & (PiecesBB[0][Knight] | PiecesBB[1][Knight])) |
		(KingAttacksBBList[toSquare] & (PiecesBB[0][King] | PiecesBB[1][King])) |
		(PawnAttacksBBList[0][toSquare] & PiecesBB[1][Pawn]) |
		(PawnAttacksBBList[1][toSquare] & PiecesBB[0][Pawn]);
	attackersBB &= occupiedBB; // Remove all the empty squares along the rays

	int sideToMoveTotalGain = 0;
	int sideNotToMoveTotalGain = offset;
	int toSquarePiece = abs(MailboxBoard64[toSquare]);
	int piece;
	uint64_t bb;

sideToMove:
	if (sideToMoveTotalGain > sideNotToMoveTotalGain)
		return true;

	if (attackersBB & PiecesBB[sideToMove][AllPieces]) // Any more attackers?
	{
		if (toSquarePiece == King)
			return true;
		// Find the lowest valued piece type
		piece = Pawn;
		while (!(attackersBB & PiecesBB[sideToMove][piece]))
			piece++;
		sideToMoveTotalGain += SeeValues[toSquarePiece];
		toSquarePiece = piece;
		bb = attackersBB & PiecesBB[sideToMove][piece];
		occupiedBB ^= (bb & -bb); // Remove the latest attacker(x & -x gives you the LS1B)
		// Add any new x-ray attacks by either side behind the just removed piece
		attackersBB |=
			(RookAttacksBB(toSquare, occupiedBB) & (PiecesBB[0][Rook] | PiecesBB[1][Rook] | PiecesBB[0][Queen] | PiecesBB[1][Queen])) |
			(BishopAttacksBB(toSquare, occupiedBB) & (PiecesBB[0][Bishop] | PiecesBB[1][Bishop] | PiecesBB[0][Queen] | PiecesBB[1][Queen]));
		attackersBB &= occupiedBB; // Remove all the empty squares and the discarded attacker(s)
	}
	else
		return false;

	//sideNotToMove:
	if (sideNotToMoveTotalGain >= sideToMoveTotalGain)
		return false;

	if (attackersBB & PiecesBB[sideNotToMove][AllPieces]) // Any more defenders?
	{
		if (toSquarePiece == King)
			return false;
		// Find the lowest valued piece type
		piece = Pawn;
		while (!(attackersBB & PiecesBB[sideNotToMove][piece]))
			piece++;
		sideNotToMoveTotalGain += SeeValues[toSquarePiece];
		toSquarePiece = piece;
		bb = attackersBB & PiecesBB[sideNotToMove][piece];
		occupiedBB ^= (bb & -bb); // Remove the latest attacker(x & -x gives you the LS1B)
		// Add any new x-ray attacks by either side behind the just removed piece
		attackersBB |=
			(RookAttacksBB(toSquare, occupiedBB) & (PiecesBB[0][Rook] | PiecesBB[1][Rook] | PiecesBB[0][Queen] | PiecesBB[1][Queen])) |
			(BishopAttacksBB(toSquare, occupiedBB) & (PiecesBB[0][Bishop] | PiecesBB[1][Bishop] | PiecesBB[0][Queen] | PiecesBB[1][Queen]));
		attackersBB &= occupiedBB; // Remove all the empty squares and the discarded attacker(s)
	}
	else
		return true;

	goto sideToMove;

	return false;
}

int Brain::KnownLowMaterialDraws(int sideToMove)
{
	// Tests for various known low material draws
	// Currently, no configurations with more than 4 pieces tested

	// No queens/rooks/pawns left for either side?
	if ((PiecesBB[0][Queen] | PiecesBB[1][Queen] | PiecesBB[0][Rook] | PiecesBB[1][Rook] | PiecesBB[0][Pawn] | PiecesBB[1][Pawn]) == 0)
	{
		// Lone white king?
		if (GameRecordPointer->totalMaterial[0] == 0)
		{
			// No pieces or lone Knight or lone Bishop? i.e. KvK, KvKB, KvKN
			if (GameRecordPointer->totalMaterial[1] <= MVBishop)
				return PVTDrawMinimumMaterial;

			// Two bishops of same colour? i.e. KvKBB
			if (GameRecordPointer->totalMaterial[1] == MVBishop + MVBishop)
				if (((PiecesBB[1][Bishop] & LightBB) == 0) || ((PiecesBB[1][Bishop] & DarkBB) == 0))
					return PVTDrawMinimumMaterial;

			// Two knights? i.e. KvKNN
			if (GameRecordPointer->totalMaterial[1] == MVKnight + MVKnight)
				if ((sideToMove == 0) || (PiecesBB[0][King] & ~CornersBB)) // White to move or the white king not in a corner
					return PVTDrawMinimumMaterial;
		}

		// Lone black king?
		if (GameRecordPointer->totalMaterial[1] == 0)
		{
			// No pieces or lone Knight or lone Bishop? i.e. KvK, KBvK, KNvK
			if (GameRecordPointer->totalMaterial[0] <= MVBishop)
				return PVTDrawMinimumMaterial;

			// Two bishops of same colour? i.e. KBBvK
			if (GameRecordPointer->totalMaterial[0] == MVBishop + MVBishop)
				if (((PiecesBB[0][Bishop] & LightBB) == 0) || ((PiecesBB[0][Bishop] & DarkBB) == 0))
					return PVTDrawMinimumMaterial;

			// Two knights? i.e. KNNvK
			if (GameRecordPointer->totalMaterial[0] == MVKnight + MVKnight)
				if ((sideToMove == 1) || (PiecesBB[1][King] & ~CornersBB)) // Black to move or the black king not in a corner
					return PVTDrawMinimumMaterial;
		}

		// KB v KB (bishops same colour)
		if ((GameRecordPointer->totalMaterial[0] == MVBishop) && (GameRecordPointer->totalMaterial[1] == MVBishop))
			if (PopulationCountX(PiecesBB[0][Bishop] & LightBB) == PopulationCountX(PiecesBB[1][Bishop] & LightBB))
				return PVTDrawMinimumMaterial;

		// Neither king in a corner?
		if (!((PiecesBB[0][King] ^ PiecesBB[1][King]) & CornersBB))
		{
			// KN v KN
			if ((GameRecordPointer->totalMaterial[0] == MVKnight) && (GameRecordPointer->totalMaterial[1] == MVKnight))
				return PVTDrawMinimumMaterial;

			// KB v KN
			if (
				((GameRecordPointer->totalMaterial[0] == MVBishop) && (GameRecordPointer->totalMaterial[1] == MVKnight)) ||
				((GameRecordPointer->totalMaterial[1] == MVBishop) && (GameRecordPointer->totalMaterial[0] == MVKnight))
				)
				return PVTDrawMinimumMaterial;

			//// Neither king on an edge?
			//if (!((piecesBB[0][King] ^ piecesBB[1][King]) & EdgesBB))
			//{
			//	if (((gameRecordPointer->totalMaterial[0] == materialValueKnight + materialValueKnight) && (gameRecordPointer->totalMaterial[1] == materialValueKnight)) || // KNN v KN
			//		((gameRecordPointer->totalMaterial[0] == materialValueKnight) && (gameRecordPointer->totalMaterial[1] == materialValueKnight + materialValueKnight)))
			//		return PVRDrawMinimumMaterial;
			//	if (((gameRecordPointer->totalMaterial[0] == materialValueKnight + materialValueKnight) && (gameRecordPointer->totalMaterial[1] == materialValueBishop)) || // KNN v KB
			//		((gameRecordPointer->totalMaterial[0] == materialValueBishop) && (gameRecordPointer->totalMaterial[1] == materialValueKnight + materialValueKnight)))
			//		return PVRDrawMinimumMaterial;
			//	// N.B. cannot do KBN v KN or KBN v KB as the lone N or B might be capturable!
			//	// COULD TEST FOR THAT! :) NO CAPTURES POSS!
			//	// OR ONLY CHECK IF THE SIDE WITH THE LONE PIECE HAS THE MOVE
			//}
		}

	}

	//bool canExtricate;
	//int blockedPawns, square;

	//// K v K + rook-file-pawn(s) + wrong coloured bishop
	//// TODO: K v K + any number of rook-file-pawn(s)
	//if (piecesBB[0][King] == piecesBB[0][AllPieces]) // Lone king?
	//	if (piecesBB[1][Pawn]) // Opponent has pawns
	//	{
	//		if (piecesBB[0][King] & BottomLeft4CornerBB)
	//		{
	//			canExtricate = true;
	//			if ((piecesBB[1][Queen] | piecesBB[1][Rook] | piecesBB[1][Knight]) == 0)
	//				if ((piecesBB[1][Bishop] & LightBB) == piecesBB[1][Bishop]) // Any number of light squared bishops cannot extricate the king from the bottom left corner
	//					canExtricate = false;

	//			if (!canExtricate)
	//				if ((piecesBB[1][Pawn] & ~FileABB) == 0) // Are all the pawns on the rook file?
	//					return PVRDrawMinimumMaterial;
	//		}
	//		else if (piecesBB[0][King] & BottomRight4CornerBB)
	//		{
	//			canExtricate = true;
	//			if ((piecesBB[1][Queen] | piecesBB[1][Rook] | piecesBB[1][Knight]) == 0)
	//				if ((piecesBB[1][Bishop] & DarkBB) == piecesBB[1][Bishop])
	//					canExtricate = false;

	//			if (!canExtricate)
	//				if ((piecesBB[1][Pawn] & ~FileHBB) == 0) // Are all the pawns on the rook file?
	//					return PVRDrawMinimumMaterial;
	//		}
	//	}

	//if (piecesBB[1][King] == piecesBB[1][AllPieces]) // Lone king?
	//	if (piecesBB[0][Pawn]) // Opponent has pawns
	//	{
	//		if (piecesBB[1][King] & TopLeft4CornerBB)
	//		{
	//			canExtricate = true;
	//			if ((piecesBB[0][Queen] | piecesBB[0][Rook] | piecesBB[0][Knight]) == 0)
	//				if ((piecesBB[0][Bishop] & DarkBB) == piecesBB[0][Bishop]) // Any number of dark squared bishops cannot extricate the king from the bottom left corner
	//					canExtricate = false;

	//			if (!canExtricate)
	//				if ((piecesBB[0][Pawn] & ~FileABB) == 0) // Are all the pawns on the rook file?
	//					return PVRDrawMinimumMaterial;
	//		}
	//		else if (piecesBB[1][King] & TopRight4CornerBB)
	//		{
	//			canExtricate = true;
	//			if ((piecesBB[0][Queen] | piecesBB[0][Rook] | piecesBB[0][Knight]) == 0)
	//				if ((piecesBB[0][Bishop] & LightBB) == piecesBB[0][Bishop]) // Any number of dark squared bishops cannot extricate the king from the bottom left corner
	//					canExtricate = false;

	//			if (!canExtricate)
	//				if ((piecesBB[0][Pawn] & ~FileHBB) == 0) // Are all the pawns on the rook file?
	//					return PVRDrawMinimumMaterial;
	//		}
	//	}

	return 0;
}

bool Brain::KnownLowMaterialWins()
{
	// Lone white king?
	if (GameRecordPointer->totalMaterial[0] == 0)
	{
		if (PiecesBB[1][Queen] | PiecesBB[1][Rook])
			return true;
	}
	// Lone black king?
	if (GameRecordPointer->totalMaterial[1] == 0)
	{
		if (PiecesBB[0][Queen] | PiecesBB[0][Rook])
			return true;
	}

	return false;
}

bool Brain::KingCanLegallyMove(int sideToMove)
{
	int fromSquare;
	uint64_t attacksBB;

	fromSquare = GetLS1BIndex(PiecesBB[sideToMove][King]);
	attacksBB = KingAttacksBBList[fromSquare] & ~PiecesBB[sideToMove][AllPieces];
	while (attacksBB)
	{
		int toSquare = GetLS1BIndex(attacksBB);
		if (!IsAttacked(toSquare, sideToMove ^ 1))
			return true;
		ClearLS1B(attacksBB);
	}

	return false;
}

bool Brain::ForcingLine(int ply, int offset)
{
	for (int i = offset; i < ply; i += 2)
	{
		//if (!((gameRecordPointer - i)->isZLKM || (gameRecordPointer - i)->isInCheck || (gameRecordPointer - i)->isO1M || (gameRecordPointer - i)->isTWM))
		//if (!((gameRecordPointer - i)->dangerConditions.dc.isZLKM))
		if (!((GameRecordPointer - i)->dangerConditions & TTFlagZeroLegalKingMoves))
			return false;
	}

	return true;
}

int Brain::SafePawnMoves(int sideToMove)
{
	uint64_t occupiedBB = PiecesBB[0][AllPieces] | PiecesBB[1][AllPieces];
	uint64_t stmPawnMoveBB = ((PiecesBB[sideToMove][Pawn] << 8) >> (sideToMove << 4)) & notOccupiedBB;
	uint64_t sntmPawnCaptureBB = ((PiecesBB[sideToMove ^ 1][Pawn] << 8) >> ((sideToMove ^ 1) << 4));
	sntmPawnCaptureBB = East(sntmPawnCaptureBB) | West(sntmPawnCaptureBB);
	return PopulationCountX(stmPawnMoveBB & ~sntmPawnCaptureBB);
}

bool Brain::HasOpposition(int sideToMove)
{
	int stmKingSquare = GetLS1BIndex(PiecesBB[sideToMove][King]);
	int sntmKingSquare = GetLS1BIndex(PiecesBB[sideToMove ^ 1][King]);
	if (ChebyshevDistance[stmKingSquare][sntmKingSquare] == 2)
		if (ManhattanDistance[stmKingSquare][sntmKingSquare] == 3)
			return true;

	return false;
}

std::string Brain::CurrentLine(int ply)
{
	// Construct a string representing the current line
	std::string currentLine = "";

	for (int i = 0; i < ply; i++)
		currentLine +=
		//"(" + MyITOA(MateLineTotalCostSaved[i + 1]) + ")" +
		MoveNotation(GameRecord[GameRecordIndexRoot + i].move.ui32) +
		" ";//"(" + lowerLimitPointer[i] + "/" + bestScorePointer[i] + "/" + upperLimitPointer[i] + ") ";

	return currentLine;
}

Move_Struct Brain::ThreateningMateInOneWithNull(int sideToMove, int &checksCount)
{
	Move_Struct result;

	GameRecordPointer++; // Normally done in make/unmake-move
	GameRecordPointer->castlingStatus = (GameRecordPointer - 1)->castlingStatus;
	GameRecordPointer->epSquare = 0;
	GameRecordPointer->pliesSinceIrreversible = 0;
	result = ThreateningMateInOne(sideToMove, checksCount);
	GameRecordPointer--; // Normally done in make/unmake-move

	return result;
}

Move_Struct Brain::ThreateningMateInOne(int sideToMove, int &checksCount)
{
	Move_Struct result;
	result.ui32 = 0;
	MoveWithScore_Struct moveList[220];
	Move_Struct currentMove;

	// Generate move list for attacker
	CalculatePinnedPieces(sideToMove); // Required for legal move generation
	CalculateDiscovererPieces(sideToMove); // Required for legal move generation
	checksCount = (int)(GenerateAllChecks(sideToMove, moveList) - moveList);

	for (int moveListIndexIterator = 0; moveListIndexIterator < checksCount; moveListIndexIterator++)
	{
		currentMove.ui32 = moveList[moveListIndexIterator].ui32;
		GameRecordPointer->move.ui32 = currentMove.ui32;
		MakeMove(sideToMove); // N.B. MakeMove increments MateBrain.gameRecordPointer!

		CalculatePinnedPieces(sideToMove ^ 1); // Required for legal move generation
		bool anyMoves = AnyMoves(sideToMove ^ 1, true);

		UnMakeMove(sideToMove); // N.B. UnMakeMove decrements MateBrain.gameRecordPointer!

		if (!anyMoves)
		{
			result.ui32 = GameRecordPointer->move.ui32;
			break;
		}
	}

	return result;
}

unsigned pyrrhic_move_to2(PyrrhicMove move) { return (move >> PYRRHIC_SHIFT_TO) & PYRRHIC_MASK_TO; }
unsigned pyrrhic_move_from2(PyrrhicMove move) { return (move >> PYRRHIC_SHIFT_FROM) & PYRRHIC_MASK_FROM; }
unsigned pyrrhic_move_promotes2(PyrrhicMove move) { return (move >> PYRRHIC_SHIFT_FLAGS) & PYRRHIC_MASK_PROMO_FLAGS; }

uint32_t Brain::SYZYGYPYRRHICMoveToColossusMove(uint16_t SYZYGYPYRRHICMove, uint32_t epSquare)
{
	uint32_t colossusMove;
	uint32_t fromSquare = pyrrhic_move_from2(SYZYGYPYRRHICMove);
	uint32_t toSquare = pyrrhic_move_to2(SYZYGYPYRRHICMove);
	colossusMove = fromSquare | (toSquare << 8);

	if (epSquare && (toSquare == epSquare) && (std::abs(MailboxBoard64[fromSquare]) == Pawn))
		colossusMove = fromSquare | ((toSquare - PawnMoveOffset[SideToMove]) << 8) | (MFEnPassant << 16);
	else
	{
		int promotionPiece = pyrrhic_move_promotes2(SYZYGYPYRRHICMove);
		if (promotionPiece)
		{
			if (promotionPiece == PYRRHIC_FLAG_QPROMO)
				promotionPiece = MFPromoteToQueen;
			else if (promotionPiece == PYRRHIC_FLAG_RPROMO)
				promotionPiece = MFPromoteToRook;
			else if (promotionPiece == PYRRHIC_FLAG_BPROMO)
				promotionPiece = MFPromoteToBishop;
			else //if (promotionPiece == PYRRHIC_FLAG_NPROMO)
				promotionPiece = MFPromoteToKnight;
			colossusMove |= (promotionPiece << 16);
		}
	}

	return colossusMove;
}