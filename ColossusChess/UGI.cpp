#include <algorithm>
#include <thread>
#define NOMINMAX // Need to include this to stop windows.h (below) breaking std::min etc
#include <Windows.h>
#include <processthreadsapi.h>

#include "Engine.h"
#include "SearchNormal.h"
#include "SearchMate.h"
#include "SearchPerft.h"
#include "SYZYGYPYRRHIC\tbprobe.h"
#include "Evaluate.h"
#include "Utilities.h"

//----------------------------------------------------------------------------------------------------

FILE *CommandFile;
bool ProcessingCommandFile;

//----------------------------------------------------------------------------------------------------

// Display the options that the engine supports, in response to a 'UCI' command
void SendOptions()
{
	// N.B. For strings (e.g. SyzygyPath) make sure you append the word 'default' (even if there is no value) as some GUIs won't recognise it as a valid option without it!
	Output("option name Hash type spin default " + MyITOA(TranspositionTableMemoryDefault) + " min " + MyITOA(TranspositionTableMemoryMin) + " max " + MyITOA(TranspositionTableMemoryMax));
	Output("option name ClearHash type button");
	Output("option name Threads type spin default " + MyITOA(ThreadsDefault) + " min " + MyITOA(ThreadsMin) + " max " + MyITOA(ThreadsMax));
	Output("option name Ponder type check default false");
	Output("option name Contempt type spin default " + MyITOA(ContemptDefault) + " min " + MyITOA(ContemptMin) + " max " + MyITOA(ContemptMax));
	Output("option name SyzygyPath type string default");
	Output("option name SyzygyProbeLimit type spin default " + MyITOA(SyzygyProbeLimitDefault) + " min " + MyITOA(SyzygyProbeLimitMin) + " max " + MyITOA(SyzygyProbeLimitMax));
	Output("option name SyzygyProbe7PieceInTree type check default false");
	Output("option name UCI_Chess960 type check default false");
	//Output("option name MateFullWidth type check default false");
	Output("option name MateAllChecks type check default false");
	Output("option name MateAllThreateningMateInOne type check default false");
	Output("option name MateMaximumDefenderKingMoves type spin default 8 min 0 max 8");
	Output("option name MateMaximumDefenderMoves type spin default 218 min 0 max 218");
	Output("option name MateMaximumDefenderMovablePieces type spin default 16 min 0 max 16");
	//Output("option name MateMinimumAttackerMaterial type spin default 0 min 0 max 39");
	Output("option name MateFixedPieces type string default");
	//Output("option name MateForcePawnMoves type check default false");
	//Output("option name EGTBPieceLimit type spin default 7 min 3 max 7");
}

void SendOptionValues()
{
	Output("info string Hash: " + MyITOA(TranspositionTableMemory));
	Output("info string Threads: " + MyITOA(Threads));
	Output("info string Ponder: " + MyBooleanTOA(Ponder));
	Output("info string Contempt: " + MyITOA(Contempt));
	Output("info string SyzygyPath: " + std::string(EndgameTablebasesPath));
	Output("info string SyzygyProbeLimit: " + MyITOA(SyzygyProbeLimit));
	Output("info string SyzygyProbe7PieceInTree: " + MyBooleanTOA(SyzygyProbe7PieceInTree));
	Output("info string UCI_Chess960: " + MyBooleanTOA(UCI_Chess960));
	Output("info string MateMaximumDefenderKingMoves: " + MyITOA(TC.MateMaximumDefenderKingMoves));
	Output("info string MateMaximumDefenderMovablePieces: " + MyITOA(TC.MateMaximumDefenderMovablePieces));
	Output("info string MateMaximumDefenderMoves: " + MyITOA(TC.MateMaximumDefenderMoves));
	Output("info string MateMaximumReversibleMoves: " + MyITOA(TC.MateMaximumReversibleMoves));
	Output("info string MateMinimumAttackerMaterial: " + MyITOA(TC.MateMinimumAttackerMaterial));
	Output("info string MateFixedPieces: " + TC.MateFixedPieces);
	Output("info string ShowPVTerminators: " + MyBooleanTOA(ShowPVTerminators));
	Output("info string BlankLines: " + MyBooleanTOA(BlankLines));
}

// Sets an engine option to a specified value
void SetOption(std::string currentLine, std::string name, std::string value)
{
	// e.g. the GUI might send 'setoption name hash value 64'
	std::string nameUpperCase = UpperCase(name);
	std::string limit = "";

	if (nameUpperCase == "HASH")
	{
		TranspositionTableMemory = atoi(value.c_str());
		if (TranspositionTableMemory > TranspositionTableMemoryMax)
			limit = " (maximum)";
		else if (TranspositionTableMemory < TranspositionTableMemoryMin)
			limit = " (minimum)";
		TranspositionTableMemory = std::max(std::min(TranspositionTableMemory, TranspositionTableMemoryMax), TranspositionTableMemoryMin);
		//if (IsDebug)
		Output("info string Transposition table memory set to " + MyITOA(TranspositionTableMemory) + "MB" + limit);
		FreeAnyTranspositionTableMemory();
	}

	else if (nameUpperCase == "THREADS")
	{
		Threads = atoi(value.c_str());
		if (Threads > ThreadsMax)
			limit = " (maximum)";
		else if (Threads < ThreadsMin)
			limit = " (minimum)";
		Threads = std::max(std::min(Threads, ThreadsMax), ThreadsMin);
		std::string warning = "";
		uint32_t hardwareThreadsMax = std::thread::hardware_concurrency();
		if (Threads > (int)hardwareThreadsMax)
			warning = " (*** Warning!: Maximum hardware threads=" + MyITOA(hardwareThreadsMax) + ")";
		//if (IsDebug)
		Output("info string Threads set to " + MyITOA(Threads) + limit + warning);
	}

	else if (nameUpperCase == "CLEARHASH")
	{
		Normal::ClearNormalTranspositionTable();
		Perft::ClearPerftTranspositionTable();
		//if (IsDebug)
		Output("info string Hash tables cleared");
	}

	else if (nameUpperCase == "PONDER")
	{
		// Ponder requires...
		// Engine to advertise its ability to ponder with 'option name Ponder type check default false' option
		// Process commands to set this option e.g. 'setoption name ponder value true'
		// When reporting the best move at the end of a search suffix it with the 'ponder' move e.g. 'bestmove e2e4 ponder c7c5'
		// 'go ponder wtime 62000 btime 59344 winc 1000 binc 1000' messages tell you to think about the position at the end of the last 'position' command but in ponder mode
		// If receives 'ponderhit' command, switch to normal processing and possibly reply immediately
		// (N.B. if the 'ponder' move isn't played, the GUI will send a 'stop' command to abort the now defunct search)
		// In 'TimeUp', if pondering, have to use 'ReplyImmediately' rather than 'Stop*' flags
		Ponder = (UpperCase(value) == "TRUE");
		//if (IsDebug)
		Output("info string Ponder set to " + MyBooleanTOA(Ponder));
	}

	else if (nameUpperCase == "CONTEMPT")
	{
		Contempt = atoi(value.c_str());
		if (Contempt > ContemptMax)
			limit = " (maximum)";
		else if (Contempt < ContemptMin)
			limit = " (minimum)";
		Contempt = std::max(std::min(Contempt, (short)ContemptMax), (short)ContemptMin);
		//if (IsDebug)
		Output("info string Contempt set to " + MyITOA(Contempt) + limit);
	}

	else if (nameUpperCase == "SYZYGYPATH")
	{
		if (value == "")
		{
			EndgameTablebasesPath[0] = '\0';
			EndgameTablebasesInitialised = false;
			EndgameTablebasesPiecesFound = 0;
		}
		else
		{
			// Initialise endgame tablebases
			strcpy_s(EndgameTablebasesPath, sizeof(EndgameTablebasesPath), value.c_str());
			EndgameTablebasesInitialised = tb_init(EndgameTablebasesPath);
			EndgameTablebasesPiecesFound = TB_LARGEST;
			//if (IsDebug)
			{
				if (EndgameTablebasesPiecesFound != 0)
				{
					std::string wdlWarning = "";
					std::string dtzWarning = "";
					if (TB_NUM_WDL != EndgameTablebasesCumulativeExpectedFileCounts[EndgameTablebasesPiecesFound])
						wdlWarning = " {*** Warning! " + MyITOA(EndgameTablebasesCumulativeExpectedFileCounts[EndgameTablebasesPiecesFound]) + " expected!}";
					if (TB_NUM_DTZ != EndgameTablebasesCumulativeExpectedFileCounts[EndgameTablebasesPiecesFound])
						dtzWarning = " {*** Warning! " + MyITOA(EndgameTablebasesCumulativeExpectedFileCounts[EndgameTablebasesPiecesFound]) + " expected!}";
					std::string status = MyITOA(EndgameTablebasesPiecesFound) + "-piece endgame tablebases found at " + value + " (" + MyITOA(TB_NUM_WDL) + " .rtbw {win/draw/loss} files" + wdlWarning + ", " + MyITOA(TB_NUM_DTZ) + " .rtbz {distance to zero} files" + dtzWarning + ")";
					Output("info string " + status);
					OutputSyzygyPathLog("Command received:\n" + currentLine + "\n\nResult:\n" + status);
				}
				else
				{
					Output("info string *** Error! No endgame tablebases found at " + value);
					OutputError("No endgame tablebases found at " + value);
				}
			}
		}
	}

	else if (nameUpperCase == "SYZYGYPROBELIMIT")
	{
		SyzygyProbeLimit = atoi(value.c_str());
		if (SyzygyProbeLimit > SyzygyProbeLimitMax)
			limit = " (maximum)";
		else if (SyzygyProbeLimit < SyzygyProbeLimitMin)
			limit = " (minimum)";
		SyzygyProbeLimit = std::max(std::min(SyzygyProbeLimit, SyzygyProbeLimitMax), SyzygyProbeLimitMin);
		//if (IsDebug)
		Output("info string SyzygyProbeLimit set to " + MyITOA(SyzygyProbeLimit) + limit);
	}

	else if (nameUpperCase == "SYZYGYPROBE7PIECEINTREE")
	{
		SyzygyProbe7PieceInTree = (UpperCase(value) == "TRUE");
		//if (IsDebug)
		Output("info string SyzygyProbe7PieceInTree set to " + MyBooleanTOA(SyzygyProbe7PieceInTree));
	}

	//else if (name == "EGTBPIECELIMIT")
	//{
	//	EndgameTablebasePieceLimit = atoi(value.c_str());
	//	Output("info string EGTBPieceLimit set to " + MyITOA(EndgameTablebasePieceLimit) + " pieces");
	//}

	else if (nameUpperCase == "UCI_CHESS960")
	{
		UCI_Chess960 = (UpperCase(value) == "TRUE");
		//if (IsDebug)
		Output("info string UCI_Chess960 set to " + MyBooleanTOA(UCI_Chess960));
	}

	else if ((nameUpperCase == "MATEFULLWIDTH") || (nameUpperCase == "MFW"))
	{
		TC.MateFullWidth = (UpperCase(value) == "TRUE");
		//if (IsDebug)
		Output("info string MateFullWidth set to " + MyBooleanTOA(TC.MateFullWidth));
	}

	else if ((nameUpperCase == "MATEALLCHECKS") || (nameUpperCase == "MAC"))
	{
		TC.MateAllChecks = (UpperCase(value) == "TRUE");
		//if (IsDebug)
		Output("info string MateAllChecks set to " + MyBooleanTOA(TC.MateAllChecks));
	}

	else if ((nameUpperCase == "MATEALLTHREATENINGMATEINONE") || (nameUpperCase == "MATMIO"))
	{
		TC.MateAllThreateningMateInOne = (UpperCase(value) == "TRUE");
		//if (IsDebug)
		Output("info string MateAllThreateningMateInOne set to " + MyBooleanTOA(TC.MateAllThreateningMateInOne));
	}

	else if ((nameUpperCase == "MATEMAXIMUMDEFENDERKINGMOVES") || (nameUpperCase == "MK"))
	{
		TC.MateMaximumDefenderKingMoves = atoi(value.c_str());
		if (TC.MateMaximumDefenderKingMoves > 8)
			limit = " (maximum)";
		else if (TC.MateMaximumDefenderKingMoves < 0)
			limit = " (minimum)";
		TC.MateMaximumDefenderKingMoves = std::max(std::min(TC.MateMaximumDefenderKingMoves, 8), 0);
		//if (IsDebug)
		Output("info string MateMaximumDefenderKingMoves set to " + MyITOA(TC.MateMaximumDefenderKingMoves) + limit);
	}

	else if ((nameUpperCase == "MATEMAXIMUMDEFENDERMOVES") || (nameUpperCase == "MM"))
	{
		TC.MateMaximumDefenderMoves = atoi(value.c_str());
		if (TC.MateMaximumDefenderMoves > 218)
			limit = " (maximum)";
		else if (TC.MateMaximumDefenderMoves < 0)
			limit = " (minimum)";
		TC.MateMaximumDefenderMoves = std::max(std::min(TC.MateMaximumDefenderMoves, 218), 0);
		//if (IsDebug)
		Output("info string MateMaximumDefenderMoves set to " + MyITOA(TC.MateMaximumDefenderMoves) + limit);
	}

	else if ((nameUpperCase == "MATEMAXIMUMDEFENDERMOVABLEPIECES") || (nameUpperCase == "MP"))
	{
		TC.MateMaximumDefenderMovablePieces = atoi(value.c_str());
		if (TC.MateMaximumDefenderMovablePieces > 16)
			limit = " (maximum)";
		else if (TC.MateMaximumDefenderMovablePieces < 0)
			limit = " (minimum)";
		TC.MateMaximumDefenderMovablePieces = std::max(std::min(TC.MateMaximumDefenderMovablePieces, 16), 0);
		//if (IsDebug)
		Output("info string MateMaximumDefenderMovablePieces set to " + MyITOA(TC.MateMaximumDefenderMovablePieces) + limit);
	}

	else if ((nameUpperCase == "MATEMAXIMUMREVERSIBLEMOVES") || (nameUpperCase == "MMRM"))
	{
		TC.MateMaximumReversibleMoves = atoi(value.c_str());
		if (TC.MateMaximumReversibleMoves > 100)
			limit = " (maximum)";
		else if (TC.MateMaximumReversibleMoves < 0)
			limit = " (minimum)";
		TC.MateMaximumReversibleMoves = std::max(std::min(TC.MateMaximumReversibleMoves, 100), 0);
		//if (IsDebug)
		Output("info string MateMaximumReversibleMoves set to " + MyITOA(TC.MateMaximumReversibleMoves) + limit);
	}

	else if ((nameUpperCase == "MATEMINIMUMATTACKERMATERIAL") || (nameUpperCase == "MMAM"))
	{
		TC.MateMinimumAttackerMaterial = atoi(value.c_str());
		if (TC.MateMinimumAttackerMaterial > 39)
			limit = " (maximum)";
		else if (TC.MateMinimumAttackerMaterial < 0)
			limit = " (minimum)";
		TC.MateMinimumAttackerMaterial = std::max(std::min(TC.MateMinimumAttackerMaterial, 39), 0);
		//if (IsDebug)
		Output("info string MateMinimumAttackerMaterial set to " + MyITOA(TC.MateMinimumAttackerMaterial) + limit);
	}

	else if ((nameUpperCase == "MATEFIXEDPIECES") || (nameUpperCase == "MF"))
	{
		TC.MateFixedPieces = value;
		//if (IsDebug)
		Output("info string MateFixedPieces set to '" + TC.MateFixedPieces + "'");
	}

	else if (nameUpperCase == "SPT")
	{
		ShowPVTerminators = (UpperCase(value) == "TRUE");
		//if (IsDebug)
		Output("info string ShowPVTerminators set to " + MyBooleanTOA(ShowPVTerminators));
	}

	else if (nameUpperCase == "BL")
	{
		BlankLines = (UpperCase(value) == "TRUE");
		//if (IsDebug)
		Output("info string BlankLines set to " + MyBooleanTOA(BlankLines));
	}

	else
	{
		Output("info string *** Unknown option!: " + name);
	}
}

// Process the input from stdin
std::string ProcessInput(std::string currentLine)
{
	std::string result = "";
	std::string originalCurrentLine = currentLine;
	std::string commandToken;

	// Send the input line to the log and flush it
	if (Logging)
		OutputLog(">>" + currentLine);

	if (currentLine != "")
	{
		trim(currentLine);
		commandToken = UpperCase(GetNextToken(&currentLine));

		// Identify the appropriate command

		if (commandToken == "ISREADY")
		{
			Output("readyok");
		}
		else

			if (commandToken == "GO")
			{ // e.g. go depth 9, go movetime 1000, go wtime nnnnn btime nnnnn winc nnn binc nnn, go perft 7, go mate 5
				if (ComputingMove)
				{
					result = "Cannot process a GO command whilst a previous GO command is still active";
					OutputError(result);
				}
				else
				{
					// Start computing a move for the current position
					std::string subCommand;
					bool validSubCommand = false;

					Pondering = false;
					Infinite = false;
					MovesToGo = 0; // If MovesToGo is not sent, then assume 'whole game' mode
					WInc = 0; // These are not sent as zero by GUI, only if non-zero
					BInc = 0;
					MinimumIterationPly = 0;

					if (currentLine == "") // If the command is just "go", interprete it as go infinite
						currentLine = "INFINITE";

					while (currentLine != "")
					{
						subCommand = UpperCase(GetNextToken(&currentLine));

						if (subCommand == "PONDER")
						{
							validSubCommand = true;
							Pondering = true;
						}
						else if (subCommand == "WTIME")
						{
							validSubCommand = true;
							TC.CurrentType = (int)TCTTournament;
							WTime = atoi(GetNextToken(&currentLine).c_str());
							MinimumIterationPly = Normal::MinimumIterationPlyTimedModes;
						}
						else if (subCommand == "BTIME")
						{
							validSubCommand = true;
							TC.CurrentType = (int)TCTTournament;
							BTime = atoi(GetNextToken(&currentLine).c_str());
							MinimumIterationPly = Normal::MinimumIterationPlyTimedModes;
						}
						else if (subCommand == "WINC")
						{
							validSubCommand = true;
							TC.CurrentType = (int)TCTTournament;
							WInc = atoi(GetNextToken(&currentLine).c_str());
							MinimumIterationPly = Normal::MinimumIterationPlyTimedModes;
						}
						else if (subCommand == "BINC")
						{
							validSubCommand = true;
							TC.CurrentType = (int)TCTTournament;
							BInc = atoi(GetNextToken(&currentLine).c_str());
							MinimumIterationPly = Normal::MinimumIterationPlyTimedModes;
						}
						else if (subCommand == "MOVESTOGO")
						{
							validSubCommand = true;
							TC.CurrentType = (int)TCTTournament;
							MovesToGo = atoi(GetNextToken(&currentLine).c_str());
							MinimumIterationPly = Normal::MinimumIterationPlyTimedModes;
						}
						else if (subCommand == "DEPTH")
						{
							validSubCommand = true;
							TC.CurrentType = TCTFixedDepth;
							TC.FixedDepthPly = atoi(GetNextToken(&currentLine).c_str());
						}
						else if (subCommand == "NODES")
						{
							validSubCommand = true;
							TC.CurrentType = TCTFixedNodes;
							TC.FixedNodesCount = atoi(GetNextToken(&currentLine).c_str());
						}
						else if (subCommand == "MATE")
						{
							validSubCommand = true;
							// e.g. go mate 5, go mate 5 file, go mate 5 file filename
							ClearEverythingForDeterminancy();
							TC.CurrentType = TCTMateInN;
							TC.MateInN = atoi(GetNextToken(&currentLine).c_str());
							TC.MateFilename = "";
							if (currentLine != "")
							{
								if (UpperCase(GetNextToken(&currentLine)) == "FILE")
								{
									// The files are generally used to run a large number of mate positions and verify the results against known correct values
									// Use default filenames unless one is explicitly specified
									TC.MateFilename = "MateTestPositions.txt";
									if (currentLine != "")
										TC.MateFilename = currentLine;
								}
							}
						}
						else if (subCommand == "PERFT")
						{
							int perftDepth = atoi(GetNextToken(&currentLine).c_str());
							if ((perftDepth >= 1) && (perftDepth <= 63))
							{
								validSubCommand = true;
								// e.g. go perft 5, go perft 5 file, go perft 5 file filename
								TC.CurrentType = TCTPerftN;
								TC.PerftN = perftDepth;
								TC.PerftFilename = "";
								if (currentLine != "")
								{
									if (UpperCase(GetNextToken(&currentLine)) == "FILE")
									{
										// The files are generally used to run a large number of perft positions and verify the results against known correct values
										// Use default filenames unless one is explicitly specified
										TC.PerftFilename = "PerftTestPositions.txt";
										if (UCI_Chess960)
											TC.PerftFilename = "PerftTestPositionsChess960.txt";
										if (currentLine != "")
											TC.PerftFilename = currentLine;
									}
								}
							}
							else
								result = "*** Error: The Perft depth must be from 1 to 63";
						}
						else if (subCommand == "MOVETIME")
						{
							validSubCommand = true;
							TC.CurrentType = TCTFixedTime;
							TC.FixedTimeMilliSeconds = atoi(GetNextToken(&currentLine).c_str());
						}
						else if (subCommand == "INFINITE")
						{
							validSubCommand = true;
							TC.CurrentType = TCTFixedDepth;
							TC.FixedDepthPly = Normal::MaximumIterationPly + 1; // The '+1' forces it to keep repeating the iteration at max depth
						}
					}

					if (validSubCommand)
					{
						ComputingMove = true;
						std::thread ComputeThread;
						switch (TC.CurrentType)
						{
						case TCTMateInN:
							ComputeThread = std::thread(Mate::ComputeMateWrapper);
							break;
						case TCTPerftN:
							ComputeThread = std::thread(Perft::ComputePerftWrapper);
							break;
						default:
							ComputeThread = std::thread(Normal::ComputeNormalWrapper);
							break;
						}
						ComputeThread.detach(); // Allow the launched 'compute' thread and this main process thread to continue independently otherwise you get an error when the thread object goes out of scope
					}
					else
						result = "Invalid GO command!";
				}
			}
			else if ((commandToken == "UCINEWGAME") || (commandToken == "UGINEWGAME"))
			{
				// The UCI command UCINEWGAME gives the engine a chance to clear data structures (e.g. tranposition tables, killers, history etc) for determinancy
				// but it is redundant for the game record etc as GUIs always send a POSITION STARTPOS MOVES m1 m2 m3 etc... (or for Chess960/FRC a POSITION FEN ...) before a GO command anyway
				NewGame(true);
			}
			else if (commandToken == "SETOPTION") //e.g. setoption name SyzygyPath value j:\chessendgametablebases
			{
				// Set engine option

				std::string s, name, value;

				s = UpperCase(GetNextToken(&currentLine));
				if (s != "NAME")
					Output("info string setoption command: 'name' token not found!");
				else
				{
					name = GetNextToken(&currentLine);
					if (name == "")
						Output("info string setoption command: 'name' identifier not found!");
					else
					{
						s = UpperCase(GetNextToken(&currentLine));
						if (s != "VALUE")
							Output("info string setoption command: 'value' token not found!");
						else
						{
							value = GetNextToken(&currentLine);
							SetOption(originalCurrentLine, name, value);
						}
					}
				}
			}
			else if ((commandToken == "POSITION") || (commandToken == "P")) // syntax: position [fen <fenstring> | startpos] moves <move1> ... <movei>
			{
				if (ComputingMove)
				{
					result = "Cannot process a POSITION command whilst a GO command is still active";
					OutputError(result);
				}
				else
				{
					// Set up position from FEN std::string (the information is stored in the idleing engine object)
					SetPositionAndMoves(originalCurrentLine);
				}
			}
			else if (commandToken == "STOP")
			{
				// Force the engine to finish its search
				StopImmediately = true;
				StopWhenIterationComplete = true;
				Pondering = false;
			}
			else if (commandToken == "PONDERHIT")
			{
				// Tell the engine that the 'ponder' (assumed) move was actually played by the opponent
				StopImmediately = ReplyImmediately;
				StopWhenIterationComplete = ReplyImmediately;
				Pondering = false;
			}
			else if ((commandToken == "UCI") || (commandToken == "UGI"))
			{
				// Display program information
				std::string softwarePopulationCount = "";
#ifdef TB_NO_HW_POP_COUNT
				SoftwarePopulationCount = "SOFTWAREPOPULATIONCOUNT";
#endif
				std::string bitness = "";
#ifndef _WIN64
				bitness = " (32-bit)";
#endif
				std::string debug = "";
#ifdef _DEBUG
				debug = "-DEBUG!";
#endif
				std::string logging = "";
				if (Logging)
					logging = " Logging Enabled!";

				Output("id name Colossus " + std::string(VersionX) + softwarePopulationCount + bitness + debug + logging);
				Output("id author Martin Bryant");
				SendOptions();
				Output("uciok");
			}
			else if (commandToken == "DEBUG")
			{
				IsDebug = (UpperCase(GetNextToken(&currentLine)) == "ON");
				Output("info string Debug set to " + MyBooleanTOA(IsDebug));
			}
			else if (commandToken == "QUIT")
			{
				// Quit the program
				Quit = true;

				FreeAnyTranspositionTableMemory();
				FreeMatingPositionsTableMemory();

				// SYZYGY EGTBs
				tb_free();
			}
			else if (commandToken == "-STOPAFTERITERATION")
			{
				// Force the engine to finish its search after the current iteration is complete
				StopWhenIterationComplete = true;
			}
			else if ((commandToken == "-WRITEBOARD") || (commandToken == "D"))
			{
				// Write the current board position
				WriteMailboxBoard64(&EngineBrain);
			}
			else if (commandToken == "V")
			{
				// Write the current option values
				SendOptionValues();
			}
			else if (commandToken == "-FILE")
			{ // e.g. -file filename
				std::string filename = "input.txt";
				if (currentLine != "")
					filename = currentLine;

				// Take input from file
				fopen_s(&CommandFile, filename.c_str(), "r");
				if (CommandFile != NULL)
					ProcessingCommandFile = true; // Used later in ComputeMove to log output
				else
					Output("***File not found");
			}
			else if (commandToken == "-SE")
			{
				// Static evaluation
				Normal* normal = new Normal;
				normal->StaticEvaluation();
				delete normal;
			}
			else if (commandToken == "-TS0")
			{
				// Test evaluation function symmetry #0
				Normal* normal = new Normal;
				normal->TestSymmetry0();
				delete normal;
			}
			else if (commandToken == "-TS1")
			{
				// Test evaluation function symmetry #1
				Normal* normal = new Normal;
				normal->TestSymmetry1();
				delete normal;
			}
			else if (commandToken == "-TS2")
			{
				// Test evaluation function symmetry #2
				Normal* normal = new Normal;
				normal->TestSymmetry2();
				delete normal;
			}
#ifdef EXPERIMENTAL
			else if (commandToken == "-MMQ")
			{
				// Search for the position with the maximum number of moves
				MaximumMovesQueens();
			}
			else if (commandToken == "-MMDD")
			{
				// Search for the position with the maximum number of moves
				MaximumMovesDeleteDuplicates();
			}
			else if (commandToken == "-MM")
			{
				//int i = std::stoi(tokens[1]);
				int i = std::stoi(currentLine);

				// Search for the position with the maximum number of moves
				MaximumMoves(i);
			}
			else

				//if (commandToken == "-TT")
				//{
				//	//TexelTuning();
				//}
				//else

				//if (commandToken == "-TTS")
				//{
				//	TexelTuningStats();
				//}
				//else

				if (commandToken == "-GS")
				{
					//GameStatistics();
				}
				else

					//if (commandToken == "-A")
					//{
					//	Adjust();
					//}
					//else

					if (commandToken == "-GKP")
					{
						//GenerateKP2();
					}
					else

						//if (commandToken == "-EGTBPST")
						//{
						//	KBNvK();
						//}
						//else

						if (commandToken == "-U")
						{
							// Generate unique positions for huge Perft
							//Unique();
							Perft::ComputePerftUnique();
						}
						else if (commandToken == "-U2")
						{
							// Generate unique positions for huge Perft
							//Unique();
							Perft::ComputePerftUnique2();
						}
						else

							if (commandToken == "-SEE")
							{
								// Used for testing SEE routine
								Normal normal;
								normal.TestSEE();
							}
							else if (commandToken == "-MISC")
							{
								// Used for any miscellaneous one-off tests
								//NNUETest();
								//EGTB7Stats();
							}
#endif // EXPERIMENTAL
			else
			{
				// Report unknown commands
				result = "Unknown command!";
			}
	}

	if (result != "")
	{
		result = "*** Error: " + result + " ('" + originalCurrentLine + "')";
		Output("info string " + result);
	}

	return result;
}
