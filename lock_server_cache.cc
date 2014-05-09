// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

lock_server_cache::lock_server_cache()
{
}

int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
        int &)
{
    pthread_mutex_lock(&mutex);
    int r;
    if (lockmapcache.find(lid) == lockmapcache.end())
    {
        mylock_cache_t alock(id);
        lockmapcache.insert(std::make_pair(lid, alock));
    }
    LMAP_C::iterator it = lockmapcache.find(lid);
    if (it->second.status == FREE && it->second.owner.compare(id) == 0)
    {
        it->second.status = LOCKED;
        it->second.owner = id;
        if (it->second.waitlist.size() == 0)
        {
            pthread_mutex_unlock(&mutex);
            return lock_protocol::OK;
        }
        else
        {
            pthread_mutex_unlock(&mutex);
            return lock_protocol::ONEOFF;
        }
    }
    else
    {
        bool isin = false;
        for (std::list<std::string>::iterator it2 = it->second.waitlist.begin(); it2 != it->second.waitlist.end(); it2++)
        {
            if (it2->compare(id) == 0)
            {
                isin = true;
            }
        }
        if (!isin)
        {
            it->second.waitlist.push_back(id);
        }
        if (it->second.waitlist.size() == 1)
        {
            handle h(it->second.owner);
            rpcc *sv = h.safebind();
            if (sv)
            {
                pthread_mutex_unlock(&mutex);
                rlock_protocol::status ret = sv->call(rlock_protocol::revoke, lid, r);
                if (ret == rlock_protocol::DIY) //Client told server to dispatch lock to other client.
                {
                    pthread_mutex_lock(&mutex);
                    it = lockmapcache.find(lid);
                    if (it->second.waitlist.front().compare(id) == 0)
                    {
                        it->second.status = LOCKED;
                        it->second.owner = id;
                        it->second.waitlist.pop_front();
                        if (it->second.waitlist.size() == 0)
                        {
                            pthread_mutex_unlock(&mutex);
                            return lock_protocol::OK;
                        }
                        else
                        {
                            pthread_mutex_unlock(&mutex);
                            return lock_protocol::ONEOFF;
                        }
                    }
                }
            }
        }
        pthread_mutex_unlock(&mutex);
        return lock_protocol::RETRY;
    }
}

    int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
        int &)
{
    pthread_mutex_lock(&mutex);
    int r;
    LMAP_C::iterator it = lockmapcache.find(lid);
    if (it != lockmapcache.end() && it->second.status == LOCKED && it->second.owner.compare(id) == 0)
    {
        it->second.status = FREE;
        if (!it->second.waitlist.empty())
        {
            it->second.owner = it->second.waitlist.front(); //give the lock to the first one in list.
            it->second.waitlist.pop_front();
            handle h(it->second.owner);
            rpcc *sv = h.safebind();
            if (sv)
            {
                pthread_mutex_unlock(&mutex);
                sv->call(rlock_protocol::retry, lid, r);
            }
        }
    }
    return lock_protocol::OK;
}

    lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
    tprintf("stat request\n");
    r = nacquire;
    return lock_protocol::OK;
}

