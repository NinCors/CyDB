#ifndef _ix_h_
#define _ix_h_

#include <vector>
#include <string>
#include <limits>
#include "../rbf/rbfm.h"

#define IX_EOF (-1)  // end of the index scan
#define PASS 0
#define FAIL -1

class IX_ScanIterator;
class IXFileHandle;
class ixPage;

typedef struct{
	short int isIndex = -1; // 0 for Index, 1 for leaf
	short int keyCounter = 0;
	short int pageLen = -1;
	short int type = -1;
	int leftPointer = -1;
}ixPageInfo;

class ixPage{
	public:
		ixPageInfo pageInfo;
		vector<int> intKeys;
		vector<string> strKeys;
		vector<float> realKeys;
		vector<RID> ridList;
		vector<int> rightPointers; // only has 1 element for the leaf page
		void loadData(void*data);
		void returnData(void*data);
		void clear();
		ixPage(short int isindex);
		~ixPage();
};

class IndexManager {

    public:
        static IndexManager* instance();

        // Create an index file.
        RC createFile(const string &fileName);

        // Delete an index file.
        RC destroyFile(const string &fileName);

        // Open an index and return an ixfileHandle.
        RC openFile(const string &fileName, IXFileHandle &ixfileHandle);

        // Close an ixfileHandle for an index.
        RC closeFile(IXFileHandle &ixfileHandle);

        // Insert an entry into the given index that is indicated by the given ixfileHandle.
        RC insertEntry(IXFileHandle &ixfileHandle, const Attribute &attribute, const void *key, const RID &rid);

        // Delete an entry from the given index that is indicated by the given ixfileHandle.
        RC deleteEntry(IXFileHandle &ixfileHandle, const Attribute &attribute, const void *key, const RID &rid);

        // Initialize and IX_ScanIterator to support a range search
        RC scan(
        		string fileName,
        		IXFileHandle &ixfileHandle,
                const Attribute &attribute,
                const void *lowKey,
                const void *highKey,
                bool lowKeyInclusive,
                bool highKeyInclusive,
                IX_ScanIterator &ix_ScanIterator);

        // Print the B+ tree in pre-order (in a JSON record format)
        void printBtree(IXFileHandle &ixfileHandle, const Attribute &attribute) const;

        void updateMetaPage(void* page,int root, int type);

        IndexManager();
        ~IndexManager();

    private:
        static IndexManager *_index_manager;
};


class IX_ScanIterator {
    public:

		// Constructor
		IX_ScanIterator();
		RC IX_ScanIterator_initial(
				string fileName,
				IXFileHandle &ixfileHandle,
                const Attribute &attribute,
                const void *lowKey,
                const void *highKey,
                bool lowKeyInclusive,
                bool highKeyInclusive);

        // Destructor
        ~IX_ScanIterator();

        // Get next matching entry
        RC getNextEntry(RID &rid, void *key);

        // Terminate index scan
        RC close();

        string fileName;
        int scanCounter;
        int currentPageNum;
        int currentPos;
        RID currentRid;
        int type;

        int currentIntKey;
        float currentRealKey;
        string currentStrKey;

        int intLowKey;
        int intHighKey;

        float realLowKey;
        float realHighKey;

        string strLowKey = "";
        string strHighKey = "";

        bool lowKeyInclusive;
        bool highKeyInclusive;
        bool highKeyNullFlag=false;

        IXFileHandle *ixfileHandle;
};



class IXFileHandle {
    public:

    // variables to keep counter for each operation
    unsigned ixReadPageCounter;
    unsigned ixWritePageCounter;
    unsigned ixAppendPageCounter;

    // Root Value
    int root;
    FileHandle fileHandle;

    // Constructor
    IXFileHandle();

    // Destructor
    ~IXFileHandle();

	// Put the current counter values of associated PF FileHandles into variables
	RC collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount);

};

#endif
