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
  long size = ftell(fp);
  rewind(fp);
  //allocate space for buffer and read contents of file into buffer
  char *buffer = malloc(size + 1);
  memset(buffer, 0, size+1);
  fread(buffer, size, 1, fp);
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
  int size = fsutil_size(fname);
  char* buffer = malloc((size+1)*sizeof(char));
  memset(buffer, 0, size+1);
  struct file *file_s = get_file_by_fname(fname);
  if (file_s == NULL) {
    file_s = filesys_open(fname);
    if (file_s == NULL) {
      return -1;
    }
    add_to_file_table(file_s, fname);
  }
  offset_t cur_offset = file_tell(file_s);
  file_seek(file_s, 0);
  fsutil_read(fname, buffer, size);
  file_seek(file_s, cur_offset);
  FILE* file = fopen(fname, "w");
  if(file == NULL){
    return 11;
  }
  fputs(buffer, file);
  fclose(file);
  free(buffer);
  return 0;
}

void find_file(char *pattern) {
  // TODO
  return;
}

void fragmentation_degree() {
  // TODO
}

int defragment() {
  // TODO
  return 0;
}

void recover(int flag) {
  if (flag == 0) { // recover deleted inodes

    // TODO
  } else if (flag == 1) { // recover all non-empty sectors

    // TODO
  } else if (flag == 2) { // data past end of file.

    // TODO
  }
}