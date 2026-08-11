#include <algorithm>
#include <chrono>
#include <assert.h>
//#include <iostream>
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
uint32_t Mate::MateTranspositionTableBuckets = 0;
uint32_t Mate::MateTranspositionTableBucketsMask;
bool Mate::MateSilent;

//const short FutilityMargin[9] = { 0,100,150,200,250,300,350,350,350 };
const short FutilityMargin[9] = { 0,50,100,150,200,250,300,300,300 };

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
void Mate::ClearMatingMoves()
{
	//for (int index = 0; index < MaximumPly; index++)
	//{
	//	KillerMoves[index].m1.ui32 = PVTUnknown;
	//	KillerMoves[index].m1.piece = 0;
	//	KillerMoves[index].m2.ui32 = PVTUnknown;
	//	KillerMoves[index].m2.piece = 0;
	//}

	memset(MatingMoves, 0, sizeof(MatingMoves));
}

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

std::string Mate::ThreadIdSuffix()
{
	if (Threads == 1)
		return "";
	return " ThreadId " + MyITOA(ThreadId);
}

void Mate::AddMessageToQueue(std::string message, bool lastMessageWasAProgressMessage)
{
#ifdef _DEBUG
	if (!MateSilent)
		Output(message);
#else
	MessageQueue[MessageQueueIndex++] = message;
	if (MessageQueueIndex == MessageQueueSize)
		MessageQueueIndex = 0;
	LastMessageWasAProgressMessage = lastMessageWasAProgressMessage;
	MessagesQueued = true;
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
	std::string IterationStartMessage = "info depth " + MyITOA(IterationPly)
		+ " seldepth " + MyITOA(MaximumPlyReached);
#ifndef _DEBUG
		if (IsDebug)
#endif
			IterationStartMessage += ThreadIdSuffix();
	AddMessageToQueue(IterationStartMessage, false);
}

void Mate::ShowProgressMessage(uint32_t move, int movesMade, short bestMoveScore, short alpha, short beta)
{
	uint64_t totalTickCount = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - StartClock).count() + 1; // +1 to avoid potential divide by zero on very fast computer!
	std::string ProgressMessage = "info time " + MyUI64TOA(totalTickCount)
		+ " nodes " + MyUI64TOA(NodeCount + NodeCountQuiescenceSearch)
		+ " currmove " + MoveNotation(move)
		+ " currmovenumber " + MyITOA(movesMade)
		;
#ifndef _DEBUG
	if (IsDebug)
#endif
	{
		ProgressMessage += " bestMoveScore " + MyITOA(bestMoveScore) + " alpha " + MyITOA(alpha) + " beta " + MyITOA(beta);
		ProgressMessage += ThreadIdSuffix();
		//ProgressMessage += " processor " + std::to_string(GetCurrentProcessorNumber());
	}
	if (LastMessageWasAProgressMessage)
		ReverseMessageQueueIndex();
	AddMessageToQueue(ProgressMessage, true);
}

void Mate::ShowFailedLowMessage(short rootAlpha)
{
	std::string FailedLowMessage = "info score cp " + MyITOA(rootAlpha) + " upperbound";
	AddMessageToQueue(FailedLowMessage, false);
}

void Mate::ShowIterationFinishMessage(uint32_t hashfull)
{
	uint64_t totalTickCount = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - StartClock).count() + 1; // +1 to avoid potential divide by zero on very fast computer!
	acs = totalTickCount;

	uint64_t totalNodes = NodeCount + NodeCountQuiescenceSearch;
	std::string IterationFinishMessage = "info time " + MyUI64TOA(totalTickCount)
		+ " nodes " + MyUI64TOA(totalNodes)
		+ " nps " + MyUI64TOA((totalNodes * 1000) / totalTickCount)
		+ " hashfull " + MyITOA(hashfull) + (EndgameTablebasesHits > 0 ? " tbhits " + MyUI64TOA(EndgameTablebasesHits) : "");
#ifndef _DEBUG
	if (IsDebug)
#endif
		IterationFinishMessage += ThreadIdSuffix();
	IterationFinishMessage += "\n";
	AddMessageToQueue(IterationFinishMessage, false);
}

void Mate::ShowQueuedMessages()
{
	// N.B. In compiler _DEBUG mode all messages are output as they occur so none will be queued
	for (int i = 0; i < MessageQueueSize; i++)
	{
		int index = (MessageQueueIndex + i) % MessageQueueSize;
		if (MessageQueue[index] != "")
		{
			if ((ThreadId == 0) || IsDebug)
				if (!MateSilent)
					Output(MessageQueue[index]);
			MessageQueue[index] = "";
		}
	}

	MessagesLastDisplayedClock = std::chrono::steady_clock::now();
	MessagesQueued = false;
}

std::string Mate::BestLine()
{
	std::string bestLine = "";

	int i = 0;
	do
	{
		bestLine += MoveNotation(PrincipalVariation[i++]) + " ";
	} while ((uint16_t)PrincipalVariation[i] != 0);

	return bestLine;
}

void Mate::ShowBestLineMessage(short alpha, uint8_t eul)
{
	uint64_t totalTickCount = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - StartClock).count() + 1; // +1 to avoid potential divide by zero on very fast computer!

	// Construct the PV
	std::string PVMessage = "";
	int i = 0;
	do
	{
		PVMessage += MoveNotation(PrincipalVariation[i++]) + " ";
	} while ((PrincipalVariation[i] & 0xFFFF) != 0);
	
	lastPV = PVMessage;
	trim(lastPV);

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
		case PVTFailedMateCondition:
			pvTerminatorMessage = "*FailedMateCondition";
			break;
		default:
			pvTerminatorMessage = "*Unknown PV terminator found!";
			break;
		}
	}

	// Final bits and bobs
	uint64_t totalNodes = NodeCount;// +NodeCountQuiescenceSearch;

	std::string scoreMessage = " score ";
	if (alpha >= MatingScore) // Mating?
	{
		scoreMessage += "mate " + MyITOA((MatingIn0Score - alpha) >> 1);
		bms += " " + MoveNotation(PrincipalVariation[0]);
	}
	else if (alpha <= MatedScore) // Mated?
		scoreMessage += "mate " + MyITOA((-MatingIn0Score - alpha + 1) >> 1);
	else // Mate
		scoreMessage += "cp " + MyITOA(alpha);

	std::string eulMessage = "";
	if (eul == 1)
		eulMessage = " lowerbound";
	else if (eul == 2)
		eulMessage = " upperbound";

	// Display the constructed message
	// N.B. the 'depth' value is provided here (as well as in the iteration 'start' message) as some GUIs (e.g. Arena, Shredder) don't display it unless it's provided with the PV!
	std::string BestLineMessage = "info depth " + MyITOA(IterationPly)
		+ " time " + MyUI64TOA(totalTickCount)
		+ " nodes " + MyUI64TOA(totalNodes) + scoreMessage + eulMessage
		+ " pv " + PVMessage + pvTerminatorMessage;
#ifndef _DEBUG
	if (IsDebug)
#endif
		BestLineMessage += ThreadIdSuffix();
	AddMessageToQueue(BestLineMessage, false);
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
			RootMoveList[index].nodes = NodeCount + NodeCountQuiescenceSearch - RootCumulativeNodeCount + 1; // +1 to ensure pruned root moves have count >=1
			assert(RootMoveList[index].nodes > 0);
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
		uint64_t highestNodes = 0;
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

	if (
		(RootScore >= (MatingIn0Score - TC.MateInN * 2) - 2)
		|| (IterationPly >= MaximumIterationPly)
		//|| (IterationPly >= TC.MateInN * 2)
		)
		//if ((RootScore >= WinningBaseScore))// || (IterationPly >= TC.MateInN * 2))
		StopWhenIterationComplete = true;
}

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

Move_Struct Mate::CanGiveMateInN(int N, int sideToMove, int isInCheck, int &checksCount)//THIS SHOULD BE IN BRAIN???
{
	Move_Struct result;
	result.ui32 = 0;
	MoveWithScore_Struct attackerMoveList[220];
	MoveWithScore_Struct defenderMoveList[220];
	Move_Struct attackerCurrentMove;
	Move_Struct defenderCurrentMove;

	mateBrain.CalculatePinnedPieces(sideToMove); // Required for legal move generation

	if (isInCheck) // Attacker in check?
	{
		int defenderKingSquare = BitScanForwardX(mateBrain.piecesBB[sideToMove ^ 1][King]);
		int attackerMovesCount = (int)(mateBrain.GenerateAllMovesOutOfCheck(sideToMove, attackerMoveList, true) - attackerMoveList);

		for (int moveListIndexIterator = 0; moveListIndexIterator < attackerMovesCount; moveListIndexIterator++)
		{
			attackerCurrentMove.ui32 = attackerMoveList[moveListIndexIterator].ui32;
			mateBrain.gameRecordPointer->move.ui32 = attackerCurrentMove.ui32;
			mateBrain.MakeMove(sideToMove); // N.B. MakeMove increments MateBrain.gameRecordPointer!

			bool givesCheck = mateBrain.IsEnemyKingAttacked(defenderKingSquare, sideToMove);
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
		uint32_t attackerMovesCount = (int)(mateBrain.GenerateAllChecks(sideToMove, attackerMoveList) - attackerMoveList);

		for (int moveListIndexIterator = 0; moveListIndexIterator < attackerMovesCount; moveListIndexIterator++)
		{
			attackerCurrentMove.ui32 = attackerMoveList[moveListIndexIterator].ui32;
			mateBrain.gameRecordPointer->move.ui32 = attackerCurrentMove.ui32;
			mateBrain.MakeMove(sideToMove); // N.B. MakeMove increments MateBrain.gameRecordPointer!

			mateBrain.CalculatePinnedPieces(sideToMove ^ 1); // Required for legal move generation

			bool anyMoves;

			if (N==1)
				anyMoves = mateBrain.AnyMoves(sideToMove ^ 1, true);
			else
			{
				anyMoves = false;
				int attackerKingSquare = BitScanForwardX(mateBrain.piecesBB[sideToMove][King]);
				uint32_t defenderMovesCount = mateBrain.GenerateAllMoves(sideToMove ^ 1, true, defenderMoveList);
				if (defenderMovesCount <= 1)
				{
					for (int moveListIndexIterator = 0; moveListIndexIterator < defenderMovesCount; moveListIndexIterator++)
					{
						defenderCurrentMove.ui32 = defenderMoveList[moveListIndexIterator].ui32;
						mateBrain.gameRecordPointer->move.ui32 = defenderCurrentMove.ui32;
						mateBrain.MakeMove(sideToMove ^ 1); // N.B. MakeMove increments MateBrain.gameRecordPointer!

						bool givesCheck = mateBrain.IsEnemyKingAttacked(attackerKingSquare, sideToMove ^ 1);

						Move_Struct m = CanGiveMateInN(N - 1, sideToMove, givesCheck, checksCount);

						mateBrain.UnMakeMove(sideToMove ^ 1); // N.B. UnMakeMove decrements MateBrain.gameRecordPointer!

						anyMoves = (m.ui32 == 0);
						if (anyMoves)
							break;
					}
				}
				else
					anyMoves = true;

			}

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
//	MoveWithScore_Struct moveList[220];
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
	for (uint32_t bucket = 0; bucket < MateTranspositionTableBuckets; bucket++)
	{
		for (uint32_t entry = 0; entry < MateTranspositionTableEntriesPerBucket; entry++)
		{
			MateTranspositionTablePointer[bucket].Entries[entry].hash64 = 0; // Setting the hash to zero doesn't really 'clear' it (because it's a valid value) but it's useful for visual debugging!
			uint64_t newData = (((uint64_t)TTFlagUpper) << 48) | (((uint64_t)((uint8_t)0)) << 56); // flag=TTFlagUpper, subTreeDepth=0
			MateTranspositionTablePointer[bucket].Entries[entry].data = newData;

		}
	}
}

__declspec(noinline)
void Mate::AllocateMateTranspositionTable()
{
	assert(sizeof(MateTranspositionTableBucket_Struct) == 64);
	assert(sizeof(MateTranspositionTableEntry_Struct) == 16);

	// Calculate the largest 'power of 2' number of entries that will fit in the specified number of bytes
	MateTranspositionTableBuckets = 1;
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
			OutputError("Mate transposition table memory could not be allocated!");
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

uint32_t Mate::HashfullMateTranspositionTable()
{
	if (MateTranspositionTableBuckets == 0)
		return 0;

	// Computes the UGI Hashfull value
	// Assuming an even distribution of used entries across the entire table a fairly accurate estimate can be made by examining a small subset of entries
	// Even with the smallest possible transposition table (1MB) we would still have 16384 buckets
	// Examining exactly 1000 entries avoids any scaling maths on return
	uint32_t usedEntries = 0;
	uint32_t bucketsToTry = 1000 / MateTranspositionTableEntriesPerBucket;
	for (uint32_t bucket = 0; bucket < bucketsToTry; bucket++)
		for (uint32_t entry = 0; entry < MateTranspositionTableEntriesPerBucket; entry++)
			if ((uint8_t)(MateTranspositionTablePointer[bucket].Entries[entry].data >> 56) != 0)
				usedEntries++;
	//return (uint32_t)((usedEntries * 1000) / (bucketsToTry * MateTranspositionTableEntriesPerBucket));
	return usedEntries;
}

void Mate::AddToMateTranspositionTable(int8_t depthRemaining, short ply, short score, uint8_t flag, uint32_t bestMove, short tteStaticEvaluation)
{
	if (MateTranspositionTableBuckets > 0)
		if (ply < TC.MateInN * 2 - 1)//SHOULD ALWAYS BE TRUE???
		{
			MateTranspositionTableEntry_Struct* tte0;
			uint64_t hash64 = mateBrain.gameRecordPointer->transpositionTableHash64WithEP;
			tte0 = (MateTranspositionTableEntry_Struct*)(MateTranspositionTablePointer + (hash64 & MateTranspositionTableBucketsMask));

			// Find candidate entry for replacement
			int entryToReplace;
			int shallowestSubTreeDepth = 999;
			uint8_t flagEUL = flag & TTFlagEULMask;
			for (int entry = 0; entry < MateTranspositionTableEntriesPerBucket; entry++) // Do we already have this position in the table?
			{
				uint64_t tteHash = tte0[entry].hash64;
				uint64_t tteData = tte0[entry].data;
				int8_t tteSubTreeDepth = (uint8_t)((tteData >> 56) & subTreeDepthMask);

				if ((tteHash ^ tteData) == hash64) // Do we already have this position in the bucket?
				{
					shallowestSubTreeDepth = 0;
					entryToReplace = entry;
					break;
				}
				else if (tteSubTreeDepth < shallowestSubTreeDepth)
				{
					shallowestSubTreeDepth = tteSubTreeDepth;
					entryToReplace = entry;
				}
			}
			assert(entryToReplace < MateTranspositionTableEntriesPerBucket);

			uint64_t ttetrHash = tte0[entryToReplace].hash64;
			uint64_t ttetrData = tte0[entryToReplace].data;
			uint64_t ttetrFlag = (ttetrData >> 48);
			uint8_t ttetrFlagEUL = (ttetrFlag & TTFlagEULMask);
			uint8_t ttetrAge = (ttetrFlag & TTFlagAgeMask);
			short ttetrScore = (uint16_t)((ttetrData >> 16) & scoreMask);
			uint8_t ttetrPlyToCeiling = (uint16_t)((ttetrData >> 40) & plyToCeilingMask);

			if (
				(depthRemaining >= shallowestSubTreeDepth)
				|| ((score >= EGTBWinningScore) && (flagEUL != TTFlagUpper)) // Prefer 'winning' scores ONLY DO IF ROOT SCORE>=WINNING
				|| (flagEUL == TTFlagExact)
				)
			{
				if (!
					(
					(ttetrHash ^ ttetrData == hash64) // Same position?
						&& (flagEUL != TTFlagUpper) // Cut or exact?
						&& (
						((ttetrScore == EGTBWinningScore) && (score < EGTBWinningScore))
							|| ((ttetrScore >= MatingScore) && (ttetrScore > score + ply)) // Winning?
							)
						)
					)

					// 'Correct' any mate scores for distance (because they are relative to the root position not to this position)
					if (score >= MatingScore)
					{
						score += ply;
						if ((flag & TTFlagEULMask) != TTFlagUpper)
						{
							// If we have a mate at an 'exact' or 'cut' node then set its depthRemaining to at least 1. Useful in e.g. KRvKN and KBNvK as it helps with the ever increasing mate distance problem
							depthRemaining = std::max(depthRemaining, (int8_t)1);

							if (score == MatingIn0Score - 1) // If we have a #1 from here (15999) then ensure the flag is 'exact' as it can't be improved on!
								flag = flag & ~TTFlagEULMask;
						}
					}
					else if (score <= MatedScore)
					{
						score -= ply;
						flag |= TTFlagThreatenedWithMate;
					}

				uint64_t newData = (uint64_t)MGCompressMove(bestMove) | (((uint64_t)((uint16_t)score)) << 16) | (((uint64_t)(TranspositionTableAge | flag)) << 48) | (((uint64_t)((uint8_t)depthRemaining)) << 56) | (((uint64_t)((uint8_t)((TC.MateInN * 2) - ply))) << 40);
				tte0[entryToReplace].data = newData;
				tte0[entryToReplace].hash64 = hash64 ^ newData;
			}
		}
}

#pragma endregion

//----------------------------------------------------------------------------------------------------

short Mate::TreeSearchMate(short alpha, short beta, int ply, int depthRemaining, int sideToMove, int isInCheck, int freeMoves)//, bool allowNull, bool isCutNode, int currentLineExpense)
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
	assert(-MatingIn0Score <= alpha && alpha < beta && beta <= MatingIn0Score);
	//assert((alpha < MatingScore) || (ply <= (MateBaseScore - alpha)));THIS FAILS... WHY???

	//----------------------------------------------------------------------------------------------------

	// Preamble

	// Stopping? (N.B. Must do this here as well as below in the main move processing loop else it may go back up the tree after a reduced search!)
	if (StopImmediately)
		return -MatingIn0Score;

	if (ply > MaximumPlyReached)
	{
		MaximumPlyReached = ply;
		if (IsDebug)
			LongestLine = mateBrain.CurrentLine(ply - 1) + " (Iteration:" + MyITOA(IterationPly) + " Ply:" + MyITOA(ply - 1) + " Alpha:" + MyITOA(alpha) + " Beta:" + MyITOA(beta) + ")";
		//Output("MaximumPlyReached=" + MyITOA(MaximumPlyReached));
		//Output(LongestLine);
	}

	//----------------------------------------------------------------------------------------------------

	if (ply & 1)
		if (mateBrain.gameRecordPointer->totalMaterial[sideToMove] < (TC.MateMinimumAttackerMaterial * 100))
		{
			*mateBrain.gameRecordPointer->principalVariationPointer = PVTFailedMateCondition;
			return -EGTBWinningScore;
		}


	int standPatScore = mateBrain.gameRecordPointer->totalMaterial[sideToMove] - mateBrain.gameRecordPointer->totalMaterial[sideToMove ^ 1];
	int kingDistance = ManhattanDistance[BitScanForwardX(mateBrain.piecesBB[0][King])][BitScanForwardX(mateBrain.piecesBB[1][King])];
	if (ply & 1)
		standPatScore -= kingDistance; // Give small bonus for the attacking K approaching the defending K
	else
		standPatScore += kingDistance;

	int checksCount = 999; // WHAT IS THIS FOR??? NOT USED NOW
	int freeMovesDelta = 0;//NOT USED???

	//----------------------------------------------------------------------------------------------------

	// Leaf nodes
	if (ply > 1) // Don't do leaf node tests at the root because we want the main body of the search to display status/result messages
		if ((ply >= (TC.MateInN * 2) - 1) || (depthRemaining <= 0)) // Leaf node?
		{
			// N.B. this section always returns a result without going into the main body of the search

			if (ply & 1) // At an odd ply? (i.e. the attacker is to move)
			{
				//if ((ply < (TC.MateInN * 2) - 1) && isInCheck) // Don't allow leaf node if attacker in check EXPLODES!
				//	depthRemaining = 2;
				//else
				{
					// If we are at the penultimate ply or have no search depth remaining then we must deliver mate in 1 here otherwise we assume it's a non-mating position

					// Can the attacker mate the defender immediately this move?
					Move_Struct tmi1 = CanGiveMateInN(1, sideToMove, isInCheck, checksCount);
					//Move_Struct tmi1 = CanGiveMateInN(((TC.MateInN * 2) - ply + 1) / 2, sideToMove, isInCheck, checksCount);
					if (tmi1.ui32)
					{
						*mateBrain.gameRecordPointer->principalVariationPointer = tmi1.ui32; // Return mating move as part of pv
						*(mateBrain.gameRecordPointer->principalVariationPointer + 1) = PVTCheckmate;

						MatingMoves[ply] = tmi1;

						return (MatingIn0Score - ply - 1);
					}

					*mateBrain.gameRecordPointer->principalVariationPointer = PVTStandPat;
					return standPatScore;
				}
			}
			else // Even ply
			{
				// If we don't have any draft left, return the material balance unless the defender is mated
				if (isInCheck)
				{
					// This only occurs at MD*2 because the previous move (which gave check) gets extended so DR will be >0
					mateBrain.CalculatePinnedPieces(sideToMove); // Required for legal move generation
					if (!mateBrain.AnyMoves(sideToMove, true))
					{
						// Checkmated at the final ply
						*mateBrain.gameRecordPointer->principalVariationPointer = PVTCheckmate;
						return (short)(-MatingIn0Score + ply);
					}
				}

				*mateBrain.gameRecordPointer->principalVariationPointer = PVTStandPat;
				return standPatScore;
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
			short ds = EGTBWinningScore / 2;

			if ((ply & 1) == 0)
			{
				if (ds > alpha) // Immediate repetition possible?
				{
					if (
						(((mateBrain.gameRecordPointer - 1)->move.mf.fromSquare) == ((mateBrain.gameRecordPointer - 3)->move.mf.toSquare))
						&& (((mateBrain.gameRecordPointer - 1)->move.mf.toSquare) == ((mateBrain.gameRecordPointer - 3)->move.mf.fromSquare))
						) // Did the opponent just undo his previous move?
					{
						if (ds >= beta)
						{
							*mateBrain.gameRecordPointer->principalVariationPointer = PVTDrawImmediateRepetition;
							return ds;
						}
					}
				}
			}

			if (pliesSinceIrreversible >= 4)
			{
				for (int i = 4; i <= pliesSinceIrreversible; i += 2) // Repetition?
					if ((mateBrain.gameRecordPointer - i)->transpositionTableHash64 == mateBrain.gameRecordPointer->transpositionTableHash64)
					{
						*mateBrain.gameRecordPointer->principalVariationPointer = PVTDrawByRepetition;
						short result = ds;
						if (ply & 1)
							result = -ds;
						return result;
					}

				if ((pliesSinceIrreversible >= 100) || (pliesSinceIrreversible >= TC.MateMaximumReversibleMoves)) // 50-move?
				{
					*mateBrain.gameRecordPointer->principalVariationPointer = PVTDrawBy50MoveRule;
					short result = ds;
					if (ply & 1)
						result = -ds;
					return result;
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
		beta = std::min(MatingIn0Score - ply - 1, (int)beta); // If the best possible score for the side to move in this position (i.e. giving mate in 1) < beta, then decrease beta
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

	// Initialisation
	MoveWithScore_Struct moveList[220];
	short bestMoveScore = -MatingIn0Score; // If anything takes over as best (a 'pv' or 'cut' node) then bestMoveScore will be equal to alpha. If nothing takes over as best (an 'all' node) then bestMoveScore will be less than alpha and will be a more accurate upper bound.
	short originalAlpha = alpha;
	int legalMovesMade;
	Move_Struct currentMove;
	short currentMoveScore;
	GameRecordEntry_Struct* currentGameRecordPointer = mateBrain.gameRecordPointer;

	//----------------------------------------------------------------------------------------------------

	// Up-date tree search variables
	currentGameRecordPointer->isInCheck = isInCheck;
	currentGameRecordPointer->isTWM = 0; // These may get set if we find a TT entry
	currentGameRecordPointer->isO1M = 0;
	currentGameRecordPointer->isFMTP = 0;
	currentGameRecordPointer->isO1PCM = 0;
	currentGameRecordPointer->isZLKM = 0;
	*currentGameRecordPointer->principalVariationPointer = PVTUnknown; // Terminator

	//----------------------------------------------------------------------------------------------------

#pragma region TT
	// Transposition tables in mate mode are tricky because we use a ceiling at MD*2
	// So say we were looking for a #7...
	// if we took 3 moves to get to position X which is a #5 and all the moves were forced and they extended, we still wouldn't find the #5 because we would hit the ceiling first and thus a draw gets put in the TT
	// if we then took 2 moves to get to position X (with the same draft) we would use the draw from the TT rather than searching and this time finding the #5!
	// We therefore have to test the draft AND the distance to the ceiling (BUT ONLY FOR DRAWS... NOT FOR MATES???)

	// JUST USE POSNS FROM SAME DEPTH WITHIN SAME ITER? WITH SAME DEPTH AND DR?
	// EXCEPT WINNING SCORE POSNS?
	// ALWAYS KEEP/USE MATING POSNS? (SET DR TO +INF?)
	// CAN ALWAYS USE MATING SCORES BUT DRAW SCORES MAY BE FAULTY SO NEED TO CHECK DEPTH/DR/WHATEVER AS WELL

	// Is this position in the tranposition table? (>50% of the time even with a modest table)
	MateTranspositionTableEntry_Struct* tte0;
	Move_Struct tteBestMove;
	tteBestMove.ui32 = 0;
	int8_t tteSubTreeDepth = 0;
	int8_t ttePlyToCeiling = 0;
	uint8_t tteEUL = 0;
	short tteScore = 0;
	//if (0)
	if ((MateTranspositionTableBuckets > 0) && (ply > 1)) // ply will always be < TC.MateInN * 2 - 1
	{
		uint64_t hash64 = mateBrain.gameRecordPointer->transpositionTableHash64WithEP;
		tte0 = (MateTranspositionTableEntry_Struct*)(MateTranspositionTablePointer + (hash64 & MateTranspositionTableBucketsMask));

		for (int entry = 0; entry < MateTranspositionTableEntriesPerBucket; entry++) // Do we already have this position in the table?
		{
			uint64_t data = tte0[entry].data;
			uint64_t hash = tte0[entry].hash64 ^ data;

			if (hash == hash64)
			{
				// Get the entry's data
				tteBestMove.ui32 = MGUnCompressMove((uint16_t)(data & bestMoveMask));
				assert((tteBestMove.ui32 == 0) == (((uint16_t)tteBestMove.ui32) == 0));
				tteSubTreeDepth = (int8_t)((data >> 56) & subTreeDepthMask);
				ttePlyToCeiling = (int8_t)((data >> 40) & plyToCeilingMask);
				uint8_t flag = (uint8_t)((data >> 48) & flagMask);
				uint8_t tteEUL = (flag & TTFlagEULMask);
				short tteScore;
				tteScore = (short)((data >> 16) & scoreMask);

				if (abs(tteScore) >= EGTBWinningScore)
				{
					if (tteScore >= EGTBWinningScore) // A 'winning' score is a lower bound
					{
						if (tteScore >= MatingScore)
							tteScore -= ply;

						//if (tteEUL != TTFlagUpper)
						//{
						//	if ((tteScore >= beta) || (tteScore == MateBaseScore - 1 - ply))
						//	{
						//		tteSubTreeDepth = MaximumPly; // We have a winning score that will cause a cutoff (or can't be improved on) so use it regardless of depthRemaining
						//		ttePlyToCeiling = MaximumPly;
						//	}
						//}
					}
					else // A 'losing' score is an upper bound
					{
						if (tteScore <= MatedScore)
							tteScore += ply;

						//if (tteEUL == TTFlagUpper)
						//{
						//	if (tteScore <= alpha)
						//	{
						//		tteSubTreeDepth = MaximumPly;
						//		ttePlyToCeiling = MaximumPly;
						//	}
						//}
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
									PRINTTREE(PrintTree(IterationPly, ply, alpha, beta, depthRemaining, -4, tteScore, 0););
									return tteScore; // We can exit because we know that at least one move will exceed current beta
								}
							}
							else if (tteEUL == TTFlagUpper) // Upper limit? (Came from an All node: exact value is "at most" (<=) this value)
							{
								if (tteScore <= alpha)
								{
									PRINTTREE(PrintTree(IterationPly, ply, alpha, beta, depthRemaining, -3, tteScore, 0););
									return tteScore; // We can exit because we know that no move will exceed current alpha
								}
							}
							else // Exact value? (Came from a PV node)
							{
								PRINTTREE(PrintTree(IterationPly, ply, alpha, beta, depthRemaining, -2, tteScore, 0););
								if (tteBestMove.ui32 != 0) // Sometimes there won't be a move stored as it might be a checkmate/stalemate/DBR position
								{
									*mateBrain.gameRecordPointer->principalVariationPointer = tteBestMove.ui32; // Return best move as part of pv
									*(mateBrain.gameRecordPointer->principalVariationPointer + 1) = PVTTTExact;
								}
								return tteScore; // We can exit because we have an exact value
							}
						}
					}

				break;
			}
		}
	}
#pragma endregion

	//----------------------------------------------------------------------------------------------------

	if ((mateBrain.KnownLowMaterialDraws(sideToMove) == PVTDrawMinimumMaterial) || ((ply & 1) && (mateBrain.piecesBB[sideToMove][AllPieces] == mateBrain.piecesBB[sideToMove][King])))
	{
		*currentGameRecordPointer->principalVariationPointer = PVTDrawMinimumMaterial;
		short result = EGTBWinningScore / 2;
		if (ply & 1)
			result = -result;
		return result;
	}

	//----------------------------------------------------------------------------------------------------

	// Testing for a quick win can improve solution times (AND SLOW DOWN TOO!)
	if (ply > 1)
		if (ply & 1) // At an odd ply? (i.e. the attacker is to move)
			if (!isInCheck)
			{
				// Testing for an immediate mate speeds up the search
				Move_Struct tmi1 = CanGiveMateInN(1, sideToMove, isInCheck, checksCount);
				if (tmi1.ui32)
				{
					*currentGameRecordPointer->principalVariationPointer = tmi1.ui32; // Return mating move as part of pv
					*(currentGameRecordPointer->principalVariationPointer + 1) = PVTCheckmate;
					return (MatingIn0Score - ply - 1);
				}
			}

	// Are we still following a 'forcing' line from the root?
	// N.B. as soon as we step off of a forcing line it never gets set to true for the remainder of the line
	currentGameRecordPointer->forcingLine = (currentGameRecordPointer - 2)->forcingLine & (currentGameRecordPointer - 2)->forcingMove;
	currentGameRecordPointer->isThreateningMateInOne.ui32 = 0;

	//----------------------------------------------------------------------------------------------------
	//
	//// Null move (at even plies) can improve solution times
	//short nullMovePrunePending = -MateBaseScore;
	////if (0)//TURNED OFF AS WE DO TMIn BELOW NOW
	////SOMETIMES THIS HELPS SPEED THINGS UP A LOT AND OTHER TIMES SLOWS IT DOWN A LOT
	//if ((ply & 1) == 0)
	//	if (!isInCheck)
	//		//if (!(currentGameRecordPointer - 1)->forcingLine)
	//		if (!(currentGameRecordPointer - 1)->forcingMove)
	//			if (depthRemaining > 2)
	//				if (depthRemaining < (TC.MateInN * 2) - 2)
	//	{
	//			
	//			
	//			if ((currentGameRecordPointer - 1)->forcingLine)
	//				AC8++;


	//		// Make null move
	//		currentGameRecordPointer->move.ui32 = NullMove;
	//		currentGameRecordPointer->move.fromSquarePiece = Pawn; // Ensure CMH treats all previous null moves as Px0
	//		currentGameRecordPointer->move.toSquarePiece = Empty; // Ensure recapture extensions don't mistakenly kick in

	//		mateBrain.gameRecordPointer++; // Normally done in make/unmake-move
	//		mateBrain.gameRecordPointer->castlingStatus = (mateBrain.gameRecordPointer - 1)->castlingStatus;
	//		mateBrain.gameRecordPointer->pliesSinceIrreversible = 0; // Don't allow DBRs across a null move (+3 ELO) // (NormalGenerate.gameRecordPointer - 1)->pliesSinceIrreversible + 1;
	//		mateBrain.gameRecordPointer->transpositionTableHash64 = ~(mateBrain.gameRecordPointer - 1)->transpositionTableHash64;
	//		mateBrain.gameRecordPointer->transpositionTableHash64WithEP = mateBrain.gameRecordPointer->transpositionTableHash64;
	//		mateBrain.gameRecordPointer->epSquare = 0;
	//		*(uint32_t*)(&mateBrain.gameRecordPointer->totalMaterial[0]) = *(uint32_t*)(&(mateBrain.gameRecordPointer - 1)->totalMaterial[0]); // N.B. Using data type overload at start of line to copy for both sides! DOUBLE CHECK THIS WORKS!!!!
	//		*(uint64_t*)(&mateBrain.gameRecordPointer->gamePhase[0]) = *(uint64_t*)(&(mateBrain.gameRecordPointer - 1)->gamePhase[0]);
	//		*(uint32_t*)(&mateBrain.gameRecordPointer->totalOpeningPST[0]) = *(uint32_t*)(&(mateBrain.gameRecordPointer - 1)->totalOpeningPST[0]);
	//		*(uint32_t*)(&mateBrain.gameRecordPointer->totalEndgamePST[0]) = *(uint32_t*)(&(mateBrain.gameRecordPointer - 1)->totalEndgamePST[0]);
	//		PRINTTREE(PrintTree(IterationPly, ply, alpha, beta, depthRemaining, -1, -999, currentGameRecordPointer->staticEvaluation);)

	//			//int R = std::max(3, ((depthRemaining + 1) >> 1));
	//			int R = 0;// (depthRemaining / 5) + ((currentGameRecordPointer->staticEvaluation - beta) / 128) + (!isPVNode * 3);
	//		//R = -(TC.MateInN * 2 - ply);
	//		//int R = 2;
	//		//if (currentGameRecordPointer->gamePhase[sideToMove] == 0)
	//		//	R = 3;
	//		//if (beta <= LosingBaseScore)
	//		//	R = 2;
	//		//assert(R >= 0);
	//		//short nullMoveScore = (short)-TreeSearchMate((short)-beta, (short)(-beta + 1), ply + 1, depthRemaining - R - 1, sideToMove ^ 1, false, freeMoves);
	//		//short nullMoveScore = (short)-TreeSearchMate((short)-beta, (short)(-beta + 1), ply + 1, 0, sideToMove ^ 1, false, freeMoves);
	//		short nullMoveScore = (short)-TreeSearchMate((short)-beta, (short)(-beta + 1), ply + 1, depthRemaining - 2 - 1, sideToMove ^ 1, false, freeMoves);
	//		//short nullMoveScore = (short)-TreeSearchMate((short)(MatingScore - 1), (short)MatingScore, ply + 1, 2, sideToMove ^ 1, false, freeMoves);
	//		// About 89% of nodes after a null move are 'all' nodes

	//		// Unmake null move
	//		mateBrain.gameRecordPointer--;

	//		if (nullMoveScore >= beta)
	//		{
	//			//*currentGameRecordPointer->principalVariationPointer = PVTStandPat;
	//			//return nullMoveScore;
	//			nullMovePrunePending = nullMoveScore;
	//		}

	//		else if (nullMoveScore <= MatedScore)
	//		{
	//			(currentGameRecordPointer - 1)->forcingMove = true;
	//			(currentGameRecordPointer - 1)->forcingLine = (currentGameRecordPointer - 3)->forcingLine; // Update the 'forcingLine' status
	//			currentGameRecordPointer->isTWM = TTFlagThreatenedWithMate;
	//		}
	//	}

	//----------------------------------------------------------------------------------------------------

	int defenderSpiteChecksSaved = defenderSpiteChecks;//NOT USED???

	//----------------------------------------------------------------------------------------------------

	// Generate move list
	mateBrain.CalculatePinnedPieces(sideToMove); // Required for legal move generation

	int movesCount;
	movesCount = mateBrain.GenerateAllMoves(sideToMove, isInCheck, moveList);

	//----------------------------------------------------------------------------------------------------

	if (movesCount == 0) // No legal moves generated?
	{
		assert((tteBestMove.ui32 == PVTUnknown) || (tteBestMove.ui32 == PVTStandPat) || (tteBestMove.ui32 == PVTEGTB));

		// Is the side to move in check?
		if (isInCheck)
		{ // Checkmate
			//assert(IsMated(sideToMove));
			assert(tteBestMove.ui32 == PVTUnknown);
			*currentGameRecordPointer->principalVariationPointer = PVTCheckmate;
			return (short)(-MatingIn0Score + ply);
		}
		else
		{ // Stalemate
			*currentGameRecordPointer->principalVariationPointer = PVTDrawStalemate;
			short result = EGTBWinningScore / 2;
			if (ply & 1)
				result = -result;
			return result;
		}
	}

	//----------------------------------------------------------------------------------------------------

	if (movesCount == 1)
	{
		currentGameRecordPointer->isO1M = TTFlagOnlyOneLegalMove;
		currentGameRecordPointer->isO1PCM = TTFlagOnlyOnePieceCanMove;
		(currentGameRecordPointer - 1)->forcingMove = true; // Set the previous move to be 'forcing'
		(currentGameRecordPointer - 1)->forcingLine = (currentGameRecordPointer - 3)->forcingLine; // Update the 'forcingLine' status
	}
	//else if (nullMovePrunePending != -MateBaseScore)
	//	return nullMovePrunePending;


	//----------------------------------------------------------------------------------------------------
	
	int reductionsFixedPieces = 0;
	int defenderKingMoves = 0;
	if ((ply & 1) == 0) // At even ply? (Defender's move)
	{
		// Calculate the number of king moves, whether only one piece can move and whether fixed pieces have been released
		int defenderKingSquare = BitScanForwardX(mateBrain.piecesBB[sideToMove][King]);
		int defenderMoveablePieces = 0;
		uint64_t defenderMoveablePiecesBB = 0;

		for (int moveListIndexIterator = 0; moveListIndexIterator < movesCount; moveListIndexIterator++)
		{
			uint64_t fromSquareBB = UINT64SetBit(moveList[moveListIndexIterator].mf.fromSquare);

			if (currentGameRecordPointer->fixedPiecesDefenderBB & fromSquareBB) // Have we allowed a defender's 'fixed' piece (specified in the MateFixedPieces option) to move?
			{
				*currentGameRecordPointer->principalVariationPointer = PVTFailedMateCondition;
				return EGTBWinningScore;
			}

			if (moveList[moveListIndexIterator].mf.fromSquare == defenderKingSquare)
				defenderKingMoves++;

			defenderMoveablePiecesBB |= UINT64SetBit(moveList[moveListIndexIterator].mf.fromSquare);
		}
		
		defenderMoveablePieces = PopulationCountX(defenderMoveablePiecesBB);

		assert(defenderKingMoves <= 8);
		assert(defenderMoveablePieces <= 16);
		assert(movesCount <= 218);

		if (isPVNode)
		//if (isPVNode || (IterationPly==2))
			//if (ply <= IterationPly) DO NOT PUT THIS BACK... IT MAKES THE AUTO-TUNING TOO RESTRICTIVE
			{
				if (defenderKingMoves > maximumDefenderKingMovesFound)
					maximumDefenderKingMovesFound = defenderKingMoves;
				if (defenderMoveablePieces > maximumDefenderMovablePiecesFound)
					maximumDefenderMovablePiecesFound = defenderMoveablePieces;
				if (movesCount > maximumDefenderMovesFound)
					maximumDefenderMovesFound = movesCount;
			}

		if (defenderKingMoves > mateMaximumDefenderKingMoves)
		{
			if (isPVNode)
				//mateMaximumDefenderKingMoves = defenderKingMoves;
				;
			else
			{
				*currentGameRecordPointer->principalVariationPointer = PVTFailedMateCondition;
				return EGTBWinningScore;
				//depthRemaining--;
			}
		}
		if (defenderMoveablePieces > mateMaximumDefenderMovablePieces)
		{
			if (isPVNode)
				//mateMaximumDefenderMovablePieces = defenderMoveablePieces;
				;
			else
			{
				*currentGameRecordPointer->principalVariationPointer = PVTFailedMateCondition;
				return EGTBWinningScore;
				//depthRemaining--;
			}
		}

		if (movesCount > mateMaximumDefenderMoves)
		{
			if (isPVNode)
				//mateMaximumDefenderMoves = movesCount;
				;
			else
			{
				*currentGameRecordPointer->principalVariationPointer = PVTFailedMateCondition;
				return EGTBWinningScore;
				//depthRemaining--;
			}
		}

		currentGameRecordPointer->DefenderKingMovesBefore = defenderKingMoves;
		currentGameRecordPointer->TotalDefenderKingMovesBefore = (currentGameRecordPointer - 2)->TotalDefenderKingMovesBefore + defenderKingMoves;
		currentGameRecordPointer->isZLKM = (defenderKingMoves == 0);
		currentGameRecordPointer->isOKCM = (defenderKingMoves == movesCount);
		currentGameRecordPointer->isO1PCM = (defenderMoveablePieces == 1);



		// SHOULDN'T THIS ALL BE DONE WITH NULL MOVE ABOVE????
		// N.B. ONLY DOES TWM TEST IF ON A FORCING LINE! AND PV! SO WON'T FIND IT IN THAT #8 c3 PROBLEM AS IT'S TWM LATER IN THE CORRECT LINE AND IT WON'T BE PV EITHER!!!

		// NOW THAT WE HAVE KICKED OUT PARAMETER PRUNED MOVES AND DETERMINED O1M ETC, DO THE TWM TEST ONLY IF NECC!
		//UNLIKE CHECKS, TWMs CAN PERSIST SO WE NEED SO WAY TO LIMIT 'THE SAME' TWM ... COMPARE isThreateningMateInOne.ui32 ??? else that #39 explodes!
		//setif (0)
		if (!isInCheck & (movesCount > 1)) // *STILL* EXPLODES SOME POSNS E.G. 5R2/1pB3p1/2pP2P1/3p1p2/pr1P1NPk/qN3p1P/3P2p1/1Kn2r1R w - - 0 1 - also that #50 - *BUT* helps speed up go mate 6 file
			//if ((currentGameRecordPointer - 2)->isTWM == TTFlagThreatenedWithMate)//OR TEST FORCINGLINE??? OR ONLY DO AT PV NODES??? ONLY DO IF ANOTHER CONDITION LIKE O1PCM/ZLKM
				//if (isPVNode)
				{
					(currentGameRecordPointer - 1)->isThreateningMateInOne = mateBrain.ThreateningMateInOneWithNull(sideToMove ^ 1, checksCount);
					if ((currentGameRecordPointer - 1)->isThreateningMateInOne.ui32 != 0)
					{
						(currentGameRecordPointer - 1)->forcingMove = true;
						currentGameRecordPointer->isTWM = TTFlagThreatenedWithMate;
						if ((currentGameRecordPointer - 1)->isThreateningMateInOne.ui32 != (currentGameRecordPointer - 3)->isThreateningMateInOne.ui32)
							(currentGameRecordPointer - 1)->forcingLine = (currentGameRecordPointer - 3)->forcingLine; // Update the 'forcingLine' status
					}
				}


		//if (!currentGameRecordPointer->isO1PCM)
		//	if (!currentGameRecordPointer->isZLKM)
		//	if (!(currentGameRecordPointer - 3)->forcingLine)
		//		if (!(currentGameRecordPointer - 1)->forcingMove)
		//		if (nullMovePrunePending != -MateBaseScore)
		//			return nullMovePrunePending;






		// Null move (at even plies) can improve solution times
		short nullMovePrunePending = -MatingIn0Score;
		//if (0)//TURNED OFF AS WE DO TMIn BELOW NOW
		//SOMETIMES THIS HELPS SPEED THINGS UP A LOT AND OTHER TIMES SLOWS IT DOWN A LOT
		//if ((ply & 1) == 0)
			if (!isInCheck)
				//if (!(currentGameRecordPointer - 1)->forcingLine)
				if (!(currentGameRecordPointer - 1)->forcingMove)
					if (!currentGameRecordPointer->isZLKM)
						if (!currentGameRecordPointer->isO1PCM)
							if (!currentGameRecordPointer->isO1M)//NOT NECC
					if (depthRemaining > 2)
						if (depthRemaining < (TC.MateInN * 2) - 2)
						{


							if ((currentGameRecordPointer - 1)->forcingLine)
								AC8++;


							// Make null move
							currentGameRecordPointer->move.ui32 = NullMove;
							currentGameRecordPointer->move.fromSquarePiece = Pawn; // Ensure CMH treats all previous null moves as Px0
							currentGameRecordPointer->move.toSquarePiece = Empty; // Ensure recapture extensions don't mistakenly kick in

							mateBrain.gameRecordPointer++; // Normally done in make/unmake-move
							mateBrain.gameRecordPointer->castlingStatus = (mateBrain.gameRecordPointer - 1)->castlingStatus;
							mateBrain.gameRecordPointer->pliesSinceIrreversible = 0; // Don't allow DBRs across a null move (+3 ELO) // (NormalGenerate.gameRecordPointer - 1)->pliesSinceIrreversible + 1;
							mateBrain.gameRecordPointer->transpositionTableHash64 = ~(mateBrain.gameRecordPointer - 1)->transpositionTableHash64;
							mateBrain.gameRecordPointer->transpositionTableHash64WithEP = mateBrain.gameRecordPointer->transpositionTableHash64;
							mateBrain.gameRecordPointer->epSquare = 0;
							*(uint32_t*)(&mateBrain.gameRecordPointer->totalMaterial[0]) = *(uint32_t*)(&(mateBrain.gameRecordPointer - 1)->totalMaterial[0]); // N.B. Using data type overload at start of line to copy for both sides! DOUBLE CHECK THIS WORKS!!!!
							*(uint64_t*)(&mateBrain.gameRecordPointer->gamePhase[0]) = *(uint64_t*)(&(mateBrain.gameRecordPointer - 1)->gamePhase[0]);
							*(uint32_t*)(&mateBrain.gameRecordPointer->totalOpeningPST[0]) = *(uint32_t*)(&(mateBrain.gameRecordPointer - 1)->totalOpeningPST[0]);
							*(uint32_t*)(&mateBrain.gameRecordPointer->totalEndgamePST[0]) = *(uint32_t*)(&(mateBrain.gameRecordPointer - 1)->totalEndgamePST[0]);
							PRINTTREE(PrintTree(IterationPly, ply, alpha, beta, depthRemaining, -1, -999, currentGameRecordPointer->staticEvaluation);)

								//int R = std::max(3, ((depthRemaining + 1) >> 1));
								int R = 0;// (depthRemaining / 5) + ((currentGameRecordPointer->staticEvaluation - beta) / 128) + (!isPVNode * 3);
							//R = -(TC.MateInN * 2 - ply);
							//int R = 2;
							//if (currentGameRecordPointer->gamePhase[sideToMove] == 0)
							//	R = 3;
							//if (beta <= LosingBaseScore)
							//	R = 2;
							//assert(R >= 0);
							//short nullMoveScore = (short)-TreeSearchMate((short)-beta, (short)(-beta + 1), ply + 1, depthRemaining - R - 1, sideToMove ^ 1, false, freeMoves);
							//short nullMoveScore = (short)-TreeSearchMate((short)-beta, (short)(-beta + 1), ply + 1, 0, sideToMove ^ 1, false, freeMoves);
							short nullMoveScore = (short)-TreeSearchMate((short)-beta, (short)(-beta + 1), ply + 1, depthRemaining - 2 - 1, sideToMove ^ 1, false, freeMoves);
							//short nullMoveScore = (short)-TreeSearchMate((short)(MatingScore - 1), (short)MatingScore, ply + 1, 2, sideToMove ^ 1, false, freeMoves);
							// About 89% of nodes after a null move are 'all' nodes

							// Unmake null move
							mateBrain.gameRecordPointer--;

							if (nullMoveScore >= beta)
							{
								//*currentGameRecordPointer->principalVariationPointer = PVTStandPat;
								return nullMoveScore;
								//nullMovePrunePending = nullMoveScore;
							}

							else if (nullMoveScore <= MatedScore)
							{
								(currentGameRecordPointer - 1)->forcingMove = true;
								(currentGameRecordPointer - 1)->forcingLine = (currentGameRecordPointer - 3)->forcingLine; // Update the 'forcingLine' status
								currentGameRecordPointer->isTWM = TTFlagThreatenedWithMate;
							}
						}



	}

	currentGameRecordPointer->checks = 0;

	assert(NoDuplicateMoves(moveList, movesCount));
	assert(TranpositionTableMoveFound(moveList, movesCount, tteBestMove.ui32));

	//----------------------------------------------------------------------------------------------------

	// Score moves for ordering
	int8_t pt1, pt2, ts1, ts2;

	// Get the previous move details
	pt1 = abs((currentGameRecordPointer - 1)->move.fromSquarePiece) - 1; // 0..5
	ts1 = (currentGameRecordPointer - 1)->move.mf.toSquare;
	assert((pt1 >= Pawn - 1) && (pt1 <= King - 1) && (ts1 >= A1) && (ts1 <= H8));
	//currentGameRecordPointer->historyPointer = &CounterMoveHistory[pt1][ts1];
	currentGameRecordPointer->historyPointer = &CounterMoveHistory->CMH[pt1][ts1];

	int8_t fupt1, futs1;
	fupt1 = abs((currentGameRecordPointer - 2)->move.fromSquarePiece) - 1;//TEST REMOVING FUMs FOR SPEED
	assert((fupt1 >= 0) && (fupt1 <= 5));
	futs1 = (currentGameRecordPointer - 2)->move.mf.toSquare;

	int enemyKingSquare = BitScanForwardX(mateBrain.piecesBB[sideToMove ^ 1][King]);

	if (ply == 1)
		ScoreRootMoveList(moveList);
	else
		//mateBrain.ScoreMovesMateMode(moveList, movesCount, tteBestMove, ply, KillerMoves, &CounterMoves[pt1][ts1], &FollowUpMoves[fupt1][futs1], enemyKingSquare, (mateBrain.gameRecordPointer - 2 + ((ply & 1) ? 0 : 1))->isThreateningMateInOne);
		mateBrain.ScoreMovesMateMode(moveList, movesCount, tteBestMove.ui32, ply, KillerMoves, &CounterMoves[pt1][ts1], &FollowUpMoves[fupt1][futs1], enemyKingSquare, MatingMoves[ply]);
	

	//----------------------------------------------------------------------------------------------------

	// DO TWO PASSES? 1ST: TTMOVE, CAPS, CHECKS 2ND:REST
	// Loop through move list
	legalMovesMade = 0;
	bool hasExtended = false;
	uint32_t quietMovesSearched[220];
	int quietMovesSearchedCount = 0;

	bool keepScanning = true;
	for (int moveListIndexIterator = 0; moveListIndexIterator < movesCount; moveListIndexIterator++)
	{
		// Get the next move
		int bestSortScore = moveList[moveListIndexIterator].score;
		int bestSortIndex = moveListIndexIterator;

		//if (keepScanning && ((ply < TC.MateInN * 2 - 1)))
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

			// Give up scanning for highest scoring move if it's likely an 'all' node
			//if ((moveListIndexIterator >= (movesCount >> 1)) && (moveListIndexIterator > 9))//2nd CLAUSE SHOULD BE 'PASSED USUAL CANDIDATES' E.G. bestSortScore < 1<<23
			//if ((moveListIndexIterator >= (movesCount >> 1)) && (bestSortScore < ((1 << 23) - 5)))// Passed 'special' moves? (TT, +ve captures, killers, counter-moves, follow-up-moves)
			if ((bestSortScore < ((1 << 23) - 99)))// Past 'special' moves? (TT, +ve captures, killers, counter-moves, follow-up-moves and the highest-scoring history move)
				keepScanning = false;
		}

		currentMove.ui32 = moveList[bestSortIndex].ui32;
		currentGameRecordPointer->move.ui32 = currentMove.ui32;
		moveList[bestSortIndex] = moveList[moveListIndexIterator]; // Re-position the first move in the list. N.B. this must be AFTER the 'if (ply == 1)' paragraph above!NOT ANY MORE

		//----------------------------------------------------------------------------------------------------

		//// SEE (used for reductions)
		//int SEEResult;
		//SEEResult = 1; // Assume it's a winning LxH
		//if (!isInCheck) // Don't (SEE) reduce if in check as might just be a delaying move
		//{
		//	// Calculate the SEE result for moves to empty squares
		//	if (ply & 1)
		//	{THE LINE BELOW IS USED TO IMMEDIATELY EXCLUDE K MOVES!!! SO PUT IT BACK
		//		//if (SeeLowHighValues[abs(MateBrain.mailboxBoard64[currentGameRecordPointer->move.mf.fromSquare])] > SeeLowHighValues[abs(MateBrain.mailboxBoard64[MateBrain.gameRecordPointer->move.mf.toSquare])])
		//		if (MateBrain.mailboxBoard64[currentGameRecordPointer->move.mf.toSquare] == Empty)
		//			SEEResult = MateBrain.SEE(currentGameRecordPointer->move.mf.fromSquare, currentGameRecordPointer->move.mf.toSquare, sideToMove); // Calculate the SEE result for HxL
		//	}
		//}

		//----------------------------------------------------------------------------------------------------

		// Up-date move
		legalMovesMade++;
		int8_t toSquarePiece = mateBrain.mailboxBoard64[currentMove.mf.toSquare]; // Needed later to determine if the move is a capture
		mateBrain.MakeMove(sideToMove); // N.B. MakeMove increments mateBrain.gameRecordPointer!
		mateBrain.gameRecordPointer->fixedPiecesAttackerBB = (mateBrain.gameRecordPointer - 1)->fixedPiecesAttackerBB;
		mateBrain.gameRecordPointer->fixedPiecesDefenderBB = (mateBrain.gameRecordPointer - 1)->fixedPiecesDefenderBB;
		mateBrain.gameRecordPointer->fixedPiecesAttackerBB = mateBrain.gameRecordPointer->fixedPiecesAttackerBB & ~(UINT64SetBit(currentMove.mf.fromSquare) | UINT64SetBit(currentMove.mf.toSquare));
		mateBrain.gameRecordPointer->fixedPiecesDefenderBB = mateBrain.gameRecordPointer->fixedPiecesDefenderBB & ~(UINT64SetBit(currentMove.mf.fromSquare) | UINT64SetBit(currentMove.mf.toSquare));
		//if (mateBrain.gameRecordPointer->fixedPiecesBB != (mateBrain.gameRecordPointer - 1)->fixedPiecesBB)
		//	AC8++;

		//if (TC.MateForcePawnMoves)
		//	if ((ply & 1) == 0)
		//		if (mateBrain.mailboxBoard64[currentMove.mf.toSquare] == -Pawn)
		//			mateBrain.gameRecordPointer->totalMaterial[sideToMove ^ 1] += (MVPawn / 2);




		// Does the move give check? NON-CHECKS DEFERRED STUFF
		bool givesCheck;
		if (
			(bestSortScore > -99) // Is this a non-deferred move?
			|| (ply == TC.MateInN * 2 - 1)
			)
		{
			givesCheck = mateBrain.IsEnemyKingAttacked(enemyKingSquare, sideToMove);
		}
		else
			givesCheck = false;


		if (ply == 1)
		{
			ShowProgressMessage(currentMove.ui32, moveListIndexIterator + 1, bestMoveScore, alpha, beta); // Display current root move (always display first move)
		}
		PRINTTREE(PrintTree(IterationPly, ply, alpha, beta, depthRemaining, currentMove.ui32, bestSortScore, 0););

		// Initiate the retrieval of the next transposition table cache line as soon as possible
		_mm_prefetch((char*)(MateTranspositionTablePointer + (mateBrain.gameRecordPointer->transpositionTableHash64 & MateTranspositionTableBucketsMask)), _MM_HINT_T0);

		bool quietMove = ((currentMove.mf.flag < MFPromotion) && (currentGameRecordPointer->move.toSquarePiece == Empty)); // N.B. toSquarePiece gets set in MakeMove

		//----------------------------------------------------------------------------------------------------

#ifdef SEARCHINGFORLINE
		std::string cl;
		TargetLineLastSearched[ply] = currentMove;
		if (TargetLineLength == ply)
		{
			cl = mateBrain.CurrentLine(ply);
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

		// Determine any 'forcing' parameters (makes logic easier to do what we can {i.e. 'threatening mate' and 'zero defender king moves'} after the attacker's moves i.e. at odd plies)


		//int expense = 0;
		int extensions = 0;
		int reductions = 0;

		if (ply & 1) // Odd ply?
		{
			// 'Forcing' moves are e.g. checks, moves which threaten mate, moves which leave the opponent with only 1 move
			// They may not be identified as such until the next ply


			//if ((currentGameRecordPointer - 2)->isOKCM) NEED TO REFINE THIS FOR ALL 'FORCED LINES FROM THE ROOT'
			//{
			//	extensions = 1;
			//	goto AssignNewDepthRemaining;
			//}

			// Move by a 'fixed' attacker piece?
			uint64_t fromSquareBB = UINT64SetBit(currentMove.mf.fromSquare);
			// DON'T PRUNE IF IS A CHECK WHICH DELIVERS MATE!!!
			//if (RootFixedPiecesAttackerBB & fromSquareBB)
			//if ((RootFixedPiecesAttackerBB & fromSquareBB) && !givesCheck && (toSquarePiece == Empty))
			//if ((RootFixedPiecesAttackerBB & fromSquareBB) && !givesCheck)
			//if (((currentGameRecordPointer - 1)->fixedPiecesAttackerBB & fromSquareBB) && !givesCheck)
			if ((currentGameRecordPointer->fixedPiecesAttackerBB & fromSquareBB) && !givesCheck && (toSquarePiece == Empty))
			{
				// if we use MFP for attackers pieces and it gets to a posn where ONLY fixed pieces CAN move, will it return a sensible score ? ? ?
				// YES IT CRASHES!
				// 8/p1p1p3/2p3p1/6Pb/p3P1k1/P1p1PNnr/2P1PKRp/7B w - -
				// setoption name mfp value h1c2e2g2h2
				// f3e5 g4g5 g2g3 g5f6 e5d7 f6e6 f2g2 h3g3
				// THE KING REPLACED THE ROOK ON G2! AND IT'S NOW ONLY THE K CAN MOVE!!




				//should treat it like checkmate/stalemate?

				goto DiscardMove;
			}

			currentGameRecordPointer->forcingMove = false; // Assume it's not a forcing move
			currentGameRecordPointer->forcingLine = false;

			// Checks
			currentGameRecordPointer->givesCheck = givesCheck;
			if (givesCheck) // Giving check?
			{
				currentGameRecordPointer->checks++;

				// ENDLESS SEQUENCES OF Q CHASING K (OFTEN WITH O1M) AROUND THE BOARD EXPLODE!
				// ONLY ALLOW THE 1ST Q CHECK IN A SEQUENCE!
				// Only extend if check is given by a different piece to previous move and it's reversible! SEEMS TO SLOW SOLNS ON AVG???
				//if ((!(currentGameRecordPointer - 3)->givesCheck) || ((currentGameRecordPointer - 1)->pliesSinceIrreversible <= 1) || (currentMove.mf.fromSquare != (mateBrain.gameRecordPointer - 3)->move.mf.toSquare))
				//(currentGameRecordPointer - 1)->forcingMove = false;
				//if (
				//	(abs(mateBrain.mailboxBoard64[currentMove.mf.toSquare]) != Queen)
				//	|| (abs((currentGameRecordPointer - 3)->move.fromSquarePiece) != Queen)
				//	|| (currentMove.mf.fromSquare != (currentGameRecordPointer - 3)->move.mf.toSquare)
				//	|| !(currentGameRecordPointer - 3)->givesCheck
				//	|| (abs((currentGameRecordPointer - 1)->move.toSquarePiece) != Empty)
				//	// TEST MATEALLCHECKS HERE TOO!
				//	) // If the SAME queen gives a sequence of checks, only extend the 1st check UNLESS IT'S A CAPTURE???
				////	extensions = 1;
					currentGameRecordPointer->forcingMove = true;
					currentGameRecordPointer->forcingLine = (currentGameRecordPointer - 2)->forcingLine;
					//hasExtended = true;
				goto AssignNewDepthRemaining;
			}
			else if (TC.MateAllChecks)
			{
			DiscardMove:
				mateBrain.UnMakeMove(sideToMove); // N.B. UnMakeMove decrements MateBrain.gameRecordPointer!

				// Root move?
				if (ply == 1)
					SaveRootNodeCounts(currentMove.ui32);

				continue;
				//*currentGameRecordPointer->principalVariationPointer = PVTStandPat;
				//currentMoveScore = 0;// -WinningBaseScore;// 0;// currentGameRecordPointer->totalMaterial[sideToMove] - MateBrain.gameRecordPointer->totalMaterial[sideToMove ^ 1];
				//goto UnMakeMove;
			}


			// More efficient to do the TMI test at the even ply above and MOST of the time we will go up anyway
			//// TMI1s
			//(currentGameRecordPointer - 1)->isThreateningMateInOne.ui32 = 0;
			//if (!givesCheck)
			//	(currentGameRecordPointer - 1)->isThreateningMateInOne = mateBrain.ThreateningMateInOneWithNull(sideToMove, checksCount);
			//if ((currentGameRecordPointer - 1)->isThreateningMateInOne.ui32 != 0) // Threatening mate in one?
			//{
			//	(currentGameRecordPointer - 1)->forcingMove = true;

			//	// Exclude some TMI1s from extension i.e. leaving the same TMI1 in place for no good reason
			//	// EVEN WITH THESE EXCLUSIONS WE GET WORSE TIMINGS THAN JUST LEAVING THEM ALL IN!!! :O
			//	// TRY TO FIND A #6 THAT IS SLOWER AND IDENTIFY MORE CLAUSES e.g. 1B4q1/1p6/4prb1/p3pr1p/P2RBkN1/5ppP/3N1RP1/1K6 w - -
			//	//if (
			//	//	((MateBrain.gameRecordPointer - 1)->isThreateningMateInOne.ui32 != (MateBrain.gameRecordPointer - 3)->isThreateningMateInOne.ui32) // Different TMI1 from 2 plies earlier?
			//	//	|| (currentMove.mf.toSquare == (MateBrain.gameRecordPointer - 1)->isThreateningMateInOne.mf.fromSquare) // Has the TMI1 piece just moved to its square (allows for defender exchanging on that square)
			//	//	|| (currentMove.mf.toSquare == (MateBrain.gameRecordPointer - 2)->move.mf.toSquare) // Just captured the defender's last moved piece? i.e. a spite check or a sacrificial defence
			//	//	) // Must be different to two plies earlier
			//	extensions = 1; // SOMETIMES WE GET POSNS WHERE THERE ARE LOADS OF TMI1 MOVES AND WE JUST OSCILLATE BETWEEN WHICH ONE WE CHOOSE FOR EXTENDING. NEED A BETTER WAY TO IDENTIFY 'NEW' TMI1s
			//	hasExtended = true;
			//	//ALSO SOMETIMES WE GET DELAYING MOVES (CHECKS) WHICH JUST DELAY THE SAME TMI1 SO WE SHOULD COUNT IT IN THOSE CASES
			//// SOMTIMES THE TMI1 DOESN'T EXIST UNTIL THE ATTACKER PLAYS ITS MOVE... SHOULD ALWAYS COUNT THOSE BUT MAY BE EXPENSIVE??? CHECK IF WE JUST MOVED TO THE SQ THAT TMI1S? unless capture?
			//// LEAVING ALL TMI1s IN MAKES THINGS FASTER IN GENeRAL SO BE VERY PICKY ABOUT EXCLUDING THEM!

			//	goto AssignNewDepthRemaining;
			//}
			//else if (TC.MateAllThreateningMateInOne)//GET RID OF THIS... NOT QUITE RIGHT WITH CHECKS TAKING PRIORITY
			//	goto DiscardMove;


			//if ((mateBrain.gameRecordPointer - 2)->isO1M || (mateBrain.gameRecordPointer - 2)->isOKCM) // Defending king consistently corralled?
			//{
			//	extensions = 1;
			//	goto AssignNewDepthRemaining;
			//}




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
			//							int8_t piece = abs(mateBrain.mailboxBoard64[currentMove.mf.toSquare]);
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


			//// Has any previous move extended (e.g. a check, TMI1 etc) and this is a passive move?
			////if (hasExtended)//SURELY MOST POSNS WILL HAVE A CHECK?
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
			if (//***HERE - surely the 1st clause below is always forcingmove as its a check???
				((isInCheck && (currentGameRecordPointer - 1)->forcingMove) || (currentGameRecordPointer->isTWM == TTFlagThreatenedWithMate) || currentGameRecordPointer->isO1M)
				//((isInCheck && (currentGameRecordPointer- 2)->forcingMove) || (currentGameRecordPointer->isTWM == TTFlagThreatenedWithMate) || (currentGameRecordPointer - 1)->isO1M || (currentGameRecordPointer - 1)->isZLKM)
				//&& (currentGameRecordPointer - 2)->forcingLine // SURELY WE SHOULD STILL EXTEND BY 1 IF THIS IS FALSE???
				)
			{
				if ((currentGameRecordPointer - 1)->forcingLine)
					extensions = 2;
				else
					extensions = 1;

				goto AssignNewDepthRemaining;
			}


			//if (givesCheck) // Giving check? (i.e. a delaying spite check)
			//	if (defenderSpiteChecks < 2)
			//	{
			//		//(currentGameRecordPointer - 1)->forcingMove = true;
			//		defenderSpiteChecks++;
			//		extensions = 1;
			//		goto AssignNewDepthRemaining;
			//	}

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

			//if ((mateBrain.gameRecordPointer - 1)->isO1M) // N.B. If O1M then OOPCM too!
			//	//if ((mateBrain.gameRecordPointer - 3)->isO1M || (mateBrain.gameRecordPointer - 3)->isOKCM || (mateBrain.gameRecordPointer - 2)->givesCheck) // Only extend O1M if there's a sequence of 2 or more (the pre-root position is considered to be O1M)
			//	if ((mateBrain.gameRecordPointer - 3)->isO1M) // Only extend O1M if there's a sequence of 2 or more (the pre-root position is considered to be O1M)
			//	{
			//		//if (
			//		//	(MateBrain.gameRecordPointer - 2)->forcingMove
			//		//	|| (MateBrain.gameRecordPointer - 1)->isInCheck MAYBE SHOULD ONLY EXTEND IF NOT IN CHECK?!
			//		//	|| (MateBrain.gameRecordPointer - 2)->isThreateningMateInOne.ui32
			//		//	|| (egtbResult == TB_LOSS)
			//		//	)
			//		extensions = 2;
			//		goto AssignNewDepthRemaining;
			//	}
			//
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
			//					extensions = 2;
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

			//if ((mateBrain.gameRecordPointer - 3)->isZLKM && (mateBrain.gameRecordPointer - 1)->isZLKM)
			//	//if ((mateBrain.gameRecordPointer - 3)->isZLKM && !(mateBrain.gameRecordPointer - 1)->isZLKM)
			//{
			//	extensions = 2; // Extending can cause some positions to explode e.g. the notorious k2b3b/8/2N2b2/6b1/1b6/b7/3b2K1/b1b2B1n w - - but it still leads to fastest solution time
			//	//reductions = 1;
			//	goto AssignNewDepthRemaining;
			//}

			//if ((mateBrain.gameRecordPointer - 1)->isOKCM)
			//	//if ((mateBrain.gameRecordPointer - 3)->isO1M || (mateBrain.gameRecordPointer - 3)->isOKCM || (mateBrain.gameRecordPointer - 2)->givesCheck)
			//	if ((mateBrain.gameRecordPointer - 3)->isOKCM)
			//{
			//	extensions = 2;
			//	goto AssignNewDepthRemaining;
			//}


			//(mateBrain.gameRecordPointer - 1)->freeMoves--;

			//if (freeMoves == 0)
			//{
			//	*mateBrain.gameRecordPointer->principalVariationPointer = PVTStandPat;
			//	currentMoveScore = initialMaterialBalance;
			//	goto UnMakeMove;
			//}



			//freeMovesDelta = 1;





			//// No attacker's pieces left?
			//if (mateBrain.piecesBB[sideToMove ^ 1][AllPieces] == (mateBrain.piecesBB[sideToMove ^ 1][King] | mateBrain.piecesBB[sideToMove ^ 1][Pawn]))
			//{
			//	reductions = 1;
			//	goto AssignNewDepthRemaining;
			//}

		}




		//Pruning
		if (
			(0)&&
			//((ply & 1)==0) &&
			(legalMovesMade > 1) // Don't prune the 1st move WHY??? UNLESS IT'S TT, MVVLVA, KR, CM, FUM WHICH IT MIGHT NOT BE! CHECK ITS bestSortScore - GOES CRAZY IF YOU TAKE IT OUT!!!
			&& (extensions == 0)
			&& (!isPVNode)
			//&& (!isInCheck) // N.B. you must NOT remove this otherwise you may get false mates returned! NOT TRUE??? IT'S THE >MATEDSCORE TEST BELOW THAT DOES THE TRICK??? but wiki says to use it!
			&& (!givesCheck)
			//&& (SEEResult <= 0) // Don't prune SEE winning moves
			&& (quietMove)
			//&& (beta > LosingBaseScore) // Never prune if we're 'losing' as we want to try EVERYTHING to find a way out! BUT IF BETA<=LosingBaseScore THEN SO TOO IS ALPHA AND THE TEST BELOW COULD NEVER KICK IN?!
			//&& (alpha < WinningBaseScore) // If we have a 'winning' score then EVERY (non-special/quiet) move will be futility pruned and you could 'lose' the EGTB win or miss a better mate! So do NOT take this out!!!
			//&& (!passedPawnMove)// || (SEEResult < 0))
			)
		{
			assert(currentMove.ui32 != tteBestMove.ui32);

			//// Late move pruning
			//if (
			//	(depthRemaining <= 8)
			//	&& (ply > 1)
			//	&& (quietMovesSearchedCount > lateMovePruningMargins[improving][depthRemaining])
			//	&& (bestMoveScore > LosingBaseScore) // Don't prune if we're losing!
			//	//&& (SEEResult < 0)
			//	&& (std::abs(currentGameRecordPointer->move.fromSquarePiece) == Pawn)
			//	)
			//{
			//	PRINTTREE(PrintTree2(IterationPly, ply, "LMP");)
			//	normalBrain.UnMakeMove(sideToMove);
			//	continue;
			//}

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
				mateBrain.UnMakeMove(sideToMove);
				continue;
			}

			//// SEE pruning
			//if (
			//	(depthRemaining <= 6)
			//	&& (SEEResult < 0)
			//	&& (bestMoveScore > LosingBaseScore) // Don't prune if we're losing! (Do NOT take this out otherwise we could get faulty mate scores returned e.g. 7r/B1b1qbPB/2k1q2q/1Nq2nN1/qQQ2QQq/8/q1RRQ2q/3K4 w - - 0 1, go depth 3, returns #3 when it's actually #7!)
			//	)
			//{
			//	PRINTTREE(PrintTree2(IterationPly, ply, "SEE pruning"););
			//	mateBrain.UnMakeMove(sideToMove);
			//	continue;
			//}
		}





	AssignNewDepthRemaining:
		int newDepthRemaining = depthRemaining - 1;
		newDepthRemaining += extensions;
		//newDepthRemaining -= reductionsFixedPieces;
		//newDepthRemaining -= reductions;
		//if (kingMoves > 6)
		//	newDepthRemaining--;//SURELY THIS SHOULD ONLY BE DONE AT EVEN PLIES???

		bool doNonReducedSearch;
		int searches = 0;

		//DO WE EVEN BENEFIT FROM DOING A MWS IN MATE MODE??? yes!
		//if (doNonReducedSearch)
		{//SURELY THIS SHOULD NOW BE A CUT NODE SO THE CHILD SHOULD BE AN 'ALL' NODE?!?!
			// Do a minimal window search
			searches++;
			//currentMoveScore = (short)-TreeSearchMate((short)(-alpha - 1), (short)-alpha, ply + 1, newDepthRemaining, sideToMove ^ 1, givesCheck, true, !isCutNode, currentLineExpense);
			//currentMoveScore = (short)-TreeSearchMate((short)(-alpha - 1), (short)-alpha, ply + 1, IterationPly - ((currentLineExpense + expense) >> 2), sideToMove ^ 1, givesCheck, true, !isCutNode, currentLineExpense + expense);
			//currentMoveScore = (short)-TreeSearchMate((short)(-alpha - 1), (short)-alpha, ply + 1, IterationPly - ((expense) >> 2), sideToMove ^ 1, givesCheck, true, !isCutNode, currentLineExpense + expense);
			
			currentMoveScore = (short)-TreeSearchMate((short)(-alpha - 1), (short)-alpha, ply + 1, newDepthRemaining, sideToMove ^ 1, givesCheck, freeMoves - freeMovesDelta);// , true, !isCutNode, currentLineExpense);
			
			//short aX, bX;
			//aX = -alpha - 1;
			//bX = -alpha;
			//if (ply & 1)
			//{
			//	aX = -MateBaseScore;
			//	bX = -WinningBaseScore;
			//}
			//currentMoveScore = (short)-TreeSearchMate(aX, bX, ply + 1, newDepthRemaining, sideToMove ^ 1, givesCheck, freeMoves - freeMovesDelta);// , true, !isCutNode, currentLineExpense);

			// About 79% of non-reduced searches don't exceed alpha
		}


		//if ((ply & 1) == 0)
		//	if (extensions == 0)
		//		if ((bestMoveScore < MatedScore) && (legalMovesMade > 1) && (currentMoveScore > MatedScore))//THIS IS PROMISING {sometimes phenomenal????} BUT NEEDS TO BE REFINED
		//		{
		//			newDepthRemaining++;
		//			currentMoveScore = (short)-TreeSearchMate((short)(-alpha - 1), (short)-alpha, ply + 1, newDepthRemaining, sideToMove ^ 1, givesCheck, freeMoves - freeMovesDelta);// , true, !isCutNode, currentLineExpense);
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
			currentMoveScore = (short)-TreeSearchMate((short)-beta, (short)-alpha, ply + 1, newDepthRemaining, sideToMove ^ 1, givesCheck, freeMoves - freeMovesDelta);// , true, false, currentLineExpense);
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
			//std::string cl = MateBrain.CurrentLine(ply); THIS IS SET IN THE SECTION ABOVE!

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
			return -MatingIn0Score;
		}

		//----------------------------------------------------------------------------------------------------

		// New best move?
		if (currentMoveScore > bestMoveScore)
		{
			if (currentMoveScore > alpha)
			{
				if (isPVNode)
					mateBrain.SavePrincipalVariation(currentMove.ui32); // Save the PV even if we (are about to) fail high as it might be useful for IID

				if (currentMoveScore >= beta)
				{
					if (currentMoveScore >= MatingScore)
						MatingMoves[ply].ui32 = currentGameRecordPointer->move.ui32;

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
						for (int count = 0; count < quietMovesSearchedCount; count++)
						{
							Move_Struct ms;
							ms.ui32 = quietMovesSearched[count];
							int8_t pt = std::abs(mateBrain.mailboxBoard64[ms.mf.fromSquare]) - 1;
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
					AddToMateTranspositionTable(depthRemaining, ply, currentMoveScore, TTFlagLower + currentGameRecordPointer->isTWM + currentGameRecordPointer->isO1M + currentGameRecordPointer->isFMTP, currentGameRecordPointer->move.ui32, currentGameRecordPointer->staticEvaluation);
					if (ply == 1)
						ShowBestLineMessage(currentMoveScore, 1);

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
					if (currentMoveScore == MatingIn0Score - ply - 1) // Cannot improve on a mate-in-1 so break out of the move loop immediately
					{
						//AC1++;
						bestMoveScore = currentMoveScore;//NEVER KICKS IN! WHY??? because of mws and TT???
						break;
					}
				}

			}

			bestMoveScore = currentMoveScore;
		}

		if (quietMove)
			quietMovesSearched[quietMovesSearchedCount++] = currentMove.ui32;

	} // (Loop through move list)

	//----------------------------------------------------------------------------------------------------

	// Update transposition table
	if (alpha == originalAlpha)
	{
		// No move has returned a score > alpha, therefore this is an 'All' node (all legal moves have been searched)
		// The bestMoveScore is an upper bound (ceiling) on the exact score of the node (i.e. the exact score might be less than bestMoveScore, it is "at most" bestMoveScore)
		// The children of an All node are Cut nodes. The parent of an All node is a Cut node. The ply distance of an All node to its PV ancestor is even.
		//AddToMateTranspositionTable(depthRemaining, ply, bestMoveScore, TTFlagUpper + fewerMovesThanPieces + threatenedWithMate, tteBestMove); // Keep any existing TT move even though it didn't raise alpha
		//AddToMateTranspositionTable(depthRemaining, ply, bestMoveScore, TTFlagUpper + fewerMovesThanPieces + MateGenerate.gameRecordPointer->isInDanger, tteBestMove); // Keep any existing TT move even though it didn't raise alpha
		AddToMateTranspositionTable(depthRemaining, ply, bestMoveScore, TTFlagUpper + currentGameRecordPointer->isTWM + currentGameRecordPointer->isO1M + currentGameRecordPointer->isFMTP, tteBestMove.ui32, currentGameRecordPointer->staticEvaluation); // Keep any existing TT move even though it didn't raise alpha
	}
	else
	{
		assert((originalAlpha < bestMoveScore) && (bestMoveScore == alpha) && (bestMoveScore < beta));
		assert(isPVNode);
		assert(*currentGameRecordPointer->principalVariationPointer != PVTUnknown);
		// A move has returned a score > (the original) alpha but < beta, therefore this is a 'PV' node (all legal moves have been searched)
		// The bestMoveScore is the EXACT score of the node
		// The root node and the leftmost nodes are always PV-nodes. All siblings of a PV node are expected Cut nodes.
		//AddToMateTranspositionTable(depthRemaining, ply, bestMoveScore, TTFlagExact + fewerMovesThanPieces + threatenedWithMate, *MateGenerate.gameRecordPointer->principalVariationPointer);
		//AddToMateTranspositionTable(depthRemaining, ply, bestMoveScore, TTFlagExact + fewerMovesThanPieces + MateGenerate.gameRecordPointer->isInDanger, *MateGenerate.gameRecordPointer->principalVariationPointer);
		AddToMateTranspositionTable(depthRemaining, ply, bestMoveScore, TTFlagExact + currentGameRecordPointer->isTWM + currentGameRecordPointer->isO1M + currentGameRecordPointer->isFMTP, *currentGameRecordPointer->principalVariationPointer, currentGameRecordPointer->staticEvaluation);
	}

	//----------------------------------------------------------------------------------------------------

	assert((bestMoveScore < MatingIn0Score) && (bestMoveScore > -MatingIn0Score));
	return bestMoveScore;
}

Mate::MateResult_Struct Mate::ComputeMate()
{
	// At the start of these Compute* routines assume that just the 64-square mailbox board is set up

	ClearAnalysisCounters();

	mateBrain.CopyFrom(&EngineBrain);

	// Set up the bit boards from the 64-square mailbox board
	ConvertMailboxBoard64ToPiecesBB(mateBrain.mailboxBoard64, mateBrain.piecesBB);

	// Initialise the PV array pointers in the GameRecord array
	for (uint32_t index = 0; index < MaximumPly; index++)
		mateBrain.gameRecord[mateBrain.GameRecordIndexRoot + index].principalVariationPointer = &PrincipalVariation[(MaximumPly + 1) * index];

	//----------------------------------------------------------------------------------------------------

	// Get move timer
	StartClock = std::chrono::steady_clock::now();
	//MessagesLastDisplayedTickCount = StartTickCount - 300; // Get the first batch of messages after 200ms
	MessagesLastDisplayedClock = StartClock;

	// Initialise any variables required for the search
	mateBrain.gameRecordPointer = &mateBrain.gameRecord[mateBrain.GameRecordIndexRoot];

	uint64_t totalNodes[MaximumPly];
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
	//*(uint32_t*)(&MateGenerate.gameRecordPointer->totalMaterial[0]) = *(uint32_t*)(&RootTotalMaterial[0]);
	InitialisePSTValues(mateBrain.mailboxBoard64, mateBrain.gameRecordPointer);
	InitialiseGamePhase(mateBrain.mailboxBoard64, mateBrain.gameRecordPointer);
	//*(uint64_t*)(&MateGenerate.gameRecordPointer->gamePhase[0]) = *(uint64_t*)(&GamePhase[0]);
	uint64_t hash64 = GenerateTranspositionTableHash64(mateBrain.mailboxBoard64, mateBrain.gameRecordPointer);
	if (SideToMove == 1)
		hash64 = ~hash64;
	mateBrain.gameRecordPointer->transpositionTableHash64 = hash64;
	mateBrain.gameRecordPointer->transpositionTableHash64WithEP = hash64 ^ TranspositionTableRandomsEnPassant[mateBrain.gameRecordPointer->epSquare]; // N.B. TranspositionTableRandomsEnPassant[0] = 0
	TranspositionTableAge++;
	TranspositionTableAge &= TTFlagAgeMask;

	// Root move list stuff
	// Generated once here and the moves stay in the same physical order in which they are generated so that they correspond with the same moves in the tree generated move list
	// The .nodes property is used to generate .score values for move ordering in the tree
	MoveWithScore_Struct moveList[220];
	RootMoveList[0].mws.ui32 = 0;
	mateBrain.CalculatePinnedPieces(SideToMove); // Required for legal move generation
	RootMovesCount = mateBrain.GenerateAllMoves(SideToMove, mateBrain.IsEnemyKingAttacked(BitScanForwardX(mateBrain.piecesBB[SideToMove][King]), SideToMove ^ 1), moveList);
	for (int moveListIndexIterator = 0; moveListIndexIterator < RootMovesCount; moveListIndexIterator++)
	{
		RootMoveList[moveListIndexIterator].mws = moveList[moveListIndexIterator];
		RootMoveList[moveListIndexIterator].nodes = 1;
	}

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
	RootFixedPiecesAttackerBB = 0;
	RootFixedPiecesDefenderBB = 0;
	std::string s = TC.MateFixedPieces;
	while (s != "")
	{
		std::string sq = s.substr(0, 2);
		int squareIndex = sq[0] - 'a' + ((sq[1] - '1') * 8);
		if (mateBrain.mailboxBoard64[squareIndex] > 0)
			RootFixedPiecesAttackerBB ^= UINT64SetBit(squareIndex);
		else if (mateBrain.mailboxBoard64[squareIndex] < 0)
			RootFixedPiecesDefenderBB ^= UINT64SetBit(squareIndex);
		s = s.substr(2);
	}
	mateBrain.gameRecordPointer->fixedPiecesAttackerBB = RootFixedPiecesAttackerBB;
	mateBrain.gameRecordPointer->fixedPiecesDefenderBB = RootFixedPiecesDefenderBB;

	autoTune = (TC.MateMaximumDefenderKingMoves == 8) && (TC.MateMaximumDefenderMovablePieces == 16) && (TC.MateMaximumDefenderMoves == 218);
	mateMaximumDefenderKingMoves = TC.MateMaximumDefenderKingMoves;
	//mateMaximumDefenderMovablePieces = TC.MateMaximumDefenderMovablePieces;
	mateMaximumDefenderMovablePieces = std::min(TC.MateMaximumDefenderMovablePieces, (int)PopulationCountX(mateBrain.piecesBB[SideToMove ^ 1][AllPieces]));
	mateMaximumDefenderMoves = TC.MateMaximumDefenderMoves;

	// Clear any killers
	ClearMatingMoves();
	ClearKillerMoves();
	ClearCounterMoves();
	ClearFollowUpMoves();
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
	int backedOffIterationPly = 0;
	int freeMoves = -1;
	bms = "";
	lastPV = "";
	do
	{
		// Update iteration depth (ensuring it doesn't exceed maximum)
		if (IterationPly < MaximumIterationPly)//USE MD!
			IterationPly += 2;
		//IterationPly = 16;
		freeMoves++;//NOT USED???

		// Set the aspiration window
		//if (IterationPly == 1)
		{
			RootAlpha = (short)(-MatingIn0Score);
			RootBeta = (short)(MatingIn0Score);
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
		maximumDefenderKingMovesFound = 0;
		maximumDefenderMovablePiecesFound = 1;
		maximumDefenderMovesFound = 1;

#ifdef SEARCHINGFORLINE
		//TargetLineLength = 1;
		TargetLinePartial = "";
#endif

		//----------------------------------------------------------------------------------------------------

	retry:
		ShowIterationStartMessage();
		PVMessageChecked = false;

		// Do the search
		RootScore = TreeSearchMate(RootAlpha, RootBeta, 1, IterationPly, SideToMove, mateBrain.IsEnemyKingAttacked(BitScanForwardX(mateBrain.piecesBB[SideToMove][King]), SideToMove ^ 1), freeMoves);// , false, false, 0);

		if (autoTune)
		{
			if ((IterationPly >= TC.MateInN * 2) && (RootScore < (MatingIn0Score - TC.MateInN * 2) - 2))
			{
				// We didn't find a mate within TC.MateInN*2 ply so relax the parameters!
				mateMaximumDefenderKingMoves = std::min(mateMaximumDefenderKingMoves + round(double(mateMaximumDefenderKingMoves + 1) / 2), double(8));
				mateMaximumDefenderMovablePieces = std::min(mateMaximumDefenderMovablePieces + round(double(mateMaximumDefenderMovablePieces + 1) / 2), double(16));
				mateMaximumDefenderMoves = std::min(mateMaximumDefenderMoves + round(double(mateMaximumDefenderMoves + 1) / 2), double(218));
			}
			else
			{
				// Tighten the parameters
				//mateMaximumDefenderKingMoves += round(double(maximumDefenderKingMovesFound - mateMaximumDefenderKingMoves) / 2);
				if (maximumDefenderKingMovesFound < mateMaximumDefenderKingMoves)
					//mateMaximumDefenderKingMoves--;
					mateMaximumDefenderKingMoves = std::max(mateMaximumDefenderKingMoves - 1, maximumDefenderKingMovesFound + 1);
				//mateMaximumDefenderMovablePieces += round(double(maximumDefenderMovablePiecesFound - mateMaximumDefenderMovablePieces) / 2);
				if (maximumDefenderMovablePiecesFound < mateMaximumDefenderMovablePieces)
					//mateMaximumDefenderMovablePieces--;
					mateMaximumDefenderMovablePieces = std::max(mateMaximumDefenderMovablePieces - 1, maximumDefenderMovablePiecesFound + 1);
				mateMaximumDefenderMoves += round(double(maximumDefenderMovesFound - mateMaximumDefenderMoves) / 2);
			}
		}

		
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

		std::string s;
		s = MyITOA(maximumDefenderKingMovesFound) + "/" + MyITOA(maximumDefenderMovablePiecesFound) + "/" + MyITOA(maximumDefenderMovesFound) + "/" +
			MyITOA(mateMaximumDefenderKingMoves) + "/" + MyITOA(mateMaximumDefenderMovablePieces) + "/" + MyITOA(mateMaximumDefenderMoves);
		AddMessageToQueue(s, false);
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
		std::string bestMoveMessage;
		bestMoveMessage = "bestmove ";
		if (RootBestMove.ui32 == 0)
			bestMoveMessage + "*** No mate found ***";
		else
			bestMoveMessage += MoveNotation(RootBestMove.ui32);
		bestMoveMessage += "\n";
		if (!MateSilent)
			Output(bestMoveMessage);


		// Save any output from -FILE command for analysis in spreadsheet
		if (ProcessingCommandFile)
		{
			FILE *sw;
			fopen_s(&sw, "output.csv", "a+");
			//std::string s = BestLine() + MyITOA(RootScore) + "," + MyUI64TOA(NodeCount + NodeCountQuiescenceSearch) + "," + MyUI64TOA(GetTickCount64() - StartTickCount);
			std::string s = BestLine() + MyITOA(RootScore) + "," + MyUI64TOA(NodeCount) + "," + MyUI64TOA(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - StartClock).count());
			fprintf(sw, "%s\n", s.c_str());
			fclose(sw);
		}

		DisplayAnalysisCounters();
	}
	//Output("Thread exiting " + MyITOA(ThreadId));

	MateResult_Struct mr;
	mr.bm = bms;
	mr.pv = lastPV;
	mr.acs = acs;
	return mr;
}

void Mate::ComputeMateMTLaunchHelperThread(int threadId)
{
	Mate ts;
	ts.ThreadId = threadId;
	ts.ComputeMate();
}

Mate::MateResult_Struct Mate::ComputeMateMT()
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
	MateResult_Struct mr = EngineMate.ComputeMate();

	//StopImmediately = true; // Ensure all helper threads terminate
	//StopWhenIterationCompleteHelperThreads = true;

	return mr;
}

void Mate::ComputeMateFile(std::string filename)
{
	char line[10000];
	std::string tokens[1000];
	int tokenCount;
	int positionsCount;
	int errors = 0;
	std::string sectionHeader = "% Mate in " + MyITOA(TC.MateInN);
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

			if (std::string(line) == sectionHeader)
			{
				while ((fgets(line, 10000, MateFile) != NULL))// && (!StopWhenIterationComplete))
				{
					line[strlen(line) - 1] = 0; // Remove the trailing "\n" we get when we read a line from a file
					if (std::string(line)[0] == '%')
						continue;
					if (strlen(line) == 0)
						goto CloseFile;

					Split(line, &tokens[0], &tokenCount, " ");
					std::string s, fen, opcodes;
					fen = tokens[0] + " " + tokens[1] + " " + tokens[2] + " " + tokens[3];
					s = "position fen " + fen;
					SetPositionAndMoves(s);
					opcodes = std::string(line).substr(fen.length() + 1);
					Split(opcodes, &tokens[0], &tokenCount, ";");

					std::string dm = "";
					std::string bm = "";
					std::string c0 = "";
					if (tokenCount > 0)
					{
						dm = tokens[0];
						if (tokenCount > 1)
						{
							bm = tokens[1];
							if (tokenCount > 2)
								c0 = tokens[2];
						}
					}

					Output(MyITOA(positionsCount) + ": " + std::string(line));

					MateResult_Struct mr = ComputeMateMT();
					if (mr.pv == "")
						AC9++;
					std::string ss = fen + " dm " + MyITOA(TC.MateInN) + "; bm" + mr.bm + "; pv " + mr.pv + "; acs " + MyFTOA((float)mr.acs / (float)1000, "%.3f") + "; " + c0;
					Output(ss);
					FILE *MateFileOutput;
					fopen_s(&MateFileOutput, ("d:\\chess\\mate\\mate" + MyITOA(TC.MateInN) + ".txt").c_str(), "a");
					ss += "\n";
					fprintf(MateFileOutput, ss.c_str());
					fclose(MateFileOutput);

					Output("");

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
