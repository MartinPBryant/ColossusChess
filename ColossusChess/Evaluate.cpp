#include <algorithm>
#include <assert.h>

#include "Engine.h"
#include "Utilities.h"
#include "Brain.h"
#include "SearchNormal.h"
//#include "NNUE\nnue.h"
//#include "nnue-probe-master\nnue-probe-master\src\nnue.h"
//#include "nncpu-probe-master\src\nncpu.h"

//----------------------------------------------------------------------------------------------------

// PSTs (64 {squares} * 2 {1 each for opening and endgame} * 6 {piece types})
const int parameters[64 * 2 * 6] =
{
// Pawn
	//Opening
 	0,    0,    0,    0,    0,    0,    0,    0, // a8-h8
   79,  115,   44,   76,   51,  107,   17,  -28,
  -25,  -10,    9,   14,   46,   37,    6,  -37,
  -33,   -4,  -11,    4,    4,   -7,   -2,  -40,
  -44,  -19,  -22,   -5,   -2,  -13,   -9,  -44,
  -43,  -21,  -21,  -27,  -16,  -16,   14,  -31,
  -52,  -18,  -37,  -40,  -34,    5,   19,  -41,
	0,    0,    0,    0,    0,    0,    0,    0, // a1-h1

	// Endgame
	0,    0,    0,    0,    0,    0,    0,    0,
  173,  166,  151,  129,  140,  127,  160,  180,
   87,   93,   78,   60,   51,   48,   77,   79,
   25,   17,    6,   -2,   -7,   -1,   12,   12,
	6,    2,  -10,  -13,  -13,  -13,   -2,   -6,
   -3,    0,  -11,   -6,   -5,  -12,   -6,  -13,
	6,    1,    1,    5,    6,   -5,   -3,  -12,
	0,    0,    0,    0,    0,    0,    0,    0,

//Knight
 -154,  -76,  -21,  -36,   74,  -84,   -2,  -94,
  -60,  -28,   85,   49,   36,   75,   20,   -4,
  -34,   73,   50,   78,   97,  142,   86,   57,
	4,   30,   32,   66,   50,   82,   31,   35,
	0,   17,   29,   26,   41,   32,   34,    5,
  -10,    4,   25,   23,   32,   30,   38,   -3,
  -16,  -40,    1,   10,   12,   31,   -1,   -6,
  -92,   -8,  -45,  -20,   -4,  -15,   -6,  -10,

 -101,  -81,  -56,  -71,  -74,  -70, -106, -142,
  -68,  -51,  -68,  -45,  -52,  -68,  -67,  -95,
  -67,  -63,  -33,  -34,  -44,  -52,  -62,  -84,
  -60,  -40,  -21,  -21,  -21,  -32,  -35,  -61,
  -61,  -49,  -27,  -18,  -27,  -26,  -39,  -61,
  -66,  -46,  -44,  -28,  -33,  -46,  -63,  -65,
  -85,  -63,  -53,  -48,  -45,  -63,  -66,  -87,
  -72,  -94,  -66,  -58,  -65,  -61,  -93, -107,

//Bishop
   11,   44,  -42,    3,   15,   -2,   47,   32,
   14,   56,   22,   27,   70,   99,   58,   -7,
   24,   77,   83,   80,   75,   90,   77,   38,
   36,   45,   59,   90,   77,   77,   47,   38,
   34,   53,   53,   66,   74,   52,   50,   44,
   40,   55,   55,   55,   54,   67,   58,   50,
   44,   55,   56,   40,   47,   61,   73,   41,
	7,   37,   26,   19,   27,   28,    1,   19,

  -42,  -49,  -39,  -36,  -35,  -37,  -45,  -52,
  -36,  -32,  -21,  -40,  -31,  -41,  -32,  -42,
  -26,  -36,  -28,  -29,  -30,  -22,  -28,  -24,
  -31,  -19,  -16,  -19,  -14,  -18,  -25,  -26,
  -34,  -25,  -15,   -9,  -21,  -18,  -31,  -37,
  -40,  -31,  -20,  -18,  -15,  -25,  -35,  -43,
  -42,  -46,  -35,  -29,  -24,  -37,  -43,  -55,
  -51,  -37,  -51,  -33,  -37,  -44,  -33,  -45,

// Rook
   10,   18,    8,   29,   39,  -13,    9,   19,
	5,    8,   36,   40,   56,   43,    4,   20,
  -27,   -3,    4,   12,   -5,   21,   37,   -8,
  -46,  -33,  -15,    2,    2,   11,  -32,  -44,
  -58,  -48,  -34,  -23,  -15,  -31,  -18,  -47,
  -67,  -47,  -38,  -39,  -21,  -24,  -29,  -57,
  -68,  -38,  -42,  -31,  -25,  -13,  -30,  -93,
  -43,  -37,  -21,   -7,   -6,  -17,  -59,  -48,

   24,   21,   29,   26,   25,   25,   21,   18,
   22,   24,   24,   22,   10,   16,   21,   16,
   18,   18,   18,   16,   17,   10,    8,   10,
   15,   14,   24,   14,   13,   14,   12,   15,
   14,   16,   19,   15,    8,    7,    5,    2,
	7,   11,    6,   10,    6,    1,    5,   -3,
	7,    5,   11,   13,    4,    4,    2,    8,
	2,   15,   14,   10,    8,    0,   15,   -7,

// Queen
   22,   50,   79,   62,  109,   94,   93,   95,
   26,   11,   45,   51,   34,  107,   78,  104,
   37,   33,   57,   58,   79,  106,   97,  107,
   23,   23,   34,   34,   49,   67,   48,   51,
   41,   24,   41,   40,   48,   46,   53,   47,
   36,   52,   39,   48,   45,   52,   64,   55,
   15,   42,   61,   52,   58,   65,   47,   51,
   49,   32,   41,   60,   35,   25,   19,    0,

  -48,  -17,  -17,  -12,  -12,  -20,  -29,  -19,
  -56,  -19,   -7,    2,   19,  -14,   -9,  -39,
  -59,  -33,  -30,   10,    8,   -4,  -20,  -30,
  -36,  -17,  -15,    6,   18,    1,   18,   -3,
  -57,  -11,  -20,    8,   -8,   -5,    0,  -16,
  -55,  -66,  -24,  -33,  -30,  -22,  -29,  -34,
  -61,  -62,  -69,  -55,  -55,  -62,  -75,  -71,
  -72,  -67,  -61,  -82,  -44,  -71,  -59,  -80,

// King
  -64,   22,   15,  -16,  -55,  -33,    3,   12,
   28,   -2,  -19,   -8,   -7,   -5,  -37,  -28,
  -10,   23,    3,  -17,  -19,    5,   23,  -21,
  -18,  -19,  -13,  -28,  -29,  -24,  -15,  -35,
  -50,   -2,  -28,  -40,  -45,  -43,  -32,  -50,
  -15,  -15,  -23,  -45,  -45,  -29,  -14,  -26,
    2,    8,   -9,  -63,  -44,  -15,    8,    7,
  -14,   35,   11,  -53,    7,  -27,   25,   13,

  -73,  -34,  -17,  -17,  -12,   14,    3,  -18,
  -11,   18,   15,   17,   17,   37,   22,   10,
   11,   18,   24,   16,   19,   44,   43,   12,
   -7,   23,   25,   26,   27,   32,   25,    2,
  -17,   -3,   22,   25,   26,   22,    8,  -12,
  -18,   -2,   12,   22,   22,   15,    6,  -10,
  -26,  -10,    4,   14,   13,    4,   -6,  -18,
  -52,  -33,  -20,  -12,  -27,  -15,  -25,  -44
};

const int* OpeningPSTs[6] =
{
	&parameters[128 * 0],
	&parameters[128 * 1],
	&parameters[128 * 2],
	&parameters[128 * 3],
	&parameters[128 * 4],
	&parameters[128 * 5]
};
const int* EndgamePSTs[6] =
{
	&parameters[64 + 128 * 0],
	&parameters[64 + 128 * 1],
	&parameters[64 + 128 * 2],
	&parameters[64 + 128 * 3],
	&parameters[64 + 128 * 4],
	&parameters[64 + 128 * 5]
};

//----------------------------------------------------------------------------------------------------

// Re-calculating the pawn structure score only when it changes seems like it would save time but in practice it makes hardly any difference!
// Perhaps if we had more detailed (expensive!) pawn structure scoring it would be worth trying again.
// Pawn structure only changes about 25-35% of the time (opening), 14-18% (mid - end game)
// Some pawn structure stuff (e.g. double, isolated) is one-sided, whereas some (e.g. passed) depends on the opps pawns too!
// N.B. Be VERY careful about what pawn scoring you cache because of its dependencies!!!
//82%, 80%, 76%, 66%
//struct PawnScore_Struct
//{
//	uint64_t bb;
//	int pawnStructureOpeningScore;
//	int pawnStructureEndgameScore;
//};
//PawnScore_Struct lastPawnScoreWhite, lastButOnePawnScoreWhite, lastPawnScoreBlack, lastButOnePawnScoreBlack;//THESE NEED TO BE LOCALISED!!!
//int pawnStructureOpeningScore[Sides];
//int pawnStructureEndgameScore[Sides];

uint32_t kingAttackDanger[8] = {0, 0, 128, 192, 225, 240, 248, 253};

int whitePassedPawnSquare, blackPassedPawnSquare;//TEMP

int Normal::EvaluateInner(int sideToMove)
{
	// Returns a score relative to the side to move e.g. white to move and a pawn up returns +100 as does black to move and a pawn up
	assert(MaterialValuesCorrect(normalBrain.mailboxBoard64, normalBrain.gameRecordPointer));
	assert(PSTValuesCorrect(normalBrain.mailboxBoard64, normalBrain.gameRecordPointer));

	int openingScore[Sides];
	int endgameScore[Sides];

	// N.B. these must be 'int' NOT 'uint32_t' else it screws up the signed multiplication in the final formula below!!!
	int gamePhase = normalBrain.gameRecordPointer->gamePhase[0] + normalBrain.gameRecordPointer->gamePhase[1];
	int scaleFactor = 0; // The lower scaleFactor is, the closer to zero the score will be pulled
	//int ceilingMaterial = MateBaseScore, floorMaterial = -MateBaseScore;
	
	whiteMovesToPromote = 9, blackMovesToPromote = 9;

	//----------------------------------------------------------------------------------------------------

	// PSTs
	openingScore[0] = normalBrain.gameRecordPointer->totalOpeningPST[0];
	openingScore[1] = normalBrain.gameRecordPointer->totalOpeningPST[1];
	endgameScore[0] = normalBrain.gameRecordPointer->totalEndgamePST[0];
	endgameScore[1] = normalBrain.gameRecordPointer->totalEndgamePST[1];

	//----------------------------------------------------------------------------------------------------

	// Pawn structure

	uint32_t doubled, isolated, backward, stragglers;
	uint64_t backwardBB, chainBB;
	

	//if (NormalGenerate.piecesBB[0][Pawn] == lastPawnScoreWhite.bb)
	//{
	//	openingScore[0] += lastPawnScoreWhite.pawnStructureOpeningScore;
	//	endgameScore[0] += lastPawnScoreWhite.pawnStructureEndgameScore;
	//}
	////else if (NormalGenerate.piecesBB[0][Pawn] == lastButOnePawnScoreWhite.bb)
	////{
	////	openingScore[0] += lastButOnePawnScoreWhite.pawnStructureOpeningScore;
	////	endgameScore[0] += lastButOnePawnScoreWhite.pawnStructureEndgameScore;
	////}
	//else


	// WOULD IT BE FASTER TO ACCUMULATE THE PAWN SCORE IN A LOCAL VARIABLE RATHER THAN lastPawnScoreWhite.pawnStructureOpeningScore? THEN IT CAN BE ENREGISTERED AND ASSIGNED INTO lastPawnScoreWhite.pawnStructureOpeningScore AFTERWARDS

	if (normalBrain.piecesBB[0][Pawn] != lastPawnScoreWhite.bb)
	{
		//AC1++;
		//lastButOnePawnScoreWhite = lastPawnScoreWhite;

		//if (NormalGenerate.piecesBB[0][Pawn] == 0) // (-2.1, +/-3.6, 20000)
		//{
		//	lastPawnScoreWhite.pawnStructureOpeningScore = -8;
		//	lastPawnScoreWhite.pawnStructureEndgameScore = -8;
		//}
		//else
		{
			lastPawnScoreWhite.pawnStructureOpeningScore = 0;
			lastPawnScoreWhite.pawnStructureEndgameScore = 0;

			doubled = PopulationCountX(normalBrain.piecesBB[0][Pawn] & SouthSpan(normalBrain.piecesBB[0][Pawn]));
			if (doubled > 0)
			{
				lastPawnScoreWhite.pawnStructureOpeningScore += -15 * doubled;
				lastPawnScoreWhite.pawnStructureEndgameScore += -10 * doubled;
			}

			isolated = PopulationCountX(isolanis(normalBrain.piecesBB[0][Pawn]));
			if (isolated > 0)
			{
				lastPawnScoreWhite.pawnStructureOpeningScore -= 16 * isolated;
				//lastPawnScoreWhite.pawnStructureEndgameScore -= 0 * isolated;
			}

			chainBB = normalBrain.piecesBB[0][Pawn];
			chainBB = East(chainBB) | West(chainBB);
			chainBB = chainBB | North(chainBB) | South(chainBB);
			chainBB = normalBrain.piecesBB[0][Pawn] & ~chainBB;
			if (chainBB)
			{
				lastPawnScoreWhite.pawnStructureOpeningScore -= 8 * PopulationCountX(chainBB);
			}
		}

		lastPawnScoreWhite.bb = normalBrain.piecesBB[0][Pawn];
	}
	openingScore[0] += lastPawnScoreWhite.pawnStructureOpeningScore;
	endgameScore[0] += lastPawnScoreWhite.pawnStructureEndgameScore;

	if (normalBrain.piecesBB[1][Pawn] != lastPawnScoreBlack.bb)
	{
		//if (NormalGenerate.piecesBB[1][Pawn] == 0)
		//{
		//	lastPawnScoreBlack.pawnStructureOpeningScore = -8;
		//	lastPawnScoreBlack.pawnStructureEndgameScore = -8;
		//}
		//else
		{
			lastPawnScoreBlack.pawnStructureOpeningScore = 0;
			lastPawnScoreBlack.pawnStructureEndgameScore = 0;

			doubled = PopulationCountX(normalBrain.piecesBB[1][Pawn] & NorthSpan(normalBrain.piecesBB[1][Pawn]));
			if (doubled > 0)
			{
				lastPawnScoreBlack.pawnStructureOpeningScore += -15 * doubled;
				lastPawnScoreBlack.pawnStructureEndgameScore += -10 * doubled;
			}

			isolated = PopulationCountX(isolanis(normalBrain.piecesBB[1][Pawn]));
			if (isolated > 0)
			{
				lastPawnScoreBlack.pawnStructureOpeningScore -= 16 * isolated;
				//lastPawnScoreBlack.pawnStructureEndgameScore -= 0 * isolated;
			}

			chainBB = normalBrain.piecesBB[1][Pawn];
			chainBB = East(chainBB) | West(chainBB);
			chainBB = chainBB | North(chainBB) | South(chainBB);
			chainBB = normalBrain.piecesBB[1][Pawn] & ~chainBB;
			if (chainBB)
			{
				lastPawnScoreBlack.pawnStructureOpeningScore -= 8 * PopulationCountX(chainBB);
			}
		}

		lastPawnScoreBlack.bb = normalBrain.piecesBB[1][Pawn];
	}
	openingScore[1] += lastPawnScoreBlack.pawnStructureOpeningScore;
	endgameScore[1] += lastPawnScoreBlack.pawnStructureEndgameScore;


	// Backward Ps on semi open files and the opponent has heavy pieces
	if (normalBrain.piecesBB[1][Rook] | normalBrain.piecesBB[1][Queen])
	{
		backwardBB = backwardSide1(normalBrain.piecesBB[0][Pawn], normalBrain.piecesBB[1][Pawn]);
		backwardBB &= halfOpenOrOpenFiles(normalBrain.piecesBB[1][Pawn]);
		if (backwardBB != 0)
		{
			openingScore[0] -= 32 * PopulationCountX(backwardBB);
			//stragglers = PopulationCountX(backwardBB  & ~SouthSpan(NormalGenerate.piecesBB[1][Pawn]) & (Rank2BB | Rank3BB));
			//stragglers = PopulationCountX(backwardBB  & ~SouthSpan(normalBrain.piecesBB[1][Pawn])  & ~SouthSpan(normalBrain.piecesBB[0][AllPieces]) & (Rank2BB | Rank3BB));
			//if (stragglers > 0)
			//	//pawnStructureOpeningScore[0] -= 16 * stragglers;
			//	openingScore[0] -= 16 * stragglers;
		}
	}

	if (normalBrain.piecesBB[0][Rook] | normalBrain.piecesBB[0][Queen])
	{
		backwardBB = backwardSide2(normalBrain.piecesBB[1][Pawn], normalBrain.piecesBB[0][Pawn]);
		backwardBB &= halfOpenOrOpenFiles(normalBrain.piecesBB[0][Pawn]);
		if (backwardBB != 0)
		{
			openingScore[1] -= 32 * PopulationCountX(backwardBB);
			//stragglers = PopulationCountX(backwardBB  & ~NorthSpan(NormalGenerate.piecesBB[0][Pawn]) & (Rank7BB | Rank6BB));
			//stragglers = PopulationCountX(backwardBB  & ~NorthSpan(NormalGenerate.piecesBB[0][Pawn]) & ~NorthSpan(NormalGenerate.piecesBB[1][AllPieces]) & (Rank7BB | Rank6BB));
			//if (stragglers > 0)
			//	//pawnStructureOpeningScore[1] -= 16 * stragglers;
			//	openingScore[1] -= 16 * stragglers;
		}
	}



	// Passed
	//TODO: connected passed pawns
	uint64_t passedBB, connectedPassedBB;
	int passed;
	passedBB = passedSide1(normalBrain.piecesBB[0][Pawn], normalBrain.piecesBB[1][Pawn]);
	passed = PopulationCountX(passedBB);
	if (passed > 0)
	{
		openingScore[0] += 15 * passed; //parameters[passedPawns] * passed;
		endgameScore[0] += 7 * passed; //parameters[passedPawns + 1] * passed;
		
		//connectedPassedBB = passedBB & ~isolanis(passedBB);
		//if (connectedPassedBB)
		//{
		//	openingScore[0] += 9;
		//	endgameScore[0] += 9;
		//}

		if (normalBrain.gameRecordPointer->gamePhase[1] == 0) // Opponent has no pieces to stop a runner?
		//if (normalBrain.gameRecordPointer->gamePhase[1] < 9) // Opponent has less than a Q to stop a runner?
		{
			// USING THIS PassedPawnCatchableByKing[STM] MAKES EVAL NON-SYMETRICAL!!!
			//USE PAWN-SQUARE BITBOARDS??? AND JUST CHECK IF THE OPP K IS INSIDE
			passedBB = passedBB & ~PassedPawnCatchableByKing[sideToMove][1][BitScanForwardX(normalBrain.piecesBB[1][King])];
			if (passedBB)
			{
				//endgameScore[0] += 16 * passed;
				whitePassedPawnSquare = (int)BitScanReverseX(passedBB);//TEMP
				whiteMovesToPromote = std::min((7 - ((int)BitScanReverseX(passedBB) >> 3)), 5); // At most 5 moves to promote as the first move can be two squares
			}
		}
	}
	passedBB = passedSide2(normalBrain.piecesBB[1][Pawn], normalBrain.piecesBB[0][Pawn]);
	passed = PopulationCountX(passedBB);
	if (passed > 0)
	{
		openingScore[1] += 15 * passed; //parameters[passedPawns] * passed;
		endgameScore[1] += 7 * passed; //parameters[passedPawns + 1] * passed;

		//connectedPassedBB = passedBB & ~isolanis(passedBB);
		//if (connectedPassedBB)
		//{
		//	openingScore[1] += 9;
		//	endgameScore[1] += 9;
		//}

		if (normalBrain.gameRecordPointer->gamePhase[0] == 0) // Opponent has no pieces to stop a runner?
		//if (normalBrain.gameRecordPointer->gamePhase[0] < 9) // Opponent has less than a Q to stop a runner?
		{
			passedBB = passedBB & ~PassedPawnCatchableByKing[sideToMove][0][BitScanForwardX(normalBrain.piecesBB[0][King])];
			if (passedBB)
			{
				//endgameScore[1] += 16 * passed;
				blackPassedPawnSquare = (int)BitScanForwardX(passedBB);//TEMP
				blackMovesToPromote = std::min((int)BitScanForwardX(passedBB) >> 3, 5); // At most 5 moves to promote as the first move can be two squares
			}
		}
	}

	//// Weak squares - http://talkchess.com/forum3/viewtopic.php?f=7&t=77674
	//uint64_t weakSquares0 = (East(NormalGenerate.piecesBB[0][Pawn]) << 8) | (West(NormalGenerate.piecesBB[0][Pawn]) << 8);
	//weakSquares0 |= weakSquares0 << 8;
	//weakSquares0 &= (Rank3BB | Rank4BB);
	//openingScore[0] -= (16 - PopulationCountX(weakSquares0)) << 4;
	//uint64_t weakSquares1 = (East(NormalGenerate.piecesBB[1][Pawn]) >> 8) | (West(NormalGenerate.piecesBB[1][Pawn]) >> 8);
	//weakSquares1 |= weakSquares1 >> 8;
	//weakSquares1 &= (Rank6BB | Rank5BB);
	//openingScore[1] -= (16 - PopulationCountX(weakSquares1)) << 4;

	//----------------------------------------------------------------------------------------------------
	
	//// Knight/Bishop adjustment for pawns
	//uint32_t totalPawns = PopulationCountX(NormalGenerate.piecesBB[0][Pawn]) + PopulationCountX(NormalGenerate.piecesBB[0][Pawn]);
	////endgameScore[0] += PopulationCountX(NormalGenerate.piecesBB[0][Knight]) * (totalPawns - 10) * 2;
	//endgameScore[0] += PopulationCountX(NormalGenerate.piecesBB[0][Bishop]) * (10 - totalPawns) * 2;
	////endgameScore[1] += PopulationCountX(NormalGenerate.piecesBB[1][Knight]) * (totalPawns - 10) * 2;
	//endgameScore[1] += PopulationCountX(NormalGenerate.piecesBB[1][Bishop]) * (10 - totalPawns) * 2;

	//----------------------------------------------------------------------------------------------------
	
	// Bishop pair
	if (PopulationCountX(normalBrain.piecesBB[0][Bishop]) > 1)
	{
		openingScore[0] += 20;
		endgameScore[0] += 20;
	}
	if (PopulationCountX(normalBrain.piecesBB[1][Bishop]) > 1)
	{
		openingScore[1] += 20;
		endgameScore[1] += 20;
	}

	//----------------------------------------------------------------------------------------------------

	// Rook on semi-open file
	uint64_t semiOpenRooks, openRooks;
	semiOpenRooks = normalBrain.piecesBB[0][Rook] & halfOpenOrOpenFiles(normalBrain.piecesBB[0][Pawn]);
	if (semiOpenRooks)
	{
		openingScore[0] += 20 * PopulationCountX(semiOpenRooks);
		endgameScore[0] += 10 * PopulationCountX(semiOpenRooks);
		//openRooks = semiOpenRooks & halfOpenOrOpenFiles(NormalGenerate.piecesBB[1][Pawn]);
		//if (openRooks)
		//{
		//	openingScore[0] += 8 * PopulationCountX(openRooks);
		//	//endgameScore[0] += 8 * PopulationCountX(openRooks);
		//}
	}
	semiOpenRooks = normalBrain.piecesBB[1][Rook] & halfOpenOrOpenFiles(normalBrain.piecesBB[1][Pawn]);
	if (semiOpenRooks)
	{
		openingScore[1] += 20 * PopulationCountX(semiOpenRooks);
		endgameScore[1] += 10 * PopulationCountX(semiOpenRooks);
		//openRooks = semiOpenRooks & halfOpenOrOpenFiles(NormalGenerate.piecesBB[0][Pawn]);
		//if (openRooks)
		//{
		//	openingScore[1] += 8 * PopulationCountX(openRooks);
		//	//endgameScore[1] += 8 * PopulationCountX(openRooks);
		//}
	}

	// Trapped rook
	if (((normalBrain.mailboxBoard64[H1] == Rook) || (normalBrain.mailboxBoard64[G1] == Rook)) && ((normalBrain.mailboxBoard64[G1] == King) || (normalBrain.mailboxBoard64[F1] == King)))
		openingScore[0] += -20;
	if (((normalBrain.mailboxBoard64[H8] == -Rook) || (normalBrain.mailboxBoard64[G8] == -Rook)) && ((normalBrain.mailboxBoard64[G8] == -King) || (normalBrain.mailboxBoard64[F8] == -King)))
		openingScore[1] += -20;

	//----------------------------------------------------------------------------------------------------

	// King pawn shelter
	uint32_t kingSquare, pawnShelter;
	
	pawnShelter = 0;
	kingSquare = BitScanForwardX(normalBrain.piecesBB[0][King]);
	//if (kingSquare <= H2)
	{
		if ((kingSquare & 7) >= 6)
			pawnShelter = std::min(6, int((PopulationCountX(normalBrain.piecesBB[0][Pawn] & F2G2H2BB) * 2) + PopulationCountX(normalBrain.piecesBB[0][Pawn] & F3G3H3BB)));
		else if ((kingSquare & 7) <= 2)
			pawnShelter = std::min(6, int((PopulationCountX(normalBrain.piecesBB[0][Pawn] & A2B2C2BB) * 2) + PopulationCountX(normalBrain.piecesBB[0][Pawn] & A3B3C3BB)));
	}
	openingScore[0] -= 3 * (6 - pawnShelter);
	
	pawnShelter = 0;
	kingSquare = BitScanForwardX(normalBrain.piecesBB[1][King]);
	//if (kingSquare >= A7)
	{
		if ((kingSquare & 7) >= 6)
			pawnShelter = std::min(6, int((PopulationCountX(normalBrain.piecesBB[1][Pawn] & F7G7H7BB) * 2) + PopulationCountX(normalBrain.piecesBB[1][Pawn] & F6G6H6BB)));
		else if ((kingSquare & 7) <= 2)
			pawnShelter = std::min(6, int((PopulationCountX(normalBrain.piecesBB[1][Pawn] & A7B7C7BB) * 2) + PopulationCountX(normalBrain.piecesBB[1][Pawn] & A6B6C6BB)));
	}
	openingScore[1] -= 3 * (6 - pawnShelter);

	//if (gamePhase == 0)
	//{
	//	if (HasOpposition(sideToMove))
	//		endgameScore[sideToMove] += 20;
	//}

	//int surroundingSquaresAttacked;
	//int fromSquare, toSquare;
	//uint64_t attacksBB;

	//if (NormalGenerate.piecesBB[0][King] & EdgesBB)
	//{

	//	surroundingSquaresAttacked = 0;
	//	fromSquare = BitScanForwardX(NormalGenerate.piecesBB[0][King]);
	//	attacksBB = KingAttacksBBList[fromSquare];
	//	while (attacksBB)
	//	{
	//		toSquare = BitScanForwardX(attacksBB);
	//		if (IsAttacked(toSquare, 0 ^ 1))
	//			surroundingSquaresAttacked++;
	//		ClearLS1B(attacksBB);
	//	}

	//	if (NormalGenerate.piecesBB[0][King] & CornersBB)
	//		surroundingSquaresAttacked += 2;

	//	openingScore[0] -= kingAttackedPenalty[surroundingSquaresAttacked];
	//	endgameScore[0] -= kingAttackedPenalty[surroundingSquaresAttacked];
	//}

	//if (NormalGenerate.piecesBB[1][King] & EdgesBB)
	//{
	//	surroundingSquaresAttacked = 0;
	//	fromSquare = BitScanForwardX(NormalGenerate.piecesBB[1][King]);
	//	attacksBB = KingAttacksBBList[fromSquare];
	//	while (attacksBB)
	//	{
	//		toSquare = BitScanForwardX(attacksBB);
	//		if (IsAttacked(toSquare, 1 ^ 1))
	//			surroundingSquaresAttacked++;
	//		ClearLS1B(attacksBB);
	//	}

	//	if (NormalGenerate.piecesBB[1][King] & CornersBB)
	//		surroundingSquaresAttacked += 2;

	//	openingScore[1] -= kingAttackedPenalty[surroundingSquaresAttacked];
	//	endgameScore[1] -= kingAttackedPenalty[surroundingSquaresAttacked];
	//}

	//----------------------------------------------------------------------------------------------------

	// Mobility
	uint32_t fromSquare;
	uint64_t attacksBB;
	uint64_t occupiedBB = normalBrain.piecesBB[0][AllPieces] | normalBrain.piecesBB[1][AllPieces];
	uint64_t pawnAttacks0 = Side0PawnAttacksBB(normalBrain.piecesBB[0][Pawn]);
	uint64_t pawnAttacks1 = Side1PawnAttacksBB(normalBrain.piecesBB[1][Pawn]);
	int mobility0 = 0, mobility1 = 0;

	//uint32_t kingSquareSide0, kingSquareSide1;
	//kingSquareSide0 = BitScanForwardX(NormalGenerate.piecesBB[0][King]);
	//kingSquareSide1 = BitScanForwardX(NormalGenerate.piecesBB[1][King]);
	//uint32_t enemyKingAttacksCountSide0 = 0, enemyKingAttacksWeightSide0 = 0, enemyKingAttacksCountSide1 = 0, enemyKingAttacksWeightSide1 = 0;
	//uint64_t kingPerimeterBBSide0, kingPerimeterBBSide1;
	//kingPerimeterBBSide0 = KingAttacksBBList[kingSquareSide0];
	//kingPerimeterBBSide1 = KingAttacksBBList[kingSquareSide1];
	
	// Pawns
	//if (GameRecordPointer->gamePhase[0] == 0)
	//{
	//	if ((North(NormalGenerate.piecesBB[0][Pawn]) & notOccupiedBB) == 0)
	//		mobility0 -= 20;
	//}
	//if (GameRecordPointer->gamePhase[1] == 0)
	//{
	//	if ((South(NormalGenerate.piecesBB[1][Pawn]) & notOccupiedBB) == 0)
	//		mobility1 -= 20;
	//}
	//if (Side0PawnAttacksBB(NormalGenerate.piecesBB[0][Pawn]) & kingPerimeterBBSide1)
	//{
	//	enemyKingAttacksCountSide0++;
	//	enemyKingAttacksWeightSide0++;
	//}
	//if (Side1PawnAttacksBB(NormalGenerate.piecesBB[1][Pawn]) & kingPerimeterBBSide0)
	//{
	//	enemyKingAttacksCountSide1++;
	//	enemyKingAttacksWeightSide1++;
	//}

	// Knights
	uint64_t knightsBB;
	knightsBB = normalBrain.piecesBB[0][Knight];
	while (knightsBB)
	{
		fromSquare = BitScanForwardX(knightsBB);
		attacksBB = KnightAttacksBBList[fromSquare];
		//if (attacksBB & kingPerimeterBBSide1)
		//{
		//	enemyKingAttacksCountSide0++;
		//	enemyKingAttacksWeightSide0 += 2;
		//}
		attacksBB = attacksBB & ~normalBrain.piecesBB[0][AllPieces] & ~pawnAttacks1;
		if (attacksBB == 0)
			mobility0 -= 20;
		else
			mobility0 += (PopulationCountX(attacksBB) << 1) + (PopulationCountX(attacksBB & Side1HalfBB) << 1);
		ClearLS1B(knightsBB);
	}
	knightsBB = normalBrain.piecesBB[1][Knight];
	while (knightsBB)
	{
		fromSquare = BitScanForwardX(knightsBB);
		attacksBB = KnightAttacksBBList[fromSquare];
		//if (attacksBB & kingPerimeterBBSide0)
		//{
		//	enemyKingAttacksCountSide1++;
		//	enemyKingAttacksWeightSide1 += 2;
		//}
		attacksBB = attacksBB & ~normalBrain.piecesBB[1][AllPieces] & ~pawnAttacks0;
		if (attacksBB == 0)
			mobility1 -= 20;
		else
			mobility1 += (PopulationCountX(attacksBB) << 1) + (PopulationCountX(attacksBB & Side0HalfBB) << 1);
		ClearLS1B(knightsBB);
	}

	// Bishops
	uint64_t bishopsBB;
	bishopsBB = normalBrain.piecesBB[0][Bishop];
	while (bishopsBB)
	{
		fromSquare = BitScanForwardX(bishopsBB);
		attacksBB = BishopAttacksBB(fromSquare, occupiedBB ^ normalBrain.piecesBB[0][Bishop] ^ normalBrain.piecesBB[0][Queen] ^ normalBrain.piecesBB[1][Bishop] ^ normalBrain.piecesBB[1][Queen]);
		//if (attacksBB & kingPerimeterBBSide1)
		//{
		//	enemyKingAttacksCountSide0++;
		//	enemyKingAttacksWeightSide0 += 2;
		//}
		attacksBB = attacksBB & ~normalBrain.piecesBB[0][AllPieces] & ~pawnAttacks1;
		if (attacksBB == 0)
			mobility0 -= 20;
		else
			mobility0 += PopulationCountX(attacksBB) + PopulationCountX(attacksBB & Side1HalfBB);
		ClearLS1B(bishopsBB);
	}
	bishopsBB = normalBrain.piecesBB[1][Bishop];
	while (bishopsBB)
	{
		fromSquare = BitScanForwardX(bishopsBB);
		attacksBB = BishopAttacksBB(fromSquare, occupiedBB ^ normalBrain.piecesBB[0][Bishop] ^ normalBrain.piecesBB[0][Queen] ^ normalBrain.piecesBB[1][Bishop] ^ normalBrain.piecesBB[1][Queen]);
		//if (attacksBB & kingPerimeterBBSide0)
		//{
		//	enemyKingAttacksCountSide1++;
		//	enemyKingAttacksWeightSide1 += 2;
		//}
		attacksBB = attacksBB & ~normalBrain.piecesBB[1][AllPieces] & ~pawnAttacks0;
		if (attacksBB == 0)
			mobility1 -= 20;
		else
			mobility1 += PopulationCountX(attacksBB) + PopulationCountX(attacksBB & Side0HalfBB);
		ClearLS1B(bishopsBB);
	}

	// Rooks
	uint64_t rooksBB;
	rooksBB = normalBrain.piecesBB[0][Rook];
	while (rooksBB)
	{
		fromSquare = BitScanForwardX(rooksBB);
		//attacksBB = RookAttacksBB(fromSquare, occupiedBB);
		attacksBB = RookAttacksBB(fromSquare, occupiedBB ^ normalBrain.piecesBB[0][Rook] ^ normalBrain.piecesBB[0][Queen] ^ normalBrain.piecesBB[1][Rook] ^ normalBrain.piecesBB[1][Queen]); // Allow mobility through own Rs & Qs
		//if (attacksBB & kingPerimeterBBSide1)
		//{
		//	enemyKingAttacksCountSide0++;
		//	enemyKingAttacksWeightSide0 += 4;
		//}
		attacksBB = attacksBB & ~normalBrain.piecesBB[0][AllPieces] & ~pawnAttacks1;// &Side1Half;
		if (attacksBB == 0)
			mobility0 -= 20;//IS THIS VALID WHEN WE ONLY COUNT THE OPPS SIDE OF BOARD???
		else
			mobility0 += PopulationCountX(attacksBB & Side1HalfBB);
		ClearLS1B(rooksBB);
	}
	rooksBB = normalBrain.piecesBB[1][Rook];
	while (rooksBB)
	{
		fromSquare = BitScanForwardX(rooksBB);
		//attacksBB = RookAttacksBB(fromSquare, occupiedBB);
		attacksBB = RookAttacksBB(fromSquare, occupiedBB ^ normalBrain.piecesBB[1][Rook] ^ normalBrain.piecesBB[1][Queen] ^ normalBrain.piecesBB[0][Rook] ^ normalBrain.piecesBB[0][Queen]);
		//if (attacksBB & kingPerimeterBBSide0)
		//{
		//	enemyKingAttacksCountSide1++;
		//	enemyKingAttacksWeightSide1 += 4;
		//}
		attacksBB = attacksBB & ~normalBrain.piecesBB[1][AllPieces] & ~pawnAttacks0;// &Side0Half;
		if (attacksBB == 0)
			mobility1 -= 20;
		else
			mobility1 += PopulationCountX(attacksBB & Side0HalfBB);
		ClearLS1B(rooksBB);
	}

	//// Queens
	//uint64_t queensBB;
	//queensBB = normalBrain.piecesBB[0][Queen];
	//while (queensBB)
	//{
	//	fromSquare = BitScanForwardX(queensBB);
	//	attacksBB = RookAttacksBB(fromSquare, occupiedBB ^ normalBrain.piecesBB[1][Queen]) | BishopAttacksBB(fromSquare, occupiedBB ^ normalBrain.piecesBB[1][Queen]);
	//	//if (attacksBB & kingPerimeterBBSide1)
	//	//{
	//	//	enemyKingAttacksCountSide0++;
	//	//	enemyKingAttacksWeightSide0 += 8;
	//	//}
	//	attacksBB = attacksBB & ~normalBrain.piecesBB[0][AllPieces] & ~pawnAttacks1;// &Side1Half;
	//	mobility0 += PopulationCountX(attacksBB & Side1Half);
	//	ClearLS1B(queensBB);
	//}
	//queensBB = normalBrain.piecesBB[1][Queen];
	//while (queensBB)
	//{
	//	fromSquare = BitScanForwardX(queensBB);
	//	attacksBB = RookAttacksBB(fromSquare, occupiedBB ^ normalBrain.piecesBB[0][Queen]) | BishopAttacksBB(fromSquare, occupiedBB ^ normalBrain.piecesBB[0][Queen]);
	//	//if (attacksBB & kingPerimeterBBSide0)
	//	//{
	//	//	enemyKingAttacksCountSide1++;
	//	//	enemyKingAttacksWeightSide1 += 8;
	//	//}
	//	attacksBB = attacksBB & ~normalBrain.piecesBB[1][AllPieces] & ~pawnAttacks0;// &Side1Half;
	//	mobility1 += PopulationCountX(attacksBB & Side0Half);
	//	ClearLS1B(queensBB);
	//}

	//enemyKingAttacksCountSide0 = std::min(enemyKingAttacksCountSide0, (uint32_t)7);
	//enemyKingAttacksCountSide1 = std::min(enemyKingAttacksCountSide1, (uint32_t)7);
	//uint32_t ka;
	//ka = (enemyKingAttacksWeightSide0 * 8 *  kingAttackDanger[enemyKingAttacksCountSide0]) / (uint32_t)256;
	//openingScore[0] += ka;
	//endgameScore[0] += ka;
	//ka = (enemyKingAttacksWeightSide1 * 8 * kingAttackDanger[enemyKingAttacksCountSide1]) / (uint32_t)256;
	//openingScore[1] += ka;
	//endgameScore[1] += ka;







	//// Kings
	//uint64_t kingsBB;
	//kingsBB = NormalGenerate.piecesBB[0][King];
	////while (kingsBB)
	//{
	//	fromSquare = BitScanForwardX(kingsBB);
	//	attacksBB = KingAttacksBBList[fromSquare] & ~NormalGenerate.piecesBB[0][AllPieces] & ~pawnAttacks1;
	//	mobility0 += PopulationCountX(attacksBB);
	//	//ClearLS1B(kingsBB);
	//}
	//kingsBB = NormalGenerate.piecesBB[1][King];
	////while (kingsBB)
	//{
	//	fromSquare = BitScanForwardX(kingsBB);
	//	attacksBB = KingAttacksBBList[fromSquare] & ~NormalGenerate.piecesBB[1][AllPieces] & ~pawnAttacks0;
	//	mobility1 += PopulationCountX(attacksBB);
	//	//ClearLS1B(kingsBB);
	//}

	openingScore[0] += mobility0;
	endgameScore[0] += mobility0;
	openingScore[1] += mobility1;
	endgameScore[1] += mobility1;

	//----------------------------------------------------------------------------------------------------

	//// Pinned pieces
	//int pinnedPieceCount;
	//CalculatePinnedPieces(0);
	//pinnedPieceCount = PopulationCountX(GameRecordPointer->pinnedRankFileBB | GameRecordPointer->pinnedDiagonalBB) * 16;
	//openingScore[0] -= pinnedPieceCount;
	//endgameScore[0] -= pinnedPieceCount;
	//CalculatePinnedPieces(1);
	//pinnedPieceCount = PopulationCountX(GameRecordPointer->pinnedRankFileBB | GameRecordPointer->pinnedDiagonalBB) * 16;
	//openingScore[1] -= pinnedPieceCount;
	//endgameScore[1] -= pinnedPieceCount;

	//----------------------------------------------------------------------------------------------------

	// Use scale factor and ceiling/floor for certain endgames
	if (gamePhase <= 10)
	{
		// Opposite coloured Bishop endings are drawish
		if ((PopulationCountX(normalBrain.piecesBB[0][Bishop]) == 1) && (PopulationCountX(normalBrain.piecesBB[1][Bishop]) == 1) && (PopulationCountX(normalBrain.piecesBB[0][Bishop] & LightBB) != PopulationCountX(normalBrain.piecesBB[1][Bishop] & LightBB)))
		{
			scaleFactor = 248;
			if ((normalBrain.piecesBB[0][Knight] | normalBrain.piecesBB[1][Knight] | normalBrain.piecesBB[0][Rook] | normalBrain.piecesBB[1][Rook] | normalBrain.piecesBB[0][Queen] | normalBrain.piecesBB[1][Queen]) == 0)
				scaleFactor = 192;
			//scaleFactor = 64;
			//scaleFactor = 128;
			goto exitScaleFactor;
		}

		// Rook + Pawn endings are drawish (+0.5, +/-3.6, 20000)
		if ((PopulationCountX(normalBrain.piecesBB[0][Rook]) > 0) && (PopulationCountX(normalBrain.piecesBB[1][Rook]) > 0) && (PopulationCountX(normalBrain.piecesBB[0][Rook]) == PopulationCountX(normalBrain.piecesBB[1][Rook])))
			if ((normalBrain.piecesBB[0][Knight] | normalBrain.piecesBB[1][Knight] | normalBrain.piecesBB[0][Bishop] | normalBrain.piecesBB[1][Bishop] | normalBrain.piecesBB[0][Queen] | normalBrain.piecesBB[1][Queen]) == 0)
			{
				scaleFactor = 240;
			}


		//if (TotalMaterial[sideToMove] <= materialValueBishop) // Lone king or king+minor WHAT ABOUT PAWNS?!?!?!!!
		//	ceilingMaterial = 0;
		//else
		//{
		//	// KRminor: value minor piece as a pawn
		//	if (TotalMaterial[sideToMove] == materialValueRook + materialValueBishop) // KRB
		//	{
		//		openingScoreMaterial[sideToMove] = openingScoreMaterial[sideToMove] - parameters[Bishop - 1] + parameters[Pawn - 1];
		//		endgameScoreMaterial[sideToMove] = endgameScoreMaterial[sideToMove] - parameters[Bishop - 1 + 6] + parameters[Pawn - 1 + 6];
		//	}
		//	if (TotalMaterial[sideToMove] == materialValueRook + materialValueKnight) // KRN
		//	{
		//		openingScoreMaterial[sideToMove] = openingScoreMaterial[sideToMove] - parameters[Knight - 1] + parameters[Pawn - 1];
		//		endgameScoreMaterial[sideToMove] = endgameScoreMaterial[sideToMove] - parameters[Knight - 1 + 6] + parameters[Pawn - 1 + 6];
		//	}
		//}

		//if (TotalMaterial[sideToMove ^ 1] <= materialValueBishop) // Lone king or king+minor
		//	floorMaterial = 0;
		//else
		//{
		//	// KRminor: value minor piece as a pawn
		//	if (TotalMaterial[sideToMove ^ 1] == materialValueRook + materialValueBishop) // KRB
		//	{
		//		openingScoreMaterial[sideToMove ^ 1] = openingScoreMaterial[sideToMove ^ 1] - parameters[Bishop - 1] + parameters[Pawn - 1];
		//		endgameScoreMaterial[sideToMove ^ 1] = endgameScoreMaterial[sideToMove ^ 1] - parameters[Bishop - 1 + 6] + parameters[Pawn - 1 + 6];
		//	}
		//	if (TotalMaterial[sideToMove ^ 1] == materialValueRook + materialValueKnight) // KRN
		//	{
		//		openingScoreMaterial[sideToMove ^ 1] = openingScoreMaterial[sideToMove ^ 1] - parameters[Knight - 1] + parameters[Pawn - 1];
		//		endgameScoreMaterial[sideToMove ^ 1] = endgameScoreMaterial[sideToMove ^ 1] - parameters[Knight - 1 + 6] + parameters[Pawn - 1 + 6];
		//	}
		//}


	}
exitScaleFactor:

	assert((openingScore[0] < EGTBWinningScore) && (openingScore[0] > -EGTBWinningScore));
	assert((openingScore[1] < EGTBWinningScore) && (openingScore[1] > -EGTBWinningScore));
	assert((endgameScore[0] < EGTBWinningScore) && (endgameScore[0] > -EGTBWinningScore));
	assert((endgameScore[1] < EGTBWinningScore) && (endgameScore[1] > -EGTBWinningScore));

	//----------------------------------------------------------------------------------------------------

	gamePhase = std::min(gamePhase, 64);
	assert((gamePhase >= 0) && (gamePhase <= 64));

	// Taper the score between the opening and the endgame
	//int totalOpeningScore = max(min(openingScoreMaterial[sideToMove] - openingScoreMaterial[sideToMove ^ 1], ceilingMaterial), floorMaterial);
	//int totalOpeningScore = openingScoreMaterial[sideToMove] - openingScoreMaterial[sideToMove ^ 1];
	int totalOpeningScore, totalEndgameScore;
	totalOpeningScore = totalEndgameScore = normalBrain.gameRecordPointer->totalMaterial[sideToMove] - normalBrain.gameRecordPointer->totalMaterial[sideToMove ^ 1];
	totalOpeningScore += openingScore[sideToMove] - openingScore[sideToMove ^ 1];
	//int totalEndgameScore = max(min(endgameScoreMaterial[sideToMove] - endgameScoreMaterial[sideToMove ^ 1], ceilingMaterial), floorMaterial);
	//int totalEndgameScore = endgameScoreMaterial[sideToMove] - endgameScoreMaterial[sideToMove ^ 1];
	totalEndgameScore += endgameScore[sideToMove] - endgameScore[sideToMove ^ 1];
	
	int openingPhase = gamePhase; // N.B. these must be 'int' NOT 'uint32_t' else it screws up the signed multiplication in the formula below!!!
	int endgamePhase = 64 - openingPhase;
	int result = ((totalOpeningScore * openingPhase + totalEndgameScore * endgamePhase) / 64);
	
	if (scaleFactor != 0)
		result = (result * scaleFactor) / 256;
	
	// 50-move tapering (KILLS TT upper/lower cutoffs as soon as it kicks in!)
	// TAKEN OUT AS SEEMS TO PROVIDE NO REAL BENEFIT IN A 20,000 GAME MATCH (Removing gives: -0.1, +/-3.6, 20000) AND CAUSES ISSUES WITH PROBLEM SOLVING
	//result = (result * max(100 - (GameRecordPointer - 1)->pliesSinceIrreversible, 0)) / 100;
	//if ((GameRecordPointer - 1)->pliesSinceIrreversible >= 36)
	//	if ((NodeCount & 15)< ((GameRecordPointer - 1)->pliesSinceIrreversible - 36))
	//	result = (result * max(64 - ((GameRecordPointer - 1)->pliesSinceIrreversible - 36), 0)) / 64;
	//if ((GameRecordPointer - 1)->pliesSinceIrreversible >= 68)
	//	result = (result * max(32 - ((GameRecordPointer - 1)->pliesSinceIrreversible - 68), 0)) / 32;
	//if ((GameRecordPointer - 1)->pliesSinceIrreversible >= 50)
	//{
	//	result = result / 2;
	//	if ((GameRecordPointer - 1)->pliesSinceIrreversible >= 75)
	//		result = result / 2;
	//}

	assert((result < EGTBWinningScore) && (result > -EGTBWinningScore));
	return result;
}

//----------------------------------------------------------------------------------------------------
// NN stuff

inline int ColossusPieceToNNUEPiece(int colossusPiece)
{
	return colossusPiece > 0 ? (7 - colossusPiece) : (13 + colossusPiece); // => WK=1...WP=6, BK=7...BP=12 - See NNCPU.H
}

//#define FLIP(x) (x ^ 56)
//#define FLIP(x) (x)

short Normal::EvaluateNN(int sideToMove, int epSquare)
{
	//// Normal
	////return Evaluate(sideToMove);

	//// Neural network
	//int pieces[33], squares[33], player, castle, fifty, move_number;
	//int NNUEindex = 2;

	////for (int square = A1; square <= H8; square++) // QUICKER TO DO THIS FROM THE BITBOARDS???
	////	if (NormalGenerate.mailboxBoard64[square] != Empty)
	////	{
	////		if (NormalGenerate.mailboxBoard64[square] == King)
	////		{
	////			pieces[0] = ColossusPieceToNNUEPiece(King);
	////			squares[0] = square;
	////		}
	////		else if (NormalGenerate.mailboxBoard64[square] == -King)
	////		{
	////			pieces[1] = ColossusPieceToNNUEPiece(-King);
	////			squares[1] = square;
	////		}
	////		else
	////		{
	////			pieces[NNUEindex] = ColossusPieceToNNUEPiece(NormalGenerate.mailboxBoard64[square]);
	////			squares[NNUEindex++] = square;
	////		}
	////	}

	//pieces[0] = 1; // ColossusPieceToNNUEPiece(King);
	//squares[0] = BitScanForwardX(normalBrain.piecesBB[0][King]);
	//pieces[1] = 7; // ColossusPieceToNNUEPiece(-King);
	//squares[1] = BitScanForwardX(normalBrain.piecesBB[1][King]);

	//uint64_t bb;

	//bb = normalBrain.piecesBB[0][Pawn];
	//while (bb)
	//{
	//	pieces[NNUEindex] = 6;
	//	squares[NNUEindex++] = BitScanForwardX(bb);
	//	ClearLS1B(bb);
	//}
	//bb = normalBrain.piecesBB[1][Pawn];
	//while (bb)
	//{
	//	pieces[NNUEindex] = 12;
	//	squares[NNUEindex++] = BitScanForwardX(bb);
	//	ClearLS1B(bb);
	//}

	//bb = normalBrain.piecesBB[0][Knight];
	//while (bb)
	//{
	//	pieces[NNUEindex] = 5;
	//	squares[NNUEindex++] = BitScanForwardX(bb);
	//	ClearLS1B(bb);
	//}
	//bb = normalBrain.piecesBB[1][Knight];
	//while (bb)
	//{
	//	pieces[NNUEindex] = 11;
	//	squares[NNUEindex++] = BitScanForwardX(bb);
	//	ClearLS1B(bb);
	//}

	//bb = normalBrain.piecesBB[0][Bishop];
	//while (bb)
	//{
	//	pieces[NNUEindex] = 4;
	//	squares[NNUEindex++] = BitScanForwardX(bb);
	//	ClearLS1B(bb);
	//}
	//bb = normalBrain.piecesBB[1][Bishop];
	//while (bb)
	//{
	//	pieces[NNUEindex] = 10;
	//	squares[NNUEindex++] = BitScanForwardX(bb);
	//	ClearLS1B(bb);
	//}

	//bb = normalBrain.piecesBB[0][Rook];
	//while (bb)
	//{
	//	pieces[NNUEindex] = 3;
	//	squares[NNUEindex++] = BitScanForwardX(bb);
	//	ClearLS1B(bb);
	//}
	//bb = normalBrain.piecesBB[1][Rook];
	//while (bb)
	//{
	//	pieces[NNUEindex] = 9;
	//	squares[NNUEindex++] = BitScanForwardX(bb);
	//	ClearLS1B(bb);
	//}

	//bb = normalBrain.piecesBB[0][Queen];
	//while (bb)
	//{
	//	pieces[NNUEindex] = 2;
	//	squares[NNUEindex++] = BitScanForwardX(bb);
	//	ClearLS1B(bb);
	//}
	//bb = normalBrain.piecesBB[1][Queen];
	//while (bb)
	//{
	//	pieces[NNUEindex] = 8;
	//	squares[NNUEindex++] = BitScanForwardX(bb);
	//	ClearLS1B(bb);
	//}


	//pieces[NNUEindex] = 0;
	//squares[NNUEindex] = 0;

	//if (epSquare != 0)
	//	squares[NNUEindex] = (epSquare >> 3) * 16 + (epSquare & 7);

	//return nnue_evaluate(sideToMove, pieces, squares);
	////return nncpu_evaluate(sideToMove, pieces, squares);
return 0;
}

short Normal::Evaluate(int sideToMove)
{
	//return EvaluateNN(sideToMove, 0);

	// Get the simple symmetrical part
	int result = EvaluateInner(sideToMove);


	//int result;
	//int kingSquare;
	//
	////THIS MAY BE SLIGHTLY ASSYMETRICAL BECAUSE OF EP CAPTURES!!!
	//kingSquare  = BitScanForwardX(normalBrain.piecesBB[sideToMove][King]);
	//normalBrain.CalculatePinnedPieces(sideToMove); // Required for legal move generation
	//result = normalBrain.CountAllMoves(sideToMove, normalBrain.IsEnemyKingAttacked(kingSquare, sideToMove ^ 1));

	//kingSquare = BitScanForwardX(normalBrain.piecesBB[sideToMove ^ 1][King]);
	//normalBrain.CalculatePinnedPieces(sideToMove ^ 1); // Required for legal move generation
	//result -= normalBrain.CountAllMoves(sideToMove ^ 1, normalBrain.IsEnemyKingAttacked(kingSquare, sideToMove));




	//// Add in any passed pawn runners
	//int passedPawnRunnerBonus = sideToMove ? -600 : 600;
	//if ((whiteMovesToPromote < blackMovesToPromote - 1) || ((whiteMovesToPromote < blackMovesToPromote) && (sideToMove == 0)))
	//{
	//	result += passedPawnRunnerBonus;
	//	//TEMP
	//	int kingSquare = (int)BitScanForwardX(normalBrain.piecesBB[1][King]);
	//	int r1, r2, r3, f1, f2, f3;
	//	r1 = whitePassedPawnSquare >> 3;
	//	f1 = whitePassedPawnSquare % 8;
	//	r2 = kingSquare >> 3;
	//	f2 = kingSquare % 8;
	//	int promotionSquare = 56 + f1;
	//	r3 = promotionSquare >> 3;
	//	f3 = promotionSquare % 8;
	//	int kingMovesToPromote = std::max(std::abs(r3 - r2), std::abs(f3 - f2));
	//	assert(kingMovesToPromote <= 7);
	//	assert(kingMovesToPromote > whiteMovesToPromote);
	//}
	//else if ((blackMovesToPromote < whiteMovesToPromote - 1) || ((blackMovesToPromote < whiteMovesToPromote) && (sideToMove == 1)))
	//{
	//	result -= passedPawnRunnerBonus;
	//	//TEMP
	//	int kingSquare = (int)BitScanForwardX(normalBrain.piecesBB[0][King]);
	//	int r1, r2, r3, f1, f2, f3;
	//	r1 = blackPassedPawnSquare >> 3;
	//	f1 = blackPassedPawnSquare % 8;
	//	r2 = kingSquare >> 3;
	//	f2 = kingSquare % 8;
	//	int promotionSquare = 0 + f1;
	//	r3 = promotionSquare >> 3;
	//	f3 = promotionSquare % 8;
	//	int kingMovesToPromote = std::max(std::abs(r3 - r2), std::abs(f3 - f2));
	//	assert(kingMovesToPromote <= 7);
	//	assert(kingMovesToPromote > blackMovesToPromote);
	//}

	//// Evaluation grain SEEMS TO LOSE ELO!
	//result = (result / 8) * 8;

	// Add in the tempo
	result += Tempo;

	return (short)result;
}

//----------------------------------------------------------------------------------------------------

void Normal::StaticEvaluation()
{
	short score;

	normalBrain.CopyFrom(&EngineBrain);

	ConvertMailboxBoard64ToPiecesBB(normalBrain.mailboxBoard64, normalBrain.piecesBB);
	InitialiseMaterialValues(normalBrain.mailboxBoard64, normalBrain.gameRecordPointer);
	InitialisePSTValues(normalBrain.mailboxBoard64, normalBrain.gameRecordPointer);
	InitialiseGamePhase(normalBrain.mailboxBoard64, normalBrain.gameRecordPointer);
	Output("info string normalBrain.gameRecordPointer->totalMaterial[0] = " + MyITOA(normalBrain.gameRecordPointer->totalMaterial[0]));
	Output("info string normalBrain.gameRecordPointer->totalMaterial[1] = " + MyITOA(normalBrain.gameRecordPointer->totalMaterial[1]));
	Output("info string normalBrain.gameRecordPointer->gamePhase[0] = " + MyITOA(normalBrain.gameRecordPointer->gamePhase[0]));
	Output("info string normalBrain.gameRecordPointer->gamePhase[1] = " + MyITOA(normalBrain.gameRecordPointer->gamePhase[1]));

	score = Evaluate(0);
	Output("info string Static Evaluation WTM = " + MyITOA(score));
	score = Evaluate(1);
	Output("info string Static Evaluation BTM = " + MyITOA(score));
}

// Generate and evaluate a huge number of random symmetrical KP positions whose evaluation should be zero!
void Normal::TestSymmetry0()
{
	int square, rank, file, errors = 0;
	short score0, score1;

	StartClock = std::chrono::steady_clock::now();

	normalBrain.CopyFrom(&EngineBrain);

	normalBrain.gameRecordPointer = &normalBrain.gameRecord[2];

	for (int count = 1; count <= 10000000; count++)
	{
		ClearMailboxBoard64(normalBrain.mailboxBoard64);

		for (int j = 0; j < 8; j++)
		{
			square = BoardRand0To63();
			if ((square >= A2) && (square <= H7))
			{
				normalBrain.mailboxBoard64[square] = Pawn;
				rank = square / 8;
				file = square % 8;
				normalBrain.mailboxBoard64[(7 - rank) * 8 + file] = -Pawn;
			}
		}

		square = BoardRand0To63();
		normalBrain.mailboxBoard64[square] = King;
		rank = square / 8;
		file = square % 8;
		normalBrain.mailboxBoard64[(7 - rank) * 8 + file] = -King;

		ConvertMailboxBoard64ToPiecesBB(normalBrain.mailboxBoard64, normalBrain.piecesBB);
		InitialiseMaterialValues(normalBrain.mailboxBoard64, normalBrain.gameRecordPointer);
		InitialisePSTValues(normalBrain.mailboxBoard64, normalBrain.gameRecordPointer);
		InitialiseGamePhase(normalBrain.mailboxBoard64, normalBrain.gameRecordPointer);
		normalBrain.gameRecord[2].castlingStatus.ui8[0][0] = 1;
		normalBrain.gameRecord[2].castlingStatus.ui8[0][1] = 1;
		normalBrain.gameRecord[2].castlingStatus.ui8[1][0] = 1;
		normalBrain.gameRecord[2].castlingStatus.ui8[1][1] = 1;
		if (normalBrain.mailboxBoard64[E1] == King)
		{
			if (normalBrain.mailboxBoard64[H1] == Rook)
				normalBrain.gameRecord[2].castlingStatus.ui8[0][0] = 0;
			if (normalBrain.mailboxBoard64[A1] == Rook)
				normalBrain.gameRecord[2].castlingStatus.ui8[0][1] = 0;
		}
		if (normalBrain.mailboxBoard64[E8] == -King)
		{
			if (normalBrain.mailboxBoard64[H8] == -Rook)
				normalBrain.gameRecord[2].castlingStatus.ui8[1][0] = 0;
			if (normalBrain.mailboxBoard64[A8] == -Rook)
				normalBrain.gameRecord[2].castlingStatus.ui8[1][1] = 0;
		}

		//WriteMailboxBoard64(normalBrain.mailboxBoard64);
		score0 = EvaluateInner(0);
		score1 = EvaluateInner(1);

		if ((score0 != 0) || (score1 != 0))
		{
			errors++;
			Output("info string *** ERROR:" + MyITOA(score0) + "," + MyITOA(score1));
			WriteMailboxBoard64(&normalBrain);
		}
	}

	Output("info string time = " + MyUI64TOA(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - StartClock).count()));
	Output("info string errors = " + MyITOA(errors));
}

// Generate and evaluate a huge number of random symmetrical positions whose evaluation should be zero!
void Normal::TestSymmetry1()
{
	int square, rank, file, errors = 0;
	short score0, score1;

	StartClock = std::chrono::steady_clock::now();

	normalBrain.CopyFrom(&EngineBrain);

	normalBrain.gameRecordPointer = &normalBrain.gameRecord[2];

	for (int count = 1; count <= 10000000; count++)
	{
		ClearMailboxBoard64(normalBrain.mailboxBoard64);

		for (int j = 0; j < 2; j++)
		{
			square = BoardRand0To63();
			normalBrain.mailboxBoard64[square] = Knight;
			rank = square / 8;
			file = square % 8;
			normalBrain.mailboxBoard64[(7 - rank) * 8 + file] = -Knight;
		}

		for (int j = 0; j < 2; j++)
		{
			square = BoardRand0To63();
			normalBrain.mailboxBoard64[square] = Bishop;
			rank = square / 8;
			file = square % 8;
			normalBrain.mailboxBoard64[(7 - rank) * 8 + file] = -Bishop;
		}

		for (int j = 0; j < 2; j++)
		{
			square = BoardRand0To63();
			normalBrain.mailboxBoard64[square] = Rook;
			rank = square / 8;
			file = square % 8;
			normalBrain.mailboxBoard64[(7 - rank) * 8 + file] = -Rook;
		}

		for (int j = 0; j < 1; j++)
		{
			square = BoardRand0To63();
			normalBrain.mailboxBoard64[square] = Queen;
			rank = square / 8;
			file = square % 8;
			normalBrain.mailboxBoard64[(7 - rank) * 8 + file] = -Queen;
		}

		for (int j = 0; j < 8; j++)
		{
			square = BoardRand0To63();
			if ((square >= A2) && (square <= H7))
			{
				normalBrain.mailboxBoard64[square] = Pawn;
				rank = square / 8;
				file = square % 8;
				normalBrain.mailboxBoard64[(7 - rank) * 8 + file] = -Pawn;
			}
		}

		square = BoardRand0To63();
		normalBrain.mailboxBoard64[square] = King;
		rank = square / 8;
		file = square % 8;
		normalBrain.mailboxBoard64[(7 - rank) * 8 + file] = -King;

		ConvertMailboxBoard64ToPiecesBB(normalBrain.mailboxBoard64, normalBrain.piecesBB);
		InitialiseMaterialValues(normalBrain.mailboxBoard64, normalBrain.gameRecordPointer);
		InitialisePSTValues(normalBrain.mailboxBoard64, normalBrain.gameRecordPointer);
		InitialiseGamePhase(normalBrain.mailboxBoard64, normalBrain.gameRecordPointer);
		normalBrain.gameRecord[2].castlingStatus.ui8[0][0] = 1;
		normalBrain.gameRecord[2].castlingStatus.ui8[0][1] = 1;
		normalBrain.gameRecord[2].castlingStatus.ui8[1][0] = 1;
		normalBrain.gameRecord[2].castlingStatus.ui8[1][1] = 1;
		if (normalBrain.mailboxBoard64[E1] == King)
		{
			if (normalBrain.mailboxBoard64[H1] == Rook)
				normalBrain.gameRecord[2].castlingStatus.ui8[0][0] = 0;
			if (normalBrain.mailboxBoard64[A1] == Rook)
				normalBrain.gameRecord[2].castlingStatus.ui8[0][1] = 0;
		}
		if (normalBrain.mailboxBoard64[E8] == -King)
		{
			if (normalBrain.mailboxBoard64[H8] == -Rook)
				normalBrain.gameRecord[2].castlingStatus.ui8[1][0] = 0;
			if (normalBrain.mailboxBoard64[A8] == -Rook)
				normalBrain.gameRecord[2].castlingStatus.ui8[1][1] = 0;
		}

		//WriteMailboxBoard64(normalBrain.mailboxBoard64);
		score0 = EvaluateInner(0);
		score1 = EvaluateInner(1);

		if ((score0 != 0) || (score1 != 0))
		{
			errors++;
			Output("info string *** ERROR:" + MyITOA(score0) + "," + MyITOA(score1));
			WriteMailboxBoard64(&normalBrain);
		}
	}

	Output("info string time = " + MyUI64TOA(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - StartClock).count()));
	Output("info string errors = " + MyITOA(errors));
}

// Generate and evaluate a huge number of random positions whose evaluation should be the negative of their mirrored position!
void Normal::TestSymmetry2()
{
	int square, errors = 0;
	short score1, score2;

	StartClock = std::chrono::steady_clock::now();

	normalBrain.CopyFrom(&EngineBrain);

	normalBrain.gameRecordPointer = &normalBrain.gameRecord[2];

	for (int count = 1; count <= 10000000; count++)
	{
		ClearMailboxBoard64(normalBrain.mailboxBoard64);

		for (int j = 0; j < 2; j++)
		{
			square = BoardRand0To63();
			normalBrain.mailboxBoard64[square] = Knight;
		}

		for (int j = 0; j < 2; j++)
		{
			square = BoardRand0To63();
			normalBrain.mailboxBoard64[square] = -Knight;
		}

		for (int j = 0; j < 2; j++)
		{
			square = BoardRand0To63();
			normalBrain.mailboxBoard64[square] = Bishop;
		}

		for (int j = 0; j < 2; j++)
		{
			square = BoardRand0To63();
			normalBrain.mailboxBoard64[square] = -Bishop;
		}

		for (int j = 0; j < 2; j++)
		{
			square = BoardRand0To63();
			normalBrain.mailboxBoard64[square] = Rook;
		}

		for (int j = 0; j < 2; j++)
		{
			square = BoardRand0To63();
			normalBrain.mailboxBoard64[square] = -Rook;
		}

		for (int j = 0; j < 1; j++)
		{
			square = BoardRand0To63();
			normalBrain.mailboxBoard64[square] = Queen;
		}

		for (int j = 0; j < 1; j++)
		{
			square = BoardRand0To63();
			normalBrain.mailboxBoard64[square] = -Queen;
		}

		for (int j = 0; j < 8; j++)
		{
			square = BoardRand0To63();
			if ((square >= A2) && (square <= H7))
				normalBrain.mailboxBoard64[square] = Pawn;
		}

		for (int j = 0; j < 8; j++)
		{
			square = BoardRand0To63();
			if ((square >= A2) && (square <= H7))
				normalBrain.mailboxBoard64[square] = -Pawn;
		}

		square = BoardRand0To63();
		normalBrain.mailboxBoard64[square] = King;

		do
		{
			square = BoardRand0To63();
		} while (normalBrain.mailboxBoard64[square] == King);
		normalBrain.mailboxBoard64[square] = -King;


		ConvertMailboxBoard64ToPiecesBB(normalBrain.mailboxBoard64, normalBrain.piecesBB);
		InitialiseMaterialValues(normalBrain.mailboxBoard64, normalBrain.gameRecordPointer);
		InitialisePSTValues(normalBrain.mailboxBoard64, normalBrain.gameRecordPointer);
		InitialiseGamePhase(normalBrain.mailboxBoard64, normalBrain.gameRecordPointer);
		normalBrain.gameRecord[2].castlingStatus.ui8[0][0] = 1;
		normalBrain.gameRecord[2].castlingStatus.ui8[0][1] = 1;
		normalBrain.gameRecord[2].castlingStatus.ui8[1][0] = 1;
		normalBrain.gameRecord[2].castlingStatus.ui8[1][1] = 1;
		if (normalBrain.mailboxBoard64[E1] == King)
		{
			if (normalBrain.mailboxBoard64[H1] == Rook)
				normalBrain.gameRecord[2].castlingStatus.ui8[0][0] = 0;
			if (normalBrain.mailboxBoard64[A1] == Rook)
				normalBrain.gameRecord[2].castlingStatus.ui8[0][1] = 0;
		}
		if (normalBrain.mailboxBoard64[E8] == -King)
		{
			if (normalBrain.mailboxBoard64[H8] == -Rook)
				normalBrain.gameRecord[2].castlingStatus.ui8[1][0] = 0;
			if (normalBrain.mailboxBoard64[A8] == -Rook)
				normalBrain.gameRecord[2].castlingStatus.ui8[1][1] = 0;
		}

		score1 = Evaluate(0);

		// Reflect the position
		for (int rank = 1; rank <= 4; rank++)
			for (int file = 1; file <= 8; file++)
			{
				int piece;
				piece = normalBrain.mailboxBoard64[(rank - 1) * 8 + file - 1];
				normalBrain.mailboxBoard64[(rank - 1) * 8 + file - 1] = -normalBrain.mailboxBoard64[(8 - rank) * 8 + file - 1];
				normalBrain.mailboxBoard64[(8 - rank) * 8 + file - 1] = -piece;
			}

		ConvertMailboxBoard64ToPiecesBB(normalBrain.mailboxBoard64, normalBrain.piecesBB);
		InitialiseMaterialValues(normalBrain.mailboxBoard64, normalBrain.gameRecordPointer);
		InitialisePSTValues(normalBrain.mailboxBoard64, normalBrain.gameRecordPointer);
		InitialiseGamePhase(normalBrain.mailboxBoard64, normalBrain.gameRecordPointer);
		normalBrain.gameRecord[2].castlingStatus.ui8[0][0] = 1;
		normalBrain.gameRecord[2].castlingStatus.ui8[0][1] = 1;
		normalBrain.gameRecord[2].castlingStatus.ui8[1][0] = 1;
		normalBrain.gameRecord[2].castlingStatus.ui8[1][1] = 1;
		if (normalBrain.mailboxBoard64[E1] == King)
		{
			if (normalBrain.mailboxBoard64[H1] == Rook)
				normalBrain.gameRecord[2].castlingStatus.ui8[0][0] = 0;
			if (normalBrain.mailboxBoard64[A1] == Rook)
				normalBrain.gameRecord[2].castlingStatus.ui8[0][1] = 0;
		}
		if (normalBrain.mailboxBoard64[E8] == -King)
		{
			if (normalBrain.mailboxBoard64[H8] == -Rook)
				normalBrain.gameRecord[2].castlingStatus.ui8[1][0] = 0;
			if (normalBrain.mailboxBoard64[A8] == -Rook)
				normalBrain.gameRecord[2].castlingStatus.ui8[1][1] = 0;
		}

		score2 = Evaluate(1);

		if (score1 != score2)
		{
			errors++;
			Output("info string *** ERROR:" + MyITOA(score1) + " != " + MyITOA(score2));
			WriteMailboxBoard64(&normalBrain);
		}
	}

	Output("info string time = " + MyUI64TOA(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - StartClock).count()));
	Output("info string errors = " + MyITOA(errors));
}

//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
// TEXEL tuning stuff

#include <stdio.h>
#include <stdlib.h>
#include "Engine.h"

//struct Position_Struct
//{
//	int8_t NormalGenerate.mailboxBoard64[64];
//	uint64_t NormalGenerate.piecesBB[Sides][King + 2];
//	int sideToMove;
//	double result;
//};
//Position_Struct positions[7600000];
//
//double Sigmoid[65536];
//
//const int TotalParameters = 792;// 776;// 640;// 128;// 32;// 192;// 768;
//
//void ReadParametersFromFile(int iteration)
//{
//	FILE *ParametersFile;
//	int bufferLength = 255;
//	char buffer[255];
//	std::string filename;
//
//	filename = "S:\\Chess\\GameLibraries\\TEXEL\\tuner\\parameters" + MyITOA(iteration) + ".txt";
//	fopen_s(&ParametersFile, filename.c_str(), "r");
//
//	int parameterIndex = 0;
//
//	// Material
//	for (int index = 0; index < 2; index++)
//	{
//		fscanf(ParametersFile, "%d,%d,%d,%d,%d,%d,\n", &parameters[parameterIndex + 0], &parameters[parameterIndex + 1], &parameters[parameterIndex + 2], &parameters[parameterIndex + 3], &parameters[parameterIndex + 4], &parameters[parameterIndex + 5]);
//		parameterIndex += 6;
//	}
//
//	// PSTs
//	for (int index = 0; index < 6 * 8 * 2; index++)
//	{
//		fscanf(ParametersFile, "%d,%d,%d,%d,%d,%d,%d,%d,\n", &parameters[parameterIndex + 0], &parameters[parameterIndex + 1], &parameters[parameterIndex + 2], &parameters[parameterIndex + 3], &parameters[parameterIndex + 4], &parameters[parameterIndex + 5], &parameters[parameterIndex + 6], &parameters[parameterIndex + 7]);
//		parameterIndex += 8;
//	}
//
//	// Miscellaneous
//	for (int index = 0; index < 6; index++)
//	{
//		fscanf(ParametersFile, "%d,%d,\n", &parameters[parameterIndex + 0], &parameters[parameterIndex + 1]);
//		parameterIndex += 2;
//	}
//
//
//	fclose(ParametersFile);
//}
//
//void WriteParametersToFile(int iteration)
//{
//	FILE *ParametersFile;
//	int bufferLength = 255;
//	char buffer[255];
//	std::string filename;
//	std::string debug = "";
//#ifdef _DEBUG
//	debug = "DEBUG";
//#endif
//
//	filename = "S:\\Chess\\GameLibraries\\TEXEL\\tuner\\parameters" + debug + MyITOA(iteration) + ".txt";
//	//filename = "S:\\Chess\\GameLibraries\\TEXEL\\tuner\\parametersBP" + debug + MyITOA(iteration) + ".txt";
//
//	//fopen_s(&ParametersFile, filename.c_str(), "w");
//
//	//for (int index = 0; index < TotalParameters; index++)
//	//	fprintf(ParametersFile, "%d\n", parameters[index]);
//
//	//fclose(ParametersFile);
//
//	//filename = "S:\\Chess\\GameLibraries\\TEXEL\\tuner\\parameters" + debug + MyITOA(iteration) + "Formatted.txt";
//	//filename = "S:\\Chess\\GameLibraries\\TEXEL\\tuner\\parametersBP" + debug + MyITOA(iteration) + "Formatted.txt";
//
//	fopen_s(&ParametersFile, filename.c_str(), "w");
//
//	for (int index = 0; index < 12; index++)
//	{
//		fprintf(ParametersFile, "%4d,", parameters[index]);
//		if (index % 6 == 5)
//			fprintf(ParametersFile, "\n");
//		if (index % 12 == 11)
//			fprintf(ParametersFile, "\n");
//	}
//
//	for (int index = 12; index < 780; index++)
//	{
//		fprintf(ParametersFile, "%4d,", parameters[index]);
//		if ((index-12) % 8 == 7)
//			fprintf(ParametersFile, "\n");
//		if ((index-12) % 64 == 63)
//			//if (index % 16 == 15)
//			fprintf(ParametersFile, "\n");
//	}
//
//	for (int index = 780; index < TotalParameters; index++)
//	{
//		fprintf(ParametersFile, "%4d,", parameters[index]);
//		if (index % 2 == 1)
//			fprintf(ParametersFile, "\n");
//	}
//
//
//	//for (int piece = 0; piece < 6; piece++)
//	//{
//	//	for (int square = 0; square < 64; square++)
//	//	{
//	//		int mSquare = square ^ 56;
//	//		fprintf(ParametersFile, "%4d,", parameters[(piece * 32) + 0 + (mSquare / 8)] + parameters[(piece * 32) + 8 + (mSquare % 8)]);
//	//		if (square % 8 == 7)
//	//			fprintf(ParametersFile, "\n");
//	//		if (square % 64 == 63)
//	//			fprintf(ParametersFile, "\n");
//	//	}
//
//	//	for (int square = 0; square < 64; square++)
//	//	{
//	//		int mSquare = square ^ 56;
//	//		fprintf(ParametersFile, "%4d,", parameters[(piece * 32) + 16 + (mSquare / 8)] + parameters[(piece * 32) + 24 + (mSquare % 8)]);
//	//		if (square % 8 == 7)
//	//			fprintf(ParametersFile, "\n");
//	//		if (square % 64 == 63)
//	//			fprintf(ParametersFile, "\n");
//	//	}
//	//	fprintf(ParametersFile, "\n");
//	//}
//
//	fclose(ParametersFile);
//
//}
//
//double TotalError(std::string* FENs, int totalFENs)
//{
//	short score;
//	double totalError, sigmoid, result, f;
//	int tokenCount;
//
//	// Scan over all positions in the testset
//	totalError = 0;
//	for (int fen = 0; fen < totalFENs; fen++)
//	{
//		// Get the next position
//		memcpy(NormalGenerate.mailboxBoard64, positions[fen].NormalGenerate.mailboxBoard64, 64);
//		memcpy(NormalGenerate.piecesBB, positions[fen].NormalGenerate.piecesBB, 2 * 8 * 8);
//		SideToMove = positions[fen].sideToMove;
//		result = positions[fen].result;
//		
//		//if ((NormalGenerate.mailboxBoard64[G8] == King) || (NormalGenerate.mailboxBoard64[G1] == -King))
//		//	continue;
//
//		// Evaluate the position (relative to white)
//		score = Evaluate(SideToMove) - Tempo;
//		if (SideToMove)
//			score = -score;
//
//		// Accumulate the total error
//		//sigmoid = 1.0 / (1.0 + pow(10.0, -1.0*((double)score) / 400.0));
//		sigmoid = Sigmoid[score + 32767];
//		f = result - sigmoid;
//		totalError += (f * f);
//	}
//	totalError = totalError / totalFENs;
//
//	return totalError;
//}
//
////----------------------------------------------------------------------------------------------------
//
//void TexelTuning()
//{ //AVOID MICRO TUNING THE WRONG TABLE BY SKIPPING ERRORS BELOW SOME MARGIN
//	// CAN WE SOMEHOW 'JUST' TUNE POSNS WITH RELEVANT PIECE PLACINGS E.G. IF TUNING K ON H8, JUST SCORE POSNS WITH K ON H8!
//	// MAKE PSTs UP OF RANK AND FILE VALUES!
//  //does putting the mat values in the PSTs make them inflationary ? ? ?
//  //I think NOT having an anchor makes it inflationary ? !
//	
//	//Adjust();
//	//ReadParametersFromFile(0);
//	//WriteParametersToFile(0);
//
//	// Initialise the sigmoid lookup table
//	for (int i = -32767; i <= 32767; i++)
//		Sigmoid[i + 32767] = (double)1.0 / (1.0 + pow(10.0, -1.13*((double)i) / 400.0));
//
//	FILE *FENFile;
//	std::string* FENs = new std::string[7600000];
//	int totalFENs;
//	int bufferLength = 255;
//	char buffer[255];
//
//	//----------------------------------------------------------------------------------------------------
//
//	// Read the FEN strings from file into array
//	totalFENs = 0;
//	//fopen_s(&FENFile, "S:\\Chess\\GameLibraries\\TEXEL\\tuner\\ccrl-40-15-elo-3000.epd", "r");
//	//fopen_s(&FENFile, "S:\\Chess\\GameLibraries\\TEXEL\\tuner\\ccrl-40-15-elo-3000SUBSET1000000.epd", "r");
//	//fopen_s(&FENFile, "S:\\Chess\\GameLibraries\\TEXEL\\tuner\\quiet-labeled.epd", "r");
//	fopen_s(&FENFile, "S:\\Chess\\GameLibraries\\TEXEL\\tuner\\quiet-labeled.v7.epd", "r");
//	//fopen_s(&FENFile, "S:\\Chess\\GameLibraries\\TEXEL\\tuner\\quiet-labeledSUBSET50000.epd", "r");
//	//fopen_s(&FENFile, "S:\\Chess\\GameLibraries\\TEXEL\\tuner\\quiet-labeledSUBSET1000.epd", "r");
//	while (fgets(buffer, bufferLength, FENFile))
//	{
//		FENs[totalFENs] = buffer;
//	
//		std::string tokens[10];
//		int tokenCount;
//		double result;
//		Split(FENs[totalFENs], &tokens[0], &tokenCount, " \t");
//		//if (tokens[7] != "-")
//		//	continue;
//
//		ConvertFENToPosition(tokens[0], tokens[1], tokens[2], tokens[3]);
//
//		//if (CalculateGamePhase() <=8)
//		//	continue;
//
//		// Save the position for analysis later
//		for (int square = 0; square < 64; square++)
//			positions[totalFENs].NormalGenerate.mailboxBoard64[square] = NormalGenerate.mailboxBoard64[square];
//		ConvertNormalGenerate.mailboxBoard64ToBitBoard(NormalGenerate.mailboxBoard64, NormalGenerate.piecesBB);
//		for (int side = 0; side < 2; side++)
//			for (int piece = 0; piece < King + 2; piece++)
//				positions[totalFENs].NormalGenerate.piecesBB[side][piece] = NormalGenerate.piecesBB[side][piece];
//		positions[totalFENs].sideToMove = SideToMove;
//		if ((tokens[5] == "\"1/2-1/2\";\n") || (tokens[8] == "pgn=0.5"))
//			result = 0.5;
//		else
//		{
//			if (tokens[5] == "\"0-1\";\n")
//				result = 0.0;
//			else if (tokens[5] == "\"1-0\";\n")
//				result = 1.0;
//			else if (tokens[8] == "pgn=0.0")
//				result = (SideToMove == 0 ? 0.0 : 1.0);
//			else if (tokens[8] == "pgn=1.0")
//				result = (SideToMove == 0 ? 1.0 : 0.0);
//			else
//				printf("*** Unknown result! %s %s", tokens[5], tokens[8]);
//		}
//		positions[totalFENs].result = result;
//
//		totalFENs++;
//	}
//	fclose(FENFile);
//	printf("%d FEN strings read into array\n", totalFENs);
//
//	//----------------------------------------------------------------------------------------------------
//
//	// Get the initial error
//	ReadParametersFromFile(0);
//	double initialTotalError = TotalError(FENs, totalFENs);
//	printf("Initial total error: %.12f\n", initialTotalError);
//
//	
//	// Tune the parameters until no more improvements
//	int parameterDelta[TotalParameters];
//	for (int i = 0; i < TotalParameters; i++)
//		parameterDelta[i] = 1;// 9;// 81;
//	double lowestTotalError = initialTotalError;
//	int iteration = 0;
//	int improvements;
//	do
//	{
//		improvements = 0;
//		iteration++;
//		printf("\nIteration %d\n", iteration);
//
//		// Loop through all parameters
//		//for (int parameter = 10 * 64; parameter < TotalParameters - 64; parameter++)
//		//for (int parameter = 0; parameter < TotalParameters; parameter++)
//		//for (int parameter = 128; parameter < 640; parameter++)
//		//for (int parameter = 774; parameter < 776; parameter++)
//		//for (int parameter = 776; parameter < 780; parameter++)
//		//for (int parameter = 0; parameter < 12; parameter++)
//		//for (int parameter = 0; parameter < 12; parameter++)
//		//for (int parameter = 12 + 128 * 5; parameter < 12 + 128 * 6; parameter++)
//		for (int parameter = 0; parameter < 12 + 128 * 6; parameter++)
//		{
//			//if ((parameter == 5) || (parameter == 11))
//			//	continue;
//			//if ((parameter >= 12 + 56) && (parameter < 12 + 56 + 8 + 8))
//			//	continue;
//			
//			printf("%d ", parameter);
//
//			double currentTotalErrorO, currentTotalErrorE;
//
//			//CHECK THE O PARAMETER AND THE E PARAMETER (+64) AND ONLY CHANGE THE ONE THAT GIVES THE GREATEST BENEFIT
//			//WOULD THIS JUST DEFER THE 'WRONG END' CHANGES TO LAST??? PROBABLY FEWER THOUGH.
//			//TESTING: JUST DO ONE SQ! :)
//
//			int initialParameterValueO = parameters[parameter];
//			//int initialParameterValueE = parameters[parameter+16];
//			int updatedParameterValueO, updatedParameterValueE;
//
//		retry:
//			parameters[parameter] = initialParameterValueO + parameterDelta[parameter];
//			updatedParameterValueO = parameters[parameter];
//			currentTotalErrorO = TotalError(FENs, totalFENs);
//			parameters[parameter] = initialParameterValueO;
//
//			//parameters[parameter + 16] = initialParameterValueE + parameterDelta[parameter + 16];
//			//updatedParameterValueE = parameters[parameter + 16];
//			//currentTotalErrorE = TotalError(FENs, totalFENs);
//			//parameters[parameter + 16] = initialParameterValueE;
//
//			if ((currentTotalErrorO >= lowestTotalError) )//&& (currentTotalErrorE >= lowestTotalError))
//			{
//				parameters[parameter] = initialParameterValueO - parameterDelta[parameter];
//				updatedParameterValueO = parameters[parameter];
//				currentTotalErrorO = TotalError(FENs, totalFENs);
//				parameters[parameter] = initialParameterValueO;
//
//				//parameters[parameter + 16] = initialParameterValueE - parameterDelta[parameter + 16];
//				//updatedParameterValueE = parameters[parameter + 16];
//				//currentTotalErrorE = TotalError(FENs, totalFENs);
//				//parameters[parameter + 16] = initialParameterValueE;
//			}
//
//			if ((currentTotalErrorO < lowestTotalError) )//|| (currentTotalErrorE < lowestTotalError))
//			{
//				improvements++;
//				lowestTotalError = currentTotalErrorO;// min(currentTotalErrorO, currentTotalErrorE);
//				//if (currentTotalErrorO < currentTotalErrorE)
//					parameters[parameter] = updatedParameterValueO;
//				//else
//					//parameters[parameter + 16] = updatedParameterValueE;
//			}
//			else
//			{
//				if (parameterDelta[parameter] > 1)
//				{
//					parameterDelta[parameter] = parameterDelta[parameter] / 3;
//					goto retry;
//				}
//			}
//		}
//
//		printf("\n");
//		printf("Lowest error %.12f\n", lowestTotalError);
//		printf("Improvements %d\n", improvements);
//		WriteParametersToFile(iteration);
//
//	} while (improvements > 0);
//
//	printf("Done\n");
//}
//
////----------------------------------------------------------------------------------------------------

//int CalculateGamePhase()
//{
//	int gamePhase = -2, whitePawns = 0, blackPawns = 0;
//
//	for (int square = A1; square <= H8; square++)
//	{
//		int piece = NormalGenerate.mailboxBoard64[square];
//		if (piece != Empty)
//		{
//			if (piece == Pawn)
//				whitePawns++;
//			else if (piece == -Pawn)
//				blackPawns++;
//			else
//				gamePhase++;
//			//if (piece < 0)
//			//	piece = -piece;
//			//piece--; // 'piece' now in the range 0 (pawn) - 5 (king)
//			//gamePhase += gamePhaseIncrement[piece];
//		}
//	}
//	if (whitePawns > 4)
//		gamePhase++;
//	if (blackPawns > 4)
//		gamePhase++;
//
//	gamePhase = min(gamePhase, 16);
//	assert((gamePhase >= 0) && (gamePhase <= 16));
//	return gamePhase;
//}

//void TexelTuningStats()
//{
//	//for (int i = -8; i <= 8; i++)
//	//	printf("%f ", ((double)1.0 / (1.0 + exp(-i))) * 256);
//	//printf("\n\n");
//
//
//
//	FILE *FENFile;
//	std::string* FENs = new std::string[10000000];
//	int totalFENs;
//	int bufferLength = 255;
//	char buffer[255];
//	int weirdKings = 0;
//
//	//int wdlOccupancy[12][64][3];
//	//memset(wdlOccupancy, 0, 12 * 64 * 3 * 4);
//	int phaseOccupancy[17][6][64];
//	memset(phaseOccupancy, 0, 17 * 6 * 64 * 4);
//	uint32_t phaseOccupancyCounts[17][6][64];
//	memset(phaseOccupancyCounts, 0, 17 * 6 * 64 * 4);
//	int gamePhaseCounts[17];
//	memset(gamePhaseCounts, 0, 17 * 4);
//	int wdl[3] = { 0,0,0 }; // WW/Draw/BW
//
//	// Read the FEN strings from file into array
//	totalFENs = 0;
//	fopen_s(&FENFile, "S:\\Chess\\GameLibraries\\TEXEL\\tuner\\ccrl-40-15-elo-3000.epd", "r");
//	//fopen_s(&FENFile, "S:\\Chess\\GameLibraries\\TEXEL\\tuner\\quiet-labeled.epd", "r");
//	//fopen_s(&FENFile, "S:\\Chess\\GameLibraries\\TEXEL\\tuner\\quiet-labeled.v7.epd", "r");
//	//fopen_s(&FENFile, "S:\\Chess\\GameLibraries\\TEXEL\\tuner\\quiet-labeledSUBSET50000.epd", "r");
//	while (fgets(buffer, bufferLength, FENFile))
//	{
//		FENs[totalFENs] = buffer;
//
//		std::string tokens[10];
//		int tokenCount;
//		//double result;
//		Split(FENs[totalFENs], &tokens[0], &tokenCount, " \t");
//		ConvertFENToPosition(tokens[0], tokens[1], tokens[2], tokens[3], tokens[4], tokens[5]);
//
//		//if (CalculateGamePhase() <= 8)
//		//	continue;
//
//
//
//		float result;
//		if ((tokens[5] == "\"1/2-1/2\";\n") || (tokens[8] == "pgn=0.5"))
//			result = 0.5;
//		else
//		{
//			if (tokens[5] == "\"0-1\";\n")
//				result = 0.0;
//			else if (tokens[5] == "\"1-0\";\n")
//				result = 1.0;
//			else if (tokens[8] == "pgn=0.0")
//				result = (SideToMove == 0 ? 0.0 : 1.0);
//			else if (tokens[8] == "pgn=1.0")
//				result = (SideToMove == 0 ? 1.0 : 0.0);
//			else
//				printf("*** Unknown result! %s %s", tokens[5], tokens[8]);
//		}
//
//		if (result == 1.0)
//			wdl[0]++;
//		else if (result == 0.5)
//			wdl[1]++;
//		else
//			wdl[2]++;
//
//
//
//
//
//		int gamePhase = CalculateGamePhase();
//		gamePhaseCounts[gamePhase]++;
//
//		for (int square = 0; square < 64; square++)
//		{
//			int piece = NormalGenerate.mailboxBoard64[square];
//			if (piece != 0)
//			{
//				int pieceSaved = piece;
//
//				if (piece < 0)
//					piece = -piece;// +6;
//				piece--; // 0-5//11
//				
//				//wdlOccupancy[piece][square][r]++;
//				//if (piece >= 6)
//					//piece = piece - 6;
//
//				if (pieceSaved > 0)
//				{
//					if (result == 1.0)
//						phaseOccupancy[gamePhase][piece][square] += 2; // COULD INSTEAD ADD ANOTHER DIMENSION(S) [OR MAKE IT AN ARRAY OF SOME STRUCTURE] TO TALLY UP WDLs
//					if (result == 0.5)
//						phaseOccupancy[gamePhase][piece][square] += 1;
//
//					phaseOccupancyCounts[gamePhase][piece][square]++;
//
//					//if (pieceSaved == 6)
//					//	if (square == 14)
//					//		if (gamePhase == 16)
//					//			if (result == 1.0)
//					//				printf("%s", buffer);
//				}
//				else
//				{
//					if (result == 0.0)
//						phaseOccupancy[gamePhase][piece][square ^ 56] += 2;
//					if (result == 0.5)
//						phaseOccupancy[gamePhase][piece][square ^ 56] += 1;
//
//					phaseOccupancyCounts[gamePhase][piece][square ^ 56]++;
//				}
//
//
//
//			}
//		}
//
//		//if (gamePhase==0)
//		//	printf("ZERO! %s", FENs[totalFENs].c_str());
//
//		//if ((NormalGenerate.mailboxBoard64[D7] == King) || (NormalGenerate.mailboxBoard64[D2] == -King))
//		//	if (gamePhase>=9)
//		//{
//		//	printf("%s", FENs[totalFENs].c_str());
//		//	weirdKings++;
//		//}
//
//		totalFENs++;
//	}
//	fclose(FENFile);
//	//printf("weirdKings = %d \n", weirdKings);
//	printf("%d FEN strings read into array\n", totalFENs);
//
//	printf("WDL: %d %d %d\n\n", wdl[0], wdl[1], wdl[2]);
//
//	int gpO = 0, gpE = 0, gpT = 0;
//	for (int i = 0; i <=16; i++)
//	{
//		printf("%d ", gamePhaseCounts[i]);
//		if (i <=8)
//			gpE += gamePhaseCounts[i];
//		if (i >=9)
//			gpO += gamePhaseCounts[i];
//		gpT += gamePhaseCounts[i] * i;
//	}
//	printf("\n");
//	printf("gpO = %d, gpE = %d\n", gpO, gpE);
//	printf("gpT avg = %f\n\n", (double)gpT / totalFENs);
//	
//	//// Print the stats for each piece on each square
//	//for (int piece = 0; piece < 12; piece++)
//	//{
//	//	printf("%d\n", piece < 6 ? piece + 1 : -(piece - 5));
//	//	for (int square = 0; square < 64; square++)
//	//	{
//	//		printf(" (%6d,%6d,%6d)", wdlOccupancy[piece][square ^ 56][0], wdlOccupancy[piece][square ^ 56][1], wdlOccupancy[piece][square ^ 56][2]);
//	//		if (square % 8 == 7)
//	//			printf("\n");
//	//	}
//	//	printf("\n");
//	//}
//
//
//	for (int phase = 0; phase <= 16; phase++)
//	{
//		printf("Phase: %d (%d)\n", phase, gamePhaseCounts[phase]);
//
//		for (int pieceType = 0; pieceType < 6; pieceType++)
//		{
//			printf("Piece: %d\n", pieceType + 1);
//
//			for (int square = 0; square < 64; square++)
//			{
//				if (phaseOccupancyCounts[phase][pieceType][square ^ 56] == 0)
//					printf("    x           ");
//				else
//				//	printf("%5.2f ", (float)phaseOccupancy[phase][pieceType][square ^ 56] / phaseOccupancyCounts[phase][pieceType][square ^ 56]);
//				//printf("%5.3f ", (float)phaseOccupancy[phase][pieceType][square ^ 56] / 2.0 / gamePhaseCounts[phase]);
//				printf("%5.3f (%6d)  ", (float)phaseOccupancy[phase][pieceType][square ^ 56] / 2.0 / (phaseOccupancyCounts[phase][pieceType][square ^ 56] + 100), phaseOccupancyCounts[phase][pieceType][square ^ 56]);
//				if (square % 8 == 7)
//					printf("\n");
//			}
//			printf("\n");
//		}
//		printf("\n\n");
//	}
//
//
//	//ACCUMULATE UNIFIED OPENING AND ENDGAME TABLE VALUES
//
//
//
//	// Accumulate occupancy tables for W, D & L by opening and endgame tapering
//	// Final values are 1*W + 0.5*D divided by total-occupancy per piece per sq
//
//	//USE A NON-LINEAR TAPER IN THE EVAL NOW!???
//}

//// RUN THRU A GAZILLION GAMES
//// AT EACH POSN ADD TO COUNTERS FOR EACH PIECE-TYPE ON EACH SQ AT EACH GAMEPHASE(16), +1 FOR A WIN, 0.5 FOR A DRAW, 0 FOR A LOSS (I.E. A BLACK WIN ADDS ITS PIECES)
//// at end, divide by total # of posns to give a value in the range 0.0 - 1.0
//// piece/sq/phase combos with no data would be zero which seems reasonable as would be counted as all losses which is likely if they never actually occurred in a game!
//// can gen opening/endgame tables by adding weighted values across all phases and dividing by # of phases
//// then finally scale up by multiplying by 100cp? more?
//// GEN PSTs BY ADDING COUNTS IN A TAPERED WAY E.G. WK ON G1: 16/16 *COUNT16 + 15/16*COUNT15 + ETC (USE A NON-LINEAR TAPER?)
//// HOW TO SCALE DOWN TOTAL THOUGH? DIVIDE BY #GAMES? #POSITIONS? SUBTRACT MIN(ALL) FROM ALL TO GET ZERO ENTRY THEN SCALE OTHERS TO SOME SENSIBLE MAX VALUE?
//// SAVE DATA TO DISC AS YOU GO
//
////----------------------------------------------------------------------------------------------------
//
//uint32_t Occupancy[17][6][64];
//
//const char* ws = " \t\n\r\f\v";
//
//// trim from end of string (right)
//inline std::string& rtrim(std::string& s, const char* t = ws)
//{
//	s.erase(s.find_last_not_of(t) + 1);
//	return s;
//}
//
//// trim from beginning of string (left)
//inline std::string& ltrim(std::string& s, const char* t = ws)
//{
//	s.erase(0, s.find_first_not_of(t));
//	return s;
//}
//
//// trim from both ends of string (right then left)
//inline std::string& trim(std::string& s, const char* t = ws)
//{
//	return ltrim(rtrim(s, t), t);
//}
//
//void GameStatistics()
//{
//	FILE* gameFile;
//	std::string line;
//	int bufferLength = 10000;
//	char buffer[10000];
//
//	// Clear occupancy counts
//	memset(Occupancy, 0, 17 * 6 * 64 * 4);
//
//	// Process every game in the file
//	fopen_s(&gameFile, "S:\\Chess\\GameLibraries\\5000Games.pgn", "r");
//	while (fgets(buffer, bufferLength, gameFile))
//	{
//		line = buffer;
//		line = trim(line);
//		if ((line == "") || (strncmp(line.c_str(), "[", 1) == 0))
//			continue;
//
//
//
//
//
//
//
//
//
//
//
//	}
//	fclose(gameFile);
//
//	// Display results
//
//
//}

//void Adjust()
//{
//	FILE *file;
//	std::string filename;
//	int parameters[128];
//	int parameterIndex = 0;
//
//	filename = "S:\\Chess\\GameLibraries\\TEXEL\\tuner\\I.txt";
//	fopen_s(&file, filename.c_str(), "r");
//
//	for (int index = 0; index < 16; index++)
//	{
//		//fscanf(file, "%d,%d,%d,%d,\n", &parameters[parameterIndex + 0], &parameters[parameterIndex + 1], &parameters[parameterIndex + 2], &parameters[parameterIndex + 3]);
//		fscanf(file, "%d,%d,%d,%d,%d,%d,%d,%d,\n", &parameters[parameterIndex + 0], &parameters[parameterIndex + 1], &parameters[parameterIndex + 2], &parameters[parameterIndex + 3], &parameters[parameterIndex + 4], &parameters[parameterIndex + 5], &parameters[parameterIndex + 6], &parameters[parameterIndex + 7]);
//		parameterIndex += 8;
//	}
//
//	fclose(file);
//
//	//parameterIndex = 0;
//	//for (int index = 0; index < 16; index++)
//	//{
//	//	parameters[parameterIndex + 7] = parameters[parameterIndex + 0];
//	//	parameters[parameterIndex + 6] = parameters[parameterIndex + 1];
//	//	parameters[parameterIndex + 5] = parameters[parameterIndex + 2];
//	//	parameters[parameterIndex + 4] = parameters[parameterIndex + 3];
//	//	parameterIndex += 8;
//	//}
//
//	//for (int index = 0; index < 128; index++)
//	//	parameters[index] = round(parameters[index] / 2.56);
//
//	parameterIndex = 0;
//	for (int index = 0; index < 16; index++)
//	{
//		parameters[parameterIndex + 0] = parameters[parameterIndex + 7] = round((parameters[parameterIndex + 0] + parameters[parameterIndex + 7]) / 2);
//		parameters[parameterIndex + 1] = parameters[parameterIndex + 6] = round((parameters[parameterIndex + 1] + parameters[parameterIndex + 6]) / 2);
//		parameters[parameterIndex + 2] = parameters[parameterIndex + 5] = round((parameters[parameterIndex + 2] + parameters[parameterIndex + 5]) / 2);
//		parameters[parameterIndex + 3] = parameters[parameterIndex + 4] = round((parameters[parameterIndex + 3] + parameters[parameterIndex + 4]) / 2);
//		parameterIndex += 8;
//	}
//
//	filename = "S:\\Chess\\GameLibraries\\TEXEL\\tuner\\O.txt";
//	fopen_s(&file, filename.c_str(), "w");
//
//	for (int index = 0; index < 128; index++)
//	{
//		fprintf(file, "%4d,", parameters[index]);
//		if (index % 8 == 7)
//			fprintf(file, "\n");
//		if (index % 64 == 63)
//			fprintf(file, "\n");
//	}
//
//	fclose(file);
//}

//void Adjust()
//{
//	FILE *file;
//	std::string filename;
//	float p[17][6][64];
//	float p2[2][6][64];
//
//
//	filename = "D:\\Developer\\Games\\CGM\\Loader\\bin\\Release\\Percentages.txt";
//	fopen_s(&file, filename.c_str(), "r");
//
//	for (int phase = 0; phase < 17; phase++)
//		for (int piece = 0; piece < 6; piece++)
//		{
//			int i = 0;
//			fscanf(file, "%f,%f,%f,%f,%f,%f,%f,%f,\n", &p[phase][piece][i + 0], &p[phase][piece][i + 1], &p[phase][piece][i + 2], &p[phase][piece][i + 3], &p[phase][piece][i + 4], &p[phase][piece][i + 5], &p[phase][piece][i + 6], &p[phase][piece][i + 7]);
//			i += 8;
//			fscanf(file, "%f,%f,%f,%f,%f,%f,%f,%f,\n", &p[phase][piece][i + 0], &p[phase][piece][i + 1], &p[phase][piece][i + 2], &p[phase][piece][i + 3], &p[phase][piece][i + 4], &p[phase][piece][i + 5], &p[phase][piece][i + 6], &p[phase][piece][i + 7]);
//			i += 8;
//			fscanf(file, "%f,%f,%f,%f,%f,%f,%f,%f,\n", &p[phase][piece][i + 0], &p[phase][piece][i + 1], &p[phase][piece][i + 2], &p[phase][piece][i + 3], &p[phase][piece][i + 4], &p[phase][piece][i + 5], &p[phase][piece][i + 6], &p[phase][piece][i + 7]);
//			i += 8;
//			fscanf(file, "%f,%f,%f,%f,%f,%f,%f,%f,\n", &p[phase][piece][i + 0], &p[phase][piece][i + 1], &p[phase][piece][i + 2], &p[phase][piece][i + 3], &p[phase][piece][i + 4], &p[phase][piece][i + 5], &p[phase][piece][i + 6], &p[phase][piece][i + 7]);
//			i += 8;
//			fscanf(file, "%f,%f,%f,%f,%f,%f,%f,%f,\n", &p[phase][piece][i + 0], &p[phase][piece][i + 1], &p[phase][piece][i + 2], &p[phase][piece][i + 3], &p[phase][piece][i + 4], &p[phase][piece][i + 5], &p[phase][piece][i + 6], &p[phase][piece][i + 7]);
//			i += 8;
//			fscanf(file, "%f,%f,%f,%f,%f,%f,%f,%f,\n", &p[phase][piece][i + 0], &p[phase][piece][i + 1], &p[phase][piece][i + 2], &p[phase][piece][i + 3], &p[phase][piece][i + 4], &p[phase][piece][i + 5], &p[phase][piece][i + 6], &p[phase][piece][i + 7]);
//			i += 8;
//			fscanf(file, "%f,%f,%f,%f,%f,%f,%f,%f,\n", &p[phase][piece][i + 0], &p[phase][piece][i + 1], &p[phase][piece][i + 2], &p[phase][piece][i + 3], &p[phase][piece][i + 4], &p[phase][piece][i + 5], &p[phase][piece][i + 6], &p[phase][piece][i + 7]);
//			i += 8;
//			fscanf(file, "%f,%f,%f,%f,%f,%f,%f,%f,\n", &p[phase][piece][i + 0], &p[phase][piece][i + 1], &p[phase][piece][i + 2], &p[phase][piece][i + 3], &p[phase][piece][i + 4], &p[phase][piece][i + 5], &p[phase][piece][i + 6], &p[phase][piece][i + 7]);
//			i += 8;
//		}
//
//	fclose(file);
//
//
//
//	for (int piece = 0; piece < 6; piece++)
//		for (int square = 0; square < 64; square++)
//		{
//			p2[0][piece][square] = ((p[0][piece][square] * 8) + (p[1][piece][square] * 7) + (p[2][piece][square] * 6) + (p[3][piece][square] * 5) + (p[4][piece][square] * 4) + (p[5][piece][square] * 3) + (p[6][piece][square] * 2) + (p[7][piece][square] * 1)) / (8 + 7 + 6 + 5 + 4 + 3 + 2 + 1);
//			p2[0][piece][square] = p2[0][piece][square] * 10 - 500;
//			p2[1][piece][square] = ((p[16][piece][square] * 8) + (p[15][piece][square] * 7) + (p[14][piece][square] * 6) + (p[13][piece][square] * 5) + (p[12][piece][square] * 4) + (p[11][piece][square] * 3) + (p[10][piece][square] * 2) + (p[9][piece][square] * 1)) / (8 + 7 + 6 + 5 + 4 + 3 + 2 + 1);
//			p2[1][piece][square] = p2[1][piece][square] * 10 - 500;
//		}
//
//
//
//
//
//	filename = "D:\\Developer\\Games\\CGM\\Loader\\bin\\Release\\Percentages2.txt";
//	fopen_s(&file, filename.c_str(), "w");
//
//	for (int phase = 0; phase < 2; phase++)
//	{
//		for (int piece = 0; piece < 6; piece++)
//		{
//			int i = 0;
//			//fprintf(file, "%5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, \n", p2[phase][piece][i + 0], p2[phase][piece][i + 1], p2[phase][piece][i + 2], p2[phase][piece][i + 3], p2[phase][piece][i + 4], p2[phase][piece][i + 5], p2[phase][piece][i + 6], p2[phase][piece][i + 7]);
//			//i += 8;
//			//fprintf(file, "%5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, \n", p2[phase][piece][i + 0], p2[phase][piece][i + 1], p2[phase][piece][i + 2], p2[phase][piece][i + 3], p2[phase][piece][i + 4], p2[phase][piece][i + 5], p2[phase][piece][i + 6], p2[phase][piece][i + 7]);
//			//i += 8;
//			//fprintf(file, "%5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, \n", p2[phase][piece][i + 0], p2[phase][piece][i + 1], p2[phase][piece][i + 2], p2[phase][piece][i + 3], p2[phase][piece][i + 4], p2[phase][piece][i + 5], p2[phase][piece][i + 6], p2[phase][piece][i + 7]);
//			//i += 8;
//			//fprintf(file, "%5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, \n", p2[phase][piece][i + 0], p2[phase][piece][i + 1], p2[phase][piece][i + 2], p2[phase][piece][i + 3], p2[phase][piece][i + 4], p2[phase][piece][i + 5], p2[phase][piece][i + 6], p2[phase][piece][i + 7]);
//			//i += 8;
//			//fprintf(file, "%5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, \n", p2[phase][piece][i + 0], p2[phase][piece][i + 1], p2[phase][piece][i + 2], p2[phase][piece][i + 3], p2[phase][piece][i + 4], p2[phase][piece][i + 5], p2[phase][piece][i + 6], p2[phase][piece][i + 7]);
//			//i += 8;
//			//fprintf(file, "%5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, \n", p2[phase][piece][i + 0], p2[phase][piece][i + 1], p2[phase][piece][i + 2], p2[phase][piece][i + 3], p2[phase][piece][i + 4], p2[phase][piece][i + 5], p2[phase][piece][i + 6], p2[phase][piece][i + 7]);
//			//i += 8;
//			//fprintf(file, "%5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, \n", p2[phase][piece][i + 0], p2[phase][piece][i + 1], p2[phase][piece][i + 2], p2[phase][piece][i + 3], p2[phase][piece][i + 4], p2[phase][piece][i + 5], p2[phase][piece][i + 6], p2[phase][piece][i + 7]);
//			//i += 8;
//			//fprintf(file, "%5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, \n", p2[phase][piece][i + 0], p2[phase][piece][i + 1], p2[phase][piece][i + 2], p2[phase][piece][i + 3], p2[phase][piece][i + 4], p2[phase][piece][i + 5], p2[phase][piece][i + 6], p2[phase][piece][i + 7]);
//			//i += 8;
//			fprintf(file, "%5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, \n", p2[phase][piece][i + 0], p2[phase][piece][i + 1], p2[phase][piece][i + 2], p2[phase][piece][i + 3], p2[phase][piece][i + 4], p2[phase][piece][i + 5], p2[phase][piece][i + 6], p2[phase][piece][i + 7]);
//			i += 8;
//			fprintf(file, "%5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, \n", p2[phase][piece][i + 0], p2[phase][piece][i + 1], p2[phase][piece][i + 2], p2[phase][piece][i + 3], p2[phase][piece][i + 4], p2[phase][piece][i + 5], p2[phase][piece][i + 6], p2[phase][piece][i + 7]);
//			i += 8;
//			fprintf(file, "%5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, \n", p2[phase][piece][i + 0], p2[phase][piece][i + 1], p2[phase][piece][i + 2], p2[phase][piece][i + 3], p2[phase][piece][i + 4], p2[phase][piece][i + 5], p2[phase][piece][i + 6], p2[phase][piece][i + 7]);
//			i += 8;
//			fprintf(file, "%5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, \n", p2[phase][piece][i + 0], p2[phase][piece][i + 1], p2[phase][piece][i + 2], p2[phase][piece][i + 3], p2[phase][piece][i + 4], p2[phase][piece][i + 5], p2[phase][piece][i + 6], p2[phase][piece][i + 7]);
//			i += 8;
//			fprintf(file, "%5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, \n", p2[phase][piece][i + 0], p2[phase][piece][i + 1], p2[phase][piece][i + 2], p2[phase][piece][i + 3], p2[phase][piece][i + 4], p2[phase][piece][i + 5], p2[phase][piece][i + 6], p2[phase][piece][i + 7]);
//			i += 8;
//			fprintf(file, "%5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, \n", p2[phase][piece][i + 0], p2[phase][piece][i + 1], p2[phase][piece][i + 2], p2[phase][piece][i + 3], p2[phase][piece][i + 4], p2[phase][piece][i + 5], p2[phase][piece][i + 6], p2[phase][piece][i + 7]);
//			i += 8;
//			fprintf(file, "%5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, \n", p2[phase][piece][i + 0], p2[phase][piece][i + 1], p2[phase][piece][i + 2], p2[phase][piece][i + 3], p2[phase][piece][i + 4], p2[phase][piece][i + 5], p2[phase][piece][i + 6], p2[phase][piece][i + 7]);
//			i += 8;
//			fprintf(file, "%5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, \n", p2[phase][piece][i + 0], p2[phase][piece][i + 1], p2[phase][piece][i + 2], p2[phase][piece][i + 3], p2[phase][piece][i + 4], p2[phase][piece][i + 5], p2[phase][piece][i + 6], p2[phase][piece][i + 7]);
//			i += 8;
//			fprintf(file, "\n");
//		}
//		fprintf(file, "\n");
//	}
//
//	fclose(file);
//}

//void Adjust()
//{
//	FILE *file;
//	std::string filename;
//	float rankTotal[8], fileTotal[8];
//
//	filename = "D:\\Developer\\Games\\UGIConsoles\\CC2021x\\PESTOFlattened.txt";
//	fopen_s(&file, filename.c_str(), "w");
//
//
//	for (int table = 0; table < 12; table++)
//	{
//		for (int i = 0; i < 8; i++)
//		{
//			rankTotal[i] = 0;
//			fileTotal[i] = 0;
//		}
//
//		for (int i = 0; i < 8; i++)
//		{
//			for (int j = 0; j < 8; j++)
//			{
//				rankTotal[i] += parameters[12 + table * 64 + i * 8 + j];
//				fileTotal[j] += parameters[12 + table * 64 + i * 8 + j];
//			}
//		}
//
//		for (int i = 0; i < 8; i++)
//		{
//			for (int j = 0; j < 8; j++)
//				fprintf(file, "%5d,", (int)round((rankTotal[i] + fileTotal[j]) / 8));
//			fprintf(file, "\n");
//		}
//		fprintf(file, "\n");
//
//	}
//
//
//	fclose(file);
//}

//void Adjust()
//{
//	FILE *file;
//	std::string filename;
//
//	filename = "D:\\Developer\\Games\\UGIConsoles\\CC2021x\\PESTOFlattened2.txt";//IS THIS WHAT WE PREVIOUSLY CALLED MIRRORED?
//	fopen_s(&file, filename.c_str(), "w");
//
//
//	for (int table = 0; table < 12; table++)
//	{
//		for (int i = 0; i < 8; i++)
//		{
//			fprintf(file, "%5d,%5d,%5d,%5d,%5d,%5d,%5d,%5d,",
//				(parameters[12 + table * 64 + i * 8 + 0] + parameters[12 + table * 64 + i * 8 + 7]) / 2,
//				(parameters[12 + table * 64 + i * 8 + 1] + parameters[12 + table * 64 + i * 8 + 6]) / 2,
//				(parameters[12 + table * 64 + i * 8 + 2] + parameters[12 + table * 64 + i * 8 + 5]) / 2,
//				(parameters[12 + table * 64 + i * 8 + 3] + parameters[12 + table * 64 + i * 8 + 4]) / 2,
//				(parameters[12 + table * 64 + i * 8 + 4] + parameters[12 + table * 64 + i * 8 + 3]) / 2,
//				(parameters[12 + table * 64 + i * 8 + 5] + parameters[12 + table * 64 + i * 8 + 2]) / 2,
//				(parameters[12 + table * 64 + i * 8 + 6] + parameters[12 + table * 64 + i * 8 + 1]) / 2,
//				(parameters[12 + table * 64 + i * 8 + 7] + parameters[12 + table * 64 + i * 8 + 0]) / 2
//			);
//			fprintf(file, "\n");
//		}
//
//		fprintf(file, "\n");
//	}
//
//
//	fclose(file);
//}

//void Adjust()
//{
//	FILE *file;
//	std::string filename;
//
//	filename = "D:\\Developer\\Games\\UGIConsoles\\CC2021x\\PESTOSlightlyEvenedOutMirror.txt";
//	fopen_s(&file, filename.c_str(), "w");
//
//
//	for (int table = 0; table < 12; table++)
//	{
//		for (int i = 0; i < 8; i++)
//		{
//			int r;
//
//			r = parameters[12 + table * 64 + i * 8 + 0];
//			if (r > parameters[12 + table * 64 + i * 8 + 7])
//				r--;
//			else if (r < parameters[12 + table * 64 + i * 8 + 7])
//				r++;
//			fprintf(file, "%5d,", r);
//
//			r = parameters[12 + table * 64 + i * 8 + 1];
//			if (r > parameters[12 + table * 64 + i * 8 + 6])
//				r--;
//			else if (r < parameters[12 + table * 64 + i * 8 + 6])
//				r++;
//			fprintf(file, "%5d,", r);
//
//			r = parameters[12 + table * 64 + i * 8 + 2];
//			if (r > parameters[12 + table * 64 + i * 8 + 5])
//				r--;
//			else if (r < parameters[12 + table * 64 + i * 8 + 5])
//				r++;
//			fprintf(file, "%5d,", r);
//
//			r = parameters[12 + table * 64 + i * 8 + 3];
//			if (r > parameters[12 + table * 64 + i * 8 + 4])
//				r--;
//			else if (r < parameters[12 + table * 64 + i * 8 + 4])
//				r++;
//			fprintf(file, "%5d,", r);
//
//			r = parameters[12 + table * 64 + i * 8 + 4];
//			if (r > parameters[12 + table * 64 + i * 8 + 3])
//				r--;
//			else if (r < parameters[12 + table * 64 + i * 8 + 3])
//				r++;
//			fprintf(file, "%5d,", r);
//
//			r = parameters[12 + table * 64 + i * 8 + 5];
//			if (r > parameters[12 + table * 64 + i * 8 + 2])
//				r--;
//			else if (r < parameters[12 + table * 64 + i * 8 + 2])
//				r++;
//			fprintf(file, "%5d,", r);
//
//			r = parameters[12 + table * 64 + i * 8 + 6];
//			if (r > parameters[12 + table * 64 + i * 8 + 1])
//				r--;
//			else if (r < parameters[12 + table * 64 + i * 8 + 1])
//				r++;
//			fprintf(file, "%5d,", r);
//
//			r = parameters[12 + table * 64 + i * 8 + 7];
//			if (r > parameters[12 + table * 64 + i * 8 + 0])
//				r--;
//			else if (r < parameters[12 + table * 64 + i * 8 + 0])
//				r++;
//			fprintf(file, "%5d,", r);
//
//			fprintf(file, "\n");
//		}
//
//		fprintf(file, "\n");
//	}
//
//
//	fclose(file);
//}

//void Adjust()
//{
//	int offset = 12 + 64 + 64 + 64 + 64 + 64 + 64 + 64 + 64 +64;
//
//	for (int i = 0; i < 8; i++)
//	{
//		for (int j = 0; j < 8; j++)
//		{
//			printf("%5d,", parameters[offset + i * 8 + j] -25);
//		}
//		printf("\n");
//	}
//}
