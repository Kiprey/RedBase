#include "rm_rid.h"
#include "rm.h"

RID::RID() : isValid_(FALSE) {}

RID::RID(PageNum pageNum, SlotNum slotNum) 
    : isValid_(TRUE), pageNum_(pageNum), slotNum_(slotNum) {}

RID::~RID() {
    // do nothing
}

RC RID::GetPageNum(PageNum &pageNum) const {
    if(!isValid_)
        return RM_RID_INVALID;
    pageNum = pageNum_;
    return OK_RC;
}

RC RID::GetSlotNum(SlotNum &slotNum) const {
    if(!isValid_)
        return RM_RID_INVALID;
    slotNum = slotNum_;
    return OK_RC;
}