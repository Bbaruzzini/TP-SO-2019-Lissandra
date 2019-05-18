
#ifndef Config_h__
#define Config_h__

#include <libcommons/config.h>
#include <pthread.h>

extern t_config* sConfig;
extern pthread_rwlock_t sConfigLock;

#endif //Config_h__
