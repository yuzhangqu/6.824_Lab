// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include <map>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"

enum xxlock { FREE, LOCKED };

typedef struct mylock {
    mylock (int _own)
    {
        status = FREE;
        owner = _own;
    }
    int status;
    int owner;
} mylock_t;

typedef std::map<lock_protocol::lockid_t, mylock_t> LMAP;

class lock_server {

    protected:
        int nacquire;
        LMAP lockmap;

    public:
        lock_server();
        ~lock_server() {};
        lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
        lock_protocol::status grant(int clt, lock_protocol::lockid_t lid, int &);
        lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
};

#endif 

