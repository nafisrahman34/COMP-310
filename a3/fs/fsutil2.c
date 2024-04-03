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
    off_t size;
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

        off_t file_size = file_length(file_s);
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
        // Find the index of the largest file
        int max_index = i;
        for (int j = i + 1; j < file_count; j++) {
            if (files[j].size > files[max_index].size) {
                max_index = j;
            }
        }

        // Swap the current file with the largest file
        FileData temp = files[i];
        files[i] = files[max_index];
        files[max_index] = temp;

        // Create and write the current file
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