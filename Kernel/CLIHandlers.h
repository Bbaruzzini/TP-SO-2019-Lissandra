
#ifndef CLIHandlers_h__
#define CLIHandlers_h__

#include "Console.h"

void HandleSelect(char const* args);
void HandleInsert(char const* args);
void HandleCreate(char const* args);
void HandleDescribe(char const* args);
void HandleDrop(char const* args);
void HandleJournal(char const* args);
void HandleAdd(char const* args);
void HandleRun(char const* args);
void HandleMetrics(char const* args);

extern CLICommand CLICommands[];

#endif //CLIHandlers_h__