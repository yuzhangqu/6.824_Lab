// lock protocol

#ifndef lock_protocol_h
#define lock_protocol_h

#include "rpc.h"

class lock_protocol {
    public:
        enum xxstatus { OK, RETRY, RPCERR, NOENT, IOERR, ONEOFF }; //add ONEOFF to tell the client use lock only once.
        typedef int status;
        typedef unsigned long long lockid_t;
        enum rpc_numbers {
            acquire = 0x7001,
            release,
            stat
        };
};

class rlock_protocol {
    public:
        enum xxstatus { OK, RPCERR, DIY }; //add DIY to tell the server release lock by itself.
        typedef int status;
        enum rpc_numbers {
            revoke = 0x8001,
            retry = 0x8002
        };
};

#endif 