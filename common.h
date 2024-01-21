#pragma once

#define MESSAGE_STORAGE_SIZE 1024

struct Message {
    char name[16];
    unsigned long rd;
    unsigned long rd_sectors;
    unsigned long wr;
    unsigned long wr_sectors;
};