#include <algorithm>
#include <chrono>
#include <assert.h>
#include <iostream>
#include <thread>
#include <map>

#include "GlobalConstants.h"
#include "GlobalTypes.h"
#include "Engine.h"
#include "UGI.h"
#include "Brain.h"
#include "Evaluate.h"
#include "Utilities.h"
#include "SearchMate.h"
#include "SYZYGYPYRRHIC\tbprobe.h"

//----------------------------------------------------------------------------------------------------

Mate::MateTranspositionTableBucket_Struct* Mate::MateTranspositionTablePointer = nullptr;
UINT32 Mate::MateTranspositionTableBuckets = 0;
UINT32 Mate::MateTranspositionTableBucketsMask;
bool Mate::MateSilent;

//alignas(64) TwoGoodMoves_Struct Mate::KillerMoves[MaximumPly];
//alignas(64) TwoGoodMoves_Struct Mate::CounterMoves[6][64];
//alignas(64) TwoGoodMoves_Struct Mate::FollowUpMoves[6][64];
//alignas(64) History_Struct Mate::CounterMoveHistory[6][64];

//const short FutilityMargin[9] = { 0,100,150,200,250,300,350,350,350 };
const short FutilityMargin[9] = { 0,50,100,150,200,250,300,300,300 };

//STRING BestLineMessage = "";

//----------------------------------------------------------------------------------------------------

Mate::Mate()
{
	CounterMoveHistory = new CounterMoveHistory_Struct;
}

Mate::~Mate()
{
	delete CounterMoveHistory;
}

//----------------------------------------------------------------------------------------------------

__declspec(noinline)
void Mate::ClearKillerMoves()
{
	//for (int index = 0; index < MaximumPly; index++)
	//{
	//	KillerMoves[index].m1.ui32 = PVTUnknown;
	//	KillerMoves[index].m1.piece = 0;
	//	KillerMoves[index].m2.ui32 = PVTUnknown;
	//	KillerMoves[index].m2.piece = 0;
	//}

	memset(KillerMoves, 0, sizeof(KillerMoves));
}

__declspec(noinline)
void Mate::ClearCounterMoves()
{
	//for (int pti = 0; pti < 6; pti++)
	//	for (int tsi = 0; tsi < 64; tsi++)
	//	{
	//		CounterMoves[pti][tsi].m1.ui32 = PVTUnknown;
	//		CounterMoves[pti][tsi].m2.ui32 = PVTUnknown;
	//	}

	memset(CounterMoves, 0, sizeof(CounterMoves));
}

__declspec(noinline)
void Mate::ClearFollowUpMoves()
{
	//for (int pti = 0; pti < 6; pti++)
	//	for (int tsi = 0; tsi < 64; tsi++)
	//	{
	//		FollowUpMoves[pti][tsi].m1.ui32 = PVTUnknown;
	//		FollowUpMoves[pti][tsi].m2.ui32 = PVTUnknown;
	//	}

	memset(FollowUpMoves, 0, sizeof(FollowUpMoves));
}

__declspec(noinline)
void Mate::ClearCounterMoveHistory()
{
	//for (int pt1i = 0; pt1i < 6; pt1i++)
	//	for (int ts1i = 0; ts1i < 64; ts1i++)
	//		for (int pt2i = 0; pt2i < 8; pt2i++)
	//			for (int ts2i = 0; ts2i < 64; ts2i++)
	//				CounterMoveHistory->CMH[pt1i][ts1i].History[pt2i][ts2i] = 0;

	memset(&CounterMoveHistory->CMH[0][0], 0, sizeof(CounterMoveHistory_Struct));
}

//----------------------------------------------------------------------------------------------------

#pragma region Message processing

void Mate::AddMessageToQueue(STRING message, bool lastMessageWasAProgressMessage)
{
#ifdef _DEBUG
	if (!MateSilent)
		Output(message);
#else
	//if (TC.CurrentType == TCTMateInN)
	//	Output(message);
	//else
	{
		MessageQueue[MessageQueueIndex++] = message;
		if (MessageQueueIndex == MessageQueueSize)
			MessageQueueIndex = 0;
		LastMessageWasAProgressMessage = lastMessageWasAProgressMessage;
		MessagesQueued = true;
	}
#endif
}

void Mate::ReverseMessageQueueIndex()
{
	MessageQueueIndex--;
	if (MessageQueueIndex == -1)
		MessageQueueIndex = MessageQueueSize - 1;
}

void Mate::ShowIterationStartMessage()
{
#ifndef _DEBUG
	if (ThreadId > 0)
		return;
#endif

	STRING IterationStartMessage = "info depth " + MyITOA(IterationPly)
		+ " seldepth " + MyITOA(MaximumPlyReached)
#ifdef _DEBUG
		+ " ThreadId " + MyITOA(ThreadId)
#endif
		;
	AddMessageToQueue(IterationStartMessage, false);
	//Output(IterationStartMessage);
}

void Mate::ShowProgressMessage(UINT32 move, int movesMade, short bestMoveScore, short alpha, short beta)
{
#ifndef _DEBUG
	if (ThreadId > 0)
		return;
#endif

	//TotalTickCount = GetTickCount64() - StartTickCount + 1; // +1 to avoid potential divide by zero on very fast computer!
	UINT64 totalTickCount = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - StartClock).count() + 1; // +1 to avoid potential divide by zero on very fast computer!
	STRING ProgressMessage = "info time " + MyUI64TOA(totalTickCount)
		+ " nodes " + MyUI64TOA(NodeCount + NodeCountQuiescenceSearch)
		+ " currmove " + MoveNotation(move)
		+ " currmovenumber " + MyITOA(movesMade)
#ifdef _DEBUG
		+ " depth " + MyITOA(IterationPly)
		+ " ThreadId " + MyITOA(ThreadId)
#endif
		;
	if (IsDebug)
	{
		ProgressMessage += " bestMoveScore " + MyITOA(bestMoveScore) + " alpha " + MyITOA(alpha) + " beta " + MyITOA(beta);
		//ProgressMessage += " processor " + std::to_string(GetCurrentProcessorNumber());
	}
	if (LastMessageWasAProgressMessage)
		ReverseMessageQueueIndex();
	AddMessageToQueue(ProgressMessage, true);
	//Output(ProgressMessage);
}

void Mate::ShowFailedLowMessage(short rootAlpha)
{
	STRING FailedLowMessage = "info score cp " + MyITOA(rootAlpha) + " upperbound";
	AddMessageToQueue(FailedLowMessage, false);
}

void Mate::ShowIterationFinishMessage(UINT32 hashfull)
{
#ifndef _DEBUG
	if (ThreadId > 0)
		return;
#endif

	//TotalTickCount = GetTickCount64() - StartTickCount + 1; // +1 to avoid potential divide by zero on very fast computer!
	UINT64 totalTickCount = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - StartClock).count() + 1; // +1 to avoid potential divide by zero on very fast computer!

	UINT64 totalNodes = NodeCount + NodeCountQuiescenceSearch;
	STRING IterationFinishMessage = "info time " + MyUI64TOA(totalTickCount)
		+ " nodes " + MyUI64TOA(totalNodes)
		+ " nps " + MyUI64TOA((totalNodes * 1000) / totalTickCount)
		+ " hashfull " + MyITOA(hashfull) + (EndgameTablebasesHits > 0 ? " tbhits " + MyUI64TOA(EndgameTablebasesHits) : "")
#ifdef _DEBUG
		+ " ThreadId " + MyITOA(ThreadId)
#endif
		+ "\n";
	//IterationFinishMessage = "info time " + MyUI64TOA(TotalTickCount) + " nodes " + MyUI64TOA(totalNodes) + " nps " + MyUI64TOA((totalNodes * 1000) / TotalTickCount) + " hashfull " + MyITOA(hashfull) + (EndgameTablebasesHits > 0 ? " tbhits " + MyUI64TOA(EndgameTablebasesHits) + "/" + MyUI64TOA(EndgameTablebasesProbes) + "/" + MyUI64TOA(EndgameTablebasesHeavyProbes) : "") + "\n";
	AddMessageToQueue(IterationFinishMessage, false);
	//Output(IterationFinishMessage);
}

void Mate::ShowQueuedMessages()
{
	//int messagesOutput = 0;
	for (int i = 0; i < MessageQueueSize; i++)
	{
		int index = (MessageQueueIndex + i) % MessageQueueSize;
		if (MessageQueue[index] != "")
		{
			if (!MateSilent)
				Output(MessageQueue[index]);
			MessageQueue[index] = "";
			//messagesOutput++;
		}
	}

	//MessagesLastDisplayedTickCount = GetTickCount64();
	MessagesLastDisplayedClock = std::chrono::steady_clock::now();
	//if (messagesOutput == 0)
	//	Output("***");
	//Output("---" + MyUI64TOA(MessagesLastDisplayedTickCount));
	MessagesQueued = false;
}

STRING Mate::BestLine()
{
	STRING bestLine = "";

	int i = 0;
	do
	{
		bestLine += MoveNotation(PrincipalVariation[i++]) + " ";
	} while ((UINT16)PrincipalVariation[i] != 0);

	return bestLine;
}

//bool ShowPVTerminators = false;
void Mate::ShowBestLineMessage(short alpha, UINT8 eul)
{
#ifndef _DEBUG
	if (ThreadId > 0)
		return;
#endif

	//TotalTickCount = GetTickCount64() - StartTickCount + 1; // +1 to avoid potential divide by zero on very fast computer!
	UINT64 totalTickCount = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - StartClock).count() + 1; // +1 to avoid potential divide by zero on very fast computer!

	// Construct the PV
	STRING PVMessage = "";
	int i = 0;
	do
	{
		PVMessage += MoveNotation(PrincipalVariation[i++]) + " ";
	} while ((PrincipalVariation[i] & 0xFFFF) != 0);

	// Construct any 'end of PV' suffix
	STRING pvTerminatorMessage = "";
#ifndef _DEBUG
	//if (Mate::ShowPVTerminators)
#endif
		//if (PrincipalVariation[i] < -1)
		//if (false)
	{
		switch (PrincipalVariation[i])
		{
		case PVTStandPat:
			pvTerminatorMessage = "*Pat";
			break;
		case PVTDrawByRepetition:
			pvTerminatorMessage = "*Draw(Repetition)";
			break;
		case PVTDrawBy50MoveRule:
			pvTerminatorMessage = "*Draw(50MoveRule)";
			break;
		case PVTDrawMinimumMaterial:
			pvTerminatorMessage = "*Draw(MinimumMaterial)";
			break;
		case PVTDrawImmediateRepetition:
			pvTerminatorMessage = "*Draw(ImmediateRepetition)";
			break;
		case PVTDrawPerpetual:
			pvTerminatorMessage = "*Perpetual";
			break;
		case PVTDrawStalemate:
			pvTerminatorMessage = "*Draw(Stalemate)";
			break;
		case PVTCheckmate:
			pvTerminatorMessage = "*Checkmate";
			break;
		case PVTEGTB:
			pvTerminatorMessage = "*EGTB";
			break;
		case PVTTTUpper:
			pvTerminatorMessage = "*TTUpper";
			break;
		case PVTTTLower:
			pvTerminatorMessage = "*TTLower";
			break;
		case PVTTTExact:
			//pvTerminatorMessage = "*TT" + MyITOA(-PrincipalVariation[i] + PVRTTExact);
			pvTerminatorMessage = "*TTExact";
			break;
		default:
			pvTerminatorMessage = "*Unknown PV terminator found!";
			break;
		}
	}

	// Final bits and bobs
	UINT64 totalNodes = NodeCount + NodeCountQuiescenceSearch;

	STRING scoreMessage = " score ";
	if (alpha >= MatingScore) // Mating?
		scoreMessage += "mate " + MyITOA((MateBaseScore - alpha) >> 1);
	else if (alpha <= MatedScore) // Mated?
		scoreMessage += "mate " + MyITOA((-MateBaseScore - alpha + 1) >> 1);
	else // Mate
		scoreMessage += "cp " + MyITOA(alpha);

	STRING eulMessage = "";
	if (eul == 1)
		eulMessage = " lowerbound";
	else if (eul == 2)
		eulMessage = " upperbound";

	// Display the constructed message
	// N.B. the 'depth' value is provided here (as well as in the iteration 'start' message) as some GUIs (e.g. Arena, Shredder) don't display it unless it's provided with the PV!
	STRING BestLineMessage = "info depth " + MyITOA(IterationPly)
		+ " time " + MyUI64TOA(totalTickCount)
		+ " nodes " + MyUI64TOA(totalNodes) + scoreMessage + eulMessage
		+ " pv " + PVMessage + pvTerminatorMessage
#ifdef _DEBUG
		+ " ThreadId " + MyITOA(ThreadId)
#endif
		;
	AddMessageToQueue(BestLineMessage, false);
	//Output(BestLineMessage);
	ShowQueuedMessages(); // In mate mode, ensure we display all dual solutions
}

#pragma endregion

//----------------------------------------------------------------------------------------------------

#pragma region Root move list handling

// Called after each root move has been searched to save its node count
void Mate::SaveRootNodeCounts(int move)
{
	// Find the current move in the root move list
	for (int index = 0; index < RootMovesCount; index++)
		if (RootMoveList[index].mws.ui32 == move)
		{
			RootMoveList[index].nodes = NodeCount + NodeCountQuiescenceSearch - RootCumulativeNodeCount;
			RootCumulativeNodeCount = NodeCount + NodeCountQuiescenceSearch;
			break;
		}
}

// Called for a root move which takes over as best to give it a very high pseudo node count
void Mate::UpdateRootNodeCounts(int move, short score)
{
	// Find the current move in the root move list
	for (int index = 0; index < RootMovesCount; index++)
		if (RootMoveList[index].mws.ui32 == move)
		{
			RootMoveList[index].nodes = UINT64_MAX - UINT32_MAX + score;
			break;
		}
}

// Called after the moves have been generated at the root to assign a simple sequential value to each root move based on its subtree size
void Mate::ScoreRootMoveList(MoveWithScore_Struct* mlp)
{
	for (int index1 = 0; index1 < RootMovesCount; index1++)
	{
		UINT64 highestNodes = 0;
		int highestIndex = 0;
		for (int index2 = 0; index2 < RootMovesCount; index2++)
		{
			if (RootMoveList[index2].nodes > highestNodes)
			{
				highestNodes = RootMoveList[index2].nodes;
				highestIndex = index2;
			}
		}
		RootMoveList[highestIndex].nodes = 0;
		mlp[highestIndex].score = INT_MAX - index1;
	}
}

#pragma endregion

//----------------------------------------------------------------------------------------------------

void Mate::TimeUp(float divisor)
{
	if (ThreadId > 0)
		return;

	switch (TC.CurrentType)
	{
	case TCTFixedDepth:
		if (IterationPly >= TC.FixedDepthPly)
			StopWhenIterationComplete = true;
		break;

	case TCTFixedTime:
		if (IterationPly < 2) // Always let it complete the 1st iteration
			break;
		if ((std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - StartClock).count()) > TC.FixedTimeMilliSeconds)
		{
			StopWhenIterationComplete = true;
			StopImmediately = true;
		}
		else if (IterationPly >= MaximumIterationPly) // If it reaches the maximum iteration within the alloted time then stop anyway
			StopWhenIterationComplete = true;
		break;

	case TCTFixedNodes:
		if ((NodeCount + NodeCountQuiescenceSearch) >= TC.FixedNodesCount)
		{
			StopWhenIterationComplete = true;
			StopImmediately = true;
		}
		break;

	case TCTMateInN: // NOT USED IN NORMAL???
		if ((RootScore >= (MateBaseScore - TC.MateInN * 2) - 2))// || (IterationPly >= TC.MateInN * 2))
		//if ((RootScore >= WinningBaseScore))// || (IterationPly >= TC.MateInN * 2))
			StopWhenIterationComplete = true;
		break;

	default:
		// Never call time up in the middle of early iterations
		if (IterationPly < MinimumIterationPly) // Typically 4
			break;
		if (IterationPly == MinimumIterationPly)
			if (*mateBrain.gameRecord[mateBrain.GameRecordIndexRoot].principalVariationPointer == PVTUnknown) // Has at least one move returned a usable value?
				break;

		// Use signed integers for time calculations in case we go below zero!
		SINT64 moveTime;
		SINT64 timeLeft;
		int movesLeft;

		// Get the time consumed so far this move
		//moveTime = GetTickCount64() - StartTickCount + 1; // Add 1 millisecond to help on blitz finishes where timer is inaccurate
		moveTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - StartClock).count() + 1; // Add 1 millisecond to help on blitz finishes where timer is inaccurate

		// Get the amount of time left on the clock
		SINT64 stmTime;
		if (SideToMove == 0)
			stmTime = WTime;
		else
			stmTime = BTime;
		stmTime = std::max(stmTime - 500, (SINT64)0); // Subtract 500ms buffer for a slow EGTB access
		timeLeft = std::max(stmTime - moveTime, (SINT64)0); // Don't allow timeLeft to go -ve

		// 'Panic' time up? (Have we used more than half of our remaining time?)
		if (moveTime > (stmTime / 2))
			divisor = 9999; // Set the divisor very high so that it calls time up!
		else if (RootAlpha == (short)(-MateBaseScore)) // Give more time if failed low and still in the middle of the 'retry' iteration (i.e. haven't searched all root moves)
		{
			if (divisor == 1)
				divisor = 0.25; // (+5.7, +/ -4.2, 14268)
		}
		else
		{
			// Use less time for 'obvious' moves (up to approximately halving the budget at most!)
			// This caters for moves like e.g. best positional, recaptures, mate found, only one move
			// 1.15 ^ 5 = 2.0113571875  REDUCES DEPTH TOO OFTEN (-14.5 ELO)
			// 1.08 ^ 9 = 1.999004627104432128 MUCH BETTER (-0.7 ELO)
			//if (ConsistentBestMoves > 0)
			if (ConsistentBestMoves >= IterationPly - 1) // Only do this if we have had the same best move EVERY iteration (+0.1, +/-4.9, 10008)
				divisor *= pow(1.08f, std::min(9, ConsistentBestMoves));
		}

		// 'Estimate' moves left to time control to give us a budget
		const int movesLeftBaseEstimate = 9;//11;
		movesLeft = movesLeftBaseEstimate;
		if (MovesToGo == 0) // 'All the moves'?
		{
			movesLeft += 1;
			if (WInc == 0) // If 'all the moves' AND no Fischer bonus then need to be VERY careful
				movesLeft += 5;
		}
		else // Repeating
		{
			if (MovesToGo < movesLeft)
				movesLeft = MovesToGo + 1; // (+10.8, +/ -3.6, 18701)
			else
				movesLeft += std::min(MovesToGo / 16, 2); // Add at most 2 extra moves near the start of each control (so it doesn't use too much time there) (+4.0, +/-3.5, 20000)

			if (WInc == 0) // If no Fischer bonus then need to be more careful
				movesLeft += 2;
		}

		if ((moveTime >= (timeLeft / movesLeft / divisor)) || (RootMovesCount == 1) || (IterationPly >= MaximumIterationPly))
		{
			if (Pondering)
				ReplyImmediately = true;
			else
			{
				StopWhenIterationComplete = true;
				StopImmediately = true;
			}
		}
	}
}

//short Mate::DrawScore(int sideToMove)
//{
//	// Use a function to provide the draw score (rather than a simple variable) because there are many tweaks possible!
//	// Like the evaluation function, it returns a score relative to the side to move
//
//	short ds = Contempt;
//
//	if (MateBrain.gameRecordPointer->totalMaterial[sideToMove] > MateBrain.gameRecordPointer->totalMaterial[sideToMove ^ 1])
//		ds++;
//	else if (MateBrain.gameRecordPointer->totalMaterial[sideToMove] < MateBrain.gameRecordPointer->totalMaterial[sideToMove ^ 1])
//		ds--;
//
//	ds += (NodeCount & 1) * 2 - 1; // Add +/-1 randomly to avoid DBR stickiness
//
//	return ds;
//}

void Mate::UpdateKillers(int move, int ply)
{
	Move_Struct ms;
	ms.ui32 = move;
	if ( // 'Quiet' move?
		(!(ms.mf.flag >= MFPromotion)) &&
		(mateBrain.mailboxBoard64[ms.mf.toSquare] == 0)
		)
	{
		if (KillerMoves[ply].m1.ui32 != ms.ui32)
		{
			KillerMoves[ply].m2 = KillerMoves[ply].m1;
			KillerMoves[ply].m1.ui32 = ms.ui32;
			KillerMoves[ply].m1.piece = mateBrain.mailboxBoard64[ms.mf.fromSquare];
		}
	}
}

//int Mate::CurrentLineExpense(int ply)
//{
//	int expense = 0;
//
//	for (int i = 1; i <= ply; i++)
//	{
//		if (i & 1)
//		{
//			if ((RootGameRecordPointer - 1 + i)->givesCheck)
//				continue;
//			if ((RootGameRecordPointer - 1 + i)->isThreateningMateInOne.ui32 != 0)
//				continue;
//			if (DefenderKingMovesAfter[i] == 0)
//			{
//				expense += 1;
//				continue;
//			}
//			expense += 4;
//		}
//		else
//		{
//			if ((RootGameRecordPointer - 1 + i)->isInCheck)
//				continue;
//			if ((RootGameRecordPointer - 1 + i)->isO1M)
//				continue;
//			if ((RootGameRecordPointer - 1 + i - 1)->isThreateningMateInOne.ui32 != 0)
//			{
//				expense += 1;
//				continue;
//			}
//			if (DefenderKingMovesAfter[i - 1] == 0)
//			{
//				expense += 1;
//				continue;
//			}
//			expense += 4;
//		}
//	}
//
//	return expense;
//}

//int Mate::CurrentLineExpense2(int ply)
//{
//	int expense = 0;
//
//	for (int i = 1; i <= ply; i++)
//		expense += (RootGameRecordPointer - 1 + i)->expense;
//
//	return expense;
//}


//bool Mate::AllForced(int ply)
//{
//	for (int i = 1; i <= ply; i ++)
//	{
//		if (i & 1)
//		{
//			if ((RootGameRecordPointer - 1 + i)->givesCheck)
//				continue;
//			if ((RootGameRecordPointer - 1 + i)->isThreateningMateInOne.ui32 != 0)
//				continue;
//			if (DefenderKingMovesAfter[i] == 0)
//				continue;
//		}
//		else
//		{
//			if ((RootGameRecordPointer - 1 + i - 1)->givesCheck)
//				continue;
//			if ((RootGameRecordPointer - 1 + i - 1)->isThreateningMateInOne.ui32 != 0)
//				continue;
//			if (DefenderKingMovesAfter[i - 1] == 0)
//				continue;
//			if ((RootGameRecordPointer - 1 + i)->isO1M)
//				continue;
//		}
//		return false;
//	}
//
//	return true;
//}

bool Mate::AllChecks(int ply)
{
	for (int i = 1; i <= ply; i += 2)
	{
		if (!(RootGameRecordPointer - 1 + i)->givesCheck)
			return false;
	}

	return true;
}

bool Mate::AllO1M(int ply)
{
	for (int i = 0; i < ply; i += 2)
	{
		if (!(RootGameRecordPointer + i)->isO1M)
			return false;
	}

	return true;
}

bool Mate::AllTWM(int ply)
{
	for (int i = 0; i < ply; i += 2)
	{
		if (!(mateBrain.gameRecordPointer - i)->isTWM)
			return false;
	}

	return true;
}

//bool Mate::AllZLKM(int ply)
//{
//	for (int i = 1; i <= ply; i += 2)
//	{
//		if (DefenderKingMovesAfter[i] > 0)
//			return false;
//	}
//
//	return true;
//}

Move_Struct Mate::CanGiveMateInOne(int sideToMove, int isInCheck, int &checksCount)//THIS SHOULD BE IN BRAIN???
{
	Move_Struct result;
	result.ui32 = 0;
	alignas(64) MoveWithScore_Struct moveList[220];
	Move_Struct currentMove;

	mateBrain.CalculatePinnedPieces(sideToMove); // Required for legal move generation

	if (isInCheck)
	{
		int enemyKingSquare = BitScanForwardX(mateBrain.piecesBB[sideToMove ^ 1][King]);
		int movesCount = (int)(mateBrain.GenerateAllMovesOutOfCheck(sideToMove, moveList, true) - moveList);

		for (int moveListIndexIterator = 0; moveListIndexIterator < movesCount; moveListIndexIterator++)
		{
			currentMove.ui32 = moveList[moveListIndexIterator].ui32;
			mateBrain.gameRecordPointer->move.ui32 = currentMove.ui32;
			mateBrain.MakeMove(sideToMove); // N.B. MakeMove increments MateBrain.gameRecordPointer!

			bool givesCheck = mateBrain.IsEnemyKingAttacked(enemyKingSquare, sideToMove);
			bool anyMoves = true;
			if (givesCheck)
			{
				mateBrain.CalculatePinnedPieces(sideToMove ^ 1); // Required for legal move generation
				anyMoves = mateBrain.AnyMoves(sideToMove ^ 1, true);
			}

			mateBrain.UnMakeMove(sideToMove); // N.B. UnMakeMove decrements MateBrain.gameRecordPointer!

			if (!anyMoves)
			{
				result.ui32 = mateBrain.gameRecordPointer->move.ui32;
				break;
			}
		}
	}
	else
	{
		mateBrain.CalculateDiscovererPieces(sideToMove); // Required for legal move generation
		checksCount = (int)(mateBrain.GenerateAllChecks(sideToMove, moveList) - moveList);

		for (int moveListIndexIterator = 0; moveListIndexIterator < checksCount; moveListIndexIterator++)
		{
			currentMove.ui32 = moveList[moveListIndexIterator].ui32;
			mateBrain.gameRecordPointer->move.ui32 = currentMove.ui32;
			mateBrain.MakeMove(sideToMove); // N.B. MakeMove increments MateBrain.gameRecordPointer!

			mateBrain.CalculatePinnedPieces(sideToMove ^ 1); // Required for legal move generation
			bool anyMoves = mateBrain.AnyMoves(sideToMove ^ 1, true);

			mateBrain.UnMakeMove(sideToMove); // N.B. UnMakeMove decrements MateBrain.gameRecordPointer!

			if (!anyMoves)
			{
				result.ui32 = mateBrain.gameRecordPointer->move.ui32;
				break;
			}
		}
	}

	return result;
}

//Move_Struct Mate::ThreateningMateInOne(int sideToMove) // SHOULD THIS BE IN BRAIN???
//{
//	Move_Struct result;
//	result.ui32 = 0;
//	alignas(64) MoveWithScore_Struct moveList[220];
//	Move_Struct currentMove;
//
//	MateBrain.gameRecordPointer++; // Normally done in make/unmake-move
//	MateBrain.gameRecordPointer->castlingStatus = (MateBrain.gameRecordPointer - 1)->castlingStatus;
//	MateBrain.gameRecordPointer->epSquare = 0;
//	MateBrain.gameRecordPointer->pliesSinceIrreversible = 0;
//
//	// Generate move list for attacker
//	MateBrain.CalculatePinnedPieces(sideToMove); // Required for legal move generation
//	int movesCount;
//	//movesCount = MateBrain.GenerateAllMoves(sideToMove, false, moveList);
//	MateBrain.CalculateDiscovererPieces(sideToMove); // Required for legal move generation
//	movesCount = MateBrain.GenerateAllChecks(sideToMove, moveList) - moveList;
//
//	int enemyKingSquare = BitScanForwardX(MateBrain.piecesBB[sideToMove ^ 1][King]);
//
//	for (int moveListIndexIterator = 0; moveListIndexIterator < movesCount; moveListIndexIterator++)
//	{
//		currentMove.ui32 = moveList[moveListIndexIterator].ui32;
//		MateBrain.gameRecordPointer->move.ui32 = currentMove.ui32;
//		MateBrain.MakeMove(sideToMove); // N.B. MakeMove increments MateBrain.gameRecordPointer!
//
//		//bool givesCheck = MateBrain.IsEnemyKingAttacked(enemyKingSquare, sideToMove);
//		bool anyMoves = true;
//		//if (givesCheck)
//		anyMoves = MateBrain.AnyMoves(sideToMove ^ 1, true);
//
//		MateBrain.UnMakeMove(sideToMove); // N.B. UnMakeMove decrements MateBrain.gameRecordPointer!
//
//		if (!anyMoves)
//		{
//			result.ui32 = MateBrain.gameRecordPointer->move.ui32;
//			break;
//		}
//	}
//
//
//	MateBrain.gameRecordPointer--;
//
//	return result;
//}
//
//----------------------------------------------------------------------------------------------------

#pragma region TT routines

__declspec(noinline)
void Mate::ClearMateTranspositionTable()
{
	for (UINT32 bucket = 0; bucket < MateTranspositionTableBuckets; bucket++)
	{
		for (UINT32 entry = 0; entry < MateTranspositionTableEntriesPerBucket; entry++)
		{
			MateTranspositionTablePointer[bucket].Entries[entry].subTreeDepth = -128;
			MateTranspositionTablePointer[bucket].Entries[entry].plyToCeiling = -128;
			//MateTranspositionTablePointer[bucket].Entries[entry].hash64 = 0; // Setting the hash to zero doesn't really 'clear' it (because it's a valid value) but it's useful for visual debugging!
			MateTranspositionTablePointer[bucket].Entries[entry].key32to63 = 0; // Setting the hash to zero doesn't really 'clear' it (because it's a valid value) but it's useful for visual debugging!
			MateTranspositionTablePointer[bucket].Entries[entry].bestMove = PVTUnknown;
			//MateTranspositionTablePointer[bucket].Entries[entry].staticEvaluation = INT16_MIN;
			MateTranspositionTablePointer[bucket].Entries[entry].flag = TTFlagUpper;
		}
		MateTranspositionTablePointer[bucket].locked = false;
	}
}

__declspec(noinline)
bool Mate::MateTranspositionTableUnlocked()
{
	// Used after a search to confirm that all transposition table entries are unlocked
	for (UINT32 bucket = 0; bucket < MateTranspositionTableBuckets; bucket++)
		if (MateTranspositionTablePointer[bucket].locked)
			return false;

	return true;
}

__declspec(noinline)
void Mate::AllocateMateTranspositionTable()
{
	// Calculate the largest 'power of 2' number of entries that will fit in the specified number of bytes
	MateTranspositionTableBuckets = 1;
	assert(sizeof(MateTranspositionTableBucket_Struct) == 64);
	assert(sizeof(MateTranspositionTableEntry_Struct) == 12);
	while ((MateTranspositionTableBuckets * sizeof(MateTranspositionTableBucket_Struct)) <= (TranspositionTableMemory * 1024ULL * 1024ULL))
		MateTranspositionTableBuckets <<= 1;
	MateTranspositionTableBuckets >>= 1;
	// N.B. Increasing the transposition table size may be counter-productive beyond some margin.
	// Once the table is not being filled after the search you are just storing the same info spread over more memory.
	// Some testing indicates that once you get more than about 50% of the table not being used you will suffer a slow down.

	// Free any previously allocated memory. If the pointer is nullptr it does nothing.
	AlignedFreeMemory(MateTranspositionTablePointer);

	// Allocate transposition table memory
	if (MateTranspositionTableBuckets > 0)
	{
		MateTranspositionTableBucketsMask = MateTranspositionTableBuckets - 1;
		MateTranspositionTablePointer = (MateTranspositionTableBucket_Struct*)AlignedAllocateMemory(MateTranspositionTableBuckets * sizeof(MateTranspositionTableBucket_Struct), 64);
		if ((MateTranspositionTablePointer == nullptr))
		{
			Output("info string *** Error! Mate transposition table memory could not be allocated!");
			MateTranspositionTableBuckets = 0;
		}
		else
			ClearMateTranspositionTable();
	}
	if (IsDebug && (MateTranspositionTablePointer != nullptr))
	{
		Output("info string Transposition table memory = " + MyUI64TOA(TranspositionTableMemory) + "MB (" + MyUI64TOA(TranspositionTableMemory * 1024ULL * 1024ULL) + " bytes)");
		Output("info string Mate transposition table bucket size = " + MyUI64TOA(sizeof(MateTranspositionTableBucket_Struct)) + " bytes");
		Output("info string Mate transposition table entry size = " + MyUI64TOA(sizeof(MateTranspositionTableEntry_Struct)) + " bytes");
		Output("info string Mate transposition table entries per bucket = " + MyUI64TOA(MateTranspositionTableEntriesPerBucket));
		Output("info string Mate transposition table buckets = " + MyUI64TOA(MateTranspositionTableBuckets));
		Output("info string Mate transposition table entries = " + MyUI64TOA(MateTranspositionTableBuckets * MateTranspositionTableEntriesPerBucket));
		Output("info string Mate transposition table memory allocated = " + MyUI64TOA(MateTranspositionTableBuckets * sizeof(MateTranspositionTableBucket_Struct) / (1024ULL * 1024ULL)) + "MB (" + MyUI64TOA(MateTranspositionTableBuckets * sizeof(MateTranspositionTableBucket_Struct)) + " bytes)");
	}
}

UINT32 Mate::HashfullMateTranspositionTable()
{
	// About 0.01% of entries are 'exact'
	// About 31.5% of entries are 'upper' ('all' node)
	// About 68.5% of entries are 'lower' ('cut' node)

	if (MateTranspositionTableBuckets == 0)
		return 0;

	// Assuming an even distribution of used entries across the entire table a fairly accurate estimate can be made by examining a small subset of entries e.g. 1024
	// Even with the smallest possible transposition table (1MB) we would still have 16384 buckets
	UINT32 usedEntries = 0;
	UINT32 bucketsToTry = 256;
	for (UINT32 bucket = 0; bucket < bucketsToTry; bucket++)
		for (UINT32 entry = 0; entry < MateTranspositionTableEntriesPerBucket; entry++)
			if (
				(MateTranspositionTablePointer[bucket].Entries[entry].subTreeDepth != -128)
				&& ((MateTranspositionTablePointer[bucket].Entries[entry].flag & TTFlagAgeMask) == TranspositionTableAge) // Only count entries of the current 'age'
				)
				usedEntries++;
	return (UINT32)((usedEntries * 1000) / (bucketsToTry * MateTranspositionTableEntriesPerBucket));
}

void Mate::AddToMateTranspositionTable(SINT8 depthRemaining, short ply, short score, UINT8 flag, UINT32 bestMove, short tteStaticEvaluation)
{
	if (MateTranspositionTableBuckets > 0)
		if (ply < TC.MateInN * 2 - 1)//SHOULD ALWAYS BE TRUE???
		{
			UINT64 hash64 = mateBrain.gameRecordPointer->transpositionTableHash64WithEP;
			MateTranspositionTableBucket_Struct* ttb = MateTranspositionTablePointer + (hash64 & MateTranspositionTableBucketsMask);
			UINT32 hash32 = (hash64 >> 32);

			//if (hash64 == 11837812802435015306)
			//{
			//	return;
			//	Output(MateBrain.CurrentLine(ply));
			//	AC1++;
			//}


			if (score >= WinningBaseScore)
			{
				MatingPositionsTablePointer[hash64 & MatingPositionsTableMask] = hash64;//TEST
			}

			UINT32 entry, entryToReplace;
			//for (entry = 0; entry < MateTranspositionTableEntriesPerBucket; entry++) // Do we already have this position in the table?
			//	//if (ttb->Entries[entry].hash64 == hash64)
			//	if (ttb->Entries[entry].key32to64 == hash32)//WOULD IT BE FASTER TO ASCERTAIN shallowestSubTreeDepth IN THIS LOOP???
			//		break;
			//if (entry == MateTranspositionTableEntriesPerBucket) // We didn't find this position in the table so let's find the shallowest entry to replace (giving preference to 'aged' entries)
			//{
			//	entryToReplace = 0;
			//	//int shallowestSubTreeDepth = ttb->Entries[0].subTreeDepth + ((TranspositionTableAge != (ttb->Entries[0].flag & TTAgeMask)) ? -128 : 0);
			//	int shallowestSubTreeDepth = ttb->Entries[0].subTreeDepth; // (+3.0, +/-3.6, 20000) for NOT using 'age' in this replacement scheme
			//	for (entry = 1; entry < MateTranspositionTableEntriesPerBucket; entry++)
			//		//if (ttb->Entries[entry].subTreeDepth + ((TranspositionTableAge != (ttb->Entries[entry].flag & TTAgeMask)) ? -128 : 0) < shallowestSubTreeDepth)
			//		if (ttb->Entries[entry].subTreeDepth < shallowestSubTreeDepth)
			//		{
			//			entryToReplace = entry;
			//			//shallowestSubTreeDepth = ttb->Entries[entry].subTreeDepth + ((TranspositionTableAge != (ttb->Entries[entry].flag & TTAgeMask)) ? -128 : 0);
			//			shallowestSubTreeDepth = ttb->Entries[entry].subTreeDepth;
			//		}
			//}
			//else
			//{
			//	//if (depthRemaining < ttb->Entries[entry].subTreeDepth) // Do NOT put this back in. Crafty argues against it and it can cause pathological behaviour
			//		//return;
			//	entryToReplace = entry;
			//}

			UINT8 flagEUL = flag & TTFlagEULMask;

			// Find candidate entry for replacement
			int shallowestSubTreeDepth = 999; // (+3.0, +/-3.6, 20000) for NOT using 'age' in this replacement scheme
			entryToReplace = -1;
			for (entry = 0; entry < MateTranspositionTableEntriesPerBucket; entry++) // Do we already have this position in the table?
			{
				if (ttb->Entries[entry].key32to63 == hash32)
				{
					entryToReplace = entry;
					break;
				}
				else if (ttb->Entries[entry].subTreeDepth < shallowestSubTreeDepth)
				{
					entryToReplace = entry;
					shallowestSubTreeDepth = ttb->Entries[entry].subTreeDepth;
				}
			}
			assert(entryToReplace < MateTranspositionTableEntriesPerBucket);

			UINT8 entryToReplaceFlagEUL = ttb->Entries[entryToReplace].flag & TTFlagEULMask;//WHAT IF IT'S AN UNUSED ENTRY?!!! :O

			//if ((depthRemaining >= ttb->Entries[entryToReplace].subTreeDepth) || (TranspositionTableAge != (ttb->Entries[entryToReplace].flag & TTAgeMask)) || (score > WinningBaseScore)) // (+2.6, +/-4.5, 12890) having the extra 'winning' clause
			if (
				(depthRemaining >= ttb->Entries[entryToReplace].subTreeDepth)
				|| (TranspositionTableAge != (ttb->Entries[entryToReplace].flag & TTFlagAgeMask)) // From a previous move?
				|| ((score >= WinningBaseScore) && (flagEUL != TTFlagUpper))
				)
			{
				if (
					(ttb->Entries[entryToReplace].key32to63 == hash32)
					&& (flagEUL != TTFlagUpper)
					&& (
					((ttb->Entries[entryToReplace].score == WinningBaseScore) && (score < WinningBaseScore))
						|| ((ttb->Entries[entryToReplace].score >= MatingScore) && (ttb->Entries[entryToReplace].score > score + ply))
						)
					)
					return; // Don't replace winning/mating scores for the same position with worse scores. Very useful in e.g. KBNvK as it helps with the ever increasing mate distance problem

				if (!ttb->locked.exchange(true))
				{
					//ttb->Entries[entryToReplace].hash64 = hash64;
					ttb->Entries[entryToReplace].key32to63 = hash32;
					// 'Correct' any mate scores for distance (because they are relative to the root position not to this position)
					if (score >= MatingScore)
					{
						score += ply;
						if (
							(score == MateBaseScore - 1)
							&& ((flag & TTFlagEULMask) != TTFlagUpper)
							) // If we have a #1 at an 'exact' or 'cut' node then set its depthRemaining to at least 1
							depthRemaining = std::max(depthRemaining, (SINT8)1);
					}
					else if (score <= MatedScore)
					{
						score -= ply;
						flag |= TTFlagThreatenedWithMate;
					}
					//WHAT EXACTLY IS WRONG WITH THAT 2ND CLAUSE BELOW???
					ttb->Entries[entryToReplace].subTreeDepth = depthRemaining;
					//ttb->Entries[entryToReplace].subTreeDepth = std::min((int)depthRemaining, TC.MateInN * 2 - ply); // Because the Mate search limits the depth to TC.MateInN * 2 the subTreeDepth has to be adjusted accordingly
					ttb->Entries[entryToReplace].plyToCeiling = (TC.MateInN * 2) - ply;
					ttb->Entries[entryToReplace].score = score;
					//ttb->Entries[entryToReplace].staticEvaluation = tteStaticEvaluation;
					ttb->Entries[entryToReplace].flag = TranspositionTableAge | flag;
					ttb->Entries[entryToReplace].bestMove = MGCompressMove(bestMove);
					ttb->locked = false;
				}
			}
		}
}

#pragma endregion

//----------------------------------------------------------------------------------------------------

short Mate::TreeSearchMate(short alpha, short beta, int ply, int depthRemaining, int sideToMove, int isInCheck)//, bool allowNull, bool isCutNode, int currentLineExpense)
{
	assert(CompareMailboxBoard64ToPiecesBB(mateBrain.mailboxBoard64, mateBrain.piecesBB));
	assert((PopulationCountX(mateBrain.piecesBB[0][King]) == 1) && (PopulationCountX(mateBrain.piecesBB[1][King]) == 1));
	assert((PopulationCountX(mateBrain.piecesBB[0][Queen]) <= 9) && (PopulationCountX(mateBrain.piecesBB[1][Queen]) <= 9));
	assert((PopulationCountX(mateBrain.piecesBB[0][Rook]) <= 10) && (PopulationCountX(mateBrain.piecesBB[1][Rook]) <= 10));
	assert((PopulationCountX(mateBrain.piecesBB[0][Bishop]) <= 10) && (PopulationCountX(mateBrain.piecesBB[1][Bishop]) <= 10));
	assert((PopulationCountX(mateBrain.piecesBB[0][Knight]) <= 10) && (PopulationCountX(mateBrain.piecesBB[1][Knight]) <= 10));
	assert((PopulationCountX(mateBrain.piecesBB[0][Pawn]) <= 8) && (PopulationCountX(mateBrain.piecesBB[1][Pawn]) <= 8));
	assert(mateBrain.piecesBB[0][AllPieces] == (mateBrain.piecesBB[0][Pawn] | mateBrain.piecesBB[0][Knight] | mateBrain.piecesBB[0][Bishop] | mateBrain.piecesBB[0][Rook] | mateBrain.piecesBB[0][Queen] | mateBrain.piecesBB[0][King]));
	assert(mateBrain.piecesBB[1][AllPieces] == (mateBrain.piecesBB[1][Pawn] | mateBrain.piecesBB[1][Knight] | mateBrain.piecesBB[1][Bishop] | mateBrain.piecesBB[1][Rook] | mateBrain.piecesBB[1][Queen] | mateBrain.piecesBB[1][King]));
	assert(mateBrain.gameRecordPointer->transpositionTableHash64 == ((sideToMove == 0) ? GenerateTranspositionTableHash64(mateBrain.mailboxBoard64, mateBrain.gameRecordPointer) : ~GenerateTranspositionTableHash64(mateBrain.mailboxBoard64, mateBrain.gameRecordPointer)));
	assert(mateBrain.gameRecordPointer->transpositionTableHash64WithEP == mateBrain.gameRecordPointer->transpositionTableHash64 ^ TranspositionTableRandomsEnPassant[mateBrain.gameRecordPointer->epSquare]);
	assert((ply >= 1) && (ply <= MaximumPly));
	assert(depthRemaining <= MaximumPly);
	assert((sideToMove >= 0) && (sideToMove < Sides));
	assert(-MateBaseScore <= alpha && alpha < beta && beta <= MateBaseScore);
	//assert((alpha < MatingScore) || (ply <= (MateBaseScore - alpha)));THIS FAILS... WHY???


	// always does even # of ply
	// treats odd/even plies as an indivisible pair
	// the attacker move can cause the pair to extend e.g. a check, TMI1
	// the defender's posn can cause the pair to extend e.g. incheck,spite-check,O1M,ZLKM,OKCM
	// only return mat balance after defender's move decides nothing happening
	// can use reductions to prioritise quiet moves later

	//----------------------------------------------------------------------------------------------------

	// Preamble

	// Stopping? (N.B. Must do this here as well as below in the main move processing loop else it may go back up the tree after a reduced search!)
	if (StopImmediately)
		return -MateBaseScore;

	if (ply > MaximumPlyReached)
	{
		MaximumPlyReached = ply;
		//Output("MaximumPlyReached=" + MyITOA(MaximumPlyReached));
		if (IsDebug)
			LongestLine = mateBrain.CurrentLine(ply - 1) + " (Iteration:" + MyITOA(IterationPly) + " Ply:" + MyITOA(ply - 1) + " Alpha:" + MyITOA(alpha) + " Beta:" + MyITOA(beta) + ")";
	}

#ifdef SEARCHINGFORLINE
	TargetLineLastSearched[ply].ui32 = PVTUnknown;
	TargetLineLastSearched[ply].mf.flag = 0;
#endif

	//----------------------------------------------------------------------------------------------------

	int initialMaterialBalance = mateBrain.gameRecordPointer->totalMaterial[sideToMove] - mateBrain.gameRecordPointer->totalMaterial[sideToMove ^ 1];
	int kingDistance = ManhattanDistance[BitScanForwardX(mateBrain.piecesBB[0][King])][BitScanForwardX(mateBrain.piecesBB[1][King])];
	if (ply & 1)
		initialMaterialBalance -= kingDistance; // Give small bonus for the attacking K approaching the defending K
	else
		initialMaterialBalance += kingDistance;
	int checksCount = 999; // WHAT IS THIS FOR??? used in cangivemateinone for some reason???

	//----------------------------------------------------------------------------------------------------

	if (ply & 1) // At an odd ply? (i.e. the attacker is to move)
	{
		if (ply > 1) // Don't do leaf node tests at the root because we want the main body of the search to display status/result messages
			if ((ply >= (TC.MateInN * 2) - 1) || (depthRemaining <= 0)) // Leaf node?
			{
				// N.B. this section always returns a result without going into the main body of the search
				// If we are at the penultimate ply or have no search depth remaining then we must deliver mate in 1 here otherwise we return the standpat score
				
				// Can the attacker mate the defender immediately this move?
				Move_Struct tmi1 = CanGiveMateInOne(sideToMove, isInCheck, checksCount);
				if (tmi1.ui32)
				{
					*mateBrain.gameRecordPointer->principalVariationPointer = tmi1.ui32; // Return mating move as part of pv
					*(mateBrain.gameRecordPointer->principalVariationPointer + 1) = PVTCheckmate;
					return (MateBaseScore - ply - 1);
				}

				*mateBrain.gameRecordPointer->principalVariationPointer = PVTStandPat;
				return initialMaterialBalance;
			}
	}

	//----------------------------------------------------------------------------------------------------

	assert(depthRemaining > 0);

	bool isPVNode = (alpha != beta - 1);
	assert((ply < TC.MateInN * 2 - 1) || (TC.MateInN == 1));

	if (ply > 1)
	{
		// Drawn?
		int pliesSinceIrreversible = mateBrain.gameRecordPointer->pliesSinceIrreversible;
		if (pliesSinceIrreversible >= 3)
		{
			short ds = 0;// DrawScore(sideToMove);

			if (ds > alpha) // Immediate repetition possible? ONLY NEED TO DO THIS AT EVEN PLIES IF WE USE ALPHA=0 INITIALLY
			{
				if (
					(((mateBrain.gameRecordPointer - 1)->move.mf.fromSquare) == ((mateBrain.gameRecordPointer - 3)->move.mf.toSquare))
					&& (((mateBrain.gameRecordPointer - 1)->move.mf.toSquare) == ((mateBrain.gameRecordPointer - 3)->move.mf.fromSquare))
					) // Did the opponent just undo his previous move?
				{
					if (ds >= beta)
					{
						*mateBrain.gameRecordPointer->principalVariationPointer = PVTDrawImmediateRepetition;
						return ds;//CAN THIS BE +INF AT EVEN PLIES??? SHOULD NEVER HAPPEN AT ODD PLIES!
					}
				}
			}

			if (pliesSinceIrreversible >= 4)
			{
				// About 9% of nodes get tested here
				for (int i = 4; i <= pliesSinceIrreversible; i += 2) // Repetition?
					if ((mateBrain.gameRecordPointer - i)->transpositionTableHash64 == mateBrain.gameRecordPointer->transpositionTableHash64)
					{
						*mateBrain.gameRecordPointer->principalVariationPointer = PVTDrawByRepetition;
						short result = WinningBaseScore;
						if (ply & 1)
							result = -WinningBaseScore;
						return result;// ds;
					}
			}
		}

		//----------------------------------------------------------------------------------------------------

		// 'Mate-distance pruning' (Helps massively when we have a mate score)
		// Ensures that we don't go deeper than the mate we've already got...
		// e.g. if STM at root has a #10 we won't go deeper than ply=19
		// e.g. if SNTM at root has a #10 we won't go deeper than ply=20
		// (In the 'alpha' line below we could test for 'isInCheck' and add +2 if we're not, but although it does make a slight difference it seems to harm the search depth rather than helping it!?)
		// This is also included in the QS because reductions can cause you to enter the QS early and bypass this test in main.
		// Because EITHER alpha gets increased, OR beta gets decreased, but not BOTH, you can omit the alpha test to save a few cycles during most normal searches with NO change in node count when you are mating (because the beta test is always hit a ply sooner than the alpha test)
		// In fact, I think that the alpha test only ever does something if alpha has been set to -INF
		//alpha = std::max(-MateBaseScore + ply, (int)alpha); // If the worst possible score for the side to move in this position (i.e. being mated here) is > alpha, then increase alpha
		beta = std::min(MateBaseScore - ply - 1, (int)beta); // If the best possible score for the side to move in this position (i.e. giving mate in 1) < beta, then decrease beta
		if (alpha >= beta)
			return alpha;

	}

	//----------------------------------------------------------------------------------------------------

	NodeCount++;

	// Process any queued messages every half a second
	if ((NodeCount & 255) == 0)
		if (MessagesQueued)
			if (ThreadId == 0)
				if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - MessagesLastDisplayedClock).count() > 500)
					ShowQueuedMessages();

	//----------------------------------------------------------------------------------------------------

	// Initialisation CAN THESE BE MOVED AFTER TT PROBE???
	alignas(64) MoveWithScore_Struct moveList[220];
	short bestMoveScore = -MateBaseScore; // If anything takes over as best (a 'pv' or 'cut' node) then bestMoveScore will be equal to alpha. If nothing takes over as best (an 'all' node) then bestMoveScore will be less than alpha and will be a more accurate upper bound.
	short originalAlpha = alpha;
	int legalMovesMade;
	Move_Struct currentMove;
	short currentMoveScore;

	//----------------------------------------------------------------------------------------------------

	// Up-date tree search variables
	mateBrain.gameRecordPointer->isInCheck = isInCheck;
	mateBrain.gameRecordPointer->isTWM = 0; // These may get set if we find a TT entry
	mateBrain.gameRecordPointer->isO1M = 0;
	mateBrain.gameRecordPointer->isFMTP = 0;
	mateBrain.gameRecordPointer->isO1PCM = 0;
	mateBrain.gameRecordPointer->isZLKM = 0;
	*mateBrain.gameRecordPointer->principalVariationPointer = PVTUnknown; // Terminator

	//----------------------------------------------------------------------------------------------------

#pragma region TT
	// Transposition tables in mate mode are tricky because we use a ceiling at MD*2
	// So say we were looking for a #7...
	// if we took 3 moves to get to position X which is a #5 and all the moves were forced and they extended, we still wouldn't find the #5 because we would hit the ceiling first and thus a draw gets put in the TT
	// if we then took 2 moves to get to position X (with the same draft) we would use the draw from the TT rather than searching and this time finding the #5!
	// We therefore have to test the draft AND the distance to the ceiling (BUT ONLY FOR DRAWS... NOT FOR MATES)

	// just use posns from same depth within same iter? with same depth and dr?
	// except winning score posns?
	// always keep/use mating posns? (set dr to +inf?)
	// can always use mating scores but draw scores may be faulty so need to check depth/dr/whatever as well

	// Is this position in the tranposition table? (>50% of the time even with a modest table)
	//MateTranspositionTableEntry_Struct tte; // Used later in singular extensions
	int tteBestMove = PVTUnknown;
	SINT8 tteSubTreeDepth = -99;
	SINT8 ttePlyToCeiling = -99;
	UINT8 tteEUL = 0;
	short tteScore = 0;
	//if (0)
	if ((MateTranspositionTableBuckets > 0) && (ply > 1)) // ply will always be < TC.MateInN * 2 - 1
	{
		UINT64 hash64 = mateBrain.gameRecordPointer->transpositionTableHash64WithEP;
		MateTranspositionTableBucket_Struct* ttb = MateTranspositionTablePointer + (hash64 & MateTranspositionTableBucketsMask);

		if (!ttb->locked.exchange(true))
		{
			UINT32 hash32 = (hash64 >> 32);

			for (int entry = 0; entry < MateTranspositionTableEntriesPerBucket; entry++) // Do we already have this position in the table?
				if (ttb->Entries[entry].key32to63 == hash32)
				{
					// Lightly validate the 'best move' to catch some transposition table type-2 errors
					bool probableType2Error = false;
					tteBestMove = MGUnCompressMove(ttb->Entries[entry].bestMove); // Will either be PVRUnknown or PVRStandPat or PVREGTB or a legal move
					assert((tteBestMove == PVTUnknown) || (tteBestMove == PVTStandPat) || (tteBestMove == PVTEGTB) || ((UINT16)tteBestMove > 0));

					if ((UINT16)tteBestMove > 0)
					{
						int fromSquarePiece, toSquarePiece;
						fromSquarePiece = mateBrain.mailboxBoard64[tteBestMove & 255];
						if (UCI_Chess960)
						{
							if (fromSquarePiece == 0)
								probableType2Error = true;
						}
						else
						{
							toSquarePiece = mateBrain.mailboxBoard64[((UINT16)tteBestMove) >> 8];
							if (sideToMove == 0)
							{
								if ((fromSquarePiece <= 0) || (toSquarePiece > 0))
									probableType2Error = true;
							}
							else
							{
								if ((fromSquarePiece >= 0) || (toSquarePiece < 0))
									probableType2Error = true;
							}
						}
					}

					if (probableType2Error)
					{
						tteBestMove = PVTUnknown;
						//OutputError("Probable type-2 error");//TEMP
					}
					else
					{
						tteSubTreeDepth = ttb->Entries[entry].subTreeDepth;
						ttePlyToCeiling = ttb->Entries[entry].plyToCeiling;
						tteEUL = (UINT8)(ttb->Entries[entry].flag & TTFlagEULMask);
						tteScore = ttb->Entries[entry].score;

						if (abs(tteScore) >= WinningBaseScore)
						{
							if (tteScore >= WinningBaseScore) // A 'winning' score is a lower bound
							{
								if (tteScore >= MatingScore)
									tteScore -= ply;

								if (tteEUL != TTFlagUpper)
								{
									if ((tteScore >= beta) || (tteScore == MateBaseScore - 1 - ply))
									{
										tteSubTreeDepth = MaximumPly; // We have a winning score that will cause a cutoff (or can't be improved on) so use it regardless of depthRemaining
										ttePlyToCeiling = MaximumPly;
									}
								}
							}
							else // A 'losing' score is an upper bound
							{
								if (tteScore <= MatedScore)
									tteScore += ply;

								if (tteEUL == TTFlagUpper)
								{
									if (tteScore <= alpha)
									{
										tteSubTreeDepth = MaximumPly;
										ttePlyToCeiling = MaximumPly;
									}
								}
							}
						}

						if (tteSubTreeDepth >= depthRemaining)
							if (ttePlyToCeiling >= (TC.MateInN * 2) - ply)
							{
								//if (!isPVNode) // Don't use TT values at a PV node to avoid search inconsistencies
								{
									if (tteEUL == TTFlagLower) // Lower limit? (Came from a Cut node: exact value is "at least" (>=) this value)
									{
										if (tteScore >= beta)
										{
											if ((UINT16)tteBestMove > 0) // Update killer moves? (~+1.5 ELO)
												UpdateKillers(tteBestMove, ply);
											PRINTTREE(PrintTree(IterationPly, ply, alpha, beta, depthRemaining, -4, tteScore, 0);)
												ttb->locked = false;
											return tteScore; // We can exit because we know that at least one move will exceed current beta
										}
									}
									else if (tteEUL == TTFlagUpper) // Upper limit? (Came from an All node: exact value is "at most" (<=) this value)
									{
										if (tteScore <= alpha)
										{
											PRINTTREE(PrintTree(IterationPly, ply, alpha, beta, depthRemaining, -3, tteScore, 0);)
												ttb->locked = false;
											return tteScore; // We can exit because we know that no move will exceed current alpha
										}
									}
									else // Exact value? (Came from a PV node)
									{
										if ((UINT16)tteBestMove > 0) // Update killer moves? (~+5.5 ELO)
											UpdateKillers(tteBestMove, ply);

										*mateBrain.gameRecordPointer->principalVariationPointer = MGUnCompressMove(ttb->Entries[entry].bestMove); // Return best move as part of pv
										*(mateBrain.gameRecordPointer->principalVariationPointer + 1) = PVTTTExact;
										PRINTTREE(PrintTree(IterationPly, ply, alpha, beta, depthRemaining, -2, tteScore, 0);)
											ttb->locked = false;
										return tteScore; // We can exit because we have an exact value
									}
								}
							}

						mateBrain.gameRecordPointer->isTWM = ttb->Entries[entry].flag & TTFlagThreatenedWithMate;
						mateBrain.gameRecordPointer->isO1M = ttb->Entries[entry].flag & TTFlagOnlyOneLegalMove;
						mateBrain.gameRecordPointer->isFMTP = ttb->Entries[entry].flag & TTFlagFewerMovesThanPieces;
					}

					break;
				}

			//if (MatingPositionsTablePointer[~hash64 & MatingPositionsTableMask] == ~hash64)
			//{
			//	MateBrain.gameRecordPointer->isTWM |= TTFlagThreatenedWithMate;
			//}

			ttb->locked = false;
		}
	}
#pragma endregion

	//----------------------------------------------------------------------------------------------------

	if (mateBrain.KnownLowMaterialDraws(sideToMove) == PVTDrawMinimumMaterial)
	{
		*mateBrain.gameRecordPointer->principalVariationPointer = PVTDrawMinimumMaterial;
		short result = WinningBaseScore;
		if (ply & 1)
			result = -WinningBaseScore;
		return result;
	}

	//----------------------------------------------------------------------------------------------------

	//// Testing for a quick win can improve solution times AND SLOW DOWN TOO!
	//if (ply > 1)
	//	if (ply & 1) // At an odd ply? (i.e. the attacker is to move)
	//		if (!isInCheck)
	//	{
	//		// Testing for an immediate mate speeds up the search
	//		Move_Struct tmi1 = CanGiveMateInOne(sideToMove, isInCheck, checksCount);
	//		if (tmi1.ui32)
	//		{
	//			*mateBrain.gameRecordPointer->principalVariationPointer = tmi1.ui32; // Return mating move as part of pv
	//			*(mateBrain.gameRecordPointer->principalVariationPointer + 1) = PVTCheckmate;

	//			//AddToMateTranspositionTable(depthRemaining, ply, MateBaseScore - ply - 1, TTFlagExact, *MateBrain.gameRecordPointer->principalVariationPointer, 0); DOESN'T HELP

	//			return (MateBaseScore - ply - 1);
	//		}
	//	}

	//----------------------------------------------------------------------------------------------------
	
	// Static evaluation
	//MateBrain.gameRecordPointer->staticEvaluation = initialMaterialBalance;//DO THIS AT NODE ENTRY NOW - NOT NEEDED???

	//----------------------------------------------------------------------------------------------------

	int defenderSpiteChecksSaved = defenderSpiteChecks;

	mateBrain.gameRecordPointer->isTWM = 0;

	// Are we following a 'forcing' line? (Only maintained at odd plies)
	if (ply & 1)
		mateBrain.gameRecordPointer->forcingLine = (mateBrain.gameRecordPointer - 2)->forcingLine & (mateBrain.gameRecordPointer - 2)->forcingMove;

	//----------------------------------------------------------------------------------------------------

	// Generate move list
	mateBrain.CalculatePinnedPieces(sideToMove); // Required for legal move generation

	int movesCount;
	movesCount = mateBrain.GenerateAllMoves(sideToMove, isInCheck, moveList);

	//----------------------------------------------------------------------------------------------------

	if (movesCount == 0) // No legal moves generated?
	{
		assert((tteBestMove == PVTUnknown) || (tteBestMove == PVTStandPat) || (tteBestMove == PVTEGTB));

		// Is the side to move in check?
		if (isInCheck)
		{ // Checkmate
			//assert(IsMated(sideToMove));
			assert(tteBestMove == PVTUnknown);
			*mateBrain.gameRecordPointer->principalVariationPointer = PVTCheckmate;
			return (short)(-MateBaseScore + ply);
		}
		else
		{ // Stalemate
			*mateBrain.gameRecordPointer->principalVariationPointer = PVTDrawStalemate;
			short result = WinningBaseScore;
			if (ply & 1)
				result = -WinningBaseScore;
			return result;
		}
	}

	//----------------------------------------------------------------------------------------------------

	if (movesCount == 1)
	{
		mateBrain.gameRecordPointer->isO1M = TTFlagOnlyOneLegalMove;
		mateBrain.gameRecordPointer->isO1PCM = TTFlagOnlyOnePieceCanMove;
		if ((ply & 1) == 0)
		{
			(mateBrain.gameRecordPointer - 1)->forcingMove = true;
			(mateBrain.gameRecordPointer - 1)->forcingLine = (mateBrain.gameRecordPointer - 3)->forcingLine;
			//(MateBrain.gameRecordPointer - 1)->expense = 0;
		}
	}

	//----------------------------------------------------------------------------------------------------
	
	int nonEdgeMoves = 0;
	int reductionsFixedPieces = 0;
	int kingMoves = 0;
	if ((ply & 1) == 0) // At even ply? (Defender's move)
	{
		// Calculate the number of king moves, whether only one piece can move (ALREADY DONE ABOVE???) and whether fixed pieces have been released
		int kingSquare = BitScanForwardX(mateBrain.piecesBB[sideToMove][King]);
		int captures = 0;
		int releasedPieces = 0;
		bool fixedPieceReleased = false;
		mateBrain.gameRecordPointer->zLMPiecesBB = (mateBrain.gameRecordPointer - 2)->zLMPiecesBB;
		for (int moveListIndexIterator = 0; moveListIndexIterator < movesCount; moveListIndexIterator++)
		{
			UINT64 fromSquareBB = UINT64SetBit(moveList[moveListIndexIterator].mf.fromSquare);

			if (RootFixedPiecesBB & fromSquareBB) // Have we allowed a defender's 'fixed' piece (specified in the MateFixedPieces option) to move?
			{
				*mateBrain.gameRecordPointer->principalVariationPointer = PVTStandPat;
				return WinningBaseScore;// initialMaterialBalance;
			}

			if (moveList[moveListIndexIterator].mf.fromSquare == kingSquare)
				kingMoves++;

			if (mateBrain.mailboxBoard64[moveList[moveListIndexIterator].mf.toSquare])
				captures++;

			if (!(EdgesBB & UINT64SetBit(moveList[moveListIndexIterator].mf.toSquare)))
				nonEdgeMoves++;

			//if (RootFixedPiecesMailboxBoard64[moveList[moveListIndexIterator].mf.fromSquare])MODIFY VALUE SO THAT ONLY PENALISED ONCE FOR EACH RELEASE
			//{
			//	if (depthRemaining == 1)//???
			//	{
			//		*MateBrain.gameRecordPointer->principalVariationPointer = PVRStandPat;
			//		return initialMaterialBalance;
			//	}
			//	fixedPieceReleased = true;
			//}

			if (mateBrain.gameRecordPointer->zLMPiecesBB & fromSquareBB)
			{
				releasedPieces++;
				mateBrain.gameRecordPointer->zLMPiecesBB ^= fromSquareBB;
			}
		}
		mateBrain.gameRecordPointer->DefenderKingMovesBefore = kingMoves;
		mateBrain.gameRecordPointer->TotalDefenderKingMovesBefore = (mateBrain.gameRecordPointer - 2)->TotalDefenderKingMovesBefore + kingMoves;
		if (releasedPieces > 0)//SEEMS TO MAKE THINGS A BIT SLOWER OVERALL BUT MUCH BETTER ON SOME - speeds up but causes some mates to take extra iteration
			reductionsFixedPieces = releasedPieces;
		mateBrain.gameRecordPointer->isZLKM = (kingMoves == 0);
		mateBrain.gameRecordPointer->isOKCM = (kingMoves == movesCount);
		//if (kingMoves > 0)
		//{
		//	if ((MateBrain.gameRecordPointer - 2)->TotalDefenderKingMovesBefore == 0)
		//		reductionsFixedPieces = 1;//THIS HELPS A BIT BUT NOT MUCH
		//}
		//else
		//{
		//	if (ply == 2)
		//		MateBrain.gameRecordPointer->TotalDefenderKingMovesBefore = 0; THIS SLOWS DOWN SOLNS A BIT THOUGH NOT REALLY SURE WHY?!?!?! as surely iy only leads to more reductions
		//}

		mateBrain.gameRecordPointer->isO1PCM = ((moveList[0].mf.fromSquare == moveList[movesCount - 1].mf.fromSquare) && (captures == 0));

		if (isInCheck || mateBrain.gameRecordPointer->isO1M || mateBrain.gameRecordPointer->isOKCM || mateBrain.gameRecordPointer->isZLKM)
		{
			(mateBrain.gameRecordPointer - 1)->forcingMove = true;
			(mateBrain.gameRecordPointer)->forcingMove = true;
		}


		//if (fixedPieceReleased)
		//	depthRemaining--;//CUTOFF??? IS THIS PIECES THAT HAD ZERO MOVES AT THE ROOT? OR PIECES SPECIFIED BY COMMAND???

		//if ((kingMoves == 0) && mateBrain.gameRecordPointer->isO1PCM)//SOME FORCING MOVES ARE LESS FORCING THAN OTHERS!
		//{
		//	(mateBrain.gameRecordPointer - 1)->forcingMove = true;
		//	(mateBrain.gameRecordPointer - 1)->forcingLine = (mateBrain.gameRecordPointer - 3)->forcingLine;
		//}
	}

	assert(NoDuplicateMoves(moveList, movesCount));
	assert(TranpositionTableMoveFound(moveList, movesCount, tteBestMove));

	//----------------------------------------------------------------------------------------------------

	// Score moves for ordering
	SINT8 pt1, pt2, ts1, ts2;

	// Get the previous move details
	pt1 = abs((mateBrain.gameRecordPointer - 1)->move.fromSquarePiece) - 1; // 0..5
	ts1 = (mateBrain.gameRecordPointer - 1)->move.mf.toSquare;
	assert((pt1 >= Pawn - 1) && (pt1 <= King - 1) && (ts1 >= A1) && (ts1 <= H8));
	//mateBrain.gameRecordPointer->historyPointer = &CounterMoveHistory[pt1][ts1];
	mateBrain.gameRecordPointer->historyPointer = &CounterMoveHistory->CMH[pt1][ts1];

	SINT8 fupt1, futs1;
	fupt1 = abs((mateBrain.gameRecordPointer - 2)->move.fromSquarePiece) - 1;//TEST REMOVING FUMs FOR SPEED
	assert((fupt1 >= 0) && (fupt1 <= 5));
	futs1 = (mateBrain.gameRecordPointer - 2)->move.mf.toSquare;

	int enemyKingSquare = BitScanForwardX(mateBrain.piecesBB[sideToMove ^ 1][King]);

	if (ply == 1)
		ScoreRootMoveList(moveList);
	else
		mateBrain.ScoreMovesMateMode(moveList, movesCount, tteBestMove, ply, KillerMoves, &CounterMoves[pt1][ts1], &FollowUpMoves[fupt1][futs1], enemyKingSquare, (mateBrain.gameRecordPointer - 2 + ((ply & 1) ? 0 : 1))->isThreateningMateInOne);

	//----------------------------------------------------------------------------------------------------

	// DO TWO PASSES? 1ST: TTMOVE, CAPS, CHECKS 2ND:REST
	// Loop through move list
	legalMovesMade = 0;
	bool hasExtended = false;

	int pass = 1;

	bool keepScanning = true;
LoopThroughMoveList:
	for (int moveListIndexIterator = 0; moveListIndexIterator < movesCount; moveListIndexIterator++)
	{
		// Get the next move
		int bestSortScore = moveList[moveListIndexIterator].score;
		int bestSortIndex = moveListIndexIterator;

		////if (keepScanning && ((ply < TC.MateInN * 2 - 1)))
		//if (keepScanning)
		//{
		//	for (int index = moveListIndexIterator + 1; index < movesCount; index++)
		//	{
		//		if (moveList[index].score > bestSortScore)
		//		{
		//			bestSortScore = moveList[index].score;
		//			bestSortIndex = index;
		//		}
		//	}

		//	// Give up scanning for highest scoring move if it's likely an 'all' node
		//	//if ((moveListIndexIterator >= (movesCount >> 1)) && (moveListIndexIterator > 9))//2nd CLAUSE SHOULD BE 'PASSED USUAL CANDIDATES' E.G. bestSortScore < 1<<23
		//	//if ((moveListIndexIterator >= (movesCount >> 1)) && (bestSortScore < ((1 << 23) - 5)))// Passed 'special' moves? (TT, +ve captures, killers, counter-moves, follow-up-moves)
		//	if ((bestSortScore < ((1 << 23) - 99)))// Past 'special' moves? (TT, +ve captures, killers, counter-moves, follow-up-moves and the highest-scoring history move)
		//		keepScanning = false;
		//}

		currentMove.ui32 = moveList[bestSortIndex].ui32;
		mateBrain.gameRecordPointer->move.ui32 = currentMove.ui32;
		//moveList[bestSortIndex] = moveList[moveListIndexIterator]; // Re-position the first move in the list. N.B. this must be AFTER the 'if (ply == 1)' paragraph above!NOT ANY MORE

		//----------------------------------------------------------------------------------------------------

		//// SEE (used for reductions)
		//int SEEResult;
		//SEEResult = 1; // Assume it's a winning LxH
		//if (!isInCheck) // Don't (SEE) reduce if in check as might just be a delaying move
		//{
		//	// Calculate the SEE result for moves to empty squares
		//	if (ply & 1)
		//	{THE LINE BELOW IS USED TO IMMEDIATELY EXCLUDE K MOVES!!! SO PUT IT BACK
		//		//if (SeeLowHighValues[abs(MateBrain.mailboxBoard64[MateBrain.gameRecordPointer->move.mf.fromSquare])] > SeeLowHighValues[abs(MateBrain.mailboxBoard64[MateBrain.gameRecordPointer->move.mf.toSquare])])
		//		if (MateBrain.mailboxBoard64[MateBrain.gameRecordPointer->move.mf.toSquare] == Empty)
		//			SEEResult = MateBrain.SEE(MateBrain.gameRecordPointer->move.mf.fromSquare, MateBrain.gameRecordPointer->move.mf.toSquare, sideToMove); // Calculate the SEE result for HxL
		//	}
		//}

		//----------------------------------------------------------------------------------------------------

		// Up-date move
		legalMovesMade++;
		SINT8 toSquarePiece = mateBrain.mailboxBoard64[currentMove.mf.toSquare]; // Needed later to determine if the move is a capture
		mateBrain.MakeMove(sideToMove); // N.B. MakeMove increments MateBrain.gameRecordPointer!

		// Does the move give check? NON-CHECKS DEFERRED STUFF
		//bool givesCheck;
		//if (
		//	(bestSortScore > -99) // Is this a non-deferred move?
		//	|| (ply == TC.MateInN * 2 - 1)
		//	)
		//	givesCheck = mateBrain.IsEnemyKingAttacked(enemyKingSquare, sideToMove);
		//else
		//	givesCheck = false;

		bool givesCheck = mateBrain.IsEnemyKingAttacked(enemyKingSquare, sideToMove);
		if ((pass == 1) && !givesCheck && (ply & 1))
		{
			mateBrain.UnMakeMove(sideToMove); // N.B. UnMakeMove decrements MateBrain.gameRecordPointer!
			continue;
		}
		if ((pass == 2) && givesCheck)
		{
			mateBrain.UnMakeMove(sideToMove); // N.B. UnMakeMove decrements MateBrain.gameRecordPointer!
			continue;
		}



		if (ply == 1)
		{
			ShowProgressMessage(currentMove.ui32, moveListIndexIterator + 1, bestMoveScore, alpha, beta); // Display current root move (always display first move)
		}
		PRINTTREE(PrintTree(IterationPly, ply, alpha, beta, depthRemaining, currentMove.ui32, bestSortScore, 0););

			// Initiate the retrieval of the next transposition table cache line as soon as possible
			_mm_prefetch((char*)(MateTranspositionTablePointer + (mateBrain.gameRecordPointer->transpositionTableHash64 & MateTranspositionTableBucketsMask)), _MM_HINT_T0);

		//----------------------------------------------------------------------------------------------------

#ifdef SEARCHINGFORLINE
		STRING cl;
		TargetLineLastSearched[ply] = currentMove;
		if (TargetLineLength == ply)
		{
			cl = MateBrain.CurrentLine(ply);
			//std::cout << cl + "\r";

			if (TargetLine.rfind(cl, 0) == 0)
			{
				if (cl.length() > TargetLinePartial.length())
				{
					TargetLinePartial = cl;
					TargetLinePartialDepthRemaining = depthRemaining;
					//TargetLinePartialThreateningMate = threateningMate;
					//TargetLineRefutedBy = TargetLineLastSearched[ply + 1];
				}
				//TargetLineLength++; //need to clear this at start of next iter?
			}

			if (TargetLine == cl)
				AC8++;

			//TargetLineRefutationsDepth
			if (TargetLineRefutationsDepth == ply)
				Output(MoveNotation(currentMove.ui32));
			else if (TargetLineRefutationsDepth > ply)
				AC8++;

			if (TargetLinePrintDepth == ply)
				Output(MoveNotation(currentMove.ui32));
		}
#endif

		//----------------------------------------------------------------------------------------------------

		// Determine any 'forcing' parameters (makes logic easier to do what we can {i.e. 'threatening mate' and 'zero defender king moves'(???)} after the attacker's moves i.e. at odd plies)


		//int expense = 0;
		int extensions = 0;
		int reductions = 0;

		if (ply & 1) // Odd ply?
		{
			//if ((mateBrain.gameRecordPointer - 2)->isOKCM) NEED TO REFINE THIS FOR ALL 'FORCED LINES FROM THE ROOT'
			//{
			//	extensions = 1;
			//	goto AssignNewDepthRemaining;
			//}

			// Move by a 'fixed' attacker piece?
			UINT64 fromSquareBB = UINT64SetBit(currentMove.mf.fromSquare);
			if (RootFixedPiecesBB & fromSquareBB)
			{
				// if we use MFP for attackers pieces and it gets to a posn where ONLY fixed pieces CAN move, will it return a sensible score ? ? ?
				//should treat it like checkmate/stalemate?

				goto DiscardMove;
			}

			(mateBrain.gameRecordPointer - 1)->forcingMove = false; // Assume it's not a forcing move

			// Checks
			(mateBrain.gameRecordPointer - 1)->givesCheck = givesCheck;
			if (givesCheck) // Giving check?
			{
				// ENDLESS SEQUENCES OF Q CHASING K (OFTEN WITH O1M) AROUND THE BOARD EXPLODE!
				// ONLY ALLOW THE 1ST Q CHECK IN A SEQUENCE!
				// Only extend if check is given by a different piece to previous move and it's reversible! SEEMS TO SLOW SOLNS ON AVG???
				//if ((!(mateBrain.gameRecordPointer - 3)->givesCheck) || ((mateBrain.gameRecordPointer - 1)->pliesSinceIrreversible <= 1) || (currentMove.mf.fromSquare != (mateBrain.gameRecordPointer - 3)->move.mf.toSquare))
				//if (
				//	(abs(mateBrain.mailboxBoard64[currentMove.mf.toSquare]) != Queen)
				//	|| (abs((mateBrain.gameRecordPointer - 3)->move.fromSquarePiece) != Queen)
				//	|| (currentMove.mf.fromSquare != (mateBrain.gameRecordPointer - 3)->move.mf.toSquare)
				//	|| !(mateBrain.gameRecordPointer - 3)->givesCheck
				//	// TEST MATEALLCHECKS HERE TOO!
				//	) // If the SAME queen gives a sequence of checks, only extend the 1st check
					extensions = 2;
				(mateBrain.gameRecordPointer - 1)->forcingMove = true;
				hasExtended = true;
				goto AssignNewDepthRemaining;
			}
			else if (TC.MateAllChecks)
			{
			DiscardMove:
				mateBrain.UnMakeMove(sideToMove); // N.B. UnMakeMove decrements MateBrain.gameRecordPointer!
				continue;
				//*mateBrain.gameRecordPointer->principalVariationPointer = PVTStandPat;
				//currentMoveScore = 0;// -WinningBaseScore;// 0;// MateBrain.gameRecordPointer->totalMaterial[sideToMove] - MateBrain.gameRecordPointer->totalMaterial[sideToMove ^ 1];
				//goto UnMakeMove;
			}

			// TMI1s
			(mateBrain.gameRecordPointer - 1)->isThreateningMateInOne.ui32 = 0;
			if (!givesCheck)
				(mateBrain.gameRecordPointer - 1)->isThreateningMateInOne = mateBrain.ThreateningMateInOneWithNull(sideToMove, checksCount);
			if ((mateBrain.gameRecordPointer - 1)->isThreateningMateInOne.ui32 != 0) // Threatening mate in one?
			{
				(mateBrain.gameRecordPointer - 1)->forcingMove = true;

				// Exclude some TMI1s from extension i.e. leaving the same TMI1 in place for no good reason
				// EVEN WITH THESE EXCLUSIONS WE GET WORSE TIMINGS THAN JUST LEAVING THEM ALL IN!!! :O
				// TRY TO FIND A #6 THAT IS SLOWER AND IDENTIFY MORE CLAUSES e.g. 1B4q1/1p6/4prb1/p3pr1p/P2RBkN1/5ppP/3N1RP1/1K6 w - -
				//if (
				//	((MateBrain.gameRecordPointer - 1)->isThreateningMateInOne.ui32 != (MateBrain.gameRecordPointer - 3)->isThreateningMateInOne.ui32) // Different TMI1 from 2 plies earlier?
				//	|| (currentMove.mf.toSquare == (MateBrain.gameRecordPointer - 1)->isThreateningMateInOne.mf.fromSquare) // Has the TMI1 piece just moved to its square (allows for defender exchanging on that square)
				//	|| (currentMove.mf.toSquare == (MateBrain.gameRecordPointer - 2)->move.mf.toSquare) // Just captured the defender's last moved piece? i.e. a spite check or a sacrificial defence
				//	) // Must be different to two plies earlier
				extensions = 2; // SOMETIMES WE GET POSNS WHERE THERE ARE LOADS OF TMI1 MOVES AND WE JUST OSCILLATE BETWEEN WHICH ONE WE CHOOSE FOR EXTENDING. NEED A BETTER WAY TO IDENTIFY 'NEW' TMI1s
				hasExtended = true;
				//ALSO SOMETIMES WE GET DELAYING MOVES (CHECKS) WHICH JUST DELAY THE SAME TMI1 SO WE SHOULD COUNT IT IN THOSE CASES
			// SOMTIMES THE TMI1 DOESN'T EXIST UNTIL THE ATTACKER PLAYS ITS MOVE... SHOULD ALWAYS COUNT THOSE BUT MAY BE EXPENSIVE??? CHECK IF WE JUST MOVED TO THE SQ THAT TMI1S
			// LEAVING ALL TMI1s IN MAKES THINGS FASTER IN GENeRAL SO BE VERY PICKY ABOUT EXCLUDING THEM!

				goto AssignNewDepthRemaining;
			}
			else if (TC.MateAllThreateningMateInOne)//GET RID OF THIS... NOT QUITE RIGHT WITH CHECKS TAKING PRIORITY
				goto DiscardMove;

			//if ((MateBrain.gameRecordPointer - 1)->TotalDefenderKingMovesAfter == 0) // Defending king consistently corralled?
			//{
			//	(MateBrain.gameRecordPointer - 1)->forcingMove = true;
			//	//extensions = 1;
			//	goto AssignExpense;
			//}

			// An O1M line doesn't extend at the odd ply if it's a quiet move even though it gets marked as forced at the next ply

			//if (newDepthRemaining == 0)
			//	if ((MateBrain.gameRecordPointer - 1)->forcingLine)
			//	{
			//		extensions = 1;
			//		goto AssignExpense;
			//	}

			//if (egtbResult == TB_WIN)
			//{
			//	//(MateBrain.gameRecordPointer - 1)->forcingMove = true;
			//	extensions = 1;
			//	goto AssignExpense;
			//}


			//if (isInCheck && (currentMove.mf.toSquare == (MateBrain.gameRecordPointer - 2)->move.mf.toSquare)) // Capturing a spite checker?
			//{
			//	goto AssignExpense;
			//}

			//if (ply > 1)
			//	if (currentMove.mf.toSquare == (mateBrain.gameRecordPointer - 2)->move.mf.toSquare) // Recapture? THIS IS JUST CAPTURE OF LAST MOVED PIECE
			//		if ((mateBrain.gameRecordPointer - 2)->move.toSquarePiece) // Recapture?
			//		{
			//			extensions = 1;
			//			hasExtended = true;
			//			goto AssignNewDepthRemaining;
			//		}

			//// Quiet moves CAPTURES COST LESS? CAPTURE OF LAST MOVED PIECE ESPECIALLY! MOVING OUT OF CHECK
			//expense = 4;
			//if (ply == TC.MateInN * 2 - 3) // Penultimate attackers move not threatening mate?
			//	if ((MateBrain.gameRecordPointer - 1)->isThreateningMateInOne.ui32 == 0)
			//		expense += 4;
			//if (!(MateBrain.gameRecordPointer - 1)->isAttacking) // Not attacking?
			//	expense += 4;
			//if ((MateBrain.gameRecordPointer - 1)->DefenderKingMovesBefore == 0) // Letting the defending king loose?
			//	expense += 4;



			//// Sliders taking 2 moves instead of 1? SEEMS TO BE SLOWER!!! MAYBE SOME SOLNS REQUIRE IT?
			//if (!isPVNode)
			//	if (mateBrain.gameRecordPointer->pliesSinceIrreversible >= 3)
			//		if (currentMove.mf.fromSquare == (mateBrain.gameRecordPointer - 3)->move.mf.toSquare)
			//			if (!(mateBrain.gameRecordPointer - 2)->isInCheck)
			//				if (!(mateBrain.gameRecordPointer - 1)->isInCheck)
			//					if ((mateBrain.gameRecordPointer - 3)->isThreateningMateInOne.ui32 == 0) // Wasn't TMI1?
			//						if (alpha > 0)//???
			//						{
			//							SINT8 piece = abs(mateBrain.mailboxBoard64[currentMove.mf.toSquare]);
			//							//if (piece == King)
			//							//{
			//							//	if (ChebyshevDistance[(normalBrain.gameRecordPointer - 3)->move.mf.fromSquare][currentMove.mf.toSquare] <= 1)
			//							//		reductions += 1;
			//							//}
			//							//else 
			//							if ((piece == Bishop) || (piece == Rook) || (piece == Queen))
			//								if (LineListBB[currentMove.mf.fromSquare][currentMove.mf.toSquare] & UINT64SetBit((mateBrain.gameRecordPointer - 3)->move.mf.fromSquare))
			//								{
			//									reductions = 1;
			//									goto AssignNewDepthRemaining;
			//								}
			//						}

			//if (SEEResult < 0)
			//{
			//	reductions = 1; // Sometimes a 'losing' SEE move to an empty square actually IS the best move :(
			//	goto AssignNewDepthRemaining;
			//}

			//if (checksCount == 0)//DON'T DO IF WE'RE IN CHECK?
			//{
			//	reductions = 1;
			//	goto AssignNewDepthRemaining;
			//}

			//if (bestSortScore < (1 << 23) - 9) // Don't reduce any special moves (TT, captures/proms, killers, CM, FUM)
			//{
			//	reductions = 1;
			//	goto AssignNewDepthRemaining;
			//}


			// Has any previous move extended (e.g. a check, TMI1 etc) and this is a passive move?
			//if (hasExtended)//SURELY MOST POSNS WILL HAVE A CHECK?
			//if (!isPVNode)
			//	if ((bestSortScore < ((1 << 23) - 99)) && (ply > 1))// Past 'special' moves? (TT, +ve(ALL???) captures, killers, counter-moves, follow-up-moves and the highest-scoring history move)
			//	//if (toSquarePiece == Empty)
			//	{
			//		reductions = 1;
			//		goto AssignNewDepthRemaining;
			//	}

			//// Futility reductions
			//if ((mateBrain.gameRecordPointer->totalMaterial[sideToMove] - mateBrain.gameRecordPointer->totalMaterial[sideToMove ^ 1] < alpha - 900) && (ply > 1))
			//{
			//	reductions = 1;
			//	goto AssignNewDepthRemaining;
			//}





		}
		else // Even ply
		{
		if ((mateBrain.gameRecordPointer - 2)->forcingMove)
		{
			//extensions = 1;
			goto AssignNewDepthRemaining;

			}





			//if (givesCheck) // Giving check? (i.e. a delaying spite check)
			//	//if (defenderSpiteChecks < 2)
			//{
			//	//(MateBrain.gameRecordPointer - 1)->forcingMove = true;
			//	defenderSpiteChecks++;
			//	extensions = 2;
			//	goto AssignNewDepthRemaining;
			//}

			//if (isInCheck)
			//{
			//	bool anyCaptures = false;
			//	for (int i = 0; i < movesCount; i++)
			//		if (MateBrain.mailboxBoard64[moveList[i].mf.toSquare] > Empty)
			//		{
			//			anyCaptures = true;
			//			break;
			//		}
			//	if (!anyCaptures)
			//	{
			//		//(MateBrain.gameRecordPointer - 1)->forcingMove = true;
			//		extensions = 1;
			//		goto AssignNewDepthRemaining;
			//	}
			//}

			//if ((MateBrain.gameRecordPointer - 1)->isThreateningMateInOne.ui32 != 0) // Threatening mate in one? THIS SEEMS TO BE AN OVERALL SPEED LOSS
			//{
			//	//(MateBrain.gameRecordPointer - 1)->forcingMove = true;
			//	extensions = 1;
			//	goto AssignNewDepthRemaining;
			//}


			//if ((MateBrain.gameRecordPointer - 1)->isInCheck)
			//{
			//	//if (currentMove.mf.toSquare == (MateBrain.gameRecordPointer - 2)->move.mf.toSquare)//THIS sometimes STOPS IT FINDING 'ALLCHECKS' SOLNS ON 1ST ITER??? AND SOMETIMES IT FINDS ONE SOLN BUT NOT OTHERS
			//	//	expense = 1;//MAYBE ONLY DO IF NOT GOT 'ALLCHECKS' FLAG SET???
			//	if (
			//		(MateBrain.gameRecordPointer - 2)->forcingMove // THIS IS A BIT RESTRICTIVE? (BECAUSE IF THE FIRST MOVE ISN'T FORCED THE REST IS PENALISED) WHAT IF MULTIPLE OF THESE FACTORS EXIST? what if more than half the line is forcing???
			//		|| (egtbResult == TB_LOSS)
			//		)
			//		extensions = 1;//ALSO COULD EXTEND IF CAN'T CAPTURE CHECKING PIECE
			//	goto AssignNewDepthRemaining;
			//}

			//if ((MateBrain.gameRecordPointer - 2)->isThreateningMateInOne.ui32)
			//{
			//	if (
			//		(MateBrain.gameRecordPointer - 2)->forcingMove
			//		|| (egtbResult == TB_LOSS)
			//		)
			//		extensions = 1;
			//	goto AssignNewDepthRemaining;
			//}

			if ((mateBrain.gameRecordPointer - 1)->isO1M) // N.B. If O1M then OOPCM too!
				if ((mateBrain.gameRecordPointer - 3)->isO1M) // Only extend O1M if there's a sequence of 2 or more (the pre-root position is considered to be O1M)
				{
					//if (
					//	(MateBrain.gameRecordPointer - 2)->forcingMove
					//	|| (MateBrain.gameRecordPointer - 1)->isInCheck MAYBE SHOULD ONLY EXTEND IF NOT IN CHECK?!
					//	|| (MateBrain.gameRecordPointer - 2)->isThreateningMateInOne.ui32
					//	|| (egtbResult == TB_LOSS)
					//	)
					extensions = 2;
					goto AssignNewDepthRemaining;
				}

			//if ((mateBrain.gameRecordPointer - 1)->isO1PCM)//ONLY IF K ON EDGE??? NOT IN CHECK!!! DON'T DO IF DOWN TO LONE K!!!
			//	if (abs(mateBrain.mailboxBoard64[currentMove.mf.toSquare]) == King)//MAYBE NEED THIS TO BE ONLY WHEN THE PIECE HAS ZERO CAPTURES?! COULD SET WHEN WE TEST EARLIER? STILL QUITE EXPENSIVE
			//		if (nonEdgeMoves == 0) // Only extend if the piece is constrained to moves on the edge
			//			if (!isInCheck)
			//				//if (mateBrain.piecesBB[sideToMove][AllPieces] != mateBrain.piecesBB[sideToMove][King]) // Ignore if defender only has lone K
			//				{
			//					//if (
			//					//	(MateBrain.gameRecordPointer - 2)->forcingMove
			//					//	|| (MateBrain.gameRecordPointer - 1)->isInCheck
			//					//	|| (MateBrain.gameRecordPointer - 2)->isThreateningMateInOne.ui32
			//					//	|| (egtbResult == TB_LOSS)
			//					//	)
			//					extensions = 1;
			//					goto AssignNewDepthRemaining;
			//				}

			//if (
			//	((MateBrain.gameRecordPointer - 1)->DefenderKingMovesBefore == 0)
			//	&& (MateBrain.gameRecordPointer - 1)->isOOPCM
			//	)
			//{
			//	if (
			//		(MateBrain.gameRecordPointer - 2)->forcingMove
			//		|| (MateBrain.gameRecordPointer - 1)->isInCheck
			//		|| (MateBrain.gameRecordPointer - 2)->isThreateningMateInOne.ui32
			//		|| (egtbResult == TB_LOSS)
			//		)
			//		extensions = 1;
			//	goto AssignNewDepthRemaining;
			//}

			////if (egtbResult == TB_LOSS) // EXPENSIVE BECAUSE IT EFFECTIVELY EXTENDS ATTACKER MOVES AT THE PREVIOUS PLY THAT GET FURTHER FROM THE MATE!
			////{
			////	//(MateBrain.gameRecordPointer - 1)->forcingMove = true;
			////	extensions = 1;
			////	goto AssignExpense;
			////}

			////if (RootFixedPiecesMailboxBoard64[currentMove.mf.fromSquare])
			////	extensions--;


			////expense = 4;
			////if (-(MateBrain.gameRecordPointer->totalMaterial[sideToMove] - MateBrain.gameRecordPointer->totalMaterial[sideToMove ^ 1]) > RootMaterialBalance)
			////	expense++;

			//if ((bestMoveScore < MatedScore) && (legalMovesMade > 1))//THIS IS PROMISING BUT NEEDS TO BE REFINED
			//{
			//	extensions = 1;
			//	goto AssignNewDepthRemaining;
			//}

			if ((mateBrain.gameRecordPointer - 3)->isZLKM && (mateBrain.gameRecordPointer - 1)->isZLKM)
				//if ((mateBrain.gameRecordPointer - 3)->isZLKM && !(mateBrain.gameRecordPointer - 1)->isZLKM)
			{
				extensions = 2; // Extending can cause some positions to explode e.g. the notorious k2b3b/8/2N2b2/6b1/1b6/b7/3b2K1/b1b2B1n w - - but it still leads to fastest solution time
				//reductions = 1;
				goto AssignNewDepthRemaining;
			}

			if ((mateBrain.gameRecordPointer - 3)->isOKCM && (mateBrain.gameRecordPointer - 1)->isOKCM)
			{
				extensions = 2;
				goto AssignNewDepthRemaining;
			}



			//// No attacker's pieces left?
			//if (mateBrain.piecesBB[sideToMove ^ 1][AllPieces] == (mateBrain.piecesBB[sideToMove ^ 1][King] | mateBrain.piecesBB[sideToMove ^ 1][Pawn]))
			//{
			//	reductions = 1;
			//	goto AssignNewDepthRemaining;
			//}

		}

	AssignNewDepthRemaining:



		//(MateBrain.gameRecordPointer - 1)->expense = expense;

		//expense = CurrentLineExpense(ply);
		//expense = CurrentLineExpense2(ply);
		//if (expense != CurrentLineExpense2(ply))
		//	AC1++;

		int newDepthRemaining = depthRemaining - 1;
		newDepthRemaining += extensions;
		//newDepthRemaining -= reductionsFixedPieces;
		//newDepthRemaining -= reductions;
		//if (kingMoves > 6)
		//	newDepthRemaining--;//SURELY THIS SHOULD ONLY BE DONE AT EVEN PLIES???

		bool doNonReducedSearch;
		int searches = 0;

		//DO WE EVEN BENEFIT FROM DOING A MWS IN MATE MODE???
		//if (doNonReducedSearch)
		{//SURELY THIS SHOULD NOW BE A CUT NODE SO THE CHILD SHOULD BE AN 'ALL' NODE?!?!
			// Do a minimal window search
			searches++;
			//currentMoveScore = (short)-TreeSearchMate((short)(-alpha - 1), (short)-alpha, ply + 1, newDepthRemaining, sideToMove ^ 1, givesCheck, true, !isCutNode, currentLineExpense);
			//currentMoveScore = (short)-TreeSearchMate((short)(-alpha - 1), (short)-alpha, ply + 1, IterationPly - ((currentLineExpense + expense) >> 2), sideToMove ^ 1, givesCheck, true, !isCutNode, currentLineExpense + expense);
			//currentMoveScore = (short)-TreeSearchMate((short)(-alpha - 1), (short)-alpha, ply + 1, IterationPly - ((expense) >> 2), sideToMove ^ 1, givesCheck, true, !isCutNode, currentLineExpense + expense);
			currentMoveScore = (short)-TreeSearchMate((short)(-alpha - 1), (short)-alpha, ply + 1, newDepthRemaining, sideToMove ^ 1, givesCheck);// , true, !isCutNode, currentLineExpense);
			// About 79% of non-reduced searches don't exceed alpha
		}


		//if ((ply & 1) == 0)
		//	if (extensions == 0)
		//		if ((bestMoveScore < MatedScore) && (legalMovesMade > 1) && (currentMoveScore > MatedScore))//THIS IS PROMISING BUT NEEDS TO BE REFINED
		//		{
		//			newDepthRemaining++;
		//			currentMoveScore = (short)-TreeSearchMate((short)(-alpha - 1), (short)-alpha, ply + 1, newDepthRemaining, sideToMove ^ 1, givesCheck);// , true, !isCutNode, currentLineExpense);
		//		}





		if (isPVNode && (legalMovesMade == 1 || (currentMoveScore > alpha && ((ply == 1) || currentMoveScore < beta))))
		{
			//assert(reductions == 0);
			assert(searches <= 1);
			assert(beta > alpha + 1);
			searches++;
			//currentMoveScore = (short)-TreeSearchMate((short)-beta, (short)-alpha, ply + 1, newDepthRemaining, sideToMove ^ 1, givesCheck, true, false, currentLineExpense);
			//currentMoveScore = (short)-TreeSearchMate((short)-beta, (short)-alpha, ply + 1, IterationPly - ((currentLineExpense + expense) >> 2), sideToMove ^ 1, givesCheck, true, false, currentLineExpense + expense);
			//currentMoveScore = (short)-TreeSearchMate((short)-beta, (short)-alpha, ply + 1, IterationPly - ((expense) >> 2), sideToMove ^ 1, givesCheck, true, false, currentLineExpense + expense);
			currentMoveScore = (short)-TreeSearchMate((short)-beta, (short)-alpha, ply + 1, newDepthRemaining, sideToMove ^ 1, givesCheck);// , true, false, currentLineExpense);
			//assert(PVSearchedFirst(ply));


			//if (extensions == 0)
			//	if ((currentMoveScore > alpha) && (currentMoveScore < beta))
			//		if (currentMoveScore<MatingScore)
			//			if (ply&1)
			//	{
			//		AC1++;
			//		currentMoveScore = (short)-TreeSearchMate((short)-beta, (short)-alpha, ply + 1, newDepthRemaining+1, sideToMove ^ 1, givesCheck, true, false, currentLineExpense);
			//	}

		}
		assert((searches > 0) && (searches < 3));


		//----------------------------------------------------------------------------------------------------

#ifdef SEARCHINGFORLINE
		if (TargetLineLength == ply)
		{
			//STRING cl = MateBrain.CurrentLine(ply); THIS IS SET IN THE SECTION ABOVE!

			if (TargetLine == cl)
			{
				TargetLineRefutedBy = TargetLineLastSearched[ply + 1];
				AC9++;
			}

			if (TargetLineRefutationsDepth == ply)
				Output(MoveNotation(currentMove.ui32));
			else if (TargetLineRefutationsDepth > ply)
				AC9++;
		}
#endif

		//----------------------------------------------------------------------------------------------------

	UnMakeMove:
		// Down-date move
		defenderSpiteChecks = defenderSpiteChecksSaved;
		mateBrain.UnMakeMove(sideToMove); // N.B. UnMakeMove decrements MateBrain.gameRecordPointer!

		// Root move?
		if (ply == 1)
			SaveRootNodeCounts(currentMove.ui32);

		//----------------------------------------------------------------------------------------------------

		// Stopping? (N.B. Must do this BEFORE the 'new best move' test below otherwise a partially searched move could take over as best or be added to the TT!)
		if (StopImmediately)
		{
			//OutputLog("Stopping:" + MyITOA(ply));
			return -MateBaseScore;
		}

		//----------------------------------------------------------------------------------------------------

		// New best move?
		if (currentMoveScore > bestMoveScore)
		{
			if (currentMoveScore > alpha)
			{
				//if (currentMoveScore == MateBaseScore - ply - 1)
				//{
				//	// corner/edge/middle : 2/4/5 (no promotions)
				//	// corner/edge/middle : 3/5/6 (promotions)

				//	//EXCLUDE POSNS WHERE ATTACKER HAS PROMOTABLE P ON 7TH

				//	UINT64 occupiedBB = MateBrain.piecesBB[0][AllPieces] | MateBrain.piecesBB[1][AllPieces];
				//	UINT64 pawnPromotionsBB = (((MateBrain.piecesBB[sideToMove][Pawn] & SeventhRankBB[sideToMove] & ~MateBrain.gameRecordPointer->pinnedDiagonalBB) << 8) >> (sideToMove << 4)) & notOccupiedBB;

				//	//if (pawnPromotionsBB == 0)
				//	{
				//		int i = MateBrain.CountKingMoves(sideToMove ^ 1);

				//		if (CornersBB & MateBrain.piecesBB[sideToMove ^ 1][King])
				//		{
				//			if (i > AC2)
				//			{
				//				AC1++;
				//				AC2 = i;
				//				WriteMailboxBoard64(&MateBrain);
				//			}
				//		}
				//		else if (EdgesBB & MateBrain.piecesBB[sideToMove ^ 1][King])
				//		{
				//			if (i > AC3)
				//			{
				//				AC1++;
				//				AC3 = i;
				//				WriteMailboxBoard64(&MateBrain);
				//			}
				//		}
				//		else
				//		{
				//			if (i > AC4)
				//			{
				//				AC1++;
				//				AC4 = i;
				//				WriteMailboxBoard64(&MateBrain);
				//			}
				//		}
				//	}
				//	//else
				//	//	Output(MyUI64TOA(pawnPromotionsBB));
				//}

				if (isPVNode)
					mateBrain.SavePrincipalVariation(currentMove.ui32); // Save the PV even if we (are about to) fail high as it might be useful for IID

				//if ( // 'Quiet' move?
				//	(!(currentMove.mf.flag >= MFPromotionNew)) &&
				//	(MateGenerate.gameRecordPointer->move.toSquarePiece == 0)
				//	)
				//{ // (Moving these here just after the 'alpha' test gave a slight improvement)
				//	// Update killers
				//	if (KillerMoves[ply].m1.ui32 != MateGenerate.gameRecordPointer->move.ui32)
				//	{
				//		KillerMoves[ply].m2 = KillerMoves[ply].m1;
				//		KillerMoves[ply].m1.ui32 = MateGenerate.gameRecordPointer->move.ui32;
				//		KillerMoves[ply].m1.piece = MateGenerate.gameRecordPointer->move.fromSquarePiece;
				//	}

				//	// Update counter move history values
				//	pt2 = abs(MateGenerate.gameRecordPointer->move.fromSquarePiece) - 1;
				//	ts2 = MateGenerate.gameRecordPointer->move.mf.toSquare;
				//	assert((pt1 >= Pawn - 1) && (pt1 <= King - 1) && (pt2 >= Pawn - 1) && (pt2 <= King - 1) && (ts1 >= A1) && (ts1 <= H8) && (ts2 >= A1) && (ts2 <= H8));
				//	//MateGenerate.gameRecordPointer->historyPointer->History[pt2][ts2] += depthRemaining * depthRemaining * (givesCheck ? 2 : 1);
				//	//if (currentMoveScore >= MatingScore)
				//	//	MateGenerate.gameRecordPointer->historyPointer->History[pt2][ts2] += 1 << 8;
				//	//if (MateGenerate.gameRecordPointer->historyPointer->History[pt2][ts2] >= (1 << 23))
				//	//	ReduceCounterMoveHistory(); // Reduce on overflow
				//	MateGenerate.gameRecordPointer->historyPointer->History[pt2][ts2] += ((1 << 22) - MateGenerate.gameRecordPointer->historyPointer->History[pt2][ts2]) >> 8;

				//	// Update counter moves
				//	if (CounterMoves[pt1][ts1].m1.ui32 != MateGenerate.gameRecordPointer->move.ui32)
				//	{
				//		CounterMoves[pt1][ts1].m2.ui32 = CounterMoves[pt1][ts1].m1.ui32;
				//		CounterMoves[pt1][ts1].m1.ui32 = MateGenerate.gameRecordPointer->move.ui32;
				//	}

				//	// Update follow-up moves
				//	if (FollowUpMoves[fupt1][futs1].m1.ui32 != MateGenerate.gameRecordPointer->move.ui32)
				//	{
				//		FollowUpMoves[fupt1][futs1].m2.ui32 = FollowUpMoves[fupt1][futs1].m1.ui32;
				//		FollowUpMoves[fupt1][futs1].m1.ui32 = MateGenerate.gameRecordPointer->move.ui32;
				//	}
				//}

				if (currentMoveScore >= beta)
				{
					if ( // 'Quiet' move?
						(!(currentMove.mf.flag >= MFPromotion)) &&
						(mateBrain.gameRecordPointer->move.toSquarePiece == 0)
						)
					{
						// Update killers
						if (KillerMoves[ply].m1.ui32 != mateBrain.gameRecordPointer->move.ui32)
						{
							KillerMoves[ply].m2 = KillerMoves[ply].m1;
							KillerMoves[ply].m1.ui32 = mateBrain.gameRecordPointer->move.ui32;
							KillerMoves[ply].m1.piece = mateBrain.gameRecordPointer->move.fromSquarePiece;
						}

						// Update counter move history values
						pt2 = abs(mateBrain.gameRecordPointer->move.fromSquarePiece) - 1;
						ts2 = mateBrain.gameRecordPointer->move.mf.toSquare;
						assert((pt1 >= Pawn - 1) && (pt1 <= King - 1) && (pt2 >= Pawn - 1) && (pt2 <= King - 1) && (ts1 >= A1) && (ts1 <= H8) && (ts2 >= A1) && (ts2 <= H8));
						//MateGenerate.gameRecordPointer->historyPointer->History[pt2][ts2] += depthRemaining * depthRemaining * (givesCheck ? 2 : 1);
						//if (currentMoveScore >= MatingScore)
						//	MateGenerate.gameRecordPointer->historyPointer->History[pt2][ts2] += 1 << 8;
						//if (MateGenerate.gameRecordPointer->historyPointer->History[pt2][ts2] >= (1 << 23))
						//	ReduceCounterMoveHistory(); // Reduce on overflow
						assert((1 << 22) > mateBrain.gameRecordPointer->historyPointer->History[pt2][ts2]);
						mateBrain.gameRecordPointer->historyPointer->History[pt2][ts2] += ((1 << 22) - mateBrain.gameRecordPointer->historyPointer->History[pt2][ts2]) >> 8; // The higher the count gets, the slower it increases (so should never overflow)

						// Update counter moves
						if (CounterMoves[pt1][ts1].m1.ui32 != mateBrain.gameRecordPointer->move.ui32)
						{
							CounterMoves[pt1][ts1].m2.ui32 = CounterMoves[pt1][ts1].m1.ui32;
							CounterMoves[pt1][ts1].m1.ui32 = mateBrain.gameRecordPointer->move.ui32;
						}

						// Update follow-up moves
						if (FollowUpMoves[fupt1][futs1].m1.ui32 != mateBrain.gameRecordPointer->move.ui32)
						{
							FollowUpMoves[fupt1][futs1].m2.ui32 = FollowUpMoves[fupt1][futs1].m1.ui32;
							FollowUpMoves[fupt1][futs1].m1.ui32 = mateBrain.gameRecordPointer->move.ui32;
						}
					}

					// This move has returned a score >= beta, therefore this is a 'Cut' node
					// The currentMoveScore is a lower bound (floor) on the exact score of the node (i.e. the exact score might be greater than currentMoveScore, it is "at least" currentMoveScore)
					AddToMateTranspositionTable(depthRemaining, ply, currentMoveScore, TTFlagLower + mateBrain.gameRecordPointer->isTWM + mateBrain.gameRecordPointer->isO1M + mateBrain.gameRecordPointer->isFMTP, mateBrain.gameRecordPointer->move.ui32, mateBrain.gameRecordPointer->staticEvaluation);
					if (ply == 1)
						ShowBestLineMessage(currentMoveScore, 1);

					//if (tteBestMove == MateBrain.gameRecordPointer->move.ui32)
					//{
					//	//AC1++;
					//	assert(legalMovesMade == 1);
					//}

					return currentMoveScore;
				}

				// PV node: score >alpha and <beta
				assert(isPVNode);
				alpha = currentMoveScore;
				if (ply == 1)
				{
					UpdateRootNodeCounts(currentMove.ui32, currentMoveScore);

					RootAlphaUpdated = alpha;
					ShowBestLineMessage(currentMoveScore, 0);
					RootBestMove = currentMove;
					if (currentMoveScore >= MatingScore)
					{
						currentMoveScore -= 2; // N.B. Subtract 2 NOT 1!
						alpha = currentMoveScore; // Find all mates
					}
				}
				else
				{
					if (currentMoveScore == MateBaseScore - ply - 1) // Cannot improve on a mate-in-1 so break out of the move loop immediately
					{
						//AC1++;
						bestMoveScore = currentMoveScore;//NEVER KICKS IN! WHY??? because of mws and TT???
						break;
					}
				}

			}

			bestMoveScore = currentMoveScore;
		}

		//----------------------------------------------------------------------------------------------------

	} // (Loop through move list)

	if (pass == 1)
		if (ply & 1)
		{
			pass++;
			goto LoopThroughMoveList;
		}

	//----------------------------------------------------------------------------------------------------

	// Update transposition table
	if (alpha == originalAlpha)
	{
		// No move has returned a score > alpha, therefore this is an 'All' node (all legal moves have been searched)
		// The bestMoveScore is an upper bound (ceiling) on the exact score of the node (i.e. the exact score might be less than bestMoveScore, it is "at most" bestMoveScore)
		// The children of an All node are Cut nodes. The parent of an All node is a Cut node. The ply distance of an All node to its PV ancestor is even.
		//AddToMateTranspositionTable(depthRemaining, ply, bestMoveScore, TTFlagUpper + fewerMovesThanPieces + threatenedWithMate, tteBestMove); // Keep any existing TT move even though it didn't raise alpha
		//AddToMateTranspositionTable(depthRemaining, ply, bestMoveScore, TTFlagUpper + fewerMovesThanPieces + MateGenerate.gameRecordPointer->isInDanger, tteBestMove); // Keep any existing TT move even though it didn't raise alpha
		AddToMateTranspositionTable(depthRemaining, ply, bestMoveScore, TTFlagUpper + mateBrain.gameRecordPointer->isTWM + mateBrain.gameRecordPointer->isO1M + mateBrain.gameRecordPointer->isFMTP, tteBestMove, mateBrain.gameRecordPointer->staticEvaluation); // Keep any existing TT move even though it didn't raise alpha
	}
	else
	{
		assert((originalAlpha < bestMoveScore) && (bestMoveScore == alpha) && (bestMoveScore < beta));
		assert(isPVNode);
		assert(*mateBrain.gameRecordPointer->principalVariationPointer != PVTUnknown);
		// A move has returned a score > (the original) alpha but < beta, therefore this is a 'PV' node (all legal moves have been searched)
		// The bestMoveScore is the EXACT score of the node
		// The root node and the leftmost nodes are always PV-nodes. All siblings of a PV node are expected Cut nodes.
		//AddToMateTranspositionTable(depthRemaining, ply, bestMoveScore, TTFlagExact + fewerMovesThanPieces + threatenedWithMate, *MateGenerate.gameRecordPointer->principalVariationPointer);
		//AddToMateTranspositionTable(depthRemaining, ply, bestMoveScore, TTFlagExact + fewerMovesThanPieces + MateGenerate.gameRecordPointer->isInDanger, *MateGenerate.gameRecordPointer->principalVariationPointer);
		AddToMateTranspositionTable(depthRemaining, ply, bestMoveScore, TTFlagExact + mateBrain.gameRecordPointer->isTWM + mateBrain.gameRecordPointer->isO1M + mateBrain.gameRecordPointer->isFMTP, *mateBrain.gameRecordPointer->principalVariationPointer, mateBrain.gameRecordPointer->staticEvaluation);
	}

	//----------------------------------------------------------------------------------------------------

	assert((bestMoveScore < MateBaseScore) && (bestMoveScore > -MateBaseScore));
	return bestMoveScore;
}

STRING Mate::ComputeMate()
{
	// At the start of these Compute* routines assume that just the 64-square mailbox board is set up

	ClearAnalysisCounters();

	mateBrain.CopyFrom(&EngineBrain);

	// Set up the bit boards from the 64-square mailbox board
	ConvertMailboxBoard64ToPiecesBB(mateBrain.mailboxBoard64, mateBrain.piecesBB);

	// Initialise the PV array pointers in the GameRecord array
	for (UINT32 index = 0; index < MaximumPly; index++)
		mateBrain.gameRecord[mateBrain.GameRecordIndexRoot + index].principalVariationPointer = &PrincipalVariation[MaximumPlyPlus1 * index];

	//----------------------------------------------------------------------------------------------------

	// Get move timer
	StartClock = std::chrono::steady_clock::now();
	//MessagesLastDisplayedTickCount = StartTickCount - 300; // Get the first batch of messages after 200ms
	MessagesLastDisplayedClock = StartClock;

	// Initialise any variables required for the search
	mateBrain.gameRecordPointer = &mateBrain.gameRecord[mateBrain.GameRecordIndexRoot];

	UINT64 totalNodes[MaximumPly];
	totalNodes[0] = 1;
	NodeCount = 0;
	NodeCountQuiescenceSearch = 0;
	RootCumulativeNodeCount = 1;
	MaximumPlyReached = 0;
	ConsistentBestMoves = 0;
	int previousBestMove = 0;
	StopImmediately = false;
	StopWhenIterationComplete = false;
	ReplyImmediately = false;
	InitialiseMaterialValues(mateBrain.mailboxBoard64, mateBrain.gameRecordPointer);
	//*(UINT32*)(&MateGenerate.gameRecordPointer->totalMaterial[0]) = *(UINT32*)(&RootTotalMaterial[0]);
	InitialisePSTValues(mateBrain.mailboxBoard64, mateBrain.gameRecordPointer);
	InitialiseGamePhase(mateBrain.mailboxBoard64, mateBrain.gameRecordPointer);
	//*(UINT64*)(&MateGenerate.gameRecordPointer->gamePhase[0]) = *(UINT64*)(&GamePhase[0]);
	UINT64 hash64 = GenerateTranspositionTableHash64(mateBrain.mailboxBoard64, mateBrain.gameRecordPointer);
	if (SideToMove == 1)
		hash64 = ~hash64;
	mateBrain.gameRecordPointer->transpositionTableHash64 = hash64;
	mateBrain.gameRecordPointer->transpositionTableHash64WithEP = hash64 ^ TranspositionTableRandomsEnPassant[mateBrain.gameRecordPointer->epSquare]; // N.B. TranspositionTableRandomsEnPassant[0] = 0
	TranspositionTableAge++;
	TranspositionTableAge &= TTFlagAgeMask;

	// Root move list stuff
	// Generated once here and the moves stay in the same physical order in which they are generated so that they correspond with the same moves in the tree generated move list
	// The .nodes property is used to generate .score values for move ordering in the tree
	alignas(64) MoveWithScore_Struct moveList[220];
	RootMoveList[0].mws.ui32 = 0;
	mateBrain.CalculatePinnedPieces(SideToMove); // Required for legal move generation
	RootMovesCount = mateBrain.GenerateAllMoves(SideToMove, mateBrain.IsEnemyKingAttacked(BitScanForwardX(mateBrain.piecesBB[SideToMove][King]), SideToMove ^ 1), moveList);
	for (int moveListIndexIterator = 0; moveListIndexIterator < RootMovesCount; moveListIndexIterator++)
	{
		RootMoveList[moveListIndexIterator].mws = moveList[moveListIndexIterator];
		RootMoveList[moveListIndexIterator].nodes = 1;
	}

	//// EGTB stuff
	//EndgameTablebasesProbes = 0;
	//EndgameTablebasesHeavyProbes = 0;
	//EndgameTablebasesHits = 0;
	//EndgameTablebasesPiecesRoot = PopulationCountX(MateBrain.piecesBB[0][AllPieces] | MateBrain.piecesBB[1][AllPieces]);
	//EndgameTablebasesRootMove = 0;
	//EndgameTablebasesRootWDL = TB_RESULT_FAILED;
	//if (EndgameTablebasesPiecesRoot <= EndgameTablebasesPiecesFound)
	//	if ((MateBrain.gameRecordPointer->castlingStatus.ui8[SideToMove][0] != 0) && (MateBrain.gameRecordPointer->castlingStatus.ui8[SideToMove][1] != 0) && (MateBrain.gameRecordPointer->castlingStatus.ui8[SideToMove ^ 1][0] != 0) && (MateBrain.gameRecordPointer->castlingStatus.ui8[SideToMove ^ 1][1] != 0)) // Don't probe the endgame tablebases if any castling is still possible
	//	{
	//		// We are in the EGTB at the root so get the 'best' EGTB move
	//		UINT32 result;
	//		result = tb_probe_root(
	//			MateBrain.piecesBB[0][AllPieces],
	//			MateBrain.piecesBB[1][AllPieces],
	//			MateBrain.piecesBB[0][King] | MateBrain.piecesBB[1][King],
	//			MateBrain.piecesBB[0][Queen] | MateBrain.piecesBB[1][Queen],
	//			MateBrain.piecesBB[0][Rook] | MateBrain.piecesBB[1][Rook],
	//			MateBrain.piecesBB[0][Bishop] | MateBrain.piecesBB[1][Bishop],
	//			MateBrain.piecesBB[0][Knight] | MateBrain.piecesBB[1][Knight],
	//			MateBrain.piecesBB[0][Pawn] | MateBrain.piecesBB[1][Pawn],
	//			0,
	//			0,
	//			MateBrain.gameRecordPointer->epSquare,
	//			(SideToMove == 0),
	//			NULL
	//		);
	//		if (result != TB_RESULT_FAILED)
	//		{
	//			// N.B.1. Sometimes can have a winning EGTB position e.g. 8/8/8/1P3p2/5r2/8/2R5/k1K5 w - - 1 2 but its analysis prefers a PV which starts ok but ends in an EGTB draw because its static eval of the later KRPvKRP posn is < drawScore! Not worth trying to fix though!
	//			// N.B.2. Sometimes can have a blessed loss position e.g. 2Q5/1K6/1P6/8/8/8/8/2q1k3 b - - 0 2 but it quickly returns a 'losing' score because it sees past the DTZ=100 point and then the EGTB returns 'loss' rather than 'blessed loss'
	//			// N.B.3. Sometimes a short mate is hidden because it involves a capture into a lesser EGTB position which also just scores +158.00
	//			EndgameTablebasesRootWDL = TB_GET_WDL(result);
	//			if (EndgameTablebasesRootWDL != TB_DRAW)
	//			{
	//				// Convert the EGTB move format into my move format
	//				int fromSquare = TB_GET_FROM(result);
	//				int toSquare = TB_GET_TO(result);
	//				EndgameTablebasesRootMove = fromSquare | (toSquare << 8);

	//				int promotionPiece = TB_GET_PROMOTES(result);
	//				if (promotionPiece != TB_PROMOTES_NONE)
	//				{
	//					if (promotionPiece == TB_PROMOTES_QUEEN)
	//						promotionPiece = MFPromoteToQueen;
	//					else if (promotionPiece == TB_PROMOTES_ROOK)
	//						promotionPiece = MFPromoteToRook;
	//					else if (promotionPiece == TB_PROMOTES_BISHOP)
	//						promotionPiece = MFPromoteToBishop;
	//					else
	//						promotionPiece = MFPromoteToKnight;
	//					EndgameTablebasesRootMove |= (promotionPiece << 16);
	//				}

	//				int epSquare = TB_GET_EP(result);
	//				if (epSquare != 0)
	//				{
	//					toSquare -= PawnMoveOffset[SideToMove];
	//					EndgameTablebasesRootMove = fromSquare | (toSquare << 8) | (MFEnPassant << 16);
	//				}
	//			}
	//		}
	//	}

	// Determine defender's 'fixed' pieces (i.e. those with zero moves at the root)
	mateBrain.CalculatePinnedPieces(SideToMove ^ 1); // Required for legal move generation
	int movesCount;
	movesCount = mateBrain.GenerateAllMoves(SideToMove ^ 1, false, moveList); // Generate defender's moves

	int kingSquare = BitScanForwardX(mateBrain.piecesBB[SideToMove ^ 1][King]);
	int kingMoves = 0;
	for (int square = A1; square <= H8; square++) // Mark all SNTM pieces as fixed
	{
		RootZLMPiecesMailboxBoard64[square] = false;
		if (
			((mateBrain.mailboxBoard64[square] > Empty) && (SideToMove == 1))
			|| ((mateBrain.mailboxBoard64[square] < Empty) && (SideToMove == 0))
			)
			RootZLMPiecesMailboxBoard64[square] = true;
	}
	(mateBrain.gameRecordPointer - 1)->zLMPiecesBB = mateBrain.piecesBB[SideToMove ^ 1][AllPieces];
	for (int moveListIndexIterator = 0; moveListIndexIterator < movesCount; moveListIndexIterator++)
	{
		RootZLMPiecesMailboxBoard64[moveList[moveListIndexIterator].mf.fromSquare] = false;
		(mateBrain.gameRecordPointer - 1)->zLMPiecesBB &= !UINT64SetBit(moveList[moveListIndexIterator].mf.fromSquare);
		if (moveList[moveListIndexIterator].mf.fromSquare == kingSquare)
			kingMoves++;
	}
	(mateBrain.gameRecordPointer - 1)->DefenderKingMovesBefore = kingMoves;
	(mateBrain.gameRecordPointer - 1)->TotalDefenderKingMovesBefore = kingMoves;
	//(mateBrain.gameRecordPointer - 1)->isZLKM = (kingMoves == 0);
	RootFixedPiecesBB = 0;
	STRING s = TC.MateFixedPieces;
	while (s != "")
	{
		STRING sq = s.substr(0, 2);
		int squareIndex = sq[0] - 'a' + ((sq[1] - '1') * 8);
		RootFixedPiecesBB ^= UINT64SetBit(squareIndex);
		s = s.substr(2);
	}

	// Clear any killers
	ClearKillerMoves();
	ClearCounterMoves();//TEMP TEST : THESE TWO MAKE IT CONSISTENT WITH SINGLE-THREADED VERSION BUT RAISES THE QUESTION IS IT BEST TO CLEAR THESE OR NOT BETWEEN MVOES (OR NEGLIGIBLE)
	ClearFollowUpMoves();//TEMP TEST : COULD TEST WITH ST VERSION?
	//ClearPrincipalVariation();// SHOULDN'T EVER NEED TO DO THIS??? CHECK CODE AND TEST
	ClearCounterMoveHistory();
	ClearMateTranspositionTable(); // Have to do this as noticed in testing that previous position's entries screwed up the solution! WHAT ABOUT BACKTRACING???

	//----------------------------------------------------------------------------------------------------

	// Do the search
	RootScore = 0;
	RootBestMove.ui32 = 0;
	RootDefenderKingMoves = mateBrain.CountKingMoves(SideToMove ^ 1); //REALLY SHOULD USE THE KINGMOVES AFTER THE PLY1 MOVE !!!
	RootGameRecordPointer = mateBrain.gameRecordPointer;
	RootMaterialBalance = mateBrain.gameRecordPointer->totalMaterial[SideToMove] - mateBrain.gameRecordPointer->totalMaterial[SideToMove ^ 1];
	IterationPly = 0;
	//if (ThreadId > 0)
	//	IterationPly++;
	//IterationPly += (ThreadId % 2);
	int backedOffIterationPly = 0;
	do
	{
		// Update iteration depth (ensuring it doesn't exceed maximum)
		if (IterationPly < MaximumIterationPly)//USE MD!
			//IterationPly += 1;
			IterationPly += 2;//2 in Mate mode not 1 ? ? ?
		//IterationPly = TC.MateInN * 2;//TEMP

		// Set the aspiration window
		//if (IterationPly == 1)
		{
			RootAlpha = (short)(-MateBaseScore);
			//RootAlpha = (short)(MatedScore); // If the attacker is being mated we don't need to know how quickly!
			//RootAlpha =-1; // If the attacker is being mated we don't need to know how quickly!
			//RootAlpha = (short)(MatingScore); // If the attacker is being mated we don't need to know how quickly!TEMP
			RootBeta = (short)(MateBaseScore);//SET IT TO MATE IN MD+1 SCORE???
		}
		//else
		//{
		//	RootAlpha = (short)std::max(RootScore - AspirationWindowDelta, -MateBaseScore);
		//	RootBeta = (short)std::min(RootScore + AspirationWindowDelta, MateBaseScore);
		//}
		RootAlphaUpdated = RootAlpha;
		RootBetaOld = RootBeta;
		RootFailHighs = 0;
		RootFailLows = 0;
		LastPrintTreePly = 0;
		defenderSpiteChecks = 0;
#ifdef SEARCHINGFORLINE
		//TargetLineLength = 1;
		TargetLinePartial = "";
#endif

		//----------------------------------------------------------------------------------------------------

	retry:
		ShowIterationStartMessage();
		PVMessageChecked = false;

		// Do the search
		RootScore = TreeSearchMate(RootAlpha, RootBeta, 1, IterationPly, SideToMove, mateBrain.IsEnemyKingAttacked(BitScanForwardX(mateBrain.piecesBB[SideToMove][King]), SideToMove ^ 1));// , false, false, 0);

		//if (!StopImmediately)
		//{
		//	if (RootScore >= RootBeta) // Failed high? i.e. a root move returned a score >= beta (N.B. rootScore can actually be > beta because of fail-soft)
		//	{
		//		RootFailHighs++;
		//		RootBetaOld = RootBeta;
		//		if (RootFailHighs == 1)
		//			RootBeta = (short)std::min(RootScore + 150, MateBaseScore);
		//		else if (RootFailHighs == 2)
		//			RootBeta = (short)std::min(RootScore + 950, MateBaseScore);
		//		else
		//			RootBeta = (short)MateBaseScore;
		//		//RootAlpha = RootAlphaUpdated; (+1.8, +/-3.6, 20000) for taking this out!
		//		//if (IterationPly > backedOffIterationPly)
		//		//{
		//		//	backedOffIterationPly = IterationPly;
		//		//	IterationPly = 2;
		//		//}
		//		goto retry;
		//	}
		//	else if (RootScore <= RootAlpha)// Failed low? i.e. no root move took over as the new best
		//	{
		//		if (ThreadId == 0)
		//			ShowFailedLowMessage(RootAlpha);
		//		//RootFailLows++;
		//		//if (RootFailLows == 1)
		//		//	RootAlpha = (short)std::max(RootScore - 150, -MateBaseScore);
		//		//else
		//			RootAlpha = (short)(-MateBaseScore);
		//		goto retry;
		//	}
		//}

		//----------------------------------------------------------------------------------------------------

		ShowIterationFinishMessage(HashfullMateTranspositionTable());

		//if (RootMoveList[0].mws.ui32 == previousBestMove)
		//	ConsistentBestMoves++;
		//else
		//	ConsistentBestMoves = 0;

		// Show diagnostics
		if (IsDebug)
			if (ThreadId == 0)
			{
				ShowQueuedMessages();
				totalNodes[IterationPly] = NodeCount + NodeCountQuiescenceSearch;
				Output("info string Nodes: Main/QS/%inQS " + MyUI64TOA(NodeCount) + " / " + MyUI64TOA(NodeCountQuiescenceSearch) + " / " + MyUI64TOA((NodeCountQuiescenceSearch * 100) / (NodeCount + NodeCountQuiescenceSearch)));
				Output("info string Branching factor: " + MyFTOA((float)totalNodes[IterationPly] / totalNodes[IterationPly - 1]));
				Output("info string Longest line: " + LongestLine);
				Output("");
			}

		// Should we stop the search? (Sets various flags internally which are tested elsewhere)
		TimeUp(2.0F);

#ifdef SEARCHINGFORLINE
		ShowQueuedMessages();
		if (TargetLinePartial != "")
			Output("IterationPly: " + MyITOA(IterationPly) + ", TargetLinePartial: " + TargetLinePartial + ", TargetLineRefutedBy: " + MoveNotation(TargetLineRefutedBy.ui32) + " (" + MyITOA(TargetLinePartialDepthRemaining) + ", " + MyITOA(TargetLinePartialThreateningMate) + ")\n");
#endif
		////TEMP
		//if (ThreadId == 0)
		//	ShowQueuedMessages();

	} while ((!StopWhenIterationComplete && (ThreadId == 0)) || (!StopImmediately && (ThreadId > 0)));

	if (ThreadId == 0)
	{
		StopImmediately = true; // As soon as the main thread finishes ensure all other threads terminate

		ShowQueuedMessages();

		// Report best move
		STRING bestMoveMessage;
		bestMoveMessage = "bestmove ";
		if (RootBestMove.ui32 == 0)
			bestMoveMessage + "*** No mate found ***";
		else
			bestMoveMessage += MoveNotation(RootBestMove.ui32);
		bestMoveMessage += "\n";
		if (!MateSilent)
			Output(bestMoveMessage);

		assert(MateTranspositionTableUnlocked());

		// Save any output from -FILE command for analysis in spreadsheet
		if (ProcessingCommandFile)
		{
			FILE *sw;
			fopen_s(&sw, "output.csv", "a+");
			//STRING s = BestLine() + MyITOA(RootScore) + "," + MyUI64TOA(NodeCount + NodeCountQuiescenceSearch) + "," + MyUI64TOA(GetTickCount64() - StartTickCount);
			STRING s = BestLine() + MyITOA(RootScore) + "," + MyUI64TOA(NodeCount) + "," + MyUI64TOA(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - StartClock).count());
			fprintf(sw, "%s\n", s.c_str());
			fclose(sw);
		}

		DisplayAnalysisCounters();
	}
	//Output("Thread exiting " + MyITOA(ThreadId));

	return BestLine() + MyITOA(IterationPly) + ", " + MyUI64TOA(NodeCount + NodeCountQuiescenceSearch) + ", " + MyUI64TOA(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - StartClock).count());
}

void Mate::ComputeMateMTLaunchHelperThread(int threadId)
{
	Mate ts;
	ts.ThreadId = threadId;
	ts.ComputeMate();
}

STRING Mate::ComputeMateMT()
{
	// If we've got some unused transposition table memory then allocate it
	if ((TranspositionTableMemory > 0) && (Mate::MateTranspositionTableBuckets == 0))
		Mate::AllocateMateTranspositionTable();

	//std::map<int, int> mapPly1, mapPly2;//TEST
	//mapPly1[0] = 99;
	//mapPly1[1] = 88;

	ConvertMailboxBoard64ToPiecesBB(EngineBrain.mailboxBoard64, EngineBrain.piecesBB);

	StopImmediately = false;

	//// Launch any helper threads independently
	//std::thread th;
	//int t = Threads;
	////t = 4;//TEMP
	//for (int threadId = 1; threadId < t; threadId++)
	//{
	//	th = std::thread(ComputeMateMTLaunchHelperThread, threadId);
	//	th.detach();
	//}

	// Compute the result in this main thread which uses the Mate class instance declared in Engine
	EngineMate.ThreadId = 0;
	STRING result = EngineMate.ComputeMate();

	//StopImmediately = true; // Ensure all helper threads terminate
	//StopWhenIterationCompleteHelperThreads = true;

	return result;
}

void Mate::ComputeMateFile(STRING filename)
{
	char line[10000];
	STRING tokens[1000];
	int tokenCount;
	int positionsCount;
	int errors = 0;
	STRING sectionHeader = "% Mate in " + MyITOA(TC.MateInN);
	FILE *MateFile;

	// Take input from file
	fopen_s(&MateFile, filename.c_str(), "r");
	if (MateFile == NULL)
		Output("*** Error! " + filename + " not found");
	else
	{
		// Get timer
		std::chrono::time_point<std::chrono::steady_clock> StartClock = std::chrono::steady_clock::now();

		StopWhenIterationComplete = false;

		positionsCount = 1;
		while ((fgets(line, 10000, MateFile) != NULL))// && (!StopWhenIterationComplete))
		{
			line[strlen(line) - 1] = 0; // Remove the trailing "\n" we get when we read a line from a file
			if (strlen(line) == 0)
				continue;

			if (STRING(line) == sectionHeader)
			{
				while ((fgets(line, 10000, MateFile) != NULL))// && (!StopWhenIterationComplete))
				{
					line[strlen(line) - 1] = 0; // Remove the trailing "\n" we get when we read a line from a file
					if (STRING(line)[0] == '%')
						continue;
					if (strlen(line) == 0)
						goto CloseFile;

					Split(line, &tokens[0], &tokenCount, " ");
					STRING s, fen, opcodes;
					fen = tokens[0] + " " + tokens[1] + " " + tokens[2] + " " + tokens[3];
					s = "position fen " + fen;
					SetPositionAndMoves(s);
					opcodes = STRING(line).substr(fen.length() + 1);
					Split(opcodes, &tokens[0], &tokenCount, ";");

					//Output(MyITOA(positionsCount) + ": " + fen);
					//Output(opcodes);
					Output(MyITOA(positionsCount) + ": " + STRING(line));

					STRING result = ComputeMateMT();
					Output(result);
					Output("");

					//if (MyUI64TOA(result) != tokens[PerftDepth])
					//{
					//	// If the value we just computed doesn't match the given correct value in the file then output an error message
					//	Output(MyUI64TOA(result));
					//	Output(tokens[PerftDepth]);
					//	Output("*** Error!");
					//	errors++;
					//}

					positionsCount++;

				}
			}
		}

	CloseFile:
		fclose(MateFile);

		// Show closing statistics
		Output("info string Time: " + MyUI64TOA(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - StartClock).count()) + "ms");
		Output("info string errors = " + MyITOA(errors));
		Output("");
	}
}

void Mate::ComputeMateWrapper()
{
	if (TC.MateFilename == "")
	{
		MateSilent = false;
		ComputeMateMT();
	}
	else
	{
		// Process mate commands from a file
		MateSilent = true;
		ComputeMateFile(TC.MateFilename);
	}

	ComputingMove = false;
}
