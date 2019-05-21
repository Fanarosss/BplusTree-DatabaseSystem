#ifndef AM_H_
#define AM_H_

/* Error codes */

#include "bf.h"

extern int AM_errno;

#define AME_OK 0
#define AME_EOF -1
#define AME_WRONGTYPE1 1
#define AME_WRONGTYPE2 2
#define AME_WRONGLENGTH1 3
#define AME_WRONGLENGTH2 4
#define AME_FILEOPEN 5
#define AME_FILENOTOPEN 6
#define AME_MAXOPENFILES 7
#define AME_MAXOPENSCANS 8
#define AME_SCANOPEN 9
#define AME_SCANNOTOPEN 10
#define AME_DESTROYINDEX 11
#define AME_WRONGOP 12

#define EQUAL 1
#define NOT_EQUAL 2
#define LESS_THAN 3
#define GREATER_THAN 4
#define LESS_THAN_OR_EQUAL 5
#define GREATER_THAN_OR_EQUAL 6

#define MAX_OPEN_SCANS 20
#define MAX_OPEN_FILES 20

#define CALL_OR_DIE(call)     \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK) {      \
      BF_PrintError(code);    \
      exit(code);             \
    }                         \
  }

typedef struct Scan{
    int fileDesc;					   //this is the number that BF_OpenFile returns
    int open_file_id;				   //this is the number of this file in the array OPENFILES
    int op;							   //this is our operation number
    int size_of_value;				   //size of key
    char type_of_value;				   //type of key
    int size_of_value2;				   //size of value 2
    char type_of_value2;		   	   //type of value 2
    int size_of_record;				   //size of record = size_of_value + size_of_value2
    void* value;					   //the value we want to execute the operation with
    int cur_block;					   //the last block we visited
    int cur_record;					   //the last record of the last block we visited
} AM_Scan;

typedef struct Index{
    int fileDesc;						//this is the number that BF_OpenFile returns
    char* fileName;					    //the name of the file
    char attrType1;					    //type of attribute 1
    int attrLength1;					//length of attribute 1
    char attrType2;					    //type of attribute 2
    int attrLength2;					//length of attribute 2
    int root;							//the root of our B+ tree
    int blockid;						//the last allocated block
} AM_Index;


AM_Index *OPENFILES[MAX_OPEN_FILES];    //array of pointers to our struct of open files
AM_Scan *SCANFILES[MAX_OPEN_SCANS];     //array of pointers to our struct of open scans

void AM_Init( void );


int AM_CreateIndex(
  char *fileName, /* όνομα αρχείου */
  char attrType1, /* τύπος πρώτου πεδίου: 'c' (συμβολοσειρά), 'i' (ακέραιος), 'f' (πραγματικός) */
  int attrLength1, /* μήκος πρώτου πεδίου: 4 γιά 'i' ή 'f', 1-255 γιά 'c' */
  char attrType2, /* τύπος πρώτου πεδίου: 'c' (συμβολοσειρά), 'i' (ακέραιος), 'f' (πραγματικός) */
  int attrLength2 /* μήκος δεύτερου πεδίου: 4 γιά 'i' ή 'f', 1-255 γιά 'c' */
);


int AM_DestroyIndex(
  char *fileName /* όνομα αρχείου */
);


int AM_OpenIndex (
  char *fileName /* όνομα αρχείου */
);


int AM_CloseIndex (
  int fileDesc /* αριθμός που αντιστοιχεί στο ανοιχτό αρχείο */
);


int AM_InsertEntry(
  int fileDesc, /* αριθμός που αντιστοιχεί στο ανοιχτό αρχείο */
  void *value1, /* τιμή του πεδίου-κλειδιού προς εισαγωγή */
  void *value2 /* τιμή του δεύτερου πεδίου της εγγραφής προς εισαγωγή */
);

int AM_OpenIndexScan(
  int fileDesc, /* αριθμός που αντιστοιχεί στο ανοιχτό αρχείο */
  int op, /* τελεστής σύγκρισης */
  void *value /* τιμή του πεδίου-κλειδιού προς σύγκριση */
);


void *AM_FindNextEntry(
  int scanDesc /* αριθμός που αντιστοιχεί στην ανοιχτή σάρωση */
);


int AM_CloseIndexScan(
  int scanDesc /* αριθμός που αντιστοιχεί στην ανοιχτή σάρωση */
);


void AM_PrintError(
  char *errString /* κείμενο για εκτύπωση */
);

void AM_Close();


#endif /* AM_H_ */
