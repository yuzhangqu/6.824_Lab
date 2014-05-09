// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

lock_server::lock_server():
    nacquire (0)
{
}

    lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
    lock_protocol::status ret = lock_protocol::OK;
    printf("stat request from clt %d\n", clt);
    r = nacquire;
    return ret;
}

    lock_protocol::status
lock_server::grant(int clt, lock_protocol::lockid_t lid, int &r)
{
    pthread_mutex_lock(&mutex);
    while (true)
    {
        if (lockmap.find(lid) == lockmap.end())
        {
            mylock_t alock(clt);
            lockmap.insert(std::make_pair(lid, alock));
        }
        LMAP::iterator it = lockmap.find(lid);
        if (it != lockmap.end() && it->second.status == FREE)
        {
            it->second.status = LOCKED;
            it->second.owner = clt;
            pthread_mutex_unlock(&mutex);
            return lock_protocol::OK;
        }
        pthread_cond_wait(&cond, &mutex);
    }
}

    lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
    pthread_mutex_lock(&mutex);
    LMAP::iterator it = lockmap.find(lid);
    if (it != lockmap.end() && it->second.status == LOCKED && it->second.owner == clt)
    {
        it->second.status = FREE;
        it->second.owner = clt;
    }
    pthread_mutex_unlock(&mutex);
    pthread_cond_broadcast(&cond);
    return lock_protocol::OK;
}

