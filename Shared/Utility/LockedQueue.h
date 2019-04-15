
#ifndef LockedQueue_h__
#define LockedQueue_h__

#include "Malloc.h"
#include <pthread.h>
#include <stdbool.h>

typedef struct Node
{
    void* Data;
    struct Node* Next;
} Node;

typedef struct
{
    pthread_mutex_t _lock;

    Node* _head;
    Node* _tail;
} LockedQueue;

static inline LockedQueue* LockedQueue_Create(void)
{
    LockedQueue* q = Malloc(sizeof(LockedQueue));
    pthread_mutex_init(&q->_lock, NULL);
    q->_head = NULL;
    q->_tail = NULL;
    return q;
}

//! Adds an item to the queue.
static inline void LockedQueue_Add(LockedQueue* q, void* item)
{
    pthread_mutex_lock(&q->_lock);

    Node* node = Malloc(sizeof(Node));
    node->Data = item;
    node->Next = NULL;

    if (!q->_head)
    {
        q->_head = node;
        q->_tail = node;
    }
    else
    {
        q->_tail->Next = node;
        q->_tail = node;
    }

    pthread_mutex_unlock(&q->_lock);
}

//! Gets the next result in the queue, if any.
static inline void* LockedQueue_Next(LockedQueue* q)
{
    pthread_mutex_lock(&q->_lock);

    if (!q->_head)
    {
        pthread_mutex_unlock(&q->_lock);
        return NULL;
    }

    Node* head = q->_head;
    void* result = head->Data;
    q->_head = head->Next;
    Free(head);

    pthread_mutex_unlock(&q->_lock);
    return result;
}

//! Destroy a LockedQueue.
static inline void LockedQueue_Destroy(LockedQueue* q, void(*element_destroyer)(void*))
{
    Node* i = q->_head;
    while (i != NULL)
    {
        Node* next = i->Next;
        element_destroyer(i->Data);
        Free(i);
        i = next;
    }

    pthread_mutex_destroy(&q->_lock);
    Free(q);
}

#endif //LockedQueue_h__
