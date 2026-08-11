#pragma once

//----------------------------------------------------------------------------------------------------

extern std::string INIPath;
extern char INIValue[1000];
extern std::string LogFileName;
extern bool Logging;
extern std::ofstream LogFile;
extern std::string SyzygyPathLogFileName;
extern std::ofstream SyzygyPathLogFile;
extern std::string ErrorFileName;
extern FILE *ErrorFile;

//#define PRINTTREEDEF // Writes the search tree to .csv files (one per iteration) which can be interrogated in a spreadsheet
#ifdef PRINTTREEDEF
#define PRINTTREE(s) {s}
#else
#define PRINTTREE(s)
#endif

extern int LastPrintTreePly;

//#define SEARCHINGFORLINE
extern std::string TargetLine;
extern int TargetLineLength;
extern bool TargetLineContainsPromotions;
extern std::string TargetLinePartial;
extern int TargetLinePartialDepthRemaining;
extern int TargetLinePartialThreateningMate;
extern Move_Struct TargetLineLastSearched[MaximumPly];
extern Move_Struct TargetLineRefutedBy;
extern int TargetLineRefutationsDepth;
extern int TargetLinePrintDepth;

extern int64_t AC1, AC2, AC3, AC4, AC5, AC6, AC7, AC8, AC9;

//----------------------------------------------------------------------------------------------------

void ReadINI(char* section, char* key);

void OutputLog(std::string s);
void OutputSyzygyPathLog(std::string s);
void OutputError(std::string s);
void Output(std::string s);

void trim(std::string &s);
std::string UpperCase(std::string s);
std::string GetNextToken(std::string *s);
void Split(std::string command, std::string *tokens, int *tokenCount, char separators[]);

std::string MyITOA(int i);
std::string MyUI64TOA(uint64_t i);
std::string MySI64TOA(int64_t i);
std::string MyBooleanTOA(bool b);
std::string MyFTOA(float f, std::string format = "%.2f");
std::string MyDTOA(double d);

void ClearAnalysisCounters();
void DisplayAnalysisCounters();
bool NoDuplicateMoves(MoveWithScore_Struct* mlp, int movesCount);
bool TranpositionTableMoveFound(MoveWithScore_Struct* mlp, int movesCount, uint32_t tteBestMove);
bool PVSearchedFirst(int ply);
void PrintTree(int iterationPly, int ply, short alpha, short beta, int depthRemaining, int move, int bestSortScore, int staticEvaluation);
void PrintTree2(int iterationPly, int ply, std::string s);
uint64_t Random64();
int BoardRand(int min, int max);
int BoardRand0To63();
