
#include "Logger.h"
#include "Appender.h"
#include "LogMessage.h"
#include "Malloc.h"
#include <libcommons/string.h>
#include <stdio.h>
#include <string.h>

static Logger sLogger;

void _vectorFreeFn(void* elem)
{
    Appender* appender = *((Appender**) elem);
    Appender_Destroy(appender);
}

void Logger_Init(LogLevel level)
{
    sLogger.Level = level;
    Vector_Construct(&sLogger.Appenders, sizeof(Appender*), _vectorFreeFn, 0);

    char* timestamp;
    struct timespec time;
    timespec_get(&time, TIME_UTC);
    GetTimeStr(&time, &timestamp);
    sLogger.TimeStampStr = string_from_format("%s", timestamp);
    Free(timestamp);
}

void Logger_AddAppender(Appender* appender)
{
    Vector_push_back(&sLogger.Appenders, &appender);
}

void Logger_DelAppenders(void)
{
    Vector_clear(&sLogger.Appenders);
}

LogLevel Logger_GetLogLevel(void)
{
    return sLogger.Level;
}

void Logger_SetLogLevel(LogLevel level)
{
    sLogger.Level = level;
}

void _writeWrapper(void* elem, void* data)
{
    Appender* appender = *((Appender**) elem);
    LogMessage* msg = (LogMessage*) data;
    Appender_Write(appender, msg);
}

void Logger_Write(LogMessage* message)
{
    if (!sLogger.Level || sLogger.Level > message->Level || !strlen(message->Text))
        return;

    Vector_iterate_with_data(&sLogger.Appenders, _writeWrapper, message);
    LogMessage_Destroy(message);
}

bool Logger_ShouldLog(LogLevel level)
{
    return sLogger.Level != LOG_LEVEL_DISABLED && sLogger.Level <= level;
}

void Logger_Format(LogLevel level, char const* format, ...)
{
    va_list v;
    va_start(v, format);
    char* text = string_from_vformat(format, v);
    va_end(v);

    LogMessage* msg = LogMessage_Init(level, text);
    Free(text);

    Logger_Write(msg);
}

char const* Logger_GetLogTimeStampStr(void)
{
    return sLogger.TimeStampStr;
}

void Logger_Terminate(void)
{
    Free(sLogger.TimeStampStr);
    Vector_Destruct(&sLogger.Appenders);
}
