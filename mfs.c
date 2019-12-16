/*
   Nihar Gupte 1001556441
*/

// The MIT License (MIT)
//
// Copyright (c) 2016, 2017 Trevor Bakker
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE

#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <time.h>

#define WHITESPACE " \t\n" /*We want to split our command line up into tokens \
                             so we need to define what delimits our tokens.   \
                             In this case  white space                        \
                             will separate the tokens on our command line*/

#define MAX_COMMAND_SIZE 255 // The maximum command-line size
#define MAX_NUM_ARGUMENTS 5  // Mav shell only supports five arguments

#define BLOCK_SIZE 8192   //as per requirements
#define NUM_BLOCKS 4226   //as per requirements
#define NUM_FILES 128     //as per requirements
#define FILE_NAME_SIZE 32 //as per requirements

FILE *fd;            //file pointer for disk image
int disk_status = 0; //a boolean variable to make prevent improper opening/closing of files
time_t now[128]; //stores date and time each file was added to the file system

uint8_t blocks[NUM_BLOCKS][BLOCK_SIZE]; //basic datastructure to hold info abt files

struct Directory_Entry
{
  int8_t valid; //boolean type value to check if file is occupied
  char name[255]; //stores file name
  //uint32_t  inode;
};

struct Inode
{
  bool attrib_h; //hidden attribute
  bool attrib_r; //read only attribute
  uint32_t size; //stores size of the file  
  uint32_t blocks[1250]; // since 10,240,000/8192 = 1250 as per requirements
};

struct Directory_Entry *dir; //main directory structure for files
uint8_t *freeBlockList;  //keeps a list of all free blocks
uint8_t *freeInodeList;  //keeps a list of all free inodes
struct Inode *inodeList; //keeps a list of all inodes

//this function initializes the valid bit, name, and inode free bit for 128 files
void initializeDirectory()
{
  int i;
  for (i = 0; i < 128; i++)
  {
    dir[i].valid = 0;
    memset(dir[i].name, 0, 255);
    //dir[i].inode=-1;
  }
}

//this function initializes the blocks in memoery to be free
void initializeBlockList()
{
  int i;
  for (i = 0; i < 4226; i++)
  {
    freeBlockList[i] = 0;
  }
}

//this function initializes the inodes in memory to be free
void initializeInodeList()
{
  int i;
  for (i = 0; i < 128; i++)
  {
    freeInodeList[i] = 0;
  }
}

//this function initializes values of inodes like attribute and size and initializes unused blocks to -1
void initializeInodes()
{
  int i;
  for (i = 0; i < 128; i++)
  {
    int j;
    //default allocations
    inodeList[i].attrib_h = false;
    inodeList[i].attrib_r = false;
    inodeList[i].size = 0;
    for (j = 0; j < 1250; j++)
    {
      inodeList[i].blocks[j] = -1; //setting blocks to -1 signifying unused
    }
  }
}

//function to return the index of next free inode from the freeInodeList
int findFreeInode()
{
  int ret = -1;
  int i;

  for (i = 0; i < 128; i++)
  {
    if (freeInodeList[i] == 0)
    {
      ret = i;
      freeInodeList[i] = 1;
      break;
    }
  }
  return ret;
}

//function to return the index of the next free block from the freeBlockList
int findFreeBlock()
{
  int ret = -1;
  int i;

  //start at 132 as first 131 blocks are reserved
  for (i = 132; i < NUM_BLOCKS; i++)
  {
    if (freeBlockList[i] == 0)
    {
      ret = i;
      freeBlockList[i] = 1;
      break;
    }
  }
  return ret;
}

//function to return the next free (i.e valid) directory from the directories
int findFreeDirectory()
{
  int ret = -1;
  int i;

  for (i = 0; i < 128; i++)
  {
    if (dir[i].valid == 0)
    {
      ret = i;
      dir[i].valid = 1;
      break;
    }
  }
  return ret;
}

//function which takes filename as in input parameters and creates a disk file system with that filename
void createfs(char *filename)
{
  memset(blocks, 0, NUM_BLOCKS * BLOCK_SIZE);
  fd = fopen(filename, "w");
  fwrite(blocks, BLOCK_SIZE, NUM_BLOCKS, fd); //write the contents of blocks to disk image after opening
  fclose(fd);
}

//function which opens the specified file name and starts reading from it
void open(char *filename)
{
  fd = fopen(filename, "r");
  if (fd == NULL)
  {
    printf("open: File does not exist.\n");
    disk_status = 0;
    return;
  }
  fread(blocks, BLOCK_SIZE, NUM_BLOCKS, fd); //start reading contents of blocks
  fclose(fd);
  fd = fopen(filename, "w");
}

//function to close the specified file name after writing to main memory
void close()
{
  fwrite(blocks, BLOCK_SIZE, NUM_BLOCKS, fd); //write the changes to blocks array and close it
  fclose(fd);
}

//function to return the available space in disk in bytes
int dfcmd()
{
  int result = 0, i;
  for (i = 132; i < NUM_BLOCKS; i++) //only these blocks are used for actual storage
  {
    //printf("%d,",freeBlockList[i]);
    if (freeBlockList[i] == 0) //if block is free
    {
      result += BLOCK_SIZE;
    }
  }
  return result;
}

//this function gets the name of the file, and puts that file into our custom file system
void putcmd(char *filen)
{
  int status; // Hold the status of all return values.
  struct stat buf;
  char *filename = filen;
  status = stat(filename, &buf);
  if (status != -1)
  {
    FILE *file = fopen(filename, "r");
    int copy_size = buf.st_size; //size of the file to be copied to disk

    if (copy_size > dfcmd()) //checking if enough space exists
    {
      printf("put error: Not Enough Space.\n");
      fclose(file);
      return;
    }
    //printf("copy size = %d\n",copy_size);
    int offset = 0;
    int dir_num = findFreeDirectory(); //getting the next free directory
    strcpy(dir[dir_num].name, filename);
    inodeList[dir_num].size = copy_size;
    int block_num = 0;

    time(&now[dir_num]); //getting the time the directory is put into the file system
    //printf("%s\n",ctime(&now));
    //strcpy(inodeList[dir_num].datetime,ctime(&now));

    //from block_copy_example.c on github
    while (copy_size > 0)
    {
      int next_block = findFreeBlock();
      if (next_block == -1)
      {
        printf("put error: Not Enough Space.\n");
        return;
      }

      fseek(file, offset, SEEK_SET);
      int bytes = fread(blocks[next_block], BLOCK_SIZE, 1, file);
      if (bytes == 0 && !feof(file))
      {
        printf("An error occured reading from the input file.\n");
        return;
      }

      clearerr(file);
      copy_size -= BLOCK_SIZE;
      offset += BLOCK_SIZE;
      inodeList[dir_num].blocks[block_num] = next_block;
      block_num++;
    }

    fclose(file);
  }
  else
  {
    printf("put error: File not found\n");
  }
}

//this function gets the filename and an optional new filename, and retrieves the file specified from the file system and puts it into cwd
void getcmd(char *filename, char *newfilename)
{
  int i, j, index;
  int flag = 0;
  FILE *temp;
  for (i = 0; i < 128; i++)
  {
    if (dir[i].valid == 1 && strcmp(dir[i].name, filename) == 0) //if we get the required file
    {
      flag = 1;
      if (newfilename == NULL) //filename same
      {
        temp = fopen(filename, "w");
      }
      else
      {
        temp = fopen(newfilename, "w"); //file rename
      }
      for (j = 0; j < 1250; j++)
      {
        index = inodeList[i].blocks[j];
        if (index == -1) //checking if the file ends
        {
          break;
        }
        fwrite(blocks[index], BLOCK_SIZE, 1, temp); //otherwise keep on writing to our blocks structure
      }
    }
  }
  if (flag == 0)
  {
    printf("get error: file not found.\n");
  }
  else
  {
    fclose(temp);
  }
}

//this function gets the filename and deletes the file from our custom filesystem
void delete (char *filename)
{
  int i, j, flag = 0;
  for (i = 0; i < 128; i++)
  {
    if (dir[i].valid == 1 && strcmp(dir[i].name, filename) == 0) //if the file matchese
    {
      flag = 1;
      if (inodeList[i].attrib_r == false) //checking if its read only (int value 2)
      {
        dir[i].valid = 0; //reseting the dir value
        freeInodeList[i] = 0; //reseting the inode value
        for (j = 0; j < 1250; j++)
        {
          freeBlockList[inodeList[i].blocks[j]] = 0; //reseting individual blocks
        }
      }
      else
      {
        printf("del error: That file is marked read-only.\n");
      }
    }
  }
  if (flag == 0)
  {
    printf("del error: File not found.\n");
  }
}

//this function accepts a parameter (like -h) and lists all the files currently in our filesytem
void list(char *parameter)
{
  int i, flag = 0;
      for (i = 0; i < 128; i++)
      {
        if (dir[i].valid == 1)
        {
          flag = 1;

          char *t = ctime(&now[i]);
          if (t[strlen(t) - 1] == '\n')
          {
            t[strlen(t) - 1] = '\0';
          }
          

          if (parameter != NULL && strcmp(parameter, "-h") == 0)
          {
            printf("%d %s %s\n", inodeList[i].size, t, dir[i].name);
          }
          else if (inodeList[i].attrib_h == false)
          {
            printf("%d %s %s\n", inodeList[i].size, t, dir[i].name);
          }
        }
      }
      if (flag == 0)
      {
        printf("list: No files found.\n");
      }
}

//driver function
int main()
{
  //making necessary initializations for the list
  dir = (struct Directory_Entry *)&blocks[0][0];
  freeInodeList = (uint8_t *)&blocks[5][0];
  freeBlockList = (uint8_t *)&blocks[6][0];

  // inodes run from block 3-131 so add 3 to the index
  inodeList = (struct Inode *)&blocks[7][0];

  /*initializeDirectory();
  initializeInodeList();
  initializeBlockList();
  initializeInodes();*/


  char *cmd_str = (char *)malloc(MAX_COMMAND_SIZE);

  while (1)
  {
    // Print out the mfs prompt
    printf("mfs> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while (!fgets(cmd_str, MAX_COMMAND_SIZE, stdin))
      ;

    if (strlen(cmd_str) == 1) //if the user enters nothing, the shell shall skip the current iteration and move to next.
    {
      continue;
    }

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    int token_count = 0;

    // Pointer to point to the token
    // parsed by strsep
    char *arg_ptr;

    char *working_str = strdup(cmd_str);

    // we are going to move the working_str pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *working_root = working_str;

    // Tokenize the input stringswith whitespace used as the delimiter
    while (((arg_ptr = strsep(&working_str, WHITESPACE)) != NULL) &&
           (token_count < MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup(arg_ptr, MAX_COMMAND_SIZE);
      if (strlen(token[token_count]) == 0)
      {
        token[token_count] = NULL;
      }
      token_count++;
    }

    // Now print the tokenized input as a debug check
    // \TODO Remove this code and replace with your shell functionality

    /* int token_index  = 0;
    for( token_index = 0; token_index < token_count; token_index ++ ) 
    {
      printf("token[%d] = %s\n", token_index, token[token_index] );  
    }
    */
    if (strcmp(token[0], "exit") == 0 || strcmp(token[0], "quit") == 0) //exit condition
    {
      exit(0);
    }
    else if (strcmp(token[0], "put") == 0) 
    {
      if (strlen(token[1]) <= 32) //checking file name size as per requirements
      {
        putcmd(token[1]);
      }
      else
      {
        printf("put error: Filename too long.\n");
      }
    }
    else if (strcmp(token[0], "get") == 0)
    {
      if (token[1] == NULL)
      {
        printf("get error: file name not specified.\n");
      }
      else
      {
        getcmd(token[1], token[2]);
      }     
    }

    else if (strcmp(token[0], "del") == 0)
    {
      if (token[1] == NULL)
      {
        printf("del error: file name not specified.\n");
      }
      else
      {
        delete (token[1]);
      }     
    }

    else if (strcmp(token[0], "list") == 0) 
    {
      list(token[1]);
    }

    else if (strcmp(token[0], "df") == 0)
    {
      printf("%d bytes free.\n", dfcmd());
    }

    else if (strcmp(token[0], "open") == 0)
    {
      if (token[1] == NULL) //i.e no file name specified
      {
        printf("open: File not found\n");
      }
      else
      {
        if (disk_status == 1) //i.e if another file is already open
        {
          printf("open: Another file already open. Close another file first.\n");
        }
        else
        {
          disk_status = 1;
          open(token[1]);
        }
      }
    }
    else if (strcmp(token[0], "close") == 0)
    {
      if (disk_status == 0) //i.e if no file is currently open
      {
        printf("close: no file system currently open that can be closed.\n");
      }
      else
      {
        close();
        disk_status = 0;
      }
    }
    else if (strcmp(token[0], "createfs") == 0)
    {

      if (token[1] == NULL) //no file name specified
      {
        printf("createfs: File not found\n");
      }
      else
      {
        createfs(token[1]);
      }
    }
    else if (strcmp(token[0], "attrib") == 0)
    {
      int i, flag = 0;
      for (i = 0; i < 128; i++)
      {
        if (dir[i].valid == 1 && strcmp(dir[i].name, token[2]) == 0) //if file currently exists in file system
        {
          flag = 1;
          if (strcmp(token[1], "-h") == 0) //unhide
          {
            inodeList[i].attrib_h = false;
          }
          else if (strcmp(token[1], "+h") == 0) //hide
          {
            inodeList[i].attrib_h = true;
          }
          else if (strcmp(token[1], "-r") == 0) //un readonly
          {
            inodeList[i].attrib_r = false;
          }
          else if (strcmp(token[1], "+r") == 0) //read only
          {
            inodeList[i].attrib_r = true;
          }
        }
      }
      if (flag == 0)
      {
        printf("attrib: File not found.\n");
      }
    }
    else
    {
      printf("%s: command does not exist.\n", token[0]);
    }

    free(working_root);
  }
  return 0;
}
