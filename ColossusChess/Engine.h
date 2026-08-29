#pragma once

#include <atomic>
#include <chrono>

#include "GlobalTypes.h"
#include "BitBoard.h"
#include "Brain.h"
#include "SearchNormal.h"
#include "SearchPerft.h"
#include "SearchMate.h"

//----------------------------------------------------------------------------------------------------

extern Brain EngineBrain;

extern Normal EngineNormal;
extern Perft EnginePerft;
extern Mate EngineMate;

// Miscellaneous
extern bool Quit;
extern bool ComputingMove;
extern std::string LastPositionAndMoves;

// UCI debug mode
extern bool IsDebug;

// Variants
extern bool UCI_Chess960;

// Endgame tablebases
extern int EndgameTablebasesPiecesFound;
extern bool EndgameTablebasesInitialised;
extern char EndgameTablebasesPath[256];
extern bool SyzygyProbe7PieceInTree;
extern int SyzygyProbeLimit;
extern const int EndgameTablebasesCumulativeExpectedFileCounts[8];

// Material/positional scoring
extern short Contempt;
extern const short Tempo;

// Castling statuses - Introduced when implementing Chess960/FRC
extern int8_t InitialKingFile;
extern int8_t InitialKingSideRookFile;
extern int8_t InitialQueenSideRookFile;

// Time control types
// Tournament mode: X moves in M seconds then Y moves in N seconds.
#define TCTTournament 0
// Average mode: average number of seconds per move (actually uses tournament mode of 60 moves in 60*average seconds).
#define TCTAverage 1
// Whole game mode: play the whole game in N seconds.
#define TCTWholeGame 2
// Match rate mode: play at the same rate as the opponent.
#define TCTMatchRate 3
// Fixed time mode: play a move after exactly N seconds.
#define TCTFixedTime 4
// Fixed depth mode: play a move after N ply search completed.
#define TCTFixedDepth 6
// Fixed nodes mode: play a move after N nodes searched.
#define TCTFixedNodes 7
// Mate in N: search for a mate in N moves.
#define TCTMateInN 8
// Perft N: do a Perft search N moves deep.
#define TCTPerftN 9

struct TimeControl_Struct
{
	int CurrentType;
	int TournamentMoves1;
	int TournamentMoves2;
	int TournamentTime1Seconds;
	int TournamentTime2Seconds;
	int AverageSeconds;
	int WholeGameSeconds;
	bool MatchRateAllowed;
	int64_t FixedTimeMilliSeconds;
	int FixedDepthPly;
	bool OddFixedDepthPlyOnly;
	uint64_t FixedNodesCount;
	int MateInN;
	bool MateFullWidth;
	bool MateAllChecks;
	bool MateAllThreateningMateInOne;
	int MateMaximumDefenderKingMoves;
	int MateMaximumDefenderMovablePieces;
	int MateMaximumDefenderMoves;
	int MateMaximumReversibleMoves;
	int MateMinimumAttackerMaterial;
	std::string MateFixedPieces;
	std::string MateFilename;
	int PerftN;
	std::string PerftFilename;
};
extern TimeControl_Struct TC;

extern int MinimumIterationPly;
extern uint64_t WInc;
extern uint64_t WTime;
extern uint64_t BInc;
extern uint64_t BTime;
extern int MovesToGo;
extern bool Ponder;
extern bool Pondering;
extern bool ReplyImmediately;

extern uint8_t SideToMove;
extern bool StopImmediately;
extern bool StopWhenIterationComplete;

// Output formatting
extern bool ShowPVTerminators;
extern bool ShowBlankLines;


#define MFSfromSquare(i) (i & 0xFF)
#define MFStoSquare(i) ((i >> 8) & 0xFF)
#define MFSflag(i) (i >> 16)

//extern std::string PVMessage;
extern bool PVMessageChecked;

// Threads
extern int Threads;

// Transposition table
extern const int TranspositionTableMemoryDefault;
extern const int TranspositionTableMemoryMin;
extern const int TranspositionTableMemoryMax;
extern int TranspositionTableMemory;

extern uint64_t TranspositionTableRandoms[Sides][King + 2][64];
extern uint64_t TranspositionTableRandomKingSideCastling[Sides];
extern uint64_t TranspositionTableRandomQueenSideCastling[Sides];
//extern uint64_t TranspositionTableRandomSideToMove;
extern uint64_t TranspositionTableRandomsEnPassant[64];

extern uint8_t TranspositionTableAge;

// Move generation stages (NOT currently used)
#define MGSTranspositionTableMove 0
#define MGSCapturesAndPromotionsGenerate 1
#define MGSCapturesAndPromotionsSearch 2
#define MGSKillerMove 3
#define MGSRestOfMovesGenerate 4
#define MGSRestOfMovesSearch 5
#define MGSRootMovesGenerate 6
#define MGSRootMovesSearch 7
#define MGSNoMoreMoves 99

//struct StagedMoveGeneration_Struct
//{
//	MoveWithScore_Struct moveList[220];
//	int MGS;
//	int tteBestMove;
//	int sideToMove;
//	int isInCheck;
//	int movesCount;
//	int km1, km2;
//
//};

#define MGCompressMove(ui32) (uint16_t)((ui32 & 0x003F) + ((ui32 & 0x3F00) >> 2) + ((ui32 & 0xF0000) >> 4))
#define MGUnCompressMove(ui16) (uint32_t)((uint32_t)(ui16 & 0x003F) + ((uint32_t)(ui16 & 0x0FC0) << 2) + ((uint32_t)(ui16 & 0xF000) << 4))

extern const int PawnMoveOffset[2];
extern const int BackRankBaseSquareIndex[2];
extern const uint64_t FirstRankBB[2];
extern const uint64_t SecondRankBB[2];
extern const uint64_t SeventhRankBB[2];
extern const uint64_t EighthRankBB[2];
extern const int SixthRank[2];
extern const int SeventhRank[2];
extern const int EigthRank[2];
#define notOccupiedBB ~occupiedBB
extern int8_t ChebyshevDistance[64][64];
extern int8_t ManhattanDistance[64][64];
extern double Reductions[256];
extern const int SeeLowHighValues[7];
extern const int SeeValues[7];

// CPU identification
extern std::string CPUVendor;
extern CPUVendorIdEnum CPUVendorId;
extern std::string CPUBrand;
extern int CPUFamily;
extern int CPUModel;
extern uint64_t ThisCPUSupports;
extern std::string ThisCPUSupportsEISNames;
#define EISMMX (1 << 0)
#define EISSSE (1 << 1)
#define EISSSE2 (1 << 2)
#define EISSSE3 (1 << 3)
#define EISSSSE3 (1 << 4)
#define EISSSE41 (1 << 5)
#define EISSSE42 (1 << 6)
#define EISAVX (1 << 7)
#define EISAVX2 (1 << 8)
#define EISAVX512 (1 << 9)
#define EISBMI1 (1 << 10)
#define EISBMI2 (1 << 11)
#define EISPOPCNT (1 << 12)
#define EISLZCNT (1 << 13)
extern std::string EISNames[14];

// Large pages
extern bool LargePagesAvailable;
extern size_t LargePageMinimum;


//----------------------------------------------------------------------------------------------------

void ClearMailboxBoard64(int8_t mailboxBoard64[64]);

void InitialiseMaterialValues(int8_t mailboxBoard64[64], GameRecordEntry_Struct* grp);
bool MaterialValuesCorrect(int8_t mailboxBoard64[64], GameRecordEntry_Struct* grp);
void InitialisePSTValues(int8_t mailboxBoard64[64], GameRecordEntry_Struct* grp);
bool PSTValuesCorrect(int8_t mailboxBoard64[64], GameRecordEntry_Struct* grp);
extern const int GamePhaseIncrement[8];
void InitialiseGamePhase(int8_t mailboxBoard64[64], GameRecordEntry_Struct* grp);

//std::string ConvertPositionToFEN(int8_t mailboxBoard64[64]);
std::string ConvertPositionToFEN(int8_t mailboxBoard64[64], int sideToMove, GameRecordCastlingStatusUnion castlingStatus, int epSquare, int pliesSinceIrreversible, int moveNumber); // N.B. this currently only works on the root position! IT SHOULD REALLY HAVE PARAMETERS PASSED IN RATHER THAN WORKING FROM GameRecordIndexRoot
//std::string ConvertPositionToFEN(Brain* brain);
//std::string ConvertPositionToFENForPerft(Brain* brain);

void InitialiseOneOffStuff();
void* AlignedAllocateMemory(size_t size, size_t alignment);
void AlignedFreeMemory(void* p);
void FreeAnyTranspositionTableMemory();
uint64_t GenerateTranspositionTableHash64(int8_t mailboxBoard64[64], GameRecordEntry_Struct* gameRecordPointer);

void ClearEverythingForDeterminancy();
void NewGame(bool wipeEverything);

void ConvertMailboxBoard64ToPiecesBB(int8_t mailboxBoard64[64], uint64_t piecesBB[Sides][King + 2]);
bool CompareMailboxBoard64ToPiecesBB(int8_t mailboxBoard64[64], uint64_t piecesBB[Sides][King + 2]);
void WriteMailboxBoard64(Brain* brain);
void WritePiecesBB(uint64_t piecesBB[Sides][King + 2]);

//void ConvertFENToPosition(std::string position, std::string sideToMove, std::string castling, std::string ep, std::string irreversible, std::string moveNumber);
void SetPositionAndMoves(std::string positionAndMoves);
void SendOptions();

extern bool MessagesQueued;
extern std::chrono::time_point<std::chrono::steady_clock> MessagesLastDisplayedClock;

std::string MoveNotation(uint32_t move);


#ifdef EXPERIMENTAL
void KBNvK();

//void StaticEvaluation();
//void TestSymmetry1();
//void TestSymmetry2();
void MaximumMovesQueens();
void MaximumMovesDeleteDuplicates();
void MaximumMoves(int inputFileNumber);
//void GenerateKP();
void GenerateKP2();
//void NNUETest();
//void EGTB7Stats();
#endif
