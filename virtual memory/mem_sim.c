#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include "mem_sim.h" 
#include <string.h>


#define NUM_OF_FRAMES 16
#define PAGE_SIZE 32
#define TOTAL_PAGES 128


typedef struct page_descriptor
{
unsigned int valid; //is page mapped to frame
unsigned int permission; //READ or READ-WRITE
unsigned int dirty; //has the page changed
unsigned int frame; //number of frame if page mapped 
unsigned int isMemAllocated; // bit that say if we memory allocate in the heap and stack area.. 0 is not , 1 yes
unsigned int isInSwap; 

} page_descriptor_t;




typedef struct sim_database
{
    page_descriptor_t* page_table; //pointer to page table
    char* swapfile_name; //name of swap file
    int swapfile_fd; //swap file fd
    char* executable_name; //name of executable file
    int executable_fd; //executable file fd

}sim_database;



int bitMap[NUM_OF_FRAMES][3];
char path[400]; // string that save the path
char exePath[400];
char ramMen[NUM_OF_FRAMES*PAGE_SIZE]; // the ram data base
int freeFrameNum;// 
int sumOfprogramMem;

int hitsCounter =0;
int missCounter =0;
int loadAndStoreCounter =0;
int wrongAdressCounter =0;
int wrongPermissionCounter =0;
int sumOfprogramMem=0;
char zeroChar = 0;

static char* readCharFromRam( unsigned char *p_char,int pageNum,int offset);
static void writeCharOnRam(int frameNum,int offset,sim_database_t* sim_db,unsigned char value,int pageNum);
static int freeFrameInBitMap(); 
static int copyPageToRam(sim_database_t* sim_db,int pageNum,int isFromSwap);
static int lru(sim_database_t* sim_db);
static void bitMapAgeUpdate();
static  void printRam();
static void printPageTable(sim_database_t *sim_db);

// ctor 
sim_database_t* vm_constructor(char* executable, int text_size, int data_size, int bss_size)
{

    int i, ret;
     sumOfprogramMem=(text_size+data_size+bss_size)*PAGE_SIZE;

       if (executable == NULL || strcmp(executable,"")==0)
       {
          free(executable);
           return NULL;
       }

       // init bit map
       for (i = 0; i < NUM_OF_FRAMES; ++i)
       {
          bitMap[i][0]=0; //free frame
          bitMap[i][1]=0; // age is 0
          bitMap[i][2]=-1; // no page number
       }



       //init ram men
       for (i = 0; i < PAGE_SIZE*NUM_OF_FRAMES; ++i)
       {
        //strcpy(ramMen[i],&zeroChar);
        ramMen[i]=(char)zeroChar;
       }

       // allocating data base 
       sim_database_t *db = (sim_database_t*)malloc(sizeof(sim_database_t));
       if (db==NULL)
       {
        perror("problem while allocating data base");
         return NULL;
       }


       // allocating data table
       db->page_table=(page_descriptor_t*) malloc(TOTAL_PAGES*sizeof(page_descriptor_t)); // create a page table
       if (db->page_table==NULL)
       {
        perror("problem while allocating page table ");
         vm_destructor(db);
         return NULL;
       }
       


       // fill the page table from 0 to TOTAL_PAGES
        for (i = 0; i < TOTAL_PAGES; ++i)
        {
          if(i < sumOfprogramMem)
          {
            if (i<text_size)
                db->page_table[i].permission=1;
              
            else
                db->page_table[i].permission=0;
   

            db->page_table[i].valid=0;
            db->page_table[i].dirty=0;
            db->page_table[i].frame=-1;
            db->page_table[i].isMemAllocated=0;
            db->page_table[i].isInSwap=0;
          }

          //heap and stuck area
          else
          {
            db->page_table[i].permission=-1;
            db->page_table[i].valid=-1;
            db->page_table[i].dirty=-1;
            db->page_table[i].frame=-1;
            db->page_table[i].isMemAllocated=0;
            db->page_table[i].isInSwap=-1;

          }
            
        }


        //set the path and create the swap file
        db->swapfile_name=(char*) malloc(8 * sizeof(char));// allocate memory to swap name
        if(db->swapfile_name==NULL)
        {
          perror("problem while allocating memory");
          vm_destructor(db);
          return NULL;
        }

        // set a new path 
        if (getcwd(path, sizeof(path)) == NULL)
        {
          perror("getcwd() error");
          vm_destructor(db);
          return NULL;
        }              
              

        strcpy(db->swapfile_name,"swapMem");
        strcat(path , "/");
        strcat(path, db->swapfile_name);

        // create swap file that can read and write
        ret=db->swapfile_fd = open ( path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR) ; 
        if(ret==-1)
        {  
          perror("problem while open swap file");
          vm_destructor(db);
          return NULL;
        }
        

        // init the swap file memory to zero chars
        for (i = 0; i < TOTAL_PAGES * PAGE_SIZE; ++i)
        {
            ret= write(db->swapfile_fd,&zeroChar,sizeof(char)); 
              if(ret ==-1)
              {
                vm_destructor(db);
                perror("problem while write to swap file\n");
                return NULL;
              }
        }



        //set the exe path and create the exe file 
        db->executable_name=(char*) malloc(strlen(executable) * sizeof(char));// allocate memory to swap name
        if(db->executable_name==NULL)
        {
          perror("problem while allocating memory");
          vm_destructor(db);
          return NULL;
        }


        //set a new path
        if (getcwd(exePath, sizeof(exePath)) == NULL)
        {
           perror("getcwd() error");
           vm_destructor(db);
           return NULL;
        }
        

        strcpy(db->executable_name,executable);
        strcat(exePath , "/");
        strcat(exePath, db->executable_name);

       
        db->executable_fd = open ( exePath, O_RDONLY) ;
        if (db->executable_fd ==-1)
        {
          perror("problem while open exe file");
          vm_destructor(db);
          return NULL;
        } 

 return db;
}
//=========================================================================================

//=========================================================================================

int vm_load(sim_database_t *sim_db, unsigned short address, unsigned char *p_char)
{
  loadAndStoreCounter++;
  int pageNum , offset, frameNum,ret;

  // check if our random adress is in range
  if (address < 0 || address >= TOTAL_PAGES*PAGE_SIZE)
  {
    missCounter++;
    wrongAdressCounter++;
    perror("address is not in executable range(load)");
    return -1;
  }

  //convert address to binary (12 bit system)
  pageNum=address>>5; // first 7 bits is the page
  offset=address&31; // 5 last bit is offset

  // checkig if the address is on the heap and stack area and if he was allready allocated memory
  if(address >= sumOfprogramMem)
  {
    if(sim_db->page_table[pageNum].isMemAllocated == 0)
    { 
      missCounter++;
      wrongAdressCounter++;
      perror("this memory cell was not allocated..cant read");
      return -1;
    }
  }

 
  // if our address is in the ram
  if(sim_db->page_table[pageNum].valid == 1) 
  {
    hitsCounter++;

    frameNum=sim_db->page_table[pageNum].frame;
    p_char=readCharFromRam(p_char,frameNum,offset);
   
    bitMap[frameNum][1]=0;
     bitMapAgeUpdate();
    return 0; // read sucsses
  } 
 

   // if is in swap area
   else if(sim_db->page_table[pageNum].isInSwap ==1 )
   {
   
        ret= copyPageToRam(sim_db,pageNum,1); // if the function faild
        if( ret == -1)
        {
            perror("problem with copyPageToRam func in vm_load in swap area");
            return -1;
        }
   }        
      

    // if is on disk area
    else
    {     
        ret= copyPageToRam(sim_db,pageNum,0); // if the function faild
        if( ret == -1)
        {
            perror("problem with copyPageToRam func in vm_load in disk area");
            return -1;
        }
    }     

// after we probably copy page to ram, we will read the char from freeFrameNum
     p_char=readCharFromRam(p_char,freeFrameNum,offset);

     freeFrameNum=-1;
     return 0;

}

//=========================================================================================

//=========================================================================================


static void printRam() // print ram and bit map
{
  printf("PRINT RAM\n\n");
  int i;
  int j;
  j=0;
  for (i = 0; i < 512; ++i)
  {
    printf("%c",ramMen[i] );
    j++;
    if (j==32)
    {
      printf("\n");
      j=0;
    }
  }

  printf("\n\n-----PRINT BIT MAP ------ \n\n");
  for(i=0;i<NUM_OF_FRAMES;i++)
  {
    printf("frame : %d :valid: %d  old : %d , page : %d\n",i,bitMap[i][0],bitMap[i][1],bitMap[i][2]);
  }

}


void vm_destructor(sim_database_t *sim_db)
{
  
  free(sim_db->page_table);
  free(sim_db->swapfile_name);
  free(sim_db->executable_name);
  close(sim_db->swapfile_fd);
  close(sim_db->executable_fd);
  
  free(sim_db);
}

static void printPageTable(sim_database_t *sim_db)
{
  int i;
  printf("\n\n----PAGE TABLE DETAILS-----\n"); 
  for (i = 0; i < TOTAL_PAGES; ++i)
  {
    printf("page:%d\t valid:%d\t permmision:%d\t dirty:%d\t isSwap:%d frame:%d \n",i,sim_db->page_table[i].valid,
          sim_db->page_table[i].permission, sim_db->page_table[i].dirty,sim_db->page_table[i].isInSwap, sim_db->page_table[i].frame);
  }
}

//================================================================================
//================================================================================

static char* readCharFromRam( unsigned char *p_char,int frameNum,int offset)
{
     *p_char = ramMen[frameNum * PAGE_SIZE+offset];
    return p_char;
}

//================================================================================
//================================================================================

static int copyPageToRam(sim_database_t* sim_db,int pageNum,int isFromSwap)// add lru
{
    int ret;
    missCounter++;

    //find a new frame
    freeFrameNum=freeFrameInBitMap();


    //if we dont have any free frame, use the lru algorithem
    if(freeFrameNum == -1)
      {
        freeFrameNum=lru(sim_db);

        if(freeFrameNum==-1)
        {
          perror("lru returned -1..problem in lru");
          return -1;
        }
      }


    if (isFromSwap==1)
    {
          //go with pointer (fd) to the specific page
         lseek(sim_db->swapfile_fd,pageNum*PAGE_SIZE,SEEK_SET);

        //read page from swap and save page in ram place
         ret= read(sim_db->swapfile_fd,&ramMen[freeFrameNum*PAGE_SIZE],PAGE_SIZE);
        if( ret == -1)
        {
            perror("problem in read() from swap method");
            return -1;
        }

      sim_db->page_table[pageNum].isInSwap=0;
      
    }


    // copy from disk
    else
    {     
          //go with pointer (fd) to the specific page in disk

         ret=lseek(sim_db->executable_fd,pageNum*PAGE_SIZE,SEEK_SET);
         if(ret==-1)
         {
          perror("problem in lseek from disk method");
            return -1;
         }
     
        //read page from disk and save page in ram
         ret= read(sim_db->executable_fd,&ramMen[freeFrameNum*32],32) ;
         if(ret==-1)
         {
          perror("problem in read() from disk method");
            return -1;
         }
    }

    bitMap[freeFrameNum][0]=1;
    bitMap[freeFrameNum][2]=pageNum;
    bitMapAgeUpdate();
    sim_db->page_table[pageNum].valid=1;
    sim_db->page_table[pageNum].frame=freeFrameNum;

      return 0;  
}

//================================================================================

//================================================================================

static int freeFrameInBitMap()// looking for a free frame 
{
  int i;
  for (i = 0; i < NUM_OF_FRAMES; i++)
  {
    if(bitMap[i][0]==0)
      return i;
  }
  return -1;

}

//================================================================================

//================================================================================


// when we load a new page to the ram we update the bitMap table on frames age
static void bitMapAgeUpdate()
{
  int i;
  for (i = 0; i < NUM_OF_FRAMES; ++i)
    {
      if (bitMap[i][0]==1)
          bitMap[i][1]++;      
    }  
}


//================================================================================

//================================================================================

void vm_print(sim_database_t* sim_db)
{
int i;
int j;
j=0;
  
  printPageTable(sim_db);

  printf("\n-----SWAP FILE NAME IS------ : %s\n",sim_db->swapfile_name);

  printf("-----FILE DESCRIPTOR IS-----: %d \n" ,sim_db->swapfile_fd);

  printf("------EXECUTBLE FILE NAME IS------: %s\n",sim_db->executable_name);

  printRam();
  
  printf("\n\n\n-----STATISTICS------\n");

  printf("HOW MANY TIMES GO TO MEMORY (LOAD & STORE) : %d\n",loadAndStoreCounter );
  printf("NUM OF HITS : %d\n",hitsCounter);
  printf("NUM OF MISSES : %d\n",missCounter);
  printf("UNLIGAL REQUEST : %d\n",wrongAdressCounter);
  printf("NUM OF UNPREMISSIOM : %d\n",wrongPermissionCounter);
}

//================================================================================
//================================================================================

static int lru(sim_database_t* sim_db)
{
  int oldestFrameNum;
  oldestFrameNum;
  int oldestAge =0;
  int i;
  int ret;

//find oldest frame
  for (i = 0; i < NUM_OF_FRAMES; ++i)
  {
    if(bitMap[i][1] > oldestAge)
      {
        oldestAge=bitMap[i][1];
        //oldestAge=bitMap[i][1];
        oldestFrameNum=i;
      }
  }  

  int pageNumToRemove=bitMap[oldestFrameNum][2];// the page number we want to move
 
  if(sim_db->page_table[pageNumToRemove].dirty==1) // if was a change in page copy to swap
  {
     ret=lseek(sim_db->swapfile_fd,pageNumToRemove*PAGE_SIZE,SEEK_SET);
  
    if(ret < 0 )// point to page area in swap dile
    {
        perror("problem in lseek in lru method");
        return -1;
    }

    // write to swap
    ret= write(sim_db->swapfile_fd, &ramMen[oldestFrameNum*PAGE_SIZE] , 32);
    if(ret == -1) 
    {
      perror("problem while write from ram to swap in lru");
      return -1;
    }

    sim_db->page_table[pageNumToRemove].isInSwap=1; // in swap now
    sim_db->page_table[pageNumToRemove].dirty=0;
  }


    sim_db->page_table[pageNumToRemove].valid=0;// not in the ram any more
    sim_db->page_table[pageNumToRemove].frame=-1; // not have a frame
    
    bitMap[oldestFrameNum][0]=0;// free frame
    bitMap[oldestFrameNum][1]=0;//reset age
    bitMap[oldestFrameNum][2]=-1; // no page in frame

    return oldestFrameNum;
}

//=========================================================================
//=========================================================================

int vm_store(sim_database_t* sim_db, unsigned short address, unsigned char value)
{
  loadAndStoreCounter++;

  int pageNum , offset, frameNum ,i,ret;
  // check if our random adress is in range
  if (address < 0 || address > TOTAL_PAGES*PAGE_SIZE)
  {
    missCounter++;
    wrongAdressCounter++;
    perror("address is not in executble range(store)");
    return -1;
  }

  //convert address to binary (12 bit system)
  pageNum=address>>5; // first 7 bits is the page
  offset=address&31; // 5 last bit is offset


  // if page is on the ram
  if(sim_db->page_table[pageNum].valid == 1 )
  {
    hitsCounter++;

    // if page is on the ram and this is read only page
    if(sim_db->page_table[pageNum].permission == 1)
    {
      wrongPermissionCounter++;
      return -1;
    }


    frameNum=sim_db->page_table[pageNum].frame;
    writeCharOnRam(frameNum,offset,sim_db,value,pageNum);
     
    bitMap[frameNum][1]=0;
    bitMapAgeUpdate();
    return 0;
  }


  // if is read only permission and not on the ram -return -1
  if(sim_db->page_table[pageNum].permission == 1)
  {
     missCounter++;
     wrongPermissionCounter++;
     perror("cant write on read only permission\n");
     return -1;
  }

  // checking if on the heap and stack area i was allocated memory
  if(address >= sumOfprogramMem)// if we in heap and stack area
  {
    if(sim_db->page_table[pageNum].isMemAllocated == 0)// if it's a page that not memory allocated
    {
        sim_db->page_table[pageNum].permission=0;
        sim_db->page_table[pageNum].isMemAllocated=1;
        sim_db->page_table[pageNum].isInSwap=0;

        ret=copyPageToRam(sim_db,pageNum,0); // send page to ram
        if(ret == -1)
        {
          perror("problem while copy page to ram in vm_store");
          return -1;
        }

        // set 31 zero and 1 value to the new page
      for(i=0 ; i<PAGE_SIZE;i++)
      {
          if(i != offset)
               writeCharOnRam(freeFrameNum,i,sim_db,'0',pageNum);

          else
               writeCharOnRam(freeFrameNum,i,sim_db,value,pageNum);        
      }
      return 0;  
    }
  }

  

  //if the page in swap area
  if(frameNum=sim_db->page_table[pageNum].isInSwap==1)
  {
    ret=copyPageToRam(sim_db,pageNum,1); // send page to ram
    if(ret == -1)
    {
      perror("problem while copy page to ram in vm_store");
      return -1;
    }
  }
        
  //if the page in disk area
  else
  {
    ret=copyPageToRam(sim_db,pageNum,0); // send page to ram
    if(ret == -1)
    {
      perror("problem while copy page to ram in vm_store");
      return -1;
    }
  }
      
  writeCharOnRam(freeFrameNum,offset,sim_db,value,pageNum);
  freeFrameNum=-1;

  return 0;
}


static void writeCharOnRam(int frameNum,int offset,sim_database_t* sim_db,unsigned char value, int pageNum)
{
  ramMen[frameNum*PAGE_SIZE +offset] = value;
  sim_db->page_table[pageNum].dirty=1;
}

