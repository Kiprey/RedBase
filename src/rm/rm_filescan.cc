#include "rm.h"
#include "rm_internal.h"

#include <cstdlib>

RM_FileScan::RM_FileScan () : isOpened_(FALSE) {}
RM_FileScan::~RM_FileScan () {}

RC RM_FileScan::OpenScan  (const RM_FileHandle &fileHandle,
                            AttrType   attrType,
                            int        attrLength,
                            int        attrOffset,
                            CompOp     compOp,
                            void       *value,
                            ClientHint pinHint) {
    if(isOpened_)
        return RM_SCAN_ALREADY_OPENED;
    if(!fileHandle.isOpened_)
        return RM_FILE_NOT_OPENED;

    // 检测 attrType & compOP 是否超出了可选范围
    if(attrType != INT && attrType != FLOAT && attrType != STRING)
        return RM_BAD_ATTRTYPE;
    if(compOp != NO_OP && compOp != EQ_OP && compOp != NE_OP && compOp != LT_OP 
        && compOp != GT_OP && compOp != LE_OP && compOp != GE_OP)
        return RM_BAD_COMPOP;
    // 检测 attrOffset 是否超出了范围
    if(attrOffset < 0 || attrOffset >= fileHandle.fHdr_.recordSize)
        return RM_ATTROFFSET_OUT_OF_RANGE;
    // 检测 attrLength & attrOffset 与 attrType 是否配对
    if(attrType == INT || attrType == FLOAT) {
        if(attrLength != 4)
           return RM_INCONSISTENT_ATTR;
        else if(attrOffset + attrLength >= fileHandle.fHdr_.recordSize)
            return RM_ATTROFFSET_OUT_OF_RANGE;
    }
    else if (attrType == STRING &&
        (attrLength < 1 || attrLength > MAXSTRINGLEN || attrOffset + attrLength >= fileHandle.fHdr_.recordSize))
        return RM_ATTRLENGTH_OUT_OF_RANGE;
    // 判断 value
    if(compOp != NO_OP && !value)
        return RM_NULL_VALUE;

    isOpened_ = TRUE;
    rmFH_ = &fileHandle;
    // 设置初始时的 PageNum
    int rc;
    PF_PageHandle pfPH;
    // 首先获取 head Page
    if((rc = rmFH_->pfFH_.GetFirstPage(pfPH)) 
        || (rc = pfPH.GetPageNum(curPageNum_)) 
        || (rc = rmFH_->pfFH_.UnpinPage(curPageNum_)))
        return rc;
    // 接着获取 first data page
    if((rc = rmFH_->pfFH_.GetNextPage(curPageNum_, pfPH)) 
        || (rc = pfPH.GetPageNum(curPageNum_)) 
        || (rc = rmFH_->pfFH_.UnpinPage(curPageNum_)))
        return rc;

    nextSlotNum_ = 0;

    attrType_ = attrType;
    attrLength_ = attrLength;
    attrOffset_ = attrOffset;
    compOp_ = compOp;
    pinHint_ = pinHint;  

    value_ = getValueFromPtr(value);

    return OK_RC;
}

RC RM_FileScan::GetNextRec(RM_Record &rec) {
    // TODO pinHint_
    if(!isOpened_)
        return RM_SCAN_NOT_OPENED;

    int rc;
    char *pData;
    PF_PageHandle pfPH;
    Boolean isFound = FALSE;

    // 如果说当前 slotNum 没问题，则获取当前页面
    if(nextSlotNum_ < rmFH_->fHdr_.numRecordsPerPage) {
        if((rc = rmFH_->pfFH_.GetThisPage(curPageNum_, pfPH)))
            return rc;
    }

    while(!isFound) {
        // 如果 next slot 超过了，则获取下一个页面
        if(nextSlotNum_ >= rmFH_->fHdr_.numRecordsPerPage) {
            // 获取下一个页面
            if((rc = rmFH_->pfFH_.GetNextPage(curPageNum_, pfPH))) {
                if (rc == PF_EOF)
                    return RM_EOF;
                return rc;
            }
            // 更新 curPageNum_ & nextSlotNum_
            if((rc = pfPH.GetPageNum(curPageNum_)))
                return rc;
            nextSlotNum_ = 0;
        }
        
        // 获取当前页面的 bitmap 
        if((rc = pfPH.GetData(pData)))
            return rc;
        char* bitmap = pData + sizeof(RM_PageHdr);

        // 在单个页面中进行查找
        void* recordP = NULL;
        for(/* nop */; !isFound && nextSlotNum_ < rmFH_->fHdr_.numRecordsPerPage; nextSlotNum_++) {
            // 如果存在记录
            if(bitmap[nextSlotNum_ / 8] & (1 << nextSlotNum_ % 8)) {
                recordP = bitmap + (rmFH_->fHdr_.numRecordsPerPage / 8) + nextSlotNum_ * rmFH_->fHdr_.recordSize;
                if(compOp_ == NO_OP)
                    isFound = TRUE;
                else {
                    // 判断该记录是否满足要求
                    ValueTy uValue = getValueFromPtr((char*)recordP + attrOffset_);
                    switch(attrType_) {
                        case INT: 
                            switch(compOp_) {
                                case EQ_OP: isFound = (uValue.intNum == value_.intNum); break;
                                case LT_OP: isFound = (uValue.intNum < value_.intNum); break;
                                case GT_OP: isFound = (uValue.intNum > value_.intNum); break;
                                case LE_OP: isFound = (uValue.intNum <= value_.intNum); break;
                                case GE_OP: isFound = (uValue.intNum >= value_.intNum); break;
                                case NE_OP: isFound = (uValue.intNum != value_.intNum); break;
                                default: abort();
                            }
                            break;
                        case FLOAT:{
                            switch(compOp_) {
                                case EQ_OP: isFound = (uValue.floatNum == value_.floatNum); break;
                                case LT_OP: isFound = (uValue.floatNum < value_.floatNum); break;
                                case GT_OP: isFound = (uValue.floatNum > value_.floatNum); break;
                                case LE_OP: isFound = (uValue.floatNum <= value_.floatNum); break;
                                case GE_OP: isFound = (uValue.floatNum >= value_.floatNum); break;
                                case NE_OP: isFound = (uValue.floatNum != value_.floatNum); break;
                                default: abort();
                            }
                            break;
                        }
                        case STRING:{
                            int res = strncmp(uValue.str, value_.str, attrLength_);
                            switch(compOp_) {
                                case EQ_OP: isFound = res == 0; break;
                                case LT_OP: isFound = res < 0; break;
                                case GT_OP: isFound = res > 0; break;
                                case LE_OP: isFound = res <= 0; break;
                                case GE_OP: isFound = res >= 0; break;
                                case NE_OP: isFound = res != 0; break;
                                default: abort();
                            }
                            break;
                        }
                        default: abort();
                    }
                }
            }
        }
        
        // 如果找到了，则 nextSlotNum_ 自增1，为下一次做准备
        if(isFound) {
            // 设置 record
            rec.isValid_ = TRUE;
            rec.size_ = rmFH_->fHdr_.recordSize;
            if(rec.pData_)
                delete[] rec.pData_;
            rec.pData_ = new char[rec.size_];
            memcpy(rec.pData_, recordP, rec.size_);

            rec.rid_.isValid_ = TRUE;
            rec.rid_.pageNum_ = curPageNum_;
            // 查找完成后， nextSlotNum_ 会自动加1, 因此这里要减去 1
            rec.rid_.slotNum_ = nextSlotNum_ - 1;
        }

        // 当前页面查找结束， unpin 当前页面
        if((rc = rmFH_->pfFH_.UnpinPage(curPageNum_)))
            return rc;
    }

    return OK_RC;
}

RC RM_FileScan::CloseScan () {
    if(!isOpened_)
        return RM_SCAN_NOT_OPENED;
    isOpened_ = FALSE;
    return OK_RC;
}

RM_FileScan::ValueTy RM_FileScan::getValueFromPtr(void* value) {
    ValueTy uValue;
    // 根据类型设置 value
    if(value) {
        if(attrType_ == INT)
            memcpy(&uValue.intNum, value, sizeof(int));
        else if(attrType_ == FLOAT)
            memcpy(&uValue.floatNum, value, sizeof(float));
        else
            uValue.str = (char*)value;
    }
    return uValue;
}