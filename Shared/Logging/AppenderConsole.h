
#ifndef AppenderConsole_h__
#define AppenderConsole_h__

#include "LogCommon.h"

typedef enum
{
    BLACK,
    RED,
    GREEN,
    BROWN,
    BLUE,
    MAGENTA,
    CYAN,
    GREY,
    YELLOW,
    LRED,
    LGREEN,
    LBLUE,
    LMAGENTA,
    LCYAN,
    WHITE,
    MAX
} ColorTypes;

typedef struct Appender Appender;

Appender* AppenderConsole_Create(LogLevel level, AppenderFlags flags, char const* colorStr);

#endif //AppenderConsole_h__
