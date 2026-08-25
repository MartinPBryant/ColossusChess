#include <algorithm>
#include <chrono>
#include <assert.h>
#include <malloc.h>
#include <stdlib.h>
#include "math.h"
#include "time.h"

#include "GlobalConstants.h"
#include "GlobalTypes.h"
#include "Engine.h"
#include "BitBoard.h"
#include "Brain.h"
#include "Evaluate.h"
#include "Utilities.h"
#include "SearchNormal.h"
#include "SearchPerft.h"
#include "SearchMate.h"
#include "SYZYGYPYRRHIC\tbprobe.h"
//#include "NNUE\nnue.h"
//#include "nnue-probe-master\nnue-probe-master\src\nnue.h"
//#include "nncpu-probe-master\src\nncpu.h"

//----------------------------------------------------------------------------------------------------

Brain EngineBrain;

// The outer 'engine' creates an instance of each of the search classes for use as 'thread 0'
// For multi-threading, the 'thread 0' classes create extra instances for lazy-SMP
Normal EngineNormal;
Perft EnginePerft;
Mate EngineMate;

// Miscellaneous
bool Quit;
bool ComputingMove;
std::string LastPositionAndMoves = "";

// UCI debug mode
bool IsDebug;

// Variants
bool UCI_Chess960 = false;

// Endgame tablebases
int EndgameTablebasesPiecesFound = 0; // Returned by the EGTB initialisation code
bool EndgameTablebasesInitialised = false;
char EndgameTablebasesPath[256];
bool SyzygyProbe7PieceInTree = false;
int SyzygyProbeLimit = 6;
const int EndgameTablebasesCumulativeExpectedFileCounts[8] = { 0, 0, 0, 5, 5 + 30, 5 + 30 + 110, 5 + 30 + 110 + 365, 5 + 30 + 110 + 365 + 1001 };

// Material/positional scoring
short Contempt = 0;
short Tempo = 1;

// Castling statuses
int8_t InitialKingFile; // Introduced when implementing Chess960/FRC
int8_t InitialKingSideRookFile;
int8_t InitialQueenSideRookFile;

//static const uint8_t FiftyMoveReduction[101] = // NOT USED
//{
//	0,
//	0,0,0,0,0,0,0,0,0,0,
//	0,0,0,0,0,0,0,0,0,0,
//	0,0,0,0,0,0,0,0,0,0,
//	0,0,0,0,0,0,0,0,0,0,
//	0,0,0,0,0,0,0,0,0,0,
//	0,0,0,0,0,0,0,0,0,0,
//	  0,  1,  2,  3,  4,  6,  8, 10, 13, 16,
//	 19, 23, 27, 31, 36, 41, 46, 52, 58, 64,
//	 70, 77, 85, 92,100,108,117,125,135,144,
//	154,164,174,185,196,207,219,231,243,255
//};

// Threads
int Threads = ThreadsDefault;

// Transposition table stuff
const int TranspositionTableMemoryDefault = 64; // MB
const int TranspositionTableMemoryMin = 0;
const int TranspositionTableMemoryMax = 16384;
int TranspositionTableMemory = TranspositionTableMemoryDefault;

//__declspec(align(8))
uint64_t TranspositionTableRandoms[Sides][King + 2][64];
uint64_t TranspositionTableRandomKingSideCastling[Sides];
uint64_t TranspositionTableRandomQueenSideCastling[Sides];
//uint64_t TranspositionTableRandomSideToMove;
//__declspec(align(8))
uint64_t TranspositionTableRandomsEnPassant[64] =
{
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	Random64(), Random64(), Random64(), Random64(), Random64(), Random64(), Random64(), Random64(),
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	Random64(), Random64(), Random64(), Random64(), Random64(), Random64(), Random64(), Random64(),
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
}; // Only need the 16 squares that the double pawn pushes pass over to be set

uint8_t TranspositionTableAge; // Lower 2 bits give 4 'ages'

#ifdef _DEBUG
int64_t transpositionTableReads, transpositionTableReadsFoundPosition, transpositionTableReadsFoundPositionAndDraft, transpositionTableReadsFoundMove;
int64_t transpositionTableReadsQS, transpositionTableReadsFoundPositionQS, transpositionTableReadsFoundPositionAndDraftQS, transpositionTableReadsFoundMoveQS, transpositionTableReadsNotHelpfulQS;
int64_t transpositionTableWrites, transpositionTableWritesSuccessful, transpositionTableWritesFailed;
int64_t evaluations, fullEvaluations;
int64_t killerMoveSearched, killerMoveCausedCutoff;
#endif

// Miscellaneous
static std::string lastFEN = "";
static int64_t pseudo1, pseudo2;

//std::string PVMessage;
bool PVMessageChecked;

// Time control stuff
TimeControl_Struct TC;

//int FixedDepthPly;
bool Infinite;

int MinimumIterationPly;
uint64_t WInc;
uint64_t WTime;
uint64_t BInc;
uint64_t BTime;
int MovesToGo; // The number of moves left to the time control. Provided by the GUI. Zero if 'all the moves'.
bool Ponder = false;
bool Pondering;
bool ReplyImmediately;

bool StopImmediately;
bool StopWhenIterationComplete;

// Output formatting
bool ShowPVTerminators;
bool ShowBlankLines;

const int PawnMoveOffset[Sides] = { 8, -8 };
const int BackRankBaseSquareIndex[Sides] = { 0, 56 };
const uint64_t FirstRankBB[Sides] = { Rank1BB, Rank8BB };
const uint64_t SecondRankBB[Sides] = { Rank2BB, Rank7BB };
const uint64_t SeventhRankBB[Sides] = { Rank7BB, Rank2BB };
const uint64_t EighthRankBB[Sides] = { Rank8BB, Rank1BB };
const int SixthRank[Sides] = { 5, 2 };
const int SeventhRank[Sides] = { 6, 1 };
const int EigthRank[Sides] = { 7, 0 };
alignas(64) int8_t ChebyshevDistance[64][64];
alignas(64) int8_t ManhattanDistance[64][64];
//int MoveListIndex;
double Reductions[256];

std::string CPUVendor = "";
std::string CPUBrand = "";
CPUVendorIdEnum CPUVendorId;
int CPUFamily;
int CPUModel;
uint64_t ThisCPUSupports;
std::string ThisCPUSupportsEISNames;
std::string EISNames[14] = {"MMX", "SSE", "SSE2", "SSE3", "SSSE3", "SSE41", "SSE42", "AVX", "AVX2", "AVX512", "BMI1", "BMI2", "POPCNT", "LZCNT"};

//----------------------------------------------------------------------------------------------------

// Side stuff
uint8_t SideToMove; // Zero based. For chess: 0 = white, 1 = black

void AdvanceSideToMove()
{
	// Advance the side	to move
	SideToMove++;

	// If passed the last side then restart with the first side
	if (SideToMove >= Sides)
		SideToMove = 0;
}

//----------------------------------------------------------------------------------------------------

std::string ConvertPositionToFEN(int8_t mailboxBoard64[64], int sideToMove, GameRecordCastlingStatusUnion castlingStatus, int epSquare, int pliesSinceIrreversible, int moveNumber) // N.B. this currently only works on the root position! IT SHOULD REALLY HAVE PARAMETERS PASSED IN RATHER THAN WORKING FROM GameRecordIndexRoot
//std::string ConvertPositionToFEN(Brain* brain) // N.B. this currently only works on the root position! IT SHOULD REALLY HAVE PARAMETERS PASSED IN RATHER THAN WORKING FROM GameRecordIndexRoot
{
	std::string fen = "";
	int rank, file, emptySquares;
	int8_t currentPiece;

	// Construct a FEN string for the current position
	for (rank = 8; rank >= 1; rank--)
	{
		emptySquares = 0;

		for (file = 1; file <= 8; file++)
		{
			currentPiece = mailboxBoard64[(rank - 1) * 8 + (file - 1)];

			if (currentPiece == 0)
				emptySquares++;
			else
			{
				if (emptySquares > 0)
				{
					fen = fen + MyITOA(emptySquares);
					emptySquares = 0;
				}
				fen += PieceToChar[currentPiece + 6];
			}
		}

		if (emptySquares > 0)
		{
			fen = fen + MyITOA(emptySquares);
			emptySquares = 0;
		}

		if (rank > 1)
			fen += "/";
	}

	if (sideToMove == 0)
		fen += " w";
	else
		fen += " b";

	std::string _castling = "";
	if (UCI_Chess960)
	{
		if (castlingStatus.ui8[0][0] == 0)
			_castling += std::string(1, 'A' + InitialKingSideRookFile);
		if (castlingStatus.ui8[0][1] == 0)
			_castling += std::string(1, 'A' + InitialQueenSideRookFile);
		if (castlingStatus.ui8[1][0] == 0)
			_castling += std::string(1, 'a' + InitialKingSideRookFile);
		if (castlingStatus.ui8[1][1] == 0)
			_castling += std::string(1, 'a' + InitialQueenSideRookFile);
	}
	else
	{
		if ((castlingStatus.ui8[0][0] == 0) && (mailboxBoard64[E1] == King) && (mailboxBoard64[H1] == Rook))
			_castling += "K";
		if ((castlingStatus.ui8[0][1] == 0) && (mailboxBoard64[E1] == King) && (mailboxBoard64[A1] == Rook))
			_castling += "Q";
		if ((castlingStatus.ui8[1][0] == 0) && (mailboxBoard64[E8] == -King) && (mailboxBoard64[H8] == -Rook))
			_castling += "k";
		if ((castlingStatus.ui8[1][1] == 0) && (mailboxBoard64[E8] == -King) && (mailboxBoard64[A8] == -Rook))
			_castling += "q";
	}
	if (_castling == "")
		_castling = "-";
	fen += " " + _castling;

	// N.B. Wikipedia states: "En passant target square in algebraic notation. If there's no en passant target square, this is "-". If a pawn has just made a two-square move, this is the position "behind" the pawn. This is recorded regardless of whether there is a pawn in position to make an en passant capture."
	std::string _enPassant;
	if (epSquare != 0)
		_enPassant = Notation64[epSquare];
	else
		_enPassant = "-";
	fen += " " + _enPassant;

	if (pliesSinceIrreversible != -1)
		fen += " " + MyITOA(pliesSinceIrreversible);

	if (moveNumber != -1)
		fen += " " + MyITOA(moveNumber);

	return fen;
}

//----------------------------------------------------------------------------------------------------

void ClearMailboxBoard64(int8_t mailboxBoard64[64])
{
	for (int square = A1; square <= H8; square++)
		mailboxBoard64[square] = Empty;
}

void ClearPiecesBB(uint64_t piecesBB[Sides][King + 2])
{
	for (int side = 0; side < Sides; side++)
		for (int piece = AllPieces; piece <= King; piece++)
			piecesBB[side][piece] = 0;
}

std::string MailboxBoard64String(int8_t mailboxBoard64[64])
{
	int rank;
	int file;
	int piece;
	std::string line;
	std::string result = "";

	for (rank = 7; rank >= 0; rank--)
	{
		line = "";

		for (file = 0; file <= 7; file++)
		{
			piece = mailboxBoard64[rank * 8 + file];
			line += PieceToChar[piece + 6];
			line += " ";
		}
		result = result + line + " " + MyITOA(rank + 1) + "\r\n";
	}
	result = result + "\r\n";
	result = result + "a b c d e f g h\r\n";

	return result;
}

void WriteMailboxBoard64(Brain* brain)
{
	Output(MailboxBoard64String(brain->mailboxBoard64));
	//Output(ConvertPositionToFEN(brain));
	Output(ConvertPositionToFEN(brain->mailboxBoard64, brain->gameRecord[brain->GameRecordIndexRoot].sideToMove, brain->gameRecord[brain->GameRecordIndexRoot].castlingStatus, brain->gameRecord[brain->GameRecordIndexRoot].epSquare, brain->gameRecord[brain->GameRecordIndexRoot].pliesSinceIrreversible, brain->gameRecord[brain->GameRecordIndexRoot].moveNumber));
	uint64_t hash64 = GenerateTranspositionTableHash64(brain->mailboxBoard64, &brain->gameRecord[brain->GameRecordIndexRoot]);

	static char buffer[100];

	std::sprintf(buffer, "%llx", hash64);
	Output(MyUI64TOA(hash64) + " / " + (std::string)buffer);
	std::sprintf(buffer, "%llx", ~hash64);
	Output(MyUI64TOA(~hash64) + " / " + (std::string)buffer);

	Output("");
}

void WritePiecesBBAddPieces(int8_t mailboxBoard64[64], uint64_t bb, int8_t pieceType)
{
	while (bb)
	{
		int square = GetLS1BIndex(bb);
		mailboxBoard64[square] = pieceType;
		ClearLS1B(bb);
	}
}

void WritePiecesBB(Brain* brain)
{
	int8_t mailboxBoard64[64];

	ClearMailboxBoard64(mailboxBoard64);

	for (int piece = Pawn; piece <= King; piece++)
	{
		WritePiecesBBAddPieces(mailboxBoard64, brain->piecesBB[0][piece], piece);
		WritePiecesBBAddPieces(mailboxBoard64, brain->piecesBB[1][piece], -piece);
	}

	Output(MailboxBoard64String(mailboxBoard64));
	Output(ConvertPositionToFEN(brain->mailboxBoard64, brain->gameRecord[brain->GameRecordIndexRoot].sideToMove, brain->gameRecord[brain->GameRecordIndexRoot].castlingStatus, brain->gameRecord[brain->GameRecordIndexRoot].epSquare, brain->gameRecord[brain->GameRecordIndexRoot].pliesSinceIrreversible, brain->gameRecord[brain->GameRecordIndexRoot].moveNumber));
	Output("");
}

void ConvertMailboxBoard64ToPiecesBB(int8_t mailboxBoard64[64], uint64_t piecesBB[Sides][King + 2])
{
	ClearPiecesBB(piecesBB);

	for (int square = A1; square <= H8; square++)
	{
		int8_t piece = mailboxBoard64[square];
		if (piece != Empty)
		{
			int sideToMove = (piece < 0 ? 1 : 0);
			piecesBB[sideToMove][abs(piece)] |= CreateBitboardFromSquare(square);
			piecesBB[sideToMove][AllPieces] |= CreateBitboardFromSquare(square);
		}
	}
}

void ConvertPiecesBBToMailboxBoard64(uint64_t piecesBB[Sides][King + 2], int8_t mailboxBoard64[64])
{
	ClearMailboxBoard64(mailboxBoard64);

	for (int side = 0; side < Sides; side++)
		for (int piece = Pawn; piece <= King; piece++)
		{
			uint64_t bb2 = piecesBB[side][piece];
			while (bb2)
			{
				int square = GetLS1BIndex(bb2);
				mailboxBoard64[square] = (side == 0 ? piece : -piece);
				ClearLS1B(bb2);
			}
		}
}

bool CompareMailboxBoard64ToPiecesBB(int8_t mailboxBoard64[64], uint64_t piecesBB[Sides][King + 2])
{
	uint64_t piecesBB2[Sides][King + 2];

	ClearPiecesBB(piecesBB2);
	ConvertMailboxBoard64ToPiecesBB(mailboxBoard64, piecesBB2);

	for (int side = 0; side < Sides; side++)
		for (int piece = AllPieces; piece <= King; piece++)
			if (piecesBB2[side][piece] != piecesBB[side][piece])
				return false;

	return true;
}

//----------------------------------------------------------------------------------------------------

//// Marcel van Kervinck's cuckoo algorithm for fast detection of "upcoming repetition" situations. Description of the algorithm in the following paper: https://marcelk.net/2013-04-06/paper/upcoming-rep-v2.pdf
//
//// First and second hash functions for indexing the cuckoo tables
//inline int Hash1(uint64_t h) { return h & 0x1fff; }
//inline int Hash2(uint64_t h) { return (h >> 16) & 0x1fff; }
//
//// Cuckoo tables with Zobrist hashes of valid reversible moves, and the moves themselves
//uint64_t Cuckoo[8192];
//int CuckooMove[8192];
//
//void InitialiseCuckooTables()
//{
//	std::memset(Cuckoo, 0, sizeof(Cuckoo));
//	std::memset(CuckooMove, 0, sizeof(CuckooMove));
//
//	int count = 0;
//	for (int piece = -King; piece <= King; piece++)
//		for (int s1 = A1; s1 <= H8; s1++)
//			for (int s2 = s1 + 1; s2 <= H8; s2++)
//				if (AttacksByPieceBBList[abs(piece)][s1] & UINT64SetBit(s2)) // Can the current piece-type pseudo-legally move from s1 to s2?
//				{
//					int move = (s1 << 8) + s2;
//
//					if (piece <= -Pawn)
//					{
//						uint64_t key = TranspositionTableRandoms[1][-piece][s1] ^ TranspositionTableRandoms[1][-piece][s2];
//						int hash = Hash1(key);
//						while (true)
//						{
//							uint64_t t1 = Cuckoo[hash]; Cuckoo[hash] = key; key = t1;
//							int t2 = CuckooMove[hash]; CuckooMove[hash] = move; move = t2;
//							if (move == 0) // Arrived at empty slot ?
//								break;
//							hash = (hash == Hash1(key)) ? Hash2(key) : Hash1(key); // Push victim to alternative slot
//						}
//						count++;
//					}
//					else if (piece >= Pawn)
//					{
//						uint64_t key = TranspositionTableRandoms[0][piece][s1] ^ TranspositionTableRandoms[0][piece][s2];
//						int hash = Hash1(key);
//						while (true)
//						{
//							uint64_t t1 = Cuckoo[hash]; Cuckoo[hash] = key; key = t1;
//							int t2 = CuckooMove[hash]; CuckooMove[hash] = move; move = t2;
//							if (move == 0) // Arrived at empty slot ?
//								break;
//							hash = (hash == Hash1(key)) ? Hash2(key) : Hash1(key); // Push victim to alternative slot
//						}
//						count++;
//					}
//				}
//	assert(count == 3668);
//}

//----------------------------------------------------------------------------------------------------

void SetNewGamePositionMailboxBoard64(int8_t mailboxBoard64[64])
{
	// Set the board for a new game
	ClearMailboxBoard64(mailboxBoard64);

	mailboxBoard64[A1] = Rook;
	mailboxBoard64[B1] = Knight;
	mailboxBoard64[C1] = Bishop;
	mailboxBoard64[D1] = Queen;
	mailboxBoard64[E1] = King;
	mailboxBoard64[F1] = Bishop;
	mailboxBoard64[G1] = Knight;
	mailboxBoard64[H1] = Rook;
	mailboxBoard64[A2] = Pawn;
	mailboxBoard64[B2] = Pawn;
	mailboxBoard64[C2] = Pawn;
	mailboxBoard64[D2] = Pawn;
	mailboxBoard64[E2] = Pawn;
	mailboxBoard64[F2] = Pawn;
	mailboxBoard64[G2] = Pawn;
	mailboxBoard64[H2] = Pawn;
	mailboxBoard64[A8] = -Rook;
	mailboxBoard64[B8] = -Knight;
	mailboxBoard64[C8] = -Bishop;
	mailboxBoard64[D8] = -Queen;
	mailboxBoard64[E8] = -King;
	mailboxBoard64[F8] = -Bishop;
	mailboxBoard64[G8] = -Knight;
	mailboxBoard64[H8] = -Rook;
	mailboxBoard64[A7] = -Pawn;
	mailboxBoard64[B7] = -Pawn;
	mailboxBoard64[C7] = -Pawn;
	mailboxBoard64[D7] = -Pawn;
	mailboxBoard64[E7] = -Pawn;
	mailboxBoard64[F7] = -Pawn;
	mailboxBoard64[G7] = -Pawn;
	mailboxBoard64[H7] = -Pawn;
}

void InitialiseDistances()
{
	int fromSquare, toSquare, fromSquareRank, toSquareRank, fromSquareFile, toSquareFile;

	for (fromSquare = A1; fromSquare <= H8; fromSquare++)
	{
		fromSquareRank = fromSquare >> 3;
		fromSquareFile = fromSquare & 7;

		for (toSquare = A1; toSquare <= H8; toSquare++)
		{
			toSquareRank = toSquare >> 3;
			toSquareFile = toSquare & 7;

			ChebyshevDistance[fromSquare][toSquare] = std::max(abs(fromSquareRank - toSquareRank), abs(fromSquareFile - toSquareFile));
			ManhattanDistance[fromSquare][toSquare] = std::abs(fromSquareRank - toSquareRank) + abs(fromSquareFile - toSquareFile);
		}
	}
}

std::string MoveNotation(uint32_t move)
{
	Move_Struct ms;
	ms.ui32 = move;
	assert((ms.mf.fromSquare >= 0) && (ms.mf.fromSquare < 64) && (ms.mf.toSquare >= 0) && (ms.mf.toSquare < 64));

	std::string s;

	if (move == 0x3F3F)
		s = "null";
	else
	{
		if (ms.mf.flag == MFEnPassant)
		{
			if (ms.mf.toSquare >= A5)
				ms.mf.toSquare = (SquaresEnum)(ms.mf.toSquare + 8);
			else
				ms.mf.toSquare = (SquaresEnum)(ms.mf.toSquare - 8);
		}
		s = Notation64[ms.mf.fromSquare];
		int toSquare = ms.mf.toSquare;
		if (UCI_Chess960 && (ms.mf.flag == MFCastling))
		{
			if ((toSquare & 7) == G) // King side?
				toSquare = toSquare - G + InitialKingSideRookFile;
			else
				toSquare = toSquare - C + InitialQueenSideRookFile;
		}
		s += Notation64[toSquare];
		if (ms.mf.flag >= MFPromotion)
		{
			if (ms.mf.flag == MFPromoteToQueen)
				s += "q";
			else if (ms.mf.flag == MFPromoteToRook)
				s += "r";
			else if (ms.mf.flag == MFPromoteToBishop)
				s += "b";
			else
				s += "n";
		}
	}

	return s;
}

//----------------------------------------------------------------------------------------------------

void* AlignedAllocateMemory(size_t size, size_t alignment)
{
#ifdef _WIN32
	return _aligned_malloc(size, alignment);
#else
	return aligned_alloc(alignment, size);
#endif
}

void AlignedFreeMemory(void* p)
{
#ifdef _WIN32
	_aligned_free(p);
#else
	free(p);
#endif
}

void FreeAnyTranspositionTableMemory()
{
	AlignedFreeMemory(Normal::NormalTranspositionTablePointer);
	Normal::NormalTranspositionTablePointer = nullptr;
	Normal::NormalTranspositionTableBuckets = 0;

	AlignedFreeMemory(Perft::PerftTranspositionTablePointer);
	Perft::PerftTranspositionTablePointer = nullptr;
	Perft::PerftTranspositionTableBuckets = 0;

	AlignedFreeMemory(Mate::MateTranspositionTablePointer);
	Mate::MateTranspositionTablePointer = nullptr;
	Mate::MateTranspositionTableBuckets = 0;
}

void InitialiseTranspositionTableRandomValues()
{
	// 2(sides) x 6(piece types) x 64(squares) = 768(randoms)
	for (int piece = Pawn; piece <= King; piece++)
		for (int square = A1; square <= H8; square++)
		{
			TranspositionTableRandoms[0][piece][square] = Random64();
			TranspositionTableRandoms[1][piece][square] = Random64();
		}

	// 4
	TranspositionTableRandomKingSideCastling[0] = Random64();
	TranspositionTableRandomQueenSideCastling[0] = Random64();
	TranspositionTableRandomKingSideCastling[1] = Random64();
	TranspositionTableRandomQueenSideCastling[1] = Random64();

	//TranspositionTableRandomSideToMove = Random64(); NOT USED

#ifdef _DEBUG
	// Sanity check random numbers for duplicates!
	uint64_t random64;
	for (int side = 0; side < 2; side++)
		for (int piece = Pawn; piece <= King; piece++)
			for (int square = A1; square <= H8; square++)
			{
				random64 = TranspositionTableRandoms[side][piece][square];
				int count = 0;
				for (int sideX = 0; sideX < 2; sideX++)
					for (int pieceX = Pawn; pieceX <= King; pieceX++)
						for (int squareX = A1; squareX <= H8; squareX++)
							if (random64 == TranspositionTableRandoms[sideX][pieceX][squareX])
								count++;
				assert(count == 1);
			}
#endif
}

uint64_t GenerateTranspositionTableHash64(int8_t mailboxBoard64[64], GameRecordEntry_Struct* gameRecordPointer)
{
	uint64_t hash64 = 0;

	for (int square = A1; square <= H8; square++)
	{
		int8_t piece = mailboxBoard64[square];
		if (piece >= Pawn)
			hash64 ^= TranspositionTableRandoms[0][piece][square];
		else if (piece <= -Pawn)
			hash64 ^= TranspositionTableRandoms[1][-piece][square];
	}

	if (gameRecordPointer->castlingStatus.ui8[0][0] == 0)
		hash64 ^= TranspositionTableRandomKingSideCastling[0];
	if (gameRecordPointer->castlingStatus.ui8[0][1] == 0)
		hash64 ^= TranspositionTableRandomQueenSideCastling[0];
	if (gameRecordPointer->castlingStatus.ui8[1][0] == 0)
		hash64 ^= TranspositionTableRandomKingSideCastling[1];
	if (gameRecordPointer->castlingStatus.ui8[1][1] == 0)
		hash64 ^= TranspositionTableRandomQueenSideCastling[1];

	// Any EP square is handled separately in the tree search code

	return hash64;
}

void ClearEverythingForDeterminancy()
{
	Perft::ClearPerftTranspositionTable();
	Normal::ClearNormalTranspositionTable();
	Mate::ClearMateTranspositionTable();

	EngineNormal.ClearKillerMoves();
	EngineNormal.ClearCounterMoves();
	EngineNormal.ClearFollowUpMoves();
	EngineNormal.ClearCounterMoveHistory();
	
	EngineMate.ClearMatingMoves();
	EngineMate.ClearKillerMoves();
	EngineMate.ClearCounterMoves();
	EngineMate.ClearFollowUpMoves();
	EngineMate.ClearCounterMoveHistory();
}

// Initialise everything for a new game
void NewGame(bool clearEverything)
{
	// This assumes a 'normal' chess starting position. 'Chess960/FRC' starting positions are specified via a subsequent POSITION FEN command
	SetNewGamePositionMailboxBoard64(EngineBrain.mailboxBoard64);
	InitialKingFile = E;
	InitialKingSideRookFile = H;
	InitialQueenSideRookFile = A;
	EngineBrain.gameRecordPointer->castlingStatus.ui8[0][0] = 0;
	EngineBrain.gameRecordPointer->castlingStatus.ui8[0][1] = 0;
	EngineBrain.gameRecordPointer->castlingStatus.ui8[1][0] = 0;
	EngineBrain.gameRecordPointer->castlingStatus.ui8[1][1] = 0;
	SideToMove = 0;
	EngineBrain.ClearGameRecord();

	if (clearEverything)
		ClearEverythingForDeterminancy(); // Clears TT, killers, etc
}

void InitialiseMaterialValues(int8_t mailboxBoard64[64], GameRecordEntry_Struct* grp)
{
	grp->totalMaterial[0] = 0;
	grp->totalMaterial[1] = 0;

	for (int square = A1; square <= H8; square++)
	{
		int8_t piece = mailboxBoard64[square];
		if ((piece != Empty) && (abs(piece) != King))
		{
			if (piece > 0)
				grp->totalMaterial[0] += MaterialValue[piece];
			else
				grp->totalMaterial[1] += MaterialValue[-piece];
		}
	}
}

bool MaterialValuesCorrect(int8_t mailboxBoard64[64], GameRecordEntry_Struct* grp)
{
	short totalMaterial[2];

	totalMaterial[0] = 0;
	totalMaterial[1] = 0;

	for (int square = A1; square <= H8; square++)
	{
		int8_t piece = mailboxBoard64[square];
		if ((piece != Empty) && (abs(piece) != King))
		{
			if (piece > 0)
				totalMaterial[0] += MaterialValue[piece];
			else
				totalMaterial[1] += MaterialValue[-piece];
		}
	}

	return (grp->totalMaterial[0] == totalMaterial[0]) && (grp->totalMaterial[1] == totalMaterial[1]);
}


void CalculatePSTValues(int8_t mailboxBoard64[64], int openingPST[Sides], int endgamePST[Sides])
{
	openingPST[0] = 0;
	openingPST[1] = 0;
	endgamePST[0] = 0;
	endgamePST[1] = 0;

	for (int square = A1; square <= H8; square++)
	{
		int8_t piece = mailboxBoard64[square];
		if (piece != Empty)
		{
			if (piece > 0)
			{
				openingPST[0] += OpeningPSTs[piece - 1][square ^ 56];
				endgamePST[0] += EndgamePSTs[piece - 1][square ^ 56];
			}
			else
			{
				openingPST[1] += OpeningPSTs[-piece - 1][square];
				endgamePST[1] += EndgamePSTs[-piece - 1][square];
			}
		}
	}
}

bool PSTValuesCorrect(int8_t mailboxBoard64[64], GameRecordEntry_Struct* grp)
{
	int openingPST[Sides];
	int endgamePST[Sides];

	CalculatePSTValues(mailboxBoard64, openingPST, endgamePST);

	return (openingPST[0] == grp->totalOpeningPST[0]) && (openingPST[1] == grp->totalOpeningPST[1]) && (endgamePST[0] == grp->totalEndgamePST[0]) && (endgamePST[1] == grp->totalEndgamePST[1]);
}

void InitialisePSTValues(int8_t mailboxBoard64[64], GameRecordEntry_Struct* grp)
{
	int openingPST[Sides];
	int endgamePST[Sides];

	CalculatePSTValues(mailboxBoard64, openingPST, endgamePST);

	grp->totalOpeningPST[0] = openingPST[0];
	grp->totalOpeningPST[1] = openingPST[1];
	grp->totalEndgamePST[0] = endgamePST[0];
	grp->totalEndgamePST[1] = endgamePST[1];
}

const int GamePhaseIncrement[8] = { 0, 0, 3, 3, 5, 9, 0, 0 };

void InitialiseGamePhase(int8_t mailboxBoard64[64], GameRecordEntry_Struct* grp)
{
	grp->gamePhase[0] = 0;
	grp->gamePhase[1] = 0;

	for (int square = A1; square <= H8; square++)
	{
		int8_t piece = mailboxBoard64[square];
		if (piece != Empty)
		{
			if (piece > 0)
				grp->gamePhase[0] += GamePhaseIncrement[piece];
			else
				grp->gamePhase[1] += GamePhaseIncrement[-piece];
		}
	}
}

//void AllocateMatingPositionsTableMemory()
//{
//	MatingPositionsTablePointer = (uint64_t*)AlignedAllocateMemory(MatingPositionsTableEntries * 8, 64);
//	for (uint32_t entry = 0; entry < MatingPositionsTableEntries; entry++)
//		MatingPositionsTablePointer[entry] = 0;
//}

//void FreeMatingPositionsTableMemory()
//{
//	AlignedFreeMemory(MatingPositionsTablePointer);
//	MatingPositionsTablePointer = NULL;
//}

void InitialiseOneOffStuff()
{
	IsDebug = false;
#ifdef _DEBUG
	ShowPVTerminators = true;
	ShowBlankLines = true;
#else
	ShowPVTerminators = false;
	ShowBlankLines = false;
#endif

	// Initialise the time controls
	TC.CurrentType = TCTWholeGame;
	TC.TournamentMoves1 = 0;
	TC.TournamentMoves2 = 0;
	TC.TournamentTime1Seconds = 0;
	TC.TournamentTime2Seconds = 0;
	TC.AverageSeconds = 10;
	TC.WholeGameSeconds = 300;
	TC.MatchRateAllowed = true;
	TC.FixedTimeMilliSeconds = 10;
	TC.FixedDepthPly = Normal::MaximumIterationPly;
	TC.OddFixedDepthPlyOnly = true;
	TC.FixedNodesCount = 10000;
	TC.MateInN = 1;
	TC.MateFullWidth = false;
	TC.MateAllChecks = false;
	TC.MateAllThreateningMateInOne = false;
	TC.MateMaximumDefenderKingMoves = 8;
	TC.MateMaximumDefenderMovablePieces = 16;
	TC.MateMaximumDefenderMoves = 218;
	TC.MateMaximumReversibleMoves = 100;
	TC.MateFixedPieces = "";

	// Initialise data structures

	// Initialise transposition table (N.B. can't 'clear' it here because it won't have been allocated yet!)
	InitialiseTranspositionTableRandomValues();
	TranspositionTableAge = 0;

	InitialiseBitBoardLists();

	InitialiseDistances();

	for (int i = 0; i < 256; i++)
		Reductions[i] = sqrt(i);

	EngineBrain.gameRecordPointer = &EngineBrain.gameRecord[0];
	for (int i = 0; i < EngineBrain.gameRecordSize; i++)
		EngineBrain.gameRecord[i].excludedMove = 0;

	// NNUE
	//nnue_init("nn-62ef826d1a6d.nnue");
	////nncpu_init("nn-62ef826d1a6d.nnue"); //SF13
	////nnue_init("nn-3475407dc199.nnue");
	////nncpu_init("nn-3475407dc199.nnue"); //SF14
	////nncpu_init("nn-13406b1dcbe0.nnue"); //SF14.1
}

//----------------------------------------------------------------------------------------------------

bool MakeMove(std::string move)
{
	// Used outside the tree search
	// TODO: Must validate?? Or is it the hosts responsibility?
	char promotionPiece;
	int delta;


	// Get the from square, to square and flag
	EngineBrain.gameRecordPointer->move.mf.fromSquare = (SquaresEnum)((move[0] - 'A') + 8 * (move[1] - '1'));
	EngineBrain.gameRecordPointer->move.mf.toSquare = (SquaresEnum)((move[2] - 'A') + 8 * (move[3] - '1'));
	EngineBrain.gameRecordPointer->move.mf.flag = MFNormal;

	// Pawn promotion?
	if (move.length() > 4)
	{
		promotionPiece = move[4];
		switch (promotionPiece)
		{
		case 'Q':
			EngineBrain.gameRecordPointer->move.mf.flag = MFPromoteToQueen;
			break;
		case 'R':
			EngineBrain.gameRecordPointer->move.mf.flag = MFPromoteToRook;
			break;
		case 'B':
			EngineBrain.gameRecordPointer->move.mf.flag = MFPromoteToBishop;
			break;
		case 'N':
			EngineBrain.gameRecordPointer->move.mf.flag = MFPromoteToKnight;
			break;
		}
	}

	// Special case flags (en-passant, castling)
	if (abs(EngineBrain.mailboxBoard64[EngineBrain.gameRecordPointer->move.mf.fromSquare]) == Pawn)
	{
		if (EngineBrain.mailboxBoard64[EngineBrain.gameRecordPointer->move.mf.toSquare] == Empty)
		{
			delta = abs(EngineBrain.gameRecordPointer->move.mf.toSquare - EngineBrain.gameRecordPointer->move.mf.fromSquare);
			if ((delta == 7) || (delta == 9))
			{
				EngineBrain.gameRecordPointer->move.mf.flag = MFEnPassant;
				EngineBrain.gameRecordPointer->move.mf.toSquare = (SquaresEnum)(EngineBrain.gameRecordPointer->move.mf.toSquare - PawnMoveOffset[SideToMove]);
			}
		}
	}
	if (abs(EngineBrain.mailboxBoard64[EngineBrain.gameRecordPointer->move.mf.fromSquare]) == King)
	{
		if (UCI_Chess960)
		{
			if (abs(EngineBrain.mailboxBoard64[EngineBrain.gameRecordPointer->move.mf.toSquare]) == Rook) // GUIs send Chess960 castling moves as 'king takes own rook' but we want 'K to G1/8 or C1/8'
				if ((EngineBrain.mailboxBoard64[EngineBrain.gameRecordPointer->move.mf.fromSquare] > 0) == (EngineBrain.mailboxBoard64[EngineBrain.gameRecordPointer->move.mf.toSquare] > 0))
				{
					EngineBrain.gameRecordPointer->move.mf.flag = MFCastling;

					if ((EngineBrain.gameRecordPointer->move.mf.toSquare & 7) == InitialKingSideRookFile)
						EngineBrain.gameRecordPointer->move.mf.toSquare = (SquaresEnum)(EngineBrain.gameRecordPointer->move.mf.toSquare - InitialKingSideRookFile + G);
					else
						EngineBrain.gameRecordPointer->move.mf.toSquare = (SquaresEnum)(EngineBrain.gameRecordPointer->move.mf.toSquare - InitialQueenSideRookFile + C);
				}
		}
		else
		{
			if (abs(EngineBrain.gameRecordPointer->move.mf.toSquare - EngineBrain.gameRecordPointer->move.mf.fromSquare) == 2)
				EngineBrain.gameRecordPointer->move.mf.flag = MFCastling;
		}
	}

	EngineBrain.MakeMove(SideToMove);

	return true;
}

bool ConvertFENToPosition(std::string position, std::string sideToMove, std::string castling, std::string ep, std::string irreversible, std::string moveNumber, Brain& brain)
{
	// Normal e.g. rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
	// FRC/Chess960 e.g. bnrknbqr/pppppppp/8/8/8/8/PPPPPPPP/BNRKNBQR w HChc - 0 1

	// The "Ghost in the Machine" 10% slower Perft bug!
	// When preparing Colossus for SMP I noticed that 'innocent' edits sometimes caused Perft to be 10% slower! :O
	// It appears to be some sort of alignment issue (code or data?) but has proven difficult to identify.
	// I tried adding a lot of 'alignas(64)' to array declarations which seemed slightly beneficial but did not address the overall problem.
	// I did however find that the 'dummy' statement below could be used to switch the bug on/off by adding/removing it!
	//AC1 = 0;

	int row, column, index;
	char c;

	// Set board to Empty in case FEN std::string faulty
	ClearMailboxBoard64(brain.mailboxBoard64);

	// Process the 'position' element
	row = 7;
	column = 0;
	index = 0;

	while (index < position.length())
	{
		c = position.data()[index++];

		switch (c)
		{
		case 'P':
			brain.mailboxBoard64[row * 8 + column] = Pawn;
			column++;
			break;
		case 'N':
			brain.mailboxBoard64[row * 8 + column] = Knight;
			column++;
			break;
		case 'B':
			EngineBrain.mailboxBoard64[row * 8 + column] = Bishop;
			column++;
			break;
		case 'R':
			brain.mailboxBoard64[row * 8 + column] = Rook;
			column++;
			break;
		case 'Q':
			brain.mailboxBoard64[row * 8 + column] = Queen;
			column++;
			break;
		case 'K':
			brain.mailboxBoard64[row * 8 + column] = King;
			column++;
			break;

		case 'p':
			brain.mailboxBoard64[row * 8 + column] = -Pawn;
			column++;
			break;
		case 'n':
			brain.mailboxBoard64[row * 8 + column] = -Knight;
			column++;
			break;
		case 'b':
			brain.mailboxBoard64[row * 8 + column] = -Bishop;
			column++;
			break;
		case 'r':
			brain.mailboxBoard64[row * 8 + column] = -Rook;
			column++;
			break;
		case 'q':
			brain.mailboxBoard64[row * 8 + column] = -Queen;
			column++;
			break;
		case 'k':
			brain.mailboxBoard64[row * 8 + column] = -King;
			column++;
			break;


		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
			column += c - '1' + 1;
			break;

		case '/':
			row--;
			column = 0;
			break;

		default:
			std::string s(1, c);
			Output("info string *** Illegal character '" + s + "' in FEN string");
			return false;
		}
	}

	// Process the 'side-to-move' element
	if (UpperCase(sideToMove) == "W")
		SideToMove = 0;
	else if (UpperCase(sideToMove) == "B")
		SideToMove = 1;
	else
	{
		Output("info string *** Illegal character '" + sideToMove + "' in the 'side-to-move' element of the FEN string");
		return false;
	}
	brain.gameRecord[2].sideToMove = SideToMove;

	// Process the castling element e.g. KQkq, HAha(Chess960/FRC)
	brain.gameRecord[2].castlingStatus.ui8[0][0] = 1; // Assume no castling allowed initially
	brain.gameRecord[2].castlingStatus.ui8[0][1] = 1;
	brain.gameRecord[2].castlingStatus.ui8[1][0] = 1;
	brain.gameRecord[2].castlingStatus.ui8[1][1] = 1;
	if ((castling != "-") && (castling != ""))
	{
		if (UCI_Chess960)
		{
			// Clear the king/rook files
			InitialKingFile = -1;
			InitialKingSideRookFile = -1;
			InitialQueenSideRookFile = -1;

			for (int count = 0; count < castling.length(); count++)
			{
				char c = castling[count];
				if (c >= 'A' && c <= 'Z') // Uppercase?
				{
					for (int8_t square = A1; square <= H1; square++)
						if (brain.mailboxBoard64[square] == King)
						{
							InitialKingFile = square & 7;
							break;
						}

					if (c == 'K')
					{
						brain.gameRecord[2].castlingStatus.ui8[0][0] = 0;
						for (int8_t square = H1; square >= A1; square--) // Find the rightmost rook
							if (brain.mailboxBoard64[square] == Rook)
							{
								InitialKingSideRookFile = square & 7;
								break;
							}
					}
					else if (c == 'Q')
					{
						brain.gameRecord[2].castlingStatus.ui8[0][1] = 0;
						for (int8_t square = A1; square <= H1; square++) // Find the leftmost rook
							if (brain.mailboxBoard64[square] == Rook)
							{
								InitialQueenSideRookFile = square & 7;
								break;
							}
					}
					else
					{
						int8_t rookfile = 7 - ('H' - c);
						if (rookfile > InitialKingFile)
						{
							brain.gameRecord[2].castlingStatus.ui8[0][0] = 0;
							InitialKingSideRookFile = rookfile;
						}
						else
						{
							brain.gameRecord[2].castlingStatus.ui8[0][1] = 0;
							InitialQueenSideRookFile = rookfile;
						}
					}
				}
				else
				{
					for (int8_t square = A8; square <= H8; square++)
						if (brain.mailboxBoard64[square] == -King)
						{
							InitialKingFile = square & 7;
							break;
						}

					if (c == 'k')
					{
						brain.gameRecord[2].castlingStatus.ui8[1][0] = 0;
						for (int8_t square = H8; square >= A8; square--) // Find the rightmost rook
							if (brain.mailboxBoard64[square] == -Rook)
							{
								InitialKingSideRookFile = square & 7;
								break;
							}
					}
					else if (c == 'q')
					{
						brain.gameRecord[2].castlingStatus.ui8[1][1] = 0;
						for (int8_t square = A8; square <= H8; square++) // Find the leftmost rook
							if (brain.mailboxBoard64[square] == -Rook)
							{
								InitialQueenSideRookFile = square & 7;
								break;
							}
					}
					else
					{
						int8_t rookfile = 7 - ('h' - c);
						if (rookfile > InitialKingFile)
						{
							brain.gameRecord[2].castlingStatus.ui8[1][0] = 0;
							InitialKingSideRookFile = rookfile;
						}
						else
						{
							brain.gameRecord[2].castlingStatus.ui8[1][1] = 0;
							InitialQueenSideRookFile = rookfile;
						}
					}
				}

			}
		}
		else
		{
			if (castling.find_first_of("K") != -1)
				brain.gameRecord[2].castlingStatus.ui8[0][0] = 0;
			if (castling.find_first_of("Q") != -1)
				brain.gameRecord[2].castlingStatus.ui8[0][1] = 0;
			if (castling.find_first_of("k") != -1)
				brain.gameRecord[2].castlingStatus.ui8[1][0] = 0;
			if (castling.find_first_of("q") != -1)
				brain.gameRecord[2].castlingStatus.ui8[1][1] = 0;
			InitialKingFile = E;
			InitialKingSideRookFile = H;
			InitialQueenSideRookFile = A;
		}
	}

	// Process the en-passant element
	if ((ep != "-") && (ep != ""))
	{
		ep = UpperCase(ep);
		uint8_t enPassantSquare;
		enPassantSquare = (uint8_t)((ep[0] - 'A') + 8 * (ep[1] - '1'));
		brain.gameRecord[2].epSquare = enPassantSquare; // Need this to be initialised for the 'EGTB position at root' code to probe correctly
		// Set root gameRecord move to the double pawn push
		if (enPassantSquare <= H3)
		{
			brain.gameRecord[1].move.ui32 = (enPassantSquare - 8) | ((enPassantSquare + 8) << 8);// | (MFPawnMove << 16);
			brain.gameRecord[1].move.fromSquarePiece = Pawn;
		}
		else
		{
			brain.gameRecord[1].move.ui32 = (enPassantSquare + 8) | ((enPassantSquare - 8) << 8);// | (MFPawnMove << 16);
			brain.gameRecord[1].move.fromSquarePiece = -Pawn;
		}
	}

	// Process the 'irreversible' element
	if (irreversible != "")
		brain.gameRecord[2].pliesSinceIrreversible = std::stoi(irreversible);

	// Process the move number element
	if (moveNumber != "")
		brain.gameRecord[2].moveNumber = std::stoi(moveNumber);

	return true;
}

void SetPositionAndMoves(std::string positionAndMoves)
{
	// syntax: position [fen <fenstring> | startpos] moves <move1> ... <movei>
	// FEN
	// Forsythe-Edwards Notation.  A compact representation for chess
	// positions.  FEN specifies the piece placement, the active color, the
	// castling availability, the en passant target square, the halfmove
	// clock, and the fullmove number as six fields separated by spaces.
	// For example, the opening position is described in FEN as follows:
	//
	// rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1

	TC.MateFixedPieces = ""; // When we receive a new position ensure we clear any previous mate tuning options
	TC.MateMaximumDefenderKingMoves = 8;
	TC.MateMaximumDefenderMovablePieces = 16;
	TC.MateMaximumDefenderMoves = 218;
	TC.MateMinimumAttackerMaterial = 0;
	TC.MateMaximumReversibleMoves = 100;


	std::string token;
	std::string s = positionAndMoves;
	std::string move;
	std::string bufferSTRING;

	// Save for error reporting
	LastPositionAndMoves = positionAndMoves;

	// Split console line into tokens
	token = GetNextToken(&positionAndMoves);

	token = UpperCase(GetNextToken(&positionAndMoves));
	if (token == "STARTPOS")
	{
		NewGame(false); // Don't want to clear things like TT, killers, etc between moves in an actual game! Also calls ClearGameRecord
	}
	else if (token == "FEN")
	{
		// Process FEN std::string
		EngineBrain.ClearGameRecord(); // Must do BEFORE processing the FEN string below so as not to overwrite any EP square/castling statuses etc specified in the FEN string


		std::string fen;
		size_t movesIndex = positionAndMoves.find("moves");
		if (movesIndex == -1)
		{
			fen = positionAndMoves;
		}
		else
		{
			fen = positionAndMoves.substr(0, movesIndex);
			positionAndMoves = positionAndMoves.substr(movesIndex);
			trim(fen);
		}

		std::string fenTokens[6];
		int fenTokenCount;
		Split(fen, &fenTokens[0], &fenTokenCount, " \t");

		if (fenTokenCount >= 2)
		{
			bool valid = ConvertFENToPosition(fenTokens[0], fenTokens[1], fenTokens[2], fenTokens[3], fenTokens[4], fenTokens[5], EngineBrain);
			if (!valid)
				return;
		}
	}
	else
	{
		Output("info string *** Error! A 'position' command must be followed by 'startpos' or 'fen'!");
		return;
	}

	// Set up the bit boards from the 64-square mailbox board
	ConvertMailboxBoard64ToPiecesBB(EngineBrain.mailboxBoard64, EngineBrain.piecesBB);

	// The calls to ClearGameRecord above should have set GameRecordIndexRoot to 2
	assert(EngineBrain.GameRecordIndexRoot == 2);
	EngineBrain.gameRecordPointer = &EngineBrain.gameRecord[EngineBrain.GameRecordIndexRoot];

	// Ensure that the root game record entry has correct values
	InitialiseMaterialValues(EngineBrain.mailboxBoard64, EngineBrain.gameRecordPointer);
	InitialiseGamePhase(EngineBrain.mailboxBoard64, EngineBrain.gameRecordPointer);

	EngineBrain.gameRecordPointer->transpositionTableHash64 = GenerateTranspositionTableHash64(EngineBrain.mailboxBoard64, EngineBrain.gameRecordPointer);
	if (SideToMove == 1)
		EngineBrain.gameRecordPointer->transpositionTableHash64 = ~EngineBrain.gameRecordPointer->transpositionTableHash64;
	EngineBrain.gameRecordPointer->transpositionTableHash64WithEP = EngineBrain.gameRecordPointer->transpositionTableHash64 ^ TranspositionTableRandomsEnPassant[EngineBrain.gameRecordPointer->epSquare];

	token = UpperCase(GetNextToken(&positionAndMoves));
	if (token == "MOVES")
	{
		// Read game record from command line std::string
		// GameRecordIndexRoot should be 2 here
		while (positionAndMoves != "")
		{
			assert(CompareMailboxBoard64ToPiecesBB(EngineBrain.mailboxBoard64, EngineBrain.piecesBB));
			assert(EngineBrain.gameRecordPointer->transpositionTableHash64 == ((SideToMove == 0) ? GenerateTranspositionTableHash64(EngineBrain.mailboxBoard64, EngineBrain.gameRecordPointer) : ~GenerateTranspositionTableHash64(EngineBrain.mailboxBoard64, EngineBrain.gameRecordPointer)));

			move = UpperCase(GetNextToken(&positionAndMoves));
			MakeMove(move);

			EngineBrain.GameRecordIndexRoot++; // UGIGenerate.gameRecordPointer gets incremented in the internal MakeMove call above
			AdvanceSideToMove();

			EngineBrain.gameRecordPointer->moveNumber = EngineBrain.gameRecord[2].moveNumber + ((EngineBrain.GameRecordIndexRoot - 2) / 2);
			EngineBrain.gameRecordPointer->sideToMove = SideToMove;
		}
	}
}

//----------------------------------------------------------------------------------------------------

#ifdef EXPERIMENTAL

//void GenerateKP2()
//{
//	int rank, file;
//
//	for (int pawnPattern = 0; pawnPattern < 256; pawnPattern++)
//	{
//		ClearMailboxBoard64(EngineBrain.mailboxBoard64);
//
//		int bitMask = 1;
//		for (int bit = 0; bit < 8; bit++)
//		{
//			int pawnSquare = A2 + bit;
//			EngineBrain.mailboxBoard64[pawnSquare] = Empty;
//			EngineBrain.mailboxBoard64[pawnSquare + 8] = Empty;
//			if (pawnPattern & bitMask)
//				pawnSquare += 8;
//			EngineBrain.mailboxBoard64[pawnSquare] = Pawn;
//			rank = pawnSquare / 8;
//			file = pawnSquare % 8;
//			EngineBrain.mailboxBoard64[(7 - rank) * 8 + file] = -Pawn;
//
//			bitMask <<= 1;
//		}
//
//		for (int kingSquare = A1; kingSquare <= H3; kingSquare++)
//		{
//			if (EngineBrain.mailboxBoard64[kingSquare] == Empty)
//			{
//				EngineBrain.mailboxBoard64[kingSquare] = King;
//				rank = kingSquare / 8;
//				file = kingSquare % 8;
//				EngineBrain.mailboxBoard64[(7 - rank) * 8 + file] = -King;
//
//				GameRecordCastlingStatusUnion castlingStatus;
//				castlingStatus.ui32 = 0x01010101;
//				Output(ConvertPositionToFEN(EngineBrain.mailboxBoard64, 0, castlingStatus, 0, 0, 1));
//
//				EngineBrain.mailboxBoard64[kingSquare] = Empty;
//				EngineBrain.mailboxBoard64[(7 - rank) * 8 + file] = Empty;
//			}
//		}
//	}
//}

//uint32_t egtbResultWDL(Brain* b, int sideToMove)
//{
//	return tb_probe_wdl(
//		b->piecesBB[0][AllPieces],
//		b->piecesBB[1][AllPieces],
//		b->piecesBB[0][King] | b->piecesBB[1][King],
//		b->piecesBB[0][Queen] | b->piecesBB[1][Queen],
//		b->piecesBB[0][Rook] | b->piecesBB[1][Rook],
//		b->piecesBB[0][Bishop] | b->piecesBB[1][Bishop],
//		b->piecesBB[0][Knight] | b->piecesBB[1][Knight],
//		b->piecesBB[0][Pawn] | b->piecesBB[1][Pawn],
//		0,
//		0,
//		0,//b.gameRecordPointer->epSquare,
//		(sideToMove == 0)
//	);
//}

//uint32_t egtbResultDTZ(Brain* b, int sideToMove)
//{
//	return tb_probe_root(
//		b->piecesBB[0][AllPieces],
//		b->piecesBB[1][AllPieces],
//		b->piecesBB[0][King] | b->piecesBB[1][King],
//		b->piecesBB[0][Queen] | b->piecesBB[1][Queen],
//		b->piecesBB[0][Rook] | b->piecesBB[1][Rook],
//		b->piecesBB[0][Bishop] | b->piecesBB[1][Bishop],
//		b->piecesBB[0][Knight] | b->piecesBB[1][Knight],
//		b->piecesBB[0][Pawn] | b->piecesBB[1][Pawn],
//		0,
//		0,
//		0,//b.gameRecordPointer->epSquare,
//		(sideToMove == 0),
//		NULL
//	);
//}

//std::string PST(double pst[64])
//{
//	std::string result;
//
//	for (int rank = 7; rank >= 0; rank--)
//	{
//		std::string line = "";
//
//		for (int file = 0; file <= 7; file++)
//		{
//			line += MyDTOA(pst[rank * 8 + file]);
//			line += " ";
//		}
//		result = result + line + " " + "\r\n";
//	}
//	result = result + "\r\n";
//
//	return result;
//}

//void KBNvK() // Longest win is 66 ply
//{
//	const int histogram1[66] = { 0, 230, 0, 150, 0, 402, 0, 1995, 0, 4974, 0, 5649, 0, 6059, 0, 5053, 0, 5345, 0, 7034, 0, 11903, 0, 17764, 0, 23702, 0, 23793, 0, 17463, 0, 14754, 0, 14278, 0, 14975, 0, 15937, 0, 27070, 0, 43776, 0, 60617, 0, 72112, 0, 73059, 0, 91133, 0, 118729, 0, 159805, 0, 191450, 0, 179298, 0, 107435, 0, 32633, 0, 4198, 0, 138 };
//	const int histogram2[67] = { 58, 0, 39, 0, 37, 0, 224, 0, 1008, 0, 1399, 0, 1899, 0, 1485, 0, 1826, 0, 1746, 0, 3124, 0, 6167, 0, 8568, 0, 10611, 0, 7871, 0, 6242, 0, 6918, 0, 6840, 0, 6748, 0, 9857, 0, 17849, 0, 29545, 0, 42264, 0, 45432, 0, 59226, 0, 81465, 0, 124734, 0, 169242, 0, 226955, 0, 253469, 0, 185651, 0, 69191, 0, 10681, 0, 370 };
//
//
//	Brain b;
//	b.CopyFrom(&EngineBrain);
//	b.gameRecordPointer = &b.gameRecord[b.GameRecordIndexRoot];
//
//	ClearPiecesBB(b.piecesBB);
//
//	uint64_t positionsCount = 0;
//	uint64_t wtm[5], btm[5];
//	double wkpst[64], wbpst[64], wnpst[64], bkpst[64];
//	uint32_t resultWDL, resultDTZ;
//	//NEED TO ACCUMULATE WIN/LOSS DISTANCES TO WEIGHT PSTs (do Ks 1st to see how the heat map looks)
//	//USE # OF DRAWS TO SCALE DOWN PSQT TOWARDS ZERO
//
//	for (int square = 0; square < 64; square++)
//	{
//		wkpst[square] = 0;
//		wbpst[square] = 0;
//		wnpst[square] = 0;
//		bkpst[square] = 0;
//	}
//
//	for (int count = 0; count < 5; count++)
//	{
//		wtm[count] = 0;
//		btm[count] = 0;
//	}
//
//	uint32_t longestWin = 66;
//
//	for (int wk = 0; wk < 64; wk++)
//	{
//		Output(MyITOA(wk));
//		Output("");
//
//		for (int wb = 0; wb < 64; wb++)
//			if (wb != wk)
//			{
//				uint64_t wbBB = UINT64SetBit(wb);
//				if (wbBB & LightBB)
//				{
//					for (int wn = 0; wn < 64; wn++)
//						if ((wn != wb) && (wn != wk))
//							for (int bk = 0; bk < 64; bk++)
//								if ((bk != wn) && (bk != wb) && (bk != wk))
//									if (ChebyshevDistance[bk][wk] > 1)
//									{
//										b.piecesBB[0][King] = UINT64SetBit(wk);
//										//b.piecesBB[0][Queen] = 0;
//										//b.piecesBB[0][Rook] = 0;
//										b.piecesBB[0][Bishop] = UINT64SetBit(wb);
//										b.piecesBB[0][Knight] = UINT64SetBit(wn);
//										//b.piecesBB[0][Pawn] = 0;
//										b.piecesBB[1][King] = UINT64SetBit(bk);
//										//b.piecesBB[1][Queen] = 0;
//										//b.piecesBB[1][Rook] = 0;
//										//b.piecesBB[1][Bishop] = 0;
//										//b.piecesBB[1][Knight] = 0;
//										//b.piecesBB[1][Pawn] = 0;
//										b.piecesBB[0][AllPieces] = b.piecesBB[0][King] | b.piecesBB[0][Bishop] | b.piecesBB[0][Knight];
//										b.piecesBB[1][AllPieces] = b.piecesBB[1][King];
//
//
//										// White to move
//										if (b.IsEnemyKingAttacked(bk, 0))
//											goto BlackToMove;
//
//										positionsCount++;
//										resultWDL = egtbResultWDL(&b, 0);
//										if (resultWDL != TB_RESULT_FAILED)
//										{
//											wtm[resultWDL]++;
//
//											if (resultWDL != TB_DRAW)
//											{
//												resultDTZ = egtbResultDTZ(&b, 0);
//												uint32_t wdl = TB_GET_WDL(resultDTZ);
//												uint32_t dtz = TB_GET_DTZ(resultDTZ);
//												if (dtz == 1)
//													dtz = 0;
//												assert((dtz & 1) == 0);
//
//												double divisor = histogram1[dtz];
//												if (divisor == 0)
//													divisor = histogram2[dtz];
//												assert(divisor != 0);
//
//												double d = (longestWin - dtz) / divisor;
//												wkpst[wk] += d;
//												wbpst[wb] += d;
//												wnpst[wn] += d;
//												bkpst[bk] -= dtz / divisor;
//											}
//										}
//
//										// Black to move
//									BlackToMove:
//										if (b.IsEnemyKingAttacked(wk, 1))
//											continue;
//
//										positionsCount++;
//										resultWDL = egtbResultWDL(&b, 1);
//										if (resultWDL != TB_RESULT_FAILED)
//										{
//											btm[resultWDL]++;
//
//										}
//									}
//				}
//			}
//	}
//
//	Output(MyITOA(positionsCount));
//	Output("");
//	for (int count = 0; count < 5; count++)
//		Output(MyITOA(wtm[count]));
//	Output("");
//	for (int count = 0; count < 5; count++)
//		Output(MyITOA(btm[count]));
//	Output("");
//
//	Output(MyFTOA((float)((wtm[4] + btm[0]) * 100) / (float)positionsCount));
//	Output(MyFTOA((float)((wtm[2] + btm[2]) * 100) / (float)positionsCount));
//	Output("");
//
//	Output(PST(wkpst));
//	Output(PST(wbpst));
//	Output(PST(wnpst));
//	Output(PST(bkpst));
//
//	Output(MyITOA(positionsCount));
//}


//std::string Subfolder = "BlackKing9QueensAllPiecesTEMP";
std::string Subfolder = "BlackKing9QueensTEMP";

void MaximumMovesQueens()
{
	//COUNT THE # OF POSNS IF WE JUST SCAN ALL WHITE PIECES SLIDING ACROSS THE BOARD WITH NO RESTRICTIONS
	//COULD SPLIT INTO SMALLER CHUNKS VIA THE WHITE K SQ AND MULTITHREAD THE 1ST Q SQS

	int maximumMoves = 0;
	int minimumMoves = INT32_MAX;
	uint64_t count = 0;
	uint64_t count140Plus = 0;
	uint64_t counts[202];
	for (int index = 0; index < 202; index++)
		counts[index] = 0;
	//MoveWithScore_Struct moveList[500];

	ClearPiecesBB(EngineBrain.piecesBB);

	EngineBrain.piecesBB[1][King] = UINT64SetBit(H8); // Put the black king on H8
	EngineBrain.piecesBB[1][AllPieces] = EngineBrain.piecesBB[1][King];
	uint64_t queenRestrictedBB = 0;// UINT64SetBit(H8) | UINT64SetBit(G8) | UINT64SetBit(H7) | UINT64SetBit(G7);
	uint64_t firstQueenAllowedBB = UINT64SetBit(A1) | UINT64SetBit(B1) | UINT64SetBit(C1) | UINT64SetBit(D1) | UINT64SetBit(B2) | UINT64SetBit(C2) | UINT64SetBit(D2) | UINT64SetBit(C3) | UINT64SetBit(D3) | UINT64SetBit(D4);
	// TO CONFIRM THE ABOVE WORKS, CHECK THE COUNTS IN THE FILES AGAINST PREVIOUS 'UNIQUE' COUNTS
	//uint64_t rookRestrictedBB = UINT64SetBit(A1) | UINT64SetBit(B1) | UINT64SetBit(A2);
	
	EngineBrain.gameRecordPointer = &EngineBrain.gameRecord[1];
	EngineBrain.gameRecordPointer->pinnedRankFileBB = 0;
	EngineBrain.gameRecordPointer->pinnedDiagonalBB = 0;

	EngineBrain.gameRecordPointer->castlingStatus.ui8[0][0] = 0;
	EngineBrain.gameRecordPointer->castlingStatus.ui8[0][1] = 0;
	EngineBrain.gameRecordPointer->castlingStatus.ui8[1][0] = 1;
	EngineBrain.gameRecordPointer->castlingStatus.ui8[1][1] = 1;


	// Reflections
	// q1 only needs to test bottom left quadrant??? bottom half??? (16 sqs)
	// WRITE FILES TO SUBDIRS OF EACH # OF MOVES E.G. 201, 200, 199, ETC
	// THEN CAN GRADUALLY PROCESS SUBDIRS OVER TIME
	//DON'T WRITE FILES, BUT WRITE ONE FILE FOR EACH MOVECOUNT CONTAINING THE Q-SQS E.G. 201.TXT
	//THEN WHEN REMOVE DUPLICATES CAN JUST WRITE A NEW FILE WITHOUT THE DUPLICATES E.G. 201X.TXT
	//THEN WHEN GEN POSNS WITH OTHER PIECES (>=218 TO GEN NEW POSNS @218) CAN WRITE THEM WITH EACH NEW MOVE COUNT E.G. 201X219.TXT


	// The maximum number of moves that nine queens can contribute is 201
	// The maximum number of moves that eight queens and a rook can contribute is 194
	// The maximum number of moves that the remaining pieces can add is...
		// 2*14 + 2*13 + 2*8 + 8 = 78 e.g. 8/2K2k2/R7/8/2NBBN2/8/7R/8 w - - 0 1
	// Therefore any position where the queens contribute <= 218-78=140 are irrelevant (COUNT THESE!!! IN AN ARRAY... ONE COUNT FOR EVERY #OFMOVES)
	// Also note that without any rooks the maximum number of moves that the remaining pieces can add is 50
	// Also note that without any bishops the maximum number of moves that the remaining pieces can add is 52
	// Also note that without any knights the maximum number of moves that the remaining pieces can add is 62

	// The maximum number of 'rook' moves that 9 queens and 2 rooks can contribute is 11*14=154
	// The maximum number of 'bishop' moves that 9 queens and 2 bishops can contribute is 11*13=143
	// The maximum number of moves that 2 knights can contribute is 16
	// The maximum number of moves that the king can contribute is 8

	// 9 queens {no duplicates???}: count = ~15B?, maximumMoves = 201, minimumMoves = 61
	for (int q1 = 0; q1 < 64 - 8; q1++) // 9 queens: count = 27,540,584,512, maximumMoves = 201, minimumMoves = 61 (Black king + 9 queens: 14,783,142,660, 201, 61)
	//for (int q1 = 8; q1 < 64 - 8; q1++) // 9 queens: count = 7,575,968,400, maximumMoves = 195, minimumMoves = 61
	{
		Output(MyITOA(q1));

		//if ((queenRestrictedBB & UINT64SetBit(q1)) == 0)
		if ((firstQueenAllowedBB & UINT64SetBit(q1)) != 0)
			for (int q2 = q1 + 1; q2 < 64 - 7; q2++)
				if ((queenRestrictedBB & UINT64SetBit(q2)) == 0)
					for (int q3 = q2 + 1; q3 < 64 - 6; q3++)
						if ((queenRestrictedBB & UINT64SetBit(q3)) == 0)
							for (int q4 = q3 + 1; q4 < 64 - 5; q4++)
								if ((queenRestrictedBB & UINT64SetBit(q4)) == 0)
									for (int q5 = q4 + 1; q5 < 64 - 4; q5++)
										if ((queenRestrictedBB & UINT64SetBit(q5)) == 0)
											for (int q6 = q5 + 1; q6 < 64 - 3; q6++)
												if ((queenRestrictedBB & UINT64SetBit(q6)) == 0)
													for (int q7 = q6 + 1; q7 < 64 - 2; q7++)
														if ((queenRestrictedBB & UINT64SetBit(q7)) == 0)
															for (int q8 = q7 + 1; q8 < 64 - 1; q8++)
																if ((queenRestrictedBB & UINT64SetBit(q8)) == 0)
																	for (int q9 = q8 + 1; q9 < 64 - 0; q9++)
																		if ((queenRestrictedBB & UINT64SetBit(q9)) == 0)
																	//for (int r1 = 0; r1 < 64 - 0; r1++)
																	//	if ((rookRestrictedBB & UINT64SetBit(r1)) == 0)
																	//		if ((r1 != q1) && (r1 != q2) && (r1 != q3) && (r1 != q4) && (r1 != q5) && (r1 != q6) && (r1 != q7) && (r1 != q8))
																		{
																			count++;

																			EngineBrain.piecesBB[0][Queen] = 0;
																			EngineBrain.piecesBB[0][Queen] |= UINT64SetBit(q1);
																			EngineBrain.piecesBB[0][Queen] |= UINT64SetBit(q2);
																			EngineBrain.piecesBB[0][Queen] |= UINT64SetBit(q3);
																			EngineBrain.piecesBB[0][Queen] |= UINT64SetBit(q4);
																			EngineBrain.piecesBB[0][Queen] |= UINT64SetBit(q5);
																			EngineBrain.piecesBB[0][Queen] |= UINT64SetBit(q6);
																			EngineBrain.piecesBB[0][Queen] |= UINT64SetBit(q7);
																			EngineBrain.piecesBB[0][Queen] |= UINT64SetBit(q8);
																			EngineBrain.piecesBB[0][Queen] |= UINT64SetBit(q9);
																			//PiecesBB[0][Rook] = UINT64SetBit(r1);
																			EngineBrain.piecesBB[0][AllPieces] = EngineBrain.piecesBB[0][Queen];
																			//PiecesBB[0][AllPieces] = PiecesBB[0][Queen] | PiecesBB[0][Rook];

																			//int moves = GenerateAllMoves(0, false, moveList);
																			int moves = EngineBrain.CountAllQueenMovesMM(0);

																			counts[moves]++;
																			//if ((moves > maximumMoves) || (moves >= 200))
																			if (moves >= 140)
																				count140Plus++;
																			if (moves >= 180)
																				//if (moves > maximumMoves)
																			{
																				FILE *file;
																				std::string filename, position;
																				position = Notation64[q1] + Notation64[q2] + Notation64[q3] + Notation64[q4] + Notation64[q5] + Notation64[q6] + Notation64[q7] + Notation64[q8] + Notation64[q9];
																				//position = Notation64[q1] + Notation64[q2] + Notation64[q3] + Notation64[q4] + Notation64[q5] + Notation64[q6] + Notation64[q7] + Notation64[q8] + Notation64[r1];
																				//filename = MyITOA(moves) + ".txt";
																				filename = "D:\\Chess\\MaximumMoves\\" + Subfolder + "\\" + MyITOA(moves) + ".txt";
																				fopen_s(&file, filename.c_str(), "a");
																				fprintf(file, (position + "\n").c_str());
																				fclose(file);
																			}
																			if (moves > maximumMoves)
																				maximumMoves = moves;
																			if (moves < minimumMoves)
																				minimumMoves = moves;
																		}
	}

	Output("count = " + MyUI64TOA(count));
	Output("count140Plus = " + MyUI64TOA(count140Plus));
	Output("maximumMoves = " + MyITOA(maximumMoves));
	Output("minimumMoves = " + MyITOA(minimumMoves));
	volatile int i = 99;
}

std::string ConvertMailboxBoard64ToPosition(int8_t mailboxBoard64[64])
{
	std::string result = "";
	for (int square = A1; square <= H8; square++)
		if (mailboxBoard64[square] == Queen)
			result += Notation64[square];
	return result;
}

void MaximumMovesDeleteDuplicates()
{
	FILE *file;
	std::string filename;
	std::string* positions = new std::string[1000000];
	bool* duplicate = new bool[1000000];
	char currentline[100];
	int positionCount;
	int8_t mailboxBoard64ORIGINAL[64];
	int8_t mailboxBoard64RH[64];
	int8_t mailboxBoard64RV[64];
	int8_t mailboxBoard64RLD1[64];
	int8_t mailboxBoard64RLD2[64];
	int8_t mailboxBoard64R90[64];
	int8_t mailboxBoard64R180[64];
	int8_t mailboxBoard64R270[64];

	for (int fileNumber = 180; fileNumber <= 201; fileNumber++)
	{
		Output("File number = " + MyITOA(fileNumber));

		// Read all positions from the 'with duplicates' file into an array
		filename = "D:\\Chess\\MaximumMoves\\" + Subfolder + "\\" + MyITOA(fileNumber) + ".txt";
		fopen_s(&file, filename.c_str(), "r");
		positionCount = 0;
		while (fgets(currentline, sizeof(currentline), file) != NULL) {
			currentline[strcspn(currentline, "\n")] = 0;
			positions[positionCount] = currentline;
			duplicate[positionCount] = false;
			positionCount++;
		}
		fclose(file);
		Output("Positions = " + MyITOA(positionCount));

		// Scan for duplicates
		for (int count1 = 0; count1 < positionCount; count1++)
		{
			if ((count1 & (1024 - 1)) == 0)
				Output(MyITOA(count1));

			if (duplicate[count1])
				continue;

			std::string position1, position2;
			std::string positionRH, positionRV, positionRLD1, positionRLD2, positionR90, positionR180, positionR270;

			position1 = positions[count1];

			// Setup boards from position
			ClearMailboxBoard64(mailboxBoard64ORIGINAL);
			//ClearMailboxBoard64(mailboxBoard64RH);
			//ClearMailboxBoard64(mailboxBoard64RV);
			ClearMailboxBoard64(mailboxBoard64RLD1);
			//ClearMailboxBoard64(mailboxBoard64RLD2);
			//ClearMailboxBoard64(mailboxBoard64R90);
			//ClearMailboxBoard64(mailboxBoard64R180);
			//ClearMailboxBoard64(mailboxBoard64R270);

			for (int squareCount = 1; squareCount <= 9; squareCount++)
			{
				std::string s = position1.substr((squareCount - 1) * 2, 2);
				int square = int(s.substr(0, 1)[0]) - int("a"[0]) + (int(s.substr(1, 1)[0]) - int("1"[0])) * 8;
				mailboxBoard64ORIGINAL[square] = Queen;
				int file = square % 8;
				int rank = square / 8;

				//mailboxBoard64RH[(7 - rank) * 8 + file] = Queen; // Reflect horizontal
				//mailboxBoard64RV[rank * 8 + (7 - file)] = Queen; // Reflect vertical
				mailboxBoard64RLD1[file * 8 + rank] = Queen; // Reflect long diagonal 1 (a1-h8)
				//mailboxBoard64RLD2[(7 - file) * 8 + (7 - rank)] = Queen; // Reflect long diagonal 2 (a8-h1)
				//mailboxBoard64R90[(7 - file) * 8 + rank] = Queen; // Rotate 90
				//mailboxBoard64R180[(7 - rank) * 8 + (7 - file)] = Queen; // Rotate 180
				//mailboxBoard64R270[file * 8 + (7 - rank)] = Queen; // Rotate 270
			}

			//positionRH = ConvertMailboxBoard64ToPosition(mailboxBoard64RH);
			//positionRV = ConvertMailboxBoard64ToPosition(mailboxBoard64RV);
			positionRLD1 = ConvertMailboxBoard64ToPosition(mailboxBoard64RLD1);
			//positionRLD2 = ConvertMailboxBoard64ToPosition(mailboxBoard64RLD2);
			//positionR90 = ConvertMailboxBoard64ToPosition(mailboxBoard64R90);
			//positionR180 = ConvertMailboxBoard64ToPosition(mailboxBoard64R180);
			//positionR270 = ConvertMailboxBoard64ToPosition(mailboxBoard64R270);

			//WriteMailboxBoard64(mailboxBoard64ORIGINAL);
			//WriteMailboxBoard64(mailboxBoard64RH);
			//WriteMailboxBoard64(mailboxBoard64RV);
			//WriteMailboxBoard64(mailboxBoard64RLD1);
			//WriteMailboxBoard64(mailboxBoard64RLD2);
			//WriteMailboxBoard64(mailboxBoard64R90);
			//WriteMailboxBoard64(mailboxBoard64R180);
			//WriteMailboxBoard64(mailboxBoard64R270);

			// Scan for duplicates
			for (int count2 = count1 + 1; count2 < positionCount; count2++)
				if (!duplicate[count2])
				{
					position2 = positions[count2];

					//if (position2 == positionRH)
					//	duplicate[count2] = true;
					//else if (position2 == positionRV)
					//	duplicate[count2] = true;
					//else 
					if (position2 == positionRLD1)
						duplicate[count2] = true;
					//else if (position2 == positionRLD2)
					//	duplicate[count2] = true;
					//else if (position2 == positionR90)
					//	duplicate[count2] = true;
					//else if (position2 == positionR180)
					//	duplicate[count2] = true;
					//else if (position2 == positionR270)
					//	duplicate[count2] = true;
				}

		}

		// Write out all the non-duplicates
		filename = "D:\\Chess\\MaximumMoves\\" + Subfolder + "\\" + MyITOA(fileNumber) + "ND.txt";
		fopen_s(&file, filename.c_str(), "w");

		for (int count = 0; count < positionCount; count++)
			if (!duplicate[count])
				fprintf(file, (positions[count] + "\n").c_str());

		fclose(file);
	}

	Output("Done!");
}

void MaximumMoves(int inputFileNumber)
{
	// Assumptions
	// Black king in corner (H8)
	// 9 white queens
	// No castling
	// All remaining white pieces used
	// All remaining white pieces on edge or inner edge



	//WHEN WRITING THESE OUT COULD COUNT THE # OF RAY CHECKS AND SUBTRACT THAT! IF <218 THEN DISCARD!
	//OR JUST DON'T COUNT CAPTURES OF THE BLACK KING IN THE TOTAL! THIS WOULD REDUCE THE # OF POSNS IN THE HIGHER Q FILES!
	//COULD DISCARD POSNS ALTOGETHER WHEN THE BLACK K IS ATTACKED? I.E. JUST LOOKING FOR POSNS WITH MAX MOVES AND NO OTHER BLACK PIECE SHELTER
	//THEN EXPAND TO INCLUDE ONE BLACK PAWN GIVING SHELTER ETC

	//COULD 'FIX' CERTAIN WHITE PIECES AROUND THE BLACK KING TO GIVE IT SHELTER WHICH WOULD REDUCE THE REMAINING PERMUTATIONS E.G. WR ON G7, WNs ON G8,H7




	//WaitForSingleObject for multithreading???
	// launch 64(55) threads for each k sq?
	//https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/beginthread-beginthreadex?f1url=%3FappId%3DDev15IDEF1%26l%3DEN-US%26k%3Dk(PROCESS%252F_beginthread);k(_beginthread);k(DevLang-C%252B%252B);k(TargetOS-Windows)%26rd%3Dtrue&view=msvc-160
	
	//MoveWithScore_Struct moveList[500];

	FILE *file;
	std::string filename;
	std::string* positions = new std::string[1000000];
	char currentline[100];
	int positionCount;

	// Process each ???ND.txt files
	//for (int fileNumber = 201; fileNumber >= 190; fileNumber--)
	for (int fileNumber = inputFileNumber; fileNumber >= inputFileNumber; fileNumber--)
	{
		Output("File number = " + MyITOA(fileNumber));

		// Read positions into array
		//filename = "D:\\Chess\\MaximumMoves\\" + MyITOA(fileNumber) + "ND.txt";
		filename = "D:\\Chess\\MaximumMoves\\" + Subfolder + "\\" + MyITOA(fileNumber) + "ND.txt";
		fopen_s(&file, filename.c_str(), "r");
		positionCount = 0;
		while (fgets(currentline, sizeof(currentline), file) != NULL) {
			currentline[strcspn(currentline, "\n")] = 0;
			positions[positionCount] = currentline;
			positionCount++;
		}
		fclose(file);
		Output("Positions = " + MyITOA(positionCount));

		for (int count1 = 0; count1 < positionCount; count1++)
		{
			std::string position = positions[count1];
			Output("File number = " + MyITOA(fileNumber) + ", Position " + MyITOA(count1) + " = " + position);

			ClearPiecesBB(EngineBrain.piecesBB);

			EngineBrain.piecesBB[1][King] |= UINT64SetBit(H8); // Put the black king on A1
			EngineBrain.piecesBB[1][AllPieces] = EngineBrain.piecesBB[1][King];
			uint64_t queenRestrictedBB = UINT64SetBit(H8) | UINT64SetBit(G8) | UINT64SetBit(H7) | UINT64SetBit(G7);
			uint64_t kingRestrictedBB = queenRestrictedBB;
			uint64_t rookRestrictedBB = UINT64SetBit(H8) | UINT64SetBit(G8) | UINT64SetBit(H7);
			uint64_t bishopRestrictedBB = UINT64SetBit(H8) | UINT64SetBit(G7);
			uint64_t knightRestrictedBB = UINT64SetBit(H8) | UINT64SetBit(F7) | UINT64SetBit(G6);

			// Setup Queen and AllPieces bitboard from filename
			EngineBrain.piecesBB[0][Queen] = 0;
			for (int count = 1; count <= 9; count++)
			{
				std::string s = position.substr((count - 1) * 2, 2);
				int square = int(s.substr(0, 1)[0]) - int("a"[0]) + (int(s.substr(1, 1)[0]) - int("1"[0])) * 8;
				EngineBrain.piecesBB[0][Queen] |= UINT64SetBit(square);
			}

			EngineBrain.gameRecordPointer = &EngineBrain.gameRecord[1];
			EngineBrain.gameRecordPointer->pinnedRankFileBB = 0;
			EngineBrain.gameRecordPointer->pinnedDiagonalBB = 0;

			EngineBrain.gameRecordPointer->castlingStatus.ui8[0][0] = 0;
			EngineBrain.gameRecordPointer->castlingStatus.ui8[0][1] = 0;
			EngineBrain.gameRecordPointer->castlingStatus.ui8[1][0] = 1;
			EngineBrain.gameRecordPointer->castlingStatus.ui8[1][1] = 1;

			ConvertPiecesBBToMailboxBoard64(EngineBrain.piecesBB, EngineBrain.mailboxBoard64);
			WriteMailboxBoard64(&EngineBrain);
			Output("");


			//ALSO SCAN THE INNER CORNER SQS B2,G2,B7,G7
			//in fact the solution may only occur if one of those sqs is occupied by a N or R
			uint64_t count = 0;
			int maximumMoves = 0;

			for (int k = 0; k < 64; k++)
			{
				Output("King = " + MyITOA(k));

				if ((kingRestrictedBB & UINT64SetBit(k)) != 0)
					continue;
				if (EngineBrain.piecesBB[0][Queen] & UINT64SetBit(k))
					continue;
				//if (((EdgesBB | InnerCornersBB) & UINT64SetBit(k)) == 0)
				//if ((EdgesBB & UINT64SetBit(k)) == 0)
				if (((EdgesBB | InnerEdgesBB) & UINT64SetBit(k)) == 0)
					continue;

				EngineBrain.piecesBB[0][King] = UINT64SetBit(k);

				//for (int r1 = -2; r1 < 64 - 1; r1++)
				for (int r1 = 0; r1 < 64 - 1; r1++)
				{
					//if (r1 >= 0)
					{
						//if (((EdgesBB | InnerCornersBB) & UINT64SetBit(r1)) == 0)
						//if (((EdgesBB)& UINT64SetBit(r1)) == 0)
						if (((EdgesBB | InnerEdgesBB) & UINT64SetBit(r1)) == 0)
							continue;
						if (EngineBrain.piecesBB[0][Queen] & UINT64SetBit(r1))
							continue;
						if ((rookRestrictedBB & UINT64SetBit(r1)) != 0)
							continue;
						if (r1 == k)
							continue;
					}

					for (int r2 = r1 + 1; r2 < 64; r2++)
					{
						//if (r2 >= 0)
						{
							//if (((EdgesBB | InnerCornersBB) & UINT64SetBit(r2)) == 0)
							//if (((EdgesBB)& UINT64SetBit(r2)) == 0)
							if (((EdgesBB | InnerEdgesBB) & UINT64SetBit(r2)) == 0)
								continue;
							if (EngineBrain.piecesBB[0][Queen] & UINT64SetBit(r2))
								continue;
							if ((rookRestrictedBB & UINT64SetBit(r2)) != 0)
								continue;
							//if (r1 == -2)
							//	continue;
							if (r2 == k)
								continue;
						}

						EngineBrain.piecesBB[0][Rook] = 0;
						//if (r1 >= 0)
						EngineBrain.piecesBB[0][Rook] |= UINT64SetBit(r1);
						//if (r2 >= 0)
						EngineBrain.piecesBB[0][Rook] |= UINT64SetBit(r2);

						//for (int b1 = -2; b1 < 64 - 1; b1++)
						for (int b1 = 0; b1 < 64 - 1; b1++)
						{
							//if (b1 >= 0)
							{
								//if ((EdgesBB & UINT64SetBit(b1)) == 0)
								if (((EdgesBB | InnerEdgesBB) & UINT64SetBit(b1)) == 0)
									continue;
								if (EngineBrain.piecesBB[0][Queen] & UINT64SetBit(b1))
									continue;
								if ((bishopRestrictedBB & UINT64SetBit(b1)) != 0)
									continue;
								if ((b1 == k) || (b1 == r1) || (b1 == r2))
									continue;
							}

							for (int b2 = b1 + 1; b2 < 64; b2++)
							{
								//if (b2 >= 0)
								{
									//if ((EdgesBB & UINT64SetBit(b2)) == 0)
									if (((EdgesBB | InnerEdgesBB) & UINT64SetBit(b2)) == 0)
										continue;
									if (EngineBrain.piecesBB[0][Queen] & UINT64SetBit(b2))
										continue;
									if ((bishopRestrictedBB & UINT64SetBit(b2)) != 0)
										continue;
									//if (b1 == -2)
									//	continue;
									if ((b2 == k) || (b2 == r1) || (b2 == r2))
										continue;
									//if (b1 >= 0)
										if (((LightBB & UINT64SetBit(b1)) == 0) == ((LightBB & UINT64SetBit(b2)) == 0))
											continue;
								}

								EngineBrain.piecesBB[0][Bishop] = 0;
								if (b1 >= 0)
									EngineBrain.piecesBB[0][Bishop] |= UINT64SetBit(b1);
								if (b2 >= 0)
									EngineBrain.piecesBB[0][Bishop] |= UINT64SetBit(b2);

								//for (int n1 = -2; n1 < 64 - 1; n1++)
								for (int n1 = 0; n1 < 64 - 1; n1++)
								{
									//if (n1 >= 0)
									{
										//if (((EdgesBB | InnerCornersBB) & UINT64SetBit(n1)) == 0)
										//if (((EdgesBB)& UINT64SetBit(n1)) == 0)
										if (((EdgesBB | InnerEdgesBB) & UINT64SetBit(n1)) == 0)
											continue;
										if (EngineBrain.piecesBB[0][Queen] & UINT64SetBit(n1))
											continue;
										if ((knightRestrictedBB & UINT64SetBit(n1)) != 0)
											continue;
										if ((n1 == k) || (n1 == r1) || (n1 == r2) || (n1 == b1) || (n1 == b2))
											continue;
									}

									for (int n2 = n1 + 1; n2 < 64; n2++)
									{
										//if (n2 >= 0)
										{
											//if (((EdgesBB | InnerCornersBB) & UINT64SetBit(n2)) == 0)
											//if (((EdgesBB)& UINT64SetBit(n2)) == 0)
											if (((EdgesBB | InnerEdgesBB) & UINT64SetBit(n2)) == 0)
												continue;
											if (EngineBrain.piecesBB[0][Queen] & UINT64SetBit(n2))
												continue;
											if ((knightRestrictedBB & UINT64SetBit(n2)) != 0)
												continue;
											//if (n1 == -2)
											//	continue;
											if ((n2 == k) || (n2 == r1) || (n2 == r2) || (n2 == b1) || (n2 == b2))
												continue;
										}

										EngineBrain.piecesBB[0][Knight] = 0;
										//if (n1 >= 0)
										EngineBrain.piecesBB[0][Knight] |= UINT64SetBit(n1);
										//if (n2 >= 0)
										EngineBrain.piecesBB[0][Knight] |= UINT64SetBit(n2);
										
										//if ((InnerCornersBB & (PiecesBB[0][Rook] | PiecesBB[0][Knight])) == 0)
										//	continue;

										EngineBrain.piecesBB[0][AllPieces] = EngineBrain.piecesBB[0][King] | EngineBrain.piecesBB[0][Queen] | EngineBrain.piecesBB[0][Rook] | EngineBrain.piecesBB[0][Bishop] | EngineBrain.piecesBB[0][Knight];

										count++;
										
										//int moves = GenerateAllMoves(0, false, moveList);
										int moves = EngineBrain.CountAllMovesMM(0);

										if (moves >= 218)
										{
											ConvertPiecesBBToMailboxBoard64(EngineBrain.piecesBB, EngineBrain.mailboxBoard64);
											WriteMailboxBoard64(&EngineBrain);
											Output(MyITOA(moves));
											Output("");

											FILE *file;
											std::string resultsFilename;
											resultsFilename = "D:\\Chess\\MaximumMoves\\" + Subfolder + "\\" + MyITOA(fileNumber) + "ND" + MyITOA(moves) +  ".txt";
											fopen_s(&file, resultsFilename.c_str(), "a");
											fprintf(file, (ConvertPositionToFEN(EngineBrain.mailboxBoard64, 0, EngineBrain.gameRecordPointer->castlingStatus, 0, 0, 1) + " = " + MyITOA(moves) + "\n").c_str());
											fclose(file);
										}
									}
								}
							}
						}
					}
				}
			}

			Output(MyUI64TOA(count));
			Output("");
		}
	}

	Output("Done!");
}

//void NNUETest()
//{
//	//int score = nnue_evaluate_fen("r1bqkbnr/ppp1pppp/2n5/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3");
//	//int score = nncpu_evaluate_fen("r1bqkbnr/ppp1pppp/2n5/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3");
//	int score;
//	score = nnue_evaluate_fen("r1bqk1nr/pppp1ppp/2n5/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4");
//	//score = nncpu_evaluate_fen("r1bqk1nr/pppp1ppp/2n5/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4");
//	Output(MyITOA(score));
//	score = nnue_evaluate_fen("r1bqk1nr/pppp1ppp/2n5/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 4 4");
//	//score = nncpu_evaluate_fen("r1bqk1nr/pppp1ppp/2n5/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 4 4");
//	Output(MyITOA(score));
//	return;
//}
//
//void GenerateKP()
//{
//	int square, pawns;
//
//	for (int count = 1; count <= 5000; count++)
//	{
//		ClearMailboxBoard64(UGIGenerate.mailboxBoard64);
//
//		pawns = 3 + Random64() % 6;
//		assert((pawns >= 3) && (pawns <= 8));
//		for (int j = 0; j < pawns; j++)
//		{
//			while (true)
//			{
//				square = BoardRand(A2, H3);
//				if (UGIGenerate.mailboxBoard64[square] == Empty)
//					break;
//			}
//			UGIGenerate.mailboxBoard64[square] = Pawn;
//			while (true)
//			{
//				square = BoardRand(A6, H7);
//				if (UGIGenerate.mailboxBoard64[square] == Empty)
//					break;
//			}
//			UGIGenerate.mailboxBoard64[square] = -Pawn;
//		}
//		while (true)
//		{
//			square = BoardRand(A1, H3);
//			if (UGIGenerate.mailboxBoard64[square] == Empty)
//				break;
//		}
//		UGIGenerate.mailboxBoard64[square] = King;
//		while (true)
//		{
//			square = BoardRand(A6, H8);
//			if (UGIGenerate.mailboxBoard64[square] == Empty)
//				break;
//		}
//		UGIGenerate.mailboxBoard64[square] = -King;
//
//		Output(ConvertPositionToFEN(UGIGenerate.mailboxBoard64));
//	}
//
//}
//
//struct Neuron
//{
//	int16_t value;
//	int8_t weights[32];
//};
//
//uint8_t NNLayer1[32]; // square indeces 0-63 + 1 (0 used to signify no piece)
//Neuron NNLayer2[16];
//Neuron NNLayer3[16];
//Neuron NNLayer4;
//
//// INITIALLY CAN GEN ALL PIECE COMBINATIONS TO TRAIN IT ON MATERIAL VALUE
//// CAN YOU KEEP WHITE/BLACK NODES SEPARATE IN LAYER1&2 TO REDUCE THE NUMBER OF WEIGHTS?
//void NN() //THINK WEIGHTS SHOULD POINT FORWARDS NOT BACKWARDS. EASIER THEN TO EXCLUDE ENTRIES IN FIRST LAYER WHEN PIECE DOES NOT EXIST
//{
//	// Initialise
//	for (int weight = 0; weight < 32; weight++)
//	{
//		for (int neuron = 0; neuron < 16; neuron++)
//		{
//			NNLayer2[neuron].weights[weight] = 0;
//			NNLayer3[neuron].weights[weight] = 0;
//		}
//		NNLayer4.weights[weight] = 0;
//	}
//}
//
//int16_t NNEvaluate()
//{
//	// Evaluate
//	for (int neuron = 0; neuron < 16; neuron++)
//	{
//		int64_t value = 0;
//		for (int weight = 0; weight < 32; weight++)
//			value += NNLayer1[weight] * NNLayer2[neuron].weights[weight];
//		NNLayer2[neuron].value = value >> 8;
//	}
//	for (int neuron = 0; neuron < 16; neuron++)
//	{
//		int64_t value = 0;
//		for (int weight = 0; weight < 16; weight++)
//			value += NNLayer2[weight].value * NNLayer3[neuron].weights[weight];
//		NNLayer3[neuron].value = value >> 8;
//	}
//	int64_t value = 0;
//	for (int weight = 0; weight < 32; weight++)
//		value += NNLayer3[weight].value * NNLayer4.weights[weight];
//	NNLayer4.value = value >> 8;
//
//	return NNLayer4.value;
//}
//
//void EGTB7Stats()
//{
//	FILE *file;
//	std::string filename;
//	std::string* tableNames = new std::string[1001];
//	char currentline[10];
//	int tableCount;
//
//	filename = "EGTB7TableNames.txt";
//	fopen_s(&file, filename.c_str(), "r");
//
//	tableCount = 0;
//	while (fgets(currentline, sizeof(currentline), file) != NULL)
//	{
//		currentline[8] = '\0';
//		tableNames[tableCount++] = currentline;
//	}
//
//	fclose(file);
//	Output("Tables = " + MyITOA(tableCount));
//
//	int pieces1[1001];
//	int pieces2[1001];
//	int weight1[1001];
//	int weight2[1001];
//	int balance[1001];
//	int promotions[1001];
//	int underPromotions[1001];
//
//	for (int count = 0; count < tableCount; count++)
//	{
//		std::string tableName = tableNames[count];
//
//		int index;
//		int weight;
//		int promotionsCount = 0;
//		int underPromotionsCount = 0;
//		int knights, bishops, rooks, queens;
//
//		for (index = 0; index < 8; index++)
//			if (tableName[index] == 'v')
//				break;
//		pieces1[count] = index;
//		pieces2[count] = 7 - index;
//
//		knights = bishops = rooks = queens = 0;
//		weight = 0;
//		for (index = 1; index < pieces1[count]; index++)
//		{
//			switch (tableName[index])
//			{
//			case 'P':
//				weight += 1;
//				break;
//			case 'N':
//				weight += 3;
//				knights++;
//				break;
//			case 'B':
//				weight += 3;
//				bishops++;
//				break;
//			case 'R':
//				weight += 5;
//				rooks++;
//				break;
//			case 'Q':
//				weight += 9;
//				queens++;
//				break;
//			}
//		}
//		weight1[count] = weight;
//
//		if (queens > 1)
//			promotionsCount += (queens - 1);
//		if (rooks > 2)
//		{
//			promotionsCount += (rooks - 2);
//			underPromotionsCount += (rooks - 2);
//		}
//		if (bishops > 2)
//		{
//			promotionsCount += (bishops - 2);
//			underPromotionsCount += (bishops - 2);
//		}
//		if (knights > 2)
//		{
//			promotionsCount += (knights - 2);
//			underPromotionsCount += (knights - 2);
//		}
//
//		knights = bishops = rooks = queens = 0;
//		weight = 0;
//		for (index = pieces1[count] + 2; index < 8; index++)
//		{
//			switch (tableName[index])
//			{
//			case 'P':
//				weight += 1;
//				break;
//			case 'N':
//				weight += 3;
//				knights++;
//				break;
//			case 'B':
//				weight += 3;
//				bishops++;
//				break;
//			case 'R':
//				weight += 5;
//				rooks++;
//				break;
//			case 'Q':
//				weight += 9;
//				queens++;
//				break;
//			}
//		}
//		weight2[count] = weight;
//
//		if (queens > 1)
//			promotionsCount += (queens - 1);
//		if (rooks > 2)
//		{
//			promotionsCount += (rooks - 2);
//			underPromotionsCount += (rooks - 2);
//		}
//		if (bishops > 2)
//		{
//			promotionsCount += (bishops - 2);
//			underPromotionsCount += (bishops - 2);
//		}
//		if (knights > 2)
//		{
//			promotionsCount += (knights - 2);
//			underPromotionsCount += (knights - 2);
//		}
//
//		promotions[count] = promotionsCount;
//		underPromotions[count] = underPromotionsCount;
//
//		balance[count] = weight1[count] - weight2[count];
//	}
//
//
//	filename = "EGTB7Statistics.csv";
//	fopen_s(&file, filename.c_str(), "w");
//
//	for (int count = 0; count < tableCount; count++)
//	{
//		fprintf(file, (tableNames[count] + "," + MyITOA(pieces1[count]) + "," + MyITOA(pieces2[count]) + "," + MyITOA(weight1[count]) + "," + MyITOA(weight2[count]) + "," + MyITOA(balance[count]) + "," + MyITOA(promotions[count]) + "," + MyITOA(underPromotions[count]) + "\n").c_str());
//	}
//
//	fclose(file);
//
//	Output("Done!");
//}

#endif // EXPERIMENTAL
