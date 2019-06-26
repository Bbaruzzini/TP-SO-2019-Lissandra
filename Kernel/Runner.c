
#include "Runner.h"
#include "Commands.h"
#include "Config.h"
#include <Console.h>
#include <libcommons/string.h>
#include <libcommons/queue.h>
#include <Logger.h>
#include <Malloc.h>
#include <string.h>
#include <Threads.h>
#include <Timer.h>

typedef struct
{
    Vector Data;
    size_t IP;
} TCB;

static TCB* _create_task(Vector const* data);
static void _delete_task(TCB* tcb);

static void _addSingleLine(char const* line);

static void _addToReadyQueue(TCB* tcb);
static bool _parseCommand(char const* command);

static void* _workerThread(void*);

// no uso LockedQueue porque quiero hacer algo de sincronizacion en el medio
static t_queue* ReadyQueue;
static pthread_mutex_t QueueLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t QueueCond = PTHREAD_COND_INITIALIZER;

void Runner_Init(void)
{
    CommandParser = _addSingleLine;

    ReadyQueue = queue_create();
    for (uint32_t i = 0; i < ConfigKernel.MULTIPROCESAMIENTO; ++i)
        Threads_CreateDetached(_workerThread, NULL);
}

void Runner_AddScript(File* sc)
{
    Vector script = file_getlines(sc);
    file_close(sc);

    _addToReadyQueue(_create_task(&script));
}

/* PRIVATE*/
static TCB* _create_task(Vector const* data)
{
    TCB* tcb = Malloc(sizeof(TCB));
    tcb->Data = *data;
    tcb->IP = 0;

    return tcb;
}

static void _delete_task(TCB* tcb)
{
    Vector_Destruct(&tcb->Data);
    Free(tcb);
}

static void _addSingleLine(char const* line)
{
    Vector script = string_n_split(line, 1, NULL);
    _addToReadyQueue(_create_task(&script));
}

static void _addToReadyQueue(TCB* tcb)
{
    pthread_mutex_lock(&QueueLock);
    queue_push(ReadyQueue, tcb);
    pthread_cond_signal(&QueueCond); // signal a un worker
    pthread_mutex_unlock(&QueueLock);
}

static bool _parseCommand(char const* command)
{
    // copy pasteo horrible de Console.c pero no se me ocurre como evitar la duplicacion codigo
    // ya que LFS y Memoria van a usar los handler de consola directamente, sin pasar por un runner como en este caso
    size_t spc = strcspn(command, " ");
    char* cmd = Malloc(spc + 1);
    strncpy(cmd, command, spc + 1);
    cmd[spc] = '\0';

    // por defecto falso, ejemplo el comando no es uno valido
    bool res = false;
    Vector args = string_q_split(command, ' ');
    for (uint32_t i = 0; ScriptCommands[i].CmdName != NULL; ++i)
    {
        if (!strcmp(ScriptCommands[i].CmdName, cmd))
        {
            if (ScriptCommands[i].Handler(&args))
                res = true;
            break;
        }
    }

    Vector_Destruct(&args);
    Free(cmd);
    return res;
}

static void* _workerThread(void* arg)
{
    (void) arg;

    while (ProcessRunning)
    {
        TCB* tcb;
        pthread_mutex_lock(&QueueLock);
        while (!(tcb = queue_pop(ReadyQueue)))
            pthread_cond_wait(&QueueCond, &QueueLock);
        pthread_mutex_unlock(&QueueLock);

        // ahora tcb esta cargado con un script, proseguimos
        // campos recargables asi que consulto al atender una nueva tarea
        uint32_t q = atomic_load(&ConfigKernel.QUANTUM);
        uint32_t delayMS = atomic_load(&ConfigKernel.SLEEP_EJECUCION);

        bool abnormalTermination = false;
        uint32_t exec = 0;
        while (tcb->IP < Vector_size(&tcb->Data))
        {
            char* const* const lines = Vector_data(&tcb->Data);
            char* const IR = lines[tcb->IP++]; // Instruction Register :P

            bool res = _parseCommand(IR);
            MSSleep(delayMS);
            if (!res)
            {
                LISSANDRA_LOG_ERROR("ScriptRunner: error al ejecutar linea %u (\"%s\")", tcb->IP, IR);
                abnormalTermination = true;
                break;
            }

            if (++exec == q)
                break;
        }

        // salimos por un error en ejecucion o termino el script?
        if (abnormalTermination || tcb->IP == Vector_size(&tcb->Data))
        {
            // borrar los datos
            _delete_task(tcb);
            continue;
        }

        // solo termino el quantum pero quedan lineas por ejecutar, replanificarlas
        _addToReadyQueue(tcb);
    }

    return NULL;
}
