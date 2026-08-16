#pragma once

//----------------------------------------------------------------------------------------------------

// CRASHLOCATIONDEF must be undefined for multi-threaded play else the constant dirtying of the cache line causes huge overhead negating any lazy-SMP benefit
//#define CRASHLOCATIONDEF
#ifdef CRASHLOCATIONDEF
#define CRASHLOCATION(s) {CrashLocation = s;}
#else
#define CRASHLOCATION(s)
#endif

//----------------------------------------------------------------------------------------------------

class Normal
{
public:
	Normal();
	~Normal();

	static const int MaximumPlyInMain = 100; // Cannot easily make this bigger than 127 because TT subTreeDepth is int8_t!
	static const int MaximumPlyInQS = 127; // Cannot easily make this bigger than 127 because TT subTreeDepth is int8_t!
	static const int MaximumIterationPly = 100;
	static const int MinimumIterationPlyTimedModes = 4;
	static const int MessageDelayTickCount = 1000;

	struct PawnScore_Struct
	{
		uint64_t bb;
		int pawnStructureOpeningScore;
		int pawnStructureEndgameScore;
	};

	// Cache line size is typically 64 bytes
	// So if we have one bucket per cache line...
	// 16 bytes per entry structure gives us 4 entries per bucket
	// 12 bytes per entry structure gives us 5 entries per bucket
	// 10 bytes per entry structure gives us 6 entries per bucket - probably not worth going any denser than this
	//  9 bytes per entry structure gives us 7 entries per bucket
	//  8 bytes per entry structure gives us 8 entries per bucket
	// Increasing the number of entries in each bucket helps as long as we don't have an 'oversized' table where the data just gets spread too thinly and the extra buckets aren't used anyway
	// Stockfish has 2 buckets per cache line with 3 entries per bucket

	static const uint32_t NormalTranspositionTableEntriesPerBucket = 4;
	static const uint64_t bestMoveMask = 0xFFFF;
	static const uint64_t scoreMask = 0xFFFF;
	static const uint64_t staticEvaluationMask = 0xFFFF;
	static const uint64_t flagMask = 0xFF;
	static const uint64_t ageMask = 3;
	static const uint64_t subTreeDepthMask = 0xFF;

	struct NormalTranspositionTableEntryDataFields_Struct
	{
		uint16_t bestMove;
		short score;
		short staticEvaluation;
		uint8_t flag;
		int8_t subTreeDepth;
	};

	struct NormalTranspositionTableEntry_Struct // 16
	{
		// N.B. Microsoft pack structures to be a multiple of the largest field, in this case 8-bytes (uint64_t)

		// With the smallest TT of 1MB we get 16384 buckets (14 bit index) so we need to store at most 50 bits (just over 6 bytes) of the hash in the key
		// With the largest TT of 4GB we get 67108864 buckets (26 bit index) so we need to store at most 38 bits (just under 5 bytes) of the hash in the key
		// With the default TT of 64MB we get 1048576 buckets (20 bit index) so we need to store at most 44 bits (just under 6 bytes) of the hash in the key. As we only store 32 bits we have 52 bits, so 12 bits of error!
		// With a TT of 4MB we get 65536 buckets (16 bit index) so we need to store at most 48 bits (exactly 6 bytes) of the hash in the key
		// Tests indicate that although reducing the number of bits in the key (down to 24 bits) does cause errors it does NOT lose any ELO

		uint64_t hash64;
		//uint64_t data; //8 - 0-15=bestMove, 16-31=score, 32-47=staticEvaluation, 48-55=flag, 56-63=subTreeDepth
		union
		{
			uint64_t data;
			NormalTranspositionTableEntryDataFields_Struct mf;
		};
	};

	struct NormalTranspositionTableBucket_Struct // 64
	{
		// A transposition table bucket typically will be 64 bytes, the same size as a memory cache line
		NormalTranspositionTableEntry_Struct Entries[NormalTranspositionTableEntriesPerBucket]; // 64
	};

	void Normal::TestSEE();

	std::string Normal::ThreadIdSuffix();
	//void AddMessageToQueue(std::string message, bool lastMessageWasAProgressMessage);
	//void ReverseMessageQueueIndex();
	void ShowIterationStartMessage();
	void ShowProgressMessage(uint32_t move, int movesMade, short bestMoveScore, short alpha, short beta);
	void ShowFailedLowMessage(short rootAlpha);
	void ShowIterationFinishMessage(uint32_t hashfull);
	//void ShowQueuedMessages();

	void SaveRootMoveData(uint32_t move, uint64_t totalNodes, short score);
	void UpdateRootMovePriority(uint32_t move);
	bool UpdateRootMoveEGTBStatus(uint32_t move, int wdl, int dtz, int rank);
	int Normal::RetrieveRootMoveWDLStatus(uint32_t move);
	int Normal::RetrieveRootMoveDTZStatus(uint32_t move);
	void ScoreRootMoveList(MoveWithScore_Struct* mlp);

	void ShowBestLineMessage(short alpha, uint8_t eul);
	std::string BestLine();

	void ClearKillerMoves();
	void ClearCounterMoves();
	void ClearFollowUpMoves();
	void ClearCounterMoveHistory();

	void StaticEvaluation();
	void TestSymmetry0();
	void TestSymmetry1();
	void TestSymmetry2();
	int EvaluateInner(int sideToMove);
	short EvaluateNN(int sideToMove, int epSquare);
	short Evaluate(int sideToMove);

	void TimeUp(float divisor);
	short DrawScore(int sideToMove);

	static void ClearNormalTranspositionTable();
	static void AllocateNormalTranspositionTable();
	uint32_t HashfullNormalTranspositionTable();
	void DisplayStatisticsNormalTranspositionTable();
	void AddToNormalTranspositionTable(int8_t depthRemaining, short ply, short score, uint8_t flag, uint32_t bestMove, short tteStaticEvaluation);
	//void AddToNormalTranspositionTable(int8_t depthRemaining, short ply, short score, uint8_t flag, uint32_t bestMove, short tteStaticEvaluation, int tteFound);
	short TreeSearchNormalQuiescence(short alpha, short beta, int ply, int depthRemaining, int sideToMove, int isInCheck);
	short TreeSearchNormal(short alpha, short beta, int ply, int depthRemaining, int sideToMove, int isInCheck, bool allowNull, bool isCutNode);
	std::string ComputeNormal();
	static void ComputeNormalMTLaunchHelperThread(int threadId);
	static void ComputeNormalMT();
	static uint32_t ComputeNormalWrapperInner();
	static void ComputeNormalWrapper();

	int ThreadId;
	static NormalTranspositionTableBucket_Struct* NormalTranspositionTablePointer;
	static uint32_t NormalTranspositionTableBuckets;
	static uint32_t NormalTranspositionTableBucketsMask;

private:
	Brain normalBrain;

	TwoGoodMoves_Struct KillerMoves[MaximumPly];
	TwoGoodMoves_Struct CounterMoves[6][64];
	TwoGoodMoves_Struct FollowUpMoves[6][64];
	CounterMoveHistory_Struct* CounterMoveHistory;

	int IterationPly;
	int MaximumPlyReached;
	int MaximumPlyReachedBeforeQS;
	uint32_t PrincipalVariation[MaximumPly * MaximumPly];
	uint32_t LastPrincipalVariation[MaximumPly];

	short RootAlpha, RootBeta, RootBetaOld, RootScore;
	int RootFailHighs, RootFailLows;
	RootMoveList_Struct RootMoveList[220];
	RootMoveList_Struct RootMoveListBackup[220];
	MoveWithScore_Struct InitialRootMoveList[220];
	int RootMovesCount;
	Move_Struct RootBestMove;
	int RootPriority; // Set to 1 before the search starts and incremented every time a root move exceeds alpha
	int ConsistentBestMoves; // Counts the number of iterations a best move has matched the best move from the previous iteration. The 1st iteration cannot be matched to anything so after ID=2 it may be set to 1. Used to reduce the search time for 'obvious' moves.
	PawnScore_Struct lastPawnScoreWhite, lastButOnePawnScoreWhite, lastPawnScoreBlack, lastButOnePawnScoreBlack;
	int whiteMovesToPromote, blackMovesToPromote;
	uint64_t NodeCount, NodeCountQuiescenceSearch, RootCumulativeNodeCount;
	uint64_t EndgameTablebasesProbes, EndgameTablebasesHeavyProbes, EndgameTablebasesHits;
	int EndgameTablebasesTreeProbeLimitMain, EndgameTablebasesTreeProbeLimitQS;
	int EndgameTablebasesPiecesRoot; // The total number of pieces on the board at the root
	uint32_t EndgameTablebasesRootMove;
	int EndgameTablebasesRootWDL; // win=1, draw=0, loss=-1
	int EndgameTablebasesRootDTZ;
	int EndgameTablebasesRootRank;
	bool EndgameTablebasesErrors;
	int EndgameTablebasesErrorCounts[8];
	bool PVExtended;
	bool isFollowingPV;
	std::string LongestLineWithQS;
	std::string LongestLineWithoutQS;
	std::string BestLineMessage;
	std::string IterationFinishMessage;
	std::string PreviousIterationsMessages;
	std::string CurrentIterationsMessages;
	uint64_t LastTickCount; // Set in TimeUp method

	uint64_t LastProgressMessageTickCount;
	uint32_t UseTTAndPruning;
	int TMI1Extensions;
	int nullMoveMinimumPly;
	int nullMoveSideToMove;

	const short FutilityMargin[9] = { 0,50,100,150,200,250,300,300,300 };

	const static int MessageQueueSize = 12;
	std::string MessageQueue[MessageQueueSize] = { "", "", "", "", "", "", "", "", "", "", "", "" };
	int MessageQueueIndex = 0;
	bool LastMessageWasAProgressMessage = false;
	bool MessagesQueued = false;
	std::chrono::time_point<std::chrono::steady_clock> StartClock, MessagesLastDisplayedClock;

	static int CrashLocation;
	static std::string ThreadResults[ThreadsMax];

	int lateMovePruningMargins[2][9] = { 2,2,2,3,3,3,3,3,3, 4,4,5,8,10,14,18,22,27 };

};
