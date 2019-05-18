
#include "Config.h"
#include <stdlib.h>

t_config* sConfig = NULL;
pthread_rwlock_t sConfigLock = PTHREAD_RWLOCK_INITIALIZER;
