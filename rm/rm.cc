
#include "rm.h"

RelationManager* RelationManager::instance()
{
    static RelationManager _rm;
    return &_rm;
}

RelationManager::RelationManager()
{
}

RelationManager::~RelationManager()
{
}

// Record Descriptor for Table
void createRecordDescriptorForTable(vector<Attribute> &recordDescriptor) {
	Attribute attr;
	attr.name = "table-id";
	attr.type = TypeInt;
	attr.length = (AttrLength) 4;
	recordDescriptor.push_back(attr);

	attr.name = "table-name";
	attr.type = TypeVarChar;
	attr.length = (AttrLength) 50;
	recordDescriptor.push_back(attr);

	attr.name = "file-name";
	attr.type = TypeVarChar;
	attr.length = (AttrLength) 50;
	recordDescriptor.push_back(attr);
}

// Record Descriptor for Table
void createRecordDescriptorForColumns(vector<Attribute> &recordDescriptor) {
	Attribute attr;
	attr.name = "table-id";
	attr.type = TypeInt;
	attr.length = (AttrLength) 4;
	recordDescriptor.push_back(attr);

	attr.name = "column-name";
	attr.type = TypeVarChar;
	attr.length = (AttrLength) 50;
	recordDescriptor.push_back(attr);

	attr.name = "column-type";
	attr.type = TypeInt;
	attr.length = (AttrLength) 4;
	recordDescriptor.push_back(attr);

	attr.name = "column-length";
	attr.type = TypeInt;
	attr.length = (AttrLength) 4;
	recordDescriptor.push_back(attr);

	attr.name = "column-position";
	attr.type = TypeInt;
	attr.length = (AttrLength) 4;
	recordDescriptor.push_back(attr);
}

void createTableRecord(const int table_id, const string table_name, const string file_name, void * record){
	int offset = 0;
	//Put nullsIndicator
	int null = 0;
	memcpy((char*)record+offset,&null,1);
	offset+=1;

	//put tableid
	memcpy((char*)record+offset,&table_id,sizeof(int));
	offset+=sizeof(int);

	//put length + table-name
	int charLen = table_name.size();
	memcpy((char*)record+offset,&charLen,sizeof(int));
	offset+=sizeof(int);
	memcpy((char*)record+offset,table_name.c_str(),charLen);
	offset+=charLen;

	//put length = file-name
	charLen = file_name.size();
	memcpy((char*)record+offset,&charLen,sizeof(int));
	offset+=sizeof(int);
	memcpy((char*)record+offset,file_name.c_str(),charLen);
	offset+=charLen;
}

void createColumnRecord(const int table_id, const string column_name, const int column_type, const int column_length,
		const int column_position, void * record){

		int offset = 0;
		int null = 0;
		//Put nullsIndicator
		memcpy((char *)record+offset,&null,1);
		offset+=1;

		//put table-id
		memcpy((char *)record+offset,&table_id,sizeof(int));
		offset+=sizeof(int);

		//copy column-name
		int size = column_name.size();
		memcpy((char *)record+offset,&size,sizeof(int));
		offset+=sizeof(int);

		memcpy((char *)record+offset,column_name.c_str(),size);
		offset+=size;

		//copy  type
		memcpy((char *)record+offset,&column_type,sizeof(int));
		offset+=sizeof(int);

		//copy attribute length
		memcpy((char *)record+offset,&column_length,sizeof(int));
		offset+=sizeof(int);

		//copy position
		memcpy((char *)record+offset,&column_position,sizeof(int));
		offset+=sizeof(int);
}

int RelationManager::findTableId(const string &tableName){
	RecordBasedFileManager *rbfm=RecordBasedFileManager::instance();
	RID rid;
	void *data =malloc(PAGE_SIZE);
	void *value =malloc(PAGE_SIZE);
	int size = tableName.size();
	memcpy((char*)value,tableName.c_str(),size);

	RM_ScanIterator table_ScanIterator;
	vector<string> attrname;

	attrname.push_back("table-name");
	attrname.push_back("table-id");

	int res = -1;

	scan("Tables","table-name",NO_OP,NULL,attrname,table_ScanIterator);

	while(table_ScanIterator.getNextTuple(rid,data)!=RM_EOF){
		//GET table id

		if(memcmp((char*)data+1+4,value,size)==0){
			memcpy(&res,(char *)data+1+4+size,sizeof(int));
			table_ScanIterator.close();
			free(value);
			free(data);
			return res;
		}

	}
	table_ScanIterator.close();

	free(value);
	free(data);

	return res;
}

int RelationManager::updateTableId(){
	/*
	 * Operation:
	 * 1 means plus 1.
	 * 0 means return current counter
	 * -1 means minus 1.
	 */
	/*
	vector<Attribute> recordDescriptor;
	Attribute attr;
	attr.name = "tableid-counter";
	attr.type = TypeInt;
	attr.length = (AttrLength) 4;
	recordDescriptor.push_back(attr);

	void *record = malloc(PAGE_SIZE);

	RecordBasedFileManager *rbfm=RecordBasedFileManager::instance();
	FileHandle fileHandle;
	rbfm->openFile("Tables",fileHandle);
	rbfm->readAttribute(fileHandle,recordDescriptor,this->tableid_rid,attr.name,record);

	int tableid_counter;
	memcpy(&tableid_counter,(char*)record+1,sizeof(int));
	tableid_counter += operation;
	if(operation!=0){
		memcpy((char*)record+1,&tableid_counter,sizeof(int));
		rbfm->updateRecord(fileHandle,recordDescriptor,record,this->tableid_rid);
	}
	return tableid_counter;*/

		RM_ScanIterator rm_ScanIterator;
		RID rid;
		char *data=(char *)malloc(PAGE_SIZE);
		vector<string> attrname;
		attrname.push_back("table-id");
		int count = -1;

		void *v = malloc(1);
		if( scan("Tables","",NO_OP,v,attrname,rm_ScanIterator)==0 ){
			while(rm_ScanIterator.getNextTuple(rid,data)!=RM_EOF){
				count++;
			}

			free(data);
			rm_ScanIterator.close();

			free(v);
			return count+1;
		}

		return -1;

}

RC RelationManager::printSystemCatalog(){

		cout<<"System table--------------------------"<<endl;
		RecordBasedFileManager *rbfm=RecordBasedFileManager::instance();
		RID rid;
		FileHandle fileHandle;
		RM_ScanIterator table_ScanIterator;
		vector<string> attrname;

		rbfm->openFile("Tables",fileHandle);
		vector<Attribute> recordDescriptor1;
		createRecordDescriptorForTable(recordDescriptor1);

		void *data=(char*)malloc(PAGE_SIZE);

		attrname.push_back("table-id");
		attrname.push_back("table-name");
		attrname.push_back("file-name");
		int res = -1;
		scan("Tables","table-name",NO_OP,NULL,attrname,table_ScanIterator);
		while(table_ScanIterator.getNextTuple(rid,data)!=RM_EOF){
				//GET table id
				cout<<"Rid is: "<<rid.pageNum<<" : "<<rid.slotNum<<endl;
				rbfm->printRecord(recordDescriptor1,data);
			}

		rbfm->closeFile(fileHandle);
		table_ScanIterator.close();


		cout<<"System columns--------------------------------"<<endl;
		rbfm->openFile("Columns",fileHandle);
		vector<Attribute> recordDescriptor;
		createRecordDescriptorForColumns(recordDescriptor);
		//construct attrnames
		vector<string> attrNames;
		attrNames.push_back("table-id");
		attrNames.push_back("column-name");
		attrNames.push_back("column-type");
		attrNames.push_back("column-length");
		attrNames.push_back("column-position");

		RM_ScanIterator column_ScanIterator;
		scan("Columns","table-id",NO_OP,NULL,attrNames,column_ScanIterator);



		while(column_ScanIterator.getNextTuple(rid,data)!=RM_EOF){
			cout<<"Rid is : "<<rid.pageNum<<" : "<<rid.slotNum<<endl;
			rbfm->printRecord(recordDescriptor,data);
		}
		column_ScanIterator.close();
		return 0;
}

RC RelationManager::createCatalog()
{
	RecordBasedFileManager *rbfm=RecordBasedFileManager::instance();
	RID rid;
	FileHandle fileHandle;
	vector<Attribute> recordDescriptor;
	RC res;

	if(rbfm->createFile("Tables")==0){
		void *record = malloc(PAGE_SIZE);
		res = rbfm->openFile("Tables",fileHandle);
		//insert table info
		createRecordDescriptorForTable(recordDescriptor);
		createTableRecord(1,"Tables","Tables",record);
		assert( res == 0 && "Open table file should not fail");
		res = rbfm->insertRecord(fileHandle,recordDescriptor,record, rid);
		assert( res == 0 && "insert table should not fail");


		createTableRecord(2,"Columns","Columns",record);
		res = rbfm->insertRecord(fileHandle,recordDescriptor,record, rid);
		assert( res == 0 && "insert column table should not fail");
		res = rbfm->closeFile(fileHandle);
		assert( res == 0 && "Close table should not fail");
		free(record);
	}
	else{
		return -1;
	}

	vector<Attribute> recordDescriptor2;
	if(rbfm->createFile("Columns")==0){
			res = rbfm->openFile("Columns",fileHandle);
			assert( res == 0 && "Open Columns file should not fail");
			createRecordDescriptorForColumns(recordDescriptor2);
			for(int i = 0; i<recordDescriptor.size();i++){
							void *record = malloc(PAGE_SIZE);
							createColumnRecord(1,recordDescriptor[i].name,recordDescriptor[i].type,recordDescriptor[i].length,i+1,record);
							res = rbfm->insertRecord(fileHandle,recordDescriptor2,record, rid);
							assert( res == 0 && "insert columns should not fail");
							free(record);
			}



			for(int i = 0; i<recordDescriptor2.size();i++){
				void *record = malloc(PAGE_SIZE);
				createColumnRecord(2,recordDescriptor2[i].name,recordDescriptor2[i].type,recordDescriptor2[i].length,i+1,record);
				res = rbfm->insertRecord(fileHandle,recordDescriptor2,record, rid);
				assert( res == 0 && "insert columns should not fail");
				free(record);
			}
			res = rbfm->closeFile(fileHandle);
			assert( res == 0 && "Close columns should not fail");
		}
	else{return -1;}
	return 0;
}

RC RelationManager::deleteCatalog()
{
	RecordBasedFileManager *rbfm=RecordBasedFileManager::instance();

	if(rbfm->destroyFile("Tables")==0&&rbfm->destroyFile("Columns")==0){
		return 0;
	}
	return -1;
}

RC RelationManager::createTable(const string &tableName, const vector<Attribute> &attrs)
{
	/* Create Table procedures
	 * 1. Create a file.
	 * 2. Append the new table in to table file.
	 * 3. Append the new atts in to columns file.
	 */
	RecordBasedFileManager *rbfm=RecordBasedFileManager::instance();
	RID rid;
	FileHandle fileHandle;
	vector<Attribute> recordDescriptor;
	vector<Attribute> recordDescriptor2;
	int table_id = updateTableId();
	if(table_id == -1){return -1;}
	RC res;
	void*record= malloc(PAGE_SIZE);
	//cout<<"Table id is "<<table_id+1<<endl;
	if(rbfm->createFile(tableName)==0){
		//update table file
		rbfm->openFile("Tables",fileHandle);
		createRecordDescriptorForTable(recordDescriptor);
		createTableRecord(table_id+1,tableName,tableName,record);
		rbfm->insertRecord(fileHandle,recordDescriptor,record,rid);
		//cout<<"Insert table with rid "<<rid.pageNum<< " : "<<rid.slotNum<<endl;
		void*d =malloc(PAGE_SIZE);
		rbfm->readRecord(fileHandle,recordDescriptor,rid,d);
		//rbfm->printRecord(recordDescriptor,d);
		free(d);
		//rbfm->deleteRecord(fileHandle,recordDescriptor,rid);
		rbfm->closeFile(fileHandle);

		//update column file
		rbfm->openFile("Columns",fileHandle);
		createRecordDescriptorForColumns(recordDescriptor2);
		for(int i = 0; i<attrs.size();i++){
			//record = malloc(PAGE_SIZE);
			createColumnRecord(table_id+1,attrs[i].name,attrs[i].type,attrs[i].length,i+1,record);

			res = rbfm->insertRecord(fileHandle,recordDescriptor2,record, rid);
			//cout<<"Insert Columns with rid "<<rid.pageNum<< " : "<<rid.slotNum<<endl;
			assert( res == 0 && "insert columns should not fail");
			//free(record);
		}
		rbfm->closeFile(fileHandle);
		free(record);
		return 0;
	}
	free(record);
	return -1;
}

RC RelationManager::deleteTable(const string &tableName)
{
    if(tableName == "Tables" || tableName == "Columns"){
    	return -1;
    }
    vector<string> attrnames;
    attrnames.push_back("table-id");
    RecordBasedFileManager *rbfm=RecordBasedFileManager::instance();
    //vector<RID> rid;
    FileHandle fileHandle;
    vector<Attribute> recordDescriptor;
    vector<Attribute> recordDescriptor1;

    if(rbfm->destroyFile(tableName)==0){
    	rbfm->openFile("Tables",fileHandle);
    	//find table id
    	int table_id = findTableId(tableName);
    	//cout<<"DELETE TABLE ID IS "<<table_id<<" with name "<<tableName<<endl;
    	if(table_id==-1){
    		return -1;
    	}

    	RM_ScanIterator table_ScanIterator;

    	//delete info in table
    	scan("Tables","table-id",EQ_OP,&table_id,attrnames,table_ScanIterator);
    	void *data = malloc(PAGE_SIZE);
    	RID rid;
    	createRecordDescriptorForTable(recordDescriptor);
    	while(table_ScanIterator.getNextTuple(rid,data) != RM_EOF){
    		//cout<<"Delete record "<<rid.pageNum<<" : "<<rid.slotNum<<endl;
    		rbfm->deleteRecord(fileHandle,recordDescriptor,rid);
    	}
    	rbfm->closeFile(fileHandle);
    	table_ScanIterator.close();

    	//delete info in column

    	RM_ScanIterator column_ScanIterator;
    	RC res = rbfm->openFile("Columns",fileHandle);
    	createRecordDescriptorForColumns(recordDescriptor1);
    	res = scan("Columns","table-id",EQ_OP,&table_id,attrnames,column_ScanIterator);
    	while(column_ScanIterator.getNextTuple(rid,data) != RM_EOF){
    	    rbfm->deleteRecord(fileHandle,recordDescriptor1,rid);
    	}
    	rbfm->closeFile(fileHandle);
    	column_ScanIterator.close();

    	return 0;
    }
	return -1;
}

RC RelationManager::getAttributes(const string &tableName, vector<Attribute> &attrs)
{
    /*
     * 1. Find table id and use it to search column table
     * 2. Return data that include column-name, column-type,column-length column-position
     * 3. Copy it into attr variable and append it to attrs
     */

	int table_id = findTableId(tableName);
	if(table_id==-1){return -1;}

	//construct attrnames
	vector<string> attrNames;
	attrNames.push_back("column-name");
	attrNames.push_back("column-type");
	attrNames.push_back("column-length");

		RM_ScanIterator column_ScanIterator;
		if(scan("Columns","table-id",EQ_OP,&table_id,attrNames,column_ScanIterator)==0){
		RID rid;
		void *data=(char*)malloc(PAGE_SIZE);
		int offset = 0;
		Attribute attr;
		while(column_ScanIterator.getNextTuple(rid,data)!=RM_EOF){
			offset = 0;
			//nullIndicator
			offset += 1;
			//Column-name
			int size;
			memcpy(&size,(char*)data+offset,sizeof(int));
			offset+=sizeof(int);
			char nameString[size+1];
			memcpy(&nameString,(char*)data+offset,size);
			offset+=size;
			nameString[size]='\0';
			string tmp(nameString);
			attr.name=tmp;

			//columntype
			memcpy(&(attr.type),(char*)data+offset,sizeof(int));
			offset+=sizeof(int);

			//column-length
			memcpy(&(attr.length),(char*)data+offset,sizeof(int));
			offset+=sizeof(int);

			attrs.push_back(attr);
		}
		column_ScanIterator.close();
		free(data);
		return 0;

	}
	return -1;
}

RC RelationManager::insertTuple(const string &tableName, const void *data, RID &rid)
{
	if(tableName == "Tables" || tableName == "Columns"){return -1;}
	RecordBasedFileManager *rbfm=RecordBasedFileManager::instance();
	FileHandle fileHandle;
	vector<Attribute> recordDescriptor;

	if(rbfm->openFile(tableName,fileHandle)==0){
		getAttributes(tableName,recordDescriptor);
		if(rbfm->insertRecord(fileHandle,recordDescriptor,data,rid)==0){
			//add index
			for(int i =0;i<recordDescriptor.size();++i){
				//find exist index file and insert the data into it
				string indexFileName = tableName + "-" + recordDescriptor[i].name +".ix";
				//cout<<indexFileName<<endl;
				if(this->im.openFile(indexFileName,this->ixfileHandle) == 0){
					//cout<<"----------------------------"<<endl;
					//cout<<"Insert data in to index "<<indexFileName<<endl;
					void*key = malloc(PAGE_SIZE);
					this->readAttribute(tableName,rid,recordDescriptor[i].name,key);
					im.insertEntry(ixfileHandle,recordDescriptor[i],(char*)key+1,rid);
					//cout<<"TRee for "<< indexFileName<< "is "<<endl;
					//im.printBtree(ixfileHandle,recordDescriptor[i]);
					//cout<<"-----------------------------------"<<endl;
					free(key);
					this->im.closeFile(ixfileHandle);
				}
			}
		rbfm->closeFile(fileHandle);
		return 0;
		}
		rbfm->closeFile(fileHandle);
	}
	return -1;
}


RC RelationManager::deleteTuple(const string &tableName, const RID &rid)
{
	if(tableName == "Tables" || tableName == "Columns"){return -1;}
	RecordBasedFileManager *rbfm=RecordBasedFileManager::instance();
	FileHandle fileHandle;
	vector<Attribute> recordDescriptor;
	if(rbfm->openFile(tableName,fileHandle)==0){
		getAttributes(tableName,recordDescriptor);

		//delete index entry in the files
		for(int i =0;i<recordDescriptor.size();i++){
			//find exist index file and insert the data into it
			string indexFileName = tableName + "-" + recordDescriptor[i].name +".ix";
			if(this->im.openFile(indexFileName,this->ixfileHandle) == 0){
					void*key = malloc(PAGE_SIZE);
					this->readAttribute(tableName,rid,recordDescriptor[i].name,key);
					im.deleteEntry(ixfileHandle,recordDescriptor[i],(char*)key+1,rid);
					this->im.closeFile(ixfileHandle);
			}
		}

		if(rbfm->deleteRecord(fileHandle,recordDescriptor,rid)==0){
			rbfm->closeFile(fileHandle);
			return 0;
		}
		rbfm->closeFile(fileHandle);
	}
	return -1;
}

RC RelationManager::updateTuple(const string &tableName, const void *data, const RID &rid)
{
	if(tableName == "Tables" || tableName == "Columns"){return -1;}
	RecordBasedFileManager *rbfm=RecordBasedFileManager::instance();
	FileHandle fileHandle;
	vector<Attribute> recordDescriptor;
	if(rbfm->openFile(tableName,fileHandle)==0){
		getAttributes(tableName,recordDescriptor);

		//delete index entry in the files
		for(int i =0;i<recordDescriptor.size();i++){
				//find exist index file and insert the data into it
				string indexFileName = tableName + "-" + recordDescriptor[i].name +".ix";
				if(this->im.openFile(indexFileName,this->ixfileHandle) == 0){
						void*key = malloc(PAGE_SIZE);
						this->readAttribute(tableName,rid,recordDescriptor[i].name,key);
						im.deleteEntry(ixfileHandle,recordDescriptor[i],(char*)key+1,rid);
						this->im.closeFile(ixfileHandle);
				}
			}

		if(rbfm->updateRecord(fileHandle,recordDescriptor,data,rid)==0){
			//add index
			for(int i =0;i<recordDescriptor.size();i++){
					//find exist index file and insert the data into it
					string indexFileName = tableName + "-" + recordDescriptor[i].name +".ix";
					if(this->im.openFile(indexFileName,this->ixfileHandle) == 0){
						void*key = malloc(PAGE_SIZE);
						this->readAttribute(tableName,rid,recordDescriptor[i].name,key);
						im.insertEntry(ixfileHandle,recordDescriptor[i],(char*)key+1,rid);
						this->im.closeFile(ixfileHandle);
					}
			}
			rbfm->closeFile(fileHandle);
			return 0;
		}
		rbfm->closeFile(fileHandle);
	}
	return -1;
}


RC RelationManager::readTuple(const string &tableName, const RID &rid, void *data)
{
		RecordBasedFileManager *rbfm=RecordBasedFileManager::instance();
		FileHandle fileHandle;
		vector<Attribute> recordDescriptor;
		if(rbfm->openFile(tableName,fileHandle)==0){
			getAttributes(tableName,recordDescriptor);
			if(rbfm->readRecord(fileHandle,recordDescriptor,rid,data)==0){
				rbfm->closeFile(fileHandle);
				return 0;
			}
			rbfm->closeFile(fileHandle);
			}
		return -1;
}

RC RelationManager::printTuple(const vector<Attribute> &attrs, const void *data)
{
	RecordBasedFileManager *rbfm=RecordBasedFileManager::instance();
	return rbfm->printRecord(attrs,data);
}

RC RelationManager::readAttribute(const string &tableName, const RID &rid, const string &attributeName, void *data)
{
	RecordBasedFileManager *rbfm=RecordBasedFileManager::instance();
	FileHandle fileHandle;
	vector<Attribute> recordDescriptor;
	if(rbfm->openFile(tableName,fileHandle)==0){
		getAttributes(tableName,recordDescriptor);
		if(rbfm->readAttribute(fileHandle,recordDescriptor,rid,attributeName,data)==0){
			rbfm->closeFile(fileHandle);
			return 0;}
		rbfm->closeFile(fileHandle);
	}
	return -1;
}

RC RelationManager::scan(const string &tableName,
      const string &conditionAttribute,
      const CompOp compOp,                  
      const void *value,                    
      const vector<string> &attributeNames,
      RM_ScanIterator &rm_ScanIterator)
{

	vector<Attribute> recordDescriptor;
	if(tableName == "Tables"){
		createRecordDescriptorForTable(recordDescriptor);
	}

	else if(tableName == "Columns"){
		createRecordDescriptorForColumns(recordDescriptor);
	}
	else{
		getAttributes(tableName,recordDescriptor);
	}

	RecordBasedFileManager *rbfm=RecordBasedFileManager::instance();
	FileHandle filehandle;
	if(rbfm->openFile(tableName,filehandle) == 0){
		rbfm->scan(filehandle,recordDescriptor,conditionAttribute,compOp,value,attributeNames,rm_ScanIterator.rbfm_scanIterator);
		return 0;
	}
	//cout<<"can't open table"<<tableName<<endl;
	return -1;
}

RC RelationManager::createIndex(const string&tableName,const string & attributeName){
	/* Procedure:
	 * Get recordDescriptor and find the target attribute
	 * use scan iterator to get RID:
	 * 		use readAttribute get the data with attributeName.
	 * 		use ixManager insert(targetAttr,key,rid)
	 * 	update the information on the system catalog with attribute
	 */

	//get recordDescriptor and target attr
	vector<Attribute> recordDescriptor;
	vector<Attribute> targetAttr;
	this->getAttributes(tableName,recordDescriptor);
	for(int i =0;i<recordDescriptor.size();i++){
			if(recordDescriptor[i].name == attributeName){
				targetAttr.push_back(recordDescriptor[i]);
				break;
			}
	}

	string indexFileName = tableName+"-"+attributeName+".ix";
	//Create index file
	//update the information on the catalog
	this->createTable(indexFileName,targetAttr);
	//Set meta page
	FileHandle fh;
	PagedFileManager::instance()->openFile(indexFileName,fh);
	void*d = malloc(PAGE_SIZE);
	im.updateMetaPage(d,0,-1);
	fh.appendPage(d);
	free(d);
	this->im.openFile(indexFileName,this->ixfileHandle);

	FileHandle fileHandle;
	RecordBasedFileManager *rbfm=RecordBasedFileManager::instance();
	rbfm->openFile(tableName,fileHandle);
	RID rid;
	RM_ScanIterator table_ScanIterator;
	void *data=(char*)malloc(PAGE_SIZE);
	vector<string> attrNames;
	attrNames.push_back(attributeName);

	int res = -1;
	scan(tableName,"table-name",NO_OP,NULL,attrNames,table_ScanIterator);

	while(table_ScanIterator.getNextTuple(rid,data)!=RM_EOF){
			//GET table id
			//cout<<"Rid is: "<<rid.pageNum<<" : "<<rid.slotNum<<endl;
			//rbfm->printRecord(targetAttr,data);
			//insert attr
			void*attr = malloc(PAGE_SIZE);
			this->readAttribute(tableName,rid,attributeName,attr);
			//rbfm->printRecord(targetAttr,attr);
			//cout<<"Insert Entry"<<endl;
			im.insertEntry(ixfileHandle,targetAttr[0],(char*)attr+1,rid);
			free(attr);
		}
	free(data);

	im.closeFile(ixfileHandle);
	rbfm->closeFile(fileHandle);
	table_ScanIterator.close();

	return PASS;
}

RC RelationManager::destroyIndex(const string&tableName,const string&attributeName){
	/*
	 * Delete the index file
	 * Delete the table on the catalog
	 */
	string indexFileName = tableName + "-"+attributeName+".ix";
	return this->deleteTable(indexFileName);
}


RC RelationManager::indexScan(const string &tableName,
        const string &attributeName,
        const void *lowKey,
        const void *highKey,
        bool lowKeyInclusive,
        bool highKeyInclusive,
		   RM_IndexScanIterator &rm_IndexScanIterator){
	string indexFileName = tableName+"-"+attributeName+".ix";
	IXFileHandle i;

	RC res = im.openFile(indexFileName,ixfileHandle);
	//cout<<"Open index file "<<indexFileName<<endl;
	if(res == FAIL){
		//cout<<"can't open index file"<<indexFileName<<endl;
		return FAIL;}
	vector<Attribute>recordDescriptor;
	Attribute attr;
	this->getAttributes(tableName,recordDescriptor);

	for(int i = 0;i<recordDescriptor.size();i++){
		if(recordDescriptor[i].name == attributeName){
			attr = recordDescriptor[i];
			break;
		}
	}
	RC rc = this->im.scan(indexFileName,ixfileHandle,attr,lowKey,highKey,lowKeyInclusive,highKeyInclusive,rm_IndexScanIterator.ix_ScanIterator);
	return rc;
}

RC RM_IndexScanIterator::getNextEntry(RID&rid,void*key){
	return this->ix_ScanIterator.getNextEntry(rid,key);
}

RC RM_IndexScanIterator::close(){

	return this->ix_ScanIterator.close();
}

// Extra credit work
RC RelationManager::dropAttribute(const string &tableName, const string &attributeName)
{
    return -1;
}

// Extra credit work
RC RelationManager::addAttribute(const string &tableName, const Attribute &attr)
{
    return -1;
}


