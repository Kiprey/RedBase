#ifndef RM_INTERNAL
#define RM_INTERNAL

#include "rm.h"

#define RM_NO_FREE_PAGE     -1
#define RM_PAGE_FULL_USED   -2

struct RM_PageHdr {
    int nextFreePage;   // 指向下一个空闲记录的页面索引
};

#endif