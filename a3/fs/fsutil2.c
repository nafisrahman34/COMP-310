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
    char fileName[NAME_MAX + 1];  // Name of the file
    void *fileData;               // Pointer to the file data
    off_t fileSize;               // Size of the file
} FileData;

// Function to defragment the file system
int defragment() {
    // Open the root directory
    struct dir *rootDir = dir_open_root();
    FileData *fileArray = NULL;
    size_t fileCount = 0;
    char name[NAME_MAX + 1];

    // Read all files into memory
    while (dir_readdir(rootDir, name)) {
        struct inode *fileInode = NULL;
        if (dir_lookup(rootDir, name, &fileInode)) {
            if (!inode_is_directory(fileInode)) {
                printf("Currently backing up given file %s\n", name);
                off_t fileSize = inode_length(fileInode);
                char *buffer = malloc(fileSize);
                if (buffer) {
                    inode_read_at(fileInode, buffer, fileSize, 0);
                    fileArray = realloc(fileArray, sizeof(FileData) * (fileCount + 1));
                    fileArray[fileCount].fileData = buffer;
                    fileArray[fileCount].fileSize = fileSize;
                    strncpy(fileArray[fileCount].fileName, name, NAME_MAX);
                    fileCount++;

                    printf("Currently removing given file: %s\n", name);
                    fsutil_rm(name);
                } else {
                    printf("Cannot allocate enough memomry for backup: %s\n", name);
                }
                inode_close(fileInode);
            } else {
                inode_close(fileInode);
            }
        }
    }

    dir_close(rootDir);

    for (size_t i = 0; i < fileCount; i++) {
        fsutil_rm(fileArray[i].fileName);
    }

    for (size_t i = 0; i < fileCount; i++) {
        printf("Restoring file: %s\n", fileArray[i].fileName);
        if (fsutil_create(fileArray[i].fileName, fileArray[i].fileSize)) {
            if (fsutil_write(fileArray[i].fileName, fileArray[i].fileData, fileArray[i].fileSize) == -1) {
                printf("Failed to write data %s.\n", fileArray[i].fileName);
            } else {
                printf("File restoration successful for file %s .\n", fileArray[i].fileName);
            }
        } else {
            printf("Failed file creation %s.\n", fileArray[i].fileName);
        }

        free(fileArray[i].fileData);
    }

    free(fileArray); 

    return 0;
}


void recover(int flag) {
  if (flag == 0) { // recover deleted inodes
    struct dir* dir = dir_open_root();
    struct inode_disk* buffer = malloc(BLOCK_SECTOR_SIZE);
    char filename[NAME_MAX+1];
    long size = bitmap_size(free_map);
    for(int i=0; i<size; i++)
    {
      if(bitmap_test(free_map, i) == false){
        buffer_cache_read(i, buffer);
        if(buffer->magic==INODE_MAGIC) {
          snprintf(filename, sizeof(filename), "recovered0-%d", i);
          if(dir_add(dir, filename, i, false)){
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