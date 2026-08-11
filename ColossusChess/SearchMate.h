#pragma once

//----------------------------------------------------------------------------------------------------

class Mate
{
public:
	Mate();
	~Mate();

	static const int MaximumIterationPly = 254;

	static const uint32_t MateTranspositionTableEntriesPerBucket = 4;
	static const uint64_t bestMoveMask = 0xFFFF;
	static const uint64_t scoreMask = 0xFFFF;
	static const uint64_t flagMask = 0xFF;
	static const uint64_t subTreeDepthMask = 0xFF;
	static const uint64_t plyToCeilingMask = 0xFF;

	struct MateTranspositionTableEntry_Struct // 16
	{
		// N.B. Microsoft normally pack structures to be a multiple of the largest field, in this case 8-bytes (uint64_t)

		uint64_t  hash64;
		uint64_t data; //8 - 0-15=bestMove, 16-31=score, 32-39=spare, 40-47=plyToCeiling, 48-55=flag, 56-63=subTreeDepth
	};

	struct MateTranspositionTableBucket_Struct // 64
	{
		// A transposition table bucket typically will be 64 bytes, the same size as a memory cache line
		MateTranspositionTableEntry_Struct Entries[MateTranspositionTableEntriesPerBucket]; // 64
	};

	struct MateResult_Struct
	{
		std::string dm;
		std::string bm;
		std::string pv;
		uint64_t acs;
	};

	std::string Mate::ThreadIdSuffix();
	void AddMessageToQueue(std::string message, bool lastMessageWasAProgressMessage);
	void ReverseMessageQueueIndex();
	void ShowIterationStartMessage();
	void ShowProgressMessage(uint32_t move, int movesMade, short bestMoveScore, short alpha, short beta);
	void ShowFailedLowMessage(short rootAlpha);
	void ShowIterationFinishMessage(uint32_t hashfull);
	void ShowQueuedMessages();

	void SaveRootNodeCounts(int move);
	void UpdateRootNodeCounts(int move, short score);
	void ScoreRootMoveList(MoveWithScore_Struct* mlp);

	void ShowBestLineMessage(short alpha, uint8_t eul);
	std::string BestLine();

	void ClearMatingMoves();
	void ClearKillerMoves();
	void ClearCounterMoves();
	void ClearFollowUpMoves();
	void ClearCounterMoveHistory();

	void TimeUp(float divisor);
	//short DrawScore(int sideToMove);
	int CurrentLineExpense2(int ply);
	//bool AllForced(int ply);
	bool AllChecks(int ply);
	bool AllO1M(int ply);
	bool AllTWM(int ply);
	//bool AllZLKM(int ply);
	Move_Struct CanGiveMateInN(int N, int sideToMove, int isInCheck, int &checksCount);
	//Move_Struct ThreateningMateInOne(int sideToMove);

	static void ClearMateTranspositionTable();
	static void AllocateMateTranspositionTable();
	uint32_t HashfullMateTranspositionTable();
	void AddToMateTranspositionTable(int8_t depthRemaining, short depth, short value, uint8_t flag, uint32_t bestMove, short tteStaticEvaluation);
	short TreeSearchMate(short alpha, short beta, int ply, int depthRemaining, int sideToMove, int isInCheck, int freeMoves);//, bool allowNull, bool isCutNode, int currentLineExpense);
	MateResult_Struct ComputeMate();
	static void ComputeMateMTLaunchHelperThread(int threadId);
	static MateResult_Struct ComputeMateMT();
	static void ComputeMateFile(std::string filename);
	static void ComputeMateWrapper();

	int ThreadId;
	static MateTranspositionTableBucket_Struct* MateTranspositionTablePointer;
	static uint32_t MateTranspositionTableBuckets;
	static bool MateSilent;

private:
	Brain mateBrain;

	Move_Struct MatingMoves[MaximumPly];
	TwoGoodMoves_Struct KillerMoves[MaximumPly];
	TwoGoodMoves_Struct CounterMoves[6][64];
	TwoGoodMoves_Struct FollowUpMoves[6][64];
	CounterMoveHistory_Struct* CounterMoveHistory;

	int IterationPly;
	int MaximumPlyReached;
	alignas(64) uint32_t PrincipalVariation[MaximumPly * MaximumPly];

	short RootAlpha, RootAlphaUpdated, RootBeta, RootBetaOld, RootScore;
	int RootFailHighs, RootFailLows;
	int RootDefenderKingMoves;
	int RootMaterialBalance;
	GameRecordEntry_Struct* RootGameRecordPointer;
	RootMoveList_Struct RootMoveList[220];
	//MoveWithScore_Struct InitialRootMoveList[220];
	int RootMovesCount;
	Move_Struct RootBestMove;
	bool RootZLMPiecesMailboxBoard64[64];
	uint64_t RootFixedPiecesBB;
	uint64_t RootFixedPiecesAttackerBB;
	uint64_t RootFixedPiecesDefenderBB;
	int ConsistentBestMoves;
	int whiteMovesToPromote, blackMovesToPromote;
	uint64_t NodeCount, NodeCountQuiescenceSearch, RootCumulativeNodeCount;
	uint64_t EndgameTablebasesProbes, EndgameTablebasesHeavyProbes, EndgameTablebasesHits;
	int EndgameTablebasesPiecesRoot;
	uint32_t EndgameTablebasesRootMove;
	uint32_t EndgameTablebasesRootWDL;

	int Passes;

	const static int MessageQueueSize = 12;
	std::string MessageQueue[MessageQueueSize] = { "", "", "", "", "", "", "", "", "", "", "", "" };
	int MessageQueueIndex = 0;
	bool LastMessageWasAProgressMessage = false;
	bool MessagesQueued = false;
	std::chrono::time_point<std::chrono::steady_clock> StartClock, MessagesLastDisplayedClock;
	
	static uint32_t MateTranspositionTableBucketsMask;
	std::string LongestLine;
	int defenderSpiteChecks;

	// Tune parameters based on PV node stats

	// Values found in the tree
	int maximumDefenderKingMovesFound;
	int maximumDefenderMovablePiecesFound;
	int maximumDefenderMovesFound;

	// Values used for auto tuning
	int mateMaximumDefenderKingMoves;
	int mateMaximumDefenderMovablePieces;
	int mateMaximumDefenderMoves;

	bool autoTune; // Could have autoTune for each parameter!?

	std::string lastPV;
	std::string bms;
	uint64_t acs;
};
