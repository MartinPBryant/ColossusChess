#include <algorithm>
#include <chrono>
#include <assert.h>
#include <iostream>
#include <thread>
#include <map>
#include <iomanip>
#include <sstream>
#define NOMINMAX // Need to include this to stop windows.h (below) breaking std::min etc
#include <windows.h>

#include "GlobalConstants.h"
#include "GlobalTypes.h"
#include "Engine.h"
#include "UGI.h"
#include "Brain.h"
#include "Evaluate.h"
#include "Utilities.h"
#include "SearchNormal.h"
#include "SYZYGYPYRRHIC\tbprobe.h"

//----------------------------------------------------------------------------------------------------

// The following data structures are shared between multiple threads
// They are only cleared when a 'ucinewgame' command is received, so they don't lose information from move to move as a game progresses

Normal::NormalTranspositionTableBucket_Struct* Normal::NormalTranspositionTablePointer = nullptr;
uint32_t Normal::NormalTranspositionTableBuckets = 0;
uint32_t Normal::NormalTranspositionTableBucketsMask;

//std::string Normal::ThreadResults[ThreadsMax];

int Normal::CrashLocation;

//----------------------------------------------------------------------------------------------------

Normal::Normal()
{
#ifdef CRASHLOCATIONDEF
	Output("*** Warning! CRASHLOCATIONDEF defined!");
#endif

	// Declaring CounterMoveHistory as a class variable loses ELO and crashes occasionally but I don't know why!
	// Creating it on the heap here seems to work fine though.
	CounterMoveHistory = new CounterMoveHistory_Struct;
}

Normal::~Normal()
{
	delete CounterMoveHistory;
}

//----------------------------------------------------------------------------------------------------

#pragma region Testing

void Normal::TestSEE()
{
	normalBrain.CopyFrom(&EngineBrain);
	ConvertMailboxBoard64ToPiecesBB(normalBrain.mailboxBoard64, normalBrain.piecesBB);

	MoveWithScore_Struct moveList[220];
	normalBrain.CalculatePinnedPieces(SideToMove); // Required for legal move generation
	int movesCount = normalBrain.GenerateAllMoves(SideToMove, normalBrain.IsEnemyKingAttacked(BitScanForwardX(normalBrain.piecesBB[SideToMove][King]), SideToMove ^ 1), moveList);

	for (int moveListIndexIterator = 0; moveListIndexIterator < movesCount; moveListIndexIterator++)
	{
		int SEEResult;
		SEEResult = normalBrain.SEE(moveList[moveListIndexIterator].mf.fromSquare, moveList[moveListIndexIterator].mf.toSquare, SideToMove);
		Output("info string " + Notation64[moveList[moveListIndexIterator].mf.fromSquare] + Notation64[moveList[moveListIndexIterator].mf.toSquare] + "=" + MyITOA(SEEResult));
	}
}

#pragma endregion

//----------------------------------------------------------------------------------------------------

__declspec(noinline)
void Normal::ClearKillerMoves()
{
	//for (int index = 0; index < MaximumPly; index++)
	//{
	//	KillerMoves[index].m1.ui32 = PVTUnknown;
	//	KillerMoves[index].m1.piece = 0;
	//	KillerMoves[index].m2.ui32 = PVTUnknown;
	//	KillerMoves[index].m2.piece = 0;

	//	//PVKillerMoves[index].m1.ui32 = PVTUnknown;
	//	//PVKillerMoves[index].m1.piece = 0;
	//	//PVKillerMoves[index].m2.ui32 = PVTUnknown;
	//	//PVKillerMoves[index].m2.piece = 0;
	//}

	memset(KillerMoves, 0, sizeof(KillerMoves));
}

__declspec(noinline)
void Normal::ClearCounterMoves()
{
	//for (int pti = 0; pti < 6; pti++)
	//	for (int tsi = A1; tsi <= H8; tsi++)
	//	{
	//		CounterMoves[pti][tsi].m1.ui32 = PVTUnknown;
	//		CounterMoves[pti][tsi].m2.ui32 = PVTUnknown;
	//	}

	memset(CounterMoves, 0, sizeof(CounterMoves));
}

__declspec(noinline)
void Normal::ClearFollowUpMoves()
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
void Normal::ClearCounterMoveHistory()
{
	//for (int pt1i = 0; pt1i < 6; pt1i++)
	//	for (int ts1i = 0; ts1i < 64; ts1i++)
	//		for (int pt2i = 0; pt2i < 8; pt2i++)
	//			for (int ts2i = 0; ts2i < 64; ts2i++)
	//				CounterMoveHistory[pt1i][ts1i].History[pt2i][ts2i] = 0;

	memset(&CounterMoveHistory->CMH[0][0], 0, sizeof(CounterMoveHistory_Struct));
}

//----------------------------------------------------------------------------------------------------

#pragma region Message processing

//bool Normal::ShowPVTerminators = false;
//bool Normal::BlankLines = false;

std::string Normal::ThreadIdSuffix()
{
	if (Threads == 1)
		return "";
	return " ThreadId " + MyITOA(ThreadId);
}

//void Normal::AddMessageToQueue(std::string message, bool lastMessageWasAProgressMessage)
//{
//#ifdef _DEBUG
//	Output(message);
//#else
//	MessageQueue[MessageQueueIndex++] = message;
//	if (MessageQueueIndex == MessageQueueSize)
//		MessageQueueIndex = 0;
//	LastMessageWasAProgressMessage = lastMessageWasAProgressMessage;
//	MessagesQueued = true;
//#endif
//}
//
//void Normal::ReverseMessageQueueIndex()
//{
//	MessageQueueIndex--;
//	if (MessageQueueIndex == -1)
//		MessageQueueIndex = MessageQueueSize - 1;
//}

//void Normal::ShowIterationStartMessage()
//{
//	std::string IterationStartMessage = "info depth " + MyITOA(IterationPly)
//		+ " seldepth " + MyITOA(MaximumPlyReached);
//#ifndef _DEBUG
//	if (IsDebug)
//#endif
//		IterationStartMessage += ThreadIdSuffix();
//	AddMessageToQueue(IterationStartMessage, false);
//}
void Normal::ShowIterationStartMessage()
{
//	if (ThreadId > 0)
//		return;
//
//#ifndef _DEBUG
//	if (LastTickCount > 1000)
//#endif
//	{
//		std::string IterationStartMessage = "info depth " + MyITOA(IterationPly)
//			+ " seldepth " + MyITOA(MaximumPlyReached);
//		if (IsDebug)
//			IterationStartMessage += ThreadIdSuffix();
//		Output(IterationStartMessage);
//	}

	if (ThreadId > 0)
		return;

	std::string IterationStartMessage = "info depth " + MySI64TOA(IterationPly);
	if (IsDebug)
		IterationStartMessage += ThreadIdSuffix();

#ifndef _DEBUG
	if (LastTickCount > MessageDelayTickCount)
#endif
		Output(IterationStartMessage);
#ifndef _DEBUG
	else
	{
		PreviousIterationsMessages = CurrentIterationsMessages;
		CurrentIterationsMessages = IterationStartMessage;
	}
#endif
}

//void Normal::ShowProgressMessage(uint32_t move, int movesMade, short bestMoveScore, short alpha, short beta)
//{
//	uint64_t totalTickCount = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - StartClock).count() + 1; // +1 to avoid potential divide by zero on very fast computer!
//	std::string ProgressMessage = "info time " + MyUI64TOA(totalTickCount)
//		+ " nodes " + MyUI64TOA(NodeCount + NodeCountQuiescenceSearch)
//		+ " currmove " + MoveNotation(move)
//		+ " currmovenumber " + MyITOA(movesMade)
//		;
//#ifndef _DEBUG
//	if (IsDebug)
//#endif
//	{
//		ProgressMessage += " depth " + MyITOA(IterationPly) + " bestMoveScore " + MyITOA(bestMoveScore) + " alpha " + MyITOA(alpha) + " beta " + MyITOA(beta);
//		ProgressMessage += ThreadIdSuffix();
//		//ProgressMessage += " processor " + std::to_string(GetCurrentProcessorNumber());
//	}
//	if (LastMessageWasAProgressMessage)
//		ReverseMessageQueueIndex();
//	AddMessageToQueue(ProgressMessage, true);
//}
void Normal::ShowProgressMessage(uint32_t move, int movesMade, short bestMoveScore, short alpha, short beta)
{
	if (ThreadId > 0)
		return;

#ifndef _DEBUG
	if (LastTickCount > MessageDelayTickCount)
		if (
			(LastTickCount > LastProgressMessageTickCount + MessageDelayTickCount)
			|| (movesMade == 1)
			|| (movesMade == RootMovesCount)
			)
#endif
		{
			LastProgressMessageTickCount = LastTickCount;

			std::string ProgressMessage = "info time " + MyUI64TOA(LastTickCount)
				+ " nodes " + MyUI64TOA(NodeCount + NodeCountQuiescenceSearch)
				+ " currmove " + MoveNotation(move)
				+ " currmovenumber " + MyITOA(movesMade)
				;
			if (IsDebug)
			{
				ProgressMessage += " depth " + MyITOA(IterationPly) + " bestMoveScore " + MyITOA(bestMoveScore) + " alpha " + MyITOA(alpha) + " beta " + MyITOA(beta);
				ProgressMessage += ThreadIdSuffix();
				//ProgressMessage += " processor " + std::to_string(GetCurrentProcessorNumber());
			}
			Output(ProgressMessage);
		}
}

//void Normal::ShowFailedLowMessage(short rootAlpha)
//{
//	std::string FailedLowMessage = "info depth " + MyITOA(IterationPly) + " score cp " + MyITOA(rootAlpha) + " upperbound";
//	AddMessageToQueue(FailedLowMessage, false);
//}
void Normal::ShowFailedLowMessage(short rootAlpha)
{
	if (ThreadId > 0)
		return;

#ifndef _DEBUG
	if (LastTickCount > MessageDelayTickCount)
#endif
	{
		std::string FailedLowMessage = "info depth " + MyITOA(IterationPly) + " score cp " + MyITOA(rootAlpha) + " upperbound";
		Output(FailedLowMessage);
	}
}

//void Normal::ShowIterationFinishMessage(uint32_t hashfull)
//{
//	uint64_t totalTickCount = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - StartClock).count() + 1; // +1 to avoid potential divide by zero on very fast computer!
//
//	uint64_t totalNodes = NodeCount + NodeCountQuiescenceSearch;
//	std::string IterationFinishMessage = "info time " + MyUI64TOA(totalTickCount)
//		+ " nodes " + MyUI64TOA(totalNodes)
//		+ " nps " + MyUI64TOA((totalNodes * 1000) / totalTickCount)
//		+ " hashfull " + MyITOA(hashfull) + (EndgameTablebasesHits > 0 ? " tbhits " + MyUI64TOA(EndgameTablebasesHits) : "");
//#ifndef _DEBUG
//	if (IsDebug)
//#endif
//		IterationFinishMessage += ThreadIdSuffix();
//	if (BlankLines)
//		IterationFinishMessage += "\n";
//	AddMessageToQueue(IterationFinishMessage, false);
//}
void Normal::ShowIterationFinishMessage(uint32_t hashfull)
{
	if (ThreadId > 0)
		return;

	uint64_t totalNodes = NodeCount + NodeCountQuiescenceSearch;
	IterationFinishMessage = "info depth " + MyITOA(IterationPly)
		+ " time " + MyUI64TOA(LastTickCount)
		+ " nodes " + MyUI64TOA(totalNodes)
		+ " nps " + MyUI64TOA((totalNodes * 1000) / LastTickCount)
		+ " hashfull " + MyITOA(hashfull)
		+ (EndgameTablebasesHits > 0 ? " tbhits " + MyUI64TOA(EndgameTablebasesHits) : "");
	if (IsDebug)
		IterationFinishMessage += ThreadIdSuffix();
	if (BlankLines)
		IterationFinishMessage += "\n";
#ifndef _DEBUG
	if (LastTickCount > MessageDelayTickCount)
#endif
	{
		//if (BestLineMessage != "")
		//{
		//	Output(BestLineMessage);
		//	BestLineMessage = "";
		//}
		Output(IterationFinishMessage);
		//IterationFinishMessage = "";
	}
#ifndef _DEBUG
	else
	{
		CurrentIterationsMessages += "\n" + IterationFinishMessage;
	}
#endif
}

//void Normal::ShowQueuedMessages()
//{
//	// N.B. In compiler _DEBUG mode all messages are output as they occur so none will be queued
//	for (int i = 0; i < MessageQueueSize; i++)
//	{
//		int index = (MessageQueueIndex + i) % MessageQueueSize;
//		if (MessageQueue[index] != "")
//		{
//			if ((ThreadId == 0) || IsDebug)
//				Output(MessageQueue[index]);
//			MessageQueue[index] = "";
//		}
//	}
//
//	MessagesLastDisplayedClock = std::chrono::steady_clock::now();
//	MessagesQueued = false;
//}

std::string Normal::BestLine()
{
	std::string bestLine = "";

	int i = 0;
	do
	{
		bestLine += MoveNotation(PrincipalVariation[i++]) + " ";
	} while ((uint16_t)PrincipalVariation[i] != 0);

	return bestLine;
}

void Normal::ShowBestLineMessage(short alpha, uint8_t eul)
{
	if (ThreadId > 0)
		return;

//#ifndef _DEBUG
//	if (LastTickCount <= MessageDelayTickCount)
//		if (eul != TTFlagExact)
//			return;
//#endif

	// Construct the PV
	std::string PVMessage = "";
	int i = 0;
	do
	{
		PVMessage += MoveNotation(PrincipalVariation[i++]) + " ";
	} while ((PrincipalVariation[i] & 0xFFFF) != 0);

	// Construct any 'end of PV' suffix
	std::string pvTerminatorMessage = "";
#ifndef _DEBUG
	if (ShowPVTerminators)
#endif
	{
		switch (PrincipalVariation[i])
		{
		case PVTUnknown:
			pvTerminatorMessage = "*Unknown";
			break;
		case PVTStandPat:
			pvTerminatorMessage = "*StandPat";
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
			pvTerminatorMessage = "*Unknown PV terminator found! " + MyITOA(PrincipalVariation[i]);
			break;
		}
	}

	// Final bits and bobs
	uint64_t totalNodes = NodeCount + NodeCountQuiescenceSearch;

	std::string scoreMessage = " score ";
	if (alpha >= MatingScore) // Mating?
		scoreMessage += "mate " + MyITOA((MatingIn0Score - alpha) >> 1);
	else if (alpha <= MatedScore) // Mated?
		scoreMessage += "mate " + MyITOA((-MatingIn0Score - alpha + 1) >> 1);
	else // Normal
	{
		// If we are in the EGTB at the root, adjust the displayed score appropriately
		short displayedScore = alpha;
		if (EndgameTablebasesRootMove != 0)
		{
			if (EndgameTablebasesRootWDL == 0) // Draw?
				displayedScore = displayedScore / 8; // Reduce the range of displayed scores to avoid UIs adjudicating the game as a loss! See https://talkchess.com/viewtopic.php?t=84821
			else if (EndgameTablebasesRootWDL == 1) // Win?
				if (displayedScore < EGTBWinningScore)
					displayedScore = displayedScore + 1000; // Ensure the score looks like a winning score! (It may be just a few centi-pawns according to the eval)
		}

		scoreMessage += "cp " + MyITOA(displayedScore);
	}

	std::string eulMessage = "";
	if (eul != TTFlagExact)
	{
		//if (BestLineMessage != "")
		//{
		//	Output(BestLineMessage);
		//	BestLineMessage = "";
		//}

		if (eul == TTFlagLower)
			eulMessage = " lowerbound";
		else if (eul == TTFlagUpper)
			eulMessage = " upperbound";
	}

	// Display the constructed message
	std::string BestLineMessage = "info depth " + MyITOA(IterationPly) // N.B. the 'depth' value is provided here (as well as in the iteration 'start' message) as some GUIs (e.g. Arena, Shredder) don't display it unless it's provided with the PV!
		+ " time " + MyUI64TOA(LastTickCount)
		+ " nodes " + MyUI64TOA(totalNodes) + scoreMessage + eulMessage
		+ " pv " + PVMessage + pvTerminatorMessage;
	if (IsDebug)
		BestLineMessage += ThreadIdSuffix();

#ifndef _DEBUG
	if (LastTickCount > MessageDelayTickCount)
#endif
	{
		Output(BestLineMessage);
		//BestLineMessage = "";
	}
#ifndef _DEBUG
	else
	{
		CurrentIterationsMessages += "\n" + BestLineMessage;
	}
#endif
}

#pragma endregion

//----------------------------------------------------------------------------------------------------

#pragma region Root move list handling

// Called after each root move has been searched on the first iteration to save its subtree size and fail-soft score
void Normal::SaveRootMoveData(uint32_t move, uint64_t totalNodes, short score)
{
	bool found = false;
	// Find the current move in the root move list
	for (int index = 0; index < RootMovesCount; index++)
		if (RootMoveList[index].mws.ui32 == move)
		{
			RootMoveList[index].nodes = totalNodes - RootCumulativeNodeCount;
			assert(RootMoveList[index].nodes > 0);
			RootCumulativeNodeCount = totalNodes;
			RootMoveList[index].mws.score = score;
			found = true;
			break;
		}
	assert(found);
}

// Called for a root move which takes over as best to save its priority
void Normal::UpdateRootMovePriority(uint32_t move)
{
	bool found = false;
	// Find the current move in the root move list
	for (int index = 0; index < RootMovesCount; index++)
		if (RootMoveList[index].mws.ui32 == move)
		{
			RootMoveList[index].priority = RootPriority;
			found = true;
			break;
		}
	assert(found);
	RootPriority++;
	assert(RootPriority > 0);
}

// Update a a root move's EGTB status
bool Normal::UpdateRootMoveEGTBStatus(uint32_t move, int wdl, int dtz, int rank)
{
	bool found = false;
	// Find the current move in the root move list
	for (int index = 0; index < RootMovesCount; index++)
		if (RootMoveList[index].mws.ui32 == move)
		{
			RootMoveList[index].EGTBWDL = wdl;
			RootMoveList[index].EGTBDTZ = dtz;
			RootMoveList[index].EGTBRank = rank;
			found = true;
			break;
		}
	return found;
}

// Retrieve a root move's WDL status
int Normal::RetrieveRootMoveWDLStatus(uint32_t move)
{
	// Find the current move in the root move list
	for (int index = 0; index < RootMovesCount; index++)
		if (RootMoveList[index].mws.ui32 == move)
			return RootMoveList[index].EGTBWDL;
	OutputError("RetrieveRootMoveWDLStatus failed to find " + MoveNotation(move));
	return 1; // Should never get here but return 'win' just in case so that the move doesn't get discarded in the search!
}

// Retrieve a root move's DTZ status
int Normal::RetrieveRootMoveDTZStatus(uint32_t move)
{
	// Find the current move in the root move list
	for (int index = 0; index < RootMovesCount; index++)
		if (RootMoveList[index].mws.ui32 == move)
			return RootMoveList[index].EGTBDTZ;
	OutputError("RetrieveRootMoveDTZStatus failed to find " + MoveNotation(move));
	return -MAXINT; // Should never get here but return -INF just in case so that the move doesn't get discarded in the search!
}

// Called after the moves have been generated at the root in the tree to assign a simple sequential value to each root move based on its fail-soft score (or subtree size) and the last time it took over as best
// For the second iteration the moves are ordered by the failsoft score from the first iteration
// For subsequent iterations the moves are ordered by priority (which is based on when they last took over as best)
void Normal::ScoreRootMoveList(MoveWithScore_Struct* mlp)
{
	// Make a copy of the root move list
	RootMoveList_Struct RootMoveListTemp[220];
	for (int index = 0; index < RootMovesCount; index++)
		RootMoveListTemp[index] = RootMoveList[index];

	for (int index1 = 0; index1 < RootMovesCount; index1++)
	{
		uint64_t highestSortScore = 0;
		int highestIndex;
		for (int index2 = 0; index2 < RootMovesCount; index2++)
			if (RootMoveListTemp[index2].priority >= 0)
			{
				uint64_t sortScore = (std::min(RootMoveListTemp[index2].priority, 18445) * 1000000000000000ULL); // The last time it took over as best supercedes its fail-soft score (or subtree size)
				// I experimented with using the fail-soft score and the subtree size to do minor odering but there was no discernible ELO difference
				sortScore += (uint64_t)(RootMoveListTemp[index2].mws.score + MatingIn0Score + 1); // ensure >0 (.score may be -16000 in EGTB)
				//sortScore += RootMoveListTemp[index2].nodes;
				if (sortScore > highestSortScore)
				{
					highestSortScore = sortScore;
					highestIndex = index2;
				}
			}
		assert(highestIndex >= 0);
		RootMoveListTemp[highestIndex].priority = -1;

		// The moves are given values from 1000 downwards
		mlp[highestIndex].score = 1000 - index1;
	}
}

#pragma endregion

//----------------------------------------------------------------------------------------------------

void Normal::TimeUp(float divisor)
{
	// Determines if search should continue
	// Larger 'divisor' makes it more likely to terminate
	// Called as follows...
	// Within the search at any ply: divisor = 0.2 (Don't continue if we're already well over budget)
	// Within the search at ply=1: divisor = 1.0 (Don't start another root move unless we are under budget)
	// After a complete iteration: divisor = 2.0 (Don't start another iteration unless we are likely to complete it within budget)

	// Only the main thread is responsible for setting time up flags
	if (ThreadId > 0)
		return;

	// Get the time consumed so far this move
	LastTickCount = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - StartClock).count() + 1; // Add 1 millisecond to help on blitz finishes where timer is inaccurate
	if (LastTickCount > MessageDelayTickCount)
	{
		//if (BestLineMessage != "")
		//{
		//	Output(BestLineMessage);
		//	BestLineMessage = "";
		//}
		//if (IterationFinishMessage != "")
		//{
		//	Output(IterationFinishMessage);
		//	IterationFinishMessage = "";
		//}
		if (CurrentIterationsMessages != "")
		{
			Output(PreviousIterationsMessages);
			Output(CurrentIterationsMessages);
			CurrentIterationsMessages = "";
		}
	}

	switch (TC.CurrentType)
	{
	case TCTFixedDepth:
		if (IterationPly >= TC.FixedDepthPly)
			StopWhenIterationComplete = true;
		break;

	case TCTFixedTime:
		if (IterationPly < 2) // Always let it complete the 1st iteration
			break;
		if (LastTickCount > TC.FixedTimeMilliSeconds)
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

	default:
		// Never call time up in the middle of early iterations
		if (IterationPly < MinimumIterationPly) // Typically 4
			break;
		if (IterationPly == MinimumIterationPly)
			if (*normalBrain.gameRecord[normalBrain.GameRecordIndexRoot].principalVariationPointer == PVTUnknown) // Has at least one move returned a usable value?
				break;

		// Use signed integers for time calculations in case we go below zero!
		int64_t timeLeft;
		int movesLeft;

		// Get the amount of time left on the clock
		int64_t stmTime;
		if (SideToMove == 0)
			stmTime = WTime;
		else
			stmTime = BTime;
		stmTime = std::max(stmTime - 500, (int64_t)0); // Subtract 500ms buffer for a slow EGTB access
		timeLeft = std::max(stmTime - (int64_t)LastTickCount, (int64_t)0); // Don't allow timeLeft to go -ve

		// 'Panic' time up? (Have we used more than half of our remaining time? {i.e. the time we had at the start of the move})
		if (LastTickCount > (stmTime / 2))
			divisor = 9999.0f; // Set the divisor very high so that it calls time up!
		else if (RootAlpha == (short)(-MatingIn0Score)) // Give more time if failed low and still in the middle of the 'retry' iteration (i.e. haven't searched all root moves)
		{//BUT DOESN'T IT NOW SET ROOTALPHA TO -INF ON FINDING +MATE TOO???
			if (divisor == 1.0f)
				divisor = 0.25f; // (+5.7, +/ -4.2, 14268)
		}
		else
		{
			// Use less time for 'obvious' moves
			if (ConsistentBestMoves >= ((float)IterationPly * 0.75f)) // Only do this if we have had the same best move for the last 75% of iterations
				divisor *= 1.95f;
		}

		// 'Estimate' moves left to time control to give us a budget
		// N.B. MovesToGo is provided by the GUI and is the actual # of moves to make before the time control
		const int movesLeftBaseEstimate = 9;//11;
		movesLeft = movesLeftBaseEstimate;
		if (MovesToGo == 0) // 'All the moves'?
		{
			movesLeft += 1;
			if (WInc == 0) // If 'all the moves' AND no Fischer bonus then need to be VERY careful. It is assumed WInc and BInc will be the same!
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

		// Have we used enough time yet? (or only one move or done maximum ply search)
		if ((LastTickCount >= ((float)timeLeft / (float)movesLeft / divisor)) || (RootMovesCount == 1) || (IterationPly >= MaximumIterationPly))
		{
			//if (!ReplyImmediately)
			//	OutputLog(
			//		"SideToMove=" + MyITOA(SideToMove)
			//		+ ", wtime=" + MyUI64TOA(WTime) + ", btime=" + MyUI64TOA(BTime) + ", winc=" + MyUI64TOA(WInc) + ", binc=" + MyUI64TOA(BInc)
			//		+ ", Pondering=" + MyBooleanTOA(Pondering) + ", IterationPly=" + MyITOA(IterationPly) + ", ConsistentBestMoves=" + MyITOA(ConsistentBestMoves)
			//		+ ", moveTime=" + MySI64TOA(moveTime) + ", timeLeft=" + MySI64TOA(timeLeft) + ", movesLeft=" + MyITOA(movesLeft) + ", divisor=" + MyFTOA(divisor)
			//	);
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

short Normal::DrawScore(int sideToMove)
{
	// Use a function to provide the draw score (rather than a simple variable) because there are many tweaks possible!
	// Like the evaluation function, it returns a score relative to the side to move

	short ds = Contempt;
	if (SideToMove == sideToMove) // The contempt value is relative to the side to move at the root!
		ds = -ds;

	//if (normalBrain.gameRecordPointer->totalMaterial[sideToMove] > normalBrain.gameRecordPointer->totalMaterial[sideToMove ^ 1])
	//	ds++;
	//else if (normalBrain.gameRecordPointer->totalMaterial[sideToMove] < normalBrain.gameRecordPointer->totalMaterial[sideToMove ^ 1])
	//	ds--;

	//CAN THIS BE MADE BRANCHLESS??? e.g. ...
	ds += (normalBrain.gameRecordPointer->totalMaterial[sideToMove] > normalBrain.gameRecordPointer->totalMaterial[sideToMove ^ 1]);
	ds -= (normalBrain.gameRecordPointer->totalMaterial[sideToMove] < normalBrain.gameRecordPointer->totalMaterial[sideToMove ^ 1]);

	return ds;
}

//----------------------------------------------------------------------------------------------------

#pragma region TT routines

__declspec(noinline)
void Normal::ClearNormalTranspositionTable()
{
	// These nested loops below with multiple assignments can be very slow when clearing huge tables! e.g. a 16GB table takes about 3.2s
	for (uint32_t bucket = 0; bucket < NormalTranspositionTableBuckets; bucket++)
	{
		for (uint32_t entry = 0; entry < NormalTranspositionTableEntriesPerBucket; entry++)
		{
			NormalTranspositionTablePointer[bucket].Entries[entry].hash64 = 0; // Setting the hash to zero doesn't really 'clear' it (because it's a valid value) but it's useful for visual debugging!
			uint64_t newData = (((uint64_t)((uint16_t)INT16_MIN)) << 32) | (((uint64_t)TTFlagUpper) << 48) | (((uint64_t)((uint8_t)-128)) << 56); // staticEvaluation=INT16_MIN, flag=TTFlagUpper, subTreeDepth=-128
			NormalTranspositionTablePointer[bucket].Entries[entry].data = newData;
		}
	}
}

__declspec(noinline)
void Normal::AllocateNormalTranspositionTable()
{
	assert(sizeof(NormalTranspositionTableBucket_Struct) == 64);
	assert(sizeof(NormalTranspositionTableEntry_Struct) == 16);

	// Calculate the largest 'power of 2' number of entries that will fit in the specified number of bytes
	NormalTranspositionTableBuckets = 1;
	while ((NormalTranspositionTableBuckets * sizeof(NormalTranspositionTableBucket_Struct)) <= (TranspositionTableMemory * 1024ULL * 1024ULL))
		NormalTranspositionTableBuckets <<= 1;
	NormalTranspositionTableBuckets >>= 1;
	// N.B. Increasing the transposition table size may be counter-productive beyond some margin.
	// Once the table is not being completely filled after the search you are just storing the same info spread over more memory.
	// Some testing indicates that once you get more than about 50% of the table not being used you will suffer a slow down.

	// Free any previously allocated memory. If the pointer is nullptr it does nothing.
	AlignedFreeMemory(NormalTranspositionTablePointer);

	// Allocate transposition table memory
	if (NormalTranspositionTableBuckets > 0)
	{
		NormalTranspositionTableBucketsMask = NormalTranspositionTableBuckets - 1;
		NormalTranspositionTablePointer = (NormalTranspositionTableBucket_Struct*)AlignedAllocateMemory(NormalTranspositionTableBuckets * sizeof(NormalTranspositionTableBucket_Struct), 64);
		if ((NormalTranspositionTablePointer == nullptr))
		{
			Output("info string *** Error! Normal transposition table memory could not be allocated!");
			OutputError("Normal transposition table memory could not be allocated!");
			NormalTranspositionTableBuckets = 0;
		}
		else
			ClearNormalTranspositionTable();
	}
	if (IsDebug && (NormalTranspositionTablePointer != nullptr))
	{
		Output("info string Transposition table memory = " + MyUI64TOA(TranspositionTableMemory) + "MB (" + MyUI64TOA(TranspositionTableMemory * 1024ULL * 1024ULL) + " bytes)");
		Output("info string Normal transposition table bucket size = " + MyUI64TOA(sizeof(NormalTranspositionTableBucket_Struct)) + " bytes");
		Output("info string Normal transposition table entry size = " + MyUI64TOA(sizeof(NormalTranspositionTableEntry_Struct)) + " bytes");
		Output("info string Normal transposition table entries per bucket = " + MyUI64TOA(NormalTranspositionTableEntriesPerBucket));
		Output("info string Normal transposition table buckets = " + MyUI64TOA(NormalTranspositionTableBuckets));
		Output("info string Normal transposition table entries = " + MyUI64TOA(NormalTranspositionTableBuckets * NormalTranspositionTableEntriesPerBucket));
		Output("info string Normal transposition table memory allocated = " + MyUI64TOA(NormalTranspositionTableBuckets * sizeof(NormalTranspositionTableBucket_Struct) / (1024ULL * 1024ULL)) + "MB (" + MyUI64TOA(NormalTranspositionTableBuckets * sizeof(NormalTranspositionTableBucket_Struct)) + " bytes)");
	}
}

uint32_t Normal::HashfullNormalTranspositionTable()
{
	if (NormalTranspositionTableBuckets == 0)
		return 0;

	// Computes the UGI Hashfull value
	// Assuming an even distribution of used entries across the entire table a fairly accurate estimate can be made by examining a small subset of entries
	// Even with the smallest possible transposition table (1MB) we would still have 16384 buckets
	// Examining exactly 1000 entries avoids any scaling maths on return
	uint32_t usedEntries = 0;
	uint32_t bucketsToTry = 1000 / NormalTranspositionTableEntriesPerBucket;
	for (uint32_t bucket = 0; bucket < bucketsToTry; bucket++)
		for (uint32_t entry = 0; entry < NormalTranspositionTableEntriesPerBucket; entry++)
			if ((int8_t)(NormalTranspositionTablePointer[bucket].Entries[entry].data >> 56) != -128)
				usedEntries++;
	//return (uint32_t)((usedEntries * 1000) / (bucketsToTry * NormalTranspositionTableEntriesPerBucket));
	return usedEntries;
}

__declspec(noinline)
void Normal::DisplayStatisticsNormalTranspositionTable()
{
	// About 0.01% of entries are 'exact'
	// About 68.5% of entries are 'lower' ('cut' node)
	// About 31.5% of entries are 'upper' ('all' node)

	uint32_t total, unused, exact, upper, lower;
	total = unused = exact = upper = lower = 0;

	for (uint32_t bucket = 0; bucket < NormalTranspositionTableBuckets; bucket++)
	{
		for (uint32_t entry = 0; entry < NormalTranspositionTableEntriesPerBucket; entry++)
		{
			total++;
			if ((int8_t)(NormalTranspositionTablePointer[bucket].Entries[entry].data >> 56) == -128)
				unused++;
			else
			{
				uint8_t ttEUL = (NormalTranspositionTablePointer[bucket].Entries[entry].data >> 48) & TTFlagEULMask;
				if (ttEUL == TTFlagUpper)
					upper++;
				else if (ttEUL == TTFlagLower)
					lower++;
				else
					exact++;
			}
		}
	}

	Output("info string TT Statistics: Total = " + MyITOA(total) + ", Unused = " + MyITOA(unused) + "(" + MyFTOA((unused * 100) / (float)total) + "%)" + ", Exact = " + MyITOA(exact) + "(" + MyFTOA((exact * 100) / (float)total) + "%)" + ", Lower = " + MyITOA(lower) + "(" + MyFTOA((lower * 100) / (float)total) + "%)" + ", Upper = " + MyITOA(upper) + "(" + MyFTOA((upper * 100) / (float)total) + "%)");
}

void Normal::AddToNormalTranspositionTable(int8_t depthRemaining, short ply, short score, uint8_t flag, uint32_t bestMove, short tteStaticEvaluation)//, int tteFound)
{
	//if (pathDependentDraw)
	//	return;

	if (NormalTranspositionTableBuckets > 0)
	{
		// Aged entries will get replaced if DR>=STD or they are 'oldest'

		//if (depthRemaining <= -6)
		//	return;

		//normalStores++;

		//// Don't store positions where we are nearly at the 50-move draw because they are path dependent - SEEMS TO BE A SLIGHT ELO LOSS - AND CAUSES WEIRD PVs TO BE RETURNED NEAR 50-MOVES
		//if (normalBrain.gameRecordPointer->pliesSinceIrreversible >= 90)
		//	return;

		NormalTranspositionTableEntry_Struct* tte0;
		uint64_t hash64 = normalBrain.gameRecordPointer->transpositionTableHash64WithEP;
		tte0 = (NormalTranspositionTableEntry_Struct*)(NormalTranspositionTablePointer + (hash64 & NormalTranspositionTableBucketsMask));

		//if (hash64 == 17219373261132904474)
		//	AC2++;

		//if (score >= WinningBaseScore)
		//{
		//	MatingPositionsTablePointer[hash64 & MatingPositionsTableMask] = hash64;//TEST
		//}

		// Find candidate entry for replacement
		int entryToReplace = 999;
		int shallowestSubTreeDepth = 999;
		uint8_t flagEUL = flag & TTFlagEULMask;

		//if (tteFound != -1)
		//	AC2++;
		//else
		//	AC1++;
		//if (tteFound != -1)
		//{
		//	shallowestSubTreeDepth = -128;
		//	entryToReplace = tteFound;
		//	AC1++;
		//}
		//else
		{
			uint8_t oldestTranspositionTableAge = (TranspositionTableAge + 1) & TTFlagAgeMask;

			for (int entry = 0; entry < NormalTranspositionTableEntriesPerBucket; entry++)
			{
				uint64_t tteHash = tte0[entry].hash64;
				uint64_t tteData = tte0[entry].data;
				//int8_t tteSubTreeDepth = (uint8_t)((tteData >> 56) & subTreeDepthMask);
				//uint8_t tteAge = (tteData >> 48) & TTFlagAgeMask;

				// Even if tteFound==-1 (i.e. we didn't find this position in the TT when we entered this node) a reduced search might have added an entry for it! So we still have to scan for it
				// Also, if the STD of the found entry is small it might have been replaced! So tteFound is invalid! COULD TEST FOR THIS ABOVE BEFORE USING IT!
				//IT DOESN'T SEEM TO BE THE END OF THE WORLD TO HAVE THE OCCASIONAL DUPLICATE! AND IF IT'S MUCH FASTER?!
				if ((tteHash ^ tteData) == hash64) // Do we already have this position in the bucket? RE-USE TTE0!!! THEN WE CAN AVOID THIS SCAN ALTOGETHER
				{
					shallowestSubTreeDepth = -128;
					entryToReplace = entry;
					//AC2++;
					break;
				}

				//if (shallowestSubTreeDepth != -128)
				{
					uint8_t tteAge = ((NormalTranspositionTableEntryDataFields_Struct*)&tteData)->flag & TTFlagAgeMask;
					if (tteAge == oldestTranspositionTableAge) // Has the entry aged the maximum # of times?
					//if (tteAge != TranspositionTableAge) // Has the entry aged the maximum # of times? TEST
					{
						//COULD TRY USING *ANY* AGED ENTRY (SET TTFlagAgeMask TO 1? not quite the same)
						shallowestSubTreeDepth = -128;
						entryToReplace = entry;
						//AC3++;
						//if (tte0TEST != nullptr)
						//	OutputError("tte0TEST != nullptr");
						break;
					}

					int8_t tteSubTreeDepth = ((NormalTranspositionTableEntryDataFields_Struct*)&tteData)->subTreeDepth;
					if (tteSubTreeDepth < shallowestSubTreeDepth)
					{
						//THE VAST MAJORITY OF TIMES WE SCAN ALL 4 ENTRIES FOR THE SHALLOWEST
						//CAN WE REDUCE THE # OF STORES IN QS??? E.G. DON'T STORE 'ALL' NODES, don't store below certain DR
						shallowestSubTreeDepth = tteSubTreeDepth;
						entryToReplace = entry;
						//AC4++;
					}
				}
			}
		}

		assert(entryToReplace < NormalTranspositionTableEntriesPerBucket);

		//if (tteFound != -1)
		//	if (tteFound != entryToReplace)
		//		AC3++;

		//uint64_t ttetrHash = tte0[entryToReplace].hash64;
		//uint64_t ttetrData = tte0[entryToReplace].data;
		//short ttetrScore = (uint16_t)((ttetrData >> 16) & scoreMask);

		if (
			(depthRemaining >= shallowestSubTreeDepth)
			|| ((score >= EGTBWinningScore) && (flagEUL != TTFlagUpper)) // Prefer 'winning' scores ONLY DO IF ROOT SCORE>=WINNING
			|| (flagEUL == TTFlagExact)
			)
		{
			// 'Winning' scores have been 'proven' and as such should never be replaced by a score<Winning!
			// 'Mating' scores have been 'proven' to their length and as such should never be replaced by a longer mate!
			//if (!
			//	(
			//	((ttetrHash ^ ttetrData) == hash64) // Same position?
			//		&& (flagEUL != TTFlagUpper) // Cut or exact?
			//		&& (
			//		((ttetrScore == WinningBaseScore) && (score < WinningBaseScore))
			//			|| ((ttetrScore >= MatingScore) && (ttetrScore > score + ply)) // Winning?
			//			)
			//		)
			//	)
			{
				// 'Correct' any mate scores for distance (because they are relative to the root position not to this position)
				//if (score >= MatingScore)
				if (score >= EGTBWinningScore)
				{
					score += ply;
					//if (score >= MatingScore)
					//{
					//	if ((flag & TTFlagEULMask) != TTFlagUpper)
					//	{
					//		// If we have a mate at an 'exact' or 'cut' node then set its depthRemaining to at least 1. Useful in e.g. KRvKN and KBNvK as it helps with the ever increasing mate distance problem
					//		depthRemaining = std::max(depthRemaining, (int8_t)1);

					//		if (score == InfiniteBaseScore - 1) // If we have a #1 from here (15999) then ensure the flag is 'exact' as it can't be improved on!
					//			flag = flag & ~TTFlagEULMask;
					//	}
					//}
				}
				//else if (score <= MatedScore)
				else if (score <= EGTBLosingScore)
				{
					score -= ply;
					if (score <= MatedScore)
					{
						flag |= TTFlagThreatenedWithMate;
					}
				}

				uint64_t newData = (uint64_t)MGCompressMove(bestMove) | (((uint64_t)((uint16_t)score)) << 16) | (((uint64_t)((uint16_t)tteStaticEvaluation)) << 32) | (((uint64_t)(TranspositionTableAge | flag)) << 48) | (((uint64_t)((uint8_t)depthRemaining)) << 56);
				tte0[entryToReplace].data = newData;
				tte0[entryToReplace].hash64 = hash64 ^ newData;
				//normalStoresSuccessful++;
			}
		}
	}
}

//void Normal::AddToNormalTranspositionTable(int8_t depthRemaining, short ply, short score, uint8_t flag, uint32_t bestMove, short tteStaticEvaluation, NormalTranspositionTableEntry_Struct* tte0TEST)
//{
//	//if (pathDependentDraw)
//	//	return;
//
//	if (NormalTranspositionTableBuckets > 0)
//	{
//		// Aged entries will get replaced if DR>=STD or they are 'oldest'
//
//
//		//normalStores++;
//
//		NormalTranspositionTableEntry_Struct* tte0;
//		NormalTranspositionTableEntry_Struct* tte0TEST2;
//		int shallowestSubTreeDepth = 999;
//		uint8_t flagEUL = flag & TTFlagEULMask;
//		uint64_t hash64 = normalBrain.gameRecordPointer->transpositionTableHash64WithEP;
//		int entryToReplace;
//		//if (tte0TEST == nullptr)
//		{
//			//NormalTranspositionTableEntry_Struct* tte0;
//			tte0 = (NormalTranspositionTableEntry_Struct*)(NormalTranspositionTablePointer + (hash64 & NormalTranspositionTableBucketsMask));
//			tte0TEST = (NormalTranspositionTableEntry_Struct*)(NormalTranspositionTablePointer + (hash64 & NormalTranspositionTableBucketsMask));
//
//			//if (hash64 == 5388830657493423719)
//			//	AC2++;
//
//			//if (score >= WinningBaseScore)
//			//{
//			//	MatingPositionsTablePointer[hash64 & MatingPositionsTableMask] = hash64;//TEST
//			//}
//
//			// Find candidate entry for replacement
//			uint8_t oldestTranspositionTableAge = (TranspositionTableAge + 1) & TTFlagAgeMask;
//			for (int entry = 0; entry < NormalTranspositionTableEntriesPerBucket; entry++)
//			{
//				uint64_t tteHash = tte0[entry].hash64;
//				uint64_t tteData = tte0[entry].data;
//				//uint64_t tteHash = (*tte0TEST).hash64;
//				//uint64_t tteData = (*tte0TEST).data;
//				//int8_t tteSubTreeDepth = (uint8_t)((tteData >> 56) & subTreeDepthMask);
//				//uint8_t tteAge = (tteData >> 48) & TTFlagAgeMask;
//
//				if ((tteHash ^ tteData) == hash64) //TODO: Do we already have this position in the bucket? RE-USE TTE0!!! (from the probe??) THEN WE CAN AVOID THIS SCAN LOOP ALTOGETHER A LOT OF THE TIME!
//					//AND ALSO RE-USE THE 'ENTRYFOUND' VARIABLE. WOULD HAVE TO BE PASSED IN AS PARAMETERS (just need tte0 if index off the ptr itself)
//					//IS IT TRUE THAT IF YOU FIND AN ENTRY AT THE START OF THE NODE IT WILL DEFINITELY STILL BE THERE BY THE END? (SINGLE THREADED)
//				{
//					shallowestSubTreeDepth = -128;
//					entryToReplace = entry;
//					tte0TEST2 = tte0TEST;
//					break;
//				}
//
//				uint8_t tteAge = ((NormalTranspositionTableEntryDataFields_Struct*)&tteData)->flag & TTFlagAgeMask;
//				if (tteAge == oldestTranspositionTableAge) // Has the entry aged the maximum # of times
//				{
//					shallowestSubTreeDepth = -128;
//					entryToReplace = entry;
//					tte0TEST2 = tte0TEST;
//					break;
//				}
//
//				int8_t tteSubTreeDepth = ((NormalTranspositionTableEntryDataFields_Struct*)&tteData)->subTreeDepth;
//				if (tteSubTreeDepth < shallowestSubTreeDepth)
//				{
//					shallowestSubTreeDepth = tteSubTreeDepth;
//					entryToReplace = entry;
//					tte0TEST2 = tte0TEST;
//				}
//
//				tte0TEST++;
//			}
//			assert(entryToReplace < NormalTranspositionTableEntriesPerBucket);
//
//			//AC1++;
//			//if (tte0TEST != nullptr)
//			//	//if (tte0TEST != &tte0[entryToReplace])
//			//	AC2++;
//		}
//		//else
//		//	tte0TEST2 = tte0TEST;
//
//		//uint64_t ttetrHash = tte0[entryToReplace].hash64;
//		//uint64_t ttetrData = tte0[entryToReplace].data;
//		//short ttetrScore = (uint16_t)((ttetrData >> 16) & scoreMask);
//
//		if (
//			(depthRemaining >= shallowestSubTreeDepth)
//			|| ((score >= WinningBaseScore) && (flagEUL != TTFlagUpper)) // Prefer 'winning' scores ONLY DO IF ROOT SCORE>=WINNING
//			|| (flagEUL == TTFlagExact)
//			)
//		{
//			// 'Winning' scores have been 'proven' and as such should never be replaced by a score<Winning!
//			// 'Mating' scores have been 'proven' to their length and as such should never be replaced by a longer mate!
//			//if (!
//			//	(
//			//	((ttetrHash ^ ttetrData) == hash64) // Same position?
//			//		&& (flagEUL != TTFlagUpper) // Cut or exact?
//			//		&& (
//			//		((ttetrScore == WinningBaseScore) && (score < WinningBaseScore))
//			//			|| ((ttetrScore >= MatingScore) && (ttetrScore > score + ply)) // Winning?
//			//			)
//			//		)
//			//	)
//			{
//				// 'Correct' any mate scores for distance (because they are relative to the root position not to this position)
//				//if (score >= MatingScore)
//				if (score >= WinningBaseScore)
//				{
//					score += ply;
//					//if (score >= MatingScore)
//					//{
//					//	if ((flag & TTFlagEULMask) != TTFlagUpper)
//					//	{
//					//		// If we have a mate at an 'exact' or 'cut' node then set its depthRemaining to at least 1. Useful in e.g. KRvKN and KBNvK as it helps with the ever increasing mate distance problem
//					//		depthRemaining = std::max(depthRemaining, (int8_t)1);
//
//					//		if (score == InfiniteBaseScore - 1) // If we have a #1 from here (15999) then ensure the flag is 'exact' as it can't be improved on!
//					//			flag = flag & ~TTFlagEULMask;
//					//	}
//					//}
//				}
//				//else if (score <= MatedScore)
//				else if (score <= LosingBaseScore)
//				{
//					score -= ply;
//					if (score <= MatedScore)
//					{
//						flag |= TTFlagThreatenedWithMate;
//					}
//				}
//
//				uint64_t newData = (uint64_t)MGCompressMove(bestMove) | (((uint64_t)((uint16_t)score)) << 16) | (((uint64_t)((uint16_t)tteStaticEvaluation)) << 32) | (((uint64_t)(TranspositionTableAge | flag)) << 48) | (((uint64_t)((uint8_t)depthRemaining)) << 56);
//				tte0[entryToReplace].data = newData;
//				tte0[entryToReplace].hash64 = hash64 ^ newData;
//				//(*tte0TEST2).data = newData;
//				//(*tte0TEST2).hash64 = hash64 ^ newData;
//				//normalStoresSuccessful++;
//			}
//		}
//	}
//}

#pragma endregion

//----------------------------------------------------------------------------------------------------

#include "SearchNormalQuiescence.cpp"

short Normal::TreeSearchNormal(short alpha, short beta, int ply, int depthRemaining, int sideToMove, int isInCheck, bool allowNull, bool isCutNode)
{
	assert(CompareMailboxBoard64ToPiecesBB(normalBrain.mailboxBoard64, normalBrain.piecesBB));
	assert((PopulationCountX(normalBrain.piecesBB[0][King]) == 1) && (PopulationCountX(normalBrain.piecesBB[1][King]) == 1));
	assert((PopulationCountX(normalBrain.piecesBB[0][Queen]) <= 9) && (PopulationCountX(normalBrain.piecesBB[1][Queen]) <= 9));
	assert((PopulationCountX(normalBrain.piecesBB[0][Rook]) <= 10) && (PopulationCountX(normalBrain.piecesBB[1][Rook]) <= 10));
	assert((PopulationCountX(normalBrain.piecesBB[0][Bishop]) <= 10) && (PopulationCountX(normalBrain.piecesBB[1][Bishop]) <= 10));
	assert((PopulationCountX(normalBrain.piecesBB[0][Knight]) <= 10) && (PopulationCountX(normalBrain.piecesBB[1][Knight]) <= 10));
	assert((PopulationCountX(normalBrain.piecesBB[0][Pawn]) <= 8) && (PopulationCountX(normalBrain.piecesBB[1][Pawn]) <= 8));
	assert(normalBrain.piecesBB[0][AllPieces] == (normalBrain.piecesBB[0][Pawn] | normalBrain.piecesBB[0][Knight] | normalBrain.piecesBB[0][Bishop] | normalBrain.piecesBB[0][Rook] | normalBrain.piecesBB[0][Queen] | normalBrain.piecesBB[0][King]));
	assert(normalBrain.piecesBB[1][AllPieces] == (normalBrain.piecesBB[1][Pawn] | normalBrain.piecesBB[1][Knight] | normalBrain.piecesBB[1][Bishop] | normalBrain.piecesBB[1][Rook] | normalBrain.piecesBB[1][Queen] | normalBrain.piecesBB[1][King]));
	assert(normalBrain.gameRecordPointer->transpositionTableHash64 == ((sideToMove == 0) ? GenerateTranspositionTableHash64(normalBrain.mailboxBoard64, normalBrain.gameRecordPointer) : ~GenerateTranspositionTableHash64(normalBrain.mailboxBoard64, normalBrain.gameRecordPointer)));
	assert(normalBrain.gameRecordPointer->transpositionTableHash64WithEP == (normalBrain.gameRecordPointer->transpositionTableHash64 ^ TranspositionTableRandomsEnPassant[normalBrain.gameRecordPointer->epSquare]));
	assert((ply >= 1) && (ply <= MaximumPlyInMain));
	assert(depthRemaining <= MaximumPlyInMain);
	assert((sideToMove >= 0) && (sideToMove < Sides));
	assert(-MatingIn0Score <= alpha && alpha < beta && beta <= MatingIn0Score);
	assert((normalBrain.gameRecordPointer->gamePhase[0] >= 0) && (normalBrain.gameRecordPointer->gamePhase[0] <= 103) && (normalBrain.gameRecordPointer->gamePhase[1] >= 0) && (normalBrain.gameRecordPointer->gamePhase[1] <= 103));

	//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(100);

	// Preamble

	if ((NodeCount & 255) == 0)
		TimeUp(0.2f);
	// Stopping? (N.B. Must do this here as well as below in the main move processing loop else it may go back up the tree after a reduced search!)
	if (StopImmediately)
		return -MatingIn0Score;

	// Deepest so far this iteration?
	if (ply > MaximumPlyReachedBeforeQS)
	{
		MaximumPlyReachedBeforeQS = ply;
		if (IsDebug)
			LongestLineWithoutQS = normalBrain.CurrentLine(ply - 1) + " (Iteration:" + MyITOA(IterationPly) + " Ply:" + MyITOA(ply - 1) + " Alpha:" + MyITOA(alpha) + " Beta:" + MyITOA(beta) + ")";
	}

	bool isPVNode = (alpha != beta - 1);

	//----------------------------------------------------------------------------------------------------

	CRASHLOCATION(110);

#pragma region Quiescence Search Test
	// If we don't have any more draft left or we are going crazy deep, go into the quiescence search
	if ((depthRemaining <= 0) || (ply >= MaximumPlyInMain) || (ply > IterationPly * 2))
		//if ((depthRemaining <= 0) || (ply >= MaximumPlyInMain) || (ply > IterationPly * 2))
	{
		// First ensure PV nodes are 'quiet' (as the first thing the QS will do is stand pat) ONLY EXTEND AT MOST ONCE? TWICE?
		//COULD YOU DO THIS AT 1ST PLY OF QS? SO BEFORE STANDINGPAT, TEST FOR OPP HAVING P ON 7TH AND IF SO SEARCH ALL MOVES LIKE WHEN IN CHECK
		//if ((isPVNode) && (ply == IterationPly + 1))
		//if ((ply == IterationPly + 1))
			//if ((isPVNode) && (ply < IterationPly + 6))
			//if ((ply < IterationPly + 6))
		{
			//	//// KP ending with passed pawns that can move (either side)?
			//	////if (PopulationCountX(NormalGenerate.piecesBB[sideToMove][Pawn] | NormalGenerate.piecesBB[sideToMove ^ 1][Pawn]) == PopulationCountX(NormalGenerate.piecesBB[sideToMove][AllPieces] | NormalGenerate.piecesBB[sideToMove ^ 1][AllPieces]) - 2)
			//	//if (GamePhase[0] + GamePhase[1] == 0)
			//	//{
			//	//	uint64_t occupiedBB = NormalGenerate.piecesBB[0][AllPieces] | NormalGenerate.piecesBB[1][AllPieces];
			//	//	uint64_t movablePassedPawnsBB;
			//	//	movablePassedPawnsBB = passedSide1(NormalGenerate.piecesBB[0][Pawn], NormalGenerate.piecesBB[1][Pawn]) & South(~occupiedBB);
			//	//	if (movablePassedPawnsBB && ((NormalGenerate.gameRecordPointer - 1)->pliesSinceIrreversible == 0))
			//	//		//if (UINT64SetBit((NormalGenerate.gameRecordPointer - 1)->move.mf.toSquare) & passedPawnsBB)
			//	//		goto continueNormalSearch;
			//	//	else
			//	//	{
			//	//		movablePassedPawnsBB = passedSide2(NormalGenerate.piecesBB[1][Pawn], NormalGenerate.piecesBB[0][Pawn]) & North(~occupiedBB);
			//	//		if (movablePassedPawnsBB && ((NormalGenerate.gameRecordPointer - 1)->pliesSinceIrreversible == 0))
			//	//			//if (UINT64SetBit((NormalGenerate.gameRecordPointer - 1)->move.mf.toSquare) & passedPawnsBB)
			//	//			goto continueNormalSearch;
			//	//	}
			//	//}

			//	//// KP ending with stm passed pawns that can run
			//	//if (GamePhase[0] + GamePhase[1] == 0)
			//	//{
			//	//	uint64_t runnerPassedPawnsBB;
			//	//	runnerPassedPawnsBB =
			//	//		(sideToMove == 0) ?
			//	//		passedSide1(NormalGenerate.piecesBB[0][Pawn], NormalGenerate.piecesBB[1][Pawn]) & ~PassedPawnCatchableByKing[0][1][BitScanForwardX(NormalGenerate.piecesBB[1][King])] :
			//	//		passedSide2(NormalGenerate.piecesBB[1][Pawn], NormalGenerate.piecesBB[0][Pawn]) & ~PassedPawnCatchableByKing[1][0][BitScanForwardX(NormalGenerate.piecesBB[0][King])];
			//	//	if (runnerPassedPawnsBB)
			//	//		goto continueNormalSearch;
			//	//}

			//	//// KP ending with K attacking undefended P - SEEMS TO BE SLIGHTLY WORSE!
			//	//if (PopulationCountX(NormalGenerate.piecesBB[sideToMove][Pawn] | NormalGenerate.piecesBB[sideToMove ^ 1][Pawn]) == PopulationCountX(NormalGenerate.piecesBB[sideToMove][AllPieces] | NormalGenerate.piecesBB[sideToMove ^ 1][AllPieces]) - 2)
			//	//{
			//	//	int kingSquare = BitScanForwardX(NormalGenerate.piecesBB[sideToMove][King]);
			//	//	int enemyKingSquare = BitScanForwardX(NormalGenerate.piecesBB[sideToMove ^ 1][King]);
			//	//	uint64_t vulnerablePawns = KingAttacksBBList[enemyKingSquare] & NormalGenerate.piecesBB[sideToMove][Pawn] & ~KingAttacksBBList[kingSquare] &((sideToMove == 0) ? ~Side1PawnAttacksBB(NormalGenerate.piecesBB[0][Pawn]) : ~Side2PawnAttacksBB(NormalGenerate.piecesBB[1][Pawn]));
			//	//	if (vulnerablePawns)
			//	//		goto continueNormalSearch;
			//	//}

			//	//// SNTM has P on 7th, free to promote
			//	//uint64_t occupiedBB = NormalGenerate.piecesBB[0][AllPieces] | NormalGenerate.piecesBB[1][AllPieces];
			//	//uint64_t promotablePawns;
			//	//promotablePawns = (sideToMove == 0) ? NormalGenerate.piecesBB[1][Pawn] & Rank2BB & North(~occupiedBB) : NormalGenerate.piecesBB[0][Pawn] & Rank7BB & South(~occupiedBB);
			//	//if (promotablePawns && ((NormalGenerate.gameRecordPointer - 1)->pliesSinceIrreversible == 0))
			//	//	//if (((NormalGenerate.gameRecordPointer - 1)->move.toSquarePiece) || (UINT64SetBit((NormalGenerate.gameRecordPointer - 1)->move.mf.toSquare) & promotablePawns))
			//	//	goto continueNormalSearch;

			//// Multiple hanging pieces?
			//uint64_t hangingBB, potentialHangersBB;
			//potentialHangersBB = normalBrain.piecesBB[sideToMove][AllPieces] ^ normalBrain.piecesBB[sideToMove][Pawn]; // KQRBN
			//hangingBB = (East((normalBrain.piecesBB[sideToMove ^ 1][Pawn] >> 8) << (sideToMove << 4))) & potentialHangersBB;
			//if (PopulationCountX(hangingBB) > 1)
			//	goto continueNormalSearch;
			//else
			//{
			//	hangingBB |= (West((normalBrain.piecesBB[sideToMove ^ 1][Pawn] >> 8) << (sideToMove << 4))) & potentialHangersBB;
			//	if (PopulationCountX(hangingBB) > 1)
			//		goto continueNormalSearch;
			//	else
			//	{
			//		potentialHangersBB ^= (normalBrain.piecesBB[sideToMove][Bishop] | normalBrain.piecesBB[sideToMove][Knight]);
			//		hangingBB |= KnightAttacksBB(normalBrain.piecesBB[sideToMove ^ 1][Knight]) & potentialHangersBB;
			//		if (PopulationCountX(hangingBB) > 1)
			//			goto continueNormalSearch;
			//	}
			//}
		}

		return TreeSearchNormalQuiescence(alpha, beta, ply, 0, sideToMove, isInCheck); // N.B. always enter the QS with depthRemaining=0

	//continueNormalSearch:
		//depthRemaining = 1; // If we have determined that this position isn't 'quiet', continue searching for another ply in the main search rather than going into the QS
	}
#pragma endregion

	//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(120);

	//// At maximum depth possible?
	//if (ply >= MaximumPlyInQS)
	//{
	//	OutputError("Reached MaximumPly in main!\nIterationPly=" + MyITOA(IterationPly) + "\nCurrent line=" + normalBrain.CurrentLine(ply)); // TEMP
	//	return Evaluate(sideToMove);
	//}

	NodeCount++; // About 40% of nodes don't go into the QS

	//// Process any queued messages every half a second
	//if ((NodeCount & 255) == 0)
	//	if (MessagesQueued)
	//		//if (ThreadId == 0)
	//		if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - MessagesLastDisplayedClock).count() > 500)
	//			ShowQueuedMessages();

	short originalAlpha = alpha;
	assert(!(isPVNode && isCutNode));
	short bestMoveScore = -MatingIn0Score; // If anything takes over as best (a 'pv' or 'cut' node) then bestMoveScore will be equal to alpha. If nothing takes over as best (an 'all' node) then bestMoveScore will be less than alpha and will be a more accurate upper bound.
	GameRecordEntry_Struct* currentGameRecordPointer = normalBrain.gameRecordPointer;
	short drawScore = DrawScore(sideToMove);

	//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(130);

	if (ply > 1)
	{

#pragma region Draws
		// Drawn?
		int pliesSinceIrreversible = currentGameRecordPointer->pliesSinceIrreversible;
		if (pliesSinceIrreversible >= 3)
		{
			short nonStickyDrawScore = drawScore + (NodeCount & 1) * 2 - 1; // Add +/-1 randomly to avoid DBR stickiness
			if (ply == 2)
				nonStickyDrawScore--; // Slightly prefer making a drawing move at the root to drawing moves deeper in the tree i.e. take the draw now rather than possibly screwing up your position!

			// 'Immediate' draw-by-repetition
			if (nonStickyDrawScore > alpha) // Immediate repetition possible?
			{
				if (
					(((currentGameRecordPointer - 1)->move.mf.fromSquare) == ((currentGameRecordPointer - 3)->move.mf.toSquare))
					&& (((currentGameRecordPointer - 1)->move.mf.toSquare) == ((currentGameRecordPointer - 3)->move.mf.fromSquare))
					) // Did the opponent just undo his previous move?
				{
					if (nonStickyDrawScore >= beta)
					{
						//pathDependentDraw = true;
						*currentGameRecordPointer->principalVariationPointer = PVTDrawImmediateRepetition; // Should NEVER see this on the end of a PV!!!
						return nonStickyDrawScore;
					}
					// Must be a PV node
					// It is tempting to increase alpha here but it seems problematic.
					// SF sets alpha to ds but when the 'drawing' move is searched later it won't become part of the PV
					// Setting it to ds-1 seemed to lose a few ELO
				}
			}

			// Draw-by-repetition
			// Allowing the engine to assess a draw at the FIRST repeat can be dangerous at the root!
			// e.g. position fen 1nbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQk - 0 1 moves e2e4 e7e5 g1f3 b8a6 f3g1 a6b8 g1f3 - the engine will play b8a6? thinking it's a DBR but it allows the opponent to play a better move!
			// Therefore I have changed it to function at the SECOND repeat at the root
			int cycles = 4;
			int requiredRepeats = 1;
			if (ply == 2)
			{
				cycles = 8;
				requiredRepeats = 2;
			}
			if (pliesSinceIrreversible >= cycles)
			{
				// About 9% of nodes get tested here
				int repeats = 0;
				for (int i = 4; i <= pliesSinceIrreversible; i += 2) // Repetition?
				{
					// Make sure we haven't gone past the start of the array. This could happen if a position is setup from a FEN string which provides a high half-move count (for the 50-move rule).
					if ((currentGameRecordPointer - i) < &normalBrain.gameRecord[2])
						break;

					if ((currentGameRecordPointer - i)->transpositionTableHash64 == currentGameRecordPointer->transpositionTableHash64)
					{
						repeats++;
						if (repeats == requiredRepeats)
						{
							//pathDependentDraw = true;
							*currentGameRecordPointer->principalVariationPointer = PVTDrawByRepetition;
							return nonStickyDrawScore;
						}
					}
				}

				// 50-move draw
				// In this position 6k1/5pp1/4p3/1bBpP1P1/1P1P1P2/1q6/7Q/K7 w - - 99 90 Colossus played Qh5 which allows a mate in 2! :O
				// (In fact, it could have played any move including leaving a piece en-prise! Or at the 99th ply it could have allowed a 'winning' knight fork at the 100th ply!)
			// This dumb 'bowel trembling' behaviour has occurred in other games too!
				// Thankfully the GUI declared it a draw!
				if (pliesSinceIrreversible >= 100) // 50-moves made?
				{
					if (isInCheck) // Being mated on the 100th ply takes precedence over the draw!
					{
						normalBrain.CalculatePinnedPieces(sideToMove); // Required for legal move generation
						if (normalBrain.CountAllMoves(sideToMove, true) == 0)
						{
							*currentGameRecordPointer->principalVariationPointer = PVTCheckmate;
							return (short)(-MatingIn0Score + ply);
						}
					}
					*currentGameRecordPointer->principalVariationPointer = PVTDrawBy50MoveRule;
					return drawScore;
				}
			}
		}

		//// Perpetual check (done from the perspective of the checking side)
		//// Normally a perpetual check would result in a draw by repetition after 4-ply (1st repetition) or 8-ply (2nd repetition)
		//// but some are in the open board where the checking piece can't be captured because of stalemate.
		//// Many are checks by a rook which result in stalemate if the rook is captured and many are checks by a queen chasing the enemy king all over the board.
		//// These can be tested for and adjudged to be drawn reasonably safely.
		//// STILL ADJUDGES CRAZY LINES THAT COULD BE STEPPED OFF OF BY THE K
		//// E.G. e2e4 e7e5 g1f3 b8c6 d2d4 e5d4 f3d4 c6e5 b1c3 g8f6 f2f4 f6e4 f4e5 e4c3 b2c3 d8h4 e1e2 h4g4 e2e1 g4h4 e1d2 h4g5 d2d3 g5g6 d3c4 g6a6 c4d5 a6a5 d5e4 c7c6
		//if (pliesSinceIrreversible >= 12) // Only test if we've had a long series of reversible moves
		//	if ((depthRemaining >= 3)) // Don't test near the leaves to preserve speed
		//	{ //ALSO SHOULD ONLY TEST DEEPER INTO THE TREE {PLY>12} AS IT COULD MAKE SOME MISTAKEN DECISION AT THE ROOT!!! :O
		//		const short perpetualCheckScore = -8;
		//		if (perpetualCheckScore > alpha) // We only want to claim the perpetual check if we are worse
		//		{
		//			const int limit = 6;
		//			int count;
		//			count = 0;
		//			for (int i = 0; i < limit; i++) // Was the SNTM in check and moving the same piece for the previous 'limit' moves?
		//			{
		//				if (
		//					(normalBrain.gameRecordPointer - 1 - i * 2)->isInCheck
		//					&& ((normalBrain.gameRecordPointer - 1 - i * 2)->move.mf.fromSquare == (normalBrain.gameRecordPointer - 3 - i * 2)->move.mf.toSquare)
		//					)
		//					count++;
		//				else
		//					break;
		//			}
		//			if (count == limit)
		//			{
		//				count = 0;
		//				for (int i = 0; i < limit; i++) // Did the STM move the same piece for the previous 'limit' moves?
		//				{
		//					if ((normalBrain.gameRecordPointer - 2 - i * 2)->move.mf.fromSquare == (normalBrain.gameRecordPointer - 4 - i * 2)->move.mf.toSquare)
		//						count++;
		//					else
		//						break;
		//				}
		//				if (count == limit)
		//				{
		//					*normalBrain.gameRecordPointer->principalVariationPointer = PVTDrawPerpetual;
		//					//OutputError(normalBrain.CurrentLine(ply - 1));
		//					return perpetualCheckScore;
		//				}
		//			}
		//		}
		//	}

#pragma endregion

		//----------------------------------------------------------------------------------------------------
		CRASHLOCATION(140);

#pragma region Mate-distance pruning
		// 'Mate-distance pruning' (Helps massively when we have a mate score)
		// Ensures that we don't go deeper than the mate we've already got...
		// e.g. if STM at root has a #10 we won't go deeper than ply=19
		// e.g. if SNTM at root has a #10 we won't go deeper than ply=20
		// (In the 'alpha' line below we could test for 'isInCheck' and add +2 if we're not, but although it does make a slight difference it seems to harm the search depth rather than helping it!?)
		// This is also included in the QS because reductions can cause you to enter the QS early and bypass this test in main.
		// Because EITHER alpha gets increased, OR beta gets decreased, but not BOTH, you can omit the alpha test to save a few cycles during most normal searches with NO change in node count when you are mating (because the beta test is always hit a ply sooner than the alpha test)
		// In fact, I think that the alpha test only ever does something if alpha has been set to -INF
		// Must NOT be used at the root
		//alpha = std::max(-MateBaseScore + ply, (int)alpha); // If the worst possible score for the side to move in this position (i.e. being mated here) is > alpha, then increase alpha
		beta = std::min(MatingIn0Score - ply - 1, (int)beta); // If the best possible score for the side to move in this position (i.e. giving mate in 1) < beta, then decrease beta
		if (alpha >= beta)
			return alpha;
		// At ply=2, alpha is set to -15998, beta is set to 15997
		// At ply=3, alpha is set to -15997, beta is set to 15996 {BUT THIS IS THE #1 SCORE FOR THIS PLY???}
#pragma endregion

	}

	//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(150);

	// Initialisation
	MoveWithScore_Struct moveList[220];
	int legalMovesMade;
	short currentMoveScore;
	Move_Struct currentMove;

	currentGameRecordPointer->staticEvaluation = INT16_MIN;
	currentGameRecordPointer->isInCheck = isInCheck;
	currentGameRecordPointer->isTWM = 0; // These may get set if we find a TT entry
	currentGameRecordPointer->isO1M = 0;
	currentGameRecordPointer->isFMTP = 0;
	currentGameRecordPointer->isZLKM = 0;
	currentGameRecordPointer->isO1PCM = 0;
	*currentGameRecordPointer->principalVariationPointer = PVTUnknown; // Terminator

	//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(160);

#pragma region TT
	// Is this position in the tranposition table?
	NormalTranspositionTableEntry_Struct* tte0;
	Move_Struct tteBestMove;
	tteBestMove.ui32 = 0;
	int8_t tteSubTreeDepth = -128; // Useful for debugging to declare this outside the block below
	uint8_t tteEUL = TTFlagUpper;
	short tteScore = INT16_MIN;
	int tteFound = -1;
	if ((NormalTranspositionTableBuckets > 0) && (ply > 1)) //TODO: is the ply>1 test for lazy SMP???
	//if ((NormalTranspositionTableBuckets > 0))
	{
		//normalProbes++;

		uint64_t hash64 = currentGameRecordPointer->transpositionTableHash64WithEP;
		tte0 = (NormalTranspositionTableEntry_Struct*)(NormalTranspositionTablePointer + (hash64 & NormalTranspositionTableBucketsMask));

		for (int entry = 0; entry < NormalTranspositionTableEntriesPerBucket; entry++) // Do we already have this position in the table?
		{
			uint64_t data = tte0[entry].data;
			uint64_t hash = tte0[entry].hash64 ^ data;

			if (hash == hash64)
			{
				// Get the entry's data
				//tteBestMove.ui32 = MGUnCompressMove((uint16_t)(data & bestMoveMask));
				tteBestMove.ui32 = MGUnCompressMove(((NormalTranspositionTableEntryDataFields_Struct*)&data)->bestMove);
				assert((tteBestMove.ui32 == 0) == (((uint16_t)tteBestMove.ui32) == 0));
				//tteSubTreeDepth = (int8_t)((data >> 56) & subTreeDepthMask);
				tteSubTreeDepth = ((NormalTranspositionTableEntryDataFields_Struct*)&data)->subTreeDepth;
				//uint8_t flag = (uint8_t)((data >> 48) & flagMask);
				uint8_t flag = ((NormalTranspositionTableEntryDataFields_Struct*)&data)->flag;
				uint8_t tteEUL = (flag & TTFlagEULMask);
				currentGameRecordPointer->isTWM = flag & TTFlagThreatenedWithMate;
				currentGameRecordPointer->isO1M = flag & TTFlagOnlyOneLegalMove;
				currentGameRecordPointer->isFMTP = flag & TTFlagFewerMovesThanPieces;
				currentGameRecordPointer->isO1PCM = flag & TTFlagOnlyOnePieceCanMove;
				//currentGameRecordPointer->staticEvaluation = (short)((data >> 32) & staticEvaluationMask);
				currentGameRecordPointer->staticEvaluation = ((NormalTranspositionTableEntryDataFields_Struct*)&data)->staticEvaluation;
				//assert((currentGameRecordPointer->staticEvaluation == INT16_MIN) || (currentGameRecordPointer->staticEvaluation == Evaluate(sideToMove)));
				//tteScore = (short)((data >> 16) & scoreMask);
				tteScore = ((NormalTranspositionTableEntryDataFields_Struct*)&data)->score;

				if (abs(tteScore) >= EGTBWinningScore)
				{
					if (tteScore >= EGTBWinningScore) // A 'winning' score is a lower bound
					{
						if (tteScore >= MatingScore)
							tteScore -= ply;
						else
						{
							tteScore -= ply;
							if (EndgameTablebasesTreeProbeLimitMain == 0)
							{
								tteSubTreeDepth = -128; // If we're in the EGTB at the root discard any EGTB scores still in the TT
								tteScore = -MatingIn0Score;
							}
						}

						//if (tteEUL != TTFlagUpper)
						//{
						//	if ((tteScore >= beta) || (tteScore == MateBaseScore - 1 - ply))
						//		tteSubTreeDepth = MaximumPly; // We have a winning score that will cause a cutoff (or can't be improved on) so use it regardless of depthRemaining
						//}
					}
					else // A 'losing' score is an upper bound
					{
						if (tteScore < MatedScore)
							tteScore += ply;
						else
						{
							tteScore += ply;
							if (EndgameTablebasesTreeProbeLimitMain == 0)
							{
								tteSubTreeDepth = -128; // If we're in the EGTB at the root discard any EGTB scores still in the TT
								tteScore = MatingIn0Score;
							}
						}

						//if (tteEUL == TTFlagUpper)
						//{
						//	if (tteScore <= alpha)
						//		tteSubTreeDepth = MaximumPly;
						//}
					}
				}

				if (tteSubTreeDepth >= depthRemaining)
				{
					tteFound = entry;

					//if (((data >> 48) & ageMask) != TranspositionTableAge) // Touch the age for aged entries
					//{
					//	data = data & ~(3ULL << 48);
					//	data = data | ((uint64_t)TranspositionTableAge << 48);
					//	tte0[entry].data = data;
					//	tte0[entry].hash64 = hash64 ^ data;
					//}

					if (!isPVNode) // Don't use TT values at a PV node to avoid search inconsistencies {bizarrely this is an ELO gain in main but an ELO loss in QS?!?!} (-9.1, +/-3.4, 20000 for taking this out)
					//if (!isPVNode || (tteSubTreeDepth == MaximumPly)) // Don't use TT values at a PV node to avoid search inconsistencies {bizarrely this is an ELO gain in main but an ELO loss in QS?!?!} (-9.1, +/-3.4, 20000 for taking this out)
						//WHAT IF IT'S >=WINNING?!?! THEN IT'S PROVEN AND SHOULD BE USED? TEST tteSubTreeDepth=MaximumPly
if (currentGameRecordPointer->pliesSinceIrreversible < 90) // Don't probe the TT when we're very close to the 50 move draw
					{
						if (tteEUL == TTFlagLower) // Lower limit? (Came from a Cut node: exact value is "at least" (>=) this value)
						{
							if (tteScore >= beta)
							{
								PRINTTREE(PrintTree(IterationPly, ply, alpha, beta, depthRemaining, -4, tteScore, currentGameRecordPointer->staticEvaluation););
								*currentGameRecordPointer->principalVariationPointer = PVTTTLower;  // Should NEVER see this on the end of a PV!!!
								//normalProbesSuccessful++;
								return tteScore; // We can exit because we know that at least one move will exceed current beta
							}
						}
						else if (tteEUL == TTFlagUpper) // Upper limit? (Came from an All node: exact value is "at most" (<=) this value)
						{
							if (tteScore <= alpha)
							{
								PRINTTREE(PrintTree(IterationPly, ply, alpha, beta, depthRemaining, -3, tteScore, currentGameRecordPointer->staticEvaluation););
								*currentGameRecordPointer->principalVariationPointer = PVTTTUpper;  // Should NEVER see this on the end of a PV!!!
								//normalProbesSuccessful++;
								return tteScore; // We can exit because we know that no move will exceed current alpha
							}
						}
						else // Exact value. (Came from a PV node)
						{
							// If we use 'exact' entries at PV nodes it can truncate the PV returned for the best move
							PRINTTREE(PrintTree(IterationPly, ply, alpha, beta, depthRemaining, -2, tteScore, currentGameRecordPointer->staticEvaluation););
							*currentGameRecordPointer->principalVariationPointer = PVTTTExact;
							if (tteBestMove.ui32 != 0) // Sometimes there won't be a move stored as it might be a checkmate/stalemate/DBR position
							{
								*currentGameRecordPointer->principalVariationPointer = tteBestMove.ui32; // Return TT best move as part of pv
								*(currentGameRecordPointer->principalVariationPointer + 1) = PVTTTExact;
							}
							//normalProbesSuccessful++;
							return tteScore; // We can exit because we have an exact value
						}
					}
				}

				isCutNode = (tteEUL == TTFlagLower); // Try to make isCutNode more accurate for IIR

				break;
			}
		}

		//if (MatingPositionsTablePointer[~hash64 & MatingPositionsTableMask] == ~hash64)
		//{
		//	currentGameRecordPointer->isTWM |= TTFlagThreatenedWithMate;
		//}
	}
#pragma endregion


	//// Are we following the previous iterations PV as the first line searched in this iteration?
	//if (isFollowingPV && (ply > 1)) // Root move ordering handled later
	//{
	//	if (tteBestMove.ui32 != LastPrincipalVariation[ply - 1])
	//		AC1++;
	//	tteBestMove.ui32 = LastPrincipalVariation[ply - 1];
	//	// At the end of the previous PV?
	//	if ((uint16_t)LastPrincipalVariation[ply] == 0)
	//		isFollowingPV = false;//THIS DOESN'T ALWAYS KICK IN!!!
	//}


	//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(170);

#pragma region EGTB
	// Is this position in the endgame tablebases?
	short egtbScore = -MatingIn0Score; // This may be tested at the end of the node
	if (ply > 1) // Don't probe the EGTBs at the root node
	{
		if (EndgameTablebasesPiecesFound == 0)
		{
			// Just in case we don't have any EGTBs
			if (normalBrain.KnownLowMaterialDraws(sideToMove) == PVTDrawMinimumMaterial)
			{
				*currentGameRecordPointer->principalVariationPointer = PVTDrawMinimumMaterial;
				return drawScore;
			}
		}
		else
		{
			// (Size of .rtbw files: 3pc: 8K, 4pc: 1.2M, 5pc: 376M, 6pc: 67.8G)
			// (N.B. it is perfectly reasonable to query the EGTBs differently (in the tree) when we are in an EGTB position at the root!)
			int totalPieces = PopulationCountX(normalBrain.piecesBB[0][AllPieces] | normalBrain.piecesBB[1][AllPieces]); // Get how many pieces remain on the board
			if (
				(totalPieces <= EndgameTablebasesTreeProbeLimitMain)
				&& (currentGameRecordPointer->castlingStatus.ui32 == 0x01010101) // Only probe the endgame tablebases when no castling possible (8/8/8/8/8/8/1Nr3P1/R3K1k1 b Q - 0 1 Rxb2? O-O-O #13)
				//&& ((RootScore < WinningBaseScore) || (depthRemaining <= 2))// If we have a winning score at the root defer EGTB use until we reach the leaves (even if we are not yet in the EGTBs at the root). This allows us to 'see through' the EGTBs to find mates to avoid problems like this position 1Q6/3B4/k4bR1/8/n7/8/K7/8 w - - where it finds #4 because Rxf6 returns EGTB score not #2 or this position B2k4/KPpP4/n1Pb4/P5p1/5p2/5P2/8/8 b - - where it can't find the #-20 because it hits the EGTB. However, because of reductions, we sometimes skip straight into the QS and bypass the final ply EGTB test here FFS!
				//&& (((RootScore > LosingBaseScore) && (RootScore < WinningBaseScore)) || (depthRemaining <= 2))// If we have a winning score at the root defer EGTB use until we reach the leaves (even if we are not yet in the EGTBs at the root). This allows us to 'see through' the EGTBs to find mates to avoid problems like this position 1Q6/3B4/k4bR1/8/n7/8/K7/8 w - - where it finds #4 because Rxf6 returns EGTB score not #2 or this position B2k4/KPpP4/n1Pb4/P5p1/5p2/5P2/8/8 b - - where it can't find the #-20 because it hits the EGTB. However, because of reductions, we sometimes skip straight into the QS and bypass the final ply EGTB test here FFS!
				&& ((alpha < EGTBWinningScore + 1000 - ply) && (beta > EGTBLosingScore - 1000 + ply)) // Is the current window such that no EGTB score can possibly be in it? If so, skip the EGTB probe. This allows us to 'see-thru' the EGTBs to find any mates in this subtree.
				)
			{
				//EndgameTablebasesProbes++;
				//if (totalPieces >= 6)
				//	EndgameTablebasesHeavyProbes++;

				uint32_t result;
				result = tb_probe_wdl(
					normalBrain.piecesBB[0][AllPieces],
					normalBrain.piecesBB[1][AllPieces],
					normalBrain.piecesBB[0][King] | normalBrain.piecesBB[1][King],
					normalBrain.piecesBB[0][Queen] | normalBrain.piecesBB[1][Queen],
					normalBrain.piecesBB[0][Rook] | normalBrain.piecesBB[1][Rook],
					normalBrain.piecesBB[0][Bishop] | normalBrain.piecesBB[1][Bishop],
					normalBrain.piecesBB[0][Knight] | normalBrain.piecesBB[1][Knight],
					normalBrain.piecesBB[0][Pawn] | normalBrain.piecesBB[1][Pawn],
					currentGameRecordPointer->epSquare,
					(sideToMove == 0)
				);

				// Probing the EGTB (even on a SSDD) can be very slow (>>100ms)
				// We want the search to be as fast as possible so we only check TimeUp at the start of the function periodically
				// Therefore we must do it after an EGTB probe to minimise any overstep of the time control (especially at hyper-bullet speeds) as we might probe the EGTB many times
				// Even with this, we sometimes overstep at my standard 100ms/move testing. It's minimal with the 4pc but naturally increases as we move to the 5pc and 6pc.
				TimeUp(0.2f);


				if (result != TB_RESULT_FAILED)
				{
					//short egtbScore;

					EndgameTablebasesHits++;

					// Do NOT try to offset decisive scores with 'ply' (as you would with mate scores) because it just seems to reduce cutoffs and make problem solutions take longer
					switch (result)
					{
					case TB_LOSS:
						egtbScore = EGTBLosingScore - 1000 + ply;
						// If we've already got a 'losing' score at a PV node then don't use the EGTB score immediately. Allow this current subtree to be searched (to possibly find a mate) and only then use the EGTB score.
						if (isPVNode)
							if (beta <= EGTBLosingScore)
								goto EGTBExit;
						break;
					case TB_BLESSED_LOSS:
						egtbScore = -5;
						break;
					case TB_DRAW:
						// Bias EGTB draw scores towards the side with the most material (so +1, 0 or -1)
						egtbScore = (currentGameRecordPointer->totalMaterial[sideToMove] > currentGameRecordPointer->totalMaterial[sideToMove ^ 1]) - (currentGameRecordPointer->totalMaterial[sideToMove] < currentGameRecordPointer->totalMaterial[sideToMove ^ 1]);
						break;
					case TB_CURSED_WIN:
						egtbScore = 5;
						break;
					case TB_WIN:
						egtbScore = EGTBWinningScore + 1000 - ply;
						// If we've already got a 'winning' score at a PV node then don't use the EGTB score immediately. Allow this current subtree to be searched (to possibly find a mate) and only then use the EGTB score.
						if (isPVNode)
							if (alpha >= EGTBWinningScore)
								goto EGTBExit;
						break;
					}

					// Tried storing EGTB results in the TT but no significant ELO difference
					//AddToNormalTranspositionTable(depthRemaining, ply, egtbScore, TTFlagExact, PVTEGTB, egtbScore);

					*currentGameRecordPointer->principalVariationPointer = PVTEGTB;
					return egtbScore;
				}
				else
				{
					EndgameTablebasesErrors = true;
					EndgameTablebasesErrorCounts[totalPieces]++;
				}
			}
		EGTBExit:;
		}
	}
#pragma endregion

	//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(180);

#pragma region StaticEvaluation
	// Get the 'static' evaluation
	if (currentGameRecordPointer->staticEvaluation == INT16_MIN) // The value may already have been retrieved from the TT
	{
		if ((currentGameRecordPointer - 1)->move.ui32 == NullMove) // If the previous move was a null move we can use its score (negated and corrected for tempo) to save some time (about 12% of nodes)
		{
			currentGameRecordPointer->staticEvaluation = -(currentGameRecordPointer - 1)->staticEvaluation + Tempo * 2;
			//assert(currentGameRecordPointer->staticEvaluation == Evaluate(sideToMove));
		}
		else
			currentGameRecordPointer->staticEvaluation = Evaluate(sideToMove);
	}

	int improving = 0; // Used in LMP and reductions
	if ((ply > 2) && (currentGameRecordPointer->staticEvaluation > (normalBrain.gameRecordPointer - 2)->staticEvaluation))
		improving = 1;
#pragma endregion

	//----------------------------------------------------------------------------------------------------

#pragma region Razoring
	// Razoring
	if (
		(depthRemaining <= 2) // Near the leaves?
		&& (!isPVNode) // Not a PV node?
		&& (!isInCheck) // Not in check?
		)
	{
		short razoringEvaluation = alpha - 350 - (depthRemaining * 50);
		if (currentGameRecordPointer->staticEvaluation < razoringEvaluation)
		{
			if (depthRemaining == 1)
				return TreeSearchNormalQuiescence(alpha, beta, ply, 0, sideToMove, isInCheck); // N.B. always enter the QS with depthRemaining=0
			short score = TreeSearchNormalQuiescence(razoringEvaluation, razoringEvaluation + 1, ply, 0, sideToMove, isInCheck); // N.B. always enter the QS with depthRemaining=0
			if (score <= razoringEvaluation)
				return score;
		}
	}
#pragma endregion

	//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(190);

#pragma region Node level futility pruning
	// Futility pruning at node level - Is the current static evaluation for this position so far above beta that we will likely find at least one move that will cause a cutoff?
	if (
		(depthRemaining < 7) // Near the leaves? (+0.8 for doing this at every ply BUT it then misses shorter mates as they are always pruned!)TRY THIS AGAIN NOW WE HAVE THE beta > -WinningBaseScore CLAUSE BELOW
		&& (!isPVNode) // Not a PV node?
		&& (!isInCheck) // Not in check?
		&& (currentGameRecordPointer->isTWM == 0) // Not threatened with mate? (+5.3, +/-3.7, 20000)
		//&& (currentGameRecordPointer->isO1M == 0)
		//&& (currentGameRecordPointer->isFMTP == 0)//NEVER USED???
		&& (beta > EGTBLosingScore) // Otherwise we always immediately cutoff (with no moves being searched) and the move at the previous ply (which might be a shorter mate) gets discarded SLOWS SEARCH TO A CRAWL - also -4.0 ELO!
		// slows things down but helps find shortest mate faster
		//COMPARING A STATIC EVAL TO 'BEING MATED' (or egtb) IS NONSENSE THOUGH! USE ROOTSCORE???
		//AND WE DO THIS TEST IN NULLMOVE! NEED TO REVISIT!
		)
	{
		// If the side-to-move has got a winning score then alpha (and therefore beta) will be >=WinningBaseScore and so the staticEvaluation will fall far short and this won't prune
		// If the side-to-move has got a losing score then alpha (and therefore beta) will be <=LosingBaseScore and so the staticEvaluation will fall far short and this won't prune
		if (currentGameRecordPointer->staticEvaluation - (MVPawn * (depthRemaining)) >= beta) // Harder to prune the further from the leaves (N.B. this won't prune if we have got a 'winning' score)
		{
			PRINTTREE(PrintTree2(IterationPly, ply, "Node level futility pruning"););
			assert(beta < EGTBWinningScore);
			return (beta <= EGTBLosingScore ? beta : currentGameRecordPointer->staticEvaluation); // Returning the static evaluation when the opponent already has a win seemed to cause silly fail-lows
				//return beta;
			//TEST ALWAYS RETURNING BETA!
		}
	}
#pragma endregion

	//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(200);

#pragma region NullMove
	// Null move (About 90% of normal (non-QS) nodes get this far)
	// Perceived wisdom is that you shouldn't reduce into the QS. However my tests showed the opposite.
	// Also that you can't use this in endgames but I believe the added zugzwang tests now allow this!? :O

	short bestKnownScore = (tteScore != INT16_MIN ? tteScore : currentGameRecordPointer->staticEvaluation);

	if (
		(allowNull) // Null moves are not allowed at the 1st ply or immediately after a null move or when using IID but can occur multiple times on a line
		&& (!isPVNode)
		&& (!isInCheck)
		&& (currentGameRecordPointer->isTWM == 0)
		&& (currentGameRecordPointer->isO1M == 0)
		&& (currentGameRecordPointer->isFMTP == 0)
		&& (bestKnownScore >= beta)

		&& ((currentGameRecordPointer->gamePhase[sideToMove] > 0) || ((normalBrain.SafePawnMoves(sideToMove)) && normalBrain.HasOpposition(sideToMove))) // If no pieces, check for safe pawn moves and having the opposition
		//&& ((currentGameRecordPointer->gamePhase[sideToMove] > 0) || ((normalBrain.SafePawnMoves(sideToMove)) && normalBrain.HasOpposition(sideToMove) && (depthRemaining <= 8))) // If no pieces, check for safe pawn moves and having the opposition
		//&& ((currentGameRecordPointer->gamePhase[sideToMove] > 0) || ((normalBrain.SafePawnMoves(sideToMove)) && normalBrain.HasOpposition(sideToMove) && !currentGameRecordPointer->isZLKM)) // If no pieces, check for safe pawn moves and having the opposition
		//&& ((currentGameRecordPointer->gamePhase[sideToMove] > 0))
		//&& ((currentGameRecordPointer->gamePhase[sideToMove] > 0) || ((normalBrain.SafePawnMoves(sideToMove)) && normalBrain.HasOpposition(sideToMove) && (currentGameRecordPointer->isO1M == 0))) // If no pieces, check for safe pawn moves and having the opposition

		//&& ((currentGameRecordPointer->gamePhase[sideToMove] > 9) || (!currentGameRecordPointer->isZLKM)) // Helps find mates when the defending K is constrained (so the defender can't just 'null' to slash the search depth)
		//&& ((currentGameRecordPointer->gamePhase[sideToMove] > 9) || ((currentGameRecordPointer->gamePhase[sideToMove] > 0) && !currentGameRecordPointer->isZLKM)) // Helps find mates when the defending K is constrained (so the defender can't just 'null' to slash the search depth)

		//&& (!currentGameRecordPointer->isZLKM) //this slows down search massively!
		//&& (!normalBrain.ForcingLine(ply, 0)) //this slows down search massively!
		&& (beta > EGTBLosingScore) // Otherwise we will almost certainly assume a null move cutoff (with no moves being searched) and the move at the previous ply (which might be a shorter mating move) gets discarded (-2.2)
		//THE ABOVE CLAUSE DESTROYS SEARCH DEPTH!!! but SF uses it. it makes sense!!!
		//&& (ply >= nullMoveMinimumPly) // Used if we do a verification search
		)
	{ // About 54% of nodes (that get past the TT) perform a null move
		// Unfortunately null move has several drawbacks
		// 1) if the side to move is in zugzwang it is a mistake to assume that it can do nothing (hides the badness of the position)
		// 2) #1 above stops the side to move seeing a better move than the one it already has (hides the goodness of the position for stm)
		// 3) because it is used recursively it smashes the search depth

		if (
			(currentGameRecordPointer->gamePhase[sideToMove] <= 8)
			&& (currentGameRecordPointer->isZLKM = !normalBrain.KingCanLegallyMove(sideToMove))
			) // Helps find mates when the defending K is constrained (so the defender can't just 'null' to slash the search depth)
				allowNull = false;
		else if (depthRemaining > 10)
		{
			normalBrain.CalculatePinnedPieces(sideToMove); // Required for legal move generation
			int count = normalBrain.CountAllMoves(sideToMove, isInCheck);
			if (count == 1)
				allowNull = false;
			else if (count < PopulationCountX(normalBrain.piecesBB[sideToMove][AllPieces]))
				allowNull = false;
		}

		if (allowNull)
		{

			assert(ply > 1);
			// Make null move
			currentGameRecordPointer->move.ui32 = NullMove;
			currentGameRecordPointer->move.fromSquarePiece = Pawn; // Ensure CMH treats all previous null moves as Px0
			currentGameRecordPointer->move.toSquarePiece = Empty; // Ensure recapture extensions don't mistakenly kick in

			normalBrain.gameRecordPointer++; // Normally done in make/unmake-move
			normalBrain.gameRecordPointer->castlingStatus = (normalBrain.gameRecordPointer - 1)->castlingStatus;
			normalBrain.gameRecordPointer->pliesSinceIrreversible = 0; // Don't allow DBRs across a null move (+3 ELO) // (NormalGenerate.gameRecordPointer - 1)->pliesSinceIrreversible + 1;
			normalBrain.gameRecordPointer->transpositionTableHash64 = ~(normalBrain.gameRecordPointer - 1)->transpositionTableHash64;
			normalBrain.gameRecordPointer->transpositionTableHash64WithEP = normalBrain.gameRecordPointer->transpositionTableHash64;
			normalBrain.gameRecordPointer->epSquare = 0;
			*(uint32_t*)(&normalBrain.gameRecordPointer->totalMaterial[0]) = *(uint32_t*)(&(normalBrain.gameRecordPointer - 1)->totalMaterial[0]); // N.B. Using data type overload at start of line to copy for both sides! DOUBLE CHECK THIS WORKS!!!!
			*(uint64_t*)(&normalBrain.gameRecordPointer->gamePhase[0]) = *(uint64_t*)(&(normalBrain.gameRecordPointer - 1)->gamePhase[0]);
			*(uint32_t*)(&normalBrain.gameRecordPointer->totalOpeningPST[0]) = *(uint32_t*)(&(normalBrain.gameRecordPointer - 1)->totalOpeningPST[0]);
			*(uint32_t*)(&normalBrain.gameRecordPointer->totalEndgamePST[0]) = *(uint32_t*)(&(normalBrain.gameRecordPointer - 1)->totalEndgamePST[0]);
			PRINTTREE(PrintTree(IterationPly, ply, alpha, beta, depthRemaining, -1, -999, currentGameRecordPointer->staticEvaluation););

			int R;
			R = 3 + (depthRemaining / 5) + std::min(9, ((bestKnownScore - beta) / 128));
			short nullMoveScore = (short)-TreeSearchNormal((short)-beta, (short)(-beta + 1), ply + 1, depthRemaining - R - 1, sideToMove ^ 1, false, false, !isCutNode);
			//short nullMoveScore = (short)-TreeSearchNormal((short)-beta, (short)(-beta + 1), ply + 1, depthRemaining - R - 1, sideToMove ^ 1, false, false, false);//TEMP
			//TODO: shouldn't this ALWAYS be a cut node? but SF says no! GET SOME COUNTS! seems that !isCutNode predicts best?

			// About 89% of nodes after a null move are 'all' nodes

			// Unmake null move
			normalBrain.gameRecordPointer--;

			if (nullMoveScore >= beta)
			{ // About 90% of null move searches exceed beta
				////if (depthRemaining > 4) //VERIFICATION SEARCH GAINS NOTHING - BUT WOULD IT HELP WITH THOSE 'DIFFICULT' POSITIONS WHERE NULL HIDES THE SOLUTION??? it does with this #16 8/8/8/2p5/1pp5/brpp4/qpprpK1P/1nkbn3 w - -
				////{
				////	nullMoveScore = (short)TreeSearchNormal(beta - 1, beta, ply, depthRemaining - 4, sideToMove, isInCheck, false, true);IS THIS CUTNODE VALUE RIGHT???
				////	if (nullMoveScore >= beta)
				////	{ // About 99% of verification searches exceed beta
				////		return nullMoveScore;
				////	}
				////}
				////else
				//{
				//	//if (nullMoveScore >= WinningBaseScore)
				//	//	nullMoveScore = beta; // EGTB/mate scores can't be relied on ???WHY??? IS THIS TRUE??? TEST +1.4 for taking this out. No apparent issue with mate finding either
				//	PRINTTREE(PrintTree2(IterationPly, ply, "Null move pruning");)
				//	return nullMoveScore;
				//}

				////if ((nullMoveMinimumPly > 0) || ((std::abs(beta) < WinningBaseScore) && (depthRemaining < 8)) || (depthRemaining - R - 1 <= 0))
				////if ((std::abs(beta) < WinningBaseScore) && ((nullMoveMinimumPly > 0) || (depthRemaining < R))
				////if ((nullMoveMinimumPly > 0) || (R >= depthRemaining))
				//if ((nullMoveMinimumPly > 0) || (depthRemaining < 10))
				//{
					return nullMoveScore;
				//}

				//// Verification search (+0.4, +/-3.4, 20000) but doesn't seem to help much with solving mates???
				////	DON'T DO IF GOES STRAIGHT INTO QS
				////if (depthRemaining > 4)
				//{
				//	//nullMoveMinimumPly = ply + 3 * (depthRemaining - R) / 4;
				//	nullMoveSideToMove = sideToMove; //NEVER USED!!!
				//	short verificationScore;
				//	//verificationScore = (short)TreeSearchNormal(beta - 1, beta, ply, depthRemaining - R - 1, sideToMove, isInCheck, false, false);
				//	//verificationScore = (short)TreeSearchNormal(beta - 1, beta, ply, depthRemaining - R, sideToMove, isInCheck, false, false);
				//	//verificationScore = (short)TreeSearchNormal(beta - 1, beta, ply, depthRemaining - 4, sideToMove, isInCheck, false, false);
				//	verificationScore = (short)TreeSearchNormal(beta - 1, beta, ply, 4, sideToMove, isInCheck, false, false);
				//	//verificationScore = (short)TreeSearchNormal(beta - 1, beta, ply, depthRemaining >> 1, sideToMove, isInCheck, false, true);
				//	nullMoveMinimumPly = 0;
				//	if (verificationScore >= beta)
				//	{
				//		return nullMoveScore;
				//	}
				//}
				////else
				////{
				////	if (nullMoveScore >= MateBaseScore - 1000)
				////		nullMoveScore = beta; // EGTB/mate scores can't be relied on
				////	return nullMoveScore;
				////}

			}
			else
			{
				if (nullMoveScore < MatedScore)
					currentGameRecordPointer->isTWM |= TTFlagThreatenedWithMate;//WHY NOT JUST '=' ??? ALSO, DOES THIS GET SET AFTER NORMAL SEARCH BELOW RETURNS MATED SCORE? SHOULD IT???
			}
			// TODO: SET THE NODETYPE TO 'ALL' NOW AS THE NULL MOVE DIDN'T CAUSE A CUTOFF??? GATHER SOME STATS TO SUPPORT THIS
			// do after all the 'likely' moves e.g. TT, captures, checks etc
		}
	}
	// About 53% of nodes don't get cutoff by the null move
#pragma endregion

	//----------------------------------------------------------------------------------------------------

	// Internal Iterative Reductions - https://chessprogramming.org/Internal_Iterative_Reductions
	//if (ply > 1)
	//	if (tteBestMove.ui32 == 0)
	//	{
	//		//if (isPVNode)
	//		//{
	//		//	depthRemaining -= 1;// +(tteSubTreeDepth >= depthRemaining);
	//		//	if (depthRemaining <= 0)
	//		//		return TreeSearchNormalQuiescence(alpha, beta, ply, 0, sideToMove, isInCheck); // N.B. always enter the QS with depthRemaining=0
	//		//}
	//		//if (isCutNode && (depthRemaining >= 8))
	//		//if (isCutNode)
	//		if (!isPVNode)
	//		{
	//			//if (isPVNode)
	//			//	AC1++;
	//			depthRemaining -= 1;
	//		}
	//	}
	if (!isPVNode
		&& isCutNode
		&& (tteBestMove.ui32 == 0)
		&& (depthRemaining > 2)
		)
		depthRemaining -= 2;


	//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(210);

	// Generate move list
	normalBrain.CalculatePinnedPieces(sideToMove); // Required for legal move generation
	int movesCount;
	movesCount = normalBrain.GenerateAllMoves(sideToMove, isInCheck, moveList);
	assert(movesCount == normalBrain.CountAllMoves(sideToMove, isInCheck));

	//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(220);

	if (movesCount == 0) // No legal moves generated?
	{
		// Is the side to move in check?
		if (isInCheck)
		{ // Checkmate
			//assert(IsMated(sideToMove));
			assert(tteBestMove.ui32 == 0);
			*currentGameRecordPointer->principalVariationPointer = PVTCheckmate;
			return (short)(-MatingIn0Score + ply); // At the root: #1=15998, #-1=-15997, #2=15996, #-2=-15995, #3=15994...
		}
		else
		{ // Stalemate
			*currentGameRecordPointer->principalVariationPointer = PVTDrawStalemate;
			return drawScore;
		}
	}

	if (movesCount == 1)
		currentGameRecordPointer->isO1M = TTFlagOnlyOneLegalMove;

	// Useful for avoiding null move in potential zugzwang positions
	if (movesCount < PopulationCountX(normalBrain.piecesBB[sideToMove][AllPieces]))
		currentGameRecordPointer->isFMTP = TTFlagFewerMovesThanPieces;

	// Only one piece can move? (potential stalemate if piece captured or a forced position)
	if (moveList[0].mf.fromSquare == moveList[movesCount - 1].mf.fromSquare)
		currentGameRecordPointer->isO1PCM = TTFlagOnlyOnePieceCanMove;

	assert(NoDuplicateMoves(moveList, movesCount));
	assert(TranpositionTableMoveFound(moveList, movesCount, tteBestMove.ui32));

	//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(230);

	//#pragma region IID (Internal Iterative Deepening)
	//	// IID (Internal Iterative Deepening)
	//	// If we don't have a good move from the transposition table, do a shallow search to obtain one to try to improve move ordering
	//	if (tteBestMove.ui32 == 0)
	//	{
	//		if ((isPVNode) && (ply > 1))
	//		{
	//			if (depthRemaining > 1)
	//			{
	//				// This is very rarely called!
	//				short iidMoveScore = (short)TreeSearchNormal(-MateBaseScore, beta, ply, std::max(depthRemaining - 2, 1), sideToMove, isInCheck, false, isCutNode); // +1.9/20000 for using -INF as alpha
	//				tteBestMove.ui32 = *currentGameRecordPointer->principalVariationPointer;
	//				//assert(tteBestMove.ui32 != 0); //This is only true if it's a 'PV' or 'Cut' type node or we use -INF as alpha in the search above CAN NOW FAIL WITH SF STEP 10 ABOVE
	//			}
	//		}
	//	}
	//#pragma endregion

		//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(240);

	// Score moves for ordering
	int8_t pt1, pt2, ts1, ts2;

	// Get the previous move details
	pt1 = abs((currentGameRecordPointer - 1)->move.fromSquarePiece) - 1; // 0..5
	ts1 = (currentGameRecordPointer - 1)->move.mf.toSquare;
	assert((pt1 >= Pawn - 1) && (pt1 <= King - 1) && (ts1 >= A1) && (ts1 <= H8));
	//currentGameRecordPointer->historyPointer = &CounterMoveHistory[pt1][ts1];
	currentGameRecordPointer->historyPointer = &CounterMoveHistory->CMH[pt1][ts1];

	int8_t fupt1, futs1;
	fupt1 = abs((currentGameRecordPointer - 2)->move.fromSquarePiece) - 1; // 0..5
	futs1 = (currentGameRecordPointer - 2)->move.mf.toSquare;
	assert((fupt1 >= Pawn - 1) && (fupt1 <= King - 1));

	if (ply == 1)
		ScoreRootMoveList(moveList);
	else
		normalBrain.ScoreMoves(moveList, movesCount, tteBestMove.ui32, ply, KillerMoves, &CounterMoves[pt1][ts1], &FollowUpMoves[fupt1][futs1]);

	//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(250);

	// Loop through move list
	legalMovesMade = 0;
	int enemyKingSquare = BitScanForwardX(normalBrain.piecesBB[sideToMove ^ 1][King]);
	int winningCaptureIndex = 999;
	//uint64_t passedPawnsBB = (sideToMove == 0) ? passedSide1(normalBrain.piecesBB[0][Pawn], normalBrain.piecesBB[1][Pawn]) : passedSide2(normalBrain.piecesBB[1][Pawn], normalBrain.piecesBB[0][Pawn]);
	//uint64_t passedPawnRunnersBB = 0;
	//if (GamePhase[sideToMove ^ 1] < 15)
	//	passedPawnRunnersBB = passedPawnsBB & ~PassedPawnCatchableByKing[sideToMove][sideToMove ^ 1][BitScanForwardX(NormalGenerate.piecesBB[sideToMove ^ 1][King])] & ~SeventhRankBB[sideToMove];
	uint32_t quietMovesSearched[220];
	int quietMovesSearchedCount = 0;
	bool tteBestMoveIsQuiet = true;

	bool keepScanning = true;
	for (int moveListIndexIterator = 0; moveListIndexIterator < movesCount; moveListIndexIterator++)
	{
		// Get next move
		int bestSortScore = moveList[moveListIndexIterator].score;
		int bestSortIndex = moveListIndexIterator;

		if (keepScanning)
		{
			for (int index = moveListIndexIterator + 1; index < movesCount; index++)
			{
				if (moveList[index].score > bestSortScore)
				{
					bestSortScore = moveList[index].score;
					bestSortIndex = index;
				}
			}

			// Give up scanning for highest scoring move when we've hit the minimum bestSortScore
			if ((bestSortScore <= 0) && (ply > 1)) // Past 'special' moves? (TT, captures, killers, counter-moves, follow-up-moves and non-zero history moves) WILL WE EVER HAVE MULTIPLE MOVES AT 0 AT THE ROOT???
			{
				keepScanning = false;
				// Updating isCutNode here improves the %age accuracy of the node type
				// Without it we get about 71% cut/76% all correct
				// With it we get about 74% cut/98% all correct
				isCutNode = false;//TODO: A GOOD PLACE TO SET THE NODE TYPE TO 'ALL' v71 - used by iir
			}
		}
		//assert((bestSortScore >= 0) && (bestSortIndex >= 0) && (bestSortIndex < movesCount));
		assert((bestSortIndex >= 0) && (bestSortIndex < movesCount));

		currentMove.ui32 = moveList[bestSortIndex].ui32;
		//assert((IterationPly == 1) || (ply > 1) || (moveListIndexIterator > 0) || (RootBestMove.ui32 == currentMove.ui32)); // Confirm we are searching the previous best root move first DOESN'T WORK UNLESS WE SET RootBestMove AFTER A ROOT FAIL HIGH BUT THAT LOSES ELO

		// Calculate the SEE result
		// N.B. Only captures/proms can be SEE winning
		int SEEResult;
		SEEResult = normalBrain.SEE(currentMove.mf.fromSquare, currentMove.mf.toSquare, sideToMove);
		currentGameRecordPointer->SEEResult = SEEResult;
		if (SEEResult == 1)
			winningCaptureIndex = moveListIndexIterator;

		currentGameRecordPointer->move.ui32 = currentMove.ui32;
		if ((moveListIndexIterator == 0) && (tteBestMove.ui32 == 0))
			tteBestMove.ui32 = currentMove.ui32; // If we don't have a 'best move' from the transposition table, use the highest ordered move. This will get saved later if this is an 'All' type node. This can help when trying to detect tranposition table type-2 errors.
		moveList[bestSortIndex] = moveList[moveListIndexIterator]; // Re-position the first move in the list
		if (ply == 1)
		{
			ShowProgressMessage(currentMove.ui32, moveListIndexIterator + 1, bestMoveScore, alpha, beta); // Display current root move (always display first move)
			if (EndgameTablebasesRootMove != 0) // Are we in the EGTB at the root
			{
				int wdl = RetrieveRootMoveWDLStatus(currentMove.ui32);
				if (wdl < EndgameTablebasesRootWDL)
					continue; // Discard moves that don't preserve the root EGTB result
			}
		}
		PRINTTREE(PrintTree(IterationPly, ply, alpha, beta, depthRemaining, currentMove.ui32, bestSortScore, currentGameRecordPointer->staticEvaluation);)
#ifdef _DEBUG
			// Are we following the previous iterations PV as the first line searched in this iteration?
			if (isFollowingPV)
			{
				if (LastPrincipalVariation[ply - 1] == currentMove.ui32)
				{
					// At the end of the previous PV?
					if ((uint16_t)LastPrincipalVariation[ply] == 0)
						isFollowingPV = false;
				}
				else
				{
					//if ((LastPrincipalVariation[ply - 1] & 0xFFFF) != 0)
					AC9++;//TEMP
					//assert(0);
					isFollowingPV = false;
				}
			}
#endif

		// Up-date move
		currentGameRecordPointer->isThreateningMateInOne.ui32 = 0;//TESTING - required for tmi1 test below
		normalBrain.MakeMove(sideToMove); // N.B. MakeMove increments normalBrain.gameRecordPointer!
		legalMovesMade++;
		//pathDependentDraw = false;

		// Initiate the retrieval of the next transposition table cache line as soon as possible
		_mm_prefetch((char*)(NormalTranspositionTablePointer + (normalBrain.gameRecordPointer->transpositionTableHash64 & NormalTranspositionTableBucketsMask)), _MM_HINT_T0);

#ifdef SEARCHINGFORLINE
		if (TargetLineLength == ply)
		{
			std::string cl = normalBrain.CurrentLine(ply);
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
			TargetLineLastSearched[ply] = currentMove;

			if (TargetLine == cl)
				AC1++;

			////TargetLineRefutationsDepth
			//if (TargetLineRefutationsDepth == ply)
			//	Output(MoveNotation(currentMove.ui32));
			//else if (TargetLineRefutationsDepth > ply)
			//	AC1++;

			//if (TargetLinePrintDepth == ply)
			//	Output(MoveNotation(currentMove.ui32));
		}
#endif

		bool givesCheck = normalBrain.IsEnemyKingAttacked(enemyKingSquare, sideToMove);
		bool quietMove = ((currentMove.mf.flag < MFPromotion) && (currentGameRecordPointer->move.toSquarePiece == Empty)); // N.B. toSquarePiece gets set in MakeMove
		if (currentMove.ui32 == tteBestMove.ui32)
			tteBestMoveIsQuiet = quietMove;

		//----------------------------------------------------------------------------------------------------

		int newDepthRemaining = depthRemaining - 1;

		// N.B. Sometimes it is an aspect of the move that makes it interesting (e.g. a P just moved to the 7th rank) and sometimes an aspect of the position (e.g. there is a P on the 7th rank)

		// Extensions (about 1% of moves get extended)
		int extensions = 0;
		int TMI1ExtensionsSaved = TMI1Extensions;
		if ((ply <= IterationPly * 2) && (ply + depthRemaining <= MaximumIterationPly)) // Hard limits to try to avoid things like hitting the MaximumPly, endless K chases in the endgame etc
		{
			if ( // 'Safe' check?
				(givesCheck)
				&& (!normalBrain.SEETargetPieceUnsafe(currentMove.mf.toSquare, sideToMove ^ 1, 0))
				)
				extensions = 1;
			else if ( // Recapture?
				(currentMove.mf.toSquare == (currentGameRecordPointer - 1)->move.mf.toSquare)
				&& ((currentGameRecordPointer - 1)->move.toSquarePiece)
				&& (SimplePieceValues[std::abs(currentGameRecordPointer->move.toSquarePiece)] == SimplePieceValues[std::abs((currentGameRecordPointer - 1)->move.toSquarePiece)]) // ExE?
				&& ((currentGameRecordPointer - 1)->move.mf.flag < MFPromotion) // Exclude promotions at the previous ply as otherwise can accidentally extend underpromotions and wouldn't strictly be an ExE capture
				)
				extensions = 1;
			else if ( // Only one move?
				(movesCount == 1)// &&
				&& (currentMove.mf.toSquare != (currentGameRecordPointer - 1)->move.mf.toSquare) // Not a recapture of last moved piece (filters out dumb throw-away checks from the previous ply)
				)
				extensions = 1;

			newDepthRemaining += extensions;
		}

		//----------------------------------------------------------------------------------------------------

		//Pruning
		if (
			(legalMovesMade > 1) // Don't prune the 1st move WHY??? UNLESS IT'S TT, MVVLVA, KR, CM, FUM WHICH IT MIGHT NOT BE! OR O1M! CHECK ITS bestSortScore - GOES CRAZY IF YOU TAKE IT OUT!!!
			&& (extensions == 0)
			&& (!isPVNode) // Ensures ply>1
			&& (!isInCheck) // N.B. you must NOT remove this otherwise you may get false mates returned! NOT TRUE??? IT'S THE >MATEDSCORE TEST BELOW THAT DOES THE TRICK??? but wiki says to use it!
			&& (!givesCheck)
			//&& (SEEResult <= 0) // Don't prune SEE winning moves
			&& (quietMove)
			//&& (beta > LosingBaseScore) // Never prune if we're 'losing' as we want to try EVERYTHING to find a way out! BUT IF BETA<=LosingBaseScore THEN SO TOO IS ALPHA AND THE TEST BELOW COULD NEVER KICK IN?!
			//&& (alpha < WinningBaseScore) // If we have a 'winning' score then EVERY (non-special/quiet) move will be futility pruned and you could 'lose' the EGTB win or miss a better mate! So do NOT take this out!!!
			//&& (!passedPawnMove)// || (SEEResult < 0))
			//&& (currentGameRecordPointer->isTWM == 0)
			)
		{
			assert(currentMove.ui32 != tteBestMove.ui32);
			assert(ply > 1);

			// Late move pruning
			if (
				(depthRemaining <= IterationPly / 3)
				&& (quietMovesSearchedCount > lateMovePruningMargins[improving][std::min(depthRemaining, 8)])
				&& (bestMoveScore > EGTBLosingScore) // Don't prune if we're losing!
				&& (!((std::abs(currentGameRecordPointer->move.fromSquarePiece) == Pawn) && ((currentMove.mf.toSquare >> 3) == SeventhRank[sideToMove]))) // P move to 7th?
				&& (bestSortScore <= 0)
				)
			{
				PRINTTREE(PrintTree2(IterationPly, ply, "LMP");)
				normalBrain.UnMakeMove(sideToMove);
				continue;
			}

			// Futility pruning
			short futilityScore;
			if (
				(depthRemaining <= 8)
				&& ((futilityScore = currentGameRecordPointer->staticEvaluation + MaterialValue[abs(currentGameRecordPointer->move.toSquarePiece)] + FutilityMargin[depthRemaining]) <= alpha)
				&& (bestMoveScore > EGTBLosingScore) // Don't prune if we're losing!
				&& (!((std::abs(currentGameRecordPointer->move.fromSquarePiece) == Pawn) && ((currentMove.mf.toSquare >> 3) == SeventhRank[sideToMove]))) // P move to 7th?
				)
			{
				PRINTTREE(PrintTree2(IterationPly, ply, "Futility pruning"););
				normalBrain.UnMakeMove(sideToMove);
				continue;
			}

			// SEE pruning
			if (
				(depthRemaining <= 6)
				&& (SEEResult < 0)
				&& (bestMoveScore > EGTBLosingScore) // Don't prune if we're losing! (Do NOT take this out otherwise we could get faulty mate scores returned e.g. 7r/B1b1qbPB/2k1q2q/1Nq2nN1/qQQ2QQq/8/q1RRQ2q/3K4 w - - 0 1, go depth 3, returns #3 when it's actually #7!)
				)
			{
				PRINTTREE(PrintTree2(IterationPly, ply, "SEE pruning"););
				normalBrain.UnMakeMove(sideToMove);
				continue;
			}
		}

		//----------------------------------------------------------------------------------------------------


		bool passedPawnMove = false;
		if (std::abs(currentGameRecordPointer->move.fromSquarePiece) == Pawn)
		{
			uint64_t passedBB;
			if (sideToMove == 0)
				passedBB = passedSide1(normalBrain.piecesBB[0][Pawn], normalBrain.piecesBB[1][Pawn]);
			else
				passedBB = passedSide2(normalBrain.piecesBB[1][Pawn], normalBrain.piecesBB[0][Pawn]);
			passedPawnMove = (passedBB & UINT64SetBit(currentGameRecordPointer->move.mf.toSquare));
		}




		bool doNonReducedSearch;
		int searches = 0;

		// Late move reductions (can massively increase search depth but also make the best root move returned very sensitive to move ordering)
		// REDUCE LESS/MORE BASED ON bestSortScore
		int reductions = 0;
		if (
			(legalMovesMade > 1) // N.B. the TT move is always searched first so if it exists it will have been searched COULD STILL REDUCE THIS IF NO TT MOVE???
			&& (extensions == 0)
			&& (!isPVNode)
			&& (!isInCheck)
			&& (!givesCheck)
			&& (SEEResult <= 0) // Don't reduce SEE winning moves
			&& ((bestSortScore < INT_MAX - 200) || (SEEResult < 0)) // Don't reduce any special moves (TT, captures/proms, killers, CM, FUM) unless they are SEE losing
			&& (depthRemaining > 1) // No point in reducing too near the leaves as you'll go straight into the QS anyway! And if you did reduce, it might research it!!
			&& (!passedPawnMove)// || (SEEResult < 0))
			//&& (!((std::abs(currentGameRecordPointer->move.fromSquarePiece) == Pawn) && ((currentMove.mf.toSquare >> 3) == SeventhRank[sideToMove]))) // P move to 7th?
			//&& (quietMove)
			//(currentGameRecordPointer->isTWM == 0) &&
			//(!(UINT64SetBit(currentMove.mf.fromSquare) & passedPawnsBB)) && // Don't reduce moves by passed Ps

			////(
			////(abs((NormalGenerate.gameRecordPointer - 1)->move.fromSquarePiece) != Knight) ||
			////(PopulationCountX(KnightAttacksBBList[currentMove.mf.toSquare] & (NormalGenerate.piecesBB[sideToMove ^ 1][Queen] | NormalGenerate.piecesBB[sideToMove ^ 1][Rook])) <= 1)
			////) &&
			////(ply > RootPliesFullWidth) &&

			//( // P fork?
			//(abs(currentGameRecordPointer->move.fromSquarePiece) != Pawn) ||
			//(
			//PopulationCountX(
			//	((sideToMove == 0) ? PawnAttacksBBList[0][currentMove.mf.toSquare] : PawnAttacksBBList[1][currentMove.mf.toSquare]) &
			//	(normalBrain.piecesBB[sideToMove ^ 1][Queen] | normalBrain.piecesBB[sideToMove ^ 1][Rook] | normalBrain.piecesBB[sideToMove ^ 1][Bishop] | normalBrain.piecesBB[sideToMove ^ 1][Knight])
			//	) <= 1
			//)
			//) //&&
			//&& (currentGameRecordPointer->isFMTP == 0)//TESTING

			////(!(matingMove = IsThreateningMateInOne(sideToMove)))
			////(!((matingMove == -1) ? (matingMove = IsThreateningMateInOne(sideToMove)) : matingMove))
			////&& (RootScore < MatingScore) // Never reduce if we're mating! Assures that shorter mates are found quickly SLOWS SEARCH MASSIVELY WHEN KICKS IN
			////&& (normalBrain.KingCanLegallyMove(sideToMove ^ 1)) // Don't reduce if opponent's K now has zero moves TESTING
			////&& (!(currentGameRecordPointer - 1)->isZLKM) // Don't reduce if opponent's K now has zero moves TESTING
			//&& ((!normalBrain.ForcingLine(ply, 2)) || normalBrain.KingCanLegallyMove(sideToMove ^ 1)) // Don't reduce if opponent's K has zero moves along the entire current line TESTING
			////&& (alpha < WinningBaseScore - 1) // TEMP TEST : HAVING THIS IN CAUSES THE SEARCH TO EXPLODE AS SOON AS A MATE IS FOUND : If the stm has a winning score then EVERY (non-special/quiet) move will be pruned and you could 'lose' the EGTB win or miss a better mate! So do NOT take this out!!!
			)
		{
			assert(tteBestMove.ui32 != currentMove.ui32);

			reductions = 1;
			//reductions = 4;
			//reductions = std::max(1, depthRemaining >> 1);
			if (bestSortScore <= 0)
			{
				reductions++;
				reductions += (legalMovesMade >> 4);
			}
			if (SEEResult < 0)
				//reductions++;
				reductions += 2;
			if (!improving)
				reductions++;

			//if (quietMovesSearchedCount > lateMovePruningMargins[improving][depthRemaining])//TEST
			//	reductions++;



			//if (isCutNode && quietMove)
			//	reductions++;
			//if (currentGameRecordPointer->isZLKM)
			//	reductions++;
			//if (winningCaptureIndex < moveListIndexIterator)
			//	reductions++;
			//if (!tteBestMoveIsQuiet)
			//	if (tteEUL != TTFlagUpper)
			//		reductions++;
			//reductions = reductions * 2;

			//REDUCE MORE IF MOVING THE SAME PIECE TWICE IN A ROW??? ESPECIALLY IF COULD'VE MOVED THERE IN ONE MOVE PREVIOUSLY OR UNDOING PREVIOUS MOVE
			//// Sliders taking 2 moves instead of 1?
			//if (normalBrain.gameRecordPointer->pliesSinceIrreversible >= 3)
			//	if (currentMove.mf.fromSquare == (normalBrain.gameRecordPointer - 3)->move.mf.toSquare)
			//		if (!(normalBrain.gameRecordPointer - 2)->isInCheck)
			//			if (!(normalBrain.gameRecordPointer - 1)->isInCheck)
			//				if (alpha > 0)
			//				{
			//					int8_t piece = abs(normalBrain.mailboxBoard64[currentMove.mf.toSquare]);
			//					//if (piece == King)
			//					//{
			//					//	if (ChebyshevDistance[(normalBrain.gameRecordPointer - 3)->move.mf.fromSquare][currentMove.mf.toSquare] <= 1)
			//					//		reductions += 1;
			//					//}
			//					//else 
			//					if ((piece == Bishop) || (piece == Rook) || (piece == Queen))
			//						if (LineListBB[currentMove.mf.fromSquare][currentMove.mf.toSquare] & UINT64SetBit((normalBrain.gameRecordPointer - 3)->move.mf.fromSquare))
			//							reductions += 2;//THIS REDUCES MOVES FOR THE LOSING SIDE THAT REPEAT... SHOULD ONLY REDUCE IF WINNING!
			//				}





			// Do a reduced search with a zero width window
			assert(reductions > 0);
			searches++;
			currentMoveScore = (short)-TreeSearchNormal((short)(-alpha - 1), (short)-alpha, ply + 1, newDepthRemaining - reductions, sideToMove ^ 1, givesCheck, true, true);

			// If it doesn't fail low, mark it for research
			// About 99% of reduced searches don't exceed alpha
			doNonReducedSearch = (currentMoveScore > alpha);
		}
		else
		{
			doNonReducedSearch = (!isPVNode) || (legalMovesMade > 1);
		}




		if ((IterationPly == 1) && (ply == 1))
		{
			// On the 1st iteration get exact scores for every root move
			currentMoveScore = (short)-TreeSearchNormal((short)-MatingIn0Score, (short)MatingIn0Score, ply + 1, newDepthRemaining, sideToMove ^ 1, givesCheck, true, false);
		}
		else
		{
			if (doNonReducedSearch)
			{//SURELY THIS SHOULD NOW BE A CUT NODE SO THE CHILD SHOULD BE AN 'ALL' NODE?!?!
				searches++;
				// Minimal window search
				currentMoveScore = (short)-TreeSearchNormal((short)(-alpha - 1), (short)-alpha, ply + 1, newDepthRemaining, sideToMove ^ 1, givesCheck, true, !isCutNode);
				// About 79% of non-reduced searches don't exceed alpha
			}

			if (isPVNode && ((legalMovesMade == 1) || ((currentMoveScore > alpha) && ((ply == 1) || (currentMoveScore < beta)))))
			{
				assert(reductions == 0);
				assert(searches <= 1);
				assert(beta > alpha + 1); // i.e. isPVNode
				searches++;

				currentMoveScore = (short)-TreeSearchNormal((short)-beta, (short)-alpha, ply + 1, newDepthRemaining, sideToMove ^ 1, givesCheck, true, false);
				//assert(PVSearchedFirst(ply));
			}
			assert((searches > 0) && (searches < 3));
		}

		//----------------------------------------------------------------------------------------------------

#ifdef SEARCHINGFORLINE
//if (TargetLineLength == ply)
		{
			std::string cl = normalBrain.CurrentLine(ply);

			if (TargetLine == cl)
			{
				TargetLineRefutedBy = TargetLineLastSearched[ply + 1];
				AC2++;
			}

			////TargetLineRefutationsDepth
			//if (TargetLineRefutationsDepth == ply)
			//	Output(MoveNotation(currentMove.ui32));
			//else if (TargetLineRefutationsDepth > ply)
			//	AC1++;
		}
#endif

		//----------------------------------------------------------------------------------------------------
		CRASHLOCATION(260);

		// Down-date move
		normalBrain.UnMakeMove(sideToMove); // N.B. UnMakeMove decrements normalBrain.gameRecordPointer!
		TMI1Extensions = TMI1ExtensionsSaved;

		//----------------------------------------------------------------------------------------------------
		CRASHLOCATION(270);

		// Stopping? (N.B. Must do this BEFORE the 'new best move' test below otherwise a partially searched move could take over as best or be added to the TT!)
		if (StopImmediately)
			return -MatingIn0Score;

		//----------------------------------------------------------------------------------------------------
		CRASHLOCATION(280);

		// Root move?
		if (ply == 1)
		{
			// If we are in the EGTB at the root then all moves that don't preserve the EGTB status will already have been pruned above
			if (EndgameTablebasesRootMove != 0)
				if (std::abs(currentMoveScore) < MatingScore) // No mate found?
				{
					int dtz = RetrieveRootMoveDTZStatus(currentMove.ui32);
					if (dtz > EndgameTablebasesRootDTZ) // Sub-optimal DTZ?
						currentMoveScore = -MatingIn0Score; // Discard
					// Only moves which preserve the EGTB result and optimal DTZ use their actual search scores

					//else
					//{
					//	// 1: Mating scores are kept
					//	// 2: If we are winning then the score is set to choose the shortest DTZ
					//	// 3: Drawing scores in a drawn position are kept
					//	// 4: If we are losing then the score is set to choose the longest DTZ
					//	//int dtz = RetrieveRootMoveDTZStatus(currentMove.ui32);
					//	//if (EndgameTablebasesRootWDL > 0)
					//	//	currentMoveScore = WinningBaseScore - dtz;
					//	//else if (EndgameTablebasesRootWDL < 0)
					//	//	currentMoveScore = -WinningBaseScore + dtz;
					//}
				}

			if (IterationPly == 1)
				SaveRootMoveData(currentMove.ui32, NodeCount + NodeCountQuiescenceSearch, currentMoveScore); // On the first iteration, save every root moves fail-soft score and subtree size

			// Should we stop the search? (Sets a couple of flags internally which are tested elsewhere)
			TimeUp(1.0f);
		}

		//----------------------------------------------------------------------------------------------------
		CRASHLOCATION(290);

		// New best move?
		if (currentMoveScore > bestMoveScore)
		{
			if (currentMoveScore > alpha)
			{
				if ((ply == 1) && (IterationPly > 1)) // On the 2nd and subsequent iterations, flag any new best root move to be at the top of the list
					UpdateRootMovePriority(currentMove.ui32);

				if (isPVNode)
					normalBrain.SavePrincipalVariation(currentMove.ui32); // Save the PV even if we (are about to) fail high as it might be useful for IID

				if (currentMoveScore >= beta)
				{
					CRASHLOCATION(292);
					if (quietMove)
					{
						// Update killers
						if (KillerMoves[ply].m1.ui32 != currentGameRecordPointer->move.ui32)
						{
							KillerMoves[ply].m2 = KillerMoves[ply].m1;
							KillerMoves[ply].m1.ui32 = currentGameRecordPointer->move.ui32;
							KillerMoves[ply].m1.piece = currentGameRecordPointer->move.fromSquarePiece;
						}

						// Update counter move history values
						pt2 = abs(currentGameRecordPointer->move.fromSquarePiece) - 1;
						ts2 = currentGameRecordPointer->move.mf.toSquare;
						assert((pt1 >= Pawn - 1) && (pt1 <= King - 1) && (pt2 >= Pawn - 1) && (pt2 <= King - 1) && (ts1 >= A1) && (ts1 <= H8) && (ts2 >= A1) && (ts2 <= H8));
						assert((1 << 30) >= currentGameRecordPointer->historyPointer->History[pt2][ts2]);
						int delta = depthRemaining * depthRemaining;
						currentGameRecordPointer->historyPointer->History[pt2][ts2] = std::min(currentGameRecordPointer->historyPointer->History[pt2][ts2] + delta, (1 << 30));
						//if (currentGameRecordPointer->historyPointer->History[pt2][ts2] == (1 << 30))
						//	OutputError("History overflow!");
						for (int count = 0; count < quietMovesSearchedCount; count++)
						{
							Move_Struct ms;
							ms.ui32 = quietMovesSearched[count];
							int8_t pt = std::abs(normalBrain.mailboxBoard64[ms.mf.fromSquare]) - 1;
							int8_t ts = ms.mf.toSquare;
							currentGameRecordPointer->historyPointer->History[pt][ts] = std::max(currentGameRecordPointer->historyPointer->History[pt][ts] - delta, 0);// -(1 << 30));
						}

						// Update counter moves
						if (CounterMoves[pt1][ts1].m1.ui32 != currentGameRecordPointer->move.ui32)
						{
							CounterMoves[pt1][ts1].m2.ui32 = CounterMoves[pt1][ts1].m1.ui32;
							CounterMoves[pt1][ts1].m1.ui32 = currentGameRecordPointer->move.ui32;
						}

						// Update follow-up moves
						if (FollowUpMoves[fupt1][futs1].m1.ui32 != currentGameRecordPointer->move.ui32)
						{
							FollowUpMoves[fupt1][futs1].m2.ui32 = FollowUpMoves[fupt1][futs1].m1.ui32;
							FollowUpMoves[fupt1][futs1].m1.ui32 = currentGameRecordPointer->move.ui32;
						}
					}

					// This move has returned a score >= beta, therefore this is a 'Cut' node
					// The currentMoveScore is a lower bound (floor) on the exact score of the node (i.e. the exact score might be greater than currentMoveScore, it is "at least" currentMoveScore)
					CRASHLOCATION(294);
					//if (!allowNull)
					//{
					//	AC2++;//TEMP
					//	if (isCutNode)
					//		AC5++;
					//}
					assert(currentMove.ui32 == currentGameRecordPointer->move.ui32);
					assert((currentGameRecordPointer->isTWM == 0) || (currentGameRecordPointer->isTWM == TTFlagThreatenedWithMate));
					assert((currentGameRecordPointer->isO1M == 0) || (currentGameRecordPointer->isO1M == TTFlagOnlyOneLegalMove));
					assert((currentGameRecordPointer->isFMTP == 0) || (currentGameRecordPointer->isFMTP == TTFlagFewerMovesThanPieces));
					AddToNormalTranspositionTable(depthRemaining, ply, currentMoveScore, TTFlagLower + currentGameRecordPointer->isTWM + currentGameRecordPointer->isO1M + currentGameRecordPointer->isFMTP + currentGameRecordPointer->isO1PCM, currentGameRecordPointer->move.ui32, currentGameRecordPointer->staticEvaluation);// , tteFound);

					if (ply == 1) // Failed high at the root?
					{
						//UpdateRootMoveStats(currentMove.ui32, currentMoveScore, IterationPly);//NEW Replace the actual node count with huge pseudo node count based on the score

						ShowBestLineMessage(currentMoveScore, TTFlagLower);
						//SEE *** COMMENT! RootBestMove SHOULD BE UPDATED WHEN WE GET A FAIL HIGH!!! OTHERWISE IT COULD STOP SEARCH BEFORE RE-SEARCH COMPLETE AND STILL USE THE PREVIOUS BEST MOVE :o
						//BUT WHAT IF IT THEN FAILS LOW??? :o
						RootBestMove = currentMove; // ELO loss if take this out!
						//TODO: SHOULD/CAN WE REMAIN IN THIS FUNCTION AND DO THE RESEARCH FROM HERE??? RATHER THAN HANDLING OUTSIDE???
					}

					PRINTTREE(PrintTree2(IterationPly, ply, "Cut"););
					return currentMoveScore;
				}

				// PV node: score >alpha and <beta
				CRASHLOCATION(296);
				assert(isPVNode);
				alpha = currentMoveScore;
				if (ply == 1)
				{
					//UpdateRootMoveStats(currentMove.ui32, currentMoveScore, IterationPly);//NEW Replace the actual node count with huge pseudo node count based on the score

					RootAlphaUpdated = alpha;//NOT USED???
					ShowBestLineMessage(currentMoveScore, TTFlagExact);
					RootBestMove = currentMove;

				}
				//else if ((currentMoveScore > WinningBaseScore) && (RootAlpha < WinningBaseScore)) // If we just got a winning score at a PV node above the root and it didn't cause a cutoff assume it's the best move
				//{
				//	// The 'RootAlpha < WinningBaseScore' clause is to ensure that it can find shorter wins when a win has been established at the root DOESN'T WORK!!!!!
				//	bestMoveScore = currentMoveScore;
				//	break;
				//}
				//else if (currentMoveScore > MatingScore)
				//if (bestMoveScore > MatingScore)
				//{
				//	// The 'RootAlpha < WinningBaseScore' clause is to ensure that it can find shorter wins when a win has been established at the root DOESN'T WORK!!!!!
				//	bestMoveScore = currentMoveScore;
				//	break;//THIS SEEMS TO BE COUNTER-PRODUCTIVE. POSSIBLY AS IT FILLS THE TT WITH SUB-OPTIMAL VALUES? ACTUALLY IT PUTS INACCURATE 'EXACT' VALUES!
				//}

			}

			bestMoveScore = currentMoveScore;
		}

		if (quietMove)
			quietMovesSearched[quietMovesSearchedCount++] = currentMove.ui32;

		CRASHLOCATION(299);
	} // (Loop through move list)

	//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(310);

	// Do we have an EGTB score from earlier?
	if (egtbScore != -MatingIn0Score)
	{
		*currentGameRecordPointer->principalVariationPointer = PVTEGTB;
		return egtbScore;
	}

	// Update transposition table
	if (alpha == originalAlpha)
	{
		//if (!allowNull)
		//{
		//	AC3++;//TEMP
		//	if (!isCutNode)
		//		AC6++;
		//}
		// No move has returned a score > alpha, therefore this is an 'All' node (all legal moves have been searched)
		// The bestMoveScore is an upper bound (ceiling) on the exact score of the node (i.e. the exact score might be less than bestMoveScore, it is "at most" bestMoveScore)
		// The children of an All node are Cut nodes. The parent of an All node is a Cut node. The ply distance of an All node to its PV ancestor is even.
		//assert((bestMoveScore > -MateBaseScore) && (bestMoveScore <= alpha));
		PRINTTREE(PrintTree2(IterationPly, ply, "All"););
		AddToNormalTranspositionTable(depthRemaining, ply, bestMoveScore, TTFlagUpper + currentGameRecordPointer->isTWM + currentGameRecordPointer->isO1M + currentGameRecordPointer->isFMTP + currentGameRecordPointer->isO1PCM, tteBestMove.ui32, currentGameRecordPointer->staticEvaluation);// , tteFound); // Keep any existing TT move even though it didn't raise alpha
		//if (!isPVNode)
		//{
		//	AC4++;
		//	if (!isCutNode)//TEMP
		//		AC5++;
		//	else
		//		AC6++;
		//}
	}
	else
	{
		//if (!allowNull)
		//{
		//	AC1++;//TEMP
		//	if (isPVNode)
		//		AC4++;
		//}
		// A move has returned a score > (the original) alpha but < beta, therefore this is a 'PV' node (all legal moves have been searched)
		// The bestMoveScore is the EXACT score of the node
		// The root node and the leftmost nodes are always PV-nodes. All siblings of a PV node are expected Cut nodes.
		assert((originalAlpha < bestMoveScore) && (bestMoveScore == alpha) && (bestMoveScore < beta));
		assert(isPVNode);
		assert(*currentGameRecordPointer->principalVariationPointer != PVTUnknown);
		PRINTTREE(PrintTree2(IterationPly, ply, "Exact"););
		AddToNormalTranspositionTable(depthRemaining, ply, bestMoveScore, TTFlagExact + currentGameRecordPointer->isTWM + currentGameRecordPointer->isO1M + currentGameRecordPointer->isFMTP + currentGameRecordPointer->isO1PCM, *currentGameRecordPointer->principalVariationPointer, currentGameRecordPointer->staticEvaluation);// , tteFound);
	}

	//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(320);

	assert((bestMoveScore > -MatingIn0Score) && (bestMoveScore < MatingIn0Score));
	return bestMoveScore;
}

//----------------------------------------------------------------------------------------------------

std::string Normal::ComputeNormal()
{
	// At the start of these Compute* routines only the 64-square mailbox board and the game record are set up in the outer engine brain

	CRASHLOCATION(10);

	normalBrain.CopyFrom(&EngineBrain);

	// Set up the bit boards from the 64-square mailbox board
	ConvertMailboxBoard64ToPiecesBB(normalBrain.mailboxBoard64, normalBrain.piecesBB);

	// Initialise the PV array pointers in the GameRecord array
	for (int index = 0; index < normalBrain.gameRecordSize; index++)
		normalBrain.gameRecord[index].principalVariationPointer = nullptr;
	for (int index = 0; index < MaximumPly; index++)
	{
		if (normalBrain.GameRecordIndexRoot + index >= normalBrain.gameRecordSize)
			break;
		normalBrain.gameRecord[normalBrain.GameRecordIndexRoot + index].principalVariationPointer = &PrincipalVariation[(MaximumPly + 1) * index];
	}

	//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(20);

	// Get timer
	StartClock = std::chrono::steady_clock::now();
	MessagesLastDisplayedClock = StartClock;

	// Initialise any variables required for the search
	normalBrain.gameRecordPointer = &normalBrain.gameRecord[normalBrain.GameRecordIndexRoot];

	uint64_t totalNodes[MaximumPly];
	totalNodes[0] = 1;
	NodeCount = 0;
	NodeCountQuiescenceSearch = 0;
	RootCumulativeNodeCount = 0;
	MaximumPlyReached = 0;
	MaximumPlyReachedBeforeQS = 0;
	ConsistentBestMoves = 0;
	uint32_t previousBestMove = 0;
	ReplyImmediately = false;
	InitialiseMaterialValues(normalBrain.mailboxBoard64, normalBrain.gameRecordPointer);
	InitialisePSTValues(normalBrain.mailboxBoard64, normalBrain.gameRecordPointer);
	InitialiseGamePhase(normalBrain.mailboxBoard64, normalBrain.gameRecordPointer);
	uint64_t hash64 = GenerateTranspositionTableHash64(normalBrain.mailboxBoard64, normalBrain.gameRecordPointer);
	if (SideToMove == 1)
		hash64 = ~hash64;
	normalBrain.gameRecordPointer->transpositionTableHash64 = hash64;
	normalBrain.gameRecordPointer->transpositionTableHash64WithEP = hash64 ^ TranspositionTableRandomsEnPassant[normalBrain.gameRecordPointer->epSquare]; // N.B. TranspositionTableRandomsEnPassant[0] = 0
	normalBrain.gameRecordPointer->isZLKM = 0;

	lastPawnScoreWhite.bb = 0;
	lastPawnScoreWhite.pawnStructureOpeningScore = 0;
	lastPawnScoreWhite.pawnStructureEndgameScore = 0;
	lastPawnScoreBlack.bb = 0;
	lastPawnScoreBlack.pawnStructureOpeningScore = 0;
	lastPawnScoreBlack.pawnStructureEndgameScore = 0;
	(normalBrain.gameRecordPointer - 1)->staticEvaluation = Evaluate(SideToMove ^ 1);

	// Root move list stuff
	// Generated once here before the first iteration and the moves stay in the same physical order in which they are generated so that they correspond with the same moves in the tree generated move list
	// All moves in the RootMoveList have their 'priority' set to zero before the first iteration
	// During the first iteration we update every root move with its fail-soft score and subtree size
	// After the first iteration we use the fail-soft score to order the moves for the second iteration
	// For the second and subsequent iterations we increase the priority value of any move that takes over as best and use that for ordering on the next iteration
	// In every iteration at the root we score the root move list (for move ordering) with ScoreRootMoveList
	MoveWithScore_Struct moveList[220];
	RootMoveList[0].mws.ui32 = 0; //WHY???
	normalBrain.CalculatePinnedPieces(SideToMove); // Required for legal move generation
	RootMovesCount = normalBrain.GenerateAllMoves(SideToMove, normalBrain.IsEnemyKingAttacked(BitScanForwardX(normalBrain.piecesBB[SideToMove][King]), SideToMove ^ 1), moveList);
	if (RootMovesCount == 0) // Sometimes the GUI or the user provide positions with zero legal moves! (e.g. checkmates/stalemates in chess)
	{
		Output("info string *** Error! There are zero moves in the position provided!");
		return "";
	}
	for (int moveListIndexIterator = 0; moveListIndexIterator < RootMovesCount; moveListIndexIterator++)
	{
		RootMoveList[moveListIndexIterator].mws = moveList[moveListIndexIterator];
		RootMoveList[moveListIndexIterator].mws.score = 0;
		RootMoveList[moveListIndexIterator].nodes = 0;
		RootMoveList[moveListIndexIterator].priority = 0; 
		RootMoveList[moveListIndexIterator].EGTBWDL = -1;
		RootMoveList[moveListIndexIterator].EGTBDTZ = -1;
		RootMoveList[moveListIndexIterator].EGTBRank = -1;
		//RootMoveList[moveListIndexIterator].EGTBCandidate = false;
	}
	RootPriority = 1;

	// EGTB stuff
	EndgameTablebasesProbes = 0;
	EndgameTablebasesHeavyProbes = 0;
	EndgameTablebasesHits = 0;
	EndgameTablebasesPiecesRoot = PopulationCountX(normalBrain.piecesBB[0][AllPieces] | normalBrain.piecesBB[1][AllPieces]);
	EndgameTablebasesTreeProbeLimitMain = EndgameTablebasesPiecesFound;
	if (EndgameTablebasesTreeProbeLimitMain == 7)
		if (!SyzygyProbe7PieceInTree)
			EndgameTablebasesTreeProbeLimitMain = 6;
	EndgameTablebasesTreeProbeLimitMain = std::min(EndgameTablebasesTreeProbeLimitMain, SyzygyProbeLimit);
	EndgameTablebasesTreeProbeLimitQS = std::min(5, SyzygyProbeLimit);
	EndgameTablebasesTreeProbeLimitQS = std::min(EndgameTablebasesTreeProbeLimitQS, EndgameTablebasesPiecesFound);
	EndgameTablebasesRootMove = 0; // This will be set to a valid move if we are in the EGTBs at the root
	EndgameTablebasesRootWDL = -MAXINT;
	EndgameTablebasesRootDTZ = 0;
	EndgameTablebasesRootRank = -MAXINT;
	EndgameTablebasesErrors = false;
	for (int index = 0; index < 8; index++)
		EndgameTablebasesErrorCounts[index] = 0;
	if (ThreadId == 0) // Only the main thread will modify its root move list based on the EGTBs
		if (EndgameTablebasesPiecesRoot <= EndgameTablebasesPiecesFound) // Are we in the EGTBs at this root position?
			if (normalBrain.gameRecordPointer->castlingStatus.ui32 == 0x01010101) // Only probe the endgame tablebases if no castling possible (8/8/8/8/8/8/1Nr3P1/R3K1k1 b Q - 0 1 Rxb2? O-O-O #13)
			{
				// Only probe the lesser EGTBs once we're in them
				EndgameTablebasesTreeProbeLimitMain = std::min(EndgameTablebasesTreeProbeLimitMain, EndgameTablebasesPiecesRoot - 1);
				EndgameTablebasesTreeProbeLimitQS = std::min(EndgameTablebasesTreeProbeLimitQS, EndgameTablebasesPiecesRoot - 1);

				// We are in the EGTBs at the root so get the EGTB values for every move
				int result;
				TbRootMoves results;

				// Try to get accurate DTZ info
				result = tb_probe_root_dtz(
					normalBrain.piecesBB[0][AllPieces],
					normalBrain.piecesBB[1][AllPieces],
					normalBrain.piecesBB[0][King] | normalBrain.piecesBB[1][King],
					normalBrain.piecesBB[0][Queen] | normalBrain.piecesBB[1][Queen],
					normalBrain.piecesBB[0][Rook] | normalBrain.piecesBB[1][Rook],
					normalBrain.piecesBB[0][Bishop] | normalBrain.piecesBB[1][Bishop],
					normalBrain.piecesBB[0][Knight] | normalBrain.piecesBB[1][Knight],
					normalBrain.piecesBB[0][Pawn] | normalBrain.piecesBB[1][Pawn],
					0,
					normalBrain.gameRecordPointer->epSquare,
					(SideToMove == 0),
					false,
					&results
				);

				// If we couldn't get the DTZ info then try the WDL info (may happen e.g. if you have the 7-piece .rtbw files but not the .rtbz files)
				if (result == 0)
				{
					//EndgameTablebasesErrors = true;
					EndgameTablebasesErrorCounts[0]++;
					result = tb_probe_root_wdl(
						normalBrain.piecesBB[0][AllPieces],
						normalBrain.piecesBB[1][AllPieces],
						normalBrain.piecesBB[0][King] | normalBrain.piecesBB[1][King],
						normalBrain.piecesBB[0][Queen] | normalBrain.piecesBB[1][Queen],
						normalBrain.piecesBB[0][Rook] | normalBrain.piecesBB[1][Rook],
						normalBrain.piecesBB[0][Bishop] | normalBrain.piecesBB[1][Bishop],
						normalBrain.piecesBB[0][Knight] | normalBrain.piecesBB[1][Knight],
						normalBrain.piecesBB[0][Pawn] | normalBrain.piecesBB[1][Pawn],
						0,
						normalBrain.gameRecordPointer->epSquare,
						(SideToMove == 0),
						false,
						&results
					);
				}

				if (result != 0)
				{
					// Save the EGTB statuses of all root moves
					for (uint32_t index = 0; index < results.size; index++)
					{
						TbRootMove move = results.moves[index];
						uint32_t colossusMove = normalBrain.SYZYGYPYRRHICMoveToColossusMove(move.move, normalBrain.gameRecordPointer->epSquare);
						int wdl; // win=1, draw=0, loss=-1
						int dtz;
						if (move.tbRank > 0)
						{
							wdl = 1;
							dtz = 0x40000 - move.tbRank;
						}
						else if (move.tbRank == 0)
						{
							wdl = 0;
							dtz = 0;
						}
						else
						{
							wdl = -1;
							dtz = 0x40000 + move.tbRank;
						}
						bool found = UpdateRootMoveEGTBStatus(colossusMove, wdl, dtz, move.tbRank);
						if (!found)
							OutputError("Did not find EGTB move in RootMoveList! " + MyUI64TOA(colossusMove));

						// Save the move with the highest rank
						if (move.tbRank > EndgameTablebasesRootRank)
						{
							EndgameTablebasesRootMove = colossusMove;
							EndgameTablebasesRootWDL = wdl;
							EndgameTablebasesRootDTZ = dtz;
							EndgameTablebasesRootRank = move.tbRank;
						}
					}
				}
				else
				{
					//EndgameTablebasesErrors = true;
					EndgameTablebasesErrorCounts[0]++;
				}
			}

	//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(30);

	// Do the search
	RootScore = 0;
	RootBestMove.ui32 = 0;
	IterationPly = 0;
	int backedOffIterationPly = 0;
	PVExtended = false;
	UseTTAndPruning = 0;
	TMI1Extensions = 0;
	nullMoveMinimumPly = 0;
	//BestLineMessage = "";
	//IterationFinishMessage = "";
	PreviousIterationsMessages = "";
	CurrentIterationsMessages = "";
	LastTickCount = 1; // To avoid any divide by zero errors
	LastProgressMessageTickCount = 0;
	//pathDependentDraw = false;

	//int EndgameTablebasesTreeProbeLimitMainSaved = EndgameTablebasesTreeProbeLimitMain;
	//int EndgameTablebasesTreeProbeLimitQSSaved = EndgameTablebasesTreeProbeLimitQS;

	do
	{
		CRASHLOCATION(35);
		// Update iteration depth (ensuring it doesn't exceed maximum)
		if (IterationPly < MaximumIterationPly)
			IterationPly++;
		isFollowingPV = (IterationPly > 1);

		// If we've got a mate score at the root, gradually reduce the pruning/reductions to try to find shorter mates NOT USED - NEED TO TRY!
		if (RootScore >= MatingScore)
			RootPliesFullWidth += 2;
		else
			RootPliesFullWidth = 0;

		// Set the aspiration window
		if (IterationPly == 1)
		{
			RootAlpha = -MatingIn0Score;
			RootBeta = MatingIn0Score;
		}
		else
		{
			//RootAlpha = (short)std::max((RootScore - AspirationWindowDelta), -InfiniteBaseScore);
			//RootBeta = (short)std::min((RootScore + AspirationWindowDelta), InfiniteBaseScore);
			////if ((RootScore >= WinningBaseScore) || (EndgameTablebasesRootMove != 0))
			////{
			////	RootAlpha = -InfiniteBaseScore; // Opening the window to -inf/+inf seems to avoid many search instabilities when we have EGTB/mate wins
			////	RootBeta = InfiniteBaseScore;
			////}
			////else if (RootScore <= LosingBaseScore)
			////{
			////	RootAlpha = -InfiniteBaseScore;
			////	RootBeta = InfiniteBaseScore;
			////}

			////EndgameTablebasesTreeProbeLimitMain = EndgameTablebasesTreeProbeLimitMainSaved;
			////EndgameTablebasesTreeProbeLimitQS = EndgameTablebasesTreeProbeLimitQSSaved;
			////if (RootScore >= WinningBaseScore)
			////{
			////	EndgameTablebasesTreeProbeLimitMain--;
			////	EndgameTablebasesTreeProbeLimitQS--;
			////}

			RootAlpha = (short)(RootScore - AspirationWindowDelta);
			RootBeta = (short)(RootScore + AspirationWindowDelta);
			if (RootScore >= EGTBWinningScore)
			{
				RootAlpha = EGTBWinningScore;
				RootBeta = MatingIn0Score;
				if (RootScore >= MatingScore)
					RootAlpha = MatingScore;

			}
			else if (RootScore <= EGTBLosingScore)
			{
				RootAlpha = -MatingIn0Score;
				RootBeta = EGTBLosingScore;
				if (RootScore <= MatedScore)
					RootBeta = MatedScore;
			}

		}

		RootAlphaUpdated = RootAlpha;
		RootBetaOld = RootBeta;
		RootFailHighs = 0;
		RootFailLows = 0;
		LastPrintTreePly = 1;
#ifdef SEARCHINGFORLINE
		//TargetLineLength = 1;
		TargetLinePartial = "";
#endif

		//----------------------------------------------------------------------------------------------------

		ShowIterationStartMessage();

	retry:
		CRASHLOCATION(40);
		//PVMessageChecked = false;

		// Do the search
		// N.B. RootScore is relative to the side-to-move so e.g. if black is moving and mating this will a large +ve score
		RootScore = TreeSearchNormal(RootAlpha, RootBeta, 1, IterationPly, SideToMove, normalBrain.IsEnemyKingAttacked(BitScanForwardX(normalBrain.piecesBB[SideToMove][King]), SideToMove ^ 1), false, false);

		CRASHLOCATION(50);

		if (!StopImmediately)
		{

			//if (IterationPly == 1)//TESTING taking this out
			//	UpdateRootMovePriority(RootBestMove.ui32); // Ensure the best move is flagged as such in the root move list ON THE FIRST ITERATION!




			////SANITY CHECK
			////AFTER A FAIL HIGH A RESEARCH MAY NOT RETURN THE SAME SCORES BUT THE ROOTMOVELIST WILL PARTIALLY REFLECT THE PREVIOUS SEARCH SCORES
			//uint64_t highestNodes = 0;
			//int highestIndex = 0;
			//for (int index2 = 0; index2 < RootMovesCount; index2++)
			//{
			//	//if (RootMoveList[index2].iterationPlyBecameBest > -1)
			//	{
			//		//uint64_t pseudoNodes = RootMoveList[index2].nodes + 1;
			//		//if (RootMoveList[index2].iterationPlyBecameBest > 0)
			//		//{
			//		//	uint64_t scoreAdjusted = (uint64_t)(RootMoveList[index2].mws.score + MateBaseScore); // +ve
			//		//	pseudoNodes += ((uint64_t)RootMoveList[index2].iterationPlyBecameBest * 1000000000000000ULL) + (scoreAdjusted * 1000000000ULL); // 1 quadrillion, 1 billion
			//		//}
			//		uint64_t pseudoNodes = (RootMoveList[index2].priority * 1000000000000000ULL);
			//		uint64_t scoreAdjusted = (uint64_t)(RootMoveList[index2].mws.score + MateBaseScore); // +ve
			//		pseudoNodes += scoreAdjusted;
			//		//pseudoNodes += RootMoveList[index2].nodes+1;

			//		if (pseudoNodes > highestNodes)
			//		{
			//			highestNodes = pseudoNodes;
			//			highestIndex = index2;
			//		}
			//	}
			//}
			//if (RootBestMove.ui32 != RootMoveList[highestIndex].mws.ui32)
			//{
			//	std::string s = "";
			//	for (int index2 = 0; index2 < RootMovesCount; index2++)
			//	{
			//		s += MoveNotation(RootMoveList[index2].mws.ui32) + " " + MyITOA(RootMoveList[index2].iterationPlyBecameBest) + " " + MyITOA(RootMoveList[index2].mws.score) + " " + MyUI64TOA(RootMoveList[index2].nodes) + " " + MyUI64TOA(RootMoveList[index2].priority) + "\n";
			//	}
			//	OutputError("*** ComputeNormal: NOT found PREVIOUS BEST ROOT MOVE 1ST! " + MoveNotation(RootBestMove.ui32) + " " + MoveNotation(RootMoveList[highestIndex].mws.ui32) + " " + MyITOA(IterationPly) + " " + MyITOA(RootMoveList[highestIndex].iterationPlyBecameBest) + " " + MyITOA(RootMoveList[highestIndex].mws.score) + " " + MyUI64TOA(RootMoveList[highestIndex].nodes) + " " + MyITOA(RootAlpha) + "/" + MyITOA(RootBeta) + "/" + MyITOA(RootScore) + ":" + MyITOA(RootFailHighs) + ":" + MyITOA(RootFailLows) + "\n" + s + AllBestLines);
			//}




			if (RootScore >= RootBeta) // Failed high? i.e. a root move returned a score >= beta (N.B. rootScore can actually be > beta because of fail-soft)
			{
				//if (RootFailLows > 0)
				//	OutputError("Fail-low-hi detected!");

				RootFailHighs++;
				RootBetaOld = RootBeta;
				if (RootScore >= EGTBWinningScore)
					RootBeta = MatingIn0Score;
				else
				{
					if (RootFailHighs == 1)
						RootBeta = RootScore + 150;
					else if (RootFailHighs == 2)
						RootBeta = RootScore + 950;
					else
						RootBeta = MatingIn0Score;
				}
				//RootAlpha = (short)(-MateBaseScore);//TESTING!!!
				//RootAlpha = RootAlphaUpdated; (+1.8, +/-3.6, 20000) for taking this out!
				//if (IterationPly > backedOffIterationPly)
				//{
				//	backedOffIterationPly = IterationPly;
				//	IterationPly = 2;
				//}


				//// AFTER A FAIL HIGH, WHEN WE RESEARCH, THE MOVE THAT CAUSED THE FAIL HIGH MAY NOW {WITH THE WIDER WINDOW} RETURN A LOWER SCORE AND THUS NOT BE THE BEST MOVE
				//// *BUT* ITS SCORE ON THE ROOTMOVELIST *MAY* STILL BE HIGHER THAN THE ACTUAL BEST MOVE THUS CAUSING ORDERING CONFUSION AT THE NEXT ITER
				//// THEREFORE WE MUST CLEAR THE SCORES OF ALL MOVES THAT TOOK OVER AS BEST THIS ITER BEFORE WE RESEARCH
				//// WE COULD REALLY USE A 7.1,7.2,7.3 ETC
				//for (int index2 = 0; index2 < RootMovesCount; index2++)
				//{
				//	if (RootMoveList[index2].iterationPlyBecameBest == IterationPly)
				//		RootMoveList[index2].mws.score = -MateBaseScore;
				//}



				//RootAlpha = -MateBaseScore;//TEST
				//RootBeta = MateBaseScore;

				goto retry;
			}
			else if (RootScore <= RootAlpha) // Failed low? i.e. no root move took over as the new best
			{
				//if (RootFailHighs > 0)
				//	OutputError("Fail-hi-low detected!");

				ShowFailedLowMessage(RootAlpha);
				RootFailLows++; // DEBUGGING INFO ONLY
				//if (RootFailLows == 1)
				//	RootAlpha = (short)std::max(RootScore - 150, -MateBaseScore);
				//else
				RootAlpha = (short)(-MatingIn0Score);

				//RootAlpha = -MateBaseScore;//TEST
				//RootBeta = MateBaseScore;

				goto retry;
			}
		}

		//----------------------------------------------------------------------------------------------------
		CRASHLOCATION(60);

		ShowIterationFinishMessage(HashfullNormalTranspositionTable());

		// Save this iteration's principal variation to ensure that it gets searched first on the next iteration
		for (int index = 0; index < MaximumPly; index++)
			LastPrincipalVariation[index] = PrincipalVariation[index];

		CRASHLOCATION(61);

		if (ThreadId == 0)
		{
			if (RootBestMove.ui32 == previousBestMove)
				ConsistentBestMoves++;
			else
				ConsistentBestMoves = 0;
			previousBestMove = RootBestMove.ui32;

			// Show diagnostics
			CRASHLOCATION(62);
			if (IsDebug)
				if (ThreadId == 0)//***NOT NEEDED
				{
					//ShowQueuedMessages();
					totalNodes[IterationPly] = NodeCount + NodeCountQuiescenceSearch;
					Output("info string Nodes: Main/QS/%inQS " + MyUI64TOA(NodeCount) + " / " + MyUI64TOA(NodeCountQuiescenceSearch) + " / " + MyUI64TOA((NodeCountQuiescenceSearch * 100) / (NodeCount + NodeCountQuiescenceSearch)));
					Output("info string Branching factor: " + MyFTOA((float)totalNodes[IterationPly] / totalNodes[IterationPly - 1]));
					Output("info string Longest line before QS: " + LongestLineWithoutQS);
					Output("info string Longest line: " + LongestLineWithQS);
					DisplayStatisticsNormalTranspositionTable();
					Output("");
				}

			// Should we stop the search? (Sets various flags internally which are tested elsewhere)
			TimeUp(2.0f);

#ifdef SEARCHINGFORLINE
			ShowQueuedMessages();
			if (TargetLinePartial != "")
				Output("IterationPly: " + MyITOA(IterationPly) + ", TargetLinePartial: " + TargetLinePartial + ", TargetLineRefutedBy: " + MoveNotation(TargetLineRefutedBy.ui32) + " (" + MyITOA(TargetLinePartialDepthRemaining) + ", " + MyITOA(TargetLinePartialThreateningMate) + ")\n");
#endif


			//CHECK NEW ROOT MOVELIST ORDER HERE


			CRASHLOCATION(64);
		}

	} while ((!StopWhenIterationComplete && (ThreadId == 0)) || (!StopImmediately && (ThreadId > 0)));

	std::string bestMoveMessage = "";

	if (ThreadId == 0)
	{
		//if (BestLineMessage != "")
		//	Output(BestLineMessage);
		//if (IterationFinishMessage != "")
		//	Output(IterationFinishMessage);
		if (CurrentIterationsMessages != "")
		{
			Output(PreviousIterationsMessages);
			Output(CurrentIterationsMessages);
		}

		StopImmediately = true; // As soon as the main thread finishes flag any helper threads to terminate

		if (EndgameTablebasesErrors)
		{
			std::string s = "Endgame tablebase errors occurred...\n";
			for (int count = 0; count < 8; count++)
				s += " " + MyITOA(EndgameTablebasesErrorCounts[count]);
			OutputError(s + "\n");
		}

		CRASHLOCATION(70);

		//ShowQueuedMessages();

		// Report best move
		if (RootBestMove.ui32 == 0)
			OutputError("No best move returned by search! (RootBestMove.ui32 == 0)");
		bestMoveMessage = "bestmove " + MoveNotation(RootBestMove.ui32);
		if (Ponder)
			if ((TC.CurrentType != TCTFixedTime) && (TC.CurrentType != TCTFixedDepth) && (TC.CurrentType != TCTFixedNodes)) // Don't ponder in any 'fixed' modes
				if ((uint16_t)normalBrain.gameRecordPointer->principalVariationPointer[1] > 0) // May be any of the PVR* terminators (which all have the bottom 16 bits set to 0)
					if (RootScore > -MatingIn0Score + 3) // Don't ponder if we're being mated in 1 else the GUI might give us the mated position and tell us to search it!
						bestMoveMessage += " ponder " + MoveNotation(normalBrain.gameRecordPointer->principalVariationPointer[1]);
		if (BlankLines)
			bestMoveMessage += "\n";

		// Save any output from -FILE command for analysis in spreadsheet
		if (ProcessingCommandFile)
		{
			FILE *sw;
			fopen_s(&sw, "output.csv", "a+");
			std::string s = BestLine() + MyITOA(RootScore) + "," + MyUI64TOA(NodeCount + NodeCountQuiescenceSearch) + "," + MyUI64TOA(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - StartClock).count());
			fprintf(sw, "%s\n", s.c_str());
			fclose(sw);
		}
	}

	CRASHLOCATION(80);

	return bestMoveMessage;
}

//----------------------------------------------------------------------------------------------------

void Normal::ComputeNormalMTLaunchHelperThread(int threadId)
{
	Normal* ts;
	ts = new Normal;
	ts->ThreadId = threadId;
	// Initialise killers etc from the persisted Normal class
	memcpy(ts->KillerMoves, EngineNormal.KillerMoves, sizeof(EngineNormal.KillerMoves));
	memcpy(ts->CounterMoves, EngineNormal.CounterMoves, sizeof(EngineNormal.CounterMoves));
	memcpy(ts->FollowUpMoves, EngineNormal.FollowUpMoves, sizeof(EngineNormal.FollowUpMoves));
	memcpy(&ts->CounterMoveHistory->CMH[0][0], &EngineNormal.CounterMoveHistory->CMH[0][0], sizeof(CounterMoveHistory_Struct));
	ts->ComputeNormal();
	delete ts;
}

void Normal::ComputeNormalMT()
{
	CRASHLOCATION(0);

	// If we've got some unused transposition table memory then allocate it (and clear it)
	if ((TranspositionTableMemory > 0) && (NormalTranspositionTableBuckets == 0))
		Normal::AllocateNormalTranspositionTable();

	// Advance the TT age
	TranspositionTableAge++;
	TranspositionTableAge &= TTFlagAgeMask;
	assert((TranspositionTableAge >= 0) && (TranspositionTableAge <= 3));

	StopImmediately = false;
	StopWhenIterationComplete = false;

	ClearAnalysisCounters();

	// Launch any helper threads independently
	// (If the number of helper threads > physical CPU threads this can take a discernible time)
	//uint32_t hardwareThreadsMax = std::thread::hardware_concurrency();
	std::thread threads[ThreadsMax];
	for (int threadId = 1; threadId < Threads; threadId++)
	{
		threads[threadId] = std::thread(ComputeNormalMTLaunchHelperThread, threadId);
		//SetThreadIdealProcessor(threads[threadId].native_handle(), threadId % hardwareThreadsMax);
	}

	// Compute the result in this main thread which uses the Normal class instance declared in Engine
	EngineNormal.ThreadId = 0;
	//ThreadResults[0] = EngineNormal.ComputeNormal();
	std::string bestMoveMessage = EngineNormal.ComputeNormal();

	// Wait for helper threads to finish
	for (int threadId = 1; threadId < Threads; threadId++)
		threads[threadId].join();

	// Display best move found
	//Output(ThreadResults[0]);
	Output(bestMoveMessage);

	DisplayAnalysisCounters();

	CRASHLOCATION(99);
}

uint32_t Normal::ComputeNormalWrapperInner()
{
	uint32_t result = 0;

	// Error handling in C++ is woeful e.g. a null pointer dereference doesn't raise a C++ exception
	// It does however raise a 'structured exception' through the OS
	// To enable these to be passed into the code we have to change the project property 'Configuration Properties/C/C++/Code Generation/Enable C++ Exceptions' to 'Yes with SEH Exceptions (/EHa)'
	__try
	{
		ComputeNormalMT();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		result = GetExceptionCode();
	}

	ComputingMove = false;

	return result;
}

void Normal::ComputeNormalWrapper()
{
	uint32_t result = ComputeNormalWrapperInner();
	if (result != 0)
	{
		OutputError("Exception occurred! " + MyUI64TOA(result) + " : CrashLocation=" + MyITOA(CrashLocation));
		exit(0);
	}
}
