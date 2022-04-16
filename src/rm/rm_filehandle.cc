#include "rm.h"
#include "rm_internal.h"

#include <cassert>

RM_FileHandle::RM_FileHandle () : modified_(FALSE), isOpened_(FALSE) {}

RM_FileHandle::~RM_FileHandle() {
	// Don't need to do anything
}

RC RM_FileHandle::GetRec (const RID &rid, RM_Record &rec) const {
    int rc;
    PageNum pageNum;
    SlotNum slotNum;
    PF_PageHandle pfPH;
    char* pData;

    if(!isOpened_)
        return RM_FILE_NOT_OPENED;

    if((rc = rid.GetPageNum(pageNum)) || (rc = rid.GetSlotNum(slotNum)))
        return rc;

    // 这里只检查了 slotNum，因为 pageNum 会在下面的 GetThisPage 中被检查
    if(!IsValidSlotNum(slotNum))
        return RM_INVALID_SLOT;

    if((rc = pfFH_.GetThisPage(pageNum, pfPH)) || (rc = pfPH.GetData(pData)))
        return rc;
    
    // 检查 record 是否为空
    int ret;

    char* bitmap = pData + sizeof(RM_PageHdr);
    // 如果不为空
    if(bitmap[slotNum / 8] & (1 << (slotNum % 8))) {
        rec.isValid_ = TRUE;
        rec.size_ = fHdr_.recordSize;
        rec.rid_ = rid;
        if(rec.pData_)
            delete[] rec.pData_;
        rec.pData_ = new char[rec.size_];

        memcpy(rec.pData_, 
                bitmap + fHdr_.numRecordsPerPage / 8 + fHdr_.recordSize * slotNum, 
                rec.size_);
        ret = OK_RC;
    }
    else
        ret = RM_RECORD_NOT_FOUND;

    if(rc = pfFH_.UnpinPage(pageNum))
        return rc;
    return ret;
}

RC RM_FileHandle::InsertRec (const char *pData, RID &rid) {
    int rc;
    PF_PageHandle pfPH;
    PageNum pageNum;
    char* data;
    size_t slot;

    if(!isOpened_)
        return RM_FILE_NOT_OPENED;

    // 尝试寻找存在空闲条目的页目录
    int nextFreePos = fHdr_.nextFreePage;
    if(nextFreePos == RM_NO_FREE_PAGE) {
        if((rc = pfFH_.AllocatePage(pfPH)))
            return rc;
        // RM_PageHdr 在后面做初始化
        // bitmap 无需初始化，因为 AllocatePage 分配的内存已经是初始化为0的
    } else {
        if((rc = pfFH_.GetThisPage(nextFreePos, pfPH)))
            return rc;
    }

    // 此时已经有了一个可以写入的 record slot
    if((rc = pfPH.GetData(data)) || (rc = pfPH.GetPageNum(pageNum)))
        return rc;
        
    char* bitmap = data + sizeof(RM_PageHdr);
    int numRecords = fHdr_.numRecordsPerPage;

    // 查找可用 record 并设置 bitmap
    for(slot = 0; slot < numRecords; slot += 8) {
        if(bitmap[slot / 8] != -1) {
            char& bitmapChar = bitmap[slot / 8];
            for(size_t i = 0; i < 8; i++) {
                if(!(bitmapChar & (1 << i))) {
                    // 设置 bitmap
                    bitmapChar |= (1 << i);
                    // 设置 slot
                    slot += i;
                    break;
                }
            }
            break;
        }
    }
    // 不可能找不到
    assert(slot != numRecords);
    // 复制数据进 Page 中
    int recordSize = fHdr_.recordSize;
    memcpy(bitmap + (numRecords / 8) + recordSize * slot, pData, recordSize);
    if((rc = pfFH_.MarkDirty(pageNum)) || (rc = pfFH_.UnpinPage(pageNum)))
        return rc;
    // 更新 RID
    rid.pageNum_ = pageNum;
    rid.slotNum_ = slot;
    rid.isValid_ = TRUE;

    // 如果当前页面是新页面
    if(nextFreePos == RM_NO_FREE_PAGE) {
        RM_PageHdr* pHdr = (RM_PageHdr*)data;
        pHdr->nextFreePage = fHdr_.nextFreePage;
        fHdr_.nextFreePage = pageNum;
        fHdr_.numPages++;
    }
    else {
        // 判断当前页是否写满，以判断是否需要更新 RM_PageHdr
        for(slot = slot / 8 * 8; slot < numRecords; slot += 8)
            if(bitmap[slot / 8] != -1)
                break;
        if(slot == numRecords) {
            RM_PageHdr* pHdr = (RM_PageHdr*)data;
            fHdr_.nextFreePage = pHdr->nextFreePage;
            pHdr->nextFreePage = RM_PAGE_FULL_USED;
        }
    }
    return OK_RC;
}

RC RM_FileHandle::DeleteRec (const RID &rid) {
    int rc;
    PageNum pageNum;
    SlotNum slotNum;
    PF_PageHandle pfPH;
    char* pData;

    if(!isOpened_)
        return RM_FILE_NOT_OPENED;

    if((rc = rid.GetPageNum(pageNum)) || (rc = rid.GetSlotNum(slotNum)))
        return rc;

    // 这里只检查了 slotNum，因为 pageNum 会在下面的 GetThisPage 中被检查
    if(!IsValidSlotNum(slotNum))
        return RM_INVALID_SLOT;

    if((rc = pfFH_.GetThisPage(pageNum, pfPH)) || (rc = pfPH.GetData(pData)))
        return rc;
    
    // 检查 record 是否为空
    int ret;

    char* bitmap = pData + sizeof(RM_PageHdr);
    // 如果不为空
    if(bitmap[slotNum / 8] & (1 << (slotNum % 8))) {
        bitmap[slotNum / 8] &= ~(1 << (slotNum % 8));
        ret = OK_RC;

        // 如果当前页面是从完全满中删除一个 rec，则将该页面追加到 fHdr 中
        RM_PageHdr* pHdr = (RM_PageHdr*)pData;
        if(pHdr->nextFreePage == RM_PAGE_FULL_USED) {
            pHdr->nextFreePage = fHdr_.nextFreePage;
            fHdr_.nextFreePage = pageNum;
        }
        /*
            如果当前页面被删除后已经完全为空了，则可以试着删除该页面
            不过由于无法在 O(1) 下找到上一个 nextFreePage 为当前页面的页面（最主要的原因）
            同时 DisposePage 只是将当前页面放置进文件的 free list 中，并非实际释放
            因此这里就没有删除该页面。
        */
    }
    else
        ret = RM_RECORD_NOT_FOUND;

    if(rc = pfFH_.UnpinPage(pageNum))
        return rc;
    return ret;
}

RC RM_FileHandle::UpdateRec (const RM_Record &rec) {
    int rc;
    PageNum pageNum;
    SlotNum slotNum;
    PF_PageHandle pfPH;
    char* pData;
    RID rid;

    if(!isOpened_)
        return RM_FILE_NOT_OPENED;

    if((rc = rec.GetRid(rid)) 
        || (rc = rid.GetPageNum(pageNum)) 
        || (rc = rid.GetSlotNum(slotNum)))
        return rc;
    
    if(rec.size_ != fHdr_.recordSize)
        return RM_RECSIZE_MISMATCH;

    // 这里只检查了 slotNum，因为 pageNum 会在下面的 GetThisPage 中被检查
    if(!IsValidSlotNum(slotNum))
        return RM_INVALID_SLOT;

    if((rc = pfFH_.GetThisPage(pageNum, pfPH)) || (rc = pfPH.GetData(pData)))
        return rc;
    
    // 检查 record 是否为空
    int ret;

    char* bitmap = pData + sizeof(RM_PageHdr);
    // 如果不为空
    if(bitmap[slotNum / 8] & (1 << (slotNum % 8))) {
        memcpy(bitmap + fHdr_.numRecordsPerPage / 8 + fHdr_.recordSize * slotNum, 
                rec.pData_, 
                rec.size_);
        if((rc = pfFH_.MarkDirty(pageNum)))
            return rc;
        ret = OK_RC;
    }
    else
        ret = RM_RECORD_NOT_FOUND;

    if(rc = pfFH_.UnpinPage(pageNum))
        return rc;
    return ret;
}

RC RM_FileHandle::ForcePages (PageNum pageNum) {
    int rc;
    if(!isOpened_)
        return RM_FILE_NOT_OPENED;

    if(modified_) {
        PF_PageHandle pfPH;

        if((rc = pfFH_.GetFirstPage(pfPH)))
            return rc;

        char *pData;
        if((rc = pfPH.GetData(pData)))
            return rc;

        memcpy(pData, &fHdr_, sizeof(RM_FileHdr));
        
        PageNum pageNum;
        if((rc = pfPH.GetPageNum(pageNum)) 
            || (rc = pfFH_.MarkDirty(pageNum)) 
            || (rc = pfFH_.UnpinPage(pageNum)))
            return rc;
    }

    if((rc = pfFH_.ForcePages(pageNum))) 
        return rc;

    modified_ = FALSE;
    return OK_RC;
}

Boolean RM_FileHandle::IsValidSlotNum(SlotNum slotNum) const {
    return isOpened_ && (slotNum >= 0) && (slotNum < fHdr_.numRecordsPerPage);
}