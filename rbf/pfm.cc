#include "pfm.h"
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <cstdlib>
using namespace std;

PagedFileManager* PagedFileManager::_pf_manager = 0;

PagedFileManager* PagedFileManager::instance()
{
    if(!_pf_manager)
        _pf_manager = new PagedFileManager();

    return _pf_manager;
}


PagedFileManager::PagedFileManager()
{
}


PagedFileManager::~PagedFileManager()
{

	_pf_manager = NULL;
	delete _pf_manager;
}


RC PagedFileManager::createFile(const string &fileName)
{
	// Using ifstream to test whether the file exist or not


	if(access(fileName.c_str(),0) == 0){ //if exist
		return -1;
	}
	else{
		//if not exist, create a new one.
		FILE* new_file = fopen(fileName.c_str(),"wb");
		FILEINFO fileInfo;
		fileInfo.appendPageCounter = 0;
		fileInfo.readPageCounter = 0;
		fileInfo.writePageCounter = 0;
		if(!new_file){
			fclose(new_file);
			return -1;
		}

		void*data = malloc(sizeof(FILEINFO));
		memcpy(data,&fileInfo,sizeof(FILEINFO));
		fseek(new_file,0,SEEK_SET);
		fwrite(data,sizeof(FILEINFO),1,new_file);

		free(data);
		fclose(new_file);
		return 0;
	}
	return -1;

}


RC PagedFileManager::destroyFile(const string &fileName)
{
	//cout<<fileName<<endl;
	if(access(fileName.c_str(), 0) != 0){
		//cout<<"doens't exist"<<endl;
		return -1;  //doesn't exist
	}
	//cout<<"remove?"<<endl;
	return remove(fileName.c_str());

}


RC PagedFileManager::openFile(const string &fileName, FileHandle &fileHandle)
{

    if(access(fileName.c_str(),0)==0){

    	//If exist, then open it and pass is to the File handle class
    	fileHandle.file = fopen(fileName.c_str(), "rb+");
    	fileHandle.fileName = fileName;
    	if(fileHandle.file == NULL){
    		//cout<<"get null file for "<<fileName<<endl;
    		return -1;
    	}
    	return 0;
    }
    else{
    	//cout<<"NOfile"<<endl;
    	return -1;
    }
}


RC PagedFileManager::closeFile(FileHandle &fileHandle)
{

	int res = fclose(fileHandle.file);
	fileHandle.file=NULL;
	return res;
}


FileHandle::FileHandle()
{
    readPageCounter = 0;
    writePageCounter = 0;
    appendPageCounter = 0;
    file = NULL;
}


FileHandle::~FileHandle()
{

	file = NULL;
}


RC FileHandle::readPage(PageNum pageNum, void *data)
{
	// if pageNum is smaller or equal than the total page number
	// and, if fseek can set the pointer to the pageNum*PAGE_SIZE,
	// and, if fread can read 1 length of Page_size,
	// finally, return 0;

	if(access(fileName.c_str(),0) != 0){ //if file not exist
		return -1;
	}

	if(pageNum < getNumberOfPages() && fseek(file,sizeof(FILEINFO) + pageNum*PAGE_SIZE,SEEK_SET) == 0){
		int size = fread(data,PAGE_SIZE,1,file);
		return size == 1 ? readPageCounter++, 0 : -1;

	}
	return -1;
}


RC FileHandle::writePage(PageNum pageNum, const void *data)
{
	// The logic is similar with read Page
	// if pageNum is smaller or equal than the total page number
	// and, if fseek can set the pointer to the pageNum*PAGE_SIZE,
	// and, if fwrite can write 1 length of Page_size,
	// finally, return 0;
	if(access(fileName.c_str(),0) != 0){ //if file not exist
		//cout<<"file not exist!"<<endl;
		return -1;
	}

	if(pageNum < getNumberOfPages() && fseek(file,sizeof(FILEINFO)+ pageNum*PAGE_SIZE,SEEK_SET) == 0){
			int size = fwrite(data,PAGE_SIZE,1,file);
			//cout<<"SIZE IS "<< size<<endl;
			if(size == 1){
				//cout<<"cool write it back"<<endl;
				writePageCounter++;
				return 0;
			}
			//cout<<"OPS"<<endl;
			return -1;

		}
	//cout<<"Fseek problem?"<<endl;
	return -1;
}


RC FileHandle::appendPage(const void *data)
{
	// Similarly with Write page except that we need set the pointer to the end of the file

	if(access(fileName.c_str(),0) != 0){ //if file not exist

			return -1;
		}
	if(fseek(file,0,SEEK_END) == 0){
		int size = fwrite(data,PAGE_SIZE,1,file);
		return size == 1 ? appendPageCounter++, 0 : -1;
	}

	return -1;
}

void FileHandle::updateCounter(){

			FILEINFO fileInfo;
			void *data = malloc(sizeof(FILEINFO));

			fseek(file,0,SEEK_SET);
			fread(data,sizeof(FILEINFO),1,file);
			memcpy(&fileInfo,data,sizeof(FILEINFO));

			fileInfo.appendPageCounter += appendPageCounter;
			fileInfo.readPageCounter += readPageCounter;
			fileInfo.writePageCounter += writePageCounter;

			readPageCounter = 0;
			writePageCounter = 0;
			appendPageCounter = 0;

			memcpy(data,&fileInfo,sizeof(FILEINFO));
			fseek(file,0,SEEK_SET);
			fwrite(data,sizeof(FILEINFO),1,file);

			free(data);
}



unsigned FileHandle::getNumberOfPages()
{
    // Use fseek to set the page write pointer from 0 to end, and use ftell get the total size of file
	fseek(file,0,SEEK_END);
	//if success, then divide by page_size to get the number of pages.
	unsigned size = ftell(file) - sizeof(FILEINFO);
    return size/PAGE_SIZE;
}


RC FileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount)
{
	updateCounter();
	FILEINFO fileInfo;
	void *data = malloc(sizeof(FILEINFO));
	fseek(file,0,SEEK_SET);
	fread(data,sizeof(FILEINFO),1,file);
	memcpy(&fileInfo,data,sizeof(FILEINFO));

	readPageCount = fileInfo.readPageCounter;
	writePageCount = fileInfo.writePageCounter ;
	appendPageCount = fileInfo.appendPageCounter;

	free(data);
    return 0;
}
