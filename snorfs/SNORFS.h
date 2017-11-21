#ifndef CODAL_SNORFS_H
#define CODAL_SNORFS_H

#include "SPIFlash.h"

// each meta row gives you 256 more file entries
#define SNORFS_META_ROWS 2

#define SNORFS_RESERVED_META_PAGES (SPIFLASH_SMALL_ROW_PAGES)

namespace codal
{
namespace snorfs
{

class File;

// Supported flash size: 1-16MB
class FS
{
    friend class File;

    SPIFlash &flash;
    uint8_t buf[SPIFLASH_PAGE_SIZE];

    uint8_t *rowRemapCache;
    uint8_t numDataRows;
    uint8_t metaFree[SNORFS_META_ROWS];

    int firstFree(uint32_t addr, int startOff);

    uint32_t dataRowAddr(uint8_t rowIdx) { return rowRemapCache[rowIdx] * SPIFLASH_BIG_ROW_SIZE; }
    uint32_t dataIndexAddr(uint16_t ptr) { return dataRowAddr(ptr >> 8) + (ptr & 0xff); }
    uint32_t dataNextPtrAddr(uint16_t ptr)
    {
        return dataRowAddr(ptr >> 8) + SPIFLASH_BIG_ROW_SIZE - 2 * SPIFLASH_PAGE_SIZE +
               2 * (ptr & 0xff);
    }
    uint32_t dataDataAddr(uint16_t ptr)
    {
        return dataRowAddr(ptr >> 8) + SPIFLASH_PAGE_SIZE * (ptr & 0xff);
    }

    // last page in meta row is index
    uint32_t metaIdxAddr(int page)
    {
        return (((page >> 8) + 1) * SPIFLASH_BIG_ROW_SIZE - SPIFLASH_PAGE_SIZE) + (page & 0xff);
    }
    uint32_t metaPageAddr(int i, int j)
    {
        return i * SPIFLASH_BIG_ROW_SIZE + j * SPIFLASH_PAGE_SIZE;
    }
    uint32_t getRemapSize()
    {
        return min(SPIFLASH_SMALL_ROW_SIZE / numDataRows, SPIFLASH_PAGE_SIZE);
    }
    uint16_t findFreeDataPage(int startRow);
    void mount();
    uint16_t findMetaEntry(const char *filename);
    uint16_t createMetaPage(const char *filename);
    uint16_t findFreeMetaPage();

public:
    FS(SPIFlash &f);
    ~FS();
    void debugDump();
    // returns NULL if file doesn't exists and create==false
    File *open(const char *filename, bool create = true);
};

class File
{
    // Invariants:
    // firstPage == 0 <==> no pages has been allocated
    // readOffset % SPIFLASH_PAGE_SIZE == 0 && readPage != 0 ==>
    //       readPage is on page for (readOffset - 1)
    // writePage % SPIFLASH_PAGE_SIZE == 0 && writePage != 0 ==>
    //       writePage is on page for (metaSize - 1)
    // if readPage is 0 it needs to be recomputed
    // if writePage is 0 it needs to be recomputed

    friend class FS;

    FS &fs;
    uint32_t metaSize;
    uint16_t firstPage; // row idx : page ID
    // this is for reading
    uint16_t readPage; // row idx : page idx
    uint32_t readOffset;
    // this is for writing (append)
    uint16_t writePage;
    uint16_t metaPage;
    uint8_t metaSizeOff;

    uint32_t metaPageAddr() { return fs.metaPageAddr(metaPage>>8, metaPage&0xff); }

    void readSize();
    void findFirstPage();
    void rewind();
    void seekNextPage();
    void allocatePage();
    void seekToStableAddr(uint16_t nextPtr);
    uint16_t stablePageAddr(uint16_t pageIdx);
    void newMetaPage();
    void findFreeMetaPage();
    void computeWritePage();
    void saveSizeDiff(int32_t sizeDiff);
    void truncateCore();
    void appendCore(const void *data, uint32_t len);
    File(FS &f, uint16_t filePage);
    File(FS &f, const char *filename);

public:
    int read(void *data, uint32_t len);
    void append(const void *data, uint32_t len);
    void seek(uint32_t pos);
    uint32_t size() { return metaSize; }
    uint32_t tell() { return readOffset; }
    uint32_t fileID() { return metaPage; }
    void debugDump();
    bool isDeleted() { return writePage == 0xffff; }
    void overwrite(const void *data, uint32_t len);
    void del();
    void truncate() { overwrite(NULL, 0); }
};
}
}

#endif
