
#ifndef Console_h__
#define Console_h__

#include "LockedQueue.h"
#include <stdatomic.h>
#include <vector.h>

typedef void CLICommandHandlerFn(Vector const* args);

typedef struct
{
    char const* CmdName;
    CLICommandHandlerFn* Handler;
} CLICommand;

extern CLICommand const CLICommands[];
extern char const* CLIPrompt;
extern LockedQueue* CLICommandQueue;
extern atomic_bool ProcessRunning;

void SigintSetup(void);
void* CliThread(void*);
void AtenderComando(char const*);

#endif //Console_h__
