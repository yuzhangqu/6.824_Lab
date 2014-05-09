#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk()
{
    bzero(blocks, sizeof(blocks));
}

    void
disk::read_block(blockid_t id, char *buf)
{
    if (id < 0 || id >= BLOCK_NUM || buf == NULL)
        return;

    memcpy(buf, blocks[id], BLOCK_SIZE);
}

    void
disk::write_block(blockid_t id, const char *buf)
{
    if (id < 0 || id >= BLOCK_NUM || buf == NULL)
        return;

    memcpy(blocks[id], buf, BLOCK_SIZE);
}

    void
disk::clear_block(blockid_t id)
{
    bzero(blocks[id], BLOCK_SIZE);
}

    void
disk::set_bitmap(blockid_t id)
{
    uint32_t charOrder = (id + 1) / 8 + ((id + 1) % 8 == 0 ? 0 : 1);
    blocks[charOrder / BLOCK_SIZE + (charOrder % BLOCK_SIZE == 0 ? 0 : 1)][(charOrder - 1) % BLOCK_SIZE] |= (char)(1 << (7 - (id % 8)));
    return;
}

    void
disk::reset_bitmap(blockid_t id)
{
    uint32_t charOrder = (id + 1) / 8 + ((id + 1) % 8 == 0 ? 0 : 1);
    blocks[charOrder / BLOCK_SIZE + (charOrder % BLOCK_SIZE == 0 ? 0 : 1)][(charOrder - 1) % BLOCK_SIZE] &= ~(char)(1 << (7 - (id % 8)));
    return;
}
// block layer -----------------------------------------

static pthread_mutex_t imutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t ibmutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t bmutex = PTHREAD_MUTEX_INITIALIZER;

// Allocate a free disk block.
    blockid_t
block_manager::alloc_block()
{
    /*
     * your lab1 code goes here.
     * note: you should mark the corresponding bit in block bitmap when alloc.
     * you need to think about which block you can start to be allocated.
     */
    pthread_mutex_lock(&bmutex);
    for (blockid_t i = IBLOCK(INODE_NUM, BLOCK_NUM) + 1; i < BLOCK_NUM; i++)
    {
        if (using_blocks.count(i) == 0 || using_blocks[i] == 0)
        {
            d->clear_block(i);
            d->set_bitmap(i);
            using_blocks[i] = 1;
            pthread_mutex_unlock(&bmutex);
            return i;
        }
    }
    printf("%s\n", "ERR: No Enough Block for this file!");
    pthread_mutex_unlock(&bmutex);
    return 0;
}

    void
block_manager::free_block(uint32_t id)
{
    /* 
     * your lab1 code goes here.
     * note: you should unmark the corresponding bit in the block bitmap when free.
     */
    d->reset_bitmap(id);
    using_blocks[id] = 0;
    return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
    d = new disk();

    // format the disk
    sb.size = BLOCK_SIZE * BLOCK_NUM; //Equal to disc_size.
    sb.nblocks = BLOCK_NUM;
    sb.ninodes = INODE_NUM;

}

    void
block_manager::read_block(uint32_t id, char *buf)
{
    d->read_block(id, buf);
}

    void
block_manager::write_block(uint32_t id, const char *buf)
{
    d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
    bm = new block_manager();
    uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
    if (root_dir != 1) {
        printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
        exit(0);
    }
}

/* Create a new file.
 * Return its inum. */
    uint32_t
inode_manager::alloc_inode(uint32_t type)
{
    /* 
     * your lab1 code goes here.
     * note: the normal inode block should begin from the 2nd inode block.
     * the 1st is used for root_dir, see inode_manager::inode_manager().
     */
    pthread_mutex_lock(&imutex);
    for (uint32_t i = 1; i < (bm->sb.ninodes + 1); i++)
    {
        if (using_inodes.count(i) == 0 || using_inodes[i] == 0)
        {
            struct inode newNode;
            memset(&newNode, 0, sizeof(newNode));
            newNode.type = type;
            newNode.size = 0;
            put_inode(i, &newNode);
            using_inodes[i] = 1;
            pthread_mutex_unlock(&imutex);
            return i;
        }
    }
    pthread_mutex_unlock(&imutex);
    return 0;
}

    void
inode_manager::free_inode(uint32_t inum)
{
    /* 
     * your lab1 code goes here.
     * note: you need to check if the inode is already a freed one;
     * if not, clear it, and remember to write back to disk.
     */
    pthread_mutex_lock(&imutex);
    struct inode *curNode;
    curNode = get_inode(inum);
    if (curNode == NULL)
    {
        printf("ERR: Try to free Non-existing File.");
        return;
    }
    memset(curNode, 0, sizeof(*curNode));
    put_inode(inum, curNode);
    free(curNode);
    using_inodes[inum] = 0;
    pthread_mutex_unlock(&imutex);
    return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
    struct inode* 
inode_manager::get_inode(uint32_t inum)
{
    struct inode *ino, *ino_disk;
    char buf[BLOCK_SIZE];

    printf("\tim: get_inode %d\n", inum);

    if (inum < 0 || inum >= INODE_NUM) {
        printf("\tim: inum out of range\n");
        return NULL;
    }

    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
    // printf("%s:%d\n", __FILE__, __LINE__);

    ino_disk = (struct inode*)buf + inum%IPB;
    if (ino_disk->type == 0) {
        printf("\tim: inode not exist\n");
        return NULL;
    }

    ino = (struct inode*)malloc(sizeof(struct inode));
    *ino = *ino_disk;

    return ino;
}

    void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
    char buf[BLOCK_SIZE];
    struct inode *ino_disk;

    printf("\tim: put_inode %d\n", inum);
    if (ino == NULL)
        return;

    pthread_mutex_lock(&ibmutex);
    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
    ino_disk = (struct inode*)buf + inum%IPB;
    *ino_disk = *ino;
    bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
    pthread_mutex_unlock(&ibmutex);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))
#define MAX(a,b) ((a)>(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
    void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
    /*
     * your lab1 code goes here.
     * note: read blocks related to inode number inum,
     * and copy them to buf_Out
     */
    struct inode *curNode;
    curNode = get_inode(inum);
    if (curNode == NULL || curNode->size == 0)
    {
        free(curNode);
        return;
    }

    *buf_out = (char*)malloc((curNode->size / BLOCK_SIZE +  (curNode->size % BLOCK_SIZE == 0 ? 0 : 1)) * BLOCK_SIZE);
    for (unsigned int i = 0; curNode->blocks[i] != 0 && i < NDIRECT; i++)//Read direct part.
    {
        bm->read_block(curNode->blocks[i], (*buf_out) + i * BLOCK_SIZE);
    }
    if (curNode->blocks[NDIRECT] != 0)//Have indirect part.
    {
        blockid_t *ptNB;
        char tmpBuf[BLOCK_SIZE];
        bm->read_block(curNode->blocks[NDIRECT], tmpBuf);
        ptNB = (blockid_t*)tmpBuf;
        for (unsigned int i = 0; i < NINDIRECT && curNode->size > (i + NDIRECT) * BLOCK_SIZE; i++)//Read indirect part.
        {
            bm->read_block(ptNB[i], (*buf_out) + (i + NDIRECT) * BLOCK_SIZE);
        }
    }
    *size = curNode->size;
    curNode->atime = time(NULL);
    put_inode(inum, curNode);
    free(curNode);
    return;
}

    void
inode_manager::alloc_block_fail(uint32_t inum, struct inode *ino)
{
    printf("%s\n", "Alloc Block Fail! Start to recover inode!");

    for (blockid_t i = ino->size / BLOCK_SIZE; i < NDIRECT; i++)//直接映射部分还原
    {
        if (ino->blocks[i] == 0)
        {
            put_inode(inum, ino);
            free(ino);
            return;
        }
        bm->free_block(ino->blocks[i]);//TODO:这两个步骤如果换过来更好，但是先就这样吧。
        ino->blocks[i] = 0;
    }
    if (ino->blocks[NDIRECT] != 0)//间接映射部分还原
    {
        char buf[BLOCK_SIZE];
        bm->read_block(ino->blocks[NDIRECT], buf);
        blockid_t *nbid;
        nbid = (blockid_t*)buf;

        for (blockid_t i = MAX(ino->size / BLOCK_SIZE, NDIRECT); i < NINDIRECT; i++)
        {
            if (nbid[i - NDIRECT] == 0)
            {
                if (ino->size <= NDIRECT * BLOCK_SIZE)
                {
                    bm->free_block(ino->blocks[NDIRECT]);
                    ino->blocks[NDIRECT] = 0;
                }
                put_inode(inum, ino);
                free(ino);
                return;
            }
            bm->free_block(nbid[i - NDIRECT]);
        }
        if (ino->size <= NDIRECT * BLOCK_SIZE)
        {
            bm->free_block(ino->blocks[NDIRECT]);
            ino->blocks[NDIRECT] = 0;
        }
        put_inode(inum, ino);
        free(ino);
    }
    return;
}

/* alloc/free blocks if needed */
    void
inode_manager::write_file(uint32_t inum, const char *buf, unsigned int size)
{
    /*
     * your lab1 code goes here.
     * note: write buf to blocks of inode inum.
     * you need to consider the situation when the size of buf 
     * is larger or smaller than the size of original inode
     */
    if (size > MAXFILE * BLOCK_SIZE)
    {
        printf("%s\n", "This file is too large to store in YFS");
        return;
    }

    struct inode *curNode;
    curNode = get_inode(inum);//free curNode later
    if (curNode == NULL)
    {
        printf("%s\n", "ERR: File Not Exists!");
        return;
    }
    blockid_t tmp;//store allocated block id;
    blockid_t i;

    if (size > curNode->size)//Need to alloc block for larger file.
    {
        if (curNode->size <= NDIRECT * BLOCK_SIZE)//原文件没有用间接映射
        {
            for (i = curNode->size / BLOCK_SIZE; size > i * BLOCK_SIZE && i < NDIRECT; i++)//分配直接映射部分
            {
                tmp = bm->alloc_block();
                if (tmp == 0)
                {
                    alloc_block_fail(inum, curNode);
                    return;
                }
                curNode->blocks[i] = tmp;
            }
            if (size > i * BLOCK_SIZE)//分配间接映射部分
            {
                tmp = bm->alloc_block();
                if (tmp == 0)
                {
                    alloc_block_fail(inum, curNode);
                    return;
                } 
                curNode->blocks[NDIRECT] = tmp;

                blockid_t nbid[NINDIRECT];
                for (; size > i * BLOCK_SIZE; i++)
                {
                    tmp = bm->alloc_block();
                    if (tmp == 0)
                    {
                        alloc_block_fail(inum, curNode);
                        return;
                    }
                    nbid[i - NDIRECT] = tmp;
                }
                bm->write_block(curNode->blocks[NDIRECT], (char*)nbid);
            }
        }
        else//原文件使用了间接映射
        {
            blockid_t *ptrNB;
            char tmpBuf[BLOCK_SIZE];
            bm->read_block(curNode->blocks[NDIRECT], tmpBuf);
            ptrNB = (blockid_t*)tmpBuf;
            for (i = curNode->size / BLOCK_SIZE; size > i * BLOCK_SIZE; i++)//分配间接映射部分
            {
                tmp = bm->alloc_block();
                if (tmp == 0)
                {
                    alloc_block_fail(inum, curNode);
                    return;
                }
                ptrNB[i - NDIRECT] = tmp;
            }
            bm->write_block(curNode->blocks[NDIRECT], tmpBuf);
        }
    }
    else if (curNode->size / BLOCK_SIZE + (curNode->size % BLOCK_SIZE == 0 ? 0 : 1) >= size / BLOCK_SIZE + (size % BLOCK_SIZE == 0 ? 0 : 1))//Need to free block for smaller file
    {
        if (curNode->size > NDIRECT * BLOCK_SIZE)//原文件使用了间接映射
        {
            blockid_t *ptrNB;
            char tmpBuf[BLOCK_SIZE];
            bm->read_block(curNode->blocks[NDIRECT], tmpBuf);
            ptrNB = (blockid_t*)tmpBuf;

            for (i = curNode->size / BLOCK_SIZE - NDIRECT - 1; i > (size / BLOCK_SIZE + (size % BLOCK_SIZE == 0 ? 0 : 1) - NDIRECT - 1) && i >= 0; i--)
            {
                bm->free_block(ptrNB[i]);
                ptrNB[i] = 0;
            }

            if (i < 0)
            {
                for (i = NDIRECT; i > (size / BLOCK_SIZE + (size % BLOCK_SIZE == 0 ? 0 : 1) - 1); i--)
                {
                    bm->free_block(curNode->blocks[i]);
                    curNode->blocks[i] = 0;
                }
            }
            else
            {
                bm->write_block(curNode->blocks[NDIRECT], tmpBuf);
            }
        }
        else//Original file didn't use indirect.
        {
            for (int j = NDIRECT - 1; j > (int)(size / BLOCK_SIZE + (size % BLOCK_SIZE == 0 ? 0 : 1) - 1); j--)
            {
                bm->free_block(curNode->blocks[j]);
                curNode->blocks[j] = 0;
            }
        }
    }// Alloc or Free Blocks Finished.

    curNode->size = size;//Update size.
    curNode->mtime = time(NULL);
    curNode->ctime = time(NULL);

    put_inode(inum, curNode);//Save matadata.

    char aBlockContent[BLOCK_SIZE];
    for (i = 0; curNode->blocks[i] != 0 && i < NDIRECT; i++)//Write direct part.
    {
        memcpy(aBlockContent, buf + i * BLOCK_SIZE, (size < (i + 1) * BLOCK_SIZE ? size - i * BLOCK_SIZE : BLOCK_SIZE));
        bm->write_block(curNode->blocks[i], aBlockContent);
    }

    if (curNode->blocks[NDIRECT] != 0)//write indirect part.
    {
        blockid_t *allocatedId;
        char indirBlock[BLOCK_SIZE];
        bm->read_block(curNode->blocks[NDIRECT], indirBlock);
        allocatedId = (blockid_t*)indirBlock;
        for (i = 0; i < NINDIRECT && size > (i + NDIRECT) * BLOCK_SIZE; i++)
        {
            memcpy(aBlockContent, buf + (i + NDIRECT) * BLOCK_SIZE, (size < (i + NDIRECT + 1) * BLOCK_SIZE ? size - (i + NDIRECT) * BLOCK_SIZE : BLOCK_SIZE));
            bm->write_block(allocatedId[i], aBlockContent);
        }
    }// Write Content Finished.
    free(curNode);
    return;
}

    void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
    /*
     * your lab1 code goes here.
     * note: get the attributes of inode inum.
     * you can refer to "struct attr" in extent_protocol.h
     */
    struct inode *ino_disk;
    char buf[BLOCK_SIZE];
    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
    ino_disk = (struct inode*)buf + inum%IPB;
    if (ino_disk->type != 0)
    {
        a.type = ino_disk->type;
        a.size = ino_disk->size;
        a.atime = ino_disk->atime;
        a.mtime = ino_disk->mtime;
        a.ctime = ino_disk->ctime;
    }
    return;
}

    void
inode_manager::remove_file(uint32_t inum)
{
    /*
     * your lab1 code goes here
     * note: you need to consider about both the data block and inode of the file
     */
    struct inode *curNode;
    curNode = get_inode(inum);
    if (curNode == NULL)
    {
        printf("ERR: File Not Exist!");
        return;
    }
    for (unsigned int i = 0; i < NDIRECT && curNode->blocks[i] != 0; i++)//free direct part data
    {
        bm->free_block(curNode->blocks[i]);
    }
    if (curNode->blocks[NDIRECT] != 0)
    {
        blockid_t *ptNB;
        char tmpBuf[BLOCK_SIZE];
        bm->read_block(curNode->blocks[NDIRECT], tmpBuf);
        ptNB = (blockid_t*)tmpBuf;
        for (unsigned int i = 0; i < NINDIRECT && curNode->size > (i + NDIRECT) * BLOCK_SIZE; i++)
        {
            bm->free_block(ptNB[i]);
        }
        bm->free_block(curNode->blocks[NDIRECT]);
    }
    free(curNode);
    free_inode(inum);
}

