
#include "CLIHandlers.h"
#include "Runner.h"
#include <libcommons/string.h>

CLICommand const CLICommands[] =
{
    { "SELECT",   ScheduleCommand },
    { "INSERT",   ScheduleCommand },
    { "CREATE",   ScheduleCommand },
    { "DESCRIBE", ScheduleCommand },
    { "DROP",     ScheduleCommand },
    { "JOURNAL",  ScheduleCommand },
    { "ADD",      ScheduleCommand },
    { "RUN",      ScheduleCommand },
    { "METRICS",  ScheduleCommand },
    { NULL,       NULL            }
};

// Request unitarias realizadas mediante la API de este componente, las cuales serán consideradas como archivos LQL de una única línea
void ScheduleCommand(Vector const* args)
{
    char* cmd = string_new();

    for (size_t i = 0; i < Vector_size(args); ++i)
    {
        char** const tokens = Vector_data(args);
        string_append_with_format(&cmd, "%s ", tokens[i]);
    }

    Runner_AddSingle(cmd);
    Free(cmd);
}
