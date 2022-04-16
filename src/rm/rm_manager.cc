#include "rm.h"
#include "rm_internal.h"

RM_Manager::RM_Manager(PF_Manager &pfm) : pfMgr_(pfm) {
    // do nothing
}

RM_Manager::~RM_Manager() {
    // do nothing
}

RC RM_Manager::CreateFile (const char *fileName, int recordSize) {
    int rc;

    if(recordSize <= 0)
        return RM_SMALL_RECORDSIZE;
    if(recordSize >= PF_PAGE_SIZE - sizeof(RM_PageHdr))
        return RM_LARGE_RECORDSIZE;

    if((rc = pfMgr_.CreateFile(fileName)))
        return rc;

    PF_FileHandle pfFH;
    if((rc = pfMgr_.OpenFile(fileName, pfFH)))
        return rc;
    
    PF_PageHandle pfPH;
    if((rc = pfFH.AllocatePage(pfPH)))
        return rc;

    char *pData;
    if((rc = pfPH.GetData(pData)))
        return rc;
    
    PageNum pageNum;
    if((rc = pfPH.GetPageNum(pageNum)))
        return rc;

    RM_FileHdr hdr;
    hdr.recordSize = recordSize;
    hdr.numPages = 0;
    hdr.nextFreePage = RM_NO_FREE_PAGE;

    // 计算每页中可存放的记录数量
    // recordSize*x(records) + x/8(bitmap) <= 4088(freePageSize)
    int records = 8 * (PF_PAGE_SIZE - sizeof(RM_PageHdr)) / (8 * recordSize + 1);
    // records 向下取8的倍数
    hdr.numRecordsPerPage = (records / 8) * 8; 

    memcpy(pData, &hdr, sizeof(RM_FileHdr));
    if((rc = pfFH.MarkDirty(pageNum)))
        return rc;

    if((rc = pfFH.UnpinPage(pageNum)))
        return rc;

    if((rc = pfMgr_.CloseFile(pfFH)))
        return rc;

    return OK_RC;
}

RC RM_Manager::DestroyFile(const char *fileName) {
    return pfMgr_.DestroyFile(fileName);
}

RC RM_Manager::OpenFile (const char *fileName, RM_FileHandle &fileHandle) {
    int rc;

    if(fileHandle.isOpened_)
        return RM_FILE_ALREADY_OPENED;

    PF_FileHandle pfFH;
    if((rc = pfMgr_.OpenFile(fileName, pfFH)))
        return rc;

    fileHandle.isOpened_ = TRUE;

    PF_PageHandle pfPH;
    if((rc = pfFH.GetFirstPage(pfPH)))
        return rc;

    char* pData;
    if((rc = pfPH.GetData(pData)))
        return rc;
    
    memcpy(&fileHandle.fHdr_, pData, sizeof(RM_FileHdr));
    fileHandle.pfFH_ = pfFH;
    fileHandle.modified_ = FALSE;

    PageNum pageNum;
    if((rc = pfPH.GetPageNum(pageNum)))
        return rc;
    if((rc = pfFH.UnpinPage(pageNum)))
        return rc;

    return OK_RC;
}

RC RM_Manager::CloseFile (RM_FileHandle &fileHandle) {
    int rc;

    if(!fileHandle.isOpened_)
        return RM_FILE_NOT_OPENED;

    if(fileHandle.modified_) {
        PF_PageHandle pfPH;

        if((rc = fileHandle.pfFH_.GetFirstPage(pfPH)))
            return rc;

        char *pData;
        if((rc = pfPH.GetData(pData)))
            return rc;

        memcpy(pData, &fileHandle.fHdr_, sizeof(RM_FileHdr));
        
        PageNum pageNum;
        if((rc = pfPH.GetPageNum(pageNum)))
            return rc;

        if((rc = fileHandle.pfFH_.MarkDirty(pageNum)))
            return rc;

        if((rc = fileHandle.pfFH_.UnpinPage(pageNum)))
            return rc;
    }

    if((rc = pfMgr_.CloseFile(fileHandle.pfFH_)))
        return rc;
    
    fileHandle.isOpened_ = FALSE;
    fileHandle.modified_ = TRUE;
    return OK_RC;
}