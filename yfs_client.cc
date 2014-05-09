// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client(extent_dst);
    lc = new lock_client_cache(lock_dst, ec);
    printf("lock_client_cache init\n");
    lc->acquire(1);
    printf("get lock 1\n");
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
    lc->release(1);
    printf("release lock 1\n");
}

yfs_client::inum

yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

    std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

    bool
yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;

    lc->acquire(inum);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        lc->release(inum);
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        lc->release(inum);
        return true;
    } 
    lc->release(inum);
    return false;
}

    bool
yfs_client::isdir(inum inum)
{
    return ! isfile(inum);
}

    int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    lc->acquire(inum);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    lc->release(inum);
    return r;
}

    int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    lc->acquire(inum);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    lc->release(inum);
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
    int
yfs_client::setattr(inum ino, size_t size)
{
    /*
     * your lab2 code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
    int r = OK;
    std::string tmp;
    lc->acquire(ino);
    if (ec->get(ino, tmp) != extent_protocol::OK)
    {
        r = IOERR;
        goto release;
    }
    tmp.resize(size, '\0');
    if (ec->put(ino, tmp) != extent_protocol::OK)
    {
        r = IOERR;
        goto release;
    }

release:
    lc->release(ino);
    return r;
}

    int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out, uint32_t type)
{
    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    int r = OK;
    bool found = false;
    std::string buf;
    struct dirent *aEnt;
    struct dirent tmpEnt;
    extent_protocol::types t;
    if (type == 1)
    {
        t = extent_protocol::T_DIR;
    }
    else if (type == 2)
    {
        t = extent_protocol::T_FILE;
    }

    lc->acquire(parent);
    if (ec->get(parent, buf) != extent_protocol::OK)
    {
        r = IOERR;
        goto release;
    }
    aEnt = (struct dirent*)(buf.data());
    for(size_t i = 0; i < buf.size() / sizeof(dirent); i++)
    {
        if (strcmp(aEnt[i].name, name) == 0)
        {
            found = true;
        }
    }
    if (found)
    {
        r = yfs_client::EXIST;
        goto release;
    }

    if (ec->create(t, ino_out) != extent_protocol::OK)
    {
        r = IOERR;
        goto release;
    }
    strcpy(tmpEnt.name, name);
    tmpEnt.inum = ino_out;
    buf.append((char *)(&tmpEnt), sizeof(dirent));
    if (ec->put(parent, buf) != extent_protocol::OK)
    {
        r = IOERR;
        goto release;
    }

release:
    lc->release(parent);
    return r;
}

    int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;
    /*
     * your lab2 code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    std::list<dirent> list;
    readdir(parent, list);
    for (std::list<dirent>::iterator it = list.begin(); it != list.end(); it++)
    {
        if (strcmp(it->name, name) == 0)
        {
            found = true;
            ino_out = it->inum;
        }
    }
    return r;
}

    int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    /*
     * your lab2 code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
    int r = OK;
    std::string buf;
    struct dirent *aEnt;
    lc->acquire(dir);
    if (ec->get(dir, buf) != extent_protocol::OK)
    {
        r = IOERR;
        goto release;
    }
    aEnt = (struct dirent*)(buf.data());
    for(size_t i = 0; i < buf.size() / sizeof(dirent); i++)
    {
        list.push_back(aEnt[i]);
    }

release:
    lc->release(dir);
    return r;
}

    int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    /*
     * your lab2 code goes here.
     * note: read using ec->get().
     */
    int r = OK;
    std::string tmp;
    lc->acquire(ino);
    if (ec->get(ino, tmp) != extent_protocol::OK)
    {
        r = IOERR;
        goto release;
    }
    char buf[size];
    if (off + size > tmp.size())
    {
        tmp.resize(off + size, '\0');
    }
    memcpy(buf, tmp.data() + off, size);
    data.assign(buf, sizeof(buf));

release:
    lc->release(ino);
    return r;
}

    int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    /*
     * your lab2 code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
    int r = OK;
    std::string tmp;
    lc->acquire(ino);
    if (ec->get(ino, tmp) != extent_protocol::OK)
    {
        r = IOERR;
        goto release;
    }
    if (off + size > tmp.size())
    {
        tmp.resize(off + size, '\0');
    }
    tmp.replace(off, size, data, size);
    if (ec->put(ino, tmp) != extent_protocol::OK)
    {
        r = IOERR;
        goto release;
    }
    bytes_written = size;//TODO:Don't know the meaning of bytes_written

release:
    lc->release(ino);
    return r;
}

int yfs_client::unlink(inum parent,const char *name)
{
    /*
     * your lab2 code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    int r = OK;
    bool found = false;
    inum tmpno = 0;
    std::string oldbuf;
    std::string newbuf;
    struct dirent *aEnt;

    lc->acquire(parent);
    if (ec->get(parent, oldbuf) != extent_protocol::OK)
    {
        r = IOERR;
        goto release;
    }
    aEnt = (struct dirent*)(oldbuf.data());
    for(size_t i = 0; i < oldbuf.size() / sizeof(dirent); i++)
    {
        if (strcmp(aEnt[i].name, name) != 0)
        {
            newbuf.append((char*)(aEnt + i), sizeof(dirent));
        }
        else
        {
            tmpno = aEnt[i].inum;
            found = true;
            if (isdir(tmpno))
            {
                r = IOERR;
                goto release;
            }
        }
    }
    if (!found)
    {
        r = NOENT;
        goto release;
    }
    ec->put(parent, newbuf);
    ec->remove(tmpno);

release:
    lc->release(parent);
    return r;
}

