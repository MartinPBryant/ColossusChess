#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>

#include "Engine.h"
#include "UGI.h"
#include "Utilities.h"

//----------------------------------------------------------------------------------------------------

// Program entry point
int main(int argc, char * argv[])
{
	// Determine and display this computer's CPU information
	CPUInfo();

	// Test for compatibility
	bool runnable = true;

	// First check that the current CPU supports POPCNT, LZCNT and TZCNT(part of BMI1)
	if (((ThisCPUSupports & EISPOPCNT) == 0) || ((ThisCPUSupports & EISLZCNT) == 0) || ((ThisCPUSupports & EISBMI1) == 0))
	{
		Output("***** This executable uses the POPCNT, LZCNT and TZCNT instructions which are NOT all supported by this CPU.");
		runnable = false;
	}
	else
	{
#ifdef PEXT
		// Next check that the current CPU supports BMI2 (because we are running the executable that requires it!)
		if (ThisCPUSupports & EISBMI2)
		{
			if ((CPUVendorId == CPUVENDORAMD) && (CPUFamily < 25))
			{
				Output("***** This executable uses the BMI2:PEXT instruction which will NOT run efficiently on AMD CPUs prior to the Zen 3 (Family>=25) architecture.");
				Output("***** Please use the correct baseline executable.");
				runnable = false;
			}
		}
		else
		{
			Output("***** This executable uses the BMI2:PEXT instruction which is NOT supported by this CPU.");
			Output("***** Please use the correct baseline executable.");
			runnable = false;
		}
#else
		// Next check if the current CPU supports BMI2 (because we are running the executable that doesn't require it!)
		if ((ThisCPUSupports & EISBMI2) && (((CPUVendorId == CPUVENDORAMD) && (CPUFamily >= 25)) || (CPUVendorId == CPUVENDORINTEL)))
		{
			Output("info string ***** This version of the executable does not use the BMI2:PEXT instruction but this CPU IS capable of using it.");
			Output("info string ***** Please use the correct BMI2 executable.\n");
			//runnable = false;
		}
#endif
	}
	if (!runnable)
	{
		Output("Press return to exit.");
		std::string s;
		std::getline(std::cin, s);
		return EXIT_FAILURE;
	}

	// Determine if 'large pages' are available
	LargePages();

	//----------------------------------------------------------------------------------------------------
	
	// Get the application's path/filename and construct various utility file paths
	std::string ApplicationPath = argv[0];
	size_t index = ApplicationPath.rfind("\\");
	std::string EXEName = ApplicationPath.substr(index + 1);
	auto threadID = std::this_thread::get_id();
	std::stringstream ss;
	ss << threadID;
	std::string UniqueID = ss.str(); // Use the thread ID as a unique identifier to distinguish instances
	LogFileName = EXEName + "." + UniqueID + ".log.txt"; // e.g. Colossus2024b.exe.29128.log.txt
	Logging = false;
	SyzygyPathLogFileName = EXEName + ".SyzygyPathLog.txt";
	ErrorFileName = EXEName + "." + UniqueID + ".error.txt";
	INIPath = ApplicationPath.substr(0, index) + "\\ColossusChess.ini";

	// No I/O buffering on stdin and stdout (required to work with GUIs)
	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);

	// Logging required?
	ReadINI("Logging", "Log");
	if (std::string(INIValue) == "1")
	{
		Output("info string *** Warning! Logging enabled!");
		Logging = true;
	}

	// Various warnings!
#ifndef _WIN64
	Output("info string *** Warning! 32-bit!");
#endif
#ifdef TB_NO_HW_POP_COUNT
	Output("info string *** Warning! Software population count! 10% SLOWER!");
#endif
#ifdef SEARCHINGFORLINE
	Output("info string *** Warning! SEARCHINGFORLINE is on!");
#endif
#ifdef BREAKONCURRENTVARIATIONDEF
	Output("info string *** Warning! BREAKONCURRENTVARIATIONDEF is defined!");
#endif
	PRINTTREE(Output("info string *** Warning! PRINTTREEDEF is defined!"););
#ifdef CRASHLOCATIONDEF
	Output("info string *** Warning! CRASHLOCATIONDEF defined!");
#endif



	//----------------------------------------------------------------------------------------------------

	// Initialise
	ProcessingCommandFile = false;
	ComputingMove = false;
	InitialiseOneOffStuff();
	NewGame(true); // Do this at startup so that an interactive user (i.e. me!) can enter UCI GO commands immediately

	// Process any initialisation commands in the .INI file
	for (char lineNumber = '1'; lineNumber <= '9'; lineNumber++)
	{
		char buffer[2];
		buffer[0] = lineNumber;
		buffer[1] = '\0';
		ReadINI("Initialisation", buffer);
		if (INIValue[0] != '\0')
		{
			std::string result = ProcessInput(INIValue);
		}
	}

	// Main command processing loop
	Quit = false;
	while (!Quit)
	{
		std::string s;
		std::getline(std::cin, s); // This 'blocks' the main thread until some input is received
		ProcessInput(s); // If any UGI 'go' command is received it will launch a separate thread to do the search and return here immediately and loop around to block again
	}

	return EXIT_SUCCESS;
}
