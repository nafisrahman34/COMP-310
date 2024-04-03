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

void fragmentation_degree() {
    struct dir *dir = dir_open_root();
    char name[NAME_MAX + 1];
    int fragmentable_files = 0;
    int fragmented_files = 0;

    while (dir_readdir(dir, name) == true) {
        struct file *file_s = filesys_open(name);
        if (file_s == NULL) {
            continue;
        }

        int total_blocks = file_length(file_s) / BLOCK_SECTOR_SIZE;
        if (total_blocks > 1) {
            fragmentable_files++;
            struct inode *inode = file_get_inode(file_s);
            block_sector_t *sectors = get_inode_data_sectors(inode);

            int last_sector = sectors[0];
            for (int j = 1; j < total_blocks; j++) {
              int current_sector = sectors[j];
              if (current_sector - last_sector > 3) {
                  fragmented_files++;
                  break;
              }
              last_sector = current_sector;
            }
            free(sectors); 
        }

        file_close(file_s);
    }

    dir_close(dir);
    printf("Num fragmentable files: %d\n", fragmentable_files);
    if (fragmentable_files == 0) {
        printf("No fragmentable files found.\n");
    } else {
        double fragmentation = (double)fragmented_files / fragmentable_files;
        printf("Num fragmented files: %d\n", fragmented_files);
        printf("Fragmentation pct: %.6f\n", fragmentation);
    }
}

typedef struct {
    char name[NAME_MAX + 1];
    void *data;
    int size;
} FileData;

int defragment() {
    struct dir *dir = dir_open_root();
    char name[NAME_MAX + 1];
    struct file *file_s;
    void *buffer;
    int i;

    // Step 1: Count the number of files
    int file_count = 0;
    while (dir_readdir(dir, name) == true) {
        file_count++;
    }

    // Allocate memory for the files array
    FileData *files = malloc(sizeof(FileData) * file_count);

    // Reset the directory entry
    dir_reopen(dir);

    // Read all files into memory
    file_count = 0;
    while (dir_readdir(dir, name) == true) {
        file_s = filesys_open(name);
        if (file_s == NULL) {
            continue;
        }

        int file_size = file_length(file_s);
        buffer = malloc(file_size);
        if (buffer == NULL) {
            printf("Failed to allocate memory for file: %s\n", name);
            file_close(file_s);
            continue;
        }

        file_read(file_s, buffer, file_size);

        strcpy(files[file_count].name, name);
        files[file_count].data = buffer;
        files[file_count].size = file_size;
        file_count++;

        file_close(file_s);
    }

    // Clear the disk
    dir_reopen(dir);
    while (dir_readdir(dir, name) == true) {
        if (!filesys_remove(name)) {
            printf("Failed to remove file: %s\n", name);
            return -1;
        }
    }

    // Write the files back to the disk
    for (i = 0; i < file_count; i++) {
        if (filesys_create(files[i].name, files[i].size, false)) {
            file_s = filesys_open(files[i].name);
            if (file_s == NULL) {
                printf("Failed to open file: %s\n", files[i].name);
                return -1;
            }
        } else {
            printf("Failed to create file: %s\n", files[i].name);
            return -1;
        }

        file_write(file_s, files[i].data, files[i].size);
        file_close(file_s);
    }

    // Free the memory
    for (i = 0; i < file_count; i++) {
        free(files[i].data);
    }
    free(files);

    dir_close(dir);

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
    struct dir* dir = dir_open_root();
    struct inode_disk* buffer = malloc(BLOCK_SECTOR_SIZE);
    char filename[NAME_MAX+1];
    long size = bitmap_size(free_map);
    for(int i=4; i<size; i++)
    {
      if(bitmap_test(free_map, i) == false){
        buffer_cache_read(i, buffer);
        if(memcmp(buffer, "\0", BLOCK_SECTOR_SIZE) != 0) {
          snprintf(filename, sizeof(filename), "recovered1-%d", i);
          if(dir_add(dir, filename, i, false)){
            bitmap_set(free_map, i, true);
          }
          else{
            printf("dir_add error\n");
            fflush(stdout);
          }
        }
      }
    dir_close(dir);
    free(buffer);
  }
  
  } else if (flag == 2) { // data past end of file.

    // TODO
  }
}