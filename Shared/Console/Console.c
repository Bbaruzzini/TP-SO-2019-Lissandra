
#include "Console.h"
#include <libcommons/string.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdint.h>

char* command_finder(char const* text, int state)
{
    static size_t idx, len;
    char const* ret;
    CLICommand* cmd = CLICommands;

    if (!state)
    {
        idx = 0;
        len = strlen(text);
    }

    while ((ret = cmd[idx].CmdName))
    {
        ++idx;

        if (!strncmp(ret, text, len))
            return strdup(ret);
        if (cmd[idx].CmdName == NULL)
            break;
    }

    return NULL;
}

char** cli_completion(char const* text, int start, int end)
{
    (void) end;
    char** matches = NULL;

    if (start)
        rl_bind_key('\t', rl_abort);
    else
        matches = rl_completion_matches(text, command_finder);
    return matches;
}

int cli_hook_func(void)
{
    if (!ProcessRunning)
        rl_done = 1;
    return 0;
}

void* CliThread(void* param)
{
    (void) param;

    rl_attempted_completion_function = cli_completion;
    rl_event_hook = cli_hook_func;

    while (ProcessRunning)
    {
        fflush(stdout);

        char* command_str = readline(CLIPrompt);
        rl_bind_key('\t', rl_complete);

        if (command_str)
        {
            command_str[strcspn(command_str, "\r\n")] = '\0';

            if (!*command_str)
            {
                Free(command_str);
                continue;
            }

            fflush(stdout);

            LockedQueue_Add(CLICommandQueue, strdup(command_str));

            add_history(command_str);
            Free(command_str);
        }
        else if (feof(stdin))
            ProcessRunning = false;
    }

    rl_clear_history();
    return 0;
}

void AtenderComando(char const* command)
{
    size_t spc = strcspn(command, " ");
    char* cmd = Malloc(spc + 1);
    strncpy(cmd, command, spc + 1);
    cmd[spc] = '\0';

    command = command + spc;

    //skip any extra spaces
    while (*command == ' ')
        ++command;

    Vector args = string_split(command, " ");
    for (uint32_t i = 0; CLICommands[i].CmdName != NULL; ++i)
    {
        if (!strcmp(CLICommands[i].CmdName, cmd))
        {
            CLICommands[i].Handler(&args);
            break;
        }
    }

    Vector_Destruct(&args);
    Free(cmd);
}
