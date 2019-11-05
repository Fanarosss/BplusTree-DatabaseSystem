#include "AM.h"
#include "bf.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "BplusTree.h"


int AM_errno = AME_OK;

void AM_Init() {
	CALL_OR_DIE(BF_Init(LRU));
	for(int i = 0; i < MAX_OPEN_FILES; i++){
		OPENFILES[i] = NULL;                                                    //initialize all open files as NULL
	}
	for(int i = 0; i < MAX_OPEN_FILES; i++){
		SCANFILES[i] = NULL;                                                    //initiliaze all open scans as NULL
	}
	return;
}


int AM_CreateIndex(char *fileName, char attrType1, int attrLength1, char attrType2, int attrLength2) {
	AM_errno = AME_OK;
	int fileDesc;
	if(attrType1=='c'){
		if((attrLength1>255) || (attrLength1<1)){
			AM_errno = AME_WRONGLENGTH1;                           //length of attrType1:c is wrong
			return AM_errno;
		}
	}else if((attrType1=='i') || (attrType1=='f')){
		if(attrLength1!=4){
			AM_errno = AME_WRONGLENGTH1;                           //length of attrType1:i or attrType1:f is wrong
			return AM_errno;
		}
	}else{
		AM_errno = AME_WRONGTYPE1;                                 //if attrType1 is not c,i,or f then it's wrong
		return AM_errno;
	}
	if(attrType2=='c'){
		if((attrLength2>255) || (attrLength2<1)){
			AM_errno = AME_WRONGLENGTH2;                          //length of attrType2:c is wrong
			return AM_errno;
		}
	}else if((attrType2=='i') || (attrType2=='f')){
		if(attrLength2!=4){
			AM_errno = AME_WRONGLENGTH2;
			return AM_errno;
		}
	}else{
		AM_errno = AME_WRONGTYPE2;
		return AM_errno;
	}
	CALL_OR_DIE(BF_CreateFile(fileName));
	CALL_OR_DIE(BF_OpenFile(fileName, &fileDesc));
	BF_Block *block;                                           //this description block contains all the necessary information of the file
	BF_Block_Init(&block);
	CALL_OR_DIE(BF_AllocateBlock(fileDesc, block));
    int blockid = 0;
	char *BlockData = BF_Block_GetData(block);
	size_t offset = 0;
                                                                //storing in the first 4 bytes the size of the 4 variables: attrType1,attrLength1 etc.
    memset(BlockData + offset, (int)sizeof(attrType1), 1);
    offset++;
    memset(BlockData + offset, (int)sizeof(attrLength1), 1);
    offset++;
    memset(BlockData + offset, (int)sizeof(attrType2), 1);
    offset++;
    memset(BlockData + offset, (int)sizeof(attrLength2), 1);
    offset++;
    //now I store their values
	memcpy(BlockData + offset, &attrType1, sizeof(attrType1));
	offset += sizeof(attrType1);
	memcpy(BlockData + offset, &attrLength1, sizeof(attrLength1));
	offset += sizeof(attrLength1);
	memcpy(BlockData + offset, &attrType2, sizeof(attrType2));
	offset += sizeof(attrType2);
	memcpy(BlockData + offset, &attrLength2, sizeof(attrLength2));
	offset += sizeof(attrLength2);
    //Allocate another data block which will be our starting root of the b+ tree
	BF_Block *block2;
	BF_Block_Init(&block2);
	CALL_OR_DIE(BF_AllocateBlock(fileDesc, block2));
	char *BlockData2 = BF_Block_GetData(block2);
    blockid ++;
    int end_of_data = -1;											//if the pointer to next data block is -1, there is not any other data block
	memcpy(BlockData2,"d",1);										//d:data block, b:interior block
	memset(BlockData2 + 1,0,1); 									//Second byte of block is the number of keys/records in block
	memcpy(BlockData2 + 2,&blockid,sizeof(int));					//store its own block id
    memcpy(BlockData2 + 2 + sizeof(int),&end_of_data,sizeof(int));	//store the id of the next data block
    memcpy(BlockData + offset,&blockid, sizeof(int)); 				//setting root at the description block
	BF_Block_SetDirty(block);
	CALL_OR_DIE(BF_UnpinBlock(block));
	BF_Block_SetDirty(block2);
	CALL_OR_DIE(BF_UnpinBlock(block2));
	BF_Block_Destroy(&block2);
	BF_Block_Destroy(&block);
	CALL_OR_DIE(BF_CloseFile(fileDesc));
  	return AM_errno;
}



int AM_DestroyIndex(char *fileName) {
    AM_errno = AME_OK;
    for(int i = 0; i < MAX_OPEN_FILES; i++){
        if(OPENFILES[i] != NULL){
            if(strcmp(OPENFILES[i]->fileName, fileName) == 0){
                AM_errno = AME_FILEOPEN;							//if the file is still open it cannot be destroyed
                return AM_errno;
            }
        }
    }
    int result = remove(fileName);
    if(result != 0){
        AM_errno = AME_DESTROYINDEX;								//if the remove() function didn't work print error
        return AM_errno;
    }
    return AM_errno;
}



int AM_OpenIndex (char *fileName) {
    AM_errno = AME_OK;
    int fileDesc;
    int i;
    for(i = 0; i < MAX_OPEN_FILES; i++){
        if (OPENFILES[i] == NULL) break;									//find the first NULL index of the OPENFILES array
    }
    if(i == MAX_OPEN_FILES ){
        AM_errno = AME_MAXOPENFILES;										//if the array of open files is full print error
        return AM_errno;
    }
    OPENFILES[i] = malloc(sizeof(AM_Index));								//create a new struct for this open file
    BF_Block *block;
    BF_Block_Init(&block);
    CALL_OR_DIE(BF_OpenFile(fileName, &fileDesc));
    CALL_OR_DIE(BF_GetBlock(fileDesc,0,block));								//take the description block to copy its contents to the struct
    char* BlockData = BF_Block_GetData(block);
    OPENFILES[i]->fileDesc = fileDesc;
    OPENFILES[i]->fileName = malloc(strlen(fileName));
    strcpy(OPENFILES[i]->fileName, fileName);
    size_t offset = 4;
    //initializing the struct with apropriate values
    memcpy(&(OPENFILES[i]->attrType1), (BlockData + offset), *(BlockData));
    offset += *(BlockData);
    memcpy(&(OPENFILES[i]->attrLength1), (BlockData + offset), *(BlockData + 1));
    offset += *(BlockData + 1);
    memcpy(&(OPENFILES[i]->attrType2), (BlockData + offset), *(BlockData + 2));
    offset += *(BlockData + 2);
    memcpy(&(OPENFILES[i]->attrLength2), (BlockData + offset), *(BlockData + 3));
    offset += *(BlockData + 3);
    memcpy(&(OPENFILES[i]->root), (BlockData + offset), sizeof(int));
    BF_Block_SetDirty(block);
    CALL_OR_DIE(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);
    CALL_OR_DIE(BF_GetBlockCounter(fileDesc, &OPENFILES[i]->blockid)); 		//initializing variable blockid in struct
    OPENFILES[i]->blockid --; 												//because block counter, counts the description block also.
    return i;
}



int AM_CloseIndex (int fileDesc) {
    AM_errno = AME_OK;
    if(OPENFILES[fileDesc] == NULL){
        AM_errno = AME_FILENOTOPEN;											//the file i want to close is not open
        return AM_errno;
    }
    for(int i = 0; i < MAX_OPEN_SCANS; i++){
        if(SCANFILES[i] != NULL){
            if((SCANFILES[i]->open_file_id) == fileDesc){
                AM_errno = AME_SCANOPEN;									//the file has open scans to it so it cannot be closed
                return AM_errno;
            }
        }
    }
    BF_Block* block0;
    BF_Block_Init(&block0);
    BF_GetBlock(OPENFILES[fileDesc]->fileDesc, 0 , block0);					//take the description block to write on it
    char* data0 = BF_Block_GetData(block0);
    size_t offset = 4 + *(data0) + *(data0 + 1) + *(data0 + 2) + *(data0 + 3);
    memcpy(data0 + offset, &OPENFILES[fileDesc]->root, sizeof(int));		//write the new root
    BF_Block_SetDirty(block0);
    BF_UnpinBlock(block0);
    BF_Block_Destroy(&block0);
    free(OPENFILES[fileDesc]->fileName);
    free(OPENFILES[fileDesc]);
    OPENFILES[fileDesc] = NULL;												//delete the struct and free the space
    CALL_OR_DIE(BF_CloseFile(fileDesc));
    return AM_errno;
}


int AM_InsertEntry(int fileDesc, void *value1, void *value2) {
    /* searching if the file is open and doesn't have open scans on it */
    for(int j = 0; j < MAX_OPEN_SCANS; j++){                                      //if it is not open,or it is open and has open scans on it
        if(SCANFILES[j] != NULL){
            if(SCANFILES[j]->open_file_id = fileDesc){
                AM_errno = AME_SCANOPEN;
                return AM_errno;                                                  // we return the appropirate error.
            }
        }
    }

    if(OPENFILES[fileDesc] == NULL){
        AM_errno = AME_FILENOTOPEN;
        return AM_errno;
    }

    /* variable decl */
    BF_Block *block1;
    BF_Block_Init(&block1);
    int blocks_num;
    char* BlockData;
    int i = fileDesc;                                                             //keeping things simple, it is what openfile returned
    int size_of_record = OPENFILES[i]->attrLength1 + OPENFILES[i]->attrLength2;
    int size_of_key = OPENFILES[i]->attrLength1;
    CALL_OR_DIE(BF_GetBlock(OPENFILES[i]->fileDesc, OPENFILES[i]->root, block1)); //getting the root block

    if(memcmp(BF_Block_GetData(block1),"d",1) == 0){                              //if the root is a data block, means that it is the only one.
        /* root block = data block*/
        BlockData = BF_Block_GetData(block1);
        size_t used_space = 2 + *(BlockData + 1)*size_of_record + 2*sizeof(int);  //in space that is used in a data block
        size_t available_space = BF_BLOCK_SIZE - used_space;
        if(available_space > size_of_record){                                     //if there is space for another record
            size_t offset = 2 + 2*sizeof(int);
            int processed_records = 0;                                            //this is a counter to know how many records I will have to move
            if(memcmp(&(OPENFILES[i]->attrType1),"f",1) == 0){                    //in case of float, memcmp doesn't work, so I have to cast them
                float f1,f2;
                //casting the variables
                f1 = *(float*)(value1);
                f2 = *(float*)(BlockData + offset);
                while((f1 >= f2) && (processed_records < *(BlockData + 1))){      //until I find a key with bigger value than value1
                                                                                  //I used "=" in the above expression
                    offset += size_of_record;                                     //because all of the equal records, go in the right of their key
                    f2 = *(float*)(BlockData + offset);
                    processed_records++;
                }
            }else{                                                                //for other types memcmp works fine
                while((memcmp(BlockData + offset, value1, OPENFILES[i]->attrLength1) < 0) && (*(BlockData + 1) > processed_records)){ //value1 > blockdata + offset
                    offset += size_of_record;
                    processed_records++;
                }
            }
            memmove(BlockData + offset + size_of_record, BlockData + offset, (*(BlockData + 1) - processed_records)*size_of_record);
            /* with memmove i move (size_of_record)*bytes to right all the records that are greater than the insertion */

            memcpy(BlockData + offset, value1, OPENFILES[i]->attrLength1);        //now i copy the first value-key in place
            offset += OPENFILES[i]->attrLength1;
            memcpy(BlockData + offset, value2, OPENFILES[i]->attrLength2);        //2nd value in place
            memset(BlockData + 1,++*(BlockData + 1),1);                           //increase the byte storing the number of records
            BF_Block_SetDirty(block1);
            BF_UnpinBlock(block1);
        }else{
            /* this is the case where there isn't more space in block to store another record */
            BF_Block *blockRight;
            BF_Block_Init(&blockRight);
            CALL_OR_DIE(BF_AllocateBlock(OPENFILES[i]->fileDesc, blockRight));
            OPENFILES[i]->blockid++;                                              //increasing the variable storing the last block allocated
            int lbid = *(BlockData + 2);
            BF_Block_SetDirty(block1);
            BF_Block_SetDirty(blockRight);
            CALL_OR_DIE(BF_UnpinBlock(blockRight));
            CALL_OR_DIE(BF_UnpinBlock(block1));
            BF_Block_Destroy(&blockRight);
            split_leaf(i, lbid, OPENFILES[i]->blockid);
            Insert_Key(i, lbid, OPENFILES[i]->blockid);                           //connect the two new blocks with the upper block
            AM_InsertEntry(fileDesc, value1, value2);
        }
    }else if(memcmp(BF_Block_GetData(block1),"b",1) == 0){                        //if the root is an index block
        /* root block = index block*/
        CALL_OR_DIE(BF_UnpinBlock(block1));
        int blocknum = AM_Traverse(i, OPENFILES[i]->root, value1);                //traversing from root to the data block where the new entry should be put, and stores the pointer in block1
        CALL_OR_DIE(BF_GetBlock(OPENFILES[i]->fileDesc,blocknum,block1));
        BlockData = BF_Block_GetData(block1);
        size_t used_space = 2 + sizeof(int) + *(BlockData + 1)*size_of_record + sizeof(int);//counting the used space in the block that we traversed
        size_t available_space = BF_BLOCK_SIZE - used_space;
        if (available_space > size_of_record){                                    //there is space for a new entry
            size_t offset = 2 + 2*sizeof(int);
            int processed_records = 0;                                            //this is a counter to know how many records i will have to move
            if(memcmp(&(OPENFILES[i]->attrType1),"f",1) == 0){                    //in case of float, memcmp doesnt work, so I have to cast
                float f1,f2;
                f1 = *(float*)(value1);
                f2 = *(float*)(BlockData + offset);

                while((f1 >= f2) && (processed_records < *(BlockData + 1))){      //until i find the key with biger value than value1
                    offset += size_of_record;
                    f2 = *(float*)(BlockData + offset);
                    processed_records++;
                }
            }else{
                while((memcmp(BlockData + offset, value1, OPENFILES[i]->attrLength1) < 0) && (processed_records < *(BlockData + 1))){ //value1 > blockdata + offset
                    offset += size_of_record;
                    processed_records++;
                }
            }
            memmove(BlockData + offset + size_of_record, BlockData + offset, (*(BlockData + 1) - processed_records)*size_of_record);
            /* with memmove i move (size_of_record)*bytes to right all the records that are greater than the insertion */
            memcpy(BlockData + offset, value1, OPENFILES[i]->attrLength1);        //now i insert the first value in place
            offset += OPENFILES[i]->attrLength1;
            memcpy(BlockData + offset, value2, OPENFILES[i]->attrLength2);        //2nd value in place
            memset(BlockData + 1,++*(BlockData + 1),1);                           //increase the byte with ne number of records
            BF_Block_SetDirty(block1);
            CALL_OR_DIE(BF_UnpinBlock(block1));
        }else{
            BF_Block *blockRight;
            BF_Block_Init(&blockRight);
            CALL_OR_DIE(BF_AllocateBlock(OPENFILES[i]->fileDesc, blockRight));
            OPENFILES[i]->blockid++;                                              //increasing the variable storing the last block allocated
            int lbid = *(BlockData + 2);
            BF_Block_SetDirty(block1);
            BF_Block_SetDirty(blockRight);
            CALL_OR_DIE(BF_UnpinBlock(blockRight));
            CALL_OR_DIE(BF_UnpinBlock(block1));
            BF_Block_Destroy(&blockRight);
            split_leaf(i, lbid, OPENFILES[i]->blockid);                           //split the leaf in the two blocks
            Insert_Key(i, lbid, OPENFILES[i]->blockid);                           //then connect the two new blocks with the upper block
            AM_InsertEntry(fileDesc, value1, value2);                             //Now insert the new value
        }
    }
    AM_errno = AME_OK;
    BF_Block_Destroy(&block1);
    return AM_errno;
}


int AM_OpenIndexScan(int fileDesc, int op, void *value) {
    AM_errno = AME_OK;
    int j;
    if(OPENFILES[fileDesc] == NULL){
        AM_errno = AME_FILENOTOPEN;													//the file I want to scan is not open
        return AM_errno;
    }
    if((op<1) || (op>6)){
        AM_errno = AME_WRONGOP;														//the operation given is wrong
        return AM_errno;
    }
    for(j = 0; j < MAX_OPEN_SCANS; j++){
        if (SCANFILES[j] == NULL) break;											//find the first NULL index of the SCANFILES array
    }
    if(j == MAX_OPEN_SCANS ){
        AM_errno = AME_MAXOPENSCANS;												//if the array of open scans is full print error
        return AM_errno;
    }
    SCANFILES[j] = malloc(sizeof(AM_Scan));											//create a new struct for the scan
    SCANFILES[j]->fileDesc = OPENFILES[fileDesc]->fileDesc;							//this is the number that BF_OpenFile returns
    SCANFILES[j]->open_file_id = fileDesc;											//this is the number of this file in the array OPENFILES
    SCANFILES[j]->op = op;
    SCANFILES[j]->size_of_value = OPENFILES[fileDesc]->attrLength1;
    SCANFILES[j]->type_of_value = OPENFILES[fileDesc]->attrType1;
    SCANFILES[j]->size_of_value2 = OPENFILES[fileDesc]->attrLength2;
    SCANFILES[j]->type_of_value2 = OPENFILES[fileDesc]->attrType2;
    SCANFILES[j]->size_of_record = OPENFILES[fileDesc]->attrLength1 + OPENFILES[fileDesc]->attrLength2;
    SCANFILES[j]->value =(void*)malloc(OPENFILES[fileDesc]->attrLength2);
    SCANFILES[j]->value = value;
    SCANFILES[j]->cur_block = OPENFILES[fileDesc]->root; 							//this is the current block
    SCANFILES[j]->cur_record = 0;													//this is the current record of the current block
    return j;
}



void *AM_FindNextEntry(int scanDesc) {
    if(SCANFILES[scanDesc] == NULL){
        AM_errno = AME_SCANNOTOPEN;													//if the scan is not open print error
        return NULL;
    }
    BF_Block* Scan_Block;
    BF_Block_Init(&Scan_Block);
    CALL_OR_DIE(BF_GetBlock(SCANFILES[scanDesc]->fileDesc, SCANFILES[scanDesc]->cur_block, Scan_Block));
    char* Data = BF_Block_GetData(Scan_Block);
    void* value2 = (void*)malloc(SCANFILES[scanDesc]->size_of_value2);
    int record = SCANFILES[scanDesc]->cur_record;
    int offset = 2 + 2*sizeof(int) + record*(SCANFILES[scanDesc]->size_of_record);	//make the offset in order to find the next record
    int check;																		//keep the result of the operation to do the check
    int next_block;																	//keep the id of the next data block
    switch(SCANFILES[scanDesc]->op){
        case(EQUAL):
            while(memcmp(Data,"b",1) == 0){											//while we are not in data block
                int num_of_keys = *(Data+1);
                int i = 0;
                int cur_offset = 2 + 2*sizeof(int);
                int cur_check;
                while(i<num_of_keys){												//find the next interior block you have to scan
                    cur_check = memcmp(Data + cur_offset, SCANFILES[scanDesc]->value, SCANFILES[scanDesc]->size_of_value);
                    if(cur_check > 0){
                        break;
                    }
                    cur_offset += SCANFILES[scanDesc]->size_of_value + sizeof(int);
                    i++;
                }
                cur_offset -= sizeof(int);
                next_block = *(Data + cur_offset);									//go to next interior block and repeat
                SCANFILES[scanDesc]->cur_block = next_block;
                CALL_OR_DIE(BF_UnpinBlock(Scan_Block));
                CALL_OR_DIE(BF_GetBlock(SCANFILES[scanDesc]->fileDesc, next_block, Scan_Block));
                Data = BF_Block_GetData(Scan_Block);
            }
            while(AM_errno == AME_OK){
                if(memcmp(&(SCANFILES[scanDesc]->type_of_value),"c",1) == 0){					//if our keys are strings, we use memcmp with strlen of our value
                    check = memcmp(Data+offset, SCANFILES[scanDesc]->value, strlen((char*)(SCANFILES[scanDesc]->value)));
                }else if(memcmp(&(SCANFILES[scanDesc]->type_of_value),"i",1) == 0){				//if our keys are integers, we use memcmp with sizeof(int)
                    check = memcmp(Data+offset, SCANFILES[scanDesc]->value, sizeof(int));
                }else if(memcmp(&(SCANFILES[scanDesc]->type_of_value),"f",1) == 0){				//if our keys are floats, we use float variables, cast our data to them and check them
                    float data_value, check_value;
                    data_value = *(float*)(Data + offset);
                    check_value = *(float*)(SCANFILES[scanDesc]->value);
                    if(data_value == check_value){
                        check = 0;
                    }else{
                        check = 1;
                    }
                }
                if(check == 0){																	//if our check is correct
                    offset += SCANFILES[scanDesc]->size_of_value;								//go to value2 to return it
                    memcpy(value2, Data + offset, SCANFILES[scanDesc]->size_of_value2);
                    offset += SCANFILES[scanDesc]->size_of_value2;
                }else{
                    offset += SCANFILES[scanDesc]->size_of_record;
                }
                SCANFILES[scanDesc]->cur_record++;												//increase the number of records
                if(SCANFILES[scanDesc]->cur_record == *(Data+1)){								//if we reached the last record of this block, go to the next one
                    next_block = *(Data + 2 + sizeof(int));										//find pointer to next data block
                    if(next_block==-1){
                        AM_errno = AME_EOF;														//if there is no other data block, return and stop
                    }else{
                        offset = 2 + 2*sizeof(int);
                        SCANFILES[scanDesc]->cur_block = next_block;
                        SCANFILES[scanDesc]->cur_record = 0;
                        CALL_OR_DIE(BF_UnpinBlock(Scan_Block));
                        CALL_OR_DIE(BF_GetBlock(SCANFILES[scanDesc]->fileDesc, next_block, Scan_Block));
                        Data = BF_Block_GetData(Scan_Block);
                    }
                }
                if(check == 0){
                    return value2;																//now return the value2
                }
            }
            CALL_OR_DIE(BF_UnpinBlock(Scan_Block));
            if(AM_errno == AME_EOF){
                return NULL;
            }
        case(NOT_EQUAL):
            while(memcmp(Data,"b",1) == 0){											//go down from root until we reach the first data block
                next_block = *(Data + 2 + sizeof(int));
                SCANFILES[scanDesc]->cur_block = next_block;
                CALL_OR_DIE(BF_UnpinBlock(Scan_Block));
                CALL_OR_DIE(BF_GetBlock(SCANFILES[scanDesc]->fileDesc, next_block, Scan_Block));
                Data = BF_Block_GetData(Scan_Block);
            }
            while(AM_errno == AME_OK){
                if(memcmp(&(SCANFILES[scanDesc]->type_of_value),"c",1) == 0){
                    check = memcmp(Data+offset, SCANFILES[scanDesc]->value, strlen((char*)(SCANFILES[scanDesc]->value)));
                }else if(memcmp(&(SCANFILES[scanDesc]->type_of_value),"i",1) == 0){
                    check = memcmp(Data+offset, SCANFILES[scanDesc]->value, sizeof(int));
                }else if(memcmp(&(SCANFILES[scanDesc]->type_of_value),"f",1) == 0){
                    float data_value, check_value;
                    data_value = *(float*)(Data + offset);
                    check_value = *(float*)(SCANFILES[scanDesc]->value);
                    if(data_value != check_value){
                        check = 1;
                    }else{
                        check = 0;
                    }
                }
                if(check != 0){
                    offset += SCANFILES[scanDesc]->size_of_value;		//go to value2 to print it
                    memcpy(value2, Data + offset, SCANFILES[scanDesc]->size_of_value2);
                    offset += SCANFILES[scanDesc]->size_of_value2;
                }else{
                    offset += SCANFILES[scanDesc]->size_of_record;
                }
                SCANFILES[scanDesc]->cur_record++;
                if(SCANFILES[scanDesc]->cur_record == *(Data+1)){			//go to next data block
                    next_block = *(Data + 2 + sizeof(int));					//find pointer to next data block
                    if(next_block==-1){
                        AM_errno = AME_EOF;
                    }else{
                        offset = 2 + 2*sizeof(int);
                        SCANFILES[scanDesc]->cur_block = next_block;
                        SCANFILES[scanDesc]->cur_record = 0;
                        CALL_OR_DIE(BF_UnpinBlock(Scan_Block));
                        CALL_OR_DIE(BF_GetBlock(SCANFILES[scanDesc]->fileDesc, next_block, Scan_Block));
                        Data = BF_Block_GetData(Scan_Block);
                    }
                }
                if(check != 0){
                    return value2;
                }
            }
            CALL_OR_DIE(BF_UnpinBlock(Scan_Block));
            return NULL;
        case(LESS_THAN):
            while(memcmp(Data,"b",1) == 0){											//until we reach data
                next_block = *(Data + 2 + sizeof(int));
                SCANFILES[scanDesc]->cur_block = next_block;
                CALL_OR_DIE(BF_UnpinBlock(Scan_Block));
                CALL_OR_DIE(BF_GetBlock(SCANFILES[scanDesc]->fileDesc, next_block, Scan_Block));
                Data = BF_Block_GetData(Scan_Block);
            }
            while(AM_errno == AME_OK){
                if(memcmp(&(SCANFILES[scanDesc]->type_of_value),"c",1) == 0){
                    check = memcmp(Data+offset, SCANFILES[scanDesc]->value, strlen((char*)(SCANFILES[scanDesc]->value)));
                }else if(memcmp(&(SCANFILES[scanDesc]->type_of_value),"i",1) == 0){
                    check = memcmp(Data+offset, SCANFILES[scanDesc]->value, sizeof(int));
                }else if(memcmp(&(SCANFILES[scanDesc]->type_of_value),"f",1) == 0){
                    float data_value, check_value;
                    data_value = *(float*)(Data + offset);
                    check_value = *(float*)(SCANFILES[scanDesc]->value);
                    if(data_value < check_value){
                        check = -1;
                    }else{
                        check = 1;
                    }
                }
                if(check < 0){
                    offset += SCANFILES[scanDesc]->size_of_value;		//go to value2 to print it
                    memcpy(value2, Data + offset, SCANFILES[scanDesc]->size_of_value2);
                    offset += SCANFILES[scanDesc]->size_of_value2;
                }else{
                    offset += SCANFILES[scanDesc]->size_of_record;
                }
                SCANFILES[scanDesc]->cur_record++;
                if(SCANFILES[scanDesc]->cur_record == *(Data+1)){			//go to next data block
                    next_block = *(Data + 2 + sizeof(int));					//find pointer to next data block
                    if(next_block==-1){
                        AM_errno = AME_EOF;
                    }else{
                        offset = 2 + 2*sizeof(int);
                        SCANFILES[scanDesc]->cur_block = next_block;
                        SCANFILES[scanDesc]->cur_record = 0;
                        CALL_OR_DIE(BF_UnpinBlock(Scan_Block));
                        CALL_OR_DIE(BF_GetBlock(SCANFILES[scanDesc]->fileDesc, next_block, Scan_Block));
                        Data = BF_Block_GetData(Scan_Block);
                    }
                }
                if(check < 0){
                    return value2;
                }
            }
            CALL_OR_DIE(BF_UnpinBlock(Scan_Block));
            return NULL;
        case(GREATER_THAN):
            while(memcmp(Data,"b",1) == 0){
                int num_of_keys = *(Data+1);
                int i = 0;
                int cur_offset = 2 + 2*sizeof(int);
                int cur_check;
                while(i<num_of_keys){
                    cur_check = memcmp(Data + cur_offset, SCANFILES[scanDesc]->value, SCANFILES[scanDesc]->size_of_value);
                    if(cur_check > 0){
                        break;
                    }
                    cur_offset += SCANFILES[scanDesc]->size_of_value + sizeof(int);
                    i++;
                }
                cur_offset -= sizeof(int);
                next_block = *(Data + cur_offset);
                SCANFILES[scanDesc]->cur_block = next_block;
                CALL_OR_DIE(BF_UnpinBlock(Scan_Block));
                CALL_OR_DIE(BF_GetBlock(SCANFILES[scanDesc]->fileDesc, next_block, Scan_Block));
                Data = BF_Block_GetData(Scan_Block);
            }
            while(AM_errno == AME_OK){
                if(memcmp(&(SCANFILES[scanDesc]->type_of_value),"c",1) == 0){
                    check = memcmp(Data+offset, SCANFILES[scanDesc]->value, strlen((char*)(SCANFILES[scanDesc]->value)));
                }else if(memcmp(&(SCANFILES[scanDesc]->type_of_value),"i",1) == 0){
                    check = memcmp(Data+offset, SCANFILES[scanDesc]->value, sizeof(int));
                }else if(memcmp(&(SCANFILES[scanDesc]->type_of_value),"f",1) == 0){
                    float data_value, check_value;
                    data_value = *(float*)(Data + offset);
                    check_value = *(float*)(SCANFILES[scanDesc]->value);
                    if(data_value > check_value){
                        check = 1;
                    }else{
                        check = -1;
                    }
                }
                if(check > 0){
                    offset += SCANFILES[scanDesc]->size_of_value;		//go to value2 to print it
                    memcpy(value2, Data + offset, SCANFILES[scanDesc]->size_of_value2);
                    offset += SCANFILES[scanDesc]->size_of_value2;
                }else{
                    offset += SCANFILES[scanDesc]->size_of_record;
                }
                SCANFILES[scanDesc]->cur_record++;
                if(SCANFILES[scanDesc]->cur_record == *(Data+1)){			//go to next data block
                    next_block = *(Data + 2 + sizeof(int));					//find pointer to next data block
                    if(next_block==-1){
                        AM_errno = AME_EOF;
                    }else{
                        offset = 2 + 2*sizeof(int);
                        SCANFILES[scanDesc]->cur_block = next_block;
                        SCANFILES[scanDesc]->cur_record = 0;
                        CALL_OR_DIE(BF_UnpinBlock(Scan_Block));
                        CALL_OR_DIE(BF_GetBlock(SCANFILES[scanDesc]->fileDesc, next_block, Scan_Block));
                        Data = BF_Block_GetData(Scan_Block);
                    }
                }
                if(check > 0){
                    return value2;
                }
            }
            CALL_OR_DIE(BF_UnpinBlock(Scan_Block));
            return NULL;
        case(LESS_THAN_OR_EQUAL):
            while(memcmp(Data,"b",1) == 0){											//until we reach data
                next_block = *(Data + 2 + sizeof(int));
                SCANFILES[scanDesc]->cur_block = next_block;
                CALL_OR_DIE(BF_UnpinBlock(Scan_Block));
                CALL_OR_DIE(BF_GetBlock(SCANFILES[scanDesc]->fileDesc, next_block, Scan_Block));
                Data = BF_Block_GetData(Scan_Block);
            }
            while(AM_errno == AME_OK){
                if(memcmp(&(SCANFILES[scanDesc]->type_of_value),"c",1) == 0){
                    check = memcmp(Data+offset, SCANFILES[scanDesc]->value, strlen((char*)(SCANFILES[scanDesc]->value)));
                }else if(memcmp(&(SCANFILES[scanDesc]->type_of_value),"i",1) == 0){
                    check = memcmp(Data+offset, SCANFILES[scanDesc]->value, sizeof(int));
                }else if(memcmp(&(SCANFILES[scanDesc]->type_of_value),"f",1) == 0){
                    float data_value, check_value;
                    data_value = *(float*)(Data + offset);
                    check_value = *(float*)(SCANFILES[scanDesc]->value);
                    if(data_value <= check_value){
                        check = 0;
                    }else{
                        check = 1;
                    }
                }
                if(check <= 0){
                    offset += SCANFILES[scanDesc]->size_of_value;		//go to value2 to print it
                    memcpy(value2, Data + offset, SCANFILES[scanDesc]->size_of_value2);
                    offset += SCANFILES[scanDesc]->size_of_value2;
                }else{
                    offset += SCANFILES[scanDesc]->size_of_record;
                }
                SCANFILES[scanDesc]->cur_record++;
                if(SCANFILES[scanDesc]->cur_record == *(Data+1)){			//go to next data block
                    next_block = *(Data + 2 + sizeof(int));					//find pointer to next data block
                    if(next_block==-1){
                        AM_errno = AME_EOF;
                    }else{
                        offset = 2 + 2*sizeof(int);
                        SCANFILES[scanDesc]->cur_block = next_block;
                        SCANFILES[scanDesc]->cur_record = 0;
                        CALL_OR_DIE(BF_UnpinBlock(Scan_Block));
                        CALL_OR_DIE(BF_GetBlock(SCANFILES[scanDesc]->fileDesc, next_block, Scan_Block));
                        Data = BF_Block_GetData(Scan_Block);
                    }
                }
                if(check <= 0){
                    return value2;
                }
            }
            CALL_OR_DIE(BF_UnpinBlock(Scan_Block));
            return NULL;
        case(GREATER_THAN_OR_EQUAL):
            while(memcmp(Data,"b",1) == 0){
                int num_of_keys = *(Data+1);
                int i = 0;
                int cur_offset = 2 + 2*sizeof(int);
                int cur_check;
                while(i<num_of_keys){
                    cur_check = memcmp(Data + cur_offset, SCANFILES[scanDesc]->value, SCANFILES[scanDesc]->size_of_value);
                    if(cur_check > 0){
                        break;
                    }
                    cur_offset += SCANFILES[scanDesc]->size_of_value + sizeof(int);
                    i++;
                }
                cur_offset -= sizeof(int);
                next_block = *(Data + cur_offset);
                SCANFILES[scanDesc]->cur_block = next_block;
                CALL_OR_DIE(BF_UnpinBlock(Scan_Block));
                CALL_OR_DIE(BF_GetBlock(SCANFILES[scanDesc]->fileDesc, next_block, Scan_Block));
                Data = BF_Block_GetData(Scan_Block);
            }
            while(AM_errno == AME_OK){
                if(memcmp(&(SCANFILES[scanDesc]->type_of_value),"c",1) == 0){
                    check = memcmp(Data+offset, SCANFILES[scanDesc]->value, strlen((char*)(SCANFILES[scanDesc]->value)));
                }else if(memcmp(&(SCANFILES[scanDesc]->type_of_value),"i",1) == 0){
                    check = memcmp(Data+offset, SCANFILES[scanDesc]->value, sizeof(int));
                }else if(memcmp(&(SCANFILES[scanDesc]->type_of_value),"f",1) == 0){
                    float data_value, check_value;
                    data_value = *(float*)(Data + offset);
                    check_value = *(float*)(SCANFILES[scanDesc]->value);
                    if(data_value >= check_value){
                        check = 1;
                    }else{
                        check = -1;
                    }
                }
                if(check >= 0){
                    offset += SCANFILES[scanDesc]->size_of_value;		//go to value2 to print it
                    memcpy(value2, Data + offset, SCANFILES[scanDesc]->size_of_value2);
                    offset += SCANFILES[scanDesc]->size_of_value2;
                }else{
                    offset += SCANFILES[scanDesc]->size_of_record;
                }
                SCANFILES[scanDesc]->cur_record++;
                if(SCANFILES[scanDesc]->cur_record == *(Data+1)){			//go to next data block
                    next_block = *(Data + 2 + sizeof(int));					//find pointer to next data block
                    if(next_block==-1){
                        AM_errno = AME_EOF;
                    }else{
                        offset = 2 + 2*sizeof(int);
                        SCANFILES[scanDesc]->cur_block = next_block;
                        SCANFILES[scanDesc]->cur_record = 0;
                        CALL_OR_DIE(BF_UnpinBlock(Scan_Block));
                        CALL_OR_DIE(BF_GetBlock(SCANFILES[scanDesc]->fileDesc, next_block, Scan_Block));
                        Data = BF_Block_GetData(Scan_Block);
                    }
                }
                if(check >= 0){
                    return value2;
                }
            }
            CALL_OR_DIE(BF_UnpinBlock(Scan_Block));
            return NULL;
    }
}



int AM_CloseIndexScan(int scanDesc) {
	AM_errno = AME_OK;
	if(SCANFILES[scanDesc] == NULL){
		AM_errno = AME_SCANNOTOPEN;												//if the scan we are going to close is not open, print error
		return AM_errno;
	}
	free(SCANFILES[scanDesc]);
	SCANFILES[scanDesc] = NULL;													//delete the struct and clear its space in the SCANFILES array
  	return AM_errno;
}



void AM_PrintError(char *errString) {
	printf("%s\n", errString);
	if(AM_errno==AME_WRONGTYPE1){
		printf("Invalid attrType1!\n");
	}else if(AM_errno==AME_WRONGTYPE2){
		printf("Invalid attrType2!\n");
	}else if(AM_errno==AME_WRONGLENGTH1){
		printf("Invalid attrLength1!\n");
	}else if(AM_errno==AME_WRONGLENGTH2){
		printf("Invalid attrLength2!\n");
	}else if(AM_errno==AME_FILEOPEN){
		printf("File is open!\n");
	}else if(AM_errno==AME_FILENOTOPEN){
		printf("File is not open!\n");
	}else if(AM_errno==AME_MAXOPENFILES){
		printf("Reached MAX_OPEN_FILES limit!\n");
	}else if(AM_errno==AME_MAXOPENSCANS){
		printf("Reached MAX_OPEN_SCANS limit!\n");
	}else if(AM_errno==AME_SCANOPEN){
		printf("File has open scans!\n");
	}else if(AM_errno==AME_SCANNOTOPEN){
		printf("There is no such scanDesc!\n");
	}else if(AM_errno==AME_DESTROYINDEX){
		printf("Index was not destroyed!\n");
	}else if(AM_errno==AME_WRONGOP){
		printf("There is no such operation!\n");
	}else if(AM_errno==AME_EOF){
		printf("END OF FILE!\n");
	}
}



void AM_Close() {
	CALL_OR_DIE(BF_Close());
}
