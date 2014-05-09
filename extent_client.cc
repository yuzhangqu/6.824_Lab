// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

extent_client::extent_client(std::string dst)
{
    sockaddr_in dstsock;
    make_sockaddr(dst.c_str(), &dstsock);
    cl = new rpcc(dstsock);
    if (cl->bind() != 0) {
        printf("extent_client: bind failed\n");
    }
}

// a demo to show how to use RPC
    extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
        extent_protocol::attr &attr)
{
    pthread_mutex_lock(&mutex);
    extent_protocol::status ret = extent_protocol::OK;
    if (!findext(eid, true) || !it->second.hasattr) {
        ret = cl->call(extent_protocol::getattr, eid, attr);
        memcpy(&it->second.a, &attr, sizeof(attr));
        it->second.hasattr = true;
    } else {
        memcpy(&attr, &it->second.a, sizeof(it->second.a));
    }
    pthread_mutex_unlock(&mutex);
    return ret;
}

    extent_protocol::status
extent_client::create(uint32_t type, extent_protocol::extentid_t &id)
{
    extent_protocol::status ret = extent_protocol::OK;
    // Your lab3 code goes here
    ret = cl->call(extent_protocol::create, type, id);
    return ret;
}

    extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
    pthread_mutex_lock(&mutex);
    extent_protocol::status ret = extent_protocol::OK;
    // Your lab3 code goes here
    if (!findext(eid, true) || it->second.status == INVALID) {
        ret = cl->call(extent_protocol::get, eid, buf);
        printf("\nGet fresh content of %llu: ", eid);
        it->second.buf = buf;
        it->second.a.size = buf.size();
        it->second.status = SHARED;
    } else {
        buf = it->second.buf;
    }
    it->second.a.atime = time(NULL);
    pthread_mutex_unlock(&mutex);
    return ret;
}

    extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
    pthread_mutex_lock(&mutex);
    extent_protocol::status ret = extent_protocol::OK;
    // Your lab3 code goes here
    findext(eid, true);
    it->second.buf = buf;
    it->second.status = EXCLUSIVE;
    it->second.a.size = buf.size();
    it->second.a.mtime = time(NULL);
    it->second.a.ctime = time(NULL);
    pthread_mutex_unlock(&mutex);
    return ret;
}

    extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
    pthread_mutex_lock(&mutex);
    extent_protocol::status ret = extent_protocol::OK;
    // Your lab3 code goes here
    int r;
    if (findext(eid)) {
        extmap.erase(it);
    }
    ret = cl->call(extent_protocol::remove, eid, r);
    pthread_mutex_unlock(&mutex);
    return ret;
}

/*
 * Return true and iterator will point to end 
 * if the given eid is in the cache.
 * The default value of create is false.
 * If the given eid is not in the cache and create
 * is true, a cache line of eid will be create and 
 * iterator will point to it.
 */
    bool
extent_client::findext(extent_protocol::extentid_t eid, bool create)
{
    bool ret = true;
    it = extmap.find(eid);
    if (it == extmap.end()) {
        ret = false;
        if (create) {
            extmap.insert(std::make_pair(eid, ext_cache()));
            it = extmap.find(eid);
        }
    }
    return ret;
}

/*
 * Synchronize the content of cache with the disk 
 * and remove the corresponding cache line.
 */
    void
extent_client::dorelease(extent_protocol::extentid_t eid)
{
    pthread_mutex_lock(&mutex);
    if (findext(eid)) {
        int r;
        switch (it->second.status) {
            case EXCLUSIVE:
                cl->call(extent_protocol::put, eid, it->second.buf, r);
                printf("Flush content of %llu back: ", eid);
                break;
            default:
                break;
        }
        extmap.erase(it);
    }
    pthread_mutex_unlock(&mutex);
}
