#include "fsutil2.h"
#include "bitmap.h"
#include "cache.h"
#include "debug.h"
#include "directory.h"
#include "file.h"
#include "filesys.h"
#include "free-map.h"
#include "fsutil.h"
#include "inode.h"
#include "off_t.h"
#include "filesys.h"
#include "partition.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

int copy_in(char *fname) {
  FILE *fp = fopen(fname, "r");
  if(fp == NULL) return -1;
  if(fseek(fp, 0, SEEK_END)<0){
    fclose(fp);
    return -1;
  }
  //obtain size of file on real hard drive
  long size = ftell(fp)+1;
  rewind(fp);
  //allocate space for buffer and read contents of file into buffer
  char *buffer = malloc(size);
  memset(buffer, 0, size);
  fread(buffer, 1, size, fp);
  fclose(fp);
  //check if space is available on shell disk for the file and create the file
  long freespace = fsutil_freespace() * BLOCK_SECTOR_SIZE;
  int status = size>freespace ? fsutil_create(fname, freespace) : fsutil_create(fname, size);
  //raise FILE_CREATION_ERROR if file not successfully created
  if(status == 0) return 9;
  int bytes_written = fsutil_write(fname, buffer, size);
  free(buffer);
  //raise FILE_WRITE_ERROR if writing to file is unsuccessful otherwise if bytes written to shell hard drive is smaller than file size, print a message
  if (bytes_written == -1) {
    return 11;
  } else if (bytes_written != size) {
    printf("Warning: could only write %d out of %ld bytes (reached end of file)\n", bytes_written, size);
  }
  return 0;
}


int copy_out(char *fname) {
  //find the size of the file and allocate space for it in the buffer
  int size = fsutil_size(fname);
  char* buffer = malloc((size+1)*sizeof(char));
  memset(buffer, 0, size+1);
  //open file
  struct file *file_s = get_file_by_fname(fname);
  if (file_s == NULL) {
    file_s = filesys_open(fname);
    if (file_s == NULL) {
      return -1;
    }
    add_to_file_table(file_s, fname);
  }
  //store the curr offset of the file and reset offset to 0 to read from the beginning
  offset_t cur_offset = file_tell(file_s);
  file_seek(file_s, 0);
  //copy the contents of the file into the buffer
  fsutil_read(fname, buffer, size);
  //restore file offset to what it was initially
  file_seek(file_s, cur_offset);
  //create the file on the real hard drive and populate it with contents of buffer
  FILE* file = fopen(fname, "w");
  if(file == NULL){
    //raises FILE_WRITE_ERROR if file is not created or opened
    return 11;
  }
  fputs(buffer, file);
  //free the memory allocated for the buffer and close the file
  fclose(file);
  free(buffer);
  return 0;
}


void find_file(char *pattern) {
  //open root directory and create a variable to store file names
  struct dir *dir = dir_open_root();
  char name[NAME_MAX + 1];
  while (dir_readdir(dir, name)==true)
  {
    //open curr file in the directory
    struct file *file_s = filesys_open(name);
    add_to_file_table(file_s, name);
    //obtain size of file and allocate space for it in the buffer
    long size = fsutil_size(name);
    char *buffer = malloc(size+1);
    //store the current offset of the file and reset offset to 0 to read from the beginning
    offset_t cur_offset = file_tell(file_s);
    file_seek(file_s, 0);
    //read contents of file and store in the buffer 
    file_read(file_s, buffer, size);
    //restore file pointer offset
    file_seek(file_s, cur_offset);
    buffer[size] = '\0';
    //check if the pattern exists within the content of the file
    if(strstr(buffer, pattern) != NULL){
      printf("%s\n", name);
    }
    //free the buffer, close the file and close the directory
    free(buffer);
    remove_from_file_table(name);
  }
  dir_close(dir);
}


//function to determine the fragmentation degree
void fragmentation_degree() {
    struct dir *rootDir = dir_open_root();
    char fileName[NAME_MAX + 1];
    //variables to keep track of files that can be fragmented and that have been fragmented
    int countFragmentable = 0; 
    int countFragmented = 0;   

    //get all the files in root
    do {
        //use given function to open file
        struct file *currentFile = filesys_open(fileName);
        if (currentFile == NULL) {
            continue;
        }

        //get total number of blocks using given function file_length
        int totalBlocks = file_length(currentFile) / BLOCK_SECTOR_SIZE;
        //when file has at least 1 block we can fragment
        if (totalBlocks > 1) {
            //increment count since fragmentable
            countFragmentable++;
            //get inode and sector
            struct inode *fileInode = file_get_inode(currentFile);
            block_sector_t *sectors = get_inode_data_sectors(fileInode);
            //verify if file is fragmented
            int previousSector = sectors[0];
            for (int i = 1; i < totalBlocks; i++) {
                int currentSector = sectors[i];
                //bigger difference than 3 means file is fragmented as seen in instructions
                if (currentSector - previousSector > 3) {
                    countFragmented++;
                    break;
                }
                previousSector = currentSector;
            }
            free(sectors); 
        }

        file_close(currentFile);
    } while (dir_readdir(rootDir, fileName));

    dir_close(rootDir);

    //print all statements necessary
    printf("Num fragmentable files: %d\n", countFragmentable);
    if (countFragmentable == 0) {
        printf("No fragmentable files were found.\n");
    } else {
        //percentage of fragmented files 
        double fragmentationPct = (double)countFragmented / countFragmentable;
        printf("Num fragmented files: %d\n", countFragmented);
        printf("Fragmentation pct: %.6f\n", fragmentationPct);
    }
}


// Define a structure to hold file data
typedef struct {
    char name[NAME_MAX + 1];  // Name of the file
    char *data;               // Pointer to the file data
    size_t size;               // Size of the file
} FileData;

// Function to defragment the file system
int defragment() {
  // Open the root directory
  struct dir *dir = dir_open_root();
  char name[NAME_MAX + 1];
  struct file *file_s;
  char *buffer;
  int i;

  // Count the number of files in the root directory
  int file_count = 0;
  while (dir_readdir(dir, name) == true) {
    file_count++;
  }

  // Allocate memory to hold data for all files
  FileData *files = malloc(sizeof(FileData) * file_count);

  // Reset the directory entry to start reading from the beginning
  dir_reopen(dir);

  // Step 1: Read all files into memory
  file_count = 0;
  while (dir_readdir(dir, name) == true) {

    struct inode *inode = NULL;
    if (!dir_lookup(dir, name, &inode))
    {
      continue; // Skip if the file is not found
    }
    if (inode_is_directory(inode))
    {
      inode_close(inode);
      continue; // Skip directories
    }



    file_s = filesys_open(name);
    if (file_s == NULL) {
      return -1; //FILE_DOES_NOT_EXIST
    }

    off_t file_size = file_length(file_s);
    buffer = malloc(file_size);
    if (buffer == NULL) {
      printf("Failed to allocate memory for file: %s\n", name);
      file_close(file_s);
      return 1; //NO_MEM_SPACE
    }

    //store the current offset of the file and reset offset to 0 to read from the beginning
    offset_t cur_offset = file_tell(file_s);
    file_seek(file_s, 0);
    //read contents of file and store in the buffer 
    file_read(file_s, buffer, file_size);
    //restore file pointer offset
    file_seek(file_s, cur_offset);

    strcpy(files[file_count].name, name);
    files[file_count].data = buffer;
    files[file_count].size = file_size;
    file_count++;
    file_close(file_s);
    // Step 2: Remove each file after adding to buffer
    fsutil_rm(name);
  }
  dir_close(dir);
  // Step 3: Create all files again and write the data from the buffer to each
  for (i = 0; i < file_count; i++) {
    if (fsutil_create(files[i].name, files[i].size)) {
      file_s = filesys_open(files[i].name);
      if (file_s == NULL) {
        printf("Failed to open file: %s\n", files[i].name);
        return -1; //FILE_DOES_NOT_EXIST
      }
      else {
        if (fsutil_write(files[i].name, files[i].data, files[i].size) != -1) {
          printf("File restored successfully: %s\n", files[i].name);
        }
        else {
          printf("Failed to write to file: %s.\n", files[i].name);
        }
      }
      file_close(file_s);
    }
    else {
      printf("File creation failed: %s\n", files[i].name);
      return 9; //FILE_CREATION_ERROR
    }
    // Free the memory used to hold the file data
    free(files[i].data);
  }
  //free memory allocated for FileData array
  free(files);
  // Close the directory
  dir_close(dir);
  return 0;
}


void recover(int flag) {
  if (flag == 0) { // recover deleted inodes
    struct dir* dir = dir_open_root();
    //allocate space for the inode_disk to be recovered and the filename string
    struct inode_disk* buffer = malloc(BLOCK_SECTOR_SIZE); 
    char filename[NAME_MAX+1];
    long size = bitmap_size(free_map);
    for(int i=0; i<size; i++)
    {
      //iterate through every sector and look for a free sector
      if(bitmap_test(free_map, i) == false){
        //if sector is free, read its contents into our inode_disk buffer
        buffer_cache_read(i, buffer);
        //check if sector being read contains an inode using INODE_MAGIC
        if(buffer->magic==INODE_MAGIC) {
          snprintf(filename, sizeof(filename), "recovered0-%d", i);
          //add to directory with new filename in the specified format
          if(dir_add(dir, filename, i, false)){
            //modify bitmap to show current sector is no longer free
            bitmap_set(free_map, i, true);
          }
          else{
            printf("dir_add error\n");
            fflush(stdout);
          }
        }
      }
    }
    dir_close(dir);
    free(buffer);

  } else if (flag == 1) { // recover all non-empty sectors
    struct inode_disk* buffer = malloc(BLOCK_SECTOR_SIZE);
    void* empty_buffer = calloc(1, BLOCK_SECTOR_SIZE);  // create a block of null characters
    char filename[NAME_MAX+1];
    long size = bitmap_size(free_map);
    for(int i=4; i<size; i++)
    {
      buffer_cache_read(i, buffer);
      if(memcmp(buffer, empty_buffer, BLOCK_SECTOR_SIZE) != 0) {  // compare with the block of null characters
        snprintf(filename, sizeof(filename), "recovered1-%d.txt", i);
        FILE *fp = fopen(filename, "w");
        if (fp != NULL) {
          // Determine the size of the data in the buffer
          size_t data_size = BLOCK_SECTOR_SIZE;
          for (size_t i = 0; i < BLOCK_SECTOR_SIZE; i++) {
            if (((char*)buffer)[i] == '\0') {
              data_size = i;
              break;
            }
          }
          fwrite(buffer, 1, data_size, fp);
          fclose(fp);
        } else {
          printf("Failed to open file: %s\n", filename);
          fflush(stdout);
        }
      }
    }
    free(buffer);
    free(empty_buffer); 
  
  } else if (flag == 2) { // data past end of file.
    struct dir* dir = dir_open_root();
    char filename[NAME_MAX+1];
    struct file *file_s;
    struct inode_disk* buffer = malloc(BLOCK_SECTOR_SIZE);
    void* empty_buffer = calloc(1, BLOCK_SECTOR_SIZE);  // create a block of null characters

    while (dir_readdir(dir, filename) == true) {
        file_s = filesys_open(filename);
        if (file_s == NULL) {
            continue;
        }

        off_t file_size = file_length(file_s);
        int block_count = DIV_ROUND_UP(file_size, BLOCK_SECTOR_SIZE);
        block_sector_t *sectors = get_inode_data_sectors(file_s->inode);
        for (int i = 0; i < block_count; i++) {
            buffer_cache_read(sectors[i], buffer);
            int start_index = (i == block_count - 1) ? file_size % BLOCK_SECTOR_SIZE : BLOCK_SECTOR_SIZE;
            int end_index = start_index;
            for (int j = start_index; j < BLOCK_SECTOR_SIZE; j++) {
                if (((char*)buffer)[j] != '\0') {
                    start_index = j;
                    end_index = j + 1;
                    break;
                }
            }
            for (int j = start_index + 1; j < BLOCK_SECTOR_SIZE; j++) {
                if (((char*)buffer)[j] != '\0') {
                    end_index = j + 1;
                }
            }
            if (start_index != end_index) {
                char recovered_filename[NAME_MAX+1];
                snprintf(recovered_filename, sizeof(recovered_filename), "recovered2-%s.txt", filename);
                FILE *fp = fopen(recovered_filename, "w");
                if (fp != NULL) {
                    fwrite((char*)buffer + start_index, 1, end_index - start_index, fp);
                    fclose(fp);
                } else {
                    printf("Failed to open file: %s\n", recovered_filename);
                    fflush(stdout);
                }
            }
        }

        free(sectors);
        file_close(file_s);
    }

    dir_close(dir);
    free(buffer);
    free(empty_buffer);
}
}