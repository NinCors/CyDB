
#include "ix.h"

IndexManager* IndexManager::_index_manager = 0;

IndexManager* IndexManager::instance()
{
    if(!_index_manager)
        _index_manager = new IndexManager();

    return _index_manager;
}

IndexManager::IndexManager()
{

}

IndexManager::~IndexManager()
{
}

void IndexManager::updateMetaPage(void* page,int root, int type){
	memcpy((char*)page,&root,sizeof(int));
	memcpy((char*)page+sizeof(int),&type,sizeof(int));
}

RC IndexManager::createFile(const string &fileName)
{
    if(PagedFileManager::instance()-> createFile(fileName)==PASS){
    	FileHandle fh;
    	PagedFileManager::instance()->openFile(fileName,fh);
    	void*data = malloc(PAGE_SIZE);
    	updateMetaPage(data,0,-1);
    	fh.appendPage(data);
    	free(data);
    	return PASS;
    }
    return FAIL;
}

RC IndexManager::destroyFile(const string &fileName)
{
	 return PagedFileManager::instance() -> destroyFile(fileName);
}

RC IndexManager::openFile(const string &fileName, IXFileHandle &ixfileHandle)
{
	if(PagedFileManager::instance() -> openFile(fileName, ixfileHandle.fileHandle) == PASS){
		void*page = malloc(PAGE_SIZE);
		ixfileHandle.fileHandle.readPage(0,page);
		memcpy(&ixfileHandle.root,(char*)page,sizeof(int));
		free(page);
		return PASS;
	}
	return FAIL;
}

RC IndexManager::closeFile(IXFileHandle &ixfileHandle)
{
	return PagedFileManager::instance() -> closeFile(ixfileHandle.fileHandle);
}

bool isFit(RID r1, RID r2){
	if(r1.pageNum>r2.pageNum){return true;}
	else if(r1.pageNum<r2.pageNum){return false;}
	return r1.slotNum>=r2.slotNum ? true:false;
}


int deleteKey(ixPage *ixpage, RID rid,string strKey, int intKey, float floatKey){
	int counter = 0;
	while(counter<ixpage->pageInfo.keyCounter){
		switch(ixpage->pageInfo.type){
						case TypeVarChar:
									if(strKey == ixpage->strKeys[counter] && ixpage->ridList[counter].pageNum == rid.pageNum && ixpage->ridList[counter].slotNum == rid.slotNum){
										ixpage->strKeys.erase(ixpage->strKeys.begin()+counter);
										ixpage->ridList.erase(ixpage->ridList.begin()+counter);
										return counter;
									}
									break;
								case TypeInt:
									if(intKey == ixpage->intKeys[counter] && ixpage->ridList[counter].pageNum == rid.pageNum && ixpage->ridList[counter].slotNum == rid.slotNum){
										ixpage->intKeys.erase(ixpage->intKeys.begin()+counter);
										ixpage->ridList.erase(ixpage->ridList.begin()+counter);
										return counter;
									}
									break;
								case TypeReal:
									if(floatKey == ixpage->realKeys[counter+1] && ixpage->ridList[counter].pageNum == rid.pageNum && ixpage->ridList[counter].slotNum == rid.slotNum){
										ixpage->realKeys.erase(ixpage->realKeys.begin()+counter);
										ixpage->ridList.erase(ixpage->ridList.begin()+counter);
										return counter;
									}
									break;
						}
		counter++;
	}
	return -1;
}


//counter = findPos(currentPage,rid,strLowKey,intLowKey,realLowKey);
int findPos(ixPage ixpage, RID rid,string strKey,int intKey,float floatKey){
	int counter = -1;
	bool find = false;
	while(!find && counter+1<ixpage.pageInfo.keyCounter){
				switch(ixpage.pageInfo.type){
						case TypeVarChar:
							if(strKey > ixpage.strKeys[counter+1]){
								counter++;
							}
							else if(strKey == ixpage.strKeys[counter+1] && isFit(rid,ixpage.ridList[counter+1])){
								counter++;
							}
							else{
								find = true;
							}
							break;
						case TypeInt:
							if(intKey > ixpage.intKeys[counter+1]){
								counter++;
							}
							else if(intKey == ixpage.intKeys[counter+1] && isFit(rid,ixpage.ridList[counter+1])){
								counter++;
							}
							else{
								find = true;
							}
							break;
						case TypeReal:
							if(floatKey > ixpage.realKeys[counter+1]){
								counter++;
							}
							else if(floatKey == ixpage.realKeys[counter+1] && isFit(rid,ixpage.ridList[counter+1])){
								counter++;
							}
							else{
								find = true;
							}
							break;
				}
	}
	return counter;
}

void dividePage(int isIndex, ixPage divPage, ixPage &left, ixPage &right){
	//0 for leaf and 1 for index
	int mid = divPage.pageInfo.keyCounter/2;

	for(int i =0; i<mid;++i){
		left.ridList.push_back(divPage.ridList[i]);
		switch(divPage.pageInfo.type){
			case TypeVarChar:
				left.strKeys.push_back(divPage.strKeys[i]);
				break;
			case TypeInt:
				left.intKeys.push_back(divPage.intKeys[i]);
				break;
			case TypeReal:
				left.realKeys.push_back(divPage.realKeys[i]);
				break;
		}
		if(isIndex == 1){
			left.rightPointers.push_back(divPage.rightPointers[i]);
		}
	}

	left.pageInfo.keyCounter=mid;
	left.pageInfo.type=divPage.pageInfo.type;

	for(int i = mid; i<divPage.pageInfo.keyCounter;++i){
			right.ridList.push_back(divPage.ridList[i]);
			switch(divPage.pageInfo.type){
				case TypeVarChar:
					right.strKeys.push_back(divPage.strKeys[i]);
					break;
				case TypeInt:
					right.intKeys.push_back(divPage.intKeys[i]);
					break;
				case TypeReal:
					right.realKeys.push_back(divPage.realKeys[i]);
					break;
			}
			if(isIndex == 1){
					right.rightPointers.push_back(divPage.rightPointers[i]);
				}
		}
	right.pageInfo.keyCounter= divPage.pageInfo.keyCounter - mid;
	right.pageInfo.type=divPage.pageInfo.type;
}

void printPageInfo(ixPageInfo pg){
	cout<<"Is Index:"<<pg.isIndex<<endl;
	cout<<"KeyCounter:"<<pg.keyCounter<<endl;
	cout<<"LeftPointer:"<<pg.leftPointer<<endl;
	cout<<"PageLen:"<<pg.pageLen<<endl;
	cout<<"Type:"<<pg.type<<endl;
	cout<<"-------------------"<<endl;
}

RC IndexManager::insertEntry(IXFileHandle &ixfileHandle, const Attribute &attribute, const void *key, const RID &rid)
{
	/* Insert Entry procedure
	 * 1. Find root. If root is empty, update metapage, add one leaf page
	 * 2. Load data based on its type, compare with root, if smaller, go left,else, go right
	 * 3. Loop untilfind the first leaf page, and loop it to find the right position to insert it. Update the leafpage length.
	 * 4. If leafpage is overflowed, create a new leaf page, and divide it in to 2, push up the middle key.
	 * 5. Push up, check the current page to see if it is overflow, if it is, go back to step 5
	 * 6. Write it back to the index file
	 */

	void*page = malloc(PAGE_SIZE);
	int type = attribute.type;
	int root = ixfileHandle.root;
	ixPage indexPage(1);
	ixPage leafPage(0);
	ixPageInfo pageInfo;

	// Find root
	if(root == 0){
		root++;
		ixfileHandle.root++;

		pageInfo.isIndex = 0;
		pageInfo.type = type;
		memcpy((char*)page, &pageInfo,sizeof(ixPageInfo));
		int rightPointer = -1;
		memcpy((char*)page+PAGE_SIZE-sizeof(int),&rightPointer,sizeof(int));
		ixfileHandle.fileHandle.appendPage(page);
		//update the metadata page
		//page = malloc(PAGE_SIZE);
		updateMetaPage(page,root,type);
		ixfileHandle.fileHandle.writePage(0,page);
	}


	//Load Data
	int intKey;
	float floatKey;
	string strKey;
	int nameLength;
	void*buffer;
	char*res;

	switch(type){
		case TypeVarChar:
			memcpy(&nameLength,(char *)key, sizeof(int));
			buffer = malloc(nameLength);
			memcpy(buffer,(char *)key+4, nameLength);
			((char *)buffer)[nameLength] = '\0';
			//printf("Original: %s\n",buffer);
			res = (char*)buffer;
			strKey = string(res);
			//cout<<strKey<<" : with size "<<strKey.size()<<"With length"<<nameLength<<endl;
			//free(buffer);
			break;
		case TypeInt:
			intKey = *(int*)key;
			break;
		case TypeReal:
			floatKey = *(float*)key;
			break;
	}


	//cout<<"INT PUT STRING "<< strKey<<endl;
	ixfileHandle.fileHandle.readPage(root,page);

	memcpy(&pageInfo,(char*)page,sizeof(ixPageInfo));

	//find target position
	vector<int>visitedPage;
	vector<int>visitedPos;
	int counter = -1;
	while(pageInfo.isIndex == 1){ //if index page. find the right location or go to next pointer
		//cout<<"Find INdex!!!!!!!!!!!!!!!!!!!"<<endl;
		//printPageInfo(pageInfo);
		indexPage.loadData(page);
		counter = findPos(indexPage,rid,strKey,intKey,floatKey);

		//save visited page
		visitedPage.push_back(root);
		visitedPos.push_back(counter);
		if(counter == -1){
			//cout<<"Go left!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1"<<endl;
			root = pageInfo.leftPointer;
		}
		else{
			root = indexPage.rightPointers[counter];
		}
		//read next root
		ixfileHandle.fileHandle.readPage(root,page);
		memcpy(&pageInfo,(char*)page, sizeof(ixPageInfo));
	}
	//cout<<"-------------------------"<<endl;
	//printPageInfo(pageInfo);
	//this->printBtree(ixfileHandle,attribute);
	//cout<<"-------------------------"<<endl;

	//load the target leaf page and append the data into it.
	leafPage.loadData(page);
	counter = findPos(leafPage,rid,strKey,intKey,floatKey);
	counter+=1;
	//cout<<"INsert pos is "<<counter<<endl;
	leafPage.ridList.insert(leafPage.ridList.begin()+counter,rid);
	leafPage.pageInfo.pageLen += sizeof(RID);
	switch(pageInfo.type){
		case TypeVarChar:
			leafPage.strKeys.insert(leafPage.strKeys.begin()+counter,strKey);
			leafPage.pageInfo.pageLen += 4;
			leafPage.pageInfo.pageLen += nameLength;
			break;
		case TypeInt:
			//cout<<"new key is "<<intKey<<endl;
			leafPage.intKeys.insert(leafPage.intKeys.begin()+counter,intKey);
			leafPage.pageInfo.pageLen += 4;
			break;
		case TypeReal:
			leafPage.realKeys.insert(leafPage.realKeys.begin()+counter,floatKey);
			leafPage.pageInfo.pageLen += 4;
			break;
	}

	leafPage.pageInfo.keyCounter++;

	//check overflow situation
	if(leafPage.pageInfo.pageLen <= PAGE_SIZE){
		//if not overflow just convert it back to the original data type, and write it back to index page
		leafPage.returnData(page);
		ixfileHandle.fileHandle.writePage(root,page);
		free(page);
		return PASS;
	}
	//cout<<"overflow!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"<<endl;
	//printPageInfo(leafPage.pageInfo);
	//printBtree(ixfileHandle,attribute);
	//if overflowed, divide the current page into two leaf page.
	ixPage left(0);//create two page
	ixPage right(0);
	dividePage(leafPage.pageInfo.isIndex,leafPage,left,right);

	right.rightPointers.push_back(leafPage.rightPointers[0]);
	right.pageInfo.leftPointer = root;
	right.pageInfo.type = type;
	//cout<<"right page:"<<endl;
	//printPageInfo(right.pageInfo);
	//cout<<"left page:"<<endl;
	//printPageInfo(left.pageInfo);

	right.returnData(page);

	if(ixfileHandle.fileHandle.appendPage(page)==FAIL){
		//cout<<"append right fail"<<endl;
		free(page);
		return FAIL;}

	int newPage = ixfileHandle.fileHandle.getNumberOfPages()-1;
	//cout<<"Print current page num: "<<newPage+1<<endl;
	left.pageInfo.leftPointer=leafPage.pageInfo.leftPointer;
	left.pageInfo.type=type;
	left.rightPointers.push_back(newPage);
	left.returnData(page);
	if(ixfileHandle.fileHandle.writePage(root,page)==FAIL){
		//cout<<"Wirte left back fail"<<endl;
		free(page);
		return FAIL;
	}

	/*
	cout<<"-------------------------------------------"<<endl;

	cout<<"Before read back check left"<<endl;
	int testR;
	memcpy(&testR,(char*)page+PAGE_SIZE-sizeof(int),sizeof(int));
	cout<<"Cp right pointer : "<<testR<<endl;
	ixPage p1(0);
	p1.loadData(page);
	cout<<p1.pageInfo.leftPointer << " : left and right pointer : " << p1.rightPointers[0]<<endl;



	cout<<"Overflow check section"<<endl;
	void*test = malloc(PAGE_SIZE);
	int leftNum = root;
	int rightNum = newPage;
	cout<<"Check left : " << leftNum <<endl;
	ixfileHandle.fileHandle.readPage(leftNum,test);
	ixPage p(0);
	p.loadData(test);
	cout<<p.pageInfo.leftPointer << " : left and right pointer : " << p.rightPointers[0]<<endl;

	cout<<"Check right:"<< rightNum<<endl;
	ixfileHandle.fileHandle.readPage(rightNum,test);
	p.loadData(test);
	cout<< p.pageInfo.leftPointer << " : left and right pointer : " << p.rightPointers[0]<<endl;
	free(test);
	cout<<"FInish"<<endl;

	cout<<"-------------------------------------------"<<endl;
	 */

	RID newRid;
	//push up key, the first key in the right leaf
	switch(type){
		case TypeVarChar:
			strKey = right.strKeys[0];
			break;
		case TypeInt:
			intKey = right.intKeys[0];
			break;
		case TypeReal:
			floatKey = right.realKeys[0];
			break;
	}
	newRid = right.ridList[0];

	bool find = false;
	//vector<int>visitedPage;
	//vector<int>visitedPos;
	while(!find){
		if(visitedPage.empty()){
			//no root page, create new one, right
			//Get previous root first.
			ixfileHandle.fileHandle.readPage(0,page);
			int preRoot;
			memcpy(&preRoot,(char*)page,sizeof(int));

			//append the root page
			ixPageInfo newPageInfo;
			newPageInfo.isIndex=1;
			newPageInfo.type=type;
			newPageInfo.keyCounter=0;
			memcpy((char*)page,&newPageInfo,sizeof(ixPageInfo));
			indexPage = ixPage(1);
			indexPage.loadData(page);
			indexPage.pageInfo.keyCounter+=1;
			indexPage.pageInfo.leftPointer=preRoot;
			indexPage.ridList.push_back(newRid);
			switch(type){
				case TypeVarChar:
					indexPage.strKeys.push_back(strKey);
					break;
				case TypeInt:
					indexPage.intKeys.push_back(intKey);
					break;
				case TypeReal:
					indexPage.realKeys.push_back(floatKey);
					break;
			}
			indexPage.rightPointers.push_back(newPage);
			indexPage.returnData(page);
			ixfileHandle.fileHandle.appendPage(page);
			//update the root in the meta page
			int nextPage = newPage+1;

			//cout<<"The new root is "<<newPage<<"!!!!!!!!!!!!!!!!!!!!!!!!!!1"<<endl;
			//printPageInfo(newPageInfo);

			//page=malloc(PAGE_SIZE);
			updateMetaPage(page,nextPage,type);
			ixfileHandle.fileHandle.writePage(0,page);
			ixfileHandle.root = nextPage;
			find = true;
			return PASS;
		}
		//printBtree(ixfileHandle,attribute);

		int prePage = visitedPage.back();
		visitedPage.pop_back();
		int prePos = visitedPos.back()+1;
		visitedPos.pop_back();
		//load the previous upper page
		ixfileHandle.fileHandle.readPage(prePage,page);
		indexPage = ixPage(1);
		indexPage.loadData(page);

		switch(type){
			case TypeVarChar:
				indexPage.strKeys.insert(indexPage.strKeys.begin()+prePos,strKey);
				indexPage.pageInfo.pageLen+=4;
				indexPage.pageInfo.pageLen+=strKey.size();
				break;
			case TypeInt:
				indexPage.intKeys.insert(indexPage.intKeys.begin()+prePos,intKey);
				indexPage.pageInfo.pageLen+=sizeof(int);
				break;
			case TypeReal:
				indexPage.realKeys.insert(indexPage.realKeys.begin()+prePos,floatKey);
				indexPage.pageInfo.pageLen+=sizeof(float);
				break;
		}
		indexPage.ridList.insert(indexPage.ridList.begin()+prePos,newRid);
		indexPage.pageInfo.pageLen+=sizeof(RID); //??
		indexPage.rightPointers.insert(indexPage.rightPointers.begin()+prePos,newPage);
		indexPage.pageInfo.pageLen+=sizeof(int);
		indexPage.pageInfo.keyCounter += 1;

		if(indexPage.pageInfo.pageLen <= PAGE_SIZE){
			indexPage.returnData(page);
			ixfileHandle.fileHandle.writePage(prePage,page);
			find = true;
			free(page);
			return PASS;
		}

		//printBtree(ixfileHandle,attribute);
		//If the upper layer is overflowed too
		left = ixPage(1);//create two page
		right = ixPage(1);
		dividePage(indexPage.pageInfo.isIndex,indexPage,left,right);

		//right.rightPointers.push_back(indexPage.rightPointers[prePos]);
		right.pageInfo.leftPointer = indexPage.rightPointers[indexPage.pageInfo.keyCounter/2];
		right.pageInfo.type = type;
		right.returnData(page);
		ixfileHandle.fileHandle.appendPage(page);

		newPage = ixfileHandle.fileHandle.getNumberOfPages()-1;
		left.pageInfo.leftPointer=indexPage.pageInfo.leftPointer;
		left.pageInfo.type=type;
		//left.rightPointers.push_back(newPage);
		left.returnData(page);
		ixfileHandle.fileHandle.writePage(prePage,page);

		//push up key, the first key in the right index
		switch(type){
			case TypeVarChar:
				strKey = right.strKeys[0];
				break;
			case TypeInt:
				intKey = right.intKeys[0];
				break;
			case TypeReal:
				floatKey = right.realKeys[0];
				break;
		}
		newRid = right.ridList[0];
	}
    return FAIL;
}

RC IndexManager::deleteEntry(IXFileHandle &ixfileHandle, const Attribute &attribute, const void *key, const RID &rid)
{
    void*page = malloc(PAGE_SIZE);
	int type = attribute.type;
	int root = ixfileHandle.root;
	ixPage indexPage(1);
	ixPage leafPage(0);
	ixPageInfo pageInfo;

	//Load Data
	int intKey;
	float floatKey;
	string strKey;
	int nameLength;
	void*buffer;
	char*res;

	switch (type) {
		case TypeVarChar:
			memcpy(&nameLength, (char *)key, sizeof(int));
			buffer = malloc(nameLength);
			memcpy(buffer, (char *)key + 4, nameLength);
			((char *)buffer)[nameLength] = '\0';
			//printf("Original: %s\n",buffer);
			res = (char*)buffer;
			strKey = string(res);
			//cout << strKey << " : Delete with size " << strKey.size() << "With length" << nameLength << endl;
			break;
		case TypeInt:
			intKey = *(int*)key;
			break;
		case TypeReal:
			floatKey = *(float*)key;
			break;
	}

	ixfileHandle.fileHandle.readPage(root,page);

	memcpy(&pageInfo,(char*)page,sizeof(ixPageInfo));

	//find target position
	vector<int>visitedPage;
	vector<int>visitedPos;
	int counter = -1;
	while(pageInfo.isIndex == 1){ //if index page. find the right location or go to next pointer
		//cout<<"Find INdex!!!!!!!!!!!!!!!!!!!"<<endl;
		//printPageInfo(pageInfo);
		indexPage.loadData(page);
		counter = findPos(indexPage,rid,strKey,intKey,floatKey);

		//save visited page
		visitedPage.push_back(root);
		visitedPos.push_back(counter);
		if(counter == -1){
			//cout<<"Go left!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1"<<endl;
			root = pageInfo.leftPointer;
		}
		else{
			root = indexPage.rightPointers[counter];
		}
		//read next root
		ixfileHandle.fileHandle.readPage(root,page);
		memcpy(&pageInfo,(char*)page, sizeof(ixPageInfo));
	}

	//load the target leaf page and append the data into it.
	leafPage.loadData(page);

	counter = 0;
	bool find = false;
	while(!find && counter<leafPage.pageInfo.keyCounter){
		switch(leafPage.pageInfo.type){
						case TypeVarChar:
									if(strKey == leafPage.strKeys[counter] && leafPage.ridList[counter].pageNum == rid.pageNum && leafPage.ridList[counter].slotNum == rid.slotNum){
										leafPage.pageInfo.pageLen -=4;
										leafPage.pageInfo.pageLen -= leafPage.strKeys[counter].size();
										leafPage.strKeys.erase(leafPage.strKeys.begin()+counter);
										leafPage.ridList.erase(leafPage.ridList.begin()+counter);
										find = true;
									}
									break;
								case TypeInt:
									if(intKey == leafPage.intKeys[counter] && leafPage.ridList[counter].pageNum == rid.pageNum && leafPage.ridList[counter].slotNum == rid.slotNum){
										leafPage.intKeys.erase(leafPage.intKeys.begin()+counter);
										leafPage.ridList.erase(leafPage.ridList.begin()+counter);
										leafPage.pageInfo.pageLen -=4;
										find = true;
									}
									break;
								case TypeReal:
									if(floatKey == leafPage.realKeys[counter] && leafPage.ridList[counter].pageNum == rid.pageNum && leafPage.ridList[counter].slotNum == rid.slotNum){
										leafPage.realKeys.erase(leafPage.realKeys.begin()+counter);
										leafPage.ridList.erase(leafPage.ridList.begin()+counter);
										leafPage.pageInfo.pageLen -=4;
										find = true;
									}
									break;
						}
		counter++;
	}
	//cout<<"Delete position is: "<<counter-1<<endl;
	if(!find){
		//cout<<"CAn't find "<<floatKey<<endl;
		//cout<<"CAN'T FIND rid "<<rid.pageNum <<" : "<<rid.slotNum<<endl;
		free(page);
		return FAIL;}
	leafPage.pageInfo.keyCounter -= 1;
	leafPage.returnData(page);
	ixfileHandle.fileHandle.writePage(root,page);
	free(page);
	return PASS;
}


RC IndexManager::scan(
		string fileName,
		IXFileHandle &ixfileHandle,
        const Attribute &attribute,
        const void      *lowKey,
        const void      *highKey,
        bool			lowKeyInclusive,
        bool        	highKeyInclusive,
        IX_ScanIterator &ix_ScanIterator)
{
    /* Scan procedure:
     * 1. Search the lowKey in the leaf page, save the leaf page and the position
     * 2. If can't find it, return -1, otherwise return 0.
     * 3. Use saved leaf page and current pos to get the current value, and increase pos++,
     * 4. If pos is greater than keyCounter,pos = 0, and leaf page = next leafpage.
     * 5. If next leafpage is -1, then no next value, return EOF
     */

	return ix_ScanIterator.IX_ScanIterator_initial(fileName,ixfileHandle,attribute,lowKey,highKey,lowKeyInclusive,highKeyInclusive);
}

void printCurrentPage(int currentPageNum,int type,IXFileHandle &ixfileHandle,int level){
	void* page = malloc(PAGE_SIZE);
	ixfileHandle.fileHandle.readPage(currentPageNum,page);
	ixPageInfo pageInfo;
	memcpy(&pageInfo,(char*)page,sizeof(ixPageInfo));
	ixPage currentPage(pageInfo.isIndex);
	currentPage.loadData(page);

	string tmp;
	if(pageInfo.isIndex == 1){// Indexpage
		printf("{\"keys\":[");
		for(int i = 0; i< pageInfo.keyCounter;++i){
			printf("\"");
			switch(type){
				case TypeVarChar:
					tmp = currentPage.strKeys[i];
					printf("%s",tmp.c_str());
					break;
				case TypeInt:
					printf("%d",currentPage.intKeys[i]);
					break;
				case TypeReal:
					printf("%f",currentPage.realKeys[i]);
					break;
				}
				printf("\"");
				if(i!=pageInfo.keyCounter-1){
					printf(",");
				}
			}
		printf("],\n");
		//print children
		printf("\"children\":[");
		printCurrentPage(currentPage.pageInfo.leftPointer,type,ixfileHandle,level+1);
		if(currentPage.pageInfo.leftPointer>=0){
			printf(",");
		}
		for(int w = 0;w<pageInfo.keyCounter;++w){
			printCurrentPage(currentPage.rightPointers[w],type,ixfileHandle,level+1);
			if(w!=pageInfo.keyCounter-1){
				printf(",");
			}
		}
		printf("]}\n");
	}
	else if(pageInfo.isIndex == 0){ //Leaf page
		//cout<<"Current page is "<< currentPageNum <<"next page is "<<currentPage.rightPointers[0]<<endl;
		printf("{\"keys\":[");
		for(int i = 0; i< pageInfo.keyCounter;++i){
			printf("\"");
			switch(type){
				case TypeVarChar:
					printf("%s", currentPage.strKeys[i].c_str());
					break;
				case TypeInt:
					printf("%d",currentPage.intKeys[i]);
					break;
				case TypeReal:
					printf("%f",currentPage.realKeys[i]);
					break;
				}
			printf(":[(%d, %d)]", currentPage.ridList[i].pageNum, currentPage.ridList[i].slotNum);
			printf("\"");
			if(i!=pageInfo.keyCounter-1){
				printf(",");
			}
		}
		printf("]}\n");
	}
	else{
		//page loading error
		printf("Something is wrong with loading page\n");
	}
}

void IndexManager::printBtree(IXFileHandle &ixfileHandle, const Attribute &attribute) const {
	/*
	 * Print B tree Procedure
	 * Start from root use BFS to loop all the nodes
	 * If it is an index page then print { "Keys":[actual keys], "children:"[children]}, each children can be index page or leaf page
	 * If it is a leaf page then print {"Keys":["key:[data]","key1":[data]]}
	 */
	void*page =malloc(PAGE_SIZE);
	ixfileHandle.fileHandle.readPage(0,page);
	int root;
	memcpy(&root,(char*)page,sizeof(int));
	if(ixfileHandle.root!=root){
		//cout<<"emmm root?"<<endl;
	}
	int type;
	memcpy(&type,(char*)page+sizeof(int),sizeof(int));
	if(type!=attribute.type){
		//cout<<"type??"<<endl;
	}
	free(page);

	printCurrentPage(root,type,ixfileHandle,0);
}

IX_ScanIterator::IX_ScanIterator(){
	currentPageNum = -1;
	currentPos = -1;
	type= -1;
	ixfileHandle = NULL;
}

RC IX_ScanIterator::IX_ScanIterator_initial(string fileName,IXFileHandle &ixfileHandle, const Attribute &attribute,const void *lowKey,const void *highKey,
    bool lowKeyInclusive,bool highKeyInclusive){

	this->ixfileHandle = &ixfileHandle;
	this->type = attribute.type;
	this->lowKeyInclusive = lowKeyInclusive;
	this->highKeyInclusive = highKeyInclusive;
	this->fileName = fileName;

	//load low and high key
	int nameLength;
	switch(type){
			case TypeVarChar:
				if(lowKey){
				memcpy(&nameLength,(char *)lowKey, sizeof(int));
				for(int i = 0; i<nameLength;i++){
					this->strLowKey+=(char*)lowKey+sizeof(int) + i;
				}}
				if(highKey){
				memcpy(&nameLength,(char *)highKey, sizeof(int));
				for(int i = 0; i<nameLength;i++){
					this->strHighKey+=(char*)highKey+sizeof(int) + i;
				}}
				else{this->highKeyNullFlag = true;}
				break;
			case TypeInt:
				if(lowKey){this->intLowKey = *(int*)lowKey;}
				else{intLowKey = -std::numeric_limits<int>::max();}
				if(highKey){this->intHighKey = *(int*)highKey;}
				else{intHighKey = std::numeric_limits<int>::max();}
				break;
			case TypeReal:
				if(lowKey){this->realLowKey = *(float*)lowKey;}
				else{realLowKey = -std::numeric_limits<float>::max();}
				//cout<<"float low key " <<realLowKey;
				if(highKey){this->realHighKey = *(float*)highKey;}
				else{realHighKey = std::numeric_limits<float>::max();}
				//cout<<"float high key " <<realHighKey;
				break;
		}
	ixPage currentPage(0);
	//find first page
	int root;
	ixPageInfo pageInfo;
	RID rid;
	rid.pageNum = -1;
	rid.slotNum = -1;
	void*page = malloc(PAGE_SIZE);
	if(this->ixfileHandle->fileHandle.readPage(0,page)==FAIL){
		//cout<<"here"<<endl;
		return FAIL;}
	memcpy(&root,(char*)page,sizeof(int));

	//find low_key position
	this->ixfileHandle->fileHandle.readPage(root,page);

	memcpy(&pageInfo,(char*)page,sizeof(ixPageInfo));
	int counter = -1;
	bool find;
	while(pageInfo.isIndex == 1){ //if index page. find the right location or go to next pointer

		//printPageInfo(pageInfo);
		currentPage.loadData(page);
		counter = -1;
		find =false;
		while(!find && counter+1<currentPage.pageInfo.keyCounter){
					switch(currentPage.pageInfo.type){
							case TypeVarChar:
								if(strLowKey > currentPage.strKeys[counter+1]){
									counter++;
								}
								else{
									find = true;
								}
								break;
							case TypeInt:
								if(intLowKey > currentPage.intKeys[counter+1]){
									counter++;
								}
								else{
									find = true;
								}
								break;
							case TypeReal:
								if(realLowKey > currentPage.realKeys[counter+1]){
									counter++;
								}
								else{
									find = true;
								}
								break;
					}}
		if(counter == -1){
			root = pageInfo.leftPointer;
		}
		else{
			root = currentPage.rightPointers[counter];
		}
		//read next root
		this->ixfileHandle->fileHandle.readPage(root,page);
		memcpy(&pageInfo,(char*)page, sizeof(ixPageInfo));
	}
	if(root < 1){
		//cout<<"failed!"<<endl;
		free(page);
		return FAIL;
	}
	//load the target leaf page and append the data into it.
	currentPage.loadData(page);

	counter = -1;
	find = false;
	while(!find && counter+1<currentPage.pageInfo.keyCounter){
					switch(currentPage.pageInfo.type){
							case TypeVarChar:
								if(strLowKey > currentPage.strKeys[counter+1]){
									counter++;
								}
								else{
									find = true;
								}
								break;
							case TypeInt:
								if(intLowKey > currentPage.intKeys[counter+1]){
									counter++;
								}
								else{
									find = true;
								}
								break;
							case TypeReal:
								if(realLowKey > currentPage.realKeys[counter+1]){
									counter++;
								}
								else{
									find = true;
								}
								break;
					}
		}
	counter+=1;
	this->currentPageNum = root;
	this->currentPos = counter;
	this->scanCounter = 0;
	if(!this->lowKeyInclusive){
		this->currentPos++;
	}
	this->currentRid.pageNum = -1;
	this->currentRid.slotNum = -1;
	free(page);
	return PASS;
}

IX_ScanIterator::~IX_ScanIterator()
{
}

RC IX_ScanIterator::getNextEntry(RID &rid, void *key)
{
	void*page = malloc(PAGE_SIZE);
	ixPage currentPage(0);
	int pageNum = this->currentPageNum;
	ixfileHandle->fileHandle.readPage(this->currentPageNum,page);
	currentPage.loadData(page);

	if(currentPage.pageInfo.type!=type){
		PagedFileManager::instance()->openFile(fileName,ixfileHandle->fileHandle);
		//cout<<"I don't know what's wrong with that but good luck"<<endl;
		return getNextEntry(rid,key);
	}

	if(scanCounter>0&&currentPos<currentPage.pageInfo.keyCounter && currentPage.ridList[currentPos].pageNum == currentRid.pageNum
			&& currentPage.ridList[currentPos].slotNum == currentRid.slotNum){
		this->currentPos++;
	}

	//cout<<"CUrrent page : "<<pageNum << " current position "<<this->currentPos<<" current page Key counter "<< currentPage.pageInfo.keyCounter<<endl;
	//cout<<"Current key : "<< currentIntKey<<endl;
	while(currentPos>=currentPage.pageInfo.keyCounter){
		//cout<<"next page???????????????????? "<<endl;
		if(currentPage.rightPointers[0] == -1){
			free(page);
			return IX_EOF;
		}
		this->currentPageNum = currentPage.rightPointers[0];
		//cout<<currentPage.rightPointers[0]<<endl;
		this->currentPos = 0;
		this->ixfileHandle->fileHandle.readPage(this->currentPageNum,page);
		currentPage.loadData(page);
	}
	bool find = false;
	int size;
	switch(type){
				case TypeVarChar:
					this->currentStrKey = currentPage.strKeys[currentPos];
					if(this->highKeyNullFlag || this->currentStrKey < this->strHighKey || (this->currentStrKey == this->strHighKey && this->highKeyInclusive)){
						find = true;
					}
					if(!find){
						free(page);
						return IX_EOF;
					}
					size = this->currentStrKey.size();
					memcpy((char*)key,&size,sizeof(int));
					memcpy((char*)key,&this->currentStrKey,size);
					rid = currentPage.ridList[currentPos];
					break;
				case TypeInt:
					this->currentIntKey = currentPage.intKeys[currentPos];
					if(this->currentIntKey < this->intHighKey || (this->currentIntKey == this->intHighKey && this->highKeyInclusive)){
						find = true;
					}
					if(!find){
						free(page);
						return IX_EOF;
					}
					memcpy((char*)key, &this->currentIntKey,sizeof(int));
					rid = currentPage.ridList[currentPos];
					break;
				case TypeReal:
					this->currentRealKey = currentPage.realKeys[currentPos];
					if(this->currentRealKey < this->realHighKey || (this->currentRealKey == this->realHighKey && this->highKeyInclusive)){
						find = true;
					}
					if(!find){
						free(page);
						return IX_EOF;
					}
					memcpy((char*)key, &this->currentRealKey,sizeof(float));
					rid = currentPage.ridList[currentPos];
					break;
		}
	this->currentRid = rid;
	this->scanCounter++;
	//this->currentPageNum =pageNum;
	free(page);
	return PASS;
}

RC IX_ScanIterator::close()
{
	this->ixfileHandle=NULL;
	return 0;
}

IXFileHandle::IXFileHandle()
{
    ixReadPageCounter = 0;
    ixWritePageCounter = 0;
    ixAppendPageCounter = 0;
    root = 0;
}

IXFileHandle::~IXFileHandle()
{

}

RC IXFileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount)
{
    fileHandle.collectCounterValues(readPageCount, writePageCount, appendPageCount);
	return PASS;
}

ixPage::ixPage(short int isindex){
	pageInfo.isIndex = isindex; // 0 for leaf, 1 for index
	pageInfo.keyCounter = 0;
	pageInfo.pageLen = 12;
	if(isindex == 0){
		pageInfo.pageLen = 16;//plus one right pointer
	}
	pageInfo.type = -1;
	pageInfo.leftPointer = -1; // always -1 for the leaf page
}

ixPage::~ixPage(){
}

void ixPage::clear(){

}

void ixPage::loadData(void *page){
	/*
	 * Index Design:
	 *	pageInfo(isIndex,keyCounter, pageLen, type, leftPointer) + data + rid + data + rid + free space rightpointer
	 * Leaf page design:
	 * 	pageInfo + data + rid+ rightPointer + data + rid + rightPointer + free space
	 */

	//clean previous design
	intKeys.clear();
		realKeys.clear();

		strKeys.clear();

		ridList.clear();
		rightPointers.clear();


	//get page Info
	int offset = 0;

	memcpy(&pageInfo,(char*)page, sizeof(ixPageInfo));
	offset+=sizeof(ixPageInfo);
	RID rid;
	void*buffer;
	char*res;

	for(int i =0;i<pageInfo.keyCounter;++i){
		switch(pageInfo.type){
			case TypeInt:
				int key;
				memcpy(&key,(char*)page+offset,sizeof(int));
				intKeys.push_back(key);
				offset+=sizeof(int);
				memcpy(&rid,(char*)page+offset,sizeof(RID));
				ridList.push_back(rid);
				offset+=sizeof(RID);
				if(pageInfo.isIndex == 1){ //index has right pointer
					int rightPointer;
					memcpy(&rightPointer,(char*)page+offset,sizeof(int));
					offset+=sizeof(int);
					rightPointers.push_back(rightPointer);
				}
				break;
			case TypeReal:
				float key1;
				memcpy(&key1,(char*)page+offset,sizeof(float));
				realKeys.push_back(key1);
				offset+=sizeof(float);
				memcpy(&rid,(char*)page+offset,sizeof(RID));
				ridList.push_back(rid);
				offset+=sizeof(RID);
				if(pageInfo.isIndex == 1){ //index has right pointer
					int rightPointer;
					memcpy(&rightPointer,(char*)page+offset,sizeof(int));
					offset+=sizeof(int);
					rightPointers.push_back(rightPointer);
				}
				break;
			case TypeVarChar:
				int strLen;
				memcpy(&strLen,(char*)page+offset,sizeof(int));

				offset+=sizeof(int);
				string str = "";
				buffer= malloc(strLen);
				memcpy(buffer,(char*)page+offset,strLen);
				((char*)buffer)[strLen] = '\0';
				res=(char*)buffer;
				str =string(res);

				offset += strLen;
				strKeys.push_back(str);
				memcpy(&rid,(char*)page+offset,sizeof(RID));
				ridList.push_back(rid);
				offset+=sizeof(RID);
				if(pageInfo.isIndex == 1){ //index has right pointer
					int rightPointer;
					memcpy(&rightPointer,(char*)page+offset,sizeof(int));
					offset+=sizeof(int);
					rightPointers.push_back(rightPointer);
				}
				break;
		}
	}

	if(pageInfo.isIndex == 0){ // if leaf
		int rightPointer;
		memcpy(&rightPointer,(char*)page+PAGE_SIZE-sizeof(int),sizeof(int));
		if(rightPointers.size()==1){
			rightPointers[0]=rightPointer;
		}
		else{
			rightPointers.push_back(rightPointer);
		}
		offset+=sizeof(int);
	}
	pageInfo.pageLen = offset;
}





void ixPage::returnData(void* page){
	/*
		 * Index Design:
		 *	pageInfo(isIndex,keyCounter, pageLen, type, leftPointer) + data + rid + data + rid + free space rightpointer
		 * Leaf page design:
		 * 	pageInfo + data + rid+ rightPointer + data + rid + rightPointer + free space
		 */
	//copy pginfo back to page
	memcpy((char*)page,&pageInfo,sizeof(ixPageInfo));
	int offset = 0;
	offset+=sizeof(ixPageInfo);
	for(int i = 0;i<pageInfo.keyCounter;++i){
			switch(pageInfo.type){
					case TypeInt:
						memcpy((char*)page+offset,&intKeys[i],sizeof(int));
						offset+=sizeof(int);
						memcpy((char*)page+offset,&ridList[i],sizeof(RID));
						offset+=sizeof(RID);
						if(pageInfo.isIndex == 1){ //index has right pointer
							memcpy((char*)page+offset,&rightPointers[i],sizeof(int));
							offset+=sizeof(int);
						}
						break;
					case TypeReal:
						memcpy((char*)page+offset,&realKeys[i],sizeof(float));
						offset+=sizeof(float);
						memcpy((char*)page+offset,&ridList[i],sizeof(RID));
						offset+=sizeof(RID);
						if(pageInfo.isIndex == 1){ //index has right pointer
							memcpy((char*)page+offset,&rightPointers[i],sizeof(int));
							offset+=sizeof(int);
						}
						break;
					case TypeVarChar:
						int strLen = strKeys[i].size();
						memcpy((char*)page+offset,&strLen,sizeof(int));
						offset+=sizeof(int);
						memcpy((char*)page+offset,strKeys[i].c_str(),strLen);
						offset += strLen;

						memcpy((char*)page+offset,&ridList[i],sizeof(RID));
						offset+=sizeof(RID);
						if(pageInfo.isIndex == 1){ //index has right pointer
							memcpy((char*)page+offset,&rightPointers[i],sizeof(int));
							offset+=sizeof(int);
						}
						break;
				}
	}

	if(pageInfo.isIndex==0){ //leaf page
		int rightPointer = rightPointers[0];
		memcpy((char*)page+PAGE_SIZE-sizeof(int),&rightPointer,sizeof(int));
		offset += sizeof(int);
	}

}

