#include <algorithm>
#include <assert.h>

#include "BitBoard.h"
#include "Engine.h"
#include "Utilities.h"
#include "Brain.h"
#include "SearchNormal.h"
#include "SYZYGYPYRRHIC\tbprobe.h"

//----------------------------------------------------------------------------------------------------

Brain::Brain()
{
	gameRecord = new GameRecordEntry_Struct[gameRecordSize];
	ClearGameRecord();
}

Brain::~Brain()
{
	delete gameRecord;
}

//----------------------------------------------------------------------------------------------------

void Brain::CopyFrom(Brain* sourceBrain)
{
	// Copy the mailbox board and the game record from the source brain into this brain
	std::copy(sourceBrain->mailboxBoard64, sourceBrain->mailboxBoard64 + 64, this->mailboxBoard64); // N.B. "+64" is correct!
	for (int i = 0; i < this->gameRecordSize; i++)
		this->gameRecord[i] = sourceBrain->gameRecord[i];
	this->GameRecordIndexRoot = sourceBrain->GameRecordIndexRoot;
	this->gameRecordPointer = &gameRecord[GameRecordIndexRoot];
}

void Brain::ClearGameRecord()
{
	// The 0th and 1th entries are placeholders BEFORE the first move of the game. They may be referenced by 'improving' code/'follow-up-move' code which examines previous plies data
	gameRecord[0].move.ui32 = 0 | (0 << 8) | (MFCastling << 16); // Anything to flag it as irreversible
	gameRecord[0].move.fromSquarePiece = King;
	gameRecord[0].move.toSquarePiece = King;
	gameRecord[0].castlingStatus.ui32 = 0;
	gameRecord[0].sideToMove = 0;
	gameRecord[0].moveNumber = 0;
	gameRecord[0].transpositionTableHash64 = 0;
	gameRecord[0].isInCheck = 0;
	gameRecord[0].isTWM = TTFlagThreatenedWithMate;
	gameRecord[0].isFMTP = TTFlagFewerMovesThanPieces;
	gameRecord[0].isO1M = TTFlagOnlyOneLegalMove;
	gameRecord[0].isO1PCM = TTFlagOnlyOnePieceCanMove;
	gameRecord[0].isThreateningMateInOne.ui32 = 0;
	gameRecord[0].isZLKM = 1;
	gameRecord[0].isOKCM = 1;
	gameRecord[0].epSquare = 0;
	gameRecord[0].pliesSinceIrreversible = 0;
	gameRecord[0].givesCheck = 0;
	gameRecord[0].isThreateningMateInOne.ui32 = 0;
	gameRecord[0].forcingLine = true;
	gameRecord[0].forcingLineTWM = true;
	gameRecord[0].forcingMove = true;
	gameRecord[0].DefenderKingMovesBefore = 0;
	gameRecord[0].TotalDefenderKingMovesBefore = 0;
	gameRecord[0].DefenderKingMovesAfter = 0;
	gameRecord[0].TotalDefenderKingMovesAfter = 0;
	gameRecord[0].SEEResult = 0;
	gameRecord[0].move.ui32 = 0;
	gameRecord[0].move.fromSquarePiece = 1;
	gameRecord[0].move.toSquarePiece = 0;

	gameRecord[1].move.ui32 = 0 | (0 << 8) | (MFCastling << 16); // Anything to flag it as irreversible
	gameRecord[1].move.fromSquarePiece = King;
	gameRecord[1].move.toSquarePiece = King;
	gameRecord[1].castlingStatus.ui32 = 0;
	gameRecord[1].sideToMove = 1;
	gameRecord[1].moveNumber = 0;
	gameRecord[1].transpositionTableHash64 = 0;
	gameRecord[1].isInCheck = 0;
	gameRecord[1].isTWM = TTFlagThreatenedWithMate;
	gameRecord[1].isFMTP = TTFlagFewerMovesThanPieces;
	gameRecord[1].isO1M = TTFlagOnlyOneLegalMove;
	gameRecord[1].isO1PCM = TTFlagOnlyOnePieceCanMove;
	gameRecord[1].isThreateningMateInOne.ui32 = 0;
	gameRecord[1].isZLKM = 1;
	gameRecord[1].isOKCM = 1;
	gameRecord[1].epSquare = 0;
	gameRecord[1].pliesSinceIrreversible = 0;
	gameRecord[1].givesCheck = 0;
	gameRecord[1].isThreateningMateInOne.ui32 = 0;
	gameRecord[1].forcingLine = true;
	gameRecord[1].forcingLineTWM = true;
	gameRecord[1].forcingMove = true;
	gameRecord[1].DefenderKingMovesBefore = 0;
	gameRecord[0].TotalDefenderKingMovesBefore = 0;
	gameRecord[1].DefenderKingMovesAfter = 0;
	gameRecord[1].TotalDefenderKingMovesAfter = 0;
	gameRecord[1].SEEResult = 0;
	gameRecord[1].move.ui32 = 0;
	gameRecord[1].move.fromSquarePiece = 1;
	gameRecord[1].move.toSquarePiece = 0;

	// The 2th entry needs some fields initialising too (which may subsequently be updated if a FEN position is specified)
	gameRecord[2].sideToMove = 0;
	gameRecord[2].moveNumber = 1;
	gameRecord[2].castlingStatus.ui32 = 0;
	gameRecord[2].epSquare = 0;
	gameRecord[2].pliesSinceIrreversible = 0;

	// The first move of the game goes in the 2th entry
	GameRecordIndexRoot = 2;
	gameRecordPointer = &gameRecord[GameRecordIndexRoot];
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
	uint64_t occupiedBB = piecesBB[0][AllPieces] | piecesBB[1][AllPieces];
	uint32_t kingSquare = BitScanForwardX(piecesBB[sideToMove][King]);

	// Pawns
	int pmo = PawnMoveOffset[sideToMove];
	uint64_t pawnPromotionsBB = (((piecesBB[sideToMove][Pawn] & SeventhRankBB[sideToMove] & ~gameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB;
	while (pawnPromotionsBB)
	{
		toSquare = BitScanForwardX(pawnPromotionsBB);
		if (
			(!(UINT64SetBit(toSquare - pmo) & gameRecordPointer->pinnedRankFileBB)) || // Not pinned?
			(UINT64SetBit(toSquare) & LineListBB[toSquare - pmo][kingSquare])
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
	uint64_t pawnsCapturesEastBB = (East(((piecesBB[sideToMove][Pawn] & ~gameRecordPointer->pinnedRankFileBB) << 8) >> (sideToMove << 4))) & piecesBB[sideToMove ^ 1][AllPieces];
	while (pawnsCapturesEastBB)
	{
		toSquare = BitScanForwardX(pawnsCapturesEastBB);
		if (
			(!(UINT64SetBit(toSquare - (pmo + 1)) & gameRecordPointer->pinnedDiagonalBB)) || // Not pinned?
			(UINT64SetBit(toSquare) & LineListBB[toSquare - (pmo + 1)][kingSquare])
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
	uint64_t pawnsCapturesWestBB = (West(((piecesBB[sideToMove][Pawn] & ~gameRecordPointer->pinnedRankFileBB) << 8) >> (sideToMove << 4))) & piecesBB[sideToMove ^ 1][AllPieces];
	while (pawnsCapturesWestBB)
	{
		toSquare = BitScanForwardX(pawnsCapturesWestBB);
		if (
			(!(UINT64SetBit(toSquare - (pmo - 1)) & gameRecordPointer->pinnedDiagonalBB)) || // Not pinned?
			(UINT64SetBit(toSquare) & LineListBB[toSquare - (pmo - 1)][kingSquare])
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
	if (gameRecordPointer->epSquare)
	{
		Move_Struct previousMove;
		previousMove.ui32 = (gameRecordPointer - 1)->move.ui32;
		if (West(UINT64SetBit(previousMove.mf.toSquare)) & piecesBB[sideToMove][Pawn])
		{
			piecesBB[sideToMove][Pawn] ^= (UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare));
			piecesBB[sideToMove][AllPieces] ^= (UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare));
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
			if (!IsAttacked(kingSquare, sideToMove ^ 1))
				mlp++->ui32 = (previousMove.mf.toSquare - 1) | ((previousMove.mf.toSquare) << 8) | (MFEnPassant << 16);
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
		}
		if (East(UINT64SetBit(previousMove.mf.toSquare)) & piecesBB[sideToMove][Pawn])
		{
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
			if (!IsAttacked(kingSquare, sideToMove ^ 1))
				mlp++->ui32 = (previousMove.mf.toSquare + 1) | ((previousMove.mf.toSquare) << 8) | (MFEnPassant << 16);
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
		}
	}

	// Knights
	uint64_t knightsBB = piecesBB[sideToMove][Knight] & ~gameRecordPointer->pinnedAllBB;
	while (knightsBB)
	{
		fromSquare = BitScanForwardX(knightsBB);
		attacksBB = KnightAttacksBBList[fromSquare] & piecesBB[sideToMove ^ 1][AllPieces];
		while (attacksBB)
		{
			int toSquare = BitScanForwardX(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(knightsBB);
	}

	// Bishops and queens
	uint64_t bishopsAndQueensBB = piecesBB[sideToMove][Bishop] | piecesBB[sideToMove][Queen];
	while (bishopsAndQueensBB)
	{
		fromSquare = BitScanForwardX(bishopsAndQueensBB);
		attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & piecesBB[sideToMove ^ 1][AllPieces];
		if (UINT64SetBit(fromSquare) & gameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			int toSquare = BitScanForwardX(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(bishopsAndQueensBB);
	}

	// Rooks and queens
	uint64_t rooksAndQueensBB = piecesBB[sideToMove][Rook] | piecesBB[sideToMove][Queen];
	while (rooksAndQueensBB)
	{
		fromSquare = BitScanForwardX(rooksAndQueensBB);
		attacksBB = RookAttacksBB(fromSquare, occupiedBB) & piecesBB[sideToMove ^ 1][AllPieces];
		if (UINT64SetBit(fromSquare) & gameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			int toSquare = BitScanForwardX(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(rooksAndQueensBB);
	}

	// King
	attacksBB = KingAttacksBBList[kingSquare] & piecesBB[sideToMove ^ 1][AllPieces];
	while (attacksBB)
	{
		int toSquare = BitScanForwardX(attacksBB);
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
	uint64_t occupiedBB = piecesBB[0][AllPieces] | piecesBB[1][AllPieces];
	uint32_t kingSquare = BitScanForwardX(piecesBB[sideToMove][King]);

	// Pawns
	int pmo = PawnMoveOffset[sideToMove];
	uint64_t pawnMove1BB = (((piecesBB[sideToMove][Pawn] & ~gameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB; // The '>> (sideToMove << 4)' clause flips north to south when the 2nd side is to move
	while (pawnMove1BB)
	{
		toSquare = BitScanForwardX(pawnMove1BB);
		if (
			(!(UINT64SetBit(toSquare - pmo) & gameRecordPointer->pinnedRankFileBB)) || // Not pinned?
			(UINT64SetBit(toSquare) & LineListBB[toSquare - pmo][kingSquare])
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
	uint64_t pawnMove2BB = ((((((piecesBB[sideToMove][Pawn] & SecondRankBB[sideToMove] & ~gameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB) << 8) >> (sideToMove << 4)) & notOccupiedBB;
	while (pawnMove2BB)
	{
		toSquare = BitScanForwardX(pawnMove2BB);
		if (
			(!(UINT64SetBit(toSquare - (pmo * 2)) & gameRecordPointer->pinnedRankFileBB)) ||
			(UINT64SetBit(toSquare) & LineListBB[toSquare - (pmo * 2)][kingSquare])
			)
			mlp++->ui32 = (toSquare - pmo * 2) | (toSquare << 8);
		ClearLS1B(pawnMove2BB);
	}
	uint64_t pawnsCapturesEastBB = (East(((piecesBB[sideToMove][Pawn] & ~gameRecordPointer->pinnedRankFileBB) << 8) >> (sideToMove << 4))) & piecesBB[sideToMove ^ 1][AllPieces];
	while (pawnsCapturesEastBB)
	{
		toSquare = BitScanForwardX(pawnsCapturesEastBB);
		if (
			(!(UINT64SetBit(toSquare - (pmo + 1)) & gameRecordPointer->pinnedDiagonalBB)) || // Not pinned?
			(UINT64SetBit(toSquare) & LineListBB[toSquare - (pmo + 1)][kingSquare])
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
	uint64_t pawnsCapturesWestBB = (West(((piecesBB[sideToMove][Pawn] & ~gameRecordPointer->pinnedRankFileBB) << 8) >> (sideToMove << 4))) & piecesBB[sideToMove ^ 1][AllPieces];
	while (pawnsCapturesWestBB)
	{
		toSquare = BitScanForwardX(pawnsCapturesWestBB);
		if (
			(!(UINT64SetBit(toSquare - (pmo - 1)) & gameRecordPointer->pinnedDiagonalBB)) || // Not pinned?
			(UINT64SetBit(toSquare) & LineListBB[toSquare - (pmo - 1)][kingSquare])
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
	if (gameRecordPointer->epSquare)
	{
		Move_Struct previousMove;
		previousMove.ui32 = (gameRecordPointer - 1)->move.ui32;
		if (West(UINT64SetBit(previousMove.mf.toSquare)) & piecesBB[sideToMove][Pawn])
		{
			piecesBB[sideToMove][Pawn] ^= (UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare));
			piecesBB[sideToMove][AllPieces] ^= (UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare));
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
			if (!IsAttacked(kingSquare, sideToMove ^ 1))
				mlp++->ui32 = (previousMove.mf.toSquare - 1) | ((previousMove.mf.toSquare) << 8) | (MFEnPassant << 16);
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
		}
		if (East(UINT64SetBit(previousMove.mf.toSquare)) & piecesBB[sideToMove][Pawn])
		{
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
			if (!IsAttacked(kingSquare, sideToMove ^ 1))
				mlp++->ui32 = (previousMove.mf.toSquare + 1) | ((previousMove.mf.toSquare) << 8) | (MFEnPassant << 16);
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
		}
	}

	// Knights
	uint64_t knightsBB = piecesBB[sideToMove][Knight] & ~gameRecordPointer->pinnedAllBB;
	while (knightsBB)
	{
		fromSquare = BitScanForwardX(knightsBB);
		attacksBB = KnightAttacksBBList[fromSquare] & ~piecesBB[sideToMove][AllPieces];
		while (attacksBB)
		{
			int toSquare = BitScanForwardX(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(knightsBB);
	}

	// Bishops and queens
	uint64_t bishopsAndQueensBB = piecesBB[sideToMove][Bishop] | piecesBB[sideToMove][Queen];
	while (bishopsAndQueensBB)
	{
		fromSquare = BitScanForwardX(bishopsAndQueensBB);
		attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & ~piecesBB[sideToMove][AllPieces];
		if (UINT64SetBit(fromSquare) & gameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			int toSquare = BitScanForwardX(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(bishopsAndQueensBB);
	}

	// Rooks and queens
	uint64_t rooksAndQueensBB = piecesBB[sideToMove][Rook] | piecesBB[sideToMove][Queen];
	while (rooksAndQueensBB)
	{
		fromSquare = BitScanForwardX(rooksAndQueensBB);
		attacksBB = RookAttacksBB(fromSquare, occupiedBB) & ~piecesBB[sideToMove][AllPieces];
		if (UINT64SetBit(fromSquare) & gameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			int toSquare = BitScanForwardX(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(rooksAndQueensBB);
	}

	// King
	attacksBB = KingAttacksBBList[kingSquare] & ~piecesBB[sideToMove][AllPieces];
	while (attacksBB)
	{
		int toSquare = BitScanForwardX(attacksBB);
		if (!IsAttacked(toSquare, sideToMove ^ 1))
			mlp++->ui32 = kingSquare | (toSquare << 8);
		ClearLS1B(attacksBB);
	}

	// Generate castling moves
	if (UCI_Chess960)
	{
		// King side
		if (gameRecordPointer->castlingStatus.ui8[sideToMove][0] == 0)
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
					if (mailboxBoard64[square] != Empty)
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
						if (mailboxBoard64[square] != Empty)
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
					if (((piecesBB[sideToMove ^ 1][Rook] | piecesBB[sideToMove ^ 1][Queen]) & UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + H)) == 0)
						mlp++->ui32 = kingSquare | ((BackRankBaseSquareIndex[sideToMove] + G) << 8) | (MFCastling << 16);
				}
			}
		}

		// Queen side
		if (gameRecordPointer->castlingStatus.ui8[sideToMove][1] == 0)
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
					if (mailboxBoard64[square] != Empty)
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
						if (mailboxBoard64[square] != Empty)
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
					uint64_t rooksAndQueensBB = piecesBB[sideToMove ^ 1][Rook] | piecesBB[sideToMove ^ 1][Queen];
					if ((rooksAndQueensBB & UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + B)) == 0)
						if (((rooksAndQueensBB & UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + A)) == 0) || ((mailboxBoard64[BackRankBaseSquareIndex[sideToMove] + B] != Empty) && (InitialQueenSideRookFile != B)))
							mlp++->ui32 = kingSquare | ((BackRankBaseSquareIndex[sideToMove] + C) << 8) | (MFCastling << 16);
				}
			}
		}
	}
	else
	{
		// King side
		if (gameRecordPointer->castlingStatus.ui8[sideToMove][0] == 0)
			if ((mailboxBoard64[kingSquare + 2] | mailboxBoard64[kingSquare + 1]) == Empty)
				if (!IsAttacked(kingSquare + 1, sideToMove ^ 1) && !IsAttacked(kingSquare + 2, sideToMove ^ 1))
					mlp++->ui32 = kingSquare | ((kingSquare + 2) << 8) | (MFCastling << 16);

		// Queen side
		if (gameRecordPointer->castlingStatus.ui8[sideToMove][1] == 0)
			if ((mailboxBoard64[kingSquare - 3] | mailboxBoard64[kingSquare - 2] | mailboxBoard64[kingSquare - 1]) == Empty)
				if (!IsAttacked(kingSquare - 1, sideToMove ^ 1) && !IsAttacked(kingSquare - 2, sideToMove ^ 1))
					mlp++->ui32 = kingSquare | ((kingSquare - 2) << 8) | (MFCastling << 16);
	}

	return mlp;
}

uint64_t Brain::CountCapturesAndNonCaptures(int sideToMove)
{
	uint32_t fromSquare, toSquare;
	uint64_t attacksBB;
	uint64_t occupiedBB = piecesBB[0][AllPieces] | piecesBB[1][AllPieces];
	uint32_t kingSquare = BitScanForwardX(piecesBB[sideToMove][King]);
	uint64_t moves = 0;

	// Pawns
	int pmo = PawnMoveOffset[sideToMove];
	uint64_t pawnMove1BB = (((piecesBB[sideToMove][Pawn] & ~gameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB;
	while (pawnMove1BB)
	{
		toSquare = BitScanForwardX(pawnMove1BB);
		if (
			(!(UINT64SetBit(toSquare - pmo) & gameRecordPointer->pinnedRankFileBB)) || // Not pinned?
			(UINT64SetBit(toSquare) & LineListBB[toSquare - pmo][kingSquare])
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
	uint64_t pawnMove2BB = ((((((piecesBB[sideToMove][Pawn] & SecondRankBB[sideToMove] & ~gameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB) << 8) >> (sideToMove << 4)) & notOccupiedBB;
	while (pawnMove2BB)
	{
		toSquare = BitScanForwardX(pawnMove2BB);
		if (
			(!(UINT64SetBit(toSquare - (pmo * 2)) & gameRecordPointer->pinnedRankFileBB)) ||
			(UINT64SetBit(toSquare) & LineListBB[toSquare - (pmo * 2)][kingSquare])
			)
			moves++;
		ClearLS1B(pawnMove2BB);
	}
	uint64_t pawnsCapturesEastBB = (East(((piecesBB[sideToMove][Pawn] & ~gameRecordPointer->pinnedRankFileBB) << 8) >> (sideToMove << 4))) & piecesBB[sideToMove ^ 1][AllPieces];
	while (pawnsCapturesEastBB)
	{
		toSquare = BitScanForwardX(pawnsCapturesEastBB);
		if (
			(!(UINT64SetBit(toSquare - (pmo + 1)) & gameRecordPointer->pinnedDiagonalBB)) || // Not pinned?
			(UINT64SetBit(toSquare) & LineListBB[toSquare - (pmo + 1)][kingSquare])
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
	uint64_t pawnsCapturesWestBB = (West(((piecesBB[sideToMove][Pawn] & ~gameRecordPointer->pinnedRankFileBB) << 8) >> (sideToMove << 4))) & piecesBB[sideToMove ^ 1][AllPieces];
	while (pawnsCapturesWestBB)
	{
		toSquare = BitScanForwardX(pawnsCapturesWestBB);
		if (
			(!(UINT64SetBit(toSquare - (pmo - 1)) & gameRecordPointer->pinnedDiagonalBB)) || // Not pinned?
			(UINT64SetBit(toSquare) & LineListBB[toSquare - (pmo - 1)][kingSquare])
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
	if (gameRecordPointer->epSquare)
	{
		Move_Struct previousMove;
		previousMove.ui32 = (gameRecordPointer - 1)->move.ui32;
		if (West(UINT64SetBit(previousMove.mf.toSquare)) & piecesBB[sideToMove][Pawn])
		{
			piecesBB[sideToMove][Pawn] ^= (UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare));
			piecesBB[sideToMove][AllPieces] ^= (UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare));
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
			if (!IsAttacked(kingSquare, sideToMove ^ 1))
				moves++;
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
		}
		if (East(UINT64SetBit(previousMove.mf.toSquare)) & piecesBB[sideToMove][Pawn])
		{
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
			if (!IsAttacked(kingSquare, sideToMove ^ 1))
				moves++;
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
		}
	}

	// Knights
	uint64_t knightsBB = piecesBB[sideToMove][Knight] & ~gameRecordPointer->pinnedAllBB;
	while (knightsBB)
	{
		fromSquare = BitScanForwardX(knightsBB);
		attacksBB = KnightAttacksBBList[fromSquare] & ~piecesBB[sideToMove][AllPieces];
		moves += PopulationCountX(attacksBB);
		ClearLS1B(knightsBB);
	}

	// Bishops and queens
	uint64_t bishopsAndQueensBB = piecesBB[sideToMove][Bishop] | piecesBB[sideToMove][Queen];
	while (bishopsAndQueensBB)
	{
		fromSquare = BitScanForwardX(bishopsAndQueensBB);
		attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & ~piecesBB[sideToMove][AllPieces];
		if (UINT64SetBit(fromSquare) & gameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		moves += PopulationCountX(attacksBB);
		ClearLS1B(bishopsAndQueensBB);
	}

	// Rooks and queens
	uint64_t rooksAndQueensBB = piecesBB[sideToMove][Rook] | piecesBB[sideToMove][Queen];
	while (rooksAndQueensBB)
	{
		fromSquare = BitScanForwardX(rooksAndQueensBB);
		attacksBB = RookAttacksBB(fromSquare, occupiedBB) & ~piecesBB[sideToMove][AllPieces];
		if (UINT64SetBit(fromSquare) & gameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		moves += PopulationCountX(attacksBB);
		ClearLS1B(rooksAndQueensBB);
	}

	// King
	attacksBB = KingAttacksBBList[kingSquare] & ~piecesBB[sideToMove][AllPieces];
	while (attacksBB)
	{
		int toSquare = BitScanForwardX(attacksBB);
		if (!IsAttacked(toSquare, sideToMove ^ 1))
			moves++;
		ClearLS1B(attacksBB);
	}

	// Generate castling moves
	if (UCI_Chess960)
	{
		// King side
		if (gameRecordPointer->castlingStatus.ui8[sideToMove][0] == 0)
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
					if (mailboxBoard64[square] != Empty)
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
						if (mailboxBoard64[square] != Empty)
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
					if (((piecesBB[sideToMove ^ 1][Rook] | piecesBB[sideToMove ^ 1][Queen]) & UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + H)) == 0)
						moves++;
				}
			}
		}

		// Queen side
		if (gameRecordPointer->castlingStatus.ui8[sideToMove][1] == 0)
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
					if (mailboxBoard64[square] != Empty)
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
						if (mailboxBoard64[square] != Empty)
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
					uint64_t rooksAndQueensBB = piecesBB[sideToMove ^ 1][Rook] | piecesBB[sideToMove ^ 1][Queen];
					if ((rooksAndQueensBB & UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + B)) == 0)
						if (((rooksAndQueensBB & UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + A)) == 0) || ((mailboxBoard64[BackRankBaseSquareIndex[sideToMove] + B] != Empty) && (InitialQueenSideRookFile != B)))
							moves++;
				}
			}
		}
	}
	else
	{
		// King side
		if (gameRecordPointer->castlingStatus.ui8[sideToMove][0] == 0)
			if ((mailboxBoard64[kingSquare + 2] | mailboxBoard64[kingSquare + 1]) == Empty)
				if (!IsAttacked(kingSquare + 1, sideToMove ^ 1) && !IsAttacked(kingSquare + 2, sideToMove ^ 1))
					moves++;

		// Queen side
		if (gameRecordPointer->castlingStatus.ui8[sideToMove][1] == 0)
			if ((mailboxBoard64[kingSquare - 3] | mailboxBoard64[kingSquare - 2] | mailboxBoard64[kingSquare - 1]) == Empty)
				if (!IsAttacked(kingSquare - 1, sideToMove ^ 1) && !IsAttacked(kingSquare - 2, sideToMove ^ 1))
					moves++;
	}

	return moves;
}

bool Brain::AnyCapturesAndNonCaptures(int sideToMove)
{
	uint32_t fromSquare, toSquare;
	uint64_t attacksBB;
	uint64_t occupiedBB = piecesBB[0][AllPieces] | piecesBB[1][AllPieces];
	uint32_t kingSquare = BitScanForwardX(piecesBB[sideToMove][King]);
	//uint64_t moves = 0;
	bool canMove;

	// Pawns
	int pmo = PawnMoveOffset[sideToMove];
	uint64_t pawnMove1BB = (((piecesBB[sideToMove][Pawn] & ~gameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB;
	while (pawnMove1BB)
	{
		toSquare = BitScanForwardX(pawnMove1BB);
		if (
			(!(UINT64SetBit(toSquare - pmo) & gameRecordPointer->pinnedRankFileBB)) || // Not pinned?
			(UINT64SetBit(toSquare) & LineListBB[toSquare - pmo][kingSquare])
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
	uint64_t pawnMove2BB = ((((((piecesBB[sideToMove][Pawn] & SecondRankBB[sideToMove] & ~gameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB) << 8) >> (sideToMove << 4)) & notOccupiedBB;
	while (pawnMove2BB)
	{
		toSquare = BitScanForwardX(pawnMove2BB);
		if (
			(!(UINT64SetBit(toSquare - (pmo * 2)) & gameRecordPointer->pinnedRankFileBB)) ||
			(UINT64SetBit(toSquare) & LineListBB[toSquare - (pmo * 2)][kingSquare])
			)
			//moves++;
			return true;
		ClearLS1B(pawnMove2BB);
	}
	uint64_t pawnsCapturesEastBB = (East(((piecesBB[sideToMove][Pawn] & ~gameRecordPointer->pinnedRankFileBB) << 8) >> (sideToMove << 4))) & piecesBB[sideToMove ^ 1][AllPieces];
	while (pawnsCapturesEastBB)
	{
		toSquare = BitScanForwardX(pawnsCapturesEastBB);
		if (
			(!(UINT64SetBit(toSquare - (pmo + 1)) & gameRecordPointer->pinnedDiagonalBB)) || // Not pinned?
			(UINT64SetBit(toSquare) & LineListBB[toSquare - (pmo + 1)][kingSquare])
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
	uint64_t pawnsCapturesWestBB = (West(((piecesBB[sideToMove][Pawn] & ~gameRecordPointer->pinnedRankFileBB) << 8) >> (sideToMove << 4))) & piecesBB[sideToMove ^ 1][AllPieces];
	while (pawnsCapturesWestBB)
	{
		toSquare = BitScanForwardX(pawnsCapturesWestBB);
		if (
			(!(UINT64SetBit(toSquare - (pmo - 1)) & gameRecordPointer->pinnedDiagonalBB)) || // Not pinned?
			(UINT64SetBit(toSquare) & LineListBB[toSquare - (pmo - 1)][kingSquare])
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
	if (gameRecordPointer->epSquare)
	{
		Move_Struct previousMove;
		previousMove.ui32 = (gameRecordPointer - 1)->move.ui32;
		if (West(UINT64SetBit(previousMove.mf.toSquare)) & piecesBB[sideToMove][Pawn])
		{
			piecesBB[sideToMove][Pawn] ^= (UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare));
			piecesBB[sideToMove][AllPieces] ^= (UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare));
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
			//if (!IsAttacked(kingSquare, sideToMove ^ 1))
				//moves++;
			canMove = !IsAttacked(kingSquare, sideToMove ^ 1);
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
			if (canMove)
				return true;
		}
		if (East(UINT64SetBit(previousMove.mf.toSquare)) & piecesBB[sideToMove][Pawn])
		{
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
			//if (!IsAttacked(kingSquare, sideToMove ^ 1))
				//moves++;
			canMove = !IsAttacked(kingSquare, sideToMove ^ 1);
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
			if (canMove)
				return true;
		}
	}

	// Knights
	uint64_t knightsBB = piecesBB[sideToMove][Knight] & ~gameRecordPointer->pinnedAllBB;
	while (knightsBB)
	{
		fromSquare = BitScanForwardX(knightsBB);
		attacksBB = KnightAttacksBBList[fromSquare] & ~piecesBB[sideToMove][AllPieces];
		//moves += PopulationCountX(attacksBB);
		if (PopulationCountX(attacksBB))
			return true;
		ClearLS1B(knightsBB);
	}

	// Bishops and queens
	uint64_t bishopsAndQueensBB = piecesBB[sideToMove][Bishop] | piecesBB[sideToMove][Queen];
	while (bishopsAndQueensBB)
	{
		fromSquare = BitScanForwardX(bishopsAndQueensBB);
		attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & ~piecesBB[sideToMove][AllPieces];
		if (UINT64SetBit(fromSquare) & gameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		//moves += PopulationCountX(attacksBB);
		if (PopulationCountX(attacksBB))
			return true;
		ClearLS1B(bishopsAndQueensBB);
	}

	// Rooks and queens
	uint64_t rooksAndQueensBB = piecesBB[sideToMove][Rook] | piecesBB[sideToMove][Queen];
	while (rooksAndQueensBB)
	{
		fromSquare = BitScanForwardX(rooksAndQueensBB);
		attacksBB = RookAttacksBB(fromSquare, occupiedBB) & ~piecesBB[sideToMove][AllPieces];
		if (UINT64SetBit(fromSquare) & gameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		//moves += PopulationCountX(attacksBB);
		if (PopulationCountX(attacksBB))
			return true;
		ClearLS1B(rooksAndQueensBB);
	}

	// King
	attacksBB = KingAttacksBBList[kingSquare] & ~piecesBB[sideToMove][AllPieces];
	while (attacksBB)
	{
		int toSquare = BitScanForwardX(attacksBB);
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
	//				if (((piecesBB[sideToMove ^ 1][Rook] | piecesBB[sideToMove ^ 1][Queen]) & UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + H)) == 0)
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
	//				if ((rooksAndQueensBB & UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + B)) == 0)
	//					if (((rooksAndQueensBB & UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + A)) == 0) || ((mailboxBoard64[BackRankBaseSquareIndex[sideToMove] + B] != Empty) && (InitialQueenSideRookFile != B)))
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
	uint64_t occupiedBB = piecesBB[0][AllPieces] | piecesBB[1][AllPieces];
	uint32_t kingSquare = BitScanForwardX(piecesBB[sideToMove][King]);

	// Generate a bitboard containing all the checking pieces
	uint64_t enemyCheckersBB =
		(BishopAttacksBB(kingSquare, occupiedBB) & (piecesBB[sideToMove ^ 1][Bishop] | piecesBB[sideToMove ^ 1][Queen])) |
		(RookAttacksBB(kingSquare, occupiedBB) & (piecesBB[sideToMove ^ 1][Rook] | piecesBB[sideToMove ^ 1][Queen])) |
		(KnightAttacksBBList[kingSquare] & piecesBB[sideToMove ^ 1][Knight]) |
		(PawnAttacksBBList[sideToMove][kingSquare] & piecesBB[sideToMove ^ 1][Pawn])
		;
	assert(PopulationCountX(enemyCheckersBB) >= 1);
	if (PopulationCountX(enemyCheckersBB) > 1) // If in double-check then only king moves are possible
		goto king;

	// So we now know that enemyCheckersBB has only one bit set
	// N.B. If we are in check a pinned piece cannot move at all

	int checkerSquare = BitScanForwardX(enemyCheckersBB);
	uint64_t enemyCheckersBetweenSquaresBB = BetweenListBB[checkerSquare][kingSquare];

	// Pawns
	int pmo = PawnMoveOffset[sideToMove];
	uint64_t pawnsCapturesEastBB = (East((piecesBB[sideToMove][Pawn] << 8) >> (sideToMove << 4))) & enemyCheckersBB;
	if (pawnsCapturesEastBB) // At most one bit set
	{
		toSquare = BitScanForwardX(pawnsCapturesEastBB);
		if (!(UINT64SetBit(toSquare - (pmo + 1)) & gameRecordPointer->pinnedAllBB)) // Not pinned?
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
	uint64_t pawnsCapturesWestBB = (West((piecesBB[sideToMove][Pawn] << 8) >> (sideToMove << 4))) & enemyCheckersBB;
	if (pawnsCapturesWestBB)
	{
		toSquare = BitScanForwardX(pawnsCapturesWestBB);
		if (!(UINT64SetBit(toSquare - (pmo - 1)) & gameRecordPointer->pinnedAllBB)) // Not pinned?
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
	if (gameRecordPointer->epSquare)
	{
		Move_Struct previousMove;
		previousMove.ui32 = (gameRecordPointer - 1)->move.ui32;
		if (West(UINT64SetBit(previousMove.mf.toSquare)) & piecesBB[sideToMove][Pawn])
		{
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
			if (!IsAttacked(kingSquare, sideToMove ^ 1))
				mlp++->ui32 = (previousMove.mf.toSquare - 1) | ((previousMove.mf.toSquare) << 8) | (MFEnPassant << 16);
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
		}
		if (East(UINT64SetBit(previousMove.mf.toSquare)) & piecesBB[sideToMove][Pawn])
		{
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
			if (!IsAttacked(kingSquare, sideToMove ^ 1))
				mlp++->ui32 = (previousMove.mf.toSquare + 1) | ((previousMove.mf.toSquare) << 8) | (MFEnPassant << 16);
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
		}
	}
	uint64_t pawnMove1BB = ((piecesBB[sideToMove][Pawn] << 8) >> (sideToMove << 4)) & enemyCheckersBetweenSquaresBB;
	while (pawnMove1BB)
	{
		toSquare = BitScanForwardX(pawnMove1BB);
		if (!(UINT64SetBit(toSquare - pmo) & gameRecordPointer->pinnedAllBB)) // Not pinned?
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
	uint64_t pawnMove2BB = ((((((piecesBB[sideToMove][Pawn] & SecondRankBB[sideToMove]) << 8) >> (sideToMove << 4)) & notOccupiedBB) << 8) >> (sideToMove << 4)) & enemyCheckersBetweenSquaresBB;
	while (pawnMove2BB)
	{
		toSquare = BitScanForwardX(pawnMove2BB);
		if (!(UINT64SetBit(toSquare - (pmo * 2)) & gameRecordPointer->pinnedAllBB)) // Not pinned?
			mlp++->ui32 = toSquare - (pmo * 2) | (toSquare << 8);
		ClearLS1B(pawnMove2BB);
	}

	// Knights
	uint64_t knightsBB = piecesBB[sideToMove][Knight] & ~gameRecordPointer->pinnedAllBB;
	while (knightsBB)
	{
		fromSquare = BitScanForwardX(knightsBB);
		attacksBB = KnightAttacksBBList[fromSquare] & (enemyCheckersBB | enemyCheckersBetweenSquaresBB);
		while (attacksBB)
		{
			int toSquare = BitScanForwardX(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(knightsBB);
	}

	// Bishops and queens
	//uint64_t bishopsAndQueensBB = piecesBB[sideToMove][Bishop] | piecesBB[sideToMove][Queen];
	uint64_t bishopsAndQueensBB = (piecesBB[sideToMove][Bishop] | piecesBB[sideToMove][Queen]) & ~gameRecordPointer->pinnedAllBB;
	while (bishopsAndQueensBB)
	{
		fromSquare = BitScanForwardX(bishopsAndQueensBB);
		//if (!(UINT64SetBit(fromSquare) & gameRecordPointer->pinnedAllBB)) // Not pinned?
		{
			attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & (enemyCheckersBB | enemyCheckersBetweenSquaresBB);
			while (attacksBB)
			{
				int toSquare = BitScanForwardX(attacksBB);
				mlp++->ui32 = fromSquare | (toSquare << 8);
				ClearLS1B(attacksBB);
			}
		}
		ClearLS1B(bishopsAndQueensBB);
	}

	// Rooks and queens
	//uint64_t rooksAndQueensBB = piecesBB[sideToMove][Rook] | piecesBB[sideToMove][Queen];
	uint64_t rooksAndQueensBB = (piecesBB[sideToMove][Rook] | piecesBB[sideToMove][Queen]) & ~gameRecordPointer->pinnedAllBB;
	while (rooksAndQueensBB)
	{
		fromSquare = BitScanForwardX(rooksAndQueensBB);
		//if (!(UINT64SetBit(fromSquare) & gameRecordPointer->pinnedAllBB)) // Not pinned?
		{
			attacksBB = RookAttacksBB(fromSquare, occupiedBB) & (enemyCheckersBB | enemyCheckersBetweenSquaresBB);
			while (attacksBB)
			{
				int toSquare = BitScanForwardX(attacksBB);
				mlp++->ui32 = fromSquare | (toSquare << 8);
				ClearLS1B(attacksBB);
			}
		}
		ClearLS1B(rooksAndQueensBB);
	}

	// King
king:
	attacksBB = KingAttacksBBList[kingSquare] & ~piecesBB[sideToMove][AllPieces];
	piecesBB[sideToMove][AllPieces] ^= piecesBB[sideToMove][King];
	while (attacksBB)
	{
		int toSquare = BitScanForwardX(attacksBB);
		if (!IsAttacked(toSquare, sideToMove ^ 1))
			mlp++->ui32 = kingSquare | (toSquare << 8);
		ClearLS1B(attacksBB);
	}
	piecesBB[sideToMove][AllPieces] ^= piecesBB[sideToMove][King];

	return mlp;
}

uint64_t Brain::CountAllMovesOutOfCheck(int sideToMove)
{
	uint32_t fromSquare, toSquare;
	uint64_t attacksBB;
	uint64_t occupiedBB = piecesBB[0][AllPieces] | piecesBB[1][AllPieces];
	uint32_t kingSquare = BitScanForwardX(piecesBB[sideToMove][King]);
	uint64_t moves = 0;

	// Generate a bitboard containing all the checking pieces
	uint64_t enemyCheckersBB =
		(BishopAttacksBB(kingSquare, occupiedBB) & (piecesBB[sideToMove ^ 1][Bishop] | piecesBB[sideToMove ^ 1][Queen])) |
		(RookAttacksBB(kingSquare, occupiedBB) & (piecesBB[sideToMove ^ 1][Rook] | piecesBB[sideToMove ^ 1][Queen])) |
		(KnightAttacksBBList[kingSquare] & piecesBB[sideToMove ^ 1][Knight]) |
		(PawnAttacksBBList[sideToMove][kingSquare] & piecesBB[sideToMove ^ 1][Pawn])
		;
	assert(PopulationCountX(enemyCheckersBB) >= 1);
	if (PopulationCountX(enemyCheckersBB) > 1) // If in double-check then only King moves are possible
		goto king;

	// So we now know that enemyCheckersBB has only one bit set
	// N.B. If we are in check a pinned piece cannot move at all

	int checkerSquare = BitScanForwardX(enemyCheckersBB);
	uint64_t enemyCheckersBetweenSquaresBB = BetweenListBB[checkerSquare][kingSquare];

	// Pawns
	int pmo = PawnMoveOffset[sideToMove];
	uint64_t pawnsCapturesEastBB = (East((piecesBB[sideToMove][Pawn] << 8) >> (sideToMove << 4))) & enemyCheckersBB;
	if (pawnsCapturesEastBB) // At most one bit set
	{
		toSquare = BitScanForwardX(pawnsCapturesEastBB);
		if (!(UINT64SetBit(toSquare - (pmo + 1)) & gameRecordPointer->pinnedAllBB)) // Not pinned?
		{
			if ((toSquare >> 3) == EigthRank[sideToMove])
			{
				moves += 4;
			}
			else
				moves++;
		}
	}
	uint64_t pawnsCapturesWestBB = (West((piecesBB[sideToMove][Pawn] << 8) >> (sideToMove << 4))) & enemyCheckersBB;
	if (pawnsCapturesWestBB)
	{
		toSquare = BitScanForwardX(pawnsCapturesWestBB);
		if (!(UINT64SetBit(toSquare - (pmo - 1)) & gameRecordPointer->pinnedAllBB)) // Not pinned?
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
	if (gameRecordPointer->epSquare)
	{
		Move_Struct previousMove;
		previousMove.ui32 = (gameRecordPointer - 1)->move.ui32;
		if (West(UINT64SetBit(previousMove.mf.toSquare)) & piecesBB[sideToMove][Pawn])
		{
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
			if (!IsAttacked(kingSquare, sideToMove ^ 1))
				moves++;
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
		}
		if (East(UINT64SetBit(previousMove.mf.toSquare)) & piecesBB[sideToMove][Pawn])
		{
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
			if (!IsAttacked(kingSquare, sideToMove ^ 1))
				moves++;
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
		}
	}
	uint64_t pawnMove1BB = ((piecesBB[sideToMove][Pawn] << 8) >> (sideToMove << 4)) & enemyCheckersBetweenSquaresBB;
	while (pawnMove1BB)
	{
		toSquare = BitScanForwardX(pawnMove1BB);
		if (!(UINT64SetBit(toSquare - pmo) & gameRecordPointer->pinnedAllBB)) // Not pinned?
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
	uint64_t pawnMove2BB = ((((((piecesBB[sideToMove][Pawn] & SecondRankBB[sideToMove]) << 8) >> (sideToMove << 4)) & notOccupiedBB) << 8) >> (sideToMove << 4)) & enemyCheckersBetweenSquaresBB;
	while (pawnMove2BB)
	{
		toSquare = BitScanForwardX(pawnMove2BB);
		if (!(UINT64SetBit(toSquare - (pmo * 2)) & gameRecordPointer->pinnedAllBB)) // Not pinned?
			moves++;
		ClearLS1B(pawnMove2BB);
	}

	// If we are in check then pinned pieces can't move at all as they couldn't capture the checker/interrupt the check without exposing the king

	// Knights
	uint64_t knightsBB = piecesBB[sideToMove][Knight] & ~gameRecordPointer->pinnedAllBB;//CAN DO PINNED TEST HERE AS IN COUNTCPAS&NONCAPS!
	while (knightsBB)
	{
		fromSquare = BitScanForwardX(knightsBB);
		//if (!(UINT64SetBit(fromSquare) & gameRecordPointer->pinnedAllBB)) // Not pinned?
		{
			attacksBB = KnightAttacksBBList[fromSquare] & (enemyCheckersBB | enemyCheckersBetweenSquaresBB);
			moves += PopulationCountX(attacksBB);
		}
		ClearLS1B(knightsBB);
	}

	// Bishops and queens
	uint64_t bishopsAndQueensBB = (piecesBB[sideToMove][Bishop] | piecesBB[sideToMove][Queen]) & ~gameRecordPointer->pinnedAllBB;
	while (bishopsAndQueensBB)
	{
		fromSquare = BitScanForwardX(bishopsAndQueensBB);
		//if (!(UINT64SetBit(fromSquare) & gameRecordPointer->pinnedAllBB)) // Not pinned?
		{
			attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & (enemyCheckersBB | enemyCheckersBetweenSquaresBB);
			moves += PopulationCountX(attacksBB);
		}
		ClearLS1B(bishopsAndQueensBB);
	}

	// Rooks and queens
	uint64_t rooksAndQueensBB = (piecesBB[sideToMove][Rook] | piecesBB[sideToMove][Queen]) & ~gameRecordPointer->pinnedAllBB;
	while (rooksAndQueensBB)
	{
		fromSquare = BitScanForwardX(rooksAndQueensBB);
		//if (!(UINT64SetBit(fromSquare) & gameRecordPointer->pinnedAllBB)) // Not pinned?
		{
			attacksBB = RookAttacksBB(fromSquare, occupiedBB) & (enemyCheckersBB | enemyCheckersBetweenSquaresBB);
			moves += PopulationCountX(attacksBB);
		}
		ClearLS1B(rooksAndQueensBB);
	}

	// King
king:
	attacksBB = KingAttacksBBList[kingSquare] & ~piecesBB[sideToMove][AllPieces];
	piecesBB[sideToMove][AllPieces] ^= piecesBB[sideToMove][King];
	while (attacksBB)
	{
		int toSquare = BitScanForwardX(attacksBB);
		if (!IsAttacked(toSquare, sideToMove ^ 1))
			moves++;
		ClearLS1B(attacksBB);
	}
	piecesBB[sideToMove][AllPieces] ^= piecesBB[sideToMove][King];

	return moves;
}

bool Brain::AnyMovesOutOfCheck(int sideToMove)
{
	uint32_t fromSquare, toSquare;
	uint64_t attacksBB;
	uint64_t occupiedBB = piecesBB[0][AllPieces] | piecesBB[1][AllPieces];
	uint32_t kingSquare = BitScanForwardX(piecesBB[sideToMove][King]);
	//uint64_t moves = 0;
	bool canMove;

	// Generate a bitboard containing all the checking pieces
	uint64_t enemyCheckersBB =
		(BishopAttacksBB(kingSquare, occupiedBB) & (piecesBB[sideToMove ^ 1][Bishop] | piecesBB[sideToMove ^ 1][Queen])) |
		(RookAttacksBB(kingSquare, occupiedBB) & (piecesBB[sideToMove ^ 1][Rook] | piecesBB[sideToMove ^ 1][Queen])) |
		(KnightAttacksBBList[kingSquare] & piecesBB[sideToMove ^ 1][Knight]) |
		(PawnAttacksBBList[sideToMove][kingSquare] & piecesBB[sideToMove ^ 1][Pawn])
		;
	assert(PopulationCountX(enemyCheckersBB) >= 1);
	if (PopulationCountX(enemyCheckersBB) > 1) // If in double-check then only King moves are possible
		goto king;

	// So we now know that enemyCheckersBB has only one bit set
	// N.B. If we are in check a pinned piece cannot move at all

	int checkerSquare = BitScanForwardX(enemyCheckersBB);
	uint64_t enemyCheckersBetweenSquaresBB = BetweenListBB[checkerSquare][kingSquare];

	// Pawns
	int pmo = PawnMoveOffset[sideToMove];
	uint64_t pawnsCapturesEastBB = (East((piecesBB[sideToMove][Pawn] << 8) >> (sideToMove << 4))) & enemyCheckersBB;
	if (pawnsCapturesEastBB) // At most one bit set
	{
		toSquare = BitScanForwardX(pawnsCapturesEastBB);
		if (!(UINT64SetBit(toSquare - (pmo + 1)) & gameRecordPointer->pinnedAllBB)) // Not pinned?
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
	uint64_t pawnsCapturesWestBB = (West((piecesBB[sideToMove][Pawn] << 8) >> (sideToMove << 4))) & enemyCheckersBB;
	if (pawnsCapturesWestBB)
	{
		toSquare = BitScanForwardX(pawnsCapturesWestBB);
		if (!(UINT64SetBit(toSquare - (pmo - 1)) & gameRecordPointer->pinnedAllBB)) // Not pinned?
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
	if (gameRecordPointer->epSquare)
	{
		Move_Struct previousMove;
		previousMove.ui32 = (gameRecordPointer - 1)->move.ui32;
		if (West(UINT64SetBit(previousMove.mf.toSquare)) & piecesBB[sideToMove][Pawn])
		{
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
			//if (!IsAttacked(kingSquare, sideToMove ^ 1))
				//moves++;
			canMove = !IsAttacked(kingSquare, sideToMove ^ 1);
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
			if (canMove)
				return true;
		}
		if (East(UINT64SetBit(previousMove.mf.toSquare)) & piecesBB[sideToMove][Pawn])
		{
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
			//if (!IsAttacked(kingSquare, sideToMove ^ 1))
				//moves++;
			canMove = !IsAttacked(kingSquare, sideToMove ^ 1);
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
			if (canMove)
				return true;
		}

	}
	uint64_t pawnMove1BB = ((piecesBB[sideToMove][Pawn] << 8) >> (sideToMove << 4)) & enemyCheckersBetweenSquaresBB;
	while (pawnMove1BB)
	{
		toSquare = BitScanForwardX(pawnMove1BB);
		if (!(UINT64SetBit(toSquare - pmo) & gameRecordPointer->pinnedAllBB)) // Not pinned?
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
	uint64_t pawnMove2BB = ((((((piecesBB[sideToMove][Pawn] & SecondRankBB[sideToMove]) << 8) >> (sideToMove << 4)) & notOccupiedBB) << 8) >> (sideToMove << 4)) & enemyCheckersBetweenSquaresBB;
	while (pawnMove2BB)
	{
		toSquare = BitScanForwardX(pawnMove2BB);
		if (!(UINT64SetBit(toSquare - (pmo * 2)) & gameRecordPointer->pinnedAllBB)) // Not pinned?
			//moves++;
			return true;
		ClearLS1B(pawnMove2BB);
	}

	// If we are in check then pinned pieces can't move at all as they couldn't capture the checker/interrupt the check without exposing the king

	// Knights
	uint64_t knightsBB = piecesBB[sideToMove][Knight] & ~gameRecordPointer->pinnedAllBB;//CAN DO PINNED TEST HERE AS IN COUNTCPAS&NONCAPS!
	while (knightsBB)
	{
		fromSquare = BitScanForwardX(knightsBB);
		//if (!(UINT64SetBit(fromSquare) & gameRecordPointer->pinnedAllBB)) // Not pinned?
		{
			attacksBB = KnightAttacksBBList[fromSquare] & (enemyCheckersBB | enemyCheckersBetweenSquaresBB);
			//moves += PopulationCountX(attacksBB);
			if (PopulationCountX(attacksBB))
				return true;
		}
		ClearLS1B(knightsBB);
	}

	// Bishops and queens
	uint64_t bishopsAndQueensBB = (piecesBB[sideToMove][Bishop] | piecesBB[sideToMove][Queen]) & ~gameRecordPointer->pinnedAllBB;
	while (bishopsAndQueensBB)
	{
		fromSquare = BitScanForwardX(bishopsAndQueensBB);
		//if (!(UINT64SetBit(fromSquare) & gameRecordPointer->pinnedAllBB)) // Not pinned?
		{
			attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & (enemyCheckersBB | enemyCheckersBetweenSquaresBB);
			//moves += PopulationCountX(attacksBB);
			if (PopulationCountX(attacksBB))
				return true;
		}
		ClearLS1B(bishopsAndQueensBB);
	}

	// Rooks and queens
	uint64_t rooksAndQueensBB = (piecesBB[sideToMove][Rook] | piecesBB[sideToMove][Queen]) & ~gameRecordPointer->pinnedAllBB;
	while (rooksAndQueensBB)
	{
		fromSquare = BitScanForwardX(rooksAndQueensBB);
		//if (!(UINT64SetBit(fromSquare) & gameRecordPointer->pinnedAllBB)) // Not pinned?
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
	attacksBB = KingAttacksBBList[kingSquare] & ~piecesBB[sideToMove][AllPieces];
	piecesBB[sideToMove][AllPieces] ^= piecesBB[sideToMove][King];
	canMove = false;
	while (attacksBB)
	{
		int toSquare = BitScanForwardX(attacksBB);
		if (!IsAttacked(toSquare, sideToMove ^ 1))
			//moves++;
		{
			canMove = true;
			break;
		}
		ClearLS1B(attacksBB);
	}
	piecesBB[sideToMove][AllPieces] ^= piecesBB[sideToMove][King];
	if (canMove)
		return true;

	return false;
}

MoveWithScore_Struct* Brain::GenerateNonCaptureNonPromotionDirectChecks(int sideToMove, MoveWithScore_Struct* mlp)
{
	int fromSquare, toSquare;
	uint64_t attacksBB;
	uint64_t occupiedBB = piecesBB[0][AllPieces] | piecesBB[1][AllPieces];
	int kingSquare = BitScanForwardX(piecesBB[sideToMove][King]);
	int enemyKingSquare = BitScanForwardX(piecesBB[sideToMove ^ 1][King]);
	uint64_t enemyKingPawnAttacksBB = PawnAttacksBBList[sideToMove ^ 1][enemyKingSquare];
	uint64_t enemyKingKnightAttacksBB = KnightAttacksBBList[enemyKingSquare];
	uint64_t enemyKingBishopAttacksBB = BishopAttacksBB(enemyKingSquare, occupiedBB);
	uint64_t enemyKingRookAttacksBB = RookAttacksBB(enemyKingSquare, occupiedBB);
	//uint64_t fromSquareBB, validBB;

	// Pawns
	int pmo = PawnMoveOffset[sideToMove];
	uint64_t pawnMove1BB = (((piecesBB[sideToMove][Pawn] & ~SeventhRankBB[sideToMove] & ~gameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB & enemyKingPawnAttacksBB;
	while (pawnMove1BB)
	{
		toSquare = BitScanForwardX(pawnMove1BB);
		if (
			(!(UINT64SetBit(toSquare - pmo) & gameRecordPointer->pinnedRankFileBB)) || // Not pinned?
			(UINT64SetBit(toSquare) & LineListBB[toSquare - pmo][kingSquare]) // Is the to-square on the line between the from-square and the king?
			)
			mlp++->ui32 = (toSquare - pmo) | (toSquare << 8);
		ClearLS1B(pawnMove1BB);
	}
	uint64_t pawnMove2BB = ((((((piecesBB[sideToMove][Pawn] & SecondRankBB[sideToMove] & ~gameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB) << 8) >> (sideToMove << 4)) & notOccupiedBB & enemyKingPawnAttacksBB;
	while (pawnMove2BB)
	{
		toSquare = BitScanForwardX(pawnMove2BB);
		if (
			(!(UINT64SetBit(toSquare - (pmo * 2)) & gameRecordPointer->pinnedRankFileBB)) ||
			(UINT64SetBit(toSquare) & LineListBB[toSquare - (pmo * 2)][kingSquare])
			)
			mlp++->ui32 = (toSquare - (pmo * 2)) | (toSquare << 8);
		ClearLS1B(pawnMove2BB);
	}

	// Knights
	uint64_t knightsBB = piecesBB[sideToMove][Knight] & ~gameRecordPointer->pinnedAllBB;
	while (knightsBB)
	{
		fromSquare = BitScanForwardX(knightsBB);
		//validBB = enemyKingKnightAttacksBB; // Discovered check?
		//fromSquareBB = UINT64SetBit(fromSquare);
		//if ((fromSquareBB & enemyKingRookAttacksBB) && (RookAttacksBB(enemyKingSquare, occupiedBB ^ fromSquareBB) & (PiecesBB[sideToMove][Rook] | PiecesBB[sideToMove][Queen])))
		//	validBB = -1;
		//else if ((fromSquareBB & enemyKingBishopAttacksBB) && (BishopAttacksBB(enemyKingSquare, occupiedBB ^ fromSquareBB) & (PiecesBB[sideToMove][Bishop] | PiecesBB[sideToMove][Queen])))
		//	validBB = -1;
		//attacksBB = KnightAttacksBBList[fromSquare] & notOccupiedBB & validBB;
		attacksBB = KnightAttacksBBList[fromSquare] & notOccupiedBB & enemyKingKnightAttacksBB;
		while (attacksBB)
		{
			toSquare = BitScanForwardX(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(knightsBB);
	}

	// Bishops
	uint64_t bishopsBB = piecesBB[sideToMove][Bishop];
	while (bishopsBB)
	{
		fromSquare = BitScanForwardX(bishopsBB);
		//validBB = enemyKingBishopAttacksBB; // Discovered check?
		//fromSquareBB = UINT64SetBit(fromSquare);
		//if (fromSquareBB & enemyKingRookAttacksBB)
		//	if (RookAttacksBB(enemyKingSquare, occupiedBB ^ fromSquareBB) & (PiecesBB[sideToMove][Rook] | PiecesBB[sideToMove][Queen]))
		//		validBB = -1;
		//attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & notOccupiedBB & validBB;
		attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & notOccupiedBB & enemyKingBishopAttacksBB;
		if (UINT64SetBit(fromSquare) & gameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			toSquare = BitScanForwardX(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(bishopsBB);
	}

	// Rooks
	uint64_t rooksBB = piecesBB[sideToMove][Rook];
	while (rooksBB)
	{
		fromSquare = BitScanForwardX(rooksBB);
		//validBB = enemyKingRookAttacksBB; // Discovered check?
		//fromSquareBB = UINT64SetBit(fromSquare);
		//if (fromSquareBB & enemyKingBishopAttacksBB)
		//	if (BishopAttacksBB(enemyKingSquare, occupiedBB ^ fromSquareBB) & (PiecesBB[sideToMove][Bishop] | PiecesBB[sideToMove][Queen]))
		//		validBB = -1;
		//attacksBB = RookAttacksBB(fromSquare, occupiedBB) & notOccupiedBB & validBB;
		attacksBB = RookAttacksBB(fromSquare, occupiedBB) & notOccupiedBB & enemyKingRookAttacksBB;
		if (UINT64SetBit(fromSquare) & gameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			toSquare = BitScanForwardX(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(rooksBB);
	}

	// Queens (have to be done separately from the rooks and bishops above as they can check along rank/files and diagonals)
	uint64_t queensBB = piecesBB[sideToMove][Queen];
	while (queensBB)
	{
		fromSquare = BitScanForwardX(queensBB);
		attacksBB = ((BishopAttacksBB(fromSquare, occupiedBB) | RookAttacksBB(fromSquare, occupiedBB)) & notOccupiedBB) & (enemyKingRookAttacksBB | enemyKingBishopAttacksBB);
		if (UINT64SetBit(fromSquare) & gameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			toSquare = BitScanForwardX(attacksBB);
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
	uint64_t occupiedBB = piecesBB[0][AllPieces] | piecesBB[1][AllPieces];
	int kingSquare = BitScanForwardX(piecesBB[sideToMove][King]);
	int enemyKingSquare = BitScanForwardX(piecesBB[sideToMove ^ 1][King]);
	uint64_t enemyKingPawnAttacksBB = PawnAttacksBBList[sideToMove ^ 1][enemyKingSquare];
	uint64_t enemyKingKnightAttacksBB = KnightAttacksBBList[enemyKingSquare];
	uint64_t enemyKingBishopAttacksBB = BishopAttacksBB(enemyKingSquare, occupiedBB);
	uint64_t enemyKingRookAttacksBB = RookAttacksBB(enemyKingSquare, occupiedBB);
	uint64_t fromSquareBB, toSquareBB;

	// Pawns
	int pmo = PawnMoveOffset[sideToMove];
	uint64_t pawnMove1BB = (((piecesBB[sideToMove][Pawn] & ~gameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB;
	while (pawnMove1BB)
	{
		toSquare = BitScanForwardX(pawnMove1BB);
		toSquareBB = UINT64SetBit(toSquare);
		fromSquare = toSquare - pmo;
		fromSquareBB = UINT64SetBit(fromSquare);
		if (
			(!(fromSquareBB & gameRecordPointer->pinnedRankFileBB)) || // Not pinned?
			(toSquareBB & LineListBB[fromSquare][kingSquare])
			)
			if (
				(fromSquareBB & gameRecordPointer->discoverersAllBB) && ((toSquareBB & LineListBB[fromSquare][enemyKingSquare]) == 0) // Discovered check?
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
	uint64_t pawnMove2BB = ((((((piecesBB[sideToMove][Pawn] & SecondRankBB[sideToMove] & ~gameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB) << 8) >> (sideToMove << 4)) & notOccupiedBB;
	while (pawnMove2BB)
	{
		toSquare = BitScanForwardX(pawnMove2BB);
		toSquareBB = UINT64SetBit(toSquare);
		fromSquare = toSquare - pmo * 2;
		fromSquareBB = UINT64SetBit(fromSquare);
		if (
			(!(fromSquareBB & gameRecordPointer->pinnedRankFileBB)) ||
			(toSquareBB & LineListBB[fromSquare][kingSquare])
			)
			if (
				(fromSquareBB & gameRecordPointer->discoverersAllBB) && ((toSquareBB & LineListBB[fromSquare][enemyKingSquare]) == 0) // Discovered check?
				|| (toSquareBB & enemyKingPawnAttacksBB) // Direct check?
				)
				mlp++->ui32 = (toSquare - pmo * 2) | (toSquare << 8);
		ClearLS1B(pawnMove2BB);
	}
	uint64_t pawnsCapturesEastBB = (East(((piecesBB[sideToMove][Pawn] & ~gameRecordPointer->pinnedRankFileBB) << 8) >> (sideToMove << 4))) & piecesBB[sideToMove ^ 1][AllPieces];
	while (pawnsCapturesEastBB)
	{
		toSquare = BitScanForwardX(pawnsCapturesEastBB);
		toSquareBB = UINT64SetBit(toSquare);
		fromSquare = toSquare - (pmo + 1);
		fromSquareBB = UINT64SetBit(fromSquare);
		if (
			(!(fromSquareBB & gameRecordPointer->pinnedDiagonalBB)) || // Not pinned?
			(toSquareBB & LineListBB[fromSquare][kingSquare])
			)
			if (
				(fromSquareBB & gameRecordPointer->discoverersAllBB) && ((toSquareBB & LineListBB[fromSquare][enemyKingSquare]) == 0) // Discovered check?
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
	uint64_t pawnsCapturesWestBB = (West(((piecesBB[sideToMove][Pawn] & ~gameRecordPointer->pinnedRankFileBB) << 8) >> (sideToMove << 4))) & piecesBB[sideToMove ^ 1][AllPieces];
	while (pawnsCapturesWestBB)
	{
		toSquare = BitScanForwardX(pawnsCapturesWestBB);
		toSquareBB = UINT64SetBit(toSquare);
		fromSquare = toSquare - (pmo - 1);
		fromSquareBB = UINT64SetBit(fromSquare);
		if (
			(!(fromSquareBB & gameRecordPointer->pinnedDiagonalBB)) || // Not pinned?
			(toSquareBB & LineListBB[fromSquare][kingSquare])
			)
			if (
				(fromSquareBB & gameRecordPointer->discoverersAllBB) && ((toSquareBB & LineListBB[fromSquare][enemyKingSquare]) == 0) // Discovered check?
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
	if (gameRecordPointer->epSquare)
	{
		Move_Struct previousMove;
		previousMove.ui32 = (gameRecordPointer - 1)->move.ui32;
		if (West(UINT64SetBit(previousMove.mf.toSquare)) & piecesBB[sideToMove][Pawn])
		{
			piecesBB[sideToMove][Pawn] ^= (UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare));
			piecesBB[sideToMove][AllPieces] ^= (UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare));
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
			if (!IsAttacked(kingSquare, sideToMove ^ 1))
				if (IsAttacked(enemyKingSquare, sideToMove))
					mlp++->ui32 = (previousMove.mf.toSquare - 1) | ((previousMove.mf.toSquare) << 8) | (MFEnPassant << 16);
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
		}
		if (East(UINT64SetBit(previousMove.mf.toSquare)) & piecesBB[sideToMove][Pawn])
		{
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
			if (!IsAttacked(kingSquare, sideToMove ^ 1))
				if (IsAttacked(enemyKingSquare, sideToMove))
					mlp++->ui32 = (previousMove.mf.toSquare + 1) | ((previousMove.mf.toSquare) << 8) | (MFEnPassant << 16);
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(gameRecordPointer->epSquare);
			piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
		}
	}

	// Knights (can give discovered checks along ranks/files and diagonals)
	uint64_t knightsBB = piecesBB[sideToMove][Knight] & ~gameRecordPointer->pinnedAllBB;
	while (knightsBB)
	{
		fromSquare = BitScanForwardX(knightsBB);
		fromSquareBB = UINT64SetBit(fromSquare);
		if (fromSquareBB & gameRecordPointer->discoverersAllBB)
			attacksBB = KnightAttacksBBList[fromSquare] & ~piecesBB[sideToMove][AllPieces]; // All moves will give discovered checks
		else
			attacksBB = KnightAttacksBBList[fromSquare] & ~piecesBB[sideToMove][AllPieces] & enemyKingKnightAttacksBB; // Some moves may directly give check
		while (attacksBB)
		{
			toSquare = BitScanForwardX(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(knightsBB);
	}

	// Bishops (can give discovered checks along ranks/files)
	uint64_t bishopsBB = piecesBB[sideToMove][Bishop];
	while (bishopsBB)
	{
		fromSquare = BitScanForwardX(bishopsBB);
		fromSquareBB = UINT64SetBit(fromSquare);
		if (fromSquareBB & gameRecordPointer->discoverersAllBB)
			attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & ~piecesBB[sideToMove][AllPieces];
		else
			attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & ~piecesBB[sideToMove][AllPieces] & enemyKingBishopAttacksBB;
		if (fromSquareBB & gameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			toSquare = BitScanForwardX(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(bishopsBB);
	}

	// Rooks (can give discovered checks along diagonals)
	uint64_t rooksBB = piecesBB[sideToMove][Rook];
	while (rooksBB)
	{
		fromSquare = BitScanForwardX(rooksBB);
		fromSquareBB = UINT64SetBit(fromSquare);
		if (fromSquareBB & gameRecordPointer->discoverersAllBB)
			attacksBB = RookAttacksBB(fromSquare, occupiedBB) & ~piecesBB[sideToMove][AllPieces];
		else
			attacksBB = RookAttacksBB(fromSquare, occupiedBB) & ~piecesBB[sideToMove][AllPieces] & enemyKingRookAttacksBB;
		if (fromSquareBB & gameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			toSquare = BitScanForwardX(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(rooksBB);
	}

	// Queens (cannot give discovered checks as they would already be checking)
	uint64_t queensBB = piecesBB[sideToMove][Queen];
	while (queensBB)
	{
		fromSquare = BitScanForwardX(queensBB);
		fromSquareBB = UINT64SetBit(fromSquare);
		//if (fromSquareBB & gameRecordPointer->discoverersAllBB)
		//	attacksBB = ((BishopAttacksBB(fromSquare, occupiedBB) | RookAttacksBB(fromSquare, occupiedBB)) & ~piecesBB[sideToMove][AllPieces]);
		//else
			attacksBB = ((BishopAttacksBB(fromSquare, occupiedBB) | RookAttacksBB(fromSquare, occupiedBB)) & ~piecesBB[sideToMove][AllPieces]) & (enemyKingRookAttacksBB | enemyKingBishopAttacksBB);
		if (UINT64SetBit(fromSquare) & gameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			toSquare = BitScanForwardX(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(queensBB);
	}

	// King (can give discovered checks along ranks/files and diagonals but not along the ray to/fro the enemy king)
	fromSquareBB = UINT64SetBit(kingSquare);//SURELY THIS IS piecesBB[sideToMove][King] ?????
	if (fromSquareBB & gameRecordPointer->discoverersAllBB)
	{
		attacksBB = KingAttacksBBList[kingSquare] & ~piecesBB[sideToMove][AllPieces] & ~LineListBB[enemyKingSquare][kingSquare];
		while (attacksBB)
		{
			int toSquare = BitScanForwardX(attacksBB);
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
		if (gameRecordPointer->castlingStatus.ui8[sideToMove][0] == 0)
			if ((mailboxBoard64[kingSquare + 2] | mailboxBoard64[kingSquare + 1]) == Empty)
				if (!IsAttacked(kingSquare + 1, sideToMove ^ 1) && !IsAttacked(kingSquare + 2, sideToMove ^ 1))
					if (BetweenListBB[enemyKingSquare][BackRankBaseSquareIndex[sideToMove] + F] != 0)
						if ((BetweenListBB[enemyKingSquare][BackRankBaseSquareIndex[sideToMove] + F] & ~(occupiedBB & ~piecesBB[sideToMove][King]) & (FirstRankBB[sideToMove] | FileFBB)) == BetweenListBB[enemyKingSquare][BackRankBaseSquareIndex[sideToMove] + F]) // Are all the squares between the enemy king and the rook empty?
							mlp++->ui32 = kingSquare | ((kingSquare + 2) << 8) | (MFCastling << 16);

		// Queen side
		if (gameRecordPointer->castlingStatus.ui8[sideToMove][1] == 0)
			if ((mailboxBoard64[kingSquare - 3] | mailboxBoard64[kingSquare - 2] | mailboxBoard64[kingSquare - 1]) == Empty)
				if (!IsAttacked(kingSquare - 1, sideToMove ^ 1) && !IsAttacked(kingSquare - 2, sideToMove ^ 1))
					if (BetweenListBB[enemyKingSquare][BackRankBaseSquareIndex[sideToMove] + D] != 0)
						if ((BetweenListBB[enemyKingSquare][BackRankBaseSquareIndex[sideToMove] + D] & ~(occupiedBB & ~piecesBB[sideToMove][King]) & (FirstRankBB[sideToMove] | FileDBB)) == BetweenListBB[enemyKingSquare][BackRankBaseSquareIndex[sideToMove] + D]) // Are all the squares between the enemy king and the rook empty?
							mlp++->ui32 = kingSquare | ((kingSquare - 2) << 8) | (MFCastling << 16);
	}

	return mlp;
}

MoveWithScore_Struct* Brain::GenerateAllNonCaptureNonPromotionChecks(int sideToMove, MoveWithScore_Struct* mlp)
{
	int fromSquare, toSquare;
	uint64_t attacksBB;
	uint64_t occupiedBB = piecesBB[0][AllPieces] | piecesBB[1][AllPieces];
	int kingSquare = BitScanForwardX(piecesBB[sideToMove][King]);
	int enemyKingSquare = BitScanForwardX(piecesBB[sideToMove ^ 1][King]);
	uint64_t enemyKingPawnAttacksBB = PawnAttacksBBList[sideToMove ^ 1][enemyKingSquare];
	uint64_t enemyKingKnightAttacksBB = KnightAttacksBBList[enemyKingSquare];
	uint64_t enemyKingBishopAttacksBB = BishopAttacksBB(enemyKingSquare, occupiedBB);
	uint64_t enemyKingRookAttacksBB = RookAttacksBB(enemyKingSquare, occupiedBB);
	uint64_t fromSquareBB, toSquareBB;

	// Pawns
	int pmo = PawnMoveOffset[sideToMove];
	uint64_t pawnMove1BB = (((piecesBB[sideToMove][Pawn] & ~SeventhRankBB[sideToMove] & ~gameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB;
	while (pawnMove1BB)
	{
		toSquare = BitScanForwardX(pawnMove1BB);
		toSquareBB = UINT64SetBit(toSquare);
		fromSquare = toSquare - pmo;
		fromSquareBB = UINT64SetBit(fromSquare);
		if (
			(!(fromSquareBB & gameRecordPointer->pinnedRankFileBB)) || // Not pinned?
			(toSquareBB & LineListBB[fromSquare][kingSquare])
			)
			if (
				(fromSquareBB & gameRecordPointer->discoverersAllBB) && ((toSquareBB & LineListBB[fromSquare][enemyKingSquare]) == 0) // Discovered check?
				|| (toSquareBB & enemyKingPawnAttacksBB) // Direct check?
				)
				mlp++->ui32 = fromSquare | (toSquare << 8);
		ClearLS1B(pawnMove1BB);
	}
	uint64_t pawnMove2BB = ((((((piecesBB[sideToMove][Pawn] & SecondRankBB[sideToMove] & ~gameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB) << 8) >> (sideToMove << 4)) & notOccupiedBB;
	while (pawnMove2BB)
	{
		toSquare = BitScanForwardX(pawnMove2BB);
		toSquareBB = UINT64SetBit(toSquare);
		fromSquare = toSquare - pmo * 2;
		fromSquareBB = UINT64SetBit(fromSquare);
		if (
			(!(fromSquareBB & gameRecordPointer->pinnedRankFileBB)) ||
			(toSquareBB & LineListBB[fromSquare][kingSquare])
			)
			if (
				(fromSquareBB & gameRecordPointer->discoverersAllBB) && ((toSquareBB & LineListBB[fromSquare][enemyKingSquare]) == 0) // Discovered check?
				|| (toSquareBB & enemyKingPawnAttacksBB) // Direct check?
				)
				mlp++->ui32 = fromSquare | (toSquare << 8);
		ClearLS1B(pawnMove2BB);
	}

	// Knights (can give discovered checks along ranks/files and diagonals)
	uint64_t knightsBB = piecesBB[sideToMove][Knight] & ~gameRecordPointer->pinnedAllBB;
	while (knightsBB)
	{
		fromSquare = BitScanForwardX(knightsBB);
		fromSquareBB = UINT64SetBit(fromSquare);
		if (fromSquareBB & gameRecordPointer->discoverersAllBB)
			attacksBB = KnightAttacksBBList[fromSquare] & notOccupiedBB; // All moves will give discovered checks
		else
			attacksBB = KnightAttacksBBList[fromSquare] & notOccupiedBB & enemyKingKnightAttacksBB; // Some moves may directly give check
		while (attacksBB)
		{
			toSquare = BitScanForwardX(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(knightsBB);
	}

	// Bishops (can give discovered checks along ranks/files)
	uint64_t bishopsBB = piecesBB[sideToMove][Bishop];
	while (bishopsBB)
	{
		fromSquare = BitScanForwardX(bishopsBB);
		fromSquareBB = UINT64SetBit(fromSquare);
		if (fromSquareBB & gameRecordPointer->discoverersAllBB)
			attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & notOccupiedBB;
		else
			attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & notOccupiedBB & enemyKingBishopAttacksBB;
		if (fromSquareBB & gameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			toSquare = BitScanForwardX(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(bishopsBB);
	}

	// Rooks (can give discovered checks along diagonals)
	uint64_t rooksBB = piecesBB[sideToMove][Rook];
	while (rooksBB)
	{
		fromSquare = BitScanForwardX(rooksBB);
		fromSquareBB = UINT64SetBit(fromSquare);
		if (fromSquareBB & gameRecordPointer->discoverersAllBB)
			attacksBB = RookAttacksBB(fromSquare, occupiedBB) & notOccupiedBB;
		else
			attacksBB = RookAttacksBB(fromSquare, occupiedBB) & notOccupiedBB & enemyKingRookAttacksBB;
		if (fromSquareBB & gameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			toSquare = BitScanForwardX(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(rooksBB);
	}

	// Queens (cannot give discovered checks as they would already be checking)
	uint64_t queensBB = piecesBB[sideToMove][Queen];
	while (queensBB)
	{
		fromSquare = BitScanForwardX(queensBB);
		fromSquareBB = UINT64SetBit(fromSquare);
		//if (fromSquareBB & gameRecordPointer->discoverersAllBB)
		//	attacksBB = ((BishopAttacksBB(fromSquare, occupiedBB) | RookAttacksBB(fromSquare, occupiedBB)) & ~piecesBB[sideToMove][AllPieces]);
		//else
		attacksBB = ((BishopAttacksBB(fromSquare, occupiedBB) | RookAttacksBB(fromSquare, occupiedBB)) & notOccupiedBB) & (enemyKingRookAttacksBB | enemyKingBishopAttacksBB);
		if (UINT64SetBit(fromSquare) & gameRecordPointer->pinnedAllBB) // Pinned?
			attacksBB &= LineListBB[fromSquare][kingSquare]; // If it's pinned then it can only move along the ray to/fro the king
		while (attacksBB)
		{
			toSquare = BitScanForwardX(attacksBB);
			mlp++->ui32 = fromSquare | (toSquare << 8);
			ClearLS1B(attacksBB);
		}
		ClearLS1B(queensBB);
	}

	// King (can give discovered checks along ranks/files and diagonals but not along the ray to/fro the enemy king)
	fromSquareBB = UINT64SetBit(kingSquare);//SURELY THIS IS piecesBB[sideToMove][King] ?????
	if (fromSquareBB & gameRecordPointer->discoverersAllBB)
	{
		attacksBB = KingAttacksBBList[kingSquare] & notOccupiedBB & ~LineListBB[enemyKingSquare][kingSquare];
		while (attacksBB)
		{
			int toSquare = BitScanForwardX(attacksBB);
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
		if (gameRecordPointer->castlingStatus.ui8[sideToMove][0] == 0)
			if ((mailboxBoard64[kingSquare + 2] | mailboxBoard64[kingSquare + 1]) == Empty)
				if (!IsAttacked(kingSquare + 1, sideToMove ^ 1) && !IsAttacked(kingSquare + 2, sideToMove ^ 1))
					if (BetweenListBB[enemyKingSquare][BackRankBaseSquareIndex[sideToMove] + F] != 0)
						if ((BetweenListBB[enemyKingSquare][BackRankBaseSquareIndex[sideToMove] + F] & ~(occupiedBB & ~piecesBB[sideToMove][King]) & (FirstRankBB[sideToMove] | FileFBB)) == BetweenListBB[enemyKingSquare][BackRankBaseSquareIndex[sideToMove] + F]) // Are all the squares between the enemy king and the rook empty?
							mlp++->ui32 = kingSquare | ((kingSquare + 2) << 8) | (MFCastling << 16);

		// Queen side
		if (gameRecordPointer->castlingStatus.ui8[sideToMove][1] == 0)
			if ((mailboxBoard64[kingSquare - 3] | mailboxBoard64[kingSquare - 2] | mailboxBoard64[kingSquare - 1]) == Empty)
				if (!IsAttacked(kingSquare - 1, sideToMove ^ 1) && !IsAttacked(kingSquare - 2, sideToMove ^ 1))
					if (BetweenListBB[enemyKingSquare][BackRankBaseSquareIndex[sideToMove] + D] != 0)
						if ((BetweenListBB[enemyKingSquare][BackRankBaseSquareIndex[sideToMove] + D] & ~(occupiedBB & ~piecesBB[sideToMove][King]) & (FirstRankBB[sideToMove] | FileDBB)) == BetweenListBB[enemyKingSquare][BackRankBaseSquareIndex[sideToMove] + D]) // Are all the squares between the enemy king and the rook empty?
							mlp++->ui32 = kingSquare | ((kingSquare - 2) << 8) | (MFCastling << 16);
	}

	return mlp;
}

uint32_t Brain::CountKingMoves(int sideToMove)
{
	int fromSquare;
	uint64_t attacksBB;
	uint32_t moves = 0;

	fromSquare = BitScanForwardX(piecesBB[sideToMove][King]);
	attacksBB = KingAttacksBBList[fromSquare] & ~piecesBB[sideToMove][AllPieces];
	while (attacksBB)
	{
		int toSquare = BitScanForwardX(attacksBB);
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
	int kingSquare = BitScanForwardX(piecesBB[sideToMove][King]);
	uint64_t pinnersBB;

	gameRecordPointer->pinnedRankFileBB = 0;
	pinnersBB = RankFilePinnersBB(kingSquare, sideToMove);
	//gameRecordPointer->pinnersRankFileBB = pinnersBB;
	while (pinnersBB)
	{
		int pinnerSquare = BitScanForwardX(pinnersBB);
		gameRecordPointer->pinnedRankFileBB |= piecesBB[sideToMove][AllPieces] & BetweenListBB[pinnerSquare][kingSquare];
		ClearLS1B(pinnersBB);
	}

	gameRecordPointer->pinnedDiagonalBB = 0;
	pinnersBB = DiagonalPinnersBB(kingSquare, sideToMove);
	//gameRecordPointer->pinnersDiagonalBB = pinnersBB;
	while (pinnersBB)
	{
		int pinnerSquare = BitScanForwardX(pinnersBB);
		gameRecordPointer->pinnedDiagonalBB |= piecesBB[sideToMove][AllPieces] & BetweenListBB[pinnerSquare][kingSquare];
		ClearLS1B(pinnersBB);
	}

	gameRecordPointer->pinnedAllBB = gameRecordPointer->pinnedRankFileBB | gameRecordPointer->pinnedDiagonalBB;
}

void Brain::CalculateDiscovererPieces(int sideToMove)
{
	// Find pieces of the side to move that are hiding a discovered check to the enemy king
	int enemyKingSquare = BitScanForwardX(piecesBB[sideToMove ^ 1][King]);
	uint64_t discovereesBB;

	gameRecordPointer->discoverersRankFileBB = 0;
	discovereesBB = RankFileDiscovereesBB(enemyKingSquare, sideToMove);
	while (discovereesBB)
	{
		int discovereeSquare = BitScanForwardX(discovereesBB);
		gameRecordPointer->discoverersRankFileBB |= piecesBB[sideToMove][AllPieces] & BetweenListBB[discovereeSquare][enemyKingSquare];
		ClearLS1B(discovereesBB);
	}

	gameRecordPointer->discoverersDiagonalBB = 0;
	discovereesBB = DiagonalDiscovereesBB(enemyKingSquare, sideToMove);
	while (discovereesBB)
	{
		int discovereeSquare = BitScanForwardX(discovereesBB);
		gameRecordPointer->discoverersDiagonalBB |= piecesBB[sideToMove][AllPieces] & BetweenListBB[discovereeSquare][enemyKingSquare];
		ClearLS1B(discovereesBB);
	}

	gameRecordPointer->discoverersAllBB = gameRecordPointer->discoverersRankFileBB | gameRecordPointer->discoverersDiagonalBB;
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
		//			(!(UINT64SetBit(toSquare - pmo) & gameRecordPointer->pinnedRankFileBB)) || // Not pinned?
		//			(UINT64SetBit(toSquare) & LineListBB[toSquare - pmo][kingSquare]) // Is the to-square on the line between the from-square and the king?
		//			)
		//			MLP++->i32 = (toSquare - pmo) | (toSquare << 8);
		//		ClearLS1B(pawnMove1BB);
		//	}
		//	uint64_t pawnMove2BB = ((((((passedBB & SecondRankBB[sideToMove] & ~gameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB) << 8) >> (sideToMove << 4)) & notOccupiedBB;
		//	while (pawnMove2BB)
		//	{
		//		toSquare = BitScanForwardX(pawnMove2BB);
		//		if (
		//			(!(UINT64SetBit(toSquare - (pmo * 2)) & gameRecordPointer->pinnedRankFileBB)) ||
		//			(UINT64SetBit(toSquare) & LineListBB[toSquare - (pmo * 2)][kingSquare])
		//			)
		//			MLP++->i32 = (toSquare - pmo * 2) | (toSquare << 8);
		//		ClearLS1B(pawnMove2BB);
		//	}
		//}

		return (uint32_t)(MLP - initialMLP);
	}

	return (uint32_t)(GenerateAllMovesOutOfCheck(sideToMove, initialMLP, false) - initialMLP);
}

uint64_t Brain::CountAllMoves(int sideToMove, int isInCheck)
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
	uint64_t occupiedBB = piecesBB[0][AllPieces] | piecesBB[1][AllPieces];
	int moves = 0;

	// Queens
	uint64_t queensBB = piecesBB[sideToMove][Queen];
	while (queensBB)
	{
		fromSquare = BitScanForwardX(queensBB);
		attacksBB = RookAttacksBB(fromSquare, occupiedBB) & ~piecesBB[sideToMove][AllPieces];
		moves += PopulationCountX(attacksBB);
		attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & ~piecesBB[sideToMove][AllPieces];
		moves += PopulationCountX(attacksBB);
		ClearLS1B(queensBB);
	}

	return moves;
}

int Brain::CountAllMovesMM(int sideToMove)
{
	int fromSquare;
	uint64_t attacksBB;
	uint64_t occupiedBB = piecesBB[0][AllPieces] | piecesBB[1][AllPieces];
	int moves = 0;

	// Rooks and queens
	// The maximum number of 'rook' moves that 9 queens and 2 rooks can contribute is 11*14=154
	uint64_t rooksAndQueensBB = piecesBB[sideToMove][Rook] | piecesBB[sideToMove][Queen];
	while (rooksAndQueensBB)
	{
		fromSquare = BitScanForwardX(rooksAndQueensBB);
		attacksBB = RookAttacksBB(fromSquare, occupiedBB) & ~piecesBB[sideToMove][AllPieces];
		moves += PopulationCountX(attacksBB);
		ClearLS1B(rooksAndQueensBB);
	}

	if (moves < 218 - 143 - 16 - 8)
		return 0;

	// Bishops and queens
	// The maximum number of 'bishop' moves that 9 queens and 2 bishops can contribute is 11*13=143
	uint64_t bishopsAndQueensBB = piecesBB[sideToMove][Bishop] | piecesBB[sideToMove][Queen];
	while (bishopsAndQueensBB)
	{
		fromSquare = BitScanForwardX(bishopsAndQueensBB);
		attacksBB = BishopAttacksBB(fromSquare, occupiedBB) & ~piecesBB[sideToMove][AllPieces];
		moves += PopulationCountX(attacksBB);
		ClearLS1B(bishopsAndQueensBB);
	}

	if (moves < 218 - 16 - 8)
		return 0;

	// Knights
	// The maximum number of moves that 2 knights can contribute is 16
	uint64_t knightsBB = piecesBB[sideToMove][Knight];
	while (knightsBB)
	{
		fromSquare = BitScanForwardX(knightsBB);
		attacksBB = KnightAttacksBBList[fromSquare] & ~piecesBB[sideToMove][AllPieces];
		moves += PopulationCountX(attacksBB);
		ClearLS1B(knightsBB);
	}

	if (moves < 218 - 8)
		return 0;

	// King
	// The maximum number of moves that the king can contribute is 8
	int kingSquare = BitScanForwardX(piecesBB[sideToMove][King]);
	uint64_t restrictedBB = UINT64SetBit(H8) | UINT64SetBit(G8) | UINT64SetBit(H7) | UINT64SetBit(G7);
	attacksBB = KingAttacksBBList[kingSquare] & ~piecesBB[sideToMove][AllPieces] & ~restrictedBB;
	moves += PopulationCountX(attacksBB);

	//// Castling
	//if (kingSquare == E1)
	//{
	//	if ((PiecesBB[sideToMove][Rook] & UINT64SetBit(H1)) && ((PiecesBB[sideToMove][AllPieces] & (UINT64SetBit(F1) | UINT64SetBit(G1))) == 0))
	//		moves++;
	//	if ((PiecesBB[sideToMove][Rook] & UINT64SetBit(A1)) && ((PiecesBB[sideToMove][AllPieces] & (UINT64SetBit(D1) | UINT64SetBit(C1) | UINT64SetBit(B1))) == 0))
	//		moves++;
	//}

	return moves;
}

//----------------------------------------------------------------------------------------------------

void Brain::MakeMove(int sideToMove)
{
	MoveUndo_Struct* currentMove = &gameRecordPointer->move;

	assert((currentMove->mf.fromSquare >= A1) && (currentMove->mf.fromSquare <= H8) && (currentMove->mf.toSquare >= A1) && (currentMove->mf.toSquare <= H8)); // Can't assert fromSquare!=toSquare because of FRC castling! Also can't assert that the toSquare is empty or contains opponent's piece
	assert(currentMove->mf.flag <= 15);
	assert(currentMove->ui32 != 0);
	assert(mailboxBoard64[currentMove->mf.fromSquare] != Empty);

	uint64_t hash64 = gameRecordPointer->transpositionTableHash64;
	(gameRecordPointer + 1)->castlingStatus = gameRecordPointer->castlingStatus;
	*(uint32_t*)(&(gameRecordPointer + 1)->totalMaterial[0]) = *(uint32_t*)(&gameRecordPointer->totalMaterial[0]);
	*(uint64_t*)(&(gameRecordPointer + 1)->gamePhase[0]) = *(uint64_t*)(&gameRecordPointer->gamePhase[0]);
	*(uint32_t*)(&(gameRecordPointer + 1)->totalOpeningPST[0]) = *(uint32_t*)(&gameRecordPointer->totalOpeningPST[0]);
	*(uint32_t*)(&(gameRecordPointer + 1)->totalEndgamePST[0]) = *(uint32_t*)(&gameRecordPointer->totalEndgamePST[0]);

	gameRecordPointer++;

	// Save pieces
	currentMove->fromSquarePiece = mailboxBoard64[currentMove->mf.fromSquare];
	currentMove->toSquarePiece = mailboxBoard64[currentMove->mf.toSquare];
	if (currentMove->toSquarePiece)
		if ((currentMove->fromSquarePiece > 0) == (currentMove->toSquarePiece > 0)) // Chess960 castling where K stays on same square or takes own rook?
			currentMove->toSquarePiece = Empty;

	currentMove->fromToXor = UINT64SetBit(currentMove->mf.fromSquare) ^ UINT64SetBit(currentMove->mf.toSquare);
	piecesBB[sideToMove][abs(currentMove->fromSquarePiece)] ^= currentMove->fromToXor;
	piecesBB[sideToMove][AllPieces] ^= currentMove->fromToXor;

	// Update the mailbox board
	mailboxBoard64[currentMove->mf.fromSquare] = Empty; // N.B. must update the from-square BEFORE the to-square for Chess960 castling as they might be the same square!
	mailboxBoard64[currentMove->mf.toSquare] = currentMove->fromSquarePiece;
	hash64 ^= TranspositionTableRandoms[sideToMove][abs(currentMove->fromSquarePiece)][currentMove->mf.toSquare] ^ TranspositionTableRandoms[sideToMove][abs(currentMove->fromSquarePiece)][currentMove->mf.fromSquare];

	int pstXOR = (sideToMove ? 0 : 56);
	gameRecordPointer->totalOpeningPST[sideToMove] += OpeningPSTs[abs(currentMove->fromSquarePiece) - 1][currentMove->mf.toSquare ^ pstXOR] - OpeningPSTs[abs(currentMove->fromSquarePiece) - 1][currentMove->mf.fromSquare ^ pstXOR];
	gameRecordPointer->totalEndgamePST[sideToMove] += EndgamePSTs[abs(currentMove->fromSquarePiece) - 1][currentMove->mf.toSquare ^ pstXOR] - EndgamePSTs[abs(currentMove->fromSquarePiece) - 1][currentMove->mf.fromSquare ^ pstXOR];


	// Piece specific updates
	gameRecordPointer->epSquare = 0;
	switch (abs(currentMove->fromSquarePiece))
	{
	case Pawn:
		// Pawn promotion?
		if (currentMove->mf.flag >= MFPromotion)
		{
			int8_t promotionPiece;
			promotionPiece = PromotedPieces[currentMove->mf.flag >> 2];

			// Update the mailbox board, bitboards and transposition table hash
			mailboxBoard64[currentMove->mf.toSquare] = (sideToMove ? -promotionPiece : promotionPiece);
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(currentMove->mf.toSquare);
			piecesBB[sideToMove][promotionPiece] ^= UINT64SetBit(currentMove->mf.toSquare);
			hash64 ^= TranspositionTableRandoms[sideToMove][Pawn][currentMove->mf.toSquare];
			hash64 ^= TranspositionTableRandoms[sideToMove][promotionPiece][currentMove->mf.toSquare];

			// Update the material etc
			gameRecordPointer->totalMaterial[sideToMove] += MaterialValue[promotionPiece] - MVPawn;
			gameRecordPointer->gamePhase[sideToMove] += GamePhaseIncrement[promotionPiece];// -GamePhaseIncrement[Pawn];
			gameRecordPointer->totalOpeningPST[sideToMove] += OpeningPSTs[promotionPiece - 1][currentMove->mf.toSquare ^ pstXOR] - OpeningPSTs[Pawn - 1][currentMove->mf.toSquare ^ pstXOR];
			gameRecordPointer->totalEndgamePST[sideToMove] += EndgamePSTs[promotionPiece - 1][currentMove->mf.toSquare ^ pstXOR] - EndgamePSTs[Pawn - 1][currentMove->mf.toSquare ^ pstXOR];
		}
		else if (
			(abs(currentMove->mf.toSquare - currentMove->mf.fromSquare) == 16)
			&& ((West(UINT64SetBit(currentMove->mf.toSquare)) | East(UINT64SetBit(currentMove->mf.toSquare))) & piecesBB[sideToMove ^ 1][Pawn])
			)
			gameRecordPointer->epSquare = currentMove->mf.toSquare + PawnMoveOffset[sideToMove ^ 1];

		break;
	case Rook:
		// Update castling statuses
		if (currentMove->mf.fromSquare == BackRankBaseSquareIndex[sideToMove] + InitialKingSideRookFile)
		{
			if (gameRecordPointer->castlingStatus.ui8[sideToMove][0] == 0)
				hash64 ^= TranspositionTableRandomKingSideCastling[sideToMove];
			gameRecordPointer->castlingStatus.ui8[sideToMove][0] = 1;
		}
		else if (currentMove->mf.fromSquare == BackRankBaseSquareIndex[sideToMove] + InitialQueenSideRookFile)
		{
			if (gameRecordPointer->castlingStatus.ui8[sideToMove][1] == 0)
				hash64 ^= TranspositionTableRandomQueenSideCastling[sideToMove];
			gameRecordPointer->castlingStatus.ui8[sideToMove][1] = 1;
		}
		break;
	case King:
		// Update castling statuses
		if (gameRecordPointer->castlingStatus.ui8[sideToMove][0] == 0)
			hash64 ^= TranspositionTableRandomKingSideCastling[sideToMove];
		gameRecordPointer->castlingStatus.ui8[sideToMove][0] = 1;
		if (gameRecordPointer->castlingStatus.ui8[sideToMove][1] == 0)
			hash64 ^= TranspositionTableRandomQueenSideCastling[sideToMove];
		gameRecordPointer->castlingStatus.ui8[sideToMove][1] = 1;
		// Was it a castling move?
		if (currentMove->mf.flag == MFCastling)
		{
			if (UCI_Chess960)
			{
				if (currentMove->mf.toSquare == BackRankBaseSquareIndex[sideToMove] + G) // King-side?
				{
					if (abs(mailboxBoard64[BackRankBaseSquareIndex[sideToMove] + InitialKingSideRookFile]) != King)
						mailboxBoard64[BackRankBaseSquareIndex[sideToMove] + InitialKingSideRookFile] = Empty;
					mailboxBoard64[BackRankBaseSquareIndex[sideToMove] + F] = (sideToMove ? -Rook : Rook);
					piecesBB[sideToMove][Rook] ^= UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + InitialKingSideRookFile) ^ UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + F);
					piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + InitialKingSideRookFile) ^ UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + F);
					hash64 ^= TranspositionTableRandoms[sideToMove][Rook][BackRankBaseSquareIndex[sideToMove] + F] ^ TranspositionTableRandoms[sideToMove][Rook][BackRankBaseSquareIndex[sideToMove] + InitialKingSideRookFile];
					gameRecordPointer->totalOpeningPST[sideToMove] += OpeningPSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + F) ^ pstXOR] - OpeningPSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + InitialKingSideRookFile) ^ pstXOR];
					gameRecordPointer->totalEndgamePST[sideToMove] += EndgamePSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + F) ^ pstXOR] - EndgamePSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + InitialKingSideRookFile) ^ pstXOR];
				}
				else
				{
					if (abs(mailboxBoard64[BackRankBaseSquareIndex[sideToMove] + InitialQueenSideRookFile]) != King)
						mailboxBoard64[BackRankBaseSquareIndex[sideToMove] + InitialQueenSideRookFile] = Empty;
					mailboxBoard64[BackRankBaseSquareIndex[sideToMove] + D] = (sideToMove ? -Rook : Rook);
					piecesBB[sideToMove][Rook] ^= UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + InitialQueenSideRookFile) ^ UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + D);
					piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + InitialQueenSideRookFile) ^ UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + D);
					hash64 ^= TranspositionTableRandoms[sideToMove][Rook][BackRankBaseSquareIndex[sideToMove] + D] ^ TranspositionTableRandoms[sideToMove][Rook][BackRankBaseSquareIndex[sideToMove] + InitialQueenSideRookFile];
					gameRecordPointer->totalOpeningPST[sideToMove] += OpeningPSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + D) ^ pstXOR] - OpeningPSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + InitialQueenSideRookFile) ^ pstXOR];
					gameRecordPointer->totalEndgamePST[sideToMove] += EndgamePSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + D) ^ pstXOR] - EndgamePSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + InitialQueenSideRookFile) ^ pstXOR];
				}
			}
			else
			{
				if (currentMove->mf.toSquare == BackRankBaseSquareIndex[sideToMove] + G) // King-side?
				{
					mailboxBoard64[BackRankBaseSquareIndex[sideToMove] + F] = (sideToMove ? -Rook : Rook);
					mailboxBoard64[BackRankBaseSquareIndex[sideToMove] + H] = Empty;
					piecesBB[sideToMove][Rook] ^= UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + H) ^ UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + F);
					piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + H) ^ UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + F);
					hash64 ^= TranspositionTableRandoms[sideToMove][Rook][BackRankBaseSquareIndex[sideToMove] + F] ^ TranspositionTableRandoms[sideToMove][Rook][BackRankBaseSquareIndex[sideToMove] + H];
					gameRecordPointer->totalOpeningPST[sideToMove] += OpeningPSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + F) ^ pstXOR] - OpeningPSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + H) ^ pstXOR];
					gameRecordPointer->totalEndgamePST[sideToMove] += EndgamePSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + F) ^ pstXOR] - EndgamePSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + H) ^ pstXOR];
				}
				else
				{
					mailboxBoard64[BackRankBaseSquareIndex[sideToMove] + D] = (sideToMove ? -Rook : Rook);
					mailboxBoard64[BackRankBaseSquareIndex[sideToMove] + A] = Empty;
					piecesBB[sideToMove][Rook] ^= UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + A) ^ UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + D);
					piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + A) ^ UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + D);
					hash64 ^= TranspositionTableRandoms[sideToMove][Rook][BackRankBaseSquareIndex[sideToMove] + D] ^ TranspositionTableRandoms[sideToMove][Rook][BackRankBaseSquareIndex[sideToMove] + A];
					gameRecordPointer->totalOpeningPST[sideToMove] += OpeningPSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + D) ^ pstXOR] - OpeningPSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + A) ^ pstXOR];
					gameRecordPointer->totalEndgamePST[sideToMove] += EndgamePSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + D) ^ pstXOR] - EndgamePSTs[Rook - 1][(BackRankBaseSquareIndex[sideToMove] + A) ^ pstXOR];
				}
			}
		}
		//break;
	}

	// Was it a capture?
	if (currentMove->toSquarePiece)
	{
		piecesBB[sideToMove ^ 1][abs(currentMove->toSquarePiece)] ^= UINT64SetBit(currentMove->mf.toSquare);
		piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(currentMove->mf.toSquare);
		gameRecordPointer->totalMaterial[sideToMove ^ 1] -= MaterialValue[abs(currentMove->toSquarePiece)];
		gameRecordPointer->gamePhase[sideToMove ^ 1] -= GamePhaseIncrement[abs(currentMove->toSquarePiece)];
		gameRecordPointer->totalOpeningPST[sideToMove ^ 1] -= OpeningPSTs[abs(currentMove->toSquarePiece) - 1][currentMove->mf.toSquare ^ pstXOR ^ 56];
		gameRecordPointer->totalEndgamePST[sideToMove ^ 1] -= EndgamePSTs[abs(currentMove->toSquarePiece) - 1][currentMove->mf.toSquare ^ pstXOR ^ 56];

		// Remove the to square piece from the to square
		hash64 ^= TranspositionTableRandoms[sideToMove ^ 1][abs(currentMove->toSquarePiece)][currentMove->mf.toSquare];

		if (abs(currentMove->toSquarePiece) == Pawn)
		{
			// Was it an en-passant capture?
			if (currentMove->mf.flag == MFEnPassant)
			{ // An EP move is stored as e.g. fromSquare=d5, toSquare = e5 (not e6), so we have to move the capturing pawn forward one square
				piecesBB[sideToMove][Pawn] ^= UINT64SetBit(currentMove->mf.toSquare + PawnMoveOffset[sideToMove]) ^ UINT64SetBit(currentMove->mf.toSquare);
				piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(currentMove->mf.toSquare + PawnMoveOffset[sideToMove]) ^ UINT64SetBit(currentMove->mf.toSquare);
				mailboxBoard64[currentMove->mf.toSquare + PawnMoveOffset[sideToMove]] = -currentMove->toSquarePiece;
				mailboxBoard64[currentMove->mf.toSquare] = Empty;
				hash64 ^= TranspositionTableRandoms[sideToMove][Pawn][currentMove->mf.toSquare + PawnMoveOffset[sideToMove]] ^ TranspositionTableRandoms[sideToMove][Pawn][currentMove->mf.toSquare];

				gameRecordPointer->totalOpeningPST[sideToMove] += OpeningPSTs[Pawn - 1][(currentMove->mf.toSquare + PawnMoveOffset[sideToMove]) ^ pstXOR] - OpeningPSTs[Pawn - 1][(currentMove->mf.toSquare) ^ pstXOR];
				gameRecordPointer->totalEndgamePST[sideToMove] += EndgamePSTs[Pawn - 1][(currentMove->mf.toSquare + PawnMoveOffset[sideToMove]) ^ pstXOR] - EndgamePSTs[Pawn - 1][(currentMove->mf.toSquare) ^ pstXOR];
			}
		}
		else
		{
			if (abs(currentMove->toSquarePiece) == Rook)
			{
				// Update castling statuses
				if (currentMove->mf.toSquare == BackRankBaseSquareIndex[sideToMove ^ 1] + InitialKingSideRookFile)
				{
					if (gameRecordPointer->castlingStatus.ui8[sideToMove ^ 1][0] == 0)
						hash64 ^= TranspositionTableRandomKingSideCastling[sideToMove ^ 1];
					gameRecordPointer->castlingStatus.ui8[sideToMove ^ 1][0] = 1;
				}
				else if (currentMove->mf.toSquare == BackRankBaseSquareIndex[sideToMove ^ 1] + InitialQueenSideRookFile)
				{
					if (gameRecordPointer->castlingStatus.ui8[sideToMove ^ 1][1] == 0)
						hash64 ^= TranspositionTableRandomQueenSideCastling[sideToMove ^ 1];
					gameRecordPointer->castlingStatus.ui8[sideToMove ^ 1][1] = 1;
				}
			}
		}
	}

	// Update '50-move' counter
	assert(((gameRecordPointer-1)->pliesSinceIrreversible >= 0) && ((gameRecordPointer-1)->pliesSinceIrreversible <= 100));
	if (
		(abs(currentMove->fromSquarePiece) == Pawn) // Pawn move?
		|| (currentMove->toSquarePiece) // Capture?
		|| (gameRecordPointer->castlingStatus.ui32 != (gameRecordPointer - 1)->castlingStatus.ui32) // Move by king (including castling) or rook that changes castling status?
		)
		gameRecordPointer->pliesSinceIrreversible = 0;
	else
		gameRecordPointer->pliesSinceIrreversible = (gameRecordPointer - 1)->pliesSinceIrreversible + 1;
	assert((gameRecordPointer->pliesSinceIrreversible >= 0) && (gameRecordPointer->pliesSinceIrreversible <= 100));

	// Update hash for side to move
	hash64 = ~hash64;

	// Update hashes
	gameRecordPointer->transpositionTableHash64 = hash64;
	gameRecordPointer->transpositionTableHash64WithEP = hash64 ^ TranspositionTableRandomsEnPassant[gameRecordPointer->epSquare]; // N.B. TranspositionTableRandomsEnPassant[0] = 0

	assert(!IsAttacked(BitScanForwardX(piecesBB[sideToMove][King]), sideToMove ^ 1));
}

void Brain::UnMakeMove(int sideToMove)
{
	gameRecordPointer--;
	MoveUndo_Struct* currentMove = &gameRecordPointer->move;
	assert((currentMove->mf.fromSquare >= A1) && (currentMove->mf.fromSquare <= H8) && (currentMove->mf.toSquare >= A1) && (currentMove->mf.toSquare <= H8));

	mailboxBoard64[currentMove->mf.toSquare] = currentMove->toSquarePiece; // N.B. must un-update the to-square BEFORE the from-square for Chess960 castling as they might be the same square!
	mailboxBoard64[currentMove->mf.fromSquare] = currentMove->fromSquarePiece;
	piecesBB[sideToMove][abs(currentMove->fromSquarePiece)] ^= currentMove->fromToXor;
	piecesBB[sideToMove][AllPieces] ^= currentMove->fromToXor;

	if (currentMove->mf.flag == MFCastling)
	{
		if (UCI_Chess960)
		{
			if (currentMove->mf.toSquare == BackRankBaseSquareIndex[sideToMove] + G)
			{
				if (abs(mailboxBoard64[BackRankBaseSquareIndex[sideToMove] + F]) != King)
					mailboxBoard64[BackRankBaseSquareIndex[sideToMove] + F] = Empty;
				mailboxBoard64[BackRankBaseSquareIndex[sideToMove] + InitialKingSideRookFile] = (sideToMove ? -Rook : Rook);
				piecesBB[sideToMove][Rook] ^= (UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + InitialKingSideRookFile) ^ UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + F));
				piecesBB[sideToMove][AllPieces] ^= (UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + InitialKingSideRookFile) ^ UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + F));
			}
			else
			{
				if (abs(mailboxBoard64[BackRankBaseSquareIndex[sideToMove] + D]) != King)
					mailboxBoard64[BackRankBaseSquareIndex[sideToMove] + D] = Empty;
				mailboxBoard64[BackRankBaseSquareIndex[sideToMove] + InitialQueenSideRookFile] = (sideToMove ? -Rook : Rook);
				piecesBB[sideToMove][Rook] ^= (UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + InitialQueenSideRookFile) ^ UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + D));
				piecesBB[sideToMove][AllPieces] ^= (UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + InitialQueenSideRookFile) ^ UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + D));
			}
		}
		else
		{
			if (currentMove->mf.toSquare == BackRankBaseSquareIndex[sideToMove] + G)
			{
				mailboxBoard64[BackRankBaseSquareIndex[sideToMove] + F] = Empty;
				mailboxBoard64[BackRankBaseSquareIndex[sideToMove] + H] = (sideToMove ? -Rook : Rook);
				piecesBB[sideToMove][Rook] ^= (UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + H) ^ UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + F));
				piecesBB[sideToMove][AllPieces] ^= (UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + H) ^ UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + F));
			}
			else
			{
				mailboxBoard64[BackRankBaseSquareIndex[sideToMove] + D] = Empty;
				mailboxBoard64[BackRankBaseSquareIndex[sideToMove] + A] = (sideToMove ? -Rook : Rook);
				piecesBB[sideToMove][Rook] ^= (UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + A) ^ UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + D));
				piecesBB[sideToMove][AllPieces] ^= (UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + A) ^ UINT64SetBit(BackRankBaseSquareIndex[sideToMove] + D));
			}
		}
	}
	else
	{
		if (currentMove->mf.flag >= MFPromotion) // Promotion
		{
			piecesBB[sideToMove][Pawn] ^= UINT64SetBit(currentMove->mf.toSquare);

			piecesBB[sideToMove][PromotedPieces[currentMove->mf.flag >> 2]] ^= UINT64SetBit(currentMove->mf.toSquare);
		}

		if (currentMove->toSquarePiece) // Capture?
		{
			// Restore the captured pieces bitboard
			piecesBB[sideToMove ^ 1][abs(currentMove->toSquarePiece)] ^= UINT64SetBit(currentMove->mf.toSquare);
			piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(currentMove->mf.toSquare);

			// Was it an en-passant capture?
			if (currentMove->mf.flag == MFEnPassant)
			{
				mailboxBoard64[currentMove->mf.toSquare + PawnMoveOffset[sideToMove]] = Empty;
				piecesBB[sideToMove][Pawn] ^= (UINT64SetBit(currentMove->mf.toSquare + PawnMoveOffset[sideToMove]) ^ UINT64SetBit(currentMove->mf.toSquare));
				piecesBB[sideToMove][AllPieces] ^= (UINT64SetBit(currentMove->mf.toSquare + PawnMoveOffset[sideToMove]) ^ UINT64SetBit(currentMove->mf.toSquare));
			}
		}
	}
}

int Brain::IsAttacked(int square, int sideToMove)
{
	return
		(KnightAttacksBBList[square] & piecesBB[sideToMove][Knight]) ||
		((piecesBB[sideToMove][Bishop] | piecesBB[sideToMove][Queen]) && (BishopAttacksBB(square, piecesBB[sideToMove][AllPieces] | piecesBB[sideToMove ^ 1][AllPieces]) & (piecesBB[sideToMove][Bishop] | piecesBB[sideToMove][Queen]))) ||
		(PawnAttacksBBList[sideToMove ^ 1][square] & piecesBB[sideToMove][Pawn]) ||
		((piecesBB[sideToMove][Rook] | piecesBB[sideToMove][Queen]) && (RookAttacksBB(square, piecesBB[sideToMove][AllPieces] | piecesBB[sideToMove ^ 1][AllPieces]) & (piecesBB[sideToMove][Rook] | piecesBB[sideToMove][Queen]))) ||
		(KingAttacksBBList[square] & piecesBB[sideToMove][King])
		;
}

int Brain::IsEnemyKingAttacked(int square, int sideToMove)
{
	// This routine is identical to IsAttacked except that it skips attacks by the king as it would be an illegal position!
	// N.B. The inclusion of the extra clause in the slider tests is intentional to allow short-circuiting
	// The king is attacked about 3% of the time
	return
		(KnightAttacksBBList[square] & piecesBB[sideToMove][Knight]) ||
		((piecesBB[sideToMove][Bishop] | piecesBB[sideToMove][Queen]) && (BishopAttacksBB(square, piecesBB[sideToMove][AllPieces] | piecesBB[sideToMove ^ 1][AllPieces]) & (piecesBB[sideToMove][Bishop] | piecesBB[sideToMove][Queen]))) ||
		(PawnAttacksBBList[sideToMove ^ 1][square] & piecesBB[sideToMove][Pawn]) ||
		((piecesBB[sideToMove][Rook] | piecesBB[sideToMove][Queen]) && (RookAttacksBB(square, piecesBB[sideToMove][AllPieces] | piecesBB[sideToMove ^ 1][AllPieces]) & (piecesBB[sideToMove][Rook] | piecesBB[sideToMove][Queen])))
		;
}

uint64_t Brain::RankFilePinnersBB(int kingSquare, int sideToMove)
{
	uint64_t occupiedBB = piecesBB[0][AllPieces] | piecesBB[1][AllPieces];
	uint64_t attacksBB = RookAttacksBB(kingSquare, occupiedBB);
	uint64_t potentiallyPinnedBB = attacksBB & piecesBB[sideToMove][AllPieces]; // Find friendly pieces that may be pinned
	return (attacksBB ^ RookAttacksBB(kingSquare, occupiedBB ^ potentiallyPinnedBB)) & (piecesBB[sideToMove ^ 1][Rook] | piecesBB[sideToMove ^ 1][Queen]);
}

uint64_t Brain::DiagonalPinnersBB(int kingSquare, int sideToMove)
{
	uint64_t occupiedBB = piecesBB[0][AllPieces] | piecesBB[1][AllPieces];
	uint64_t attacksBB = BishopAttacksBB(kingSquare, occupiedBB);
	uint64_t potentiallyPinnedBB = attacksBB & piecesBB[sideToMove][AllPieces]; // Find friendly pieces that may be pinned
	return (attacksBB ^ BishopAttacksBB(kingSquare, occupiedBB ^ potentiallyPinnedBB)) & (piecesBB[sideToMove ^ 1][Bishop] | piecesBB[sideToMove ^ 1][Queen]);
}

uint64_t Brain::RankFileDiscovereesBB(int enemyKingSquare, int sideToMove)
{
	uint64_t occupiedBB = piecesBB[0][AllPieces] | piecesBB[1][AllPieces];
	uint64_t attacksBB = RookAttacksBB(enemyKingSquare, occupiedBB);
	uint64_t potentiallyDiscovererBB = attacksBB & piecesBB[sideToMove][AllPieces]; // Find friendly pieces that may be discoverers
	return (attacksBB ^ RookAttacksBB(enemyKingSquare, occupiedBB ^ potentiallyDiscovererBB)) & (piecesBB[sideToMove][Rook] | piecesBB[sideToMove][Queen]);
}

uint64_t Brain::DiagonalDiscovereesBB(int enemyKingSquare, int sideToMove)
{
	uint64_t occupiedBB = piecesBB[0][AllPieces] | piecesBB[1][AllPieces];
	uint64_t attacksBB = BishopAttacksBB(enemyKingSquare, occupiedBB);
	uint64_t potentiallyDiscovererBB = attacksBB & piecesBB[sideToMove][AllPieces]; // Find friendly pieces that may be discoverers
	return (attacksBB ^ BishopAttacksBB(enemyKingSquare, occupiedBB ^ potentiallyDiscovererBB)) & (piecesBB[sideToMove][Bishop] | piecesBB[sideToMove][Queen]);
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
			int fromSquarePiece = std::abs(mailboxBoard64[fromSquare]);
			int toSquarePiece = std::abs(mailboxBoard64[toSquare]);

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
					score = gameRecordPointer->historyPointer->History[fromSquarePiece - 1][toSquare];
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
		int fromSquarePiece = std::abs(mailboxBoard64[fromSquare]);
		int toSquarePiece = std::abs(mailboxBoard64[toSquare]);

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
			int fromSquarePiece = std::abs(mailboxBoard64[fromSquare]);
			int toSquarePiece = std::abs(mailboxBoard64[toSquare]);

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
						score = gameRecordPointer->historyPointer->History[fromSquarePiece - 1][toSquare];
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
	uint32_t* p1 = gameRecordPointer->principalVariationPointer;
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
	int latestToSquarePiece = abs(mailboxBoard64[fromSquare]);
	int sideToMoveTotalGain = SeeValues[abs(mailboxBoard64[toSquare])];
	int sideNotToMoveTotalGain = 0;
	int piece;

	occupiedBB = piecesBB[0][AllPieces] | piecesBB[1][AllPieces];
	occupiedBB ^= UINT64SetBit(fromSquare); // Remove the initial capturing piece

	if (latestToSquarePiece == Pawn)
	{
		if (abs(toSquare - fromSquare) == 1) // Handle en-passant
		{
			occupiedBB ^= UINT64SetBit(toSquare); // Remove the ep captured pawn
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
		(RookAttacksBB(toSquare, occupiedBB) & (piecesBB[0][Rook] | piecesBB[1][Rook] | piecesBB[0][Queen] | piecesBB[1][Queen])) |
		(BishopAttacksBB(toSquare, occupiedBB) & (piecesBB[0][Bishop] | piecesBB[1][Bishop] | piecesBB[0][Queen] | piecesBB[1][Queen])) |
		(KnightAttacksBBList[toSquare] & (piecesBB[0][Knight] | piecesBB[1][Knight])) |
		(KingAttacksBBList[toSquare] & (piecesBB[0][King] | piecesBB[1][King])) |
		(PawnAttacksBBList[0][toSquare] & piecesBB[1][Pawn]) |
		(PawnAttacksBBList[1][toSquare] & piecesBB[0][Pawn]);
	attackersBB &= occupiedBB; // Remove all the empty squares along the rays

	int floor = -1, ceiling = 1;

sideNotToMove:
	if (sideNotToMoveTotalGain >= sideToMoveTotalGain)
	{
		if (sideNotToMoveTotalGain > sideToMoveTotalGain)
			goto exit;
		ceiling = 0;
	}

	if (attackersBB & piecesBB[sideNotToMove][AllPieces]) // Any more defenders?
	{
		// Find the lowest valued piece type
		sideNotToMoveTotalGain += SeeValues[latestToSquarePiece];
		if (latestToSquarePiece == King)
			goto exit;
		piece = Pawn;
		while (!(attackersBB & piecesBB[sideNotToMove][piece]))
			piece++;
		latestToSquarePiece = piece;

		if (latestToSquarePiece == Pawn)
			if ((toSquare >> 3) == EigthRank[sideNotToMove]) // Handle promotion
			{
				sideNotToMoveTotalGain += SeeValues[Queen] - SeeValues[Pawn];
				latestToSquarePiece = Queen;
			}

		uint64_t bb = attackersBB & piecesBB[sideNotToMove][piece];
		occupiedBB ^= (bb & -bb); // Remove the latest attacker(x & -x gives you the LS1B)
		// Add any new x-ray attacks by either side behind the just removed piece
		attackersBB |=
			(RookAttacksBB(toSquare, occupiedBB) & (piecesBB[0][Rook] | piecesBB[1][Rook] | piecesBB[0][Queen] | piecesBB[1][Queen])) |
			(BishopAttacksBB(toSquare, occupiedBB) & (piecesBB[0][Bishop] | piecesBB[1][Bishop] | piecesBB[0][Queen] | piecesBB[1][Queen]));
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

	if (attackersBB & piecesBB[sideToMove][AllPieces]) // Any more attackers?
	{
		// Find the lowest valued piece type
		sideToMoveTotalGain += SeeValues[latestToSquarePiece];
		if (latestToSquarePiece == King)
			goto exit;
		piece = Pawn;
		while (!(attackersBB & piecesBB[sideToMove][piece]))
			piece++;
		latestToSquarePiece = piece;

		if (latestToSquarePiece == Pawn)
			if ((toSquare >> 3) == EigthRank[sideToMove]) // Handle promotion
			{
				sideToMoveTotalGain += SeeValues[Queen] - SeeValues[Pawn];
				latestToSquarePiece = Queen;
			}

		uint64_t bb = attackersBB & piecesBB[sideToMove][piece];
		occupiedBB ^= (bb & -bb); // Remove the latest attacker(x & -x gives you the LS1B)
		// Add any new x-ray attacks by either side behind the just removed piece
		attackersBB |=
			(RookAttacksBB(toSquare, occupiedBB) & (piecesBB[0][Rook] | piecesBB[1][Rook] | piecesBB[0][Queen] | piecesBB[1][Queen])) |
			(BishopAttacksBB(toSquare, occupiedBB) & (piecesBB[0][Bishop] | piecesBB[1][Bishop] | piecesBB[0][Queen] | piecesBB[1][Queen]));
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
	int latestToSquarePiece = abs(mailboxBoard64[fromSquare]);
	int sideToMoveTotalGain = threshold;
	int sideNotToMoveTotalGain = 0;
	int piece;

	occupiedBB = piecesBB[0][AllPieces] | piecesBB[1][AllPieces];
	occupiedBB ^= UINT64SetBit(fromSquare); // Remove the initial capturing piece

	if (latestToSquarePiece == Pawn)
	{
		if (abs(toSquare - fromSquare) == 1) // Handle en-passant
		{
			occupiedBB ^= UINT64SetBit(toSquare); // Remove the ep captured pawn
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
		(RookAttacksBB(toSquare, occupiedBB) & (piecesBB[0][Rook] | piecesBB[1][Rook] | piecesBB[0][Queen] | piecesBB[1][Queen])) |
		(BishopAttacksBB(toSquare, occupiedBB) & (piecesBB[0][Bishop] | piecesBB[1][Bishop] | piecesBB[0][Queen] | piecesBB[1][Queen])) |
		(KnightAttacksBBList[toSquare] & (piecesBB[0][Knight] | piecesBB[1][Knight])) |
		(KingAttacksBBList[toSquare] & (piecesBB[0][King] | piecesBB[1][King])) |
		(PawnAttacksBBList[0][toSquare] & piecesBB[1][Pawn]) |
		(PawnAttacksBBList[1][toSquare] & piecesBB[0][Pawn]);
	attackersBB &= occupiedBB; // Remove all the empty squares along the rays

	int floor = -1, ceiling = 1;

sideNotToMove:
	if (sideNotToMoveTotalGain >= sideToMoveTotalGain)
	{
		if (sideNotToMoveTotalGain > sideToMoveTotalGain)
			goto exit;
		ceiling = 0;
	}

	if (attackersBB & piecesBB[sideNotToMove][AllPieces]) // Any more defenders?
	{
		// Find the lowest valued piece type
		sideNotToMoveTotalGain += SeeValues[latestToSquarePiece];
		if (latestToSquarePiece == King)
			goto exit;
		piece = Pawn;
		while (!(attackersBB & piecesBB[sideNotToMove][piece]))
			piece++;
		latestToSquarePiece = piece;
		uint64_t bb = attackersBB & piecesBB[sideNotToMove][piece];
		occupiedBB ^= (bb & -bb); // Remove the latest attacker(x & -x gives you the LS1B)
		// Add any new x-ray attacks by either side behind the just removed piece
		attackersBB |=
			(RookAttacksBB(toSquare, occupiedBB) & (piecesBB[0][Rook] | piecesBB[1][Rook] | piecesBB[0][Queen] | piecesBB[1][Queen])) |
			(BishopAttacksBB(toSquare, occupiedBB) & (piecesBB[0][Bishop] | piecesBB[1][Bishop] | piecesBB[0][Queen] | piecesBB[1][Queen]));
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

	if (attackersBB & piecesBB[sideToMove][AllPieces]) // Any more attackers?
	{
		// Find the lowest valued piece type
		sideToMoveTotalGain += SeeValues[latestToSquarePiece];
		if (latestToSquarePiece == King)
			goto exit;
		piece = Pawn;
		while (!(attackersBB & piecesBB[sideToMove][piece]))
			piece++;
		latestToSquarePiece = piece;
		uint64_t bb = attackersBB & piecesBB[sideToMove][piece];
		occupiedBB ^= (bb & -bb); // Remove the latest attacker(x & -x gives you the LS1B)
		// Add any new x-ray attacks by either side behind the just removed piece
		attackersBB |=
			(RookAttacksBB(toSquare, occupiedBB) & (piecesBB[0][Rook] | piecesBB[1][Rook] | piecesBB[0][Queen] | piecesBB[1][Queen])) |
			(BishopAttacksBB(toSquare, occupiedBB) & (piecesBB[0][Bishop] | piecesBB[1][Bishop] | piecesBB[0][Queen] | piecesBB[1][Queen]));
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

	occupiedBB = piecesBB[0][AllPieces] | piecesBB[1][AllPieces];

	// Get all the attackers for both sides
	attackersBB =
		(RookAttacksBB(toSquare, occupiedBB) & (piecesBB[0][Rook] | piecesBB[1][Rook] | piecesBB[0][Queen] | piecesBB[1][Queen])) |
		(BishopAttacksBB(toSquare, occupiedBB) & (piecesBB[0][Bishop] | piecesBB[1][Bishop] | piecesBB[0][Queen] | piecesBB[1][Queen])) |
		(KnightAttacksBBList[toSquare] & (piecesBB[0][Knight] | piecesBB[1][Knight])) |
		(KingAttacksBBList[toSquare] & (piecesBB[0][King] | piecesBB[1][King])) |
		(PawnAttacksBBList[0][toSquare] & piecesBB[1][Pawn]) |
		(PawnAttacksBBList[1][toSquare] & piecesBB[0][Pawn]);
	attackersBB &= occupiedBB; // Remove all the empty squares along the rays

	int sideToMoveTotalGain = 0;
	int sideNotToMoveTotalGain = offset;
	int toSquarePiece = abs(mailboxBoard64[toSquare]);
	int piece;
	uint64_t bb;

sideToMove:
	if (sideToMoveTotalGain > sideNotToMoveTotalGain)
		return true;

	if (attackersBB & piecesBB[sideToMove][AllPieces]) // Any more attackers?
	{
		if (toSquarePiece == King)
			return true;
		// Find the lowest valued piece type
		piece = Pawn;
		while (!(attackersBB & piecesBB[sideToMove][piece]))
			piece++;
		sideToMoveTotalGain += SeeValues[toSquarePiece];
		toSquarePiece = piece;
		bb = attackersBB & piecesBB[sideToMove][piece];
		occupiedBB ^= (bb & -bb); // Remove the latest attacker(x & -x gives you the LS1B)
		// Add any new x-ray attacks by either side behind the just removed piece
		attackersBB |=
			(RookAttacksBB(toSquare, occupiedBB) & (piecesBB[0][Rook] | piecesBB[1][Rook] | piecesBB[0][Queen] | piecesBB[1][Queen])) |
			(BishopAttacksBB(toSquare, occupiedBB) & (piecesBB[0][Bishop] | piecesBB[1][Bishop] | piecesBB[0][Queen] | piecesBB[1][Queen]));
		attackersBB &= occupiedBB; // Remove all the empty squares and the discarded attacker(s)
	}
	else
		return false;

	//sideNotToMove:
	if (sideNotToMoveTotalGain >= sideToMoveTotalGain)
		return false;

	if (attackersBB & piecesBB[sideNotToMove][AllPieces]) // Any more defenders?
	{
		if (toSquarePiece == King)
			return false;
		// Find the lowest valued piece type
		piece = Pawn;
		while (!(attackersBB & piecesBB[sideNotToMove][piece]))
			piece++;
		sideNotToMoveTotalGain += SeeValues[toSquarePiece];
		toSquarePiece = piece;
		bb = attackersBB & piecesBB[sideNotToMove][piece];
		occupiedBB ^= (bb & -bb); // Remove the latest attacker(x & -x gives you the LS1B)
		// Add any new x-ray attacks by either side behind the just removed piece
		attackersBB |=
			(RookAttacksBB(toSquare, occupiedBB) & (piecesBB[0][Rook] | piecesBB[1][Rook] | piecesBB[0][Queen] | piecesBB[1][Queen])) |
			(BishopAttacksBB(toSquare, occupiedBB) & (piecesBB[0][Bishop] | piecesBB[1][Bishop] | piecesBB[0][Queen] | piecesBB[1][Queen]));
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
	if ((piecesBB[0][Queen] | piecesBB[1][Queen] | piecesBB[0][Rook] | piecesBB[1][Rook] | piecesBB[0][Pawn] | piecesBB[1][Pawn]) == 0)
	{
		// Lone white king?
		if (gameRecordPointer->totalMaterial[0] == 0)
		{
			// No pieces or lone Knight or lone Bishop? i.e. KvK, KvKB, KvKN
			if (gameRecordPointer->totalMaterial[1] <= MVBishop)
				return PVTDrawMinimumMaterial;

			// Two bishops of same colour? i.e. KvKBB
			if (gameRecordPointer->totalMaterial[1] == MVBishop + MVBishop)
				if (((piecesBB[1][Bishop] & LightBB) == 0) || ((piecesBB[1][Bishop] & DarkBB) == 0))
					return PVTDrawMinimumMaterial;

			// Two knights? i.e. KvKNN
			if (gameRecordPointer->totalMaterial[1] == MVKnight + MVKnight)
				if ((sideToMove == 0) || (piecesBB[0][King] & ~CornersBB)) // White to move or the white king not in a corner
					return PVTDrawMinimumMaterial;
		}

		// Lone black king?
		if (gameRecordPointer->totalMaterial[1] == 0)
		{
			// No pieces or lone Knight or lone Bishop? i.e. KvK, KBvK, KNvK
			if (gameRecordPointer->totalMaterial[0] <= MVBishop)
				return PVTDrawMinimumMaterial;

			// Two bishops of same colour? i.e. KBBvK
			if (gameRecordPointer->totalMaterial[0] == MVBishop + MVBishop)
				if (((piecesBB[0][Bishop] & LightBB) == 0) || ((piecesBB[0][Bishop] & DarkBB) == 0))
					return PVTDrawMinimumMaterial;

			// Two knights? i.e. KNNvK
			if (gameRecordPointer->totalMaterial[0] == MVKnight + MVKnight)
				if ((sideToMove == 1) || (piecesBB[1][King] & ~CornersBB)) // Black to move or the black king not in a corner
					return PVTDrawMinimumMaterial;
		}

		// KB v KB (bishops same colour)
		if ((gameRecordPointer->totalMaterial[0] == MVBishop) && (gameRecordPointer->totalMaterial[1] == MVBishop))
			if (PopulationCountX(piecesBB[0][Bishop] & LightBB) == PopulationCountX(piecesBB[1][Bishop] & LightBB))
				return PVTDrawMinimumMaterial;

		// Neither king in a corner?
		if (!((piecesBB[0][King] ^ piecesBB[1][King]) & CornersBB))
		{
			// KN v KN
			if ((gameRecordPointer->totalMaterial[0] == MVKnight) && (gameRecordPointer->totalMaterial[1] == MVKnight))
				return PVTDrawMinimumMaterial;

			// KB v KN
			if (
				((gameRecordPointer->totalMaterial[0] == MVBishop) && (gameRecordPointer->totalMaterial[1] == MVKnight)) ||
				((gameRecordPointer->totalMaterial[1] == MVBishop) && (gameRecordPointer->totalMaterial[0] == MVKnight))
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
	if (gameRecordPointer->totalMaterial[0] == 0)
	{
		if (piecesBB[1][Queen] | piecesBB[1][Rook])
			return true;
	}
	// Lone black king?
	if (gameRecordPointer->totalMaterial[1] == 0)
	{
		if (piecesBB[0][Queen] | piecesBB[0][Rook])
			return true;
	}

	return false;
}

bool Brain::KingCanLegallyMove(int sideToMove)
{
	int fromSquare;
	uint64_t attacksBB;

	fromSquare = BitScanForwardX(piecesBB[sideToMove][King]);
	attacksBB = KingAttacksBBList[fromSquare] & ~piecesBB[sideToMove][AllPieces];
	while (attacksBB)
	{
		int toSquare = BitScanForwardX(attacksBB);
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
		if (!((gameRecordPointer - i)->isZLKM))
			return false;
	}

	return true;
}

int Brain::SafePawnMoves(int sideToMove)
{
	uint64_t occupiedBB = piecesBB[0][AllPieces] | piecesBB[1][AllPieces];
	uint64_t stmPawnMoveBB = ((piecesBB[sideToMove][Pawn] << 8) >> (sideToMove << 4)) & notOccupiedBB;
	uint64_t sntmPawnCaptureBB = ((piecesBB[sideToMove ^ 1][Pawn] << 8) >> ((sideToMove ^ 1) << 4));
	sntmPawnCaptureBB = East(sntmPawnCaptureBB) | West(sntmPawnCaptureBB);
	return PopulationCountX(stmPawnMoveBB & ~sntmPawnCaptureBB);
}

bool Brain::HasOpposition(int sideToMove)
{
	int stmKingSquare = BitScanForwardX(piecesBB[sideToMove][King]);
	int sntmKingSquare = BitScanForwardX(piecesBB[sideToMove ^ 1][King]);
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
		MoveNotation(gameRecord[GameRecordIndexRoot + i].move.ui32) +
		" ";//"(" + lowerLimitPointer[i] + "/" + bestScorePointer[i] + "/" + upperLimitPointer[i] + ") ";

	return currentLine;
}

Move_Struct Brain::ThreateningMateInOneWithNull(int sideToMove, int &checksCount)
{
	Move_Struct result;

	gameRecordPointer++; // Normally done in make/unmake-move
	gameRecordPointer->castlingStatus = (gameRecordPointer - 1)->castlingStatus;
	gameRecordPointer->epSquare = 0;
	gameRecordPointer->pliesSinceIrreversible = 0;
	result = ThreateningMateInOne(sideToMove, checksCount);
	gameRecordPointer--; // Normally done in make/unmake-move

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
		gameRecordPointer->move.ui32 = currentMove.ui32;
		MakeMove(sideToMove); // N.B. MakeMove increments MateBrain.gameRecordPointer!

		CalculatePinnedPieces(sideToMove ^ 1); // Required for legal move generation
		bool anyMoves = AnyMoves(sideToMove ^ 1, true);

		UnMakeMove(sideToMove); // N.B. UnMakeMove decrements MateBrain.gameRecordPointer!

		if (!anyMoves)
		{
			result.ui32 = gameRecordPointer->move.ui32;
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

	if (epSquare && (toSquare == epSquare) && (std::abs(mailboxBoard64[fromSquare]) == Pawn))
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