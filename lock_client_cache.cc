// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int lock_client_cache::last_port = 0;

lock_client_cache::lock_client_cache(std::string xdst, 
        class lock_release_user *_lu)
: lock_client(xdst), lu(_lu)
{
    srand(time(NULL)^last_port);
    rlock_port = ((rand()%32000) | (0x1 << 10));
    const char *hname;
    // VERIFY(gethostname(hname, 100) == 0);
    hname = "127.0.0.1";
    std::ostringstream host;
    host << hname << ":" << rlock_port;
    id = host.str();
    last_port = rlock_port;
    rpcs *rlsrpc = new rpcs(rlock_port);
    rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
    rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);
}

    lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
    pthread_mutex_lock(&mutex);

    int r;
    lock_protocol::status ret;
    std::map<lock_protocol::lockid_t, state_t*>::iterator it;
    while (true)
    {
        if (slm.find(lid) == slm.end())
        {
            //initialize state_t
            state_t *astate;
            astate = (state_t*)malloc(sizeof(state_t));
            astate->status = NONE;
            astate->releasing = false;
            astate->busy = 0;
            pthread_cond_init(&astate->cond, NULL);

            slm.insert(std::make_pair(lid, astate));
        }
        it = slm.find(lid);
        if (it->second->status == FREE || (it->second->status == LOCKED && it->second->tid == pthread_self()))
        {
            it->second->status = LOCKED;
            it->second->tid = pthread_self();
            pthread_mutex_unlock(&mutex);
            ret = lock_protocol::OK;
            return ret;
        }
        else if (it->second->status == LOCKED || it->second->status == ACQUIRING)
        {
            it->second->busy++;
            pthread_cond_wait(&it->second->cond, &mutex);
            it = slm.find(lid);
            it->second->busy--;
        }
        else if (it->second->status == NONE)
        {
            it->second->status = ACQUIRING;

            pthread_mutex_unlock(&mutex);

            ret = cl->call(lock_protocol::acquire, lid, id, r);
            if (ret == lock_protocol::OK)
            {
                printf("client %s get lock %llu\n", id.data(), lid);
                it->second->status = LOCKED;
                it->second->tid = pthread_self();
                pthread_mutex_unlock(&mutex);
                return ret;
            } 
            if (ret == lock_protocol::ONEOFF)
            {
                printf("client %s get one-off lock %llu\n", id.data(), lid);
                it->second->status = LOCKED;
                it->second->releasing = true;
                it->second->tid = pthread_self();
                ret = lock_protocol::OK;
                pthread_mutex_unlock(&mutex);
                return ret;
            }
            if (ret == lock_protocol::RETRY)
            {
                pthread_mutex_lock(&mutex);
                it = slm.find(lid);
                if (it->second->status == ACQUIRING)
                {
                    it->second->busy++;
                    pthread_cond_wait(&it->second->cond, &mutex);
                    it = slm.find(lid);
                    it->second->busy--;
                }
            }
        }
    }
}

    lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
    pthread_mutex_lock(&mutex);

    int r;
    lock_protocol::status ret;
    std::map<lock_protocol::lockid_t, state_t*>::iterator it;
    it = slm.find(lid);
    if (it->second->releasing)
    {
        if (it->second->busy)
        {
            lu->dorelease(lid); //flush
            pthread_mutex_unlock(&mutex);

            printf("client %s release lock %llu...", id.data(), lid);
            ret = cl->call(lock_protocol::release, lid, id, r);
            printf("OK\n");

            pthread_mutex_lock(&mutex);
            it = slm.find(lid);
            it->second->status = NONE;
            it->second->releasing = false;
            pthread_mutex_unlock(&mutex);

            pthread_cond_signal(&it->second->cond);
        }
        else
        {
            lu->dorelease(lid); //flush
            pthread_mutex_unlock(&mutex);

            printf("client %s release lock %llu...", id.data(), lid);
            ret = cl->call(lock_protocol::release, lid, id, r);
            printf("OK\n");

            pthread_mutex_lock(&mutex);
            it = slm.find(lid);
            pthread_cond_destroy(&it->second->cond);
            free(it->second);
            slm.erase(it);
            pthread_mutex_unlock(&mutex);
        }
    }
    else
    {
        it->second->status = FREE;
        if (it->second->busy)
        {
            pthread_mutex_unlock(&mutex);
            pthread_cond_signal(&it->second->cond);
        }
        else
        {
            pthread_mutex_unlock(&mutex);
        }
    }
    return ret;
}

    rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
        int &)
{
    pthread_mutex_lock(&mutex);
    std::map<lock_protocol::lockid_t, state_t*>::iterator it;
    it = slm.find(lid);
    it->second->releasing = true;
    if (!it->second->busy && it->second->status == FREE)
    {
        lu->dorelease(lid); //flush
        pthread_cond_destroy(&it->second->cond);
        free(it->second);
        slm.erase(it);
        pthread_mutex_unlock(&mutex);

        printf("client %s release lock %llu OK\n", id.data(), lid);
        return rlock_protocol::DIY;
    }
    pthread_mutex_unlock(&mutex);
    return rlock_protocol::OK;
}

    rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
        int &)
{
    pthread_mutex_lock(&mutex);
    std::map<lock_protocol::lockid_t, state_t*>::iterator it;
    it = slm.find(lid);
    it->second->status = NONE;
    pthread_mutex_unlock(&mutex);
    pthread_cond_signal(&it->second->cond);
    return rlock_protocol::OK;
}

