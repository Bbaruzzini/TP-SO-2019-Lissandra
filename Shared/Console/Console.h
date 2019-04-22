
#ifndef Console_h__
#define Console_h__

#include "LockedQueue.h"
#include <stdatomic.h>

typedef void CLICommandHandlerFn(char const* args);

typedef struct
{
    char const* CmdName;
    CLICommandHandlerFn* Handler;
} CLICommand;

extern CLICommand CLICommands[];
extern char const* CLIPrompt;
extern LockedQueue* CLICommandQueue;
extern atomic_bool ProcessRunning;

void* CliThread(void*);
void AtenderComando(char const*);

#endif //Console_h__
