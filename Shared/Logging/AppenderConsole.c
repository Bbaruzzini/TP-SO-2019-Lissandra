
#include "AppenderConsole.h"
#include "Appender.h"
#include "LogMessage.h"
#include "Malloc.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    Appender _base;

    bool colored;
    ColorTypes colors[NUM_ENABLED_LOG_LEVELS];

} AppenderConsole;

static void _initColors(Appender* appender, char const* colorStr);

static void _write(Appender* appender, LogMessage* message);
static void _destroy(Appender* appender);

Appender* AppenderConsole_Create(LogLevel level, AppenderFlags flags, char const* colorStr)
{
    AppenderConsole* me = Malloc(sizeof(AppenderConsole));
    Appender_Init(&me->_base, level, flags, _write, _destroy);

    for (uint8_t i = 0; i < NUM_ENABLED_LOG_LEVELS; ++i)
        me->colors[i] = MAX;

    _initColors(&me->_base, colorStr);
    return (Appender*) me;
}

static void _initColors(Appender* appender, char const* colorStr)
{
    AppenderConsole* me = (AppenderConsole*) appender;
    if (!colorStr)
    {
        me->colored = false;
        return;
    }

    int color[NUM_ENABLED_LOG_LEVELS];
    for (uint8_t i = 0; i < NUM_ENABLED_LOG_LEVELS; ++i)
    {
        char c = toupper(colorStr[i]);
        if (!c || ((c < '0' || c > '9') && (c < 'A' || c >= 'F')))
            return;

        color[i] = isdigit(c) ? c - '0' : c - 'A' + 10;
        if (color[i] < 0 || color[i] >= MAX)
            return;
    }

    for (uint8_t i = 0; i < NUM_ENABLED_LOG_LEVELS; ++i)
        me->colors[i] = (ColorTypes) color[i];

    me->colored = true;
}

static void _write(Appender* appender, LogMessage* message)
{
    AppenderConsole* me = (AppenderConsole*) appender;
    FILE* const out = (message->Level < LOG_LEVEL_ERROR) ? stdout : stderr;

    if (me->colored)
    {
        enum ANSITextAttr
        {
            TA_NORMAL = 0, TA_BOLD = 1, TA_BLINK = 5, TA_REVERSE = 7
        };

        enum ANSIFgTextAttr
        {
            FG_BLACK = 30, FG_RED, FG_GREEN, FG_BROWN, FG_BLUE, FG_MAGENTA, FG_CYAN, FG_WHITE, FG_YELLOW
        };

        enum ANSIBgTextAttr
        {
            BG_BLACK = 40, BG_RED, BG_GREEN, BG_BROWN, BG_BLUE, BG_MAGENTA, BG_CYAN, BG_WHITE
        };

        static uint8_t UnixColorFG[MAX] =
        {
            FG_BLACK,    // BLACK
            FG_RED,      // RED
            FG_GREEN,    // GREEN
            FG_BROWN,    // BROWN
            FG_BLUE,     // BLUE
            FG_MAGENTA,  // MAGENTA
            FG_CYAN,     // CYAN
            FG_WHITE,    // WHITE
            FG_YELLOW,   // YELLOW
            FG_RED,      // LRED
            FG_GREEN,    // LGREEN
            FG_BLUE,     // LBLUE
            FG_MAGENTA,  // LMAGENTA
            FG_CYAN,     // LCYAN
            FG_WHITE     // LWHITE
        };

        uint8_t index;
        switch (message->Level)
        {
            case LOG_LEVEL_TRACE:
                index = 5;
                break;
            case LOG_LEVEL_DEBUG:
                index = 4;
                break;
            case LOG_LEVEL_INFO:
                index = 3;
                break;
            case LOG_LEVEL_WARN:
                index = 2;
                break;
            case LOG_LEVEL_FATAL:
                index = 0;
                break;
            case LOG_LEVEL_ERROR: // No break on purpose
            default:
                index = 1;
                break;
        }

        ColorTypes const color = me->colors[index];
        fprintf(out, "\x1b[%d%sm%s%s\x1b[0m\n", UnixColorFG[color], (color >= YELLOW && color < MAX ? ";1" : ""),
                message->Prefix, message->Text);
    }
    else
        fprintf(out, "%s%s\n", message->Prefix, message->Text);
}

static void _destroy(Appender* appender)
{
    AppenderConsole* me = (AppenderConsole*) appender;
    Free(me);
}
