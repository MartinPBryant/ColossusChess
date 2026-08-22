#include <algorithm>
#include <iostream>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#endif

#include "Engine.h"

//----------------------------------------------------------------------------------------------------

std::string INIPath;
char INIValue[1000];
std::string LogFileName;
bool Logging;
std::ofstream LogFile;
std::string SyzygyPathLogFileName;
std::ofstream SyzygyPathLogFile;
std::string ErrorFileName;
std::ofstream ErrorFile;

int LastPrintTreePly;

// In 'TargetLine' below, to specify a null move use a1a1
std::string TargetLine = "c8b6 a7b6 a6a7 b6b5 a7a8b b5c4 a8e4 c4c3 e4d3 c3d2 d3c4 d2d1q a1d1 ";

int TargetLineLength = (int)TargetLine.length() / 5;
bool TargetLineContainsPromotions = false;
//int TargetLineLength = 1;
std::string TargetLinePartial;
int TargetLinePartialDepthRemaining;
int TargetLinePartialThreateningMate;
Move_Struct TargetLineLastSearched[MaximumPly];
Move_Struct TargetLineRefutedBy;
int TargetLineRefutationsDepth = 2;
int TargetLinePrintDepth = 0;

// 'Analysis counters' used to accumulate stats during development/debugging
int64_t AC1, AC2, AC3, AC4, AC5, AC6, AC7, AC8, AC9;

//----------------------------------------------------------------------------------------------------

void ReadINI(char* section, char* key)
{
#ifdef _WIN32
	GetPrivateProfileString(section, key, "", INIValue, sizeof(INIValue), INIPath.c_str());
#else
	INIValue[0] = '\0'; // INI files not supported on Linux
#endif
}

std::atomic<bool> OutputLogLocked = false;
void OutputLog(std::string s)
{
	// Occasionally the 'main' thread and the outer-engine thread would clash here and crash DESPITE the try/catch clause! So I implemented multi-thread locking.
	if (!OutputLogLocked.exchange(true)) // The exchange method returns the existing value so if any other thread has already set it to true this block won't be executed
	{
		try
		{
			LogFile.open(LogFileName, std::ios_base::app);
			LogFile << s + "\n";
			LogFile.close();
		}
		catch (...)
		{
		}
		OutputLogLocked = false;
	}
}

std::atomic<bool> OutputSyzygyPathLogLocked = false;
void OutputSyzygyPathLog(std::string s)
{
	if (!OutputSyzygyPathLogLocked.exchange(true)) // The exchange method returns the existing value so if any other thread has already set it to true this block won't be executed
	{
		try
		{
			SyzygyPathLogFile.open(SyzygyPathLogFileName, std::ios_base::trunc);
			time_t t;
			time(&t);
			char buffer[100];
			ctime_s(buffer, sizeof(buffer), &t);
			SyzygyPathLogFile << buffer;
			SyzygyPathLogFile << "\n" + s + "\n";
			SyzygyPathLogFile.close();
		}
		catch (...)
		{
		}
		OutputSyzygyPathLogLocked = false;
	}
}

void OutputError(std::string s)
{
	try
	{
		ErrorFile.open(ErrorFileName, std::ios_base::app);
		time_t t;
		time(&t);
		char buffer[100];
		ctime_s(buffer, sizeof(buffer), &t);
		ErrorFile << buffer;
		ErrorFile << "*** Error!: " + s + "\n";
		ErrorFile << ConvertPositionToFEN(EngineBrain.mailboxBoard64, EngineBrain.gameRecord[EngineBrain.GameRecordIndexRoot].sideToMove, EngineBrain.gameRecord[EngineBrain.GameRecordIndexRoot].castlingStatus, EngineBrain.gameRecord[EngineBrain.GameRecordIndexRoot].epSquare, EngineBrain.gameRecord[EngineBrain.GameRecordIndexRoot].pliesSinceIrreversible, EngineBrain.gameRecord[EngineBrain.GameRecordIndexRoot].moveNumber) + "\n";

		ErrorFile << LastPositionAndMoves << "\n";
		ErrorFile << "TranspositionTableMemory=" + std::to_string(TranspositionTableMemory) << "\n";
		ErrorFile << "StopImmediately=" + std::to_string(StopImmediately) << "\n";
		ErrorFile << "StopWhenIterationComplete=" + std::to_string(StopWhenIterationComplete) << "\n";

		ErrorFile << "\n";
		ErrorFile.close();
	}
	catch (...)
	{
	}
}

// Output a string to stdout (and optionally the log file)
void Output(std::string s)
{
	std::cout << s + "\n";
	if (Logging)
		OutputLog(s);
}

//----------------------------------------------------------------------------------------------------

#include <cctype>

void ltrim(std::string &s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
		return !std::isspace(ch);
	}));
}

void rtrim(std::string &s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
		return !std::isspace(ch);
	}).base(), s.end());
}

void trim(std::string &s) {
	rtrim(s);
	ltrim(s);
}

// Convert string to uppercase
std::string UpperCase(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), ::toupper);
	return s;
}

std::string GetNextToken(std::string *s)
{
	std::string result;

	size_t firstWhiteSpaceIndex;
	firstWhiteSpaceIndex = (*s).find_first_of(" \t", 0);
	if (firstWhiteSpaceIndex == -1)
	{
		result = *s;
		*s = "";
	}
	else
	{
		result = UpperCase((*s).substr(0, firstWhiteSpaceIndex));
		(*s) = (*s).substr(firstWhiteSpaceIndex);
		trim((*s));
	}

	trim(result);
	return result;
}

// Split a line into tokens
void Split(std::string command, std::string *tokens, int *tokenCount, char separators[])
{
	char *token;
	char line[10000]; // N.B. sometimes the UCI 'position' commands can be very long as they contain all the moves of the game!

	strcpy_s(line, command.c_str());
	*tokenCount = 0;

	char *next_token1 = NULL;
	token = strtok_s(line, separators, &next_token1);
	while (token != NULL)
	{
		tokens[(*tokenCount)++] = token;
		token = strtok_s(NULL, separators, &next_token1);
	}
}

//----------------------------------------------------------------------------------------------------

std::string MyITOA(int i)
{
	return std::to_string(i);
}

std::string MyUI64TOA(uint64_t i)
{
	return std::to_string(i);
}

std::string MySI64TOA(int64_t i)
{
	return std::to_string(i);
}

std::string MyBooleanTOA(bool b)
{
	if (b)
		return "true";
	return "false";
}

std::string MyFTOA(float f, std::string format)
{
	static char buffer[100];
	std::sprintf(buffer, format.c_str(), f);
	return (std::string)buffer;
}

std::string MyDTOA(double d)
{
	static char buffer[100];
	std::sprintf(buffer, "%9.2f", d);
	return (std::string)buffer;
}

//----------------------------------------------------------------------------------------------------

__declspec(noinline)
void ClearAnalysisCounters()
{
	AC1 = 0;
	AC2 = 0;
	AC3 = 0;
	AC4 = 0;
	AC5 = 0;
	AC6 = 0;
	AC7 = 0;
	AC8 = 0;
	AC9 = 0;
}

__declspec(noinline)
void DisplayAnalysisCounters()
{
	if (AC1 != 0)
	{
		Output("AC1=" + MySI64TOA(AC1));
		Output("AC2=" + MySI64TOA(AC2));
		Output("AC3=" + MySI64TOA(AC3));
		Output("AC4=" + MySI64TOA(AC4));
		Output("AC5=" + MySI64TOA(AC5));
		Output("AC6=" + MySI64TOA(AC6));
		Output("AC7=" + MySI64TOA(AC7));
		Output("AC8=" + MySI64TOA(AC8));
		Output("AC9=" + MySI64TOA(AC9));
	}
}

//----------------------------------------------------------------------------------------------------

// Check that there are no duplicate moves in the specified move list
bool NoDuplicateMoves(MoveWithScore_Struct* mlp, int movesCount)
{
	for (int index1 = 0; index1 < movesCount - 1; index1++)
	{
		uint32_t move = mlp[index1].ui32;
		for (int index2 = index1 + 1; index2 < movesCount; index2++)
			if (move == mlp[index2].ui32)
				return false;
	}

	return true;
}

// Check that the best move from the transposition table is found in the move list
bool TranpositionTableMoveFound(MoveWithScore_Struct* mlp, int movesCount, uint32_t tteBestMove)
{
	if (tteBestMove == PVTUnknown)
		return true;

	for (int index = 0; index < movesCount; index++)
		if (tteBestMove = mlp[index].ui32)
			return true;

	return false;
}

bool PVSearchedFirst(int ply)
{
	bool result = true;
	//if (!PVMessageChecked)
	//{
	//	if (PVMessage != "")
	//	{
	//		std::string cl = CurrentLine(ply - 1);
	//		if (
	//			(strncmp(PVMessage.c_str(), cl.c_str(), strlen(cl.c_str())) != 0) &&
	//			(strncmp(cl.c_str(), PVMessage.c_str(), strlen(PVMessage.c_str())) != 0)
	//			)
	//			result = false;
	//	}
	//	PVMessageChecked = true;
	//}
	return result;
}

void PrintTree(int iterationPly, int ply, short alpha, short beta, int depthRemaining, int move, int bestSortScore, int staticEvaluation)
{
	if (ply <= 8)
	{
		FILE *f;
		char treeFilename[100];
		sprintf(treeFilename, "tree%d.csv", iterationPly);
		f = fopen(treeFilename, "a");

		if (ply > LastPrintTreePly)
			fprintf(f, ",");
		if (ply <= LastPrintTreePly)
		{
			fprintf(f, "\n");
			for (int i = 1; i < ply; i++)
				fprintf(f, ",");
		}

		LastPrintTreePly = ply;
		fprintf(f, "%d.%s (%d %d %d %d %d) ", ply, (move == -1 ? "Null" : (move == -2 ? "TTExact" : (move == -3 ? "TTUpper" : (move == -4 ? "TTLower" : MoveNotation(move).c_str())))), alpha, beta, depthRemaining, bestSortScore, staticEvaluation);
		fclose(f);
	}
}

void PrintTree2(int iterationPly, int ply, std::string s)
{
	if (ply <= 8)
	{
		FILE *f;
		char treeFilename[100];
		sprintf(treeFilename, "tree%d.csv", iterationPly);
		f = fopen(treeFilename, "a");

		if (ply > LastPrintTreePly)
			fprintf(f, ",");
		if (ply <= LastPrintTreePly)
		{
			fprintf(f, "\n");
			for (int i = 1; i < ply; i++)
				fprintf(f, ",");
		}
		
		LastPrintTreePly = ply;
		fprintf(f, "%d.%s", ply, s.c_str());
		fclose(f);
	}
}

static uint64_t random64 = 17489870280564659055ULL;

// Generates 64-bit random numbers used for hashing
// N.B. this vanilla routine generates alternating odd/even numbers so be careful that your code doesn't e.g. assign all the evens to white and odds to black!
uint64_t Random64()
{
	// MMIX by Donald Knuth 'Linear congruential generator' https://en.wikipedia.org/wiki/Linear_congruential_generator
	do
	{
		random64 = random64 * 6364136223846793005ULL + 1442695040888963407ULL;
	} while (PopulationCountX(random64) != 32); // Added to ensure all random numbers have 32x1bits - this also alleviates the alternating odd/even problem

	return random64;
}

int BoardRand(int min, int max)
{
	return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}

int BoardRand0To63()
{
	return BoardRand(0, 63);
}

//----------------------------------------------------------------------------------------------------
#include <array>

void CPUInfo()
{
	std::array<int, 4> cpui;

	// Calling __cpuid with 0x0 as the function_id argument
	// gets the number of the highest valid function ID.
	__cpuid(cpui.data(), 0);
}
