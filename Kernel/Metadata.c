
#include "Metadata.h"
#include <libcommons/dictionary.h>
#include <Malloc.h>

typedef struct
{
    CriteriaType ct;
} MDEntry;

static t_dictionary* Metadata;

void Metadata_Init(void)
{
    Metadata = dictionary_create();
}

void Metadata_Add(char const* name, CriteriaType ct)
{
    MDEntry* entry = dictionary_get(Metadata, name);
    if (entry)
    {
        entry->ct = ct;
        return;
    }

    entry = Malloc(sizeof(MDEntry));
    entry->ct = ct;
    dictionary_put(Metadata, name, entry);
}

bool Metadata_Get(char const* name, CriteriaType* ct)
{
    MDEntry* entry = dictionary_get(Metadata, name);
    if (!entry)
        return false;

    *ct = entry->ct;
    return true;
}

void Metadata_Del(char const* name)
{
    dictionary_remove_and_destroy(Metadata, name, Free);
}

void Metadata_Clear(void)
{
    dictionary_clean_and_destroy_elements(Metadata, Free);
}

void Metadata_Destroy(void)
{
    dictionary_destroy_and_destroy_elements(Metadata, Free);
}
