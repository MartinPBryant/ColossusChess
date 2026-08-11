#pragma once

#include "Brain.h"

//----------------------------------------------------------------------------------------------------

extern FILE *CommandFile;
extern bool ProcessingCommandFile;

//----------------------------------------------------------------------------------------------------

std::string ProcessInput(std::string currentLine);
