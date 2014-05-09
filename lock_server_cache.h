#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>
#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"

typedef struct mylock_cache
{
    mylock_cache (std::string _own)
    {
        status = FREE;
        owner = _own;
    }
    int status;
    std::string owner;
    std::list<std::string> waitlist;
} mylock_cache_t;

typedef std::map<lock_protocol::lockid_t, mylock_cache_t> LMAP_C;

class lock_server_cache {
    private:
        int nacquire;
        LMAP_C lockmapcache;

    public:
        lock_server_cache();
        ~lock_server_cache() {};
        lock_protocol::status stat(lock_protocol::lockid_t, int &);
        int acquire(lock_protocol::lockid_t, std::string id, int &);
        int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
