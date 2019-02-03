
#include "qe.h"

//helper function
RC getAttrData(vector<Attribute> recordDescriptor, vector<string> targetAttrs, void* original_data, void* attrData){
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
				newNullFieldsIndicator[w] =nullFieldsIndicator[i/8]& 1<<(7-i%8);
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
	return PASS;

}

bool meetCondition(Attribute atr,Condition cond, void* attr_data){
	if(cond.op == NO_OP){
		return true;
	}

	int result = -1;
	/*
	string s_val = "";
	string s_data = "";
	void*buffer_val;
	char*res_val;
	void*buffer_data;
	char*res_data;*/

	switch(atr.type){
		case TypeVarChar:
			//evalutate the length
			//use memcmp to compare two typeVarChar


			size_t value_size;
			size_t data_size;
			memcpy(&value_size,(char*)cond.rhsValue.data,sizeof(int));
			memcpy(&data_size,(char*)attr_data+1,sizeof(int));

			/*
			buffer_val = malloc(value_size);
			memcpy(buffer_val,(char*)cond.rhsValue.data+4,value_size);
			((char *)buffer_val)[value_size] = '\0';
			res_val = (char*)buffer_val;
			s_val = string(res_val);

			buffer_data = malloc(data_size);
			memcpy(buffer_data,(char*)attr_data+1+4,data_size);
			((char *)buffer_data)[data_size] = '\0';
			res_data = (char*)buffer_data;
			s_data = string(res_data);


			cout<<"SIZE IS "<<value_size<<" : "<<data_size<<endl;
			cout<<"value is "<< s_val<<endl;
			cout<<"data is "<<s_data<<endl;


			if(s_data == s_val){result = 0;}
			if(s_data > s_val){result = 1;}
			if(s_data<s_val){result = -1;}*/

			if(value_size == data_size){
			result = memcmp((char*)cond.rhsValue.data + 4,(char*)attr_data+1+4,value_size);
			}
			else{
				result = data_size - value_size;
			}




			break;

		case TypeInt:
			int value_int;
			int data_int;
			memcpy(&value_int,(char*)cond.rhsValue.data,sizeof(int));
			memcpy(&data_int,(char*)attr_data+1,sizeof(int));
			result = data_int - value_int;
			break;

		case TypeReal:
			float value_float;
			float data_float;
			memcpy(&value_float,(char*)cond.rhsValue.data,sizeof(float));
			memcpy(&data_float,(char*)attr_data+1,sizeof(float));
			//avoid the precision error
			float res = data_float - value_float;
			if(res>0){result=1;}
			if(res==0){result=0;};
			if(res<0){result=-1;};
			break;
	}



	switch(cond.op){
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

int calculateRecordSize(const vector<Attribute> &recordDescriptor,const void *original_data){
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

void mergeData(vector<Attribute> leftAttr, void* leftData, vector<Attribute> rightAttr, void*rightData,void*returnData){
						int leftSize= leftAttr.size();
						int leftNullSize = ceil((double)leftSize/ CHAR_BIT);
						unsigned char *leftNullFieldsIndicator = (unsigned char *) malloc(leftNullSize);
						memcpy(leftNullFieldsIndicator, (char*)leftData, leftNullSize);

						int rightSize = rightAttr.size();
						int rightNullSize = ceil((double)rightSize / CHAR_BIT);
						unsigned char *rightNullFieldsIndicator = (unsigned char *) malloc(rightNullSize);
						memcpy(rightNullFieldsIndicator, (char*)rightData, rightNullSize);

						int nullSize = ceil((double)(leftSize+rightSize) / CHAR_BIT);
						unsigned char*newNullFieldsIndicator = new unsigned char[nullSize];

						for(int i = 0;i<leftSize;++i){
							newNullFieldsIndicator[i] = leftNullFieldsIndicator[i];
						}

						for(int w = 0;w<rightSize;++w){
							newNullFieldsIndicator[w+leftSize] = leftNullFieldsIndicator[w];
						}

						//copy the new NullFieldsIndicator back to data
						memcpy((char*)returnData,newNullFieldsIndicator,nullSize);

						//copy the leftdata and right data into newdata;
						int leftAtrSize = calculateRecordSize(leftAttr,leftData);
						int righAtrSize = calculateRecordSize(rightAttr,rightData);

						memcpy((char*)returnData+nullSize,(char*)leftData+leftNullSize,leftAtrSize - leftNullSize);
						memcpy((char*)returnData+nullSize+leftAtrSize - leftNullSize,
								(char*)rightData+rightNullSize,righAtrSize-rightNullSize);

}

//--------------------------------------------------------------------------------------------------------------------

Filter::Filter(Iterator* input, const Condition &condition) {
	this->input = input;
	this->condition = condition;
	input->getAttributes(this->attrDescriptor);
	for(int i = 0;i<this->attrDescriptor.size();i++){
		if(this->attrDescriptor[i].name == condition.lhsAttr){
			this->leftAttr = this->attrDescriptor[i];
			break;
		}
	}
}

RC Filter::getNextTuple(void*data){
	while(input->getNextTuple(data) != QE_EOF){
		//cout<<"Filter"<<endl;
		void*attr_data = malloc(PAGE_SIZE);
		vector<string>name;
		name.push_back(this->condition.lhsAttr);
		getAttrData(this->attrDescriptor,name,data,attr_data);

		//vector<Attribute>va;
		//va.push_back(leftAttr);
		//RecordBasedFileManager::instance()->printRecord(attrDescriptor,data);
		//RecordBasedFileManager::instance()->printRecord(va,attr_data);
		if(!meetCondition(this->leftAttr,condition,attr_data)){
			continue;
		}
		return PASS;
	}
	return QE_EOF;
}

void Filter::getAttributes(vector<Attribute> &attrs)const{
	attrs = this->attrDescriptor;
}

// ... the rest of your implementations go here

Project::Project(Iterator *input, const vector<string> &attrNames) {
	this->input = input;
	this->attrNames = attrNames;
	input->getAttributes(attrDescriptor);
}

RC Project::getNextTuple(void *data) {
	void*total_data = malloc(PAGE_SIZE);
	while(input->getNextTuple(total_data) != QE_EOF) {
		//cout<<"Project"<<endl;
		//RecordBasedFileManager::instance()->printRecord(attrDescriptor,total_data);
		getAttrData(this->attrDescriptor,this->attrNames,total_data,data);
		//vector<Attribute> rd;
		//this->getAttributes(rd);
		//RecordBasedFileManager::instance()->printRecord(rd,data);
		return PASS;
	}
	return QE_EOF;
}

void Project::getAttributes(vector<Attribute> &attrs) const {
	if(attrs.size()>0){attrs.clear();}

	for(int i=0; i<attrNames.size(); ++i)
		for(int j=0; j<this->attrDescriptor.size(); ++j)
			if(attrNames[i] == this->attrDescriptor[j].name) {
				attrs.push_back(this->attrDescriptor[j]);
				break;
			}
}

BNLJoin::BNLJoin(Iterator *leftIn, TableScan *rightIn, const Condition &condition, const unsigned numPages) {
	this->leftIn = leftIn;
	this->rightIn = rightIn;
	this->condition = condition;
	this->numPages = numPages;
	leftIn->getAttributes(leftAttr);
	rightIn->getAttributes(rightAttr);

	allAttrs = leftAttr;
	for(int i=0; i<rightAttr.size(); ++i)
		allAttrs.push_back(rightAttr[i]);

	this->isRun=false;

	for(int i=0; i<leftAttr.size(); ++i){
		if(leftAttr[i].name == condition.lhsAttr){
			this->targetAtr = leftAttr[i];
			break;
		}
	}
}

RC BNLJoin::getNextTuple(void *data) {
	/* Procedure:
	 * 	if not run, load the data from left in to the hashmap with value,void*data format
	 *  if run:
	 *  	get the attrData,and then attr value.
	 *  	if hashmap has it, then find it, return nullPointer(Left+right) + all the data;
	 */

	if(!isRun){
		this->isRun = true;

		if(leftIn->getNextTuple(data) == QE_EOF){return QE_EOF;}
		rightIn->setIterator();

		if(this->intMap.size()>0){intMap.clear();}
		if(this->stringMap.size()>0){stringMap.clear();}
		if(this->floatMap.size()>0){floatMap.clear();}

		int freeSpace = numPages*PAGE_SIZE;

		do{
			int record_size;
			record_size=calculateRecordSize(this->leftAttr,data);
			freeSpace -= record_size;

			Value value;
			value.type =targetAtr.type;
			value.data = malloc(record_size);
			memcpy(value.data,(char*)data,record_size);

			vector<string> attrName;
			attrName.push_back(condition.lhsAttr);
			void*attr_data = malloc(record_size);
			getAttrData(this->leftAttr,attrName,data,attr_data);

			if(this->targetAtr.type == TypeInt){
				int intVal;
				memcpy(&intVal,(char*)attr_data+1,sizeof(int));
				this->intMap.insert(make_pair(intVal,value));
				continue;
			}
			if(this->targetAtr.type == TypeVarChar){
				int strLen;
				string strVal = "";
				memcpy(&strLen,(char*)attr_data+1,sizeof(int));
				for(int i = 0;i<strLen;++i){
					strVal += *(char*)((char*)data+1+4+i);
				}
				this->stringMap.insert(make_pair(strVal,value));
			}

			if(this->targetAtr.type== TypeReal){
				float realVal;
				memcpy(&realVal,(char*)attr_data+1,sizeof(float));
				this->floatMap.insert(make_pair(realVal,value));
			}

		}while(freeSpace>=0 && leftIn->getNextTuple(data)!= QE_EOF);

		return getNextTuple(data);
	}

	else{
		void*rightData = malloc(PAGE_SIZE);

		while(rightIn->getNextTuple(rightData) != QE_EOF){

			vector<string> attrName1;
			attrName1.push_back(condition.rhsAttr);
			getAttrData(this->rightAttr,attrName1,rightData,data);

			if(this->targetAtr.type == TypeInt){
				int intVal;
				memcpy(&intVal,(char*)data+1,sizeof(int));

				if(this->intMap.count(intVal)>0){
					mergeData(this->leftAttr,intMap[intVal].data,this->rightAttr,rightData,data);
					return PASS;
				}
				continue;
			}

			if(this->targetAtr.type == TypeVarChar){
				int strLen;
				string strVal = "";
				memcpy(&strLen,(char*)data+1,sizeof(int));
				for(int i = 0;i<strLen;++i){
					strVal += *(char*)((char*)data+1+4+i);
				}

				if(this->stringMap.count(strVal)>0){
					mergeData(this->leftAttr,stringMap[strVal].data,this->rightAttr,rightData,data);
					return PASS;
				}
				continue;
			}

			if(this->targetAtr.type== TypeReal){
				float realVal;
				memcpy(&realVal,(char*)data+1,sizeof(float));

				if(this->floatMap.count(realVal)>0){
					mergeData(this->leftAttr,floatMap[realVal].data,this->rightAttr,rightData,data);
					return PASS;
				}
				continue;
			}


		}
		this->isRun = false;
		return getNextTuple(data);
	}


}

void BNLJoin::getAttributes(vector<Attribute> &attrs) const{
	attrs = allAttrs;
}

INLJoin::INLJoin(Iterator *leftIn, IndexScan *rightIn, const Condition &condition) {
	this->leftIn = leftIn;
	this->rightIn = rightIn;
	this->condition = condition;

	leftIn->getAttributes(leftAttr);
	rightIn->getAttributes(rightAttr);
	allAttrs = leftAttr;
	for(int i=0; i<rightAttr.size(); ++i){
		allAttrs.push_back(rightAttr[i]);
	}

	this->current = malloc(PAGE_SIZE);
	this->buffer = malloc(PAGE_SIZE);

	this->isRun = false;
}

RC INLJoin::getNextTuple(void *data) {
	if(!isRun){
		isRun = true;
		if(leftIn->getNextTuple(buffer) != QE_EOF){
			//cout<<"InLJoin"<<endl;
			vector<string> attrName;
			attrName.push_back(condition.lhsAttr);
			getAttrData(this->leftAttr,attrName,buffer,current);
			cout<<"------------------"<<endl;
			cout<<"LEft is "<<endl;
			RecordBasedFileManager::instance()->printRecord(leftAttr,buffer);
			rightIn->setIterator((char*)current+1,(char*)current+1,true,true);
			return getNextTuple(data);
		}
		else{
			isRun = false;
			return QE_EOF;}
	}
	else{
		if(rightIn->getNextTuple(data) != QE_EOF){
			int record_size;
			record_size = calculateRecordSize(this->rightAttr,data);
			memcpy((char*)current,(char*)data,record_size);
			cout<<"Right is "<<endl;
			RecordBasedFileManager::instance()->printRecord(rightAttr,current);
			mergeData(this->leftAttr,buffer,this->rightAttr,current,data);
			return PASS;
		}
		else{
			isRun = false;
			cout<<"------------------"<<endl;
			return getNextTuple(data);
		}
	}
}

void INLJoin::getAttributes(vector<Attribute> &attrs) const {
	attrs = this->allAttrs;
}

Aggregate::Aggregate(Iterator *input, Attribute aggAttr, AggregateOp op){
	this->input = input;
	this->attr = aggAttr;
	input->getAttributes(attrs);
	this->op = op;
	this->group = false;


    int intVal = 0;
    float floatVal = 0.0;
    void *data = malloc(PAGE_SIZE);
    this->buffer = malloc(PAGE_SIZE);

	while(input->getNextTuple(data) != QE_EOF) {

		vector<string> attrNames;
		attrNames.push_back(aggAttr.name);
		getAttrData(attrs, attrNames, data, buffer);

		if(aggAttr.type == TypeInt){
			intVal= *(int*) ((char *) buffer + 1);
			//cout<<"INT VAL IS "<<intVal<<endl;
			if(intVal < minVal){minVal = intVal;}
			if(intVal > maxVal){maxVal = intVal;}

			count += 1;
			sum += intVal;
			aveVal = sum / count;

		}else{
			floatVal= *(float*) ((char *) buffer + 1);
			//cout << "float: " << floatVal << endl;
			if(floatVal < minVal)
				{minVal = floatVal;}
			if(floatVal > maxVal)
				{maxVal = floatVal;}
			count += 1;
			sum += floatVal;
			aveVal = sum / count;
		}

	}
    free(data);
}


Aggregate::Aggregate(Iterator *input, Attribute aggAttr, Attribute groupAttr, AggregateOp op){
	this->input = input;
	this->attr = aggAttr;
	this->groupAttr = groupAttr;
	this->input->getAttributes(attrs);
	this->op = op;
	this->group=true;

    void *data = malloc(PAGE_SIZE);
    this->buffer = malloc(PAGE_SIZE);

    int intVal = 0;
    int intGroupVal = 0;
    float floatVal = 0.0;
    float floatGroupVal =0.0;

    GroupData groupdata;

    while(input->getNextTuple(data) != QE_EOF){
    	//get the attr data and groupAttr data
    	vector<string> attrNames;
    	attrNames.push_back(attr.name);
    	attrNames.push_back(groupAttr.name);
    	getAttrData(attrs,attrNames,data,buffer);

    	if(attr.type == TypeInt){memcpy(&intVal,(char*)buffer+1,sizeof(int));}
    	if(attr.type == TypeReal){memcpy(&floatVal,(char*)buffer+1,sizeof(float));}

    	if(groupAttr.type == TypeInt){
    		intGroupVal = *(int*) ((char *) buffer + 5);
    		if(attr.type == TypeInt){
    			if(this->groupIntMap.count(intGroupVal)>0){
    				groupIntMap[intGroupVal].count++;
    				groupIntMap[intGroupVal].sum +=intVal;
    				groupIntMap[intGroupVal].aveVal = groupIntMap[intGroupVal].sum / groupIntMap[intGroupVal].count;
    				if(intVal < groupIntMap[intGroupVal].minVal){
    					groupIntMap[intGroupVal].minVal = intVal;
    				}
    				else{
    					groupIntMap[intGroupVal].maxVal = intVal;
    				}
    			}
    			else{
    				groupdata.minVal = intVal;
    				groupdata.maxVal = intVal;
    				groupdata.sum = intVal;
    				groupdata.count = 1;
    				groupdata.aveVal = intVal;
    				groupIntMap[intGroupVal] = groupdata;
    			}
    		}
    		if(attr.type == TypeReal){
    			if(this->groupIntMap.count(intGroupVal)>0){
    			    	groupIntMap[intGroupVal].count++;
    			    	groupIntMap[intGroupVal].sum +=floatVal;
    			    	groupIntMap[intGroupVal].aveVal = groupIntMap[intGroupVal].sum / groupIntMap[intGroupVal].count;
    			    	if(floatVal < groupIntMap[intGroupVal].minVal){
    			    		groupIntMap[intGroupVal].minVal = floatVal;
    			    		}
    			    	else{
    			    		groupIntMap[intGroupVal].maxVal = floatVal;
    			    	}
    			 }
    			 else{
    			    	groupdata.minVal = floatVal;
    			    	groupdata.maxVal = floatVal;
    			    	groupdata.sum = floatVal;
    			    	groupdata.count = 1;
    			    	groupdata.aveVal = floatVal;
    			    	groupIntMap[intGroupVal] = groupdata;
    	   			}
    		}
    	}

    	if(groupAttr.type == TypeReal){
    		floatGroupVal = *(float*) ((char *) buffer + 5);
    		    		if(attr.type == TypeInt){
    		    			if(this->groupFloatMap.count(floatGroupVal)>0){
    		    				groupFloatMap[floatGroupVal].count++;
    		    				groupFloatMap[floatGroupVal].sum +=intVal;
    		    				groupFloatMap[floatGroupVal].aveVal = groupFloatMap[floatGroupVal].sum / groupFloatMap[floatGroupVal].count;
    		    				if(intVal < groupFloatMap[floatGroupVal].minVal){
    		    					groupFloatMap[floatGroupVal].minVal = intVal;
    		    				}
    		    				else{
    		    					groupFloatMap[floatGroupVal].maxVal = intVal;
    		    				}
    		    			}
    		    			else{
    		    				groupdata.minVal = intVal;
    		    				groupdata.maxVal = intVal;
    		    				groupdata.sum = intVal;
    		    				groupdata.count = 1;
    		    				groupdata.aveVal = intVal;
    		    				groupFloatMap[floatGroupVal] = groupdata;
    		    			}
    		    		}
    		    		if(attr.type == TypeReal){
    		    			if(this->groupFloatMap.count(floatGroupVal)>0){
    		    				groupFloatMap[floatGroupVal].count++;
    		    				groupFloatMap[floatGroupVal].sum +=floatVal;
    		    				groupFloatMap[floatGroupVal].aveVal = groupFloatMap[floatGroupVal].sum / groupFloatMap[floatGroupVal].count;
    		    			    	if(floatVal < groupFloatMap[floatGroupVal].minVal){
    		    			    		groupFloatMap[floatGroupVal].minVal = floatVal;
    		    			    		}
    		    			    	else{
    		    			    		groupFloatMap[floatGroupVal].maxVal = floatVal;
    		    			    	}
    		    			 }
    		    			 else{
    		    			    	groupdata.minVal = floatVal;
    		    			    	groupdata.maxVal = floatVal;
    		    			    	groupdata.sum = floatVal;
    		    			    	groupdata.count = 1;
    		    			    	groupdata.aveVal = floatVal;
    		    			    	groupFloatMap[floatGroupVal] = groupdata;
    		    	   			}
    		    		}
    	}

    }

    free(data);
}


RC Aggregate::getNextTuple(void *data) {

	if(!group){
			switch(this->op){
				case MIN:
						*(float *) ((char *) data + 1) = this->minVal;
					break;
				case MAX:
						*(float *) ((char *) data + 1) = this->maxVal;
					break;
				case COUNT:
						*(float *) ((char *) data + 1) = this->count;
					break;
				case SUM:
						*(float *) ((char *) data + 1) = this->sum;
					break;
				case AVG:
						*(float *) ((char *) data + 1) = this->aveVal;
					break;
			}
			if(!this->finish){
				this->finish = true;
				return PASS;
			}
			else{
				return QE_EOF;
			}
	}

	else{

		if(this->groupAttr.type == TypeInt){
			if(groupIntMap.size() == 0){return QE_EOF;}
			*(int *) ((char *) data + 1) = groupIntMap.begin()->first;
			switch(op){
					case MIN:
						*(float *) ((char *) data + 5) = groupIntMap.begin()->second.minVal;
						break;
					case MAX:
						*(float *) ((char *) data + 5) = groupIntMap.begin()->second.maxVal;
						break;
					case COUNT:
						*(float *) ((char *) data + 5) = groupIntMap.begin()->second.count;
						break;
					case SUM:
						*(float *) ((char *) data + 5) = groupIntMap.begin()->second.sum;
						break;
					case AVG:
						*(float *) ((char *) data + 5) = groupIntMap.begin()->second.aveVal;
						break;
			}
			groupIntMap.erase(groupIntMap.begin());
			return PASS;
		}

		if(this->groupAttr.type == TypeReal){
			if(groupFloatMap.size()==0){return QE_EOF;}
			*(float *) ((char *) data + 1) = groupFloatMap.begin()->first;
			switch(this->op){
					case MIN:
						*(float *) ((char *) data + 5) = groupFloatMap.begin()->second.minVal;
						break;
					case MAX:
						*(float *) ((char *) data + 5) = groupFloatMap.begin()->second.maxVal;
						break;
					case COUNT:
						*(float *) ((char *) data + 5) = groupFloatMap.begin()->second.count;
						break;
					case SUM:
						*(float *) ((char *) data + 5) = groupFloatMap.begin()->second.sum;
						break;
					case AVG:
						*(float *) ((char *) data + 5) = groupFloatMap.begin()->second.aveVal;
						break;
					}
			groupFloatMap.erase(groupFloatMap.begin());
			return PASS;
		}
	}
	return QE_EOF;
}

void Aggregate::getAttributes(vector<Attribute> &attrs) const {
	attrs = this->attrs;
}

