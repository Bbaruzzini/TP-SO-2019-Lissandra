
#include "Runner.h"
#include "Commands.h"
#include <Config.h>
#include <Console.h>
#include <libcommons/string.h>
#include <libcommons/queue.h>
#include <Logger.h>
#include <Malloc.h>
#include <pthread.h>
#include <string.h>
#include <Timer.h>

typedef struct
{
    Vector Data;
    size_t IP;
} TCB;

static void _terminateWorker(void* pWorkId);
static void _addToReadyQueue(TCB* tcb);
static bool _parseCommand(char const* command);

static void* _schedulerThread(void*);
static void* _workerThread(void*);

// no uso LockedQueeue porque quiero hacer algo de sincronizacion en el medio
static t_queue* ReadyQueue;
static pthread_mutex_t QueueLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t QueueCond = PTHREAD_COND_INITIALIZER;

static pthread_t SchedulerThread;

// uso una segunda cola para los procesos que terminan su quantum, parecido a un VRR pero sin la prioridad que ello conlleva
// sirve para el caso limite de tener MP = 1, evita que el signal en la cola ready llegue antes que el
// wait, bloqueando al unico consumidor indefinidamente
static t_queue* ReturningQueue;
static pthread_mutex_t RetQueueLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t RetQueueCond = PTHREAD_COND_INITIALIZER;

static Vector WorkerIds;

void Runner_Init(void)
{
    ReadyQueue = queue_create();
    ReturningQueue = queue_create();

    pthread_rwlock_rdlock(&sConfigLock);
    size_t multiprocessing = config_get_int_value(sConfig, "MULTIPROCESAMIENTO");
    pthread_rwlock_unlock(&sConfigLock);

    Vector_Construct(&WorkerIds, sizeof(pthread_t), _terminateWorker, multiprocessing);
    for (size_t i = 0; i < multiprocessing; ++i)
    {
        pthread_t tid;
        pthread_create(&tid, NULL, _workerThread, NULL);
        Vector_push_back(&WorkerIds, &tid);
    }
    pthread_create(&SchedulerThread, NULL, _schedulerThread, NULL);
}

void Runner_AddSingle(char const* line)
{
    TCB* tcb = Malloc(sizeof(TCB));
    tcb->Data = string_n_split(line, 1, NULL);
    tcb->IP = 0;

    pthread_mutex_lock(&QueueLock);
    _addToReadyQueue(tcb);
    pthread_mutex_unlock(&QueueLock);
}

void Runner_AddScript(File* sc)
{
    Vector lines = file_getlines(sc);
    file_close(sc);

    TCB* tcb = Malloc(sizeof(TCB));
    tcb->Data = lines;
    tcb->IP = 0;

    pthread_mutex_lock(&QueueLock);
    _addToReadyQueue(tcb);
    pthread_mutex_unlock(&QueueLock);
}

void Runner_Terminate(void)
{
    pthread_cancel(SchedulerThread);
    Vector_Destruct(&WorkerIds);
    queue_destroy_and_destroy_elements(ReturningQueue, Free);
    queue_destroy_and_destroy_elements(ReadyQueue, Free);
}

/* PRIVATE*/
static void _terminateWorker(void* pWorkId)
{
    pthread_t const tid = *((pthread_t*) pWorkId);
    pthread_cancel(tid);
}

static void _addToReadyQueue(TCB* tcb)
{
    //PRE: debe poseer el ReadyLock
    queue_push(ReadyQueue, tcb);
    pthread_cond_signal(&QueueCond); // signal a un worker
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
    Vector args = string_split(command, " ");
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

static void* _schedulerThread(void* arg)
{
    (void) arg;

    while (ProcessRunning)
    {
        pthread_mutex_lock(&RetQueueLock);
        while (queue_is_empty(ReturningQueue))
            pthread_cond_wait(&RetQueueCond, &RetQueueLock);

        pthread_mutex_lock(&QueueLock);
        TCB* tcb;
        while ((tcb = queue_pop(ReturningQueue)))
            _addToReadyQueue(tcb);
        pthread_mutex_unlock(&QueueLock);

        pthread_mutex_unlock(&RetQueueLock);
    }

    return NULL;
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
        pthread_rwlock_rdlock(&sConfigLock);
        uint32_t q = config_get_long_value(sConfig, "QUANTUM");
        uint32_t delayMS = config_get_long_value(sConfig, "SLEEP_EJECUCION");
        pthread_rwlock_unlock(&sConfigLock);

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
            Vector_Destruct(&tcb->Data);
            Free(tcb);
        }
        else
        {
            // solo termino el quantum pero quedan lineas por ejecutar, replanificarlas
            pthread_mutex_lock(&RetQueueLock);
            queue_push(ReturningQueue, tcb);
            pthread_cond_signal(&RetQueueCond); // avisamos al sched que hay tareas para replanificar
            pthread_mutex_unlock(&RetQueueLock);
        }
    }

    return NULL;
}
