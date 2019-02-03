#include "rbfm.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <iostream>
#include <string>

RecordBasedFileManager* RecordBasedFileManager::_rbf_manager = 0;

RecordBasedFileManager* RecordBasedFileManager::instance()
{
    if(!_rbf_manager)
        _rbf_manager = new RecordBasedFileManager();

    return _rbf_manager;
}

RecordBasedFileManager::RecordBasedFileManager()
{
}

RecordBasedFileManager::~RecordBasedFileManager()
{
	if(_rbf_manager){
		_rbf_manager = 0;
		delete _rbf_manager;
	}
}

RC RecordBasedFileManager::createFile(const string &fileName) {
    return PagedFileManager::instance() -> createFile(fileName);
}

RC RecordBasedFileManager::destroyFile(const string &fileName) {
    return PagedFileManager::instance() -> destroyFile(fileName);
}

RC RecordBasedFileManager::openFile(const string &fileName, FileHandle &fileHandle) {
    return PagedFileManager::instance() -> openFile(fileName, fileHandle);
}

RC RecordBasedFileManager::closeFile(FileHandle &fileHandle) {
	return PagedFileManager::instance() -> closeFile(fileHandle);
}


RC getAttrData1(vector<Attribute> recordDescriptor, vector<string> targetAttrs, void* original_data, void* attrData){
	/*
	 * Get specific data with attribute from inputdata
	 * attrData format: nullIndicator + data
	 */


	int offset_attr =0;

	short int recordDes_size = recordDescriptor.size();

	int attrs_size = targetAttrs.size();
	int nullSize = ceil((double)attrs_size/CHAR_BIT);
	offset_attr += nullSize;
	unsigned char*newNullFieldsIndicator = new unsigned char[nullSize];

	//get null fields indicator
	int nullFieldsIndicatorSize = ceil((double)recordDes_size / CHAR_BIT);
	unsigned char *nullFieldsIndicator = (unsigned char *) malloc(nullFieldsIndicatorSize);
	memcpy(nullFieldsIndicator, original_data, nullFieldsIndicatorSize);


	bool isNull;
	int counter = 0;

	for(int w = 0; w<targetAttrs.size();w++){
		int offset_origin = nullFieldsIndicatorSize;
		for(int i = 0;i<recordDes_size;i++){
			isNull = nullFieldsIndicator[i/8] & 1<<(7-i%8);
			if(recordDescriptor[i].name == targetAttrs[w]){
				counter++;
				newNullFieldsIndicator[w] =nullFieldsIndicator[i]& 1<<(7-i%8);
				if(!isNull){
					if(recordDescriptor[i].type == TypeVarChar){
						int nameLength;
						memcpy(&nameLength,(char*)original_data + offset_origin, sizeof(int));
						memcpy((char*)attrData+offset_attr,(char*)original_data+offset_origin,sizeof(int)+nameLength);
						offset_attr += (sizeof(int) + nameLength);
						offset_origin += (sizeof(int) + nameLength);
						}
					else{
						memcpy((char*)attrData+offset_attr,(char*)original_data+offset_origin,sizeof(int)); //sizeof(int) and float are same
						offset_attr += 4;
						offset_origin +=4;
					}
				}
			}
			else{
				if(!isNull){
					switch(recordDescriptor[i].type){
					//for TypeVarChar, the incoming format is namelenght + namestr
					case TypeVarChar:
						int nameLength1;
						memcpy(&nameLength1,(char *)original_data + offset_origin, sizeof(int));
						offset_origin += (sizeof(int)+nameLength1);
						//cout<<"Put var char with length: " <<sizeof(int) + nameLength<<endl;
						break;
					case TypeInt:
						//cout<<"Put int " << sizeof(int) <<endl;
						offset_origin+= sizeof(int);
						break;
					case TypeReal:
						//cout<<"Put real!"<<sizeof(float)<<endl;
						offset_origin+= sizeof(float);
						break;
					}
				}
			}
		}
	}
	//get the nullfieldsIndicator back
	memcpy((char*)attrData,newNullFieldsIndicator,nullSize);

	free(newNullFieldsIndicator);
	free(nullFieldsIndicator);
	return 0;

}


RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, RID &rid) {
	/* First, get parsed data and size from recrodDescriptor and data
	 * Second, check if the current page have enough free space to take the data
	 * Finally, insert the data
	 *
	 * The page format will be dataAreaDescriptor + Free Space +
	 * slotDirectory indicator + numberofSlotDirectory+freeSpacePointer(int)
	 */

	void* new_data;
	int record_size;

	parseData(recordDescriptor,&record_size,data,new_data);

	//Append record to the new page
	RC result = writeRecordToPage(fileHandle,new_data,-1,record_size,rid);

	free(new_data);
	return result;

}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid, void *data) {
    /*
     * Read record by given RID - pageNUm and slotnum.
     * 1. Read page by given pageNum. If fail, return -1
     * 2. Get pageinfo from the rightside of file. If pageinfo.slotCounter < rid.slotNum, return -1
     * 3. Use the slotNum to access slot directory, and check offset, if -1, return -1.
     * 4. Otherwise, use slotdirecotry.offset and length to access the data.
     * 5. Get data, convert it back to the original_data format, nullFieldIndicator + actual data, get rid of fieldsize and offsetIndicator
     * 6. Return original_data
     */

	void *page = malloc(PAGE_SIZE);

	if(fileHandle.readPage(rid.pageNum,page) == 0){
		PAGEINFO pginfo;
		memcpy(&pginfo, (char *) page + PAGE_SIZE - sizeof(PAGEINFO), sizeof(PAGEINFO));

		//If there is enough slot directory
		if(pginfo.slotCounter >= rid.slotNum){
			SLOTDIRECTORY slotDir;
			memcpy(&slotDir,(char *) page + PAGE_SIZE - sizeof(PAGEINFO) - rid.slotNum*sizeof(SLOTDIRECTORY), sizeof(SLOTDIRECTORY));

			//Check if the record still exist
			if(slotDir.offset >= 0){
				// Access data by slot offset

				void *formed_data = malloc(slotDir.length);
				memcpy(formed_data, (char *)page + slotDir.offset, slotDir.length);
				short int fieldSize;
				memcpy(&fieldSize, (char *)formed_data, sizeof(short int));

				//check tombstone
				if(fieldSize == -1){
					RID new_rid;
					memcpy(&new_rid,(char*)formed_data+sizeof(short int),sizeof(RID));
					free(page);
					free(formed_data);
					return readRecord(fileHandle,recordDescriptor,new_rid,data);
				}

				//if not tombstone
				int nullFieldIndicatorSize = ceil((double)fieldSize/CHAR_BIT);

				//calculate new data size = formated data size - sizeof(fieldSize) - sizeof(offsetIndicator)
				int newDataSize = slotDir.length - sizeof(short int) - fieldSize*sizeof(unsigned short int);

				// First, copy nullFieldIndicator into new data
				memcpy(data,(char *)formed_data + sizeof(short int), nullFieldIndicatorSize);
				// Then, copy the stored data into new data
				memcpy((char *)data + nullFieldIndicatorSize,
						(char *)formed_data + sizeof(short int) + nullFieldIndicatorSize + fieldSize*(sizeof(short int)),
						newDataSize-nullFieldIndicatorSize);
				//free memory
				free(page);
				free(formed_data);

				return 0;
			}
			//printf("Slot deleted");
			free(page);
			return -1;
		}
		//printf("Slot doesn't exist");
		free(page);
		return -1;
	}
	//printf("Page doesn't exist");
	free(page);
	return -1;
}

RC RecordBasedFileManager::printRecord(const vector<Attribute> &recordDescriptor, const void *data) {
	/*
     * Print record by the given data
     * 1.Get size of field from recrodDes, and Get the nullfieldIndicator from data.
     * 2.loop through nullfieldIndicator, and use the recordDes to print data
     * 3.Check types, and attr.name to print the formed print message
	*/
	if(data == NULL){
		return -1;
	}

	int offset = 0;
	short int fieldSize = recordDescriptor.size();
	int nullFieldsIndicatorSize = ceil((double)fieldSize / CHAR_BIT);
	unsigned char *nullFieldsIndicator = (unsigned char *) malloc(nullFieldsIndicatorSize);
	memcpy(nullFieldsIndicator, data, nullFieldsIndicatorSize);
	offset+=nullFieldsIndicatorSize;

	bool isNull = false;
	void *buffer;
	bool charUse =false;

	for(int i = 0; i< fieldSize; i++){
		isNull = nullFieldsIndicator[i/8] & 1<<(7-i%8);
		cout<<recordDescriptor[i].name<< " : ";
		string s ="";
		if(!isNull){
			switch(recordDescriptor[i].type){
				//for TypeVarChar, the incoming format is namelenght + namestr
				case TypeVarChar:
					charUse= true;
					//get name length
					int nameLength;
					nameLength = 0;
					memcpy(&nameLength,(char *)data + offset, sizeof(int));
					offset += sizeof(int);

					/*
					for(int w = 0;w<nameLength;++w){
						s+=(char*)data+offset+w;
					}
					offset += nameLength;*/

					//get name string
					buffer = malloc(nameLength);
					memcpy(buffer,(char *)data + offset, nameLength);
					((char *)buffer)[nameLength] = '\0';
					offset+=nameLength;
					printf("%s",buffer);

					break;

				case TypeInt:
					int integer;
					memcpy(&integer,(char *)data + offset, sizeof(int));
					cout<<integer;
					offset += sizeof(int);
					break;

				case TypeReal:
					float real;
					memcpy(&real,(char *)data + offset, sizeof(float));
					printf("%f",real);
					offset += sizeof(float);
					break;
				}

		}
		else{
			cout<< "NULL";
		}
		cout<<"    ";
	}
    cout<<endl;
    if(charUse){free(buffer);}
    free(nullFieldsIndicator);
	return -1;
}

RC RecordBasedFileManager::deleteRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid){
	/*
	 * Delete Record procedures:
	 * 1. Use RID get page info, then get slot directory info
	 * 2. Use slot directory info to get the real data
	 * 3. THe size to move is firstFreePage - offset - length
	 * 4. Copy the memory from page + offset+length to page + offset, update the slotdir,update pageinfo
	 * 5. Write the page back
	 */
	void *page = malloc(PAGE_SIZE);
	void *newPage= malloc(PAGE_SIZE);

	if(fileHandle.readPage(rid.pageNum,page) == 0){
		PAGEINFO pginfo;
		memcpy(&pginfo, (char *) page + PAGE_SIZE - sizeof(PAGEINFO), sizeof(PAGEINFO));
		if(rid.slotNum <= pginfo.slotCounter){
			SLOTDIRECTORY slotDir;
			memcpy(&slotDir,(char *) page + PAGE_SIZE - sizeof(PAGEINFO) - rid.slotNum*sizeof(SLOTDIRECTORY), sizeof(SLOTDIRECTORY));
			if(slotDir.offset >= 0){
				void *record = malloc(slotDir.length);
				//get record
				memcpy(record, (char *) page + slotDir.offset, slotDir.length);
				//Check tombstone
				short int fieldSize;
				memcpy(&fieldSize, (char*)record, sizeof(short int));
				//check tombstone
				if(fieldSize == -1){
					RID new_rid;
					memcpy(&new_rid,(char*)record+sizeof(short int),sizeof(RID));
					return deleteRecord(fileHandle,recordDescriptor,new_rid);
				}


				// old page = dataBefore + deleteData+ dataAfter + freeSpace + slotDirectory + pageInfo
				memcpy((char*)newPage,(char*)page,slotDir.offset);
				memcpy((char*)newPage+slotDir.offset,(char*)page+slotDir.offset+slotDir.length,pginfo.firstFreeSpace-slotDir.offset-slotDir.length);

				//If not tombstone

				//update slot Directory
				SLOTDIRECTORY tmp;
				for(int i = 1; i<= pginfo.slotCounter; i++){
					//Get slot dic
					memcpy(&tmp,(char *) page + PAGE_SIZE - sizeof(PAGEINFO) - i*sizeof(SLOTDIRECTORY), sizeof(SLOTDIRECTORY));
					if(tmp.offset>slotDir.offset){
						tmp.offset = tmp.offset - slotDir.length;
					}
					// write it back to page
					memcpy((char *) newPage + PAGE_SIZE - sizeof(PAGEINFO) - i*sizeof(SLOTDIRECTORY),&tmp, sizeof(SLOTDIRECTORY));
				}
				//update pageinfo
				//pginfo.slotCounter -= 1;
				pginfo.firstFreeSpace = pginfo.firstFreeSpace-slotDir.length;

				//update slotDir
				slotDir.offset=-1;
				slotDir.length=0;

				//write slotDir back to page
				memcpy((char *) newPage + PAGE_SIZE - sizeof(PAGEINFO) - rid.slotNum*sizeof(SLOTDIRECTORY),&slotDir, sizeof(SLOTDIRECTORY));
				//write pageInfo back to page
				memcpy((char *) newPage + PAGE_SIZE - sizeof(PAGEINFO), &pginfo,  sizeof(PAGEINFO));
				//write page back
				//cout<<"The original page size"<<pginfo.firstFreeSpace<<endl;
				RC rc = fileHandle.writePage(rid.pageNum,newPage);
				rc = fileHandle.readPage(rid.pageNum,page);
				memcpy(&pginfo, (char *) page + PAGE_SIZE - sizeof(PAGEINFO), sizeof(PAGEINFO));
				//cout<<"The after free page size"<<pginfo.firstFreeSpace<<endl;
				memcpy(&slotDir,(char *) page + PAGE_SIZE - sizeof(PAGEINFO) - rid.slotNum*sizeof(SLOTDIRECTORY), sizeof(SLOTDIRECTORY));
				//cout<<"The slot after delete "<<slotDir.offset<<" : "<<slotDir.length<<endl;

				free(page);
				free(newPage);
				free(record);
				return rc;
			}
			return -1;//Already deleted
		}
		return -1;//Slot doesn't exist
	}
	return -1;//can't open the page
}

RC RecordBasedFileManager::updateRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, const RID &rid){
	/*
	 * Update record procedures:
	 * 1. Check if the record exist.
	 * 2. If exist, save it, and delete it from page
	 * 3. Check if the record is tombstone
	 * 4. If not, Check if there is enough space for this page. If there is, just append it.
	 * 	  Else, create a tombstone with target rid, and append it in the end of page
	 * 5. If it is tombstone, then get the rid, and use it to call updateRecord again.
	 */

	void *page = malloc(PAGE_SIZE);
	if(fileHandle.readPage(rid.pageNum,page) == 0){
		PAGEINFO pginfo;
		memcpy(&pginfo, (char *) page + PAGE_SIZE - sizeof(PAGEINFO), sizeof(PAGEINFO));
		if(rid.slotNum <= pginfo.slotCounter){
			SLOTDIRECTORY slotDir;
			memcpy(&slotDir,(char *) page + PAGE_SIZE - sizeof(PAGEINFO) - rid.slotNum*sizeof(SLOTDIRECTORY), sizeof(SLOTDIRECTORY));
			if(slotDir.offset >= 0){
				//get record from page
				void*record = malloc(slotDir.length);
				memcpy(record, (char *) page + slotDir.offset, slotDir.length);

				//Check tombstone
				short int fieldSize;
				memcpy(&fieldSize, (char*)record, sizeof(short int));

				RID new_rid;
				void* new_data;
				int record_size;
				parseData(recordDescriptor,&record_size,data,new_data);


				if(fieldSize >= 0){ // if not tombstone
					//delete the original record
					deleteRecord(fileHandle,recordDescriptor, rid);
					//update the page
					fileHandle.readPage(rid.pageNum,page);

					memcpy(&pginfo, (char *) page + PAGE_SIZE - sizeof(PAGEINFO), sizeof(PAGEINFO));
					//Check free space
					if(hasFreeSpace(fileHandle,rid.pageNum,record_size)){
						//Append the data, update slotdirctory, and page info
						memcpy((char*)page+pginfo.firstFreeSpace,new_data,record_size);
						//update slotdir
						slotDir.offset=pginfo.firstFreeSpace;
						slotDir.length = record_size;
						memcpy((char*)page+PAGE_SIZE - sizeof(PAGEINFO) - rid.slotNum*sizeof(SLOTDIRECTORY),&slotDir,sizeof(slotDir));
						//update pginfo
						pginfo.firstFreeSpace +=record_size;
						memcpy((char *) page + PAGE_SIZE - sizeof(PAGEINFO),&pginfo, sizeof(PAGEINFO));
					}
					else{ // no free space
						writeRecordToPage(fileHandle,new_data,rid.pageNum,record_size,new_rid);
						//create tombstone and append it into page
						void*tombstone = malloc(sizeof(short int)+sizeof(RID)); //field size + new_rid
						fieldSize = -1;
						memcpy(tombstone,&fieldSize,sizeof(fieldSize));
						memcpy((char*)tombstone+sizeof(fieldSize), &new_rid,sizeof(RID));
						memcpy((char*)page+pginfo.firstFreeSpace,tombstone, sizeof(fieldSize)+sizeof(RID));
						//update slot directory
						slotDir.offset=pginfo.firstFreeSpace;
						slotDir.length=sizeof(fieldSize)+sizeof(RID);
						//write it back to page
						memcpy((char*)page+PAGE_SIZE - sizeof(PAGEINFO) - rid.slotNum*sizeof(SLOTDIRECTORY),&slotDir,sizeof(slotDir));
						//update pginfo
						pginfo.firstFreeSpace += sizeof(fieldSize)+sizeof(RID);
						memcpy((char *) page + PAGE_SIZE - sizeof(PAGEINFO),&pginfo, sizeof(PAGEINFO));
						free(tombstone);
					}
					RC result = fileHandle.writePage(rid.pageNum,page);
					free(record);
					free(page);
					return result;
					}
				// if tombstone
				memcpy(&new_rid,(char*)record+sizeof(short int),sizeof(RID));
				free(page);
				free(record);
				return updateRecord(fileHandle,recordDescriptor,data,new_rid);
			}
			free(page);
			//cout<<"SLot deleted"<<endl;
			return -1; //Slot deleted
		}
		free(page);
		//cout<<"Slot doesn't exitst"<<rid.slotNum<<endl;
		return -1; //Slot doesn't exist
	}
	//cout<<"CAn't open page "<<rid.pageNum<<endl;
	free(page);
	return -1;//can't open page
}

RC RecordBasedFileManager::readAttribute(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid, const string &attributeName, void *data){
	/*
	 * ReadAttribute procedure
	 * 1. Find the page, use the page info and slot dirctory info to get record
	 * 2. loop the record Descriptor and nullsFieldIndicator
	 * 3. Compare the attri name, if same and not null, get data and return 0
	 * 4. If cant't find it,return -1
	 */
	void *page = malloc(PAGE_SIZE);
	if(fileHandle.readPage(rid.pageNum,page) == 0){
		PAGEINFO pginfo;
		memcpy(&pginfo, (char *) page + PAGE_SIZE - sizeof(PAGEINFO), sizeof(PAGEINFO));
		if(rid.slotNum <= pginfo.slotCounter){
			SLOTDIRECTORY slotDir;
			memcpy(&slotDir,(char *) page + PAGE_SIZE - sizeof(PAGEINFO) - rid.slotNum*sizeof(SLOTDIRECTORY), sizeof(SLOTDIRECTORY));
			if(slotDir.offset >= 0){
				//get record from page
				void*record = malloc(slotDir.length);
				memcpy(record, (char *) page + slotDir.offset, slotDir.length);

				//Check tombstone
				short int fieldSize;
				memcpy(&fieldSize, (char*)record, sizeof(short int));

				if(fieldSize == -1){ // if tombstone
					RID new_rid;
					memcpy(&new_rid,(char*)record+sizeof(short int),sizeof(RID));
					free(page);
					free(record);
					return readAttribute(fileHandle,recordDescriptor,new_rid,attributeName,data);
				}

				//if not tombstone check if fieldSize equal with recordDescriptor size
				if(fieldSize == recordDescriptor.size()){
					//get nullfieldIndicator
					int nullFieldsIndicatorSize = ceil((double)fieldSize / CHAR_BIT);
					unsigned char *nullFieldsIndicator = (unsigned char *) malloc(nullFieldsIndicatorSize);
					memcpy(nullFieldsIndicator, (char*)record+sizeof(short int), nullFieldsIndicatorSize);
					//get fieldOffsetIndicator
					unsigned short int fieldOffsetIndicatorSize = fieldSize * sizeof(unsigned short int);
					unsigned short int *fieldOffsetIndicator = (unsigned short int *)malloc(fieldOffsetIndicatorSize);
					memcpy(fieldOffsetIndicator,(char*)record+sizeof(short int)+ nullFieldsIndicatorSize,fieldOffsetIndicatorSize);
					//loop recordDescriptor
					bool isNull;
					for(int i = 0; i < fieldSize; i++){
						//if not null
						isNull = nullFieldsIndicator[i/8] & 1<<(7-i%8);
						if(!isNull && recordDescriptor[i].name == attributeName){
							//use this i to find the offset of attribute from fieldOffsetIndicator
							int offset = fieldOffsetIndicator[i];

							//put nullindicator
							unsigned char *nullsIndicator = (unsigned char *) malloc(1);
							memset(nullsIndicator, 0,1);
							memcpy((char*)data,(char*)nullsIndicator,1);
							free(nullsIndicator);

							if(recordDescriptor[i].type == TypeVarChar){
								int nameLength;
								memcpy(&nameLength,(char*)record + offset, sizeof(int));
								//data = malloc(sizeof(int)+nameLength);
								memcpy((char*)data+1,(char*)record+offset,sizeof(int)+nameLength);
							}
							else{
								//data = malloc(sizeof(int));
								memcpy((char*)data+1,(char*)record+offset,sizeof(int)); //sizeof(int) and float are same
							}

							free(record);
							free(nullFieldsIndicator);
							free(fieldOffsetIndicator);
							free(page);
							return 0;
						}
						if(isNull && recordDescriptor[i].name == attributeName){
							memset(data, 128,1);
						}
					}
					free(record);
					free(nullFieldsIndicator);
					free(fieldOffsetIndicator);
				    free(page);
				    //cout<<"can't find attribute"<<endl;
					return -1;//doesn't find attribute
				}
				free(record);
				free(page);
				//cout<<"name is "<<attributeName<<endl;
				//cout<<"Not same record"<<endl;
				return -1;//Not the same record to read
			}
			free(page);
			//cout<<"slot deleted"<<endl;
			return -1;
		}
		free(page);
		//cout<<"Can't find slot"<<endl;
		return -1;
	}
	free(page);
	//cout<<"Can't find page"<<endl;
	return -1;
}

RC RecordBasedFileManager::writeRecordToPage(FileHandle &fileHandle,void *data,int pageNum, int record_size,RID &rid){
	/*
	 * Write the record with correct format into the page with correct format
	 * 1.Open the file and check the pageInfo to get the first free pointer and slotNumber
	 * 2.Loop slotDirectory by slotNumber, if offset = -1, use this slot as slot directory
	 * 3.Otherwise, Create a new SlotDirectory with offset= firstFreePointer, and SlotNumber=slotNumber+1;
	 * 4.Move the data into offset, slotNumber++; firstFreePointer = offset+record_size
	 * 5.Write the page back
	 */


	//Check if the given page has enough space to write record

	if(!hasFreeSpace(fileHandle,pageNum,record_size)){
		pageNum = fileHandle.getNumberOfPages()-1;
		bool findPage = false;
		while(pageNum>=0){
				if(hasFreeSpace(fileHandle,pageNum,record_size)){
					findPage = true;
					break;
				}
				pageNum--;
		}
			//If there is no page has free space, then create a new one
		if(!findPage){
				pageNum = createNewPage(fileHandle);
		}
	}


	void *page = malloc(PAGE_SIZE);
	if(fileHandle.readPage(pageNum,page) == 0){
		rid.pageNum = pageNum;
		PAGEINFO pginfo;
		memcpy(&pginfo,(char *)page+ PAGE_SIZE - sizeof(PAGEINFO), sizeof(PAGEINFO));
		SLOTDIRECTORY target_slot;
		bool findEmpty = false;
		//loop the slot directory to find the empty one
		for(int i = 1;i<=pginfo.slotCounter;i++){
			SLOTDIRECTORY slotDir;
			memcpy(&slotDir,(char *)page + PAGE_SIZE - sizeof(PAGEINFO) - (i)*sizeof(SLOTDIRECTORY),sizeof(SLOTDIRECTORY));
			// If this slot is empty , move the data into this slot
			if(slotDir.offset == -1){
				slotDir.offset = pginfo.firstFreeSpace;
				slotDir.length = record_size;
				rid.slotNum = i;
				target_slot = slotDir;
				findEmpty = true;
				break;
			}
		}
		if(!findEmpty){
			//if not find the previous empty slot, then create a new one
			target_slot.offset = pginfo.firstFreeSpace;
			target_slot.length = record_size;
			rid.slotNum = pginfo.slotCounter+1;
			pginfo.slotCounter++;
		}

		//now move the data to the page by the guide of target_slot directory;
		memcpy((char *)page+target_slot.offset, data,record_size);

		//update pageInfo, and move it back to page
		pginfo.firstFreeSpace += record_size;

		memcpy((char *)page+ PAGE_SIZE - sizeof(PAGEINFO), &pginfo, sizeof(PAGEINFO));

		//update slot directory in page
		memcpy((char *)page+ PAGE_SIZE - sizeof(PAGEINFO) - rid.slotNum * sizeof(SLOTDIRECTORY), &target_slot, sizeof(SLOTDIRECTORY));

		//write page back
		RC rc = fileHandle.writePage(rid.pageNum,page);

		//Free memory
		free(page);
		return rc;
	}
	//cout<<"write record to page doesn't exist "<<endl;
	free(page);
	return -1;

}

PageNum RecordBasedFileManager::createNewPage(FileHandle &fileHandle){
	/*
	 * Create a new formatted page in this file
	 * 1.Create PageInfo with value 0 and 0.
	 * 2.Put it in the right side of page
	 * 3.Return the created page num
	 */

	void *page = malloc(PAGE_SIZE);
	PAGEINFO pginfo;
	pginfo.firstFreeSpace = 0;
	pginfo.slotCounter = 0;
	memcpy((char*)page + PAGE_SIZE - sizeof(PAGEINFO),&pginfo, sizeof(PAGEINFO));
	fileHandle.appendPage(page);
	free(page);
	return fileHandle.getNumberOfPages()-1;
}

bool RecordBasedFileManager::hasFreeSpace(FileHandle &fileHandle,unsigned int page_num,int record_size){
	/*
	 * Check if the given page_num have enough space for the incoming record
	 * 1. Check if page exist or not. If not return false;
	 * 2. Check the right side of page to get freespace pointer
	 * 3. Then get the number of slotdirectory
	 * 4. If the freeSpacepointer + sizeof(pageInfo) + numSlotDir* sizeof(slot) + record_size > page_size
	 * 5. Return false;
	 */

	void *page = malloc(PAGE_SIZE);
	if(page_num>=0 && fileHandle.readPage(page_num,page) == 0){
		PAGEINFO pgInfo;
		memcpy(&pgInfo,(char*)page+PAGE_SIZE-sizeof(PAGEINFO),sizeof(PAGEINFO));
		free(page);
		if(pgInfo.firstFreeSpace + (pgInfo.slotCounter+1)*sizeof(SLOTDIRECTORY)+ sizeof(PAGEINFO)+ record_size < PAGE_SIZE){
			return true;
		}
		return false;
	}
	free(page);
	return false;
}

void RecordBasedFileManager::parseData(const vector<Attribute> &recordDescriptor, int* record_size,const void *original_data, void* &new_data){
	// new record format:
	// newData = fieldSize + nullFieldIndicator + fieldOffsetIndicatorSize + realData

	int offset = 0;

	//get record size by ceil()
	short int recordDes_size = recordDescriptor.size();

	//get null fields indicator
	int nullFieldsIndicatorSize = ceil((double)recordDes_size / CHAR_BIT);
	unsigned char *nullFieldsIndicator = (unsigned char *) malloc(nullFieldsIndicatorSize);
	memcpy(nullFieldsIndicator, original_data, nullFieldsIndicatorSize);
	offset += nullFieldsIndicatorSize;

	//Create fieldoffsetIndicator
	unsigned short int fieldOffsetIndicatorSize = recordDes_size * sizeof(unsigned short int);
	unsigned short int *fieldOffsetIndicator = (unsigned short int *)malloc(fieldOffsetIndicatorSize);

	bool isNull;
	//Now fill the fieldOffsetIndicator
	for(int i = 0; i<recordDes_size; i++){
		//if not null
		isNull = nullFieldsIndicator[i/8] & 1<<(7-i%8);
		fieldOffsetIndicator[i] = offset + fieldOffsetIndicatorSize + sizeof(short int);
		if(!isNull){
			switch(recordDescriptor[i].type){
			//for TypeVarChar, the incoming format is namelenght + namestr
			case TypeVarChar:
				int nameLength;
				memcpy(&nameLength,(char *)original_data + offset, sizeof(int));
				offset += (sizeof(int)+nameLength);
				//cout<<"Put var char with length: " <<sizeof(int) + nameLength<<endl;
				break;
			case TypeInt:
				//cout<<"Put int " << sizeof(int) <<endl;
				offset+= sizeof(int);
				break;
			case TypeReal:
				//cout<<"Put real!"<<sizeof(float)<<endl;
				offset+= sizeof(float);
				break;
			}
		}
	}

	//now offset stands for the size of original data
	//The size of new_data should equal the size of original data + the size of field size + the size of offsetfieldindicator
	new_data = malloc(offset+sizeof(short int)+fieldOffsetIndicatorSize);
	//cout<<"The size of nullfield indicator : "<<nullFieldsIndicatorSize<<endl;
	//copy the field size into new_data
	memcpy(new_data,&recordDes_size,sizeof(short int));
	//copy the nullField indicator into new_data
	memcpy((char*)new_data+sizeof(short int),original_data, nullFieldsIndicatorSize);
	//Copy the offsetfield indicator into new_data
	memcpy((char*)new_data+sizeof(short int)+nullFieldsIndicatorSize, fieldOffsetIndicator,fieldOffsetIndicatorSize);
	//Copy the original real data into new_data
	memcpy((char*)new_data+sizeof(short int)+nullFieldsIndicatorSize+fieldOffsetIndicatorSize,
			(char *)original_data+nullFieldsIndicatorSize, offset-nullFieldsIndicatorSize);

	//Free all the used memory
	free(fieldOffsetIndicator);
	free(nullFieldsIndicator);
	*record_size = offset+sizeof(short int)+fieldOffsetIndicatorSize;
}

int RecordBasedFileManager::calculateRecordSize(const vector<Attribute> &recordDescriptor,const void *original_data){
		int offset = 0;

		//get record size by ceil()
		short int recordDes_size = recordDescriptor.size();

		//get null fields indicator
		int nullFieldsIndicatorSize = ceil((double)recordDes_size / CHAR_BIT);
		unsigned char *nullFieldsIndicator = (unsigned char *) malloc(nullFieldsIndicatorSize);
		memcpy(nullFieldsIndicator, original_data, nullFieldsIndicatorSize);
		offset += nullFieldsIndicatorSize;

		bool isNull;
		//Now fill the fieldOffsetIndicator
		for(int i = 0; i<recordDes_size; i++){
			//if not null
			isNull = nullFieldsIndicator[i/8] & 1<<(7-i%8);

			if(!isNull){
				switch(recordDescriptor[i].type){
				//for TypeVarChar, the incoming format is namelenght + namestr
				case TypeVarChar:
					int nameLength;
					memcpy(&nameLength,(char *)original_data + offset, sizeof(int));
					offset += (sizeof(int)+nameLength);
					//cout<<"Put var char with length: " <<sizeof(int) + nameLength<<endl;
					break;
				case TypeInt:
					//cout<<"Put int " << sizeof(int) <<endl;
					offset+= sizeof(int);
					break;
				case TypeReal:
					//cout<<"Put real!"<<sizeof(float)<<endl;
					offset+= sizeof(float);
					break;
				}
			}
		}

		free(nullFieldsIndicator);
		return offset;
}

RC RecordBasedFileManager::scan(FileHandle &fileHandle,
      const vector<Attribute> &recordDescriptor,
      const string &conditionAttribute,
      const CompOp compOp,                  // comparision type such as "<" and "="
      const void *value,                    // used in the comparison
      const vector<string> &attributeNames, // a list of projected attributes
      RBFM_ScanIterator &rbfm_ScanIterator){

	return rbfm_ScanIterator.InitialScanIterator(fileHandle,
			  recordDescriptor,
		      conditionAttribute,
			  compOp,                  // comparision type such as "<" and "="
		      value,                    // used in the comparison
		      attributeNames);
}

RC RBFM_ScanIterator::InitialScanIterator(FileHandle &fileHandle,
	      const vector<Attribute> &recordDescriptor,
	      const string &conditionAttribute,
	      const CompOp compOp,                  // comparision type such as "<" and "="
	      const void *value,                    // used in the comparison
	      const vector<string> &attributeNames // a list of projected attributes
	      ){

	//this->rbfm =  RecordBasedFileManager::instance();
	this->compOp = compOp;
	this->attributeNames = attributeNames;
	this->conditionAttribute = conditionAttribute;
	this->fileHandle = fileHandle;
	this->recordDescriptor = recordDescriptor;
	//this->current_value=malloc(PAGE_SIZE);
	//this->current_lengh=PAGE_SIZE;
	this->value = (char*)value;

	this->pre_rid.pageNum = 0;
	this->pre_rid.slotNum = 1;
	this->map;
	//map[position in attributeNames] = postion in recordDescriptor

	for(int j = 0;j<attributeNames.size();++j){
		this->map.push_back(-1);
	}

	int position = 0;
	for(int i = 0; i<recordDescriptor.size();++i){
		for(int w = 0; w<attributeNames.size();++w){
			if(recordDescriptor[i].name == attributeNames[w]){
				this->map[w] = i;
				break;
			}
		}
		if(conditionAttribute ==recordDescriptor[i].name ){this->attrPostion = i;}
	}
	return 0;
}

RC RBFM_ScanIterator::getNextRecord(RID &rid, void *data) {
	/* Get next record procedure
	 * 1. Get previous RID -> pageNum,slotNum
	 * 2. Check PageInfo.slotcounter, if have more available slot, go with this page, if not, go to next page
	 * 3. For each page, call the getNextRecordFromCurrentPage function
	 * 4. If find, return 0, else -1
	 */
	bool find = false;

	while(pre_rid.pageNum < fileHandle.getNumberOfPages()){
		if(getNextRecordFromCurrentPage(pre_rid,data) == 0){
			rid.pageNum = pre_rid.pageNum;
			rid.slotNum = pre_rid.slotNum;
			pre_rid.slotNum+=1;
			return 0;
		}
		pre_rid.pageNum += 1;
		pre_rid.slotNum = 1;
	}
	return RBFM_EOF;
}

RC RBFM_ScanIterator::close(){
	RecordBasedFileManager *rbfm =  RecordBasedFileManager::instance();
	rbfm->closeFile(fileHandle);
}

RC RBFM_ScanIterator::getNextRecordFromCurrentPage(RID &rid,void *data){
	/* Goal: get the next matched record in this current page after the given RID.
	 * 1. Loop through the slotDirectory and check all the slot, and use readAttribute to get the target attribute
	 * 		Detail: loop each slot, compare attributes with with conditionAttribute,
	 * 				if right, use readAttribute get each attributes by different arributeNames,append it into returned data,
	 * 				if all qualifed, then return the returned data
	 * 2. Compare the target attribute with the compare value with compOp.
	 * 3. If qualified, then return the rid and copy the data into it.
	 */
	void *page = malloc(PAGE_SIZE);
	RecordBasedFileManager *rbfm =  RecordBasedFileManager::instance();
	if(fileHandle.readPage(rid.pageNum,page) == 0){
		/*
		void*test = malloc(PAGE_SIZE);
		rbfm->readRecord(fileHandle,recordDescriptor,pre_rid,test);
		if(memcmp(test,current_value,current_lengh)==0){
			pre_rid.slotNum+=1;
		}
		cout<<"Check record!!!!!------------"<<endl;
		cout<<"This current len is "<<this->current_lengh<<endl;
		rbfm->printRecord(recordDescriptor,test);
		rbfm->printRecord(recordDescriptor,current_value);
		cout<<"-----------------------"<<endl;
		free(test);*/
		PAGEINFO pginfo;
		memcpy(&pginfo, (char *) page + PAGE_SIZE - sizeof(PAGEINFO), sizeof(PAGEINFO));
		//cout<<"Current page has "<<pginfo.slotCounter<<endl;
		//cout<<"Current page free soace "<<pginfo.firstFreeSpace<<endl;
		while(rid.slotNum <= pginfo.slotCounter){
			//check if the condition method work
			RC res;
			void*targetAtr = malloc(PAGE_SIZE);
			if(compOp!=NO_OP){

			Attribute target = recordDescriptor[this->attrPostion];


			res = rbfm->readAttribute(fileHandle,recordDescriptor,rid,conditionAttribute,targetAtr);

			if(res == -1){
				free(targetAtr);
				rid.slotNum+=1;
				continue;
			}

			if(!meetCondition(target,targetAtr)){
				free(targetAtr);
				rid.slotNum+=1;
				continue;
			}
			}
			free(targetAtr);
			// if work, copy the record into data
			int offset = 0;
			int find;
			//build nullsPointIndicator
			int size = attributeNames.size();
			int nullFieldsIndicatorSize = ceil((double)size/ CHAR_BIT);
			unsigned char *nullFieldsIndicator = (unsigned char *) malloc(nullFieldsIndicatorSize);
			memset( nullFieldsIndicator, 0 , nullFieldsIndicatorSize);
			offset+=nullFieldsIndicatorSize;
			//add data

			void*record = malloc(PAGE_SIZE);
			find =rbfm->readRecord(fileHandle,recordDescriptor,rid,record);
			find = getAttrData1(recordDescriptor, this->attributeNames,record , data);
			free(record);
			/*
			for(int i = 0; i<size;i++){
				Attribute atr = recordDescriptor[map[i]];
				void *attr = malloc(atr.length);
				res = rbfm->readAttribute(fileHandle,recordDescriptor,rid,attributeNames[i],attr);
				if(res==RBFM_EOF){
					find = false;
					break;
				}
				//check if this attr is null
				unsigned char *attrNullFieldsIndicator = (unsigned char *) malloc(1);
				memcpy(attrNullFieldsIndicator,(char*)attr,1);
				bool isNull = attrNullFieldsIndicator[0/8] & 1<<(7-0%8);
				if(isNull){
					nullFieldsIndicator[i/8] = (1<<(7-(i%8)));
					continue;
				}
				//if not null add the data
				switch(atr.type){
					case TypeVarChar:
						int nameLength;
						memcpy(&nameLength,(char*)attr+1,sizeof(int));
						memcpy((char*)data+offset,&nameLength,sizeof(int));
						offset += sizeof(int);
						memcpy((char*)data+offset,(char*)attr+1+sizeof(int),nameLength);
						offset += nameLength;
						break;
			    		case TypeInt:
						memcpy((char*)data+offset,(char*)attr+1,sizeof(int));
						offset += sizeof(int);
						break;
					case TypeReal:
						memcpy((char*)data+offset,(char*)attr+1,sizeof(float));
						offset += sizeof(float);
						break;
				}
				free(attr);
				free(attrNullFieldsIndicator);
			}*/
			if(find==-1){
				rid.slotNum+=1;
				continue;
			}
			/*
			//add nullIndicator in the beginning
			memcpy((char*)data,nullFieldsIndicator,nullFieldsIndicatorSize);
			//rbfm->readRecord(fileHandle,recordDescriptor,pre_rid,current_value);
			//this->current_lengh=rbfm->calculateRecordSize(recordDescriptor,current_value);*/
			free(page);

			free(nullFieldsIndicator);
			return 0;
		}
		free(page);
		return RBFM_EOF;
	}
	free(page);
	return RBFM_EOF;
}

/* Comparison Operator (NOT needed for part 1 of the project)
typedef enum { EQ_OP = 0, // no condition// =
           LT_OP,      // <
           LE_OP,      // <=
           GT_OP,      // >
           GE_OP,      // >=
           NE_OP,      // !=
           NO_OP       // no condition
 */

bool RBFM_ScanIterator::meetCondition(Attribute atr, void* data){
	if(this->compOp == NO_OP){
		return true;
	}
	int result = -1;
	string s_val = "";
	string s_data = "";

	switch(atr.type){
		case TypeVarChar:
			//evalutate the length
			//use memcmp to compare two typeVarChar
			int value_size;
			int data_size;
			memcpy(&value_size,(char*)value,sizeof(int));
			memcpy(&data_size,(char*)data+1,sizeof(int));


			for(int z = 0; z<value_size;++z){
				s_val+=*((char*)value+4+z);
			}
			for(int z = 0; z<value_size;++z){
				s_data+=*((char*)data+1+4+z);
			}

			if(s_data == s_val){result = 0;}
			if(s_data > s_val){result = 1;}
			if(s_data<s_val){result = -1;}

			break;

		case TypeInt:
			int value_int;
			int data_int;
			memcpy(&value_int,(char*)value,sizeof(int));
			memcpy(&data_int,(char*)data+1,sizeof(int));
			result = data_int - value_int;
			break;

		case TypeReal:
			float value_float;
			float data_float;
			memcpy(&value_float,(char*)value,sizeof(float));
			memcpy(&data_float,(char*)data+1,sizeof(float));
			//avoid the precision error
			float res = data_float - value_float;
			if(res>0){result=1;}
			if(res==0){result=0;};
			if(res<0){result=-1;};
			break;
	}

	switch(compOp){
		case EQ_OP:
			if(result == 0){return true;}
			break;
		case LT_OP:
			if(result < 0){return true;}
			break;
		case LE_OP:
			if(result<=0){return true;}
			break;
		case GT_OP:
			if(result>0){return true;}
			break;
		case GE_OP:
			if(result>=0){return true;}
			break;
		case NE_OP:
			if(result!=0){return true;}
			break;
	}
	return false;
}


