#include "httplib.h" // See https://github.com/yhirose/cpp-httplib
#include "RSJparser.tcc" // See https://github.com/subh83/RSJp-cpp

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

//#include "NNUE\NNUE.h"
#include "GlobalConstants.h"
#include "GlobalTypes.h"
#include "Engine.h"
#include "UGI.h"
//#include "Brain.h"
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

httplib::Client LichessEGTBClient("http://tablebase.lichess.org");
bool LichessEGTBProbing = false;
bool LichessEGTBThrottling = false;
std::chrono::time_point<std::chrono::steady_clock> LichessEGTBThrottlingStartClock;
int LichessEGTBMax = 8;
std::string LichessQueryFEN = "";
std::string RootFEN = "";
struct LichessMove
{
	std::string move;
	int rank;
};
LichessMove LichessMoves[256];

int Normal::CrashLocation;

//----------------------------------------------------------------------------------------------------

Normal::Normal()
{
	// Declaring CounterMoveHistory as a class variable loses ELO and crashes occasionally but I don't know why!
	// Creating it on the heap here seems to work fine though.
	CounterMoveHistory = new CounterMoveHistory_Struct;

	//if (!nnue.load("nnue_weights.bin"))
	//{
	//	Output("Could not load NNUE!");
	//}
	//else
	//	Output("Loaded NNUE!");
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
	ConvertMailboxBoard64ToPiecesBB(normalBrain.MailboxBoard64, normalBrain.piecesBB);

	MoveWithScore_Struct moveList[220];
	normalBrain.CalculatePinnedPieces(SideToMove); // Required for legal move generation
	int movesCount = normalBrain.GenerateAllMoves(SideToMove, normalBrain.IsEnemyKingAttacked(GetLS1BIndex(normalBrain.piecesBB[SideToMove][King]), SideToMove ^ 1), moveList);

	for (int moveListIndexIterator = 0; moveListIndexIterator < movesCount; moveListIndexIterator++)
	{
		int SEEResult;
		SEEResult = normalBrain.SEE(moveList[moveListIndexIterator].mf.fromSquare, moveList[moveListIndexIterator].mf.toSquare, SideToMove);
		Output("info string " + Notation64[moveList[moveListIndexIterator].mf.fromSquare] + Notation64[moveList[moveListIndexIterator].mf.toSquare] + "=" + MyITOA(SEEResult));
	}
}

#pragma endregion

//----------------------------------------------------------------------------------------------------

void Normal::LichessEGTBProbe(std::string fen)
{
	// N.B. you can pass any FEN you like to Lichess (e.g. the original position!) and if it doesn't have an EGTB for that position it still returns a Json document with all the moves but just nulls for all the data

	// If we are being throttled, wait at least 61 seconds before issuing another request
	if (LichessEGTBThrottling)
	{
		if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - LichessEGTBThrottlingStartClock).count() < 61000)
			goto exit;
		//Output("LichessEGTBThrottling finish!");
		LichessEGTBThrottling = false;
	}

	try
	{
		//Output("Entered LichessEGTBProbe");


		// Probe the Lichess EGTBs
		auto httpResult = LichessEGTBClient.Get("/standard?fen=" + fen);

		// Did we get anything back?
		if (httpResult)
		{
			if (httpResult->status == 429) // The server is throttling us!
			{
				LichessQueriesThrottled++;
				LichessEGTBThrottling = true;
				LichessEGTBThrottlingStartClock = std::chrono::steady_clock::now();
				//Output("LichessEGTBThrottling start!");
			}
			else if (httpResult->status == 200) // Status OK
			{
				// Get the result string and convert it into a Json object
				RSJresource jsonResults(httpResult->body);

				// Get the 'moves' array from within the outer Json objectrank
				RSJresource moves = jsonResults["moves"];

				// Iterate over the moves array and copy the required data into the Normal engine thread's variables
				// N.B. moves.size() will be 0 if the query failed in any way
				int moveCount;
				for (moveCount = 0; moveCount < moves.size(); moveCount++)
				{
					//Output(moves[i]["uci"].as<std::string>() + " : " + moves[i]["dtz"].as<std::string>() + " : " + moves[i]["dtc"].as<std::string>());
					LichessMoves[moveCount].move = moves[moveCount]["uci"].as<std::string>();
					int rank;
					if (moves[moveCount]["dtz"].as<std::string>() != "null")
						rank = atoi(moves[moveCount]["dtz"].as<std::string>().c_str());
					else
						rank = atoi(moves[moveCount]["dtc"].as<std::string>().c_str());
					LichessMoves[moveCount].rank = rank;
				}
				LichessMoves[moveCount].move = "";
			}
		}
		//Output("Exited LichessEGTBProbe");
	}
	catch (...)
	{
	}

exit:
	LichessQueriesReturned++;
	LichessEGTBProbing = false;
}

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

	memset(CounterMoveHistory, 0, sizeof(CounterMoveHistory_Struct));
}

//----------------------------------------------------------------------------------------------------

#pragma region Message processing

std::string Normal::ThreadIdSuffix()
{
	return (Threads == 1) ? "" : " ThreadId " + std::to_string(ThreadId);
}

void Normal::ShowIterationStartMessage()
{
	if (ThreadId == 0)
	{
		std::string iterationStartMessage = "info depth " + std::to_string(IterationPly);
		if (IsDebug)
			iterationStartMessage += ThreadIdSuffix();

#ifndef _DEBUG
		if (LastTickCount > MessageDelayTickCount)
#endif
			Output(iterationStartMessage);
#ifndef _DEBUG
		else
		{
			PreviousIterationsMessages = CurrentIterationsMessages;
			CurrentIterationsMessages = iterationStartMessage;
		}
#endif
	}
}

void Normal::ShowProgressMessage(uint32_t move, int movesMade, short bestMoveScore, short alpha, short beta)
{
	if (ThreadId == 0)
	{
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

				std::string progressMessage = "info time " + std::to_string(LastTickCount)
					+ " nodes " + std::to_string(NodeCount + NodeCountQuiescenceSearch)
					+ " currmove " + MoveNotation(move)
					+ " currmovenumber " + std::to_string(movesMade)
					;
				if (IsDebug)
				{
					progressMessage += " depth " + std::to_string(IterationPly) + " bestMoveScore " + std::to_string(bestMoveScore) + " alpha " + std::to_string(alpha) + " beta " + std::to_string(beta);
					progressMessage += ThreadIdSuffix();
					//ProgressMessage += " processor " + std::to_string(GetCurrentProcessorNumber());
				}
				Output(progressMessage);
			}
	}
}

void Normal::ShowFailedLowMessage(short rootAlpha)
{
	if (ThreadId == 0)
	{
		std::string failedLowMessage = "info depth " + std::to_string(IterationPly) + " score cp " + std::to_string(rootAlpha) + " upperbound";

#ifndef _DEBUG
		if (LastTickCount > MessageDelayTickCount)
#endif
			Output(failedLowMessage);
#ifndef _DEBUG
		else
			CurrentIterationsMessages += "\n" + failedLowMessage;
#endif
	}
}

void Normal::ShowIterationFinishMessage(uint32_t hashfull)
{
	if (ThreadId == 0)
	{
		uint64_t totalNodes = NodeCount + NodeCountQuiescenceSearch;
		std::string iterationFinishMessage = "info depth " + std::to_string(IterationPly)
			+ " time " + std::to_string(LastTickCount)
			+ " nodes " + std::to_string(totalNodes)
			+ " nps " + std::to_string((totalNodes * 1000) / LastTickCount)
			+ " hashfull " + std::to_string(hashfull)
			+ (EndgameTablebasesHits > 0 ? " tbhits " + std::to_string(EndgameTablebasesHits) : "");
		if (IsDebug)
		{
			iterationFinishMessage += " seldepth " + std::to_string(MaximumPlyReached);
			iterationFinishMessage += ThreadIdSuffix();
		}
		if (ShowBlankLines)
			iterationFinishMessage += "\n";
#ifndef _DEBUG
		if (LastTickCount > MessageDelayTickCount)
#endif
			Output(iterationFinishMessage);
#ifndef _DEBUG
		else
			CurrentIterationsMessages += "\n" + iterationFinishMessage;
#endif
	}
}

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
	if (ThreadId == 0)
	{
		// Construct the PV
		std::string pvMessage = "";
		int i = 0;
		do
		{
			pvMessage += MoveNotation(PrincipalVariation[i++]) + " ";
		} while ((uint16_t)PrincipalVariation[i] != 0);

		// Construct any 'end of PV' suffix
		std::string pvTerminatorMessage = "";
		if (ShowPVTerminators)
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
				pvTerminatorMessage = "*TTExact";
				break;
			default:
				pvTerminatorMessage = "*Unknown PV terminator found! " + std::to_string(PrincipalVariation[i]);
				break;
			}
		}

		// Final bits and bobs
		uint64_t totalNodes = NodeCount + NodeCountQuiescenceSearch;

		std::string scoreMessage;
		if (alpha >= MatingScore) // Mating?
			scoreMessage = "mate " + std::to_string((MatingIn0Score - alpha) >> 1);
		else if (alpha <= MatedScore) // Mated?
			scoreMessage = "mate " + std::to_string((-MatingIn0Score - alpha + 1) >> 1);
		else // Normal
		{
			// If we are in the EGTB at the root, adjust the displayed score appropriately
			short displayedScore = alpha;
			if (EndgameTablebasesRootMove.ui32 != 0)
			{
				if (EndgameTablebasesRootWDL == 0) // Draw?
				{
					// Reduce the range of displayed scores to avoid UIs adjudicating the game as a loss! See https://talkchess.com/viewtopic.php?t=84821
					//displayedScore = displayedScore / 8; // THIS CAUSED CONFUSION WHEN TRYING TO FIND A BUG AROUND SMALL DRAW SCORES!
					// Reduce the part of the score above +/-100
					if (std::abs(displayedScore) >= 100)
						displayedScore = (((std::abs(displayedScore) - 100) / 8) + 100) * ((displayedScore > 0) - (displayedScore < 0));
				}
				else if (EndgameTablebasesRootWDL == 1) // Win?
					if (displayedScore < EGTBWinningScore)
						displayedScore = displayedScore + 1000; // Ensure the score looks like a winning score! (It may be just a few centi-pawns according to the eval)
			}

			scoreMessage = "cp " + std::to_string(displayedScore);
		}

		// N.B. eul is only ever provided as TTFlagExact or TTFlagLower. Fail lows are handled by ShowFailedLowMessage
		std::string eulMessage = "";
		if (eul != TTFlagExact)
		{
			//if (eul == TTFlagLower)
			eulMessage = " lowerbound";
			//else if (eul == TTFlagUpper)
			//	eulMessage = " upperbound";
		}

		// Display the constructed message
		std::string BestLineMessage = "info depth " + std::to_string(IterationPly) // N.B. the 'depth' value is provided here (as well as in the iteration 'start' message) as some GUIs (e.g. Arena, Shredder) don't display it unless it's provided with the PV!
			+ " time " + std::to_string(LastTickCount)
			+ " nodes " + std::to_string(totalNodes)
			+ " score " + scoreMessage + eulMessage
			+ " pv " + pvMessage + pvTerminatorMessage;
		if (IsDebug)
			BestLineMessage += ThreadIdSuffix();

#ifndef _DEBUG
		if (LastTickCount > MessageDelayTickCount)
#endif
			Output(BestLineMessage);
#ifndef _DEBUG
		else
			CurrentIterationsMessages += "\n" + BestLineMessage;
#endif
	}
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
bool Normal::UpdateRootMoveEGTBStatus(uint32_t move, int wdl, int dtz)// , int rank)
{
	bool found = false;
	// Find the current move in the root move list
	for (int index = 0; index < RootMovesCount; index++)
		if (RootMoveList[index].mws.ui32 == move)
		{
			RootMoveList[index].EGTBWDL = wdl;
			RootMoveList[index].EGTBDTZ = dtz;
			//RootMoveList[index].EGTBRank = rank;
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
		int highestIndex = 219;
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
		{
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
		// N.B. MovesToGo is provided by the GUI and is either zero ('all the moves') or the actual # of moves to make before the time control ('repeating')
		const int movesLeftBaseEstimate = 8;// 9;//11;
		movesLeft = movesLeftBaseEstimate;
		if (MovesToGo == 0) // 'All the moves'?
		{
			movesLeft += 1;
			if (WInc == 0) // If 'all the moves' AND no Fischer bonus then need to be VERY careful. It is assumed WInc and BInc will be the same!
			{
				movesLeft += 5;
				// Without increment: movesLeft=14 (8+1+5) : so at the start of a 10 min game the target would be 42s (600/14)
			}
			else
			{
				// If we have an increment, add it to the timeleft in advance for every move
				// If it is huge then we will probably hit the panic timeup but then get given the increment for the next move
				// If it is small then it will just encourage slightly longer think times
				timeLeft += movesLeft * WInc;
				// With increment: movesLeft=9 (8+1) : so at the start of a 10 min game with 1s increment the target would be 67s (609/9)
				// With increment: movesLeft=9 (8+1) : so at the start of a 10 min game with 1m increment the target would be 126s (1140/9)
			}
		}
		else // Repeating
		{
			if (MovesToGo < movesLeft) // Fewer than 8 moves left?
				movesLeft = MovesToGo + 1; // (+10.8, +/ -3.6, 18701)
			else
				movesLeft += std::min(MovesToGo / 16, 2); // Add at most 2 extra moves near the start of each control (so it doesn't use too much time there) (+4.0, +/-3.5, 20000)

			if (WInc == 0) // If no Fischer bonus then need to be more careful
				movesLeft += 2;
			else
			{
				// If we have an increment, add it to the timeleft in advance for every move
				// If it is huge then we will probably hit the panic timeup but then get given the increment for the next move
				// If it is small then it will just encourage slightly longer think times
				timeLeft += movesLeft * WInc;
			}
		}

		// Have we used enough time yet? (or only one move or done maximum ply search)
		if ((LastTickCount >= ((float)timeLeft / (float)movesLeft / divisor)) || (RootMovesCount == 1) || (IterationPly >= MaximumIterationPly))
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

short Normal::DrawScore(int sideToMove)
{
	// Use a function to provide the draw score (rather than a simple variable) because there are many tweaks possible!
	// Like the evaluation function, it returns a score relative to the side to move

	short ds = Contempt;
	if (SideToMove == sideToMove) // The contempt value is relative to the side to move at the root i.e. the computer!
		ds = -ds;

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
	NormalTranspositionTableBuckets = (TranspositionTableMemory * 1024ULL * 1024ULL) / sizeof(NormalTranspositionTableBucket_Struct);
	NormalTranspositionTableBuckets = 1ULL << GetMS1BIndex(NormalTranspositionTableBuckets);

	// N.B. Increasing the transposition table size may be counter-productive beyond some margin.
	// Once the table is not being completely filled after the search you are just storing the same info spread over more memory.
	// Some testing indicates that once you get more than about 50% of the table not being used you will suffer a slow down.

	// Free any previously allocated memory. If the pointer is nullptr it does nothing.
	AlignedFreeMemory(NormalTranspositionTablePointer);
	NormalTranspositionTablePointer = nullptr;

	// Allocate transposition table memory
	if (NormalTranspositionTableBuckets > 0)
	{
		NormalTranspositionTableBucketsMask = NormalTranspositionTableBuckets - 1;
		if (LargePagesAvailable && UseLargePages)
		{
			MemoryAlocatedViaLargePages = true;
			NormalTranspositionTablePointer = (NormalTranspositionTableBucket_Struct*)VirtualAlloc(NULL, NormalTranspositionTableBuckets * sizeof(NormalTranspositionTableBucket_Struct), MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
			if (NormalTranspositionTablePointer == nullptr)
			{
				uint32_t errorCode = GetLastError();
				Output("info string *** Error! Normal 'large pages' transposition table memory could not be allocated! (Code:" + std::to_string(errorCode) + ") Falling back to standard pages.");
				OutputError("Normal 'large pages' transposition table memory could not be allocated! Falling back to standard pages.");
			}
		}
		if (NormalTranspositionTablePointer == nullptr)
		{
			MemoryAlocatedViaLargePages = false;
			NormalTranspositionTablePointer = (NormalTranspositionTableBucket_Struct*)AlignedAllocateMemory(NormalTranspositionTableBuckets * sizeof(NormalTranspositionTableBucket_Struct), 64);
		}
		if ((NormalTranspositionTablePointer == nullptr))
		{
			uint32_t errorCode = GetLastError();
			Output("info string *** Error! Normal transposition table memory could not be allocated! (Code:" + std::to_string(errorCode) + ")");
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

	Output("info string TT Statistics: Total = " + std::to_string(total) + ", Unused = " + std::to_string(unused) + "(" + MyFTOA((unused * 100) / (float)total) + "%)" + ", Exact = " + std::to_string(exact) + "(" + MyFTOA((exact * 100) / (float)total) + "%)" + ", Lower = " + std::to_string(lower) + "(" + MyFTOA((lower * 100) / (float)total) + "%)" + ", Upper = " + std::to_string(upper) + "(" + MyFTOA((upper * 100) / (float)total) + "%)");
}

void Normal::AddToNormalTranspositionTable(int8_t depthRemaining, short ply, short score, uint8_t flag, uint32_t bestMove, short tteStaticEvaluation, int tteFound)
{
	// tteFound is an attempt to speed up the selection of which entry within the bucket to replace
	// It may be set when we probe this node soon after entry
	// N.B.
	// If tteFound==-1: (i.e. we didn't find this position in the TT when we entered this node) a reduced search might have added an entry for it anyway!
	// If tteFound>0: if the STD of the found entry is small (it may be less than the current depthRemaining) it might have been replaced!
	// Blindly reusing any tteFound entry (i.e. assuming it's still for the current position) MAY lead to duplicate entries for the current position. However the constant replacement will overwrite these anyway and it actually seems beneficial to speed up the replacement rather than worry about occasional duplicates.

	if (NormalTranspositionTableBuckets > 0)
	{
		GATHERSTATS(TranspositionTableStores++;);

		NormalTranspositionTableEntry_Struct* tte0;
		uint64_t hash64 = normalBrain.gameRecordPointer->transpositionTableHash64WithEP;
		tte0 = (NormalTranspositionTableEntry_Struct*)(NormalTranspositionTablePointer + (hash64 & NormalTranspositionTableBucketsMask));


		//if (hash64 == 6547430464751700826)
		//	AC1++;//TEMP




		// Find candidate entry for replacement
		int entryToReplace, shallowestSubTreeDepth;
		uint64_t tteHash, tteData;

		if (tteFound != -1)
		{
			shallowestSubTreeDepth = -128;
			entryToReplace = tteFound;
		}
		else
		{
			shallowestSubTreeDepth = -128;
			entryToReplace = 0;
			tteHash = tte0[0].hash64;
			tteData = tte0[0].data;

			if ((tteHash ^ tteData) != hash64) // Is the 0th entry not an exact match?
			{
				uint8_t oldestTranspositionTableAge = (TranspositionTableAge + 1) & TTFlagAgeMask;
				uint8_t tteAge = ((NormalTranspositionTableEntryDataFields_Struct*)&tteData)->flag & TTFlagAgeMask;
				if (tteAge != oldestTranspositionTableAge) // Has the 0th entry aged the maximum # of times? i.e. a really old entry
				{
					// Then use the 0th entry's STD
					int8_t tteSubTreeDepth = ((NormalTranspositionTableEntryDataFields_Struct*)&tteData)->subTreeDepth;
					shallowestSubTreeDepth = tteSubTreeDepth;
				}

				// Scan the entries in the bucket for the best entry to replace
				for (int entry = 1; entry < NormalTranspositionTableEntriesPerBucket; entry++)
				{
					tteHash = tte0[entry].hash64;
					tteData = tte0[entry].data;

					if ((tteHash ^ tteData) == hash64) // Exact match?
					{
						// If so, re-use it immediately
						shallowestSubTreeDepth = -128;
						entryToReplace = entry;
						break;
					}

					if (shallowestSubTreeDepth != -128) // If we have found an 'oldest' entry then don't bother checking for another one or for an entry with a lower STD but do keep scanning for a position match
					{
						uint8_t tteAge = ((NormalTranspositionTableEntryDataFields_Struct*)&tteData)->flag & TTFlagAgeMask;
						if (tteAge == oldestTranspositionTableAge) // Has the entry aged the maximum # of times? i.e. a really old entry
						{
							shallowestSubTreeDepth = -128;
							entryToReplace = entry;
							continue;
						}

						int8_t tteSubTreeDepth = ((NormalTranspositionTableEntryDataFields_Struct*)&tteData)->subTreeDepth;
						if (tteSubTreeDepth < shallowestSubTreeDepth)
						{
							shallowestSubTreeDepth = tteSubTreeDepth;
							entryToReplace = entry;
						}
					}
				}
			}
		}

		assert(entryToReplace < NormalTranspositionTableEntriesPerBucket);

		uint8_t flagEUL = flag & TTFlagEULMask;

		if (
			(depthRemaining >= shallowestSubTreeDepth)
			|| ((score >= EGTBWinningScore) && (flagEUL != TTFlagUpper)) // Always save 'winning' scores - ~6 ELO
			|| (flagEUL == TTFlagExact) // Always save PV entries
			)
		{
			{
				// 'Correct' any 'winning' scores for distance (because they are relative to the root position not to this position)
				if (score >= EGTBWinningScore)
					score += ply;
				else if (score <= EGTBLosingScore)
				{
					score -= ply;
					if (score <= MatedScore)
						flag |= TTFlagThreatenedWithMate;
				}

				uint64_t newData = (uint64_t)MGCompressMove(bestMove) | (((uint64_t)((uint16_t)score)) << 16) | (((uint64_t)((uint16_t)tteStaticEvaluation)) << 32) | (((uint64_t)(TranspositionTableAge | flag)) << 48) | (((uint64_t)((uint8_t)depthRemaining)) << 56);
				tte0[entryToReplace].data = newData;
				tte0[entryToReplace].hash64 = hash64 ^ newData;
				GATHERSTATS(TranspositionTableStoresSuccessful++;);
			}
		}
	}
}

#pragma endregion

//----------------------------------------------------------------------------------------------------

#include "SearchNormalQuiescence.cpp"

short Normal::TreeSearchNormal(short alpha, short beta, int ply, int depthRemaining, int sideToMove, int isInCheck, bool allowNull, bool isCutNode)
{
	assert(CompareMailboxBoard64ToPiecesBB(normalBrain.MailboxBoard64, normalBrain.piecesBB));
	assert((PopulationCountX(normalBrain.piecesBB[0][King]) == 1) && (PopulationCountX(normalBrain.piecesBB[1][King]) == 1));
	assert((PopulationCountX(normalBrain.piecesBB[0][Queen]) <= 9) && (PopulationCountX(normalBrain.piecesBB[1][Queen]) <= 9));
	assert((PopulationCountX(normalBrain.piecesBB[0][Rook]) <= 10) && (PopulationCountX(normalBrain.piecesBB[1][Rook]) <= 10));
	assert((PopulationCountX(normalBrain.piecesBB[0][Bishop]) <= 10) && (PopulationCountX(normalBrain.piecesBB[1][Bishop]) <= 10));
	assert((PopulationCountX(normalBrain.piecesBB[0][Knight]) <= 10) && (PopulationCountX(normalBrain.piecesBB[1][Knight]) <= 10));
	assert((PopulationCountX(normalBrain.piecesBB[0][Pawn]) <= 8) && (PopulationCountX(normalBrain.piecesBB[1][Pawn]) <= 8));
	assert(normalBrain.piecesBB[0][AllPieces] == (normalBrain.piecesBB[0][Pawn] | normalBrain.piecesBB[0][Knight] | normalBrain.piecesBB[0][Bishop] | normalBrain.piecesBB[0][Rook] | normalBrain.piecesBB[0][Queen] | normalBrain.piecesBB[0][King]));
	assert(normalBrain.piecesBB[1][AllPieces] == (normalBrain.piecesBB[1][Pawn] | normalBrain.piecesBB[1][Knight] | normalBrain.piecesBB[1][Bishop] | normalBrain.piecesBB[1][Rook] | normalBrain.piecesBB[1][Queen] | normalBrain.piecesBB[1][King]));
	assert(normalBrain.gameRecordPointer->transpositionTableHash64 == ((sideToMove == 0) ? GenerateTranspositionTableHash64(normalBrain.MailboxBoard64, normalBrain.gameRecordPointer) : ~GenerateTranspositionTableHash64(normalBrain.MailboxBoard64, normalBrain.gameRecordPointer)));
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
		return alpha;

	// Deepest so far this iteration?
	if (ply > MaximumPlyReachedBeforeQS)
	{
		MaximumPlyReachedBeforeQS = ply;
		if (IsDebug)
			LongestLineWithoutQS = normalBrain.CurrentLine(ply - 1) + " (Iteration:" + std::to_string(IterationPly) + " Ply:" + std::to_string(ply - 1) + " Alpha:" + std::to_string(alpha) + " Beta:" + std::to_string(beta) + ")";
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
			//	//		//if (CreateBitboardFromSquare((NormalGenerate.gameRecordPointer - 1)->move.mf.toSquare) & passedPawnsBB)
			//	//		goto continueNormalSearch;
			//	//	else
			//	//	{
			//	//		movablePassedPawnsBB = passedSide2(NormalGenerate.piecesBB[1][Pawn], NormalGenerate.piecesBB[0][Pawn]) & North(~occupiedBB);
			//	//		if (movablePassedPawnsBB && ((NormalGenerate.gameRecordPointer - 1)->pliesSinceIrreversible == 0))
			//	//			//if (CreateBitboardFromSquare((NormalGenerate.gameRecordPointer - 1)->move.mf.toSquare) & passedPawnsBB)
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
			//	//	//if (((NormalGenerate.gameRecordPointer - 1)->move.toSquarePiece) || (CreateBitboardFromSquare((NormalGenerate.gameRecordPointer - 1)->move.mf.toSquare) & promotablePawns))
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

	NodeCount++; // About 40% of nodes don't go into the QS

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
		// Ensures that we don't go deeper than any mate we've already got...
		// e.g. if STM at root has a #10 we won't go deeper than ply=19
		// e.g. if SNTM at root has a #10 we won't go deeper than ply=20
		// This is also included in the QS because reductions can cause you to enter the QS early and bypass this test in main.
		// Must NOT be used at the root

		// The best possible score for the side to move in this position (i.e. giving mate in 1) is MatingIn0Score - ply - 1, e.g. at ply 3 {a mate in 2} would score 15996
		// If an earlier variation has already acheived that score we can return immediately
		if (alpha >= MatingIn0Score - ply - 1)
			return alpha;
		// Some programs fiddle with alpha and beta and even set beta one lower so that if a #1 from here is found it causes a beta cutoff immediately BUT it doesn't store the move in the PV! (Which I find irritating!)
		// You can also test for the worst possible score but it gains nothing measurable as the above test will apply at the next ply
#pragma endregion

	}

	//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(150);

	// Initialisation
	MoveWithScore_Struct moveList[220];
	int legalMovesMade;
	short currentMoveScore = -MatingIn0Score;
	Move_Struct currentMove;

	currentGameRecordPointer->staticEvaluation = INT16_MIN;
	currentGameRecordPointer->isInCheck = isInCheck;

	//currentGameRecordPointer->dangerConditions.ui64 = 0; // These may get set if we find a TT entry
	currentGameRecordPointer->dangerConditions = 0; // These may get set if we find a TT entry
	*currentGameRecordPointer->principalVariationPointer = PVTUnknown; // Default PV terminator

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
		GATHERSTATS(TranspositionTableProbes++;);

		uint64_t hash64 = currentGameRecordPointer->transpositionTableHash64WithEP;
		tte0 = (NormalTranspositionTableEntry_Struct*)(NormalTranspositionTablePointer + (hash64 & NormalTranspositionTableBucketsMask));

		for (int entry = 0; entry < NormalTranspositionTableEntriesPerBucket; entry++) // Do we already have this position in the table?
		{
			uint64_t data = tte0[entry].data;
			uint64_t hash = tte0[entry].hash64 ^ data;

			if (hash == hash64)
			{
				tteFound = entry;



				//if (hash == 16746684170983717740)
				//	AC1++;//TEMP


				// Get the entry's data
				tteBestMove.ui32 = MGUnCompressMove(((NormalTranspositionTableEntryDataFields_Struct*)&data)->bestMove);
				assert((tteBestMove.ui32 == 0) == (((uint16_t)tteBestMove.ui32) == 0));
				tteSubTreeDepth = ((NormalTranspositionTableEntryDataFields_Struct*)&data)->subTreeDepth;
				uint8_t flag = ((NormalTranspositionTableEntryDataFields_Struct*)&data)->flag;
				tteEUL = (flag & TTFlagEULMask);
				//currentGameRecordPointer->dangerConditions.dc.isO1PCM = flag & TTFlagOnlyOnePieceCanMove;
				//currentGameRecordPointer->dangerConditions.dc.isTWM = flag & TTFlagThreatenedWithMate;
				//currentGameRecordPointer->dangerConditions.dc.isO1M = flag & TTFlagOnlyOneMove;
				//currentGameRecordPointer->dangerConditions.dc.isFMTP = flag & TTFlagFewerMovesThanPieces;
				currentGameRecordPointer->dangerConditions |= flag & TTFlagIsInDangerMask;
				currentGameRecordPointer->staticEvaluation = ((NormalTranspositionTableEntryDataFields_Struct*)&data)->staticEvaluation;
				assert((currentGameRecordPointer->staticEvaluation == INT16_MIN) || (currentGameRecordPointer->staticEvaluation == Evaluate(sideToMove)));
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
					}
				}

				if (tteSubTreeDepth >= depthRemaining)
				{
					//if (((data >> 48) & ageMask) != TranspositionTableAge) // Touch the age for aged entries - NEVER SEEMS TO GIVE ANY ELO INCREASE
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
									PRINTTREE(PrintTree(IterationPly, ply, alpha, beta, depthRemaining, -4, tteScore, currentGameRecordPointer->staticEvaluation, bestMoveScore););
									*currentGameRecordPointer->principalVariationPointer = PVTTTLower; // Should NEVER see this on the end of a PV!!!
									GATHERSTATS(TranspositionTableProbesSuccessful++;);
									return tteScore; // We can exit because we know that at least one move will exceed current beta
								}
							}
							else if (tteEUL == TTFlagUpper) // Upper limit? (Came from an All node: exact value is "at most" (<=) this value)
							{
								if (tteScore <= alpha)
								{
									PRINTTREE(PrintTree(IterationPly, ply, alpha, beta, depthRemaining, -3, tteScore, currentGameRecordPointer->staticEvaluation, bestMoveScore););
									*currentGameRecordPointer->principalVariationPointer = PVTTTUpper; // Should NEVER see this on the end of a PV!!!
									GATHERSTATS(TranspositionTableProbesSuccessful++;);
									return tteScore; // We can exit because we know that no move will exceed current alpha
								}
							}
							else // Exact value. (Came from a PV node)
							{
								// If we use 'exact' entries at PV nodes it can truncate the PV returned for the best move
								PRINTTREE(PrintTree(IterationPly, ply, alpha, beta, depthRemaining, -2, tteScore, currentGameRecordPointer->staticEvaluation, bestMoveScore););
								*currentGameRecordPointer->principalVariationPointer = PVTTTExact;
								assert(tteBestMove.ui32 != 0);
								//if (tteBestMove.ui32 != 0) // Sometimes there won't be a move stored as it might be a checkmate/stalemate/DBR position NOT ANY MORE???
								//{
								*currentGameRecordPointer->principalVariationPointer = tteBestMove.ui32; // Return TT best move as part of pv
								*(currentGameRecordPointer->principalVariationPointer + 1) = PVTTTExact;
								//}
								GATHERSTATS(TranspositionTableProbesSuccessful++;);
								return tteScore; // We can exit because we have an exact value
							}
						}
				}

				isCutNode = (tteEUL == TTFlagLower); // Try to make isCutNode more accurate for IIR

				break;
			}
		}
	}
#pragma endregion

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
				&& ((alpha < EGTBWinningScore + 1000 - ply) && (beta > EGTBLosingScore - 1000 + ply)) // Is the current window such that no EGTB score can possibly be in it? If so, skip the EGTB probe. This allows us to 'see-thru' the EGTBs to find any mates in this subtree.
				)
			{
				GATHERSTATS(EndgameTablebasesProbes++; if (totalPieces >= 6) EndgameTablebasesHeavyProbes++;);

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
					EndgameTablebasesHits++;

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
						// Bias EGTB draw scores towards the side with the most material (so +4, 0 or -4)
						// So it prefers a material up EGTB draw (+4) to other draws, e.g. DBR etc (+2...0...-2), to a material down EGTB draw (-4)
						egtbScore = 4 * ((currentGameRecordPointer->totalMaterial[sideToMove] > currentGameRecordPointer->totalMaterial[sideToMove ^ 1]) - (currentGameRecordPointer->totalMaterial[sideToMove] < currentGameRecordPointer->totalMaterial[sideToMove ^ 1]));
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

					//AddToNormalTranspositionTable(depthRemaining, ply, egtbScore, TTFlagExact, PVTEGTB, egtbScore); // I tried storing EGTB results in the TT but no significant ELO difference
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
	// Get the 'static' evaluation (our 'best guess' for the score of the current position)
	// N.B. staticEvaluation is just a material/positional evaluation so can never be in the winning/losing range
	if (currentGameRecordPointer->staticEvaluation == INT16_MIN) // The value may already have been retrieved from the TT
	{
		if ((currentGameRecordPointer - 1)->move.ui32 == NullMove) // If the previous move was a null move we can use its score (negated and corrected for tempo) to save some time (about 12% of nodes)
		{
			currentGameRecordPointer->staticEvaluation = -(currentGameRecordPointer - 1)->staticEvaluation + Tempo * 2;
			//currentGameRecordPointer->staticEvaluation = -(currentGameRecordPointer - 1)->staticEvaluation;// + Tempo * 2;//TEMP TESTING NNUE
			assert(currentGameRecordPointer->staticEvaluation == Evaluate(sideToMove));
		}
		else
			currentGameRecordPointer->staticEvaluation = Evaluate(sideToMove);
	}

	int improving = 0; // Used in LMP and reductions
	if ((ply > 2) && (currentGameRecordPointer->staticEvaluation > (normalBrain.gameRecordPointer - 2)->staticEvaluation))
		improving = 1;

	short bestGuessScore = currentGameRecordPointer->staticEvaluation;

	// Can we find a better guess in the TT?
	// N.B. winning/losing scores could be pulled from the TT
	if (tteScore != INT16_MIN)
	{
		if (tteEUL == TTFlagUpper)
		{
			//bestKnownScore = std::min(bestKnownScore, tteScore);
			if (tteScore < bestGuessScore)//THIS CODE HIGHLIGHTED SOME OF THE STUPIDITY OF PRUNING!
				bestGuessScore = tteScore;
		}
		else if (tteEUL == TTFlagLower)
		{
			bestGuessScore = std::max(bestGuessScore, tteScore);
			//if (tteScore > bestKnownScore)
			//	bestKnownScore = tteScore;
		}
		else
			bestGuessScore = tteScore;
	}

	//bestMoveScore = std::min(bestKnownScore, alpha);
	//bestMoveScore = std::min(currentGameRecordPointer->staticEvaluation, alpha);

#pragma endregion

		//----------------------------------------------------------------------------------------------------

#pragma region Razoring
	// Razoring - opposite of Node level futility pruning
	// Near the leaves, if our best guess for this position is way below alpha AND a quiescence search confirms we don't have any improving tactics then abandon this line as not good enough
	// The score we return here will be <=alpha and cause the opponent's previous move to fail high >=beta which will cause us to give up on our previous move 2 ply below as it also will be <=alpha
	if (
		(depthRemaining <= 5) // Near the leaves?
		&& (!isPVNode) // Not a PV node?
		&& (!isInCheck) // Not in check?
		)
	{
		assert(depthRemaining > 0);
		//short razoringEvaluation = alpha - (2 * MVPawn) - ((depthRemaining - 1) * (depthRemaining - 1) * MVPawn); // Harder to abandon the further from the leaves
		short razoringEvaluation = alpha - (MVPawn) - ((depthRemaining-1) * (depthRemaining-1) * MVPawn); // Harder to abandon the further from the leaves
		if (bestGuessScore < razoringEvaluation)
		{
			short score = TreeSearchNormalQuiescence(alpha - 1, alpha, ply, 0, sideToMove, isInCheck); // N.B. always enter the QS with depthRemaining=0
			if (score <= alpha)
			{
				PRINTTREE(PrintTree2(IterationPly, ply, "Razoring"););
				return score;
			}
		}
	}
#pragma endregion

	//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(190);

#pragma region Node level futility pruning
	// Node level futility pruning - opposite of Razoring
	// Near the leaves, if our best guess for this position is way above beta assume that we will likely find at least one move that will cause a cutoff
	// Don't do this when we have a losing/winning score as comparing what is probably a material/positional score with losing/winning scores is obviously nonsense
	// If beta<=EGTBLosingScore then there may be some losing tactic here that probably won't be reflected in our best guess so we need to search this position fully rather than assume it's going to cause a cutoff
	// If we don't test for this we (nearly) always immediately cutoff (with no moves being searched) and the move at the previous ply (which might be a shorter mate) gets discarded!
	// If beta>=EGTBWinningScore then our best guess will probably be well below beta anyway and if it's not we're comparing winning scores which is nonsense
	// We also test various special 'danger' conditions (TWM/O1M/O1PCM/ZLKM/FMTP) as it would be risky to assume we're ok should any of those conditions exist
	if (
		(!isPVNode) // Not a PV node?
		&& (!isInCheck) // Not in check?
		&& (currentGameRecordPointer->dangerConditions == 0)
		&& (std::abs(beta) < EGTBWinningScore) // Otherwise we always immediately cutoff (with no moves being searched) and the move at the previous ply (which might be a shorter mate) gets discarded - NEVER remove this!
		//&& (ply > FullWidthPlies)
		)
	{
		if (bestGuessScore - (depthRemaining * MVPawn) >= beta) // Harder to cutoff the further from the leaves
		{
			PRINTTREE(PrintTree2(IterationPly, ply, "Node level futility pruning"););
			return bestGuessScore;
		}
	}
#pragma endregion

	//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(200);

	int threatenedSquare = -1;

#pragma region NullMove
	// Null move (About 90% of normal (non-QS) nodes get this far)
	// If we do nothing here (a null move) and let the opponent try to hurt us and he can't, then we will likely find at least one move that will cause a cutoff (unless we're in zugzwang)
	// Perceived wisdom is that you shouldn't reduce into the QS. However my tests showed the opposite.
	// Also that you can't use this in endgames but I believe the added zugzwang tests now allow this!? :O
	// We also test various special 'danger' conditions (TWM/O1M/O1PCM/ZLKM/FMTP) as it would be risky to assume we're ok should any of those conditions exist
	if (
		(allowNull) // Null moves are not allowed at the 1st ply or immediately after a null move or when using IID but can occur multiple times on a line
		&& (!isPVNode)
		&& (!isInCheck)
		&& (currentGameRecordPointer->dangerConditions == 0)
		&& (bestGuessScore >= beta)
		&& ((currentGameRecordPointer->gamePhase[sideToMove] > 0) || ((normalBrain.SafePawnMoves(sideToMove)) && normalBrain.HasOpposition(sideToMove))) // If no pieces, check for safe pawn moves and having the opposition
		&& (beta > EGTBLosingScore) // Otherwise we will almost certainly assume a null move cutoff (with no moves being searched) and the move at the previous ply (which might be a shorter mating move) gets discarded (-2.2)
		//&& (ply >= nullMoveMinimumPly) // Used if we do a verification search
		//&& (ply > FullWidthPlies)
		)
	{ // About 54% of nodes (that get past the TT) perform a null move
		// Unfortunately null move has several drawbacks
		// 1) if the side to move is in zugzwang it is a mistake to assume that it can do nothing (hides the badness of the position)
		// 2) #1 above stops the side to move seeing a better move than the one it already has (hides the goodness of the position for stm)
		// 3) because it is used recursively it smashes the search depth meaning that we sometimes miss tactics in this variation

		if (
			(currentGameRecordPointer->gamePhase[sideToMove] <= 8) // In the endgame?
			//&& (currentGameRecordPointer->dangerConditions.dc.isZLKM = !normalBrain.KingCanLegallyMove(sideToMove)) // Have we got any legal king moves (and save it)
			&& (currentGameRecordPointer->dangerConditions |= (TTFlagZeroLegalKingMoves * (!normalBrain.KingCanLegallyMove(sideToMove)))) // Have we got any legal king moves (and save it)
			) // Helps find mates when the defending K is constrained (so the defender can't just 'null' to slash the search depth and hide the fact that it's being mated)
			allowNull = false;
		else if (depthRemaining > 10)
		{
			normalBrain.CalculatePinnedPieces(sideToMove); // Required for legal move generation
			uint32_t count = normalBrain.CountAllMoves(sideToMove, isInCheck);
			if (count == 1) // O1M
			{
				//currentGameRecordPointer->dangerConditions.dc.isO1M = TTFlagOnlyOneMove;
				currentGameRecordPointer->dangerConditions |= TTFlagOnlyOneMove;
				allowNull = false;
			}
			else if (count < PopulationCountX(normalBrain.piecesBB[sideToMove][AllPieces])) // FMTP
			{
				//currentGameRecordPointer->dangerConditions.dc.isFMTP = TTFlagFewerMovesThanPieces;
				currentGameRecordPointer->dangerConditions |= TTFlagFewerMovesThanPieces;
				allowNull = false;
			}
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
			normalBrain.gameRecordPointer->pliesSinceIrreversible = 0; // Don't allow DBRs across a null move (+3 ELO)
			normalBrain.gameRecordPointer->transpositionTableHash64 = ~(normalBrain.gameRecordPointer - 1)->transpositionTableHash64;
			normalBrain.gameRecordPointer->transpositionTableHash64WithEP = normalBrain.gameRecordPointer->transpositionTableHash64;
			normalBrain.gameRecordPointer->epSquare = 0;
			*(uint32_t*)(&normalBrain.gameRecordPointer->totalMaterial[0]) = *(uint32_t*)(&(normalBrain.gameRecordPointer - 1)->totalMaterial[0]); // N.B. Using data type overload at start of line to copy for both sides!
			*(uint64_t*)(&normalBrain.gameRecordPointer->gamePhase[0]) = *(uint64_t*)(&(normalBrain.gameRecordPointer - 1)->gamePhase[0]);
			*(uint32_t*)(&normalBrain.gameRecordPointer->totalOpeningPST[0]) = *(uint32_t*)(&(normalBrain.gameRecordPointer - 1)->totalOpeningPST[0]);
			*(uint32_t*)(&normalBrain.gameRecordPointer->totalEndgamePST[0]) = *(uint32_t*)(&(normalBrain.gameRecordPointer - 1)->totalEndgamePST[0]);
			PRINTTREE(PrintTree(IterationPly, ply, alpha, beta, depthRemaining, -1, -999, currentGameRecordPointer->staticEvaluation, bestMoveScore););


			int R;
			R = 3 + (depthRemaining / 5) + std::min(9, ((bestGuessScore - beta) / 128));
			normalBrain.gameRecordPointer->move.ui32 = 0;
			short nullMoveScore = (short)-TreeSearchNormal((short)-beta, (short)(-beta + 1), ply + 1, depthRemaining - R - 1, sideToMove ^ 1, false, false, !isCutNode);

			// About 89% of nodes after a null move are 'all' nodes

			// Unmake null move
			normalBrain.gameRecordPointer--;

			if (nullMoveScore >= beta)
				return nullMoveScore;

			if (nullMoveScore <= alpha - 80)
			{
				if ((normalBrain.gameRecordPointer + 1)->move.ui32 != 0)
					threatenedSquare = (normalBrain.gameRecordPointer + 1)->move.mf.toSquare;
				if (nullMoveScore < MatedScore)
					//currentGameRecordPointer->dangerConditions.dc.isTWM = TTFlagThreatenedWithMate;
					currentGameRecordPointer->dangerConditions |= TTFlagThreatenedWithMate;
			}
		}
	}
	// About 53% of nodes don't get cutoff by the null move
#pragma endregion

	//----------------------------------------------------------------------------------------------------

#pragma region IIR
	// Internal Iterative Reductions - https://chessprogramming.org/Internal_Iterative_Reductions
	if (!isPVNode
		&& isCutNode
		&& (tteBestMove.ui32 == 0)
		//&& (depthRemaining > 2)
		//)
		//depthRemaining -= 2;
		&& (depthRemaining > 1)
		)
		depthRemaining = std::max(depthRemaining - 2, 1);
#pragma endregion

	//----------------------------------------------------------------------------------------------------

	//// Singular extensions
	//Move_Struct excludedMove;
	//excludedMove.ui32 = 0;
	//short originalBeta;
	//int originalDepthRemaining;
	//bool singular = false;

	////if (isPVNode && (tteBestMove.ui32 != 0) && (ply > 1) && (depthRemaining >= 8))
	//if (
	//	!isInCheck
	//	&& !isPVNode
	//	&& (tteEUL == TTFlagLower)
	//	&& (tteBestMove.ui32 != 0)
	//	&& (ply > 1)
	//	&& (depthRemaining >= 8)
	//	&& (tteScore >= alpha)
	//	&& (std::abs(tteScore) < EGTBWinningScore)
	//	&& (tteSubTreeDepth >= depthRemaining - 3)
	//	&& !SingularExtending
	//	&& 0
	//	)
	//{
	//	excludedMove = tteBestMove;
	//	//originalAlpha = alpha;
	//	originalBeta = beta;
	//	originalDepthRemaining = depthRemaining;

	//	alpha = tteScore;
	//	alpha -= 100;
	//	beta = alpha + 1;
	//	depthRemaining = depthRemaining / 2;


	//}

	//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(210);

GenerateMoveList:
	// Generate move list
	int movesCount;
	normalBrain.CalculatePinnedPieces(sideToMove); // Required for legal move generation
	movesCount = normalBrain.GenerateAllMoves(sideToMove, isInCheck, moveList);
	assert(movesCount == normalBrain.CountAllMoves(sideToMove, isInCheck));

	//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(220);

	if (movesCount == 0) // No legal moves generated?
	{
		// Is the side to move in check?
		if (isInCheck)
		{
			// Checkmate
			assert(tteBestMove.ui32 == 0);
			*currentGameRecordPointer->principalVariationPointer = PVTCheckmate;
			return (short)(-MatingIn0Score + ply); // At the root: #1=15998, #-1=-15997, #2=15996, #-2=-15995, #3=15994...
		}
		else
		{
			// Stalemate
			assert(tteBestMove.ui32 == 0);
			*currentGameRecordPointer->principalVariationPointer = PVTDrawStalemate;
			return drawScore;
		}
	}

	if (movesCount == 1)
		//currentGameRecordPointer->dangerConditions.dc.isO1M = TTFlagOnlyOneMove;
		currentGameRecordPointer->dangerConditions |= TTFlagOnlyOneMove;

	// Useful for avoiding null move in potential zugzwang positions
	if (movesCount < PopulationCountX(normalBrain.piecesBB[sideToMove][AllPieces]))
		//currentGameRecordPointer->dangerConditions.dc.isFMTP = TTFlagFewerMovesThanPieces;
		currentGameRecordPointer->dangerConditions |= TTFlagFewerMovesThanPieces;

	// Only one piece can move? (potential stalemate if piece captured or a forced position)
	if (moveList[0].mf.fromSquare == moveList[movesCount - 1].mf.fromSquare)
		//currentGameRecordPointer->dangerConditions.dc.isO1PCM = TTFlagOnlyOnePieceCanMove;
		currentGameRecordPointer->dangerConditions |= TTFlagOnlyOnePieceCanMove;

	assert(NoDuplicateMoves(moveList, movesCount));
	assert(TranpositionTableMoveFound(moveList, movesCount, tteBestMove.ui32));

	//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(230);

#pragma region IID (Internal Iterative Deepening)
	//// IID (Internal Iterative Deepening)
	//// If we don't have a good move from the transposition table, do a shallow search to obtain one to try to improve move ordering
	//if (tteBestMove.ui32 == 0)
	//{
	//	if ((isPVNode) && (ply > 1))
	//	{
	//		if (depthRemaining > 1)
	//		{
	//			// This is very rarely called!
	//			short iidMoveScore = (short)TreeSearchNormal(-MateBaseScore, beta, ply, std::max(depthRemaining - 2, 1), sideToMove, isInCheck, false, isCutNode); // +1.9/20000 for using -INF as alpha
	//			tteBestMove.ui32 = *currentGameRecordPointer->principalVariationPointer;
	//			//assert(tteBestMove.ui32 != 0); //This is only true if it's a 'PV' or 'Cut' type node or we use -INF as alpha in the search above CAN NOW FAIL WITH SF STEP 10 ABOVE
	//		}
	//	}
	//}
#pragma endregion

	//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(240);

	// Score moves for ordering
	int8_t pt1, pt2, ts1, ts2;

	// Get the previous move details
	pt1 = abs((currentGameRecordPointer - 1)->move.fromSquarePiece) - 1; // 0..5
	ts1 = (currentGameRecordPointer - 1)->move.mf.toSquare;
	assert((pt1 >= Pawn - 1) && (pt1 <= King - 1) && (ts1 >= A1) && (ts1 <= H8));
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

	bool anyPruningDone = false;

	// Loop through move list
	*currentGameRecordPointer->principalVariationPointer = PVTUnknown; // Default PV terminator (setting this again here as razoring can disturb it!)
	legalMovesMade = 0;
	int enemyKingSquare = GetLS1BIndex(normalBrain.piecesBB[sideToMove ^ 1][King]);
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
				// Without it we get about 71% cut / 76% all correct
				// With it we get about 74% cut / 98% all correct
				isCutNode = false;
			}
		}
		assert((bestSortIndex >= 0) && (bestSortIndex < movesCount));

		currentMove.ui32 = moveList[bestSortIndex].ui32;

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
			if (EndgameTablebasesRootMove.ui32 != 0) // Are we in the EGTB at the root
			{
				int wdl = RetrieveRootMoveWDLStatus(currentMove.ui32);
				if (wdl < EndgameTablebasesRootWDL)
					continue; // Discard moves that don't preserve the root EGTB result e.g. if we're in a win at the root discard moves that only draw or lose
			}
		}
		PRINTTREE(PrintTree(IterationPly, ply, alpha, beta, depthRemaining, currentMove.ui32, bestSortScore, currentGameRecordPointer->staticEvaluation, bestMoveScore););


		//if (currentMove.ui32 == excludedMove.ui32)
		//	continue;



		// Up-date move
		currentGameRecordPointer->isThreateningMateInOne.ui32 = 0;
		normalBrain.MakeMove(sideToMove); // N.B. MakeMove increments normalBrain.gameRecordPointer!
		legalMovesMade++;

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
		CRASHLOCATION(251);

		BREAKONCURRENTVARIATION("a4b4 ");

		bool safePassedPawnMove = false;
		// Have we just pushed a passed pawn safely? (captures excluded)
		if (
			(SEEResult == 0)
			&& (std::abs(currentGameRecordPointer->move.fromSquarePiece) == Pawn)
			&& (currentGameRecordPointer->move.toSquarePiece == Empty)
			)
		{
			uint64_t passedBB;
			if (sideToMove == 0)
				passedBB = passedSide1(normalBrain.piecesBB[0][Pawn], normalBrain.piecesBB[1][Pawn]);
			else
				passedBB = passedSide2(normalBrain.piecesBB[1][Pawn], normalBrain.piecesBB[0][Pawn]);
			//passedBB = passedSTM[sideToMove](normalBrain.piecesBB[sideToMove][Pawn], normalBrain.piecesBB[sideToMove ^ 1][Pawn]); // Tried using a function array but it was slower!
			safePassedPawnMove = (passedBB & CreateBitboardFromSquare(currentGameRecordPointer->move.mf.toSquare));
		}

		//----------------------------------------------------------------------------------------------------

		//N.B. Sometimes it is an aspect of the move that makes it interesting (e.g. a P just moved to the 7th rank) and sometimes an aspect of the position (e.g. there is a P on the 7th rank)
		int newDepthRemaining = depthRemaining - 1;

		// Extensions (about 1% of moves get extended)
		int extensions = 0;
		int TMI1ExtensionsSaved = TMI1Extensions;//NOT USED???
		//if ((ply <= IterationPly * 2) && (ply + depthRemaining <= MaximumIterationPly)) // Hard limits to try to avoid things like hitting the MaximumPly, endless K chases in the endgame etc
		if ((ply <= IterationPly) || ((ply <= IterationPly * 2) && isPVNode))
		{
			//if (singular)
			//	extensions = 1;
			//else

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
			else if (
				safePassedPawnMove
				)
				extensions = 1;

			newDepthRemaining += extensions;
		}

		//----------------------------------------------------------------------------------------------------
		CRASHLOCATION(252);

		//Pruning
		if (
			(legalMovesMade > 1) // Don't prune the 1st move WHY??? UNLESS IT'S TT, MVVLVA, KR, CM, FUM WHICH IT MIGHT NOT BE! OR O1M! CHECK ITS bestSortScore - GOES CRAZY IF YOU TAKE IT OUT!!!
			&& (extensions == 0)
			&& (!isPVNode)
			&& (!isInCheck) // N.B. you must NOT remove this otherwise you may get false mates returned!
			&& (!givesCheck)
			&& (quietMove) // N.B. this excludes captures and promotions
			//&& (beta > EGTBLosingScore) // Never prune if we're 'losing' as we want to try EVERYTHING to find a way out! BUT IF BETA<=LosingBaseScore THEN SO TOO IS ALPHA AND THE TEST BELOW COULD NEVER KICK IN?!
			//&& (alpha < WinningBaseScore) // If we have a 'winning' score then EVERY (non-special/quiet) move will be futility pruned and you could 'lose' the EGTB win or miss a better mate! So do NOT take this out!!!
			//&& (currentGameRecordPointer->isTWM == 0)
			//&& (ply > FullWidthPlies)
			&& (!safePassedPawnMove)
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
				//&& ((int)(NodeCount & 255) > (IterationPly - ply))//TESTING
				//&& (bestMoveScore == alpha)
				//&& (threatenedSquare != currentMove.mf.fromSquare)
				)
			{
				assert(quietMove);
				PRINTTREE(PrintTree2(IterationPly, ply, "LMP"););
				//if (depthRemaining == 1)
				//{
				normalBrain.UnMakeMove(sideToMove);
				anyPruningDone = true;
				continue;
				//}
				//else
				//{
				//	short score = TreeSearchNormalQuiescence((short)-beta, (short)-alpha, ply + 1, 0, sideToMove ^ 1, 0); // N.B. always enter the QS with depthRemaining=0
				//	if (score <= alpha)
				//	{
				//		normalBrain.UnMakeMove(sideToMove);
				//		continue;
				//	}
				//	else
				//		goto xxx;
				//}
			}

			// Futility pruning
			short futilityScore; // NOT USED???
			if (
				(depthRemaining <= 8)
				&& ((futilityScore = currentGameRecordPointer->staticEvaluation + MaterialValue[abs(currentGameRecordPointer->move.toSquarePiece)] + FutilityMargin[depthRemaining]) <= alpha)
				//&& ((futilityScore = bestKnownScore + MaterialValue[abs(currentGameRecordPointer->move.toSquarePiece)] + FutilityMargin[depthRemaining]) <= alpha)
				&& (bestMoveScore > EGTBLosingScore) // Don't prune if we're losing!
				&& (!((std::abs(currentGameRecordPointer->move.fromSquarePiece) == Pawn) && ((currentMove.mf.toSquare >> 3) == SeventhRank[sideToMove]))) // P move to 7th?
				//&& (bestMoveScore==alpha)
				//&& (bestSortScore <= 0)
				//&& (threatenedSquare != currentMove.mf.fromSquare)
				)
			{
				PRINTTREE(PrintTree2(IterationPly, ply, "Futility pruning"););
				normalBrain.UnMakeMove(sideToMove);
				anyPruningDone = true;
				continue;
			}

			// SEE pruning
			if (
				(depthRemaining <= 6)
				&& (SEEResult < 0)
				&& (bestMoveScore > EGTBLosingScore) // Don't prune if we're losing! (Do NOT take this out otherwise we could get faulty mate scores returned e.g. 7r/B1b1qbPB/2k1q2q/1Nq2nN1/qQQ2QQq/8/q1RRQ2q/3K4 w - - 0 1, go depth 3, returns #3 when it's actually #7!)
				//DOESN'T DO IT ANY MORE!!!!!
				)
			{
				PRINTTREE(PrintTree2(IterationPly, ply, "SEE pruning"););
				normalBrain.UnMakeMove(sideToMove);
				anyPruningDone = true;
				continue;
			}
		}
	xxx:

		//----------------------------------------------------------------------------------------------------
		CRASHLOCATION(253);

		bool doNonReducedSearch;
		int searches = 0;

		// Late move reductions (can massively increase search depth but also make the best root move returned very sensitive to move ordering)
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
			&& (!safePassedPawnMove)
			//&& (!((std::abs(currentGameRecordPointer->move.fromSquarePiece) == Pawn) && ((currentMove.mf.toSquare >> 3) == SeventhRank[sideToMove]))) // P move to 7th?
			//&& (quietMove)
			//(currentGameRecordPointer->isTWM == 0) &&
			//(!(CreateBitboardFromSquare(currentMove.mf.fromSquare) & passedPawnsBB)) && // Don't reduce moves by passed Ps

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
			if (bestSortScore <= 0)
			{
				reductions++;
				reductions += (legalMovesMade >> 4);
			}
			if (SEEResult < 0)
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
			//						if (LineListBB[currentMove.mf.fromSquare][currentMove.mf.toSquare] & CreateBitboardFromSquare((normalBrain.gameRecordPointer - 3)->move.mf.fromSquare))
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



		CRASHLOCATION(254);

		if (IterationPly == 1)
		{
			// On the 1st iteration use an open window to get exact scores for every root move to help with initial ordering
			currentMoveScore = (short)-TreeSearchNormal((short)-MatingIn0Score, (short)MatingIn0Score, ply + 1, newDepthRemaining, sideToMove ^ 1, givesCheck, true, false);
			CRASHLOCATION(255);
		}
		else
		{
			if (doNonReducedSearch)
			{//SURELY THIS SHOULD NOW BE A CUT NODE SO THE CHILD SHOULD BE AN 'ALL' NODE?!?!
				searches++;
				// Minimal window search
				currentMoveScore = (short)-TreeSearchNormal((short)(-alpha - 1), (short)-alpha, ply + 1, newDepthRemaining, sideToMove ^ 1, givesCheck, true, !isCutNode);
				CRASHLOCATION(256);
				// About 79% of non-reduced searches don't exceed alpha
			}

			if (isPVNode && ((legalMovesMade == 1) || ((currentMoveScore > alpha) && ((ply == 1) || (currentMoveScore < beta)))))
			{
				CRASHLOCATION(257);
				assert(reductions == 0);
				assert(searches <= 1);
				searches++;

				currentMoveScore = (short)-TreeSearchNormal((short)-beta, (short)-alpha, ply + 1, newDepthRemaining, sideToMove ^ 1, givesCheck, true, false);
				CRASHLOCATION(258);
				//assert(PVSearchedFirst(ply));
			}
			assert((searches > 0) && (searches < 3));
		}

		// N.B. currentMoveScore will probably be a bound rather than an exact score!

		//----------------------------------------------------------------------------------------------------

#ifdef SEARCHINGFORLINE
		if (TargetLineLength == ply)
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
			return alpha;

		//----------------------------------------------------------------------------------------------------
		CRASHLOCATION(280);

		// Root move?
		if (ply == 1)
		{
			// If we are in the EGTB at the root then all moves that don't preserve the EGTB status will already have been pruned above
			if (EndgameTablebasesRootMove.ui32 != 0)
				if (std::abs(currentMoveScore) < MatingScore) // No mate found?
				{
					int dtz = RetrieveRootMoveDTZStatus(currentMove.ui32); // +ve for wins, 0 for draws, -ve for losses
					if (dtz > EndgameTablebasesRootDTZ) // Sub-optimal DTZ? (For wins EndgameTablebasesRootDTZ will be the lowest +ve dtz, for losses EndgameTablebasesRootDTZ will be the lowest -ve dtz)
						currentMoveScore = -MatingIn0Score; // Discard
					// Only moves which preserve the EGTB result and optimal DTZ use their actual search scores
				}

			if (IterationPly == 1)
				SaveRootMoveData(currentMove.ui32, NodeCount + NodeCountQuiescenceSearch, currentMoveScore); // On the first iteration, save every root moves fail-soft score and subtree size

			// Should we stop the search? (Sets a couple of flags internally which are tested elsewhere)
			TimeUp(1.0f);
		}

		//----------------------------------------------------------------------------------------------------
		CRASHLOCATION(290);

		//if (currentMoveScore < MatedScore)
		//	currentGameRecordPointer->isTWM = TTFlagThreatenedWithMate; // Seems to lose a few ELO

		//if (singular)
		//	SingularExtending = false;

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
						//currentGameRecordPointer->historyPointer->History[pt2][ts2] = std::min(std::max(currentGameRecordPointer->historyPointer->History[pt2][ts2], 0) + delta, (1 << 30));
						for (int count = 0; count < quietMovesSearchedCount; count++)
						{
							Move_Struct ms;
							ms.ui32 = quietMovesSearched[count];
							int8_t pt = std::abs(normalBrain.MailboxBoard64[ms.mf.fromSquare]) - 1;
							int8_t ts = ms.mf.toSquare;
							//currentGameRecordPointer->historyPointer->History[pt][ts] = std::max(currentGameRecordPointer->historyPointer->History[pt][ts] - delta, 0);// -(1 << 30));
							currentGameRecordPointer->historyPointer->History[pt][ts] = std::max(currentGameRecordPointer->historyPointer->History[pt][ts] - delta, -(1 << 30));
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
					assert(currentMove.ui32 == currentGameRecordPointer->move.ui32);
					//AddToNormalTranspositionTable(depthRemaining, ply, currentMoveScore, TTFlagLower + currentGameRecordPointer->dangerConditions.dc.isTWM + currentGameRecordPointer->dangerConditions.dc.isO1M + currentGameRecordPointer->dangerConditions.dc.isFMTP + currentGameRecordPointer->dangerConditions.dc.isO1PCM, currentGameRecordPointer->move.ui32, currentGameRecordPointer->staticEvaluation, tteFound);
					AddToNormalTranspositionTable(depthRemaining, ply, currentMoveScore, TTFlagLower + currentGameRecordPointer->dangerConditions, currentGameRecordPointer->move.ui32, currentGameRecordPointer->staticEvaluation, tteFound);

					if (ply == 1) // Failed high at the root?
					{
						ShowBestLineMessage(currentMoveScore, TTFlagLower);
						if (CurrentIterationsPreviousRootBestMove.ui32 == 0) // In case of multiple fail-highs
							CurrentIterationsPreviousRootBestMove = RootBestMove; // Save this so that it can be restored in the case of a fail-high-low situation
						RootBestMove = currentMove; // Save the RootBestMove immediately in case we don't get a chance to complete the research!
					}

					PRINTTREE(PrintTree2(IterationPly, ply, "Cut"););
					//if (excludedMove.ui32 != 0)
					//	goto singularExtensionAbort;
					return currentMoveScore;
				}

				// PV node: score >alpha and <beta
				CRASHLOCATION(296);
				assert(isPVNode);
				alpha = currentMoveScore;
				if (ply == 1)
				{
					ShowBestLineMessage(currentMoveScore, TTFlagExact);
					RootBestMove = currentMove;

				}
			}

			bestMoveScore = currentMoveScore;
		}

		if (quietMove)
			quietMovesSearched[quietMovesSearchedCount++] = currentMove.ui32;

		CRASHLOCATION(299);
	} // (Loop through move list)

	//----------------------------------------------------------------------------------------------------
	CRASHLOCATION(310);

	//	// Were we just doing a singular extension test search?
	//singularExtensionAbort:
	//	if (excludedMove.ui32 != 0)
	//	{
	//		//AC1++;
	//		if (currentMoveScore < beta)
	//		{
	//			singular = true;
	//			//AC2++;
	//			SingularExtending = true;
	//		}
	//		excludedMove.ui32 = 0;
	//		alpha = originalAlpha;
	//		beta = originalBeta;
	//		depthRemaining = originalDepthRemaining;
	//		bestMoveScore = -MatingIn0Score;
	//
	//		goto GenerateMoveList;
	//	}




		// Do we have an EGTB score from earlier?
	if (egtbScore != -MatingIn0Score)
	{
		*currentGameRecordPointer->principalVariationPointer = PVTEGTB;
		return egtbScore;
	}

	// Update transposition table
	if (alpha == originalAlpha)
	{
		// No move has returned a score > alpha, therefore this is an 'All' node (all legal moves have been searched)
		// The bestMoveScore is an upper bound (ceiling) on the exact score of the node (i.e. the exact score might be less than bestMoveScore, it is "at most" bestMoveScore)
		// The children of an All node are Cut nodes. The parent of an All node is a Cut node. The ply distance of an All node to its PV ancestor is even.
		if (anyPruningDone)
		{
			//bestMoveScore = alpha;
			if (bestGuessScore > bestMoveScore)
				if (bestGuessScore < alpha)
					bestMoveScore = bestGuessScore;
			//bestMoveScore = std::min(bestKnownScore, alpha);
		}
		assert((bestMoveScore > -MatingIn0Score) && (bestMoveScore <= alpha));
		assert(*currentGameRecordPointer->principalVariationPointer == PVTUnknown);
		PRINTTREE(PrintTree2(IterationPly, ply, "All:" + std::to_string(bestMoveScore)););
		//AddToNormalTranspositionTable(depthRemaining, ply, bestMoveScore, TTFlagUpper + currentGameRecordPointer->dangerConditions.dc.isTWM + currentGameRecordPointer->dangerConditions.dc.isO1M + currentGameRecordPointer->dangerConditions.dc.isFMTP + currentGameRecordPointer->dangerConditions.dc.isO1PCM, tteBestMove.ui32, currentGameRecordPointer->staticEvaluation, tteFound); // Keep any existing TT move even though it didn't raise alpha
		AddToNormalTranspositionTable(depthRemaining, ply, bestMoveScore, TTFlagUpper + currentGameRecordPointer->dangerConditions, tteBestMove.ui32, currentGameRecordPointer->staticEvaluation, tteFound); // Keep any existing TT move even though it didn't raise alpha
	}
	else
	{
		// A move has returned a score > (the original) alpha but < beta, therefore this is a 'PV' node (all legal moves have been searched)
		// The bestMoveScore is the EXACT score of the node
		// The root node and the leftmost nodes are always PV-nodes. All siblings of a PV node are expected Cut nodes.
		assert((originalAlpha < bestMoveScore) && (bestMoveScore == alpha) && (bestMoveScore < beta));
		assert(isPVNode);
		assert(*currentGameRecordPointer->principalVariationPointer != PVTUnknown);
		PRINTTREE(PrintTree2(IterationPly, ply, "Exact:" + std::to_string(bestMoveScore)););
		//AddToNormalTranspositionTable(depthRemaining, ply, bestMoveScore, TTFlagExact + currentGameRecordPointer->dangerConditions.dc.isTWM + currentGameRecordPointer->dangerConditions.dc.isO1M + currentGameRecordPointer->dangerConditions.dc.isFMTP + currentGameRecordPointer->dangerConditions.dc.isO1PCM, *currentGameRecordPointer->principalVariationPointer, currentGameRecordPointer->staticEvaluation, tteFound);
		AddToNormalTranspositionTable(depthRemaining, ply, bestMoveScore, TTFlagExact + currentGameRecordPointer->dangerConditions, *currentGameRecordPointer->principalVariationPointer, currentGameRecordPointer->staticEvaluation, tteFound);
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



	//int score = nnue.evaluate(
	//	normalBrain.mailboxBoard64,
	//	SideToMove
	//);
	//Output("NNUE=" + std::to_string(score));




	// Set up the bit boards from the 64-square mailbox board
	ConvertMailboxBoard64ToPiecesBB(normalBrain.MailboxBoard64, normalBrain.piecesBB);

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
	uint32_t previousIterationsRootBestMove = 0;
	ReplyImmediately = false;
	InitialiseMaterialValues(normalBrain.MailboxBoard64, normalBrain.gameRecordPointer);
	InitialisePSTValues(normalBrain.MailboxBoard64, normalBrain.gameRecordPointer);
	InitialiseGamePhase(normalBrain.MailboxBoard64, normalBrain.gameRecordPointer);
	uint64_t hash64 = GenerateTranspositionTableHash64(normalBrain.MailboxBoard64, normalBrain.gameRecordPointer);
	if (SideToMove == 1)
		hash64 = ~hash64;
	normalBrain.gameRecordPointer->transpositionTableHash64 = hash64;
	normalBrain.gameRecordPointer->transpositionTableHash64WithEP = hash64 ^ TranspositionTableRandomsEnPassant[normalBrain.gameRecordPointer->epSquare]; // N.B. TranspositionTableRandomsEnPassant[0] = 0
	//normalBrain.gameRecordPointer->dangerConditions.dc.isZLKM = 0;
	normalBrain.gameRecordPointer->dangerConditions &= ~TTFlagZeroLegalKingMoves;

	lastPawnScoreWhite.bb = 0;
	lastPawnScoreWhite.pawnStructureOpeningScore = 0;
	lastPawnScoreWhite.pawnStructureEndgameScore = 0;
	lastPawnScoreBlack.bb = 0;
	lastPawnScoreBlack.pawnStructureOpeningScore = 0;
	lastPawnScoreBlack.pawnStructureEndgameScore = 0;
	(normalBrain.gameRecordPointer - 1)->staticEvaluation = Evaluate(SideToMove ^ 1);

	TranspositionTableStores = 0;
	TranspositionTableStoresSuccessful = 0;
	TranspositionTableProbes = 0;
	TranspositionTableProbesSuccessful = 0;

	// Root move list stuff
	// Generated once here before the first iteration and the moves stay in the same physical order in which they are generated so that they correspond with the same moves in the tree generated move list
	// All moves in the RootMoveList have their 'priority' set to zero before the first iteration
	// During the first iteration we update every root move with its fail-soft score and subtree size
	// After the first iteration we use the fail-soft score to order the moves for the second iteration
	// For the second and subsequent iterations we increase the priority value of any move that takes over as best and use that for ordering on the next iteration
	// Inside every iteration at the root we score the root move list (for move ordering) with ScoreRootMoveList
	MoveWithScore_Struct moveList[220];
	RootMoveList[0].mws.ui32 = 0; //WHY???
	normalBrain.CalculatePinnedPieces(SideToMove); // Required for legal move generation
	RootMovesCount = normalBrain.GenerateAllMoves(SideToMove, normalBrain.IsEnemyKingAttacked(GetLS1BIndex(normalBrain.piecesBB[SideToMove][King]), SideToMove ^ 1), moveList);
	if (RootMovesCount == 0) // Sometimes the GUI or the user provide positions with zero legal moves! (e.g. checkmates/stalemates in chess)
	{
		Output("info string *** Error! There are zero moves in the position provided!");
		OutputError("*** Error! There are zero moves in the position provided!");
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
		//RootMoveList[moveListIndexIterator].EGTBRank = -1;
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

	EndgameTablebasesTreeProbeLimitQS = std::min(5, EndgameTablebasesPiecesFound); // Only probe a maximum of the 5pc in the QS
	EndgameTablebasesTreeProbeLimitQS = std::min(EndgameTablebasesTreeProbeLimitQS, SyzygyProbeLimit);

	EndgameTablebasesRootMove.ui32 = 0; // This will be set to a valid move if we are in the EGTBs at the root
	EndgameTablebasesRootWDL = -MAXINT;
	EndgameTablebasesRootDTZ = 0;
	EndgameTablebasesRootRank = -MAXINT;
	EndgameTablebasesErrors = false;
	for (int index = 0; index < 8; index++)
		EndgameTablebasesErrorCounts[index] = 0;
	LichessMoves[0].move = "";
	RootFEN = ConvertPositionToFEN(normalBrain.MailboxBoard64, SideToMove, normalBrain.gameRecordPointer->castlingStatus, normalBrain.gameRecordPointer->epSquare, 0, 1);

	if (ThreadId == 0) // Only the main thread will modify its root move list based on the EGTBs
	{
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

				if (result != 0)
				{
					// If we have the DTZ info then don't probe the EGTBs in the tree
					EndgameTablebasesTreeProbeLimitMain = 0;
					EndgameTablebasesTreeProbeLimitQS = 0;
				}
				else
				{
					// If we don't have the DTZ info then
					// 1: allow EGTB probes in the tree so that hopefully it can force exchanges into a lower pieces win or avoid exchanges into a lower pieces loss
					// 2: get the WDL info (may happen e.g. if you have the 7-piece .rtbw files but not the .rtbz files)
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
					// N.B. unlike the Lichess http queries these are not ordered by distance of any sort, rather just how they come out of the move generator
					for (uint32_t index = 0; index < results.size; index++)
					{
						TbRootMove move = results.moves[index];
						// If we have the DTZ info (.rtbz files) : move.tbRank will be +262144-1-DTZ (0x40000) for wins, -262144+1+DTZ for losses and 0 for draws
						// If we don't have the DTZ info (.rtbz files) : move.tbRank will be +262144 (0x40000) for wins, -262144 for losses and 0 for draws, so dtz will be 0 for all moves
						Move_Struct colossusMove;
						colossusMove.ui32 = normalBrain.SYZYGYPYRRHICMoveToColossusMove(move.move, normalBrain.gameRecordPointer->epSquare);
						int wdl; // win=1, draw=0, loss=-1 : this is deduced by the sign of tbRank : no distinction is made for cursed-wins and blessed-losses
						int dtz; // >0 for wins, 0 for draws, <0 for losses : we want to select the lowest value, so for wins the lowest +ve dtz, for losses the lowest -ve dtz
						if (move.tbRank > 0)
						{
							wdl = 1;
							dtz = 0x40000 - 1 - move.tbRank; // The -1 brings the DTZ in line with that displayed on the SYZYGY website https://syzygy-tables.info/
							assert(dtz > 0); // For wins we want to play the lowest dtz
						}
						else if (move.tbRank == 0)
						{
							wdl = 0;
							dtz = 0;
						}
						else
						{
							wdl = -1;
							dtz = -(0x40000 - 1 + move.tbRank);
							assert(dtz > 0); // For losses we want to play the highest dtz
						}
						bool found = UpdateRootMoveEGTBStatus(colossusMove.ui32, wdl, dtz);// , move.tbRank);
						if (!found)
							OutputError("Did not find EGTB move in RootMoveList! " + MoveNotation(colossusMove.ui32));

						// Save the move with the highest rank
						if (move.tbRank > EndgameTablebasesRootRank)
						{
							EndgameTablebasesRootMove.ui32 = colossusMove.ui32;
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

		//// Did we fail to find the position in local EGTBs?
		//if (EndgameTablebasesRootMove.ui32 == 0)
		//{
		//	if (EndgameTablebasesPiecesRoot <= LichessEGTBMax) // Are we potentially in the Lichess EGTBs at this root position?
		//	{
		//		// Probe the Lichess EGTBs
		//		// N.B. this is totally impractical for hyper-bullet testing as the engine has moved on to the next move long before the query has returned!
		//		if (UseLichessEGTB)
		//		{
		//			if (!LichessEGTBProbing)
		//			{
		//				LichessQueries++;
		//				LichessEGTBProbing = true;
		//				LichessQueryFEN = RootFEN;
		//				//LichessQueryFEN = "4k3/6KP/8/8/8/8/7p/8_w_-_-_0_1";//TEMP
		//				std::thread LichessEGTBProbeThread;
		//				LichessEGTBProbeThread = std::thread(Normal::LichessEGTBProbe, LichessQueryFEN);
		//				LichessEGTBProbeThread.detach(); // Allow the launched 'LichessEGTBProbeThread' thread and this Normal engine thread to continue independently
		//			}
		//		}
		//	}
		//}
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
	PreviousIterationsMessages = "";
	CurrentIterationsMessages = "";
	LastTickCount = 1; // To avoid any divide by zero errors
	LastProgressMessageTickCount = 0;
	//SingularExtending = false;

	do
	{
		CRASHLOCATION(35);

		//// Has a Lichess query returned some results?
		//if (!LichessEGTBProbing) // Ensure any probe has completely finished
		//	if (LichessQueryFEN == RootFEN) // Ensure the query results are for the current position
		//		if (LichessMoves[0].move != "")
		//		{
		//			LichessQueriesUsed++;

		//			// N.B. the entire root movelist will have been set to loss/-1/-1 above
		//			// Just use the first (best) move returned
		//			// N.B. Lichess EGTB results always include a distance measure so we don't need to search multiple moves of the same wdl category
		//			std::string s = UpperCase(LichessMoves[0].move);
		//			Move_Struct m;
		//			m.mf.fromSquare = (SquaresEnum)((s[0] - 'A') + 8 * (s[1] - '1'));
		//			m.mf.toSquare = (SquaresEnum)((s[2] - 'A') + 8 * (s[3] - '1'));
		//			m.mf.flag = MFNormal;
		//			// N.B. there are no castling moves in EGTBs

		//			// Pawn promotion?
		//			if (s.length() > 4)
		//			{
		//				char promotionPiece;
		//				promotionPiece = s[4];
		//				switch (promotionPiece)
		//				{
		//				case 'Q':
		//					m.mf.flag = MFPromoteToQueen;
		//					break;
		//				case 'R':
		//					m.mf.flag = MFPromoteToRook;
		//					break;
		//				case 'B':
		//					m.mf.flag = MFPromoteToBishop;
		//					break;
		//				case 'N':
		//					m.mf.flag = MFPromoteToKnight;
		//					break;
		//				}
		//			}

		//			bool found = UpdateRootMoveEGTBStatus(m.ui32, 0, 1); // Pretend the move is a draw (so as not to get confusing EGTB 'special' scores!) in 1
		//			if (!found)
		//			{
		//				m.mf.flag = MFEnPassant; // Lazy EP fix :D
		//				bool found = UpdateRootMoveEGTBStatus(m.ui32, 0, 1);
		//				if (!found)
		//					OutputError("Did not find Lichess EGTB move in RootMoveList! " + MyUI64TOA(m.ui32));
		//			}
		//			if (found)
		//			{
		//				EndgameTablebasesRootMove.ui32 = m.ui32;
		//				EndgameTablebasesRootWDL = 0;
		//				EndgameTablebasesRootDTZ = 1;
		//			}

		//			LichessQueryFEN = ""; // Flag that we've used the move so that we don't redo this section every iteration
		//		}


		// Update iteration depth (ensuring it doesn't exceed maximum)
		if (IterationPly < MaximumIterationPly)
			IterationPly++;
		FullWidthPlies = (IterationPly + 2) / 4; // So... ID1-3 --> 0, ID4-11 --> 1, ID12-19 --> 2, etc

		isFollowingPV = (IterationPly > 1);

		// Set the aspiration window
		if (IterationPly == 1)
		{
			RootAlpha = -MatingIn0Score;
			RootBeta = MatingIn0Score;
		}
		else
		{
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

		RootBetaOld = RootBeta;
		RootFailHighs = 0;
		RootFailLows = 0;
		LastPrintTreePly = 1;
		CurrentIterationsPreviousRootBestMove.ui32 = 0;
#ifdef SEARCHINGFORLINE
		//TargetLineLength = 1;
		TargetLinePartial = "";
#endif

		//----------------------------------------------------------------------------------------------------

		ShowIterationStartMessage();

	retry:
		CRASHLOCATION(40);
		//PVMessageChecked = false;

		// Do the tree search
		// N.B. RootScore is relative to the side-to-move so e.g. if black is moving and mating this will a large +ve score
		RootScore = TreeSearchNormal(RootAlpha, RootBeta, 1, IterationPly, SideToMove, normalBrain.IsEnemyKingAttacked(GetLS1BIndex(normalBrain.piecesBB[SideToMove][King]), SideToMove ^ 1), false, false);

		CRASHLOCATION(50);

		if (!StopImmediately)
		{
			if (RootScore >= RootBeta) // Failed high? i.e. a root move returned a score >= beta (N.B. rootScore can actually be > beta because of fail-soft)
			{
				//if (RootFailLows > 0)
				//	OutputError("Fail-low-hi detected!");

				RootFailLows = 0;
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

				goto retry;
			}
			else if (RootScore <= RootAlpha) // Failed low? i.e. no root move took over as the new best
			{
				if (RootFailHighs > 0)
				{
					//OutputError("Fail-hi-low detected!");
					// Within this iteration we have failed high (at least once) and have now failed low so the RootBestMove is now in question so restore it to the last stable RootBestMove
					assert(CurrentIterationsPreviousRootBestMove.ui32 != 0);
					//RootBestMove = CurrentIterationsPreviousRootBestMove; // Seems to lose a few ELO
				}

				ShowFailedLowMessage(RootAlpha);
				RootFailHighs = 0;
				RootFailLows++; // For debugging purposes only
				RootAlpha = (short)(-MatingIn0Score);

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
			if (RootBestMove.ui32 == previousIterationsRootBestMove)
				ConsistentBestMoves++;
			else
				ConsistentBestMoves = 0;
			previousIterationsRootBestMove = RootBestMove.ui32;

			// Show diagnostics
			CRASHLOCATION(62);
			if (IsDebug)
			{
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
			if (TargetLinePartial != "")
				Output("IterationPly: " + std::to_string(IterationPly) + ", TargetLinePartial: " + TargetLinePartial + ", TargetLineRefutedBy: " + MoveNotation(TargetLineRefutedBy.ui32) + " (" + std::to_string(TargetLinePartialDepthRemaining) + ", " + std::to_string(TargetLinePartialThreateningMate) + ")\n");
#endif

			CRASHLOCATION(64);
		}

	} while ((!StopWhenIterationComplete && (ThreadId == 0)) || (!StopImmediately && (ThreadId > 0)));

	std::string bestMoveMessage = "";

	if (ThreadId == 0)
	{
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
				s += " " + std::to_string(EndgameTablebasesErrorCounts[count]);
			OutputError(s + "\n");
		}

		CRASHLOCATION(70);

		// Report best move
		if (RootBestMove.ui32 == 0)
			OutputError("No best move returned by search! (RootBestMove.ui32 == 0)");
		bestMoveMessage = "bestmove " + MoveNotation(RootBestMove.ui32);
		if (Ponder)
			if ((TC.CurrentType != TCTFixedTime) && (TC.CurrentType != TCTFixedDepth) && (TC.CurrentType != TCTFixedNodes)) // Don't ponder in any 'fixed' modes
				if ((uint16_t)normalBrain.gameRecordPointer->principalVariationPointer[1] > 0) // May be any of the PVT* terminators (which all have the bottom 16 bits set to 0)
					if (RootScore > -MatingIn0Score + 3) // Don't ponder if we're being mated in 1 else the GUI might give us the mated position and tell us to search it!
						bestMoveMessage += " ponder " + MoveNotation(normalBrain.gameRecordPointer->principalVariationPointer[1]);
		if (ShowBlankLines)
			bestMoveMessage += "\n";

		// Save any output from -FILE command for analysis in spreadsheet
		if (ProcessingCommandFile)
		{
			FILE *sw;
			fopen_s(&sw, "output.csv", "a+");
			std::string s = BestLine() + std::to_string(RootScore) + "," + MyUI64TOA(NodeCount + NodeCountQuiescenceSearch) + "," + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - StartClock).count());
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
	memcpy(ts->CounterMoveHistory, EngineNormal.CounterMoveHistory, sizeof(CounterMoveHistory_Struct));
	ts->ComputeNormal();
	delete ts;
}

void Normal::ComputeNormalMT()
{
	CRASHLOCATION(0);

	// If we've got some unused transposition table memory then allocate it (and clear it)
	if ((TranspositionTableMemory > 0) && (NormalTranspositionTableBuckets == 0))
		AllocateNormalTranspositionTable();

	CRASHLOCATION(1);

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
	std::string bestMoveMessage = EngineNormal.ComputeNormal();

	// Wait for helper threads to finish
	for (int threadId = 1; threadId < Threads; threadId++)
		threads[threadId].join();

	// Display best move found
	Output(bestMoveMessage);

	DisplayAnalysisCounters();

	CRASHLOCATION(9);
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
		OutputError("Exception occurred! " + MyUI64TOA(result) + " : CrashLocation=" + std::to_string(CrashLocation));
		exit(0);
	}
}
