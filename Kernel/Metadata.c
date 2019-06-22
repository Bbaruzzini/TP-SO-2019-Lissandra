
#include "Metadata.h"
#include <libcommons/dictionary.h>
#include <Malloc.h>
#include <pthread.h>

typedef struct
{
    CriteriaType ct;
} MDEntry;

static t_dictionary* Metadata;
static pthread_rwlock_t MetadataLock = PTHREAD_RWLOCK_INITIALIZER;

void Metadata_Init(void)
{
    Metadata = dictionary_create();
}

void Metadata_Add(char const* name, CriteriaType ct)
{
    pthread_rwlock_wrlock(&MetadataLock);

    MDEntry* entry = dictionary_get(Metadata, name);
    if (entry)
        entry->ct = ct;
    else
    {
        entry = Malloc(sizeof(MDEntry));
        entry->ct = ct;
        dictionary_put(Metadata, name, entry);
    }

    pthread_rwlock_unlock(&MetadataLock);
}

bool Metadata_Get(char const* name, CriteriaType* ct)
{
    MDEntry* entry;

    pthread_rwlock_rdlock(&MetadataLock);
    entry = dictionary_get(Metadata, name);
    pthread_rwlock_unlock(&MetadataLock);

    if (!entry)
        return false;

    if (ct)
        *ct = entry->ct;
    return true;
}

void Metadata_Del(char const* name)
{
    pthread_rwlock_wrlock(&MetadataLock);

    dictionary_remove_and_destroy(Metadata, name, Free);

    pthread_rwlock_unlock(&MetadataLock);
}

void Metadata_Clear(void)
{
    pthread_rwlock_wrlock(&MetadataLock);

    dictionary_clean_and_destroy_elements(Metadata, Free);

    pthread_rwlock_unlock(&MetadataLock);
}

void Metadata_Destroy(void)
{
    dictionary_destroy_and_destroy_elements(Metadata, Free);
}
