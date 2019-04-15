
#ifndef Console_h__
#define Console_h__

typedef void CLICommandHandlerFn(char const* args);

typedef struct
{
    char const* CmdName;
    CLICommandHandlerFn* Handler;
} CLICommand;

void* CliThread(void*);

#endif //Console_h__
