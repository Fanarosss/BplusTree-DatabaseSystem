#include "AM.h"
#include "bf.h"
#include "BplusTree.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"


void Insert_Key(int i, int blockL_id, int blockR_id){BF_Block* blockL;
    BF_Block* blockR;
    BF_Block_Init(&blockL);
    BF_Block_Init(&blockR);
    CALL_OR_DIE(BF_GetBlock(OPENFILES[i]->fileDesc,blockL_id,blockL));
    CALL_OR_DIE(BF_GetBlock(OPENFILES[i]->fileDesc,blockR_id,blockR));
    int size_of_key = OPENFILES[i]->attrLength1;
    int size_of_record = OPENFILES[i]->attrLength1 + OPENFILES[i]->attrLength2;
    char *blockLData = BF_Block_GetData(blockL);
    char* blockRData = BF_Block_GetData(blockR);
    if( *(blockLData + 2) != OPENFILES[i]->root){
        BF_Block *UpperBlock;
        BF_Block_Init(&UpperBlock);
        int UBnum;
        AM_Find_Block(i, OPENFILES[i]->root, *(blockLData + 2), &UBnum);      //finds block which have a pointer to blockL
        CALL_OR_DIE(BF_GetBlock(OPENFILES[i]->fileDesc, UBnum, UpperBlock));
        char* UpperBlockData = BF_Block_GetData(UpperBlock);
        size_t used_space = 2 + sizeof(int) + *(UpperBlockData + 1)*size_of_key + (*(UpperBlockData + 1) + 1)*sizeof(int);
        size_t available_space = BF_BLOCK_SIZE - used_space;
        if ((available_space > (size_of_key + sizeof(int)))){                  //if there is space for another key
            if (memcmp(blockRData,"d",1) == 0){                                //if we are connecting data block
                size_t offset = 2 + 2*sizeof(int);
                int processed_keys = 0;                                       //this is a counter to know how many keys i will have to move
                if (memcmp(&(OPENFILES[i]->attrType1),"f",1) == 0){            //in case of float memcmp doesnt work, so I have to cast them
                  float f1,f2;
                  f1 = *(float*)(blockRData + 2 + 2*sizeof(int));
                  f2 = *(float*)(UpperBlockData + offset);
                  while((f1 >= f2) && (processed_keys < *(UpperBlockData + 1))){ //until I find the key with biger value than value1
                      offset += size_of_key + sizeof(int);
                      f2 = *(float*)(UpperBlockData + offset);
                      processed_keys++;
                }
                }else{
                    while ((memcmp(UpperBlockData + offset, blockRData + 2 + 2*sizeof(int) , OPENFILES[i]->attrLength1) < 0) && (processed_keys < *(UpperBlockData + 1))){ //value1 > blockdata + offset
                        offset += size_of_key + sizeof(int);
                        processed_keys++;
                    }
                }
                if (processed_keys != *(UpperBlockData + 1)){                 //if I put my data at the end I dont have to move anything
                         memmove(UpperBlockData + offset + size_of_key + sizeof(int), UpperBlockData + offset, (*(UpperBlockData + 1) - processed_keys)*size_of_key + (*(UpperBlockData + 1) - processed_keys + 1)*sizeof(int));
                }
                memcpy(UpperBlockData + offset, blockRData + 2 + 2*sizeof(int), OPENFILES[i]->attrLength1); //taking the first value of the right below block
                memcpy(UpperBlockData + offset + size_of_key, blockRData + 2, sizeof(int)); //fix pointers
                memset(UpperBlockData + 1, ++*(UpperBlockData + 1), 1);       //fix num of keys
                BF_Block_SetDirty(blockL);
                BF_Block_SetDirty(blockR);
                CALL_OR_DIE(BF_UnpinBlock(blockL));
                CALL_OR_DIE(BF_UnpinBlock(blockR));
            }else{                                                              //if we are connecting index blocks
                size_t offset = 2 + 2*sizeof(int);
                int processed_keys = 0;                                         //this is a counter to know how many keys i will have to move
                  if(memcmp(&(OPENFILES[i]->attrType1),"f",1) == 0){
                      float f1,f2;
                      f1 = *(float*)(blockLData + 2 + sizeof(int) + *(blockLData + 1)*sizeof(int) + (*(blockLData + 1)-1)*size_of_key);
                      f2 = *(float*)(UpperBlockData + offset);
                      while((f1 >= f2) && (processed_keys < *(UpperBlockData + 1))){ //until i find the key with biger value than value1
                          offset += size_of_key + sizeof(int);
                          f2 = *(float*)(UpperBlockData + offset);
                          processed_keys++;
                    }
                  }else{
                      while((memcmp(UpperBlockData + offset, blockLData + 2 + sizeof(int) + *(blockLData + 1)*sizeof(int) + (*(blockLData + 1)-1)*size_of_key , OPENFILES[i]->attrLength1) < 0) && (processed_keys < *(UpperBlockData + 1))){ //value1 > blockdata + offset
                          offset += size_of_key + sizeof(int);
                          processed_keys++;
                      }
                  }
                memmove(UpperBlockData + offset + size_of_key + sizeof(int), UpperBlockData + offset, (*(blockLData + 1) - processed_keys)*size_of_key + (*(blockLData + 1) - processed_keys)*sizeof(int));
                memcpy(UpperBlockData + offset, blockLData + 2 + sizeof(int) + *(blockLData + 1)*sizeof(int) + (*(blockLData + 1)-1)*size_of_key , OPENFILES[i]->attrLength1); //the last value of left block is middle one of the old one
                memcpy(UpperBlockData + offset + size_of_key, &OPENFILES[i]->blockid, sizeof(int));
                memset(UpperBlockData + 1, ++*(UpperBlockData + 1), 1);
                memset(blockLData + 1, --*(blockLData + 1), 1);                 //reduce the number of keys in the left block without deleting it
            }
            BF_Block_SetDirty(UpperBlock);
            BF_Block_SetDirty(blockL);
            BF_Block_SetDirty(blockR);
            CALL_OR_DIE(BF_UnpinBlock(UpperBlock));
            CALL_OR_DIE(BF_UnpinBlock(blockL));
            CALL_OR_DIE(BF_UnpinBlock(blockR));
            BF_Block_Destroy(&UpperBlock);
        }else{
            BF_Block *UpperBlockR;
            BF_Block_Init(&UpperBlockR);
            CALL_OR_DIE(BF_AllocateBlock(OPENFILES[i]->fileDesc, UpperBlockR));
            OPENFILES[i]->blockid++;
            int lbid = *(UpperBlockData + 2);
            BF_Block_SetDirty(blockL);
            BF_Block_SetDirty(blockR);
            CALL_OR_DIE(BF_UnpinBlock(blockL));
            CALL_OR_DIE(BF_UnpinBlock(blockR));
            BF_Block_SetDirty(UpperBlock);
            BF_Block_SetDirty(UpperBlockR);
            CALL_OR_DIE(BF_UnpinBlock(UpperBlock));
            CALL_OR_DIE(BF_UnpinBlock(UpperBlockR));
            BF_Block_Destroy(&UpperBlockR);
            BF_Block_Destroy(&UpperBlock);
            split_interior(i, lbid, OPENFILES[i]->blockid);                 //splitting the interior node
            Insert_Key(i, lbid, OPENFILES[i]->blockid);                     //extract the key that goes up one level and insert in in the apropriate node
        }
    }else{                                                                    //this is the case there the root is splitting, creating a new one
        BF_Block *RootBlock;
        BF_Block_Init(&RootBlock);
        BF_AllocateBlock(OPENFILES[i]->fileDesc,RootBlock);
        OPENFILES[i]->blockid++;
        char* RootBlockData = BF_Block_GetData(RootBlock);
        memcpy(RootBlockData,"b",1);
        memset(RootBlockData + 1,1,1);
        memcpy(RootBlockData + 2, &OPENFILES[i]->blockid, sizeof(int));
        if(memcmp(blockRData,"d",1) == 0){
          size_t offset = 2 + 2*sizeof(int);
          memcpy(RootBlockData + offset, blockRData + 2 + 2*sizeof(int) , OPENFILES[i]->attrLength1); //taking the first value of the below block
          memcpy(RootBlockData + offset + size_of_key, blockRData + 2, sizeof(int)); //fix the pointers
          memcpy(RootBlockData + offset - sizeof(int), blockLData + 2, sizeof(int)); //fix the pointers
        }else{
          size_t offset = 2 + 2*sizeof(int);
          memcpy(RootBlockData + offset, blockLData + 2 + sizeof(int) + *(blockLData + 1)*sizeof(int) + (*(blockLData + 1)-1)*size_of_key , OPENFILES[i]->attrLength1); //taking the first value of the below block
          memcpy(RootBlockData + offset + size_of_key, blockRData + 2, sizeof(int));
          memcpy(RootBlockData + offset - sizeof(int), blockLData + 2, sizeof(int));
          memset(blockLData + 1, --*(blockLData + 1), 1);                   //reduce the number of keys in the left block without deleting it
        }
        OPENFILES[i]->root = OPENFILES[i]->blockid;
        BF_Block_SetDirty(RootBlock);
        BF_UnpinBlock(RootBlock);
        BF_Block_Destroy(&RootBlock);
    }
    BF_Block_SetDirty(blockL);
    BF_Block_SetDirty(blockR);
    BF_UnpinBlock(blockL);
    BF_UnpinBlock(blockR);
    BF_Block_Destroy(&blockL);
    BF_Block_Destroy(&blockR);
    return;
 }


void split_leaf(int i, int block1_id, int block3_id){                           //splitting leaf node in block1 and block3
    BF_Block *block1;
    BF_Block *block3;
    BF_Block_Init(&block1);
    BF_Block_Init(&block3);
    BF_GetBlock(OPENFILES[i]->fileDesc,block1_id,block1);
    BF_GetBlock(OPENFILES[i]->fileDesc,block3_id,block3);
    int size_of_key = OPENFILES[i]->attrLength1;
    int size_of_record = OPENFILES[i]->attrLength1 + OPENFILES[i]->attrLength2;
    char* BlockData = BF_Block_GetData(block1);
    char* BlockData3 = BF_Block_GetData(block3);
    int num_of_records = *(BlockData + 1);
    int MidRec = num_of_records / 2;                                            //the mid_rec key goes up one level
    size_t offset = MidRec*size_of_record + 2 + 2*sizeof(int);                  //half of the records to be moved
    memcpy(BlockData3,"d",1);                                                   //setting it data level block
    memcpy(BlockData3 + 2,&OPENFILES[i]->blockid,sizeof(int));
    memcpy(BlockData3 + 2 + 2*sizeof(int), BlockData + offset, (*(BlockData + 1) - MidRec)*size_of_record); // move the records from the olf block to the new one
    memcpy(BlockData3 + 2 + sizeof(int), BlockData + 2 + sizeof(int), sizeof(int)); //fix the pointers
    memcpy(BlockData + 2 + sizeof(int), &OPENFILES[i]->blockid, sizeof(int));   //fix pointers
    memset(BlockData3 + 1, *(BlockData + 1) - MidRec, 1);                       //increase the number of records in the new block
    memset(BlockData + 1, MidRec, 1);                                           //reduce the records of the old block
    BF_Block_SetDirty(block1);
    BF_Block_SetDirty(block3);
    CALL_OR_DIE(BF_UnpinBlock(block1));
    CALL_OR_DIE(BF_UnpinBlock(block3));
    BF_Block_Destroy(&block1);
    BF_Block_Destroy(&block3);
    return;
  }

void split_interior(int i, int block1_id, int block3_id){                       //keeps on block1 all keys until the middle one and moves the rest to a new one
    BF_Block *block1;
    BF_Block *block3;
    BF_Block_Init(&block1);
    BF_Block_Init(&block3);
    BF_GetBlock(OPENFILES[i]->fileDesc,block1_id,block1);
    BF_GetBlock(OPENFILES[i]->fileDesc,block3_id,block3);
    int size_of_key = OPENFILES[i]->attrLength1;
    int size_of_record = OPENFILES[i]->attrLength1 + OPENFILES[i]->attrLength2;
    char *BlockData = BF_Block_GetData(block1);
    char* BlockData3 = BF_Block_GetData(block3);
    int num_of_keys = *(BlockData + 1);
    int MidKey = num_of_keys / 2;                                               //the mid_rec key goes up one level
    if((num_of_keys % 2) == 1)    MidKey++;                                     //+1 because i want an extra more on he left, to move it up
    size_t offset = (MidKey)*size_of_key + 2 + sizeof(int) + MidKey*sizeof(int); //half of the records to be moved
    memcpy(BlockData3,"b",1);                                                   //setting it interior block
    memcpy(BlockData3 + 2, &OPENFILES[i]->blockid, sizeof(int));                //setting its id
    memcpy(BlockData3 + 2 + sizeof(int), BlockData + offset, (*(BlockData + 1) - MidKey)*size_of_key + (*(BlockData + 1) - MidKey + 1)*sizeof(int)); // move the records from the old block to the new one
    memset(BlockData3 + 1, *(BlockData + 1) - MidKey, 1);                       //increase the number of records in the new block
    memset(BlockData + 1, MidKey, 1);                                           //reduce the keys of the old block
    BF_Block_SetDirty(block1);
    BF_Block_SetDirty(block3);
    CALL_OR_DIE(BF_UnpinBlock(block1));
    CALL_OR_DIE(BF_UnpinBlock(block3));
    BF_Block_Destroy(&block1);
    BF_Block_Destroy(&block3);
    return;
  }



int AM_Traverse(int i, int block1, void* value1){                               //traversing throught the tree unti leaf, where we will put the new entry
    BF_Block *block;
    BF_Block_Init(&block);
    int cmp_size;
    CALL_OR_DIE(BF_GetBlock(OPENFILES[i]->fileDesc,block1, block));
    char *BlockData = BF_Block_GetData(block);
    if(memcmp(BlockData,"d",1) == 0){                                           //I reached the data block
        int result_id = *(int*)(BlockData + 2);
        CALL_OR_DIE(BF_UnpinBlock(block));
        BF_Block_Destroy(&block);
        return result_id;
    }
    size_t offset = 2 + 2*sizeof(int);
    int key_count = 0;
    if(memcmp(&(OPENFILES[i]->attrType1),"c",1) == 0){                          //because i care about the exact value of memcmp I cast the variables
        cmp_size = strlen((char*)(value1));                                     //without strlen, memcmp should never return 0, because attrLength is bigger than the actual length
        while((memcmp(BlockData + offset, value1, cmp_size) <= 0) && (*(BlockData + 1) > key_count)){ //until i find the key with biger value than value1
            offset += OPENFILES[i]->attrLength1 + sizeof(int);
            key_count++;
        }
    }else if(memcmp(&(OPENFILES[i]->attrType1),"i",1) == 0){
        cmp_size = sizeof(int);
        while((memcmp(BlockData + offset, value1, cmp_size) <= 0) && (*(BlockData + 1) > key_count)){ //until i find the key with biger value than value1
            offset += OPENFILES[i]->attrLength1 + sizeof(int);
            key_count++;
        }
    }else if(memcmp(&(OPENFILES[i]->attrType1),"f",1) == 0){
        float f1,f2;
        f1 = *(float*)(value1);
        f2 = *(float*)(BlockData + offset);
        while((f1 >= f2) && (*(BlockData + 1) > key_count)){                    //until i find the key with biger value than value1
            offset += OPENFILES[i]->attrLength1 + sizeof(int);
            f2 = *(float*)(BlockData + offset);
            key_count++;
        }
      }
      offset = offset - sizeof(int);
      int LowerBlock = 0;
      LowerBlock =  *(BlockData + offset);                                      //copy the adress of block in LowerBlock
      CALL_OR_DIE(BF_UnpinBlock(block));
      BF_Block_Destroy(&block);
      return AM_Traverse( i, LowerBlock, value1);
}


int AM_Find_Block(int i, int root, int block_id, int *Destination){             //find the block which has a pointer to block1
     BF_Block *BlockRoot;
     BF_Block_Init(&BlockRoot);
     CALL_OR_DIE(BF_GetBlock(OPENFILES[i]->fileDesc, root, BlockRoot));
     char* BlockData = BF_Block_GetData(BlockRoot);
     if(memcmp(BlockData,"d",1) == 0){
         CALL_OR_DIE(BF_UnpinBlock(BlockRoot));
         BF_Block_Destroy(&BlockRoot);
         return AME_EOF;
     }
     size_t offset = 2 + sizeof(int);
     int key_count = 0;
     while(*(BlockData + 1) >= key_count){                                      //until i find the key with bigger value than value1
         if(*(BlockData + offset) != block_id){
             if(AM_Find_Block(i, *(BlockData + offset), block_id, Destination) == AME_OK){
                 CALL_OR_DIE(BF_UnpinBlock(BlockRoot));
                 BF_Block_Destroy(&BlockRoot);
                 return AME_OK;
             }
             offset += sizeof(int) + OPENFILES[i]->attrLength1;                 //if find blocks doesnt return ok, means the in this subtree block_id
             key_count++;                                                       //was not found, so I increase offset
         }else{
              CALL_OR_DIE(BF_UnpinBlock(BlockRoot));
              BF_Block_Destroy(&BlockRoot);
              *Destination = root;                                              //store the value in destination
              return AME_OK;
         }
     }
     CALL_OR_DIE(BF_UnpinBlock(BlockRoot));
     BF_Block_Destroy(&BlockRoot);
     return AME_EOF;
}
