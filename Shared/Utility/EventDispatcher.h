
#ifndef EventDispatcher_h__
#define EventDispatcher_h__

#include <stdbool.h>

// tiempo a dormir en milisegundos por iteración
#define SLEEP_CONST 50

typedef struct EventDispatcher EventDispatcher;

enum
{
    EV_READ  = 0x01,
    EV_WRITE = 0x02
};

/*
 * Inicializa el EventDispatcher
 */
bool EventDispatcher_Init(void);

/*
 * Añade un nuevo item que implemente la interfaz de abstracciones de File Descriptors
 * Para que sus callback sean llamados
 */
void EventDispatcher_AddFDI(void* interface);

/*
 * Notifica un cambio en los eventos que nos interesan
 */
void EventDispatcher_Notify(void* interface);

/*
 * Quita un item de la lista activa, sus eventos serán ignorados
 */
void EventDispatcher_RemoveFDI(void* interface);

/*
 * Chequea la existencia de eventos, en cuyo caso invoca los callback correspondientes
 */
void EventDispatcher_Dispatch(void);

/*
 * Main loop de los procesos
 */
void EventDispatcher_Loop(void);

/*
 * Destruye el EventDispatcher
 */
void EventDispatcher_Terminate(void);

#endif //EventDispatcher_h__
