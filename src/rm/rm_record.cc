#include "rm.h"

RM_Record::RM_Record () : isValid_(FALSE) {}

RM_Record::RM_Record (const RM_Record& r) {
    size_ = r.size_;
    rid_ = r.rid_;
    isValid_ = r.isValid_;

    char *pData_ = new char[size_];
    memcpy(pData_, r.pData_, size_);
}

RM_Record& RM_Record::operator= (const RM_Record &r)
{
	if (this != &r) {
        size_ = r.size_;
        rid_ = r.rid_;
        isValid_ = r.isValid_;

        char *pData_ = new char[size_];
        memcpy(pData_, r.pData_, size_);
	}

	return (*this);
}

RM_Record::~RM_Record() {
    if(isValid_) {
        delete pData_;
        isValid_ = FALSE;
    }
}

RC RM_Record::GetData(char *&pData) const {
    if(!isValid_)
        return RM_RECORD_INVALID;
    pData = pData_;
    return OK_RC;
}

RC RM_Record::GetRid (RID &rid) const {
    if(!isValid_)
        return RM_RECORD_INVALID;
    rid = rid_;
    return OK_RC;
}