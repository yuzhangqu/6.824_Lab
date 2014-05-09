// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include "extent_protocol.h"
#include "extent_server.h"
#include "lock_client_cache.h"

class extent_client : public lock_release_user {
    public:
        enum ext_cache_status {
            SHARED, 
            EXCLUSIVE, 
            INVALID
        };
        struct ext_cache {
            int status;
            std::string buf;
            bool hasattr;
            extent_protocol::attr a;

            ext_cache() {
                status = INVALID;
                buf.clear();
                hasattr = false;
                memset(&a, 0, sizeof(a));
            }
        };

    private:
        std::map<extent_protocol::extentid_t, struct ext_cache> extmap;
        std::map<extent_protocol::extentid_t, struct ext_cache>::iterator it;
        rpcc *cl;

    public:
        extent_client(std::string dst);

        extent_protocol::status create(uint32_t type, extent_protocol::extentid_t &eid);
        extent_protocol::status get(extent_protocol::extentid_t eid, 
                std::string &buf);
        extent_protocol::status getattr(extent_protocol::extentid_t eid, 
                extent_protocol::attr &a);
        extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
        extent_protocol::status remove(extent_protocol::extentid_t eid);
        bool findext(extent_protocol::extentid_t eid, bool create = false);
        extent_protocol::status flush(extent_protocol::extentid_t eid);
        void dorelease(extent_protocol::extentid_t eid);

};

#endif 

