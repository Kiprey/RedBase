//
// rm.h
//
//   Record Manager component interface
//
// This file does not include the interface for the RID class.  This is
// found in rm_rid.h
//

#ifndef RM_H
#define RM_H

// Please DO NOT include any files other than redbase.h and pf.h in this
// file.  When you submit your code, the test program will be compiled
// with your rm.h and your redbase.h, along with the standard pf.h that
// was given to you.  Your rm.h, your redbase.h, and the standard pf.h
// should therefore be self-contained (i.e., should not depend upon
// declarations in any other file).

// Do not change the following includes
#include "redbase.h"
#include "rm_rid.h"
#include "pf.h"

#include <cstring>

//
// RM_Record: RM Record interface
//
class RM_Record {
    friend class RM_FileScan;
    friend class RM_FileHandle;
public:
	RM_Record ();
    RM_Record (const RM_Record&);
    RM_Record& operator= (const RM_Record&);

	~RM_Record();

	// Return the data corresponding to the record.  Sets *pData to the
	// record contents.
	RC GetData(char *&pData) const;

	// Return the RID associated with the record
	RC GetRid (RID &rid) const;

private:
    char *pData_;
    size_t size_;
    RID rid_;
    Boolean isValid_;
};

// RM_FileHdr: RM File header

struct RM_FileHdr {
    int recordSize;         // 每一条记录的大小
    int numRecordsPerPage;  // 每页中可存放的记录数量
    int numPages;       // 当前文件的总 **存放记录** 的页数(不包括头页)

    int nextFreePage;   // 指向下一个空闲记录的页面索引
};

//
// RM_FileHandle: RM File interface
//
class RM_FileHandle {
	friend class RM_Manager;
    friend class RM_FileScan;
public:
	RM_FileHandle ();
	~RM_FileHandle();

	// Given a RID, return the record
	RC GetRec     (const RID &rid, RM_Record &rec) const;

	RC InsertRec  (const char *pData, RID &rid);       // Insert a new record

	RC DeleteRec  (const RID &rid);                    // Delete a record
	RC UpdateRec  (const RM_Record &rec);              // Update a record

	// Forces a page (along with any contents stored in this class)
	// from the buffer pool to disk.  Default value forces all pages.
	RC ForcePages (PageNum pageNum = ALL_PAGES);

private:
    Boolean IsValidSlotNum(SlotNum) const;

    PF_FileHandle pfFH_;
    RM_FileHdr fHdr_;
    Boolean modified_;
    Boolean isOpened_;
};

//
// RM_FileScan: condition-based scan of records in the file
//
class RM_FileScan {
public:
	RM_FileScan  ();
	~RM_FileScan ();

	RC OpenScan  (const RM_FileHandle &fileHandle,
	              AttrType   attrType,
	              int        attrLength,
	              int        attrOffset,
	              CompOp     compOp,
	              void       *value,
	              ClientHint pinHint = NO_HINT); // Initialize a file scan
	RC GetNextRec(RM_Record &rec);               // Get next matching record
	RC CloseScan ();                             // Close the scan
private:
    Boolean isOpened_;
    const RM_FileHandle* rmFH_;
    // 下一个待扫描的位置
    PageNum curPageNum_;
    SlotNum nextSlotNum_;

    AttrType attrType_;
    int attrLength_;
    int attrOffset_;
    CompOp compOp_;
    ClientHint pinHint_;
    union ValueTy{
        int intNum;
        float floatNum;
        char *str;
    } value_;

    // 从 value 所指向的内存，根据 comOp 转换成某个 Value
    ValueTy getValueFromPtr(void* value);

};

//
// RM_Manager: provides RM file management
//
class RM_Manager {
public:
	RM_Manager    (PF_Manager &pfm);
	~RM_Manager   ();

	RC CreateFile (const char *fileName, int recordSize);
	RC DestroyFile(const char *fileName);
	RC OpenFile   (const char *fileName, RM_FileHandle &fileHandle);

	RC CloseFile  (RM_FileHandle &fileHandle);
private:

    PF_Manager & pfMgr_;
};

//
// Print-error function
//
void RM_PrintError(RC rc);

#define RM_RID_INVALID              (START_RM_WARN + 0) // RID is not initialized
#define RM_RECORD_INVALID           (START_RM_WARN + 1) // RM_Record is not initialized
#define RM_FILE_ALREADY_OPENED      (START_RM_WARN + 2) // File is already opened
#define RM_FILE_NOT_OPENED          (START_RM_WARN + 3) // File is not opened
#define RM_INVALID_SLOT             (START_RM_WARN + 4) // slotNum out of range
#define RM_RECORD_NOT_FOUND         (START_RM_WARN + 5) // Record not found
#define RM_RECSIZE_MISMATCH         (START_RM_WARN + 6) // Record size is mismatch
#define RM_EOF                      (START_RM_WARN + 7) // no more records in scan
#define RM_SCAN_ALREADY_OPENED      (START_RM_WARN + 8) // last opened scan is not closed
#define RM_SCAN_NOT_OPENED          (START_RM_WARN + 9) // FileScan is not opened
#define RM_LASTWARN                 RM_SCAN_NOT_OPENED

#define RM_LARGE_RECORDSIZE         (START_RM_ERR - 0) // record size larger than PF_PAGE_SIZE
#define RM_SMALL_RECORDSIZE         (START_RM_ERR - 1) // record size is too small
#define RM_BAD_ATTRTYPE             (START_RM_ERR - 2) // Bad attribute type
#define RM_BAD_COMPOP               (START_RM_ERR - 3) // Bad compare operator
#define RM_ATTROFFSET_OUT_OF_RANGE  (START_RM_ERR - 4) // Attribute offset is out of range
#define RM_INCONSISTENT_ATTR        (START_RM_ERR - 5) // Attribute is incosistent
#define RM_ATTRLENGTH_OUT_OF_RANGE  (START_RM_ERR - 6) // Attribute length is out of range
#define RM_NULL_VALUE               (START_RM_ERR - 7) // Value is null
#define RM_LASTERROR            RM_NULL_VALUE

#endif
