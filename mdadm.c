/* Author: Urmil Mishra 
   Date:
    */
    
    
    
/***
 *      ______ .___  ___. .______     _______.  ______              ____    __   __  
 *     /      ||   \/   | |   _  \   /       | /      |            |___ \  /_ | /_ | 
 *    |  ,----'|  \  /  | |  |_)  | |   (----`|  ,----'              __) |  | |  | | 
 *    |  |     |  |\/|  | |   ___/   \   \    |  |                  |__ <   | |  | | 
 *    |  `----.|  |  |  | |  |   .----)   |   |  `----.             ___) |  | |  | | 
 *     \______||__|  |__| | _|   |_______/     \______|            |____/   |_|  |_| 
 *                                                                                   
 */


#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "cache.h"
#include "mdadm.h"
#include "util.h"
#include "jbod.h"
#include "net.h"
 
  //mount implementation
  int mdadm_mount(void) {
   int returnVal = jbod_client_operation(JBOD_MOUNT, NULL);
   if (returnVal == 0){
     return 1;
   }
   else return -1;
 }
 
 //unmount implementation
 int mdadm_unmount(void) {
   int returnVal = jbod_client_operation(JBOD_UNMOUNT, NULL);
   if (returnVal == 0){
     return 1;
   }
   else return -1;
 }
 
 int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
 //finding which disk to read from
 int disk = addr/65536;
 int returnVal;
 if (disk == 0){
   returnVal = jbod_client_operation((1<<24) | JBOD_SEEK_TO_DISK, NULL);
 }
 else{
   int op = disk << 20;
   returnVal = jbod_client_operation(op | JBOD_SEEK_TO_DISK, NULL);
 }
 
 //catch an error connecting to the disk (if initiated before mouting)
 if (returnVal == -1){
   return -1;
 }
 
 //Check base invalid cases
 if ((addr + len) > 1048576){
   return -1;
 }
 if (len > 1028){
   return -1;
 }
 if (len != 0 && buf == NULL){
   return -1;
 }
 if (len == 0 && buf == NULL){
   return len;
 }
 
 //find block number and which byte to read from the block
 int block = (addr%65536)/256;
 int blockpos = (addr%65536)%256;
 
 //declare temporary buffer and set block to read from
 uint8_t tempbuf[256];
 jbod_client_operation((block << 24) | JBOD_SEEK_TO_BLOCK, NULL);
 
 //if multiple blocks are needed to be read
 if ((blockpos + len) > 255){
   int remaininglen = len - (256-blockpos);
   int currentspot = 256 - blockpos;
   int retval = cache_lookup(disk, block, tempbuf);
   if (retval == -1){
     jbod_client_operation(JBOD_READ_BLOCK, tempbuf);
     cache_insert(disk, block, tempbuf);
     block++;
   }
   else{
    block++;
    if (block <= 255){
      jbod_client_operation((block << 24) | JBOD_SEEK_TO_BLOCK, NULL);
    }
   }
   memcpy(buf,&tempbuf[blockpos],(256-blockpos));

   //check if you reached the end of the disk
   if(block > 255){
     jbod_client_operation((disk + 1)<< 20 | JBOD_SEEK_TO_DISK, NULL);
     jbod_client_operation(JBOD_SEEK_TO_BLOCK, NULL);
     disk += 1;
     block = 0;
   }
 
   //check if more than two blocks need to be read (implements a while loop to read until one block is left to read)
   if (remaininglen > 256){
     while (remaininglen > 256){
       retval = cache_lookup(disk, block, tempbuf);
       if (retval == -1){
         jbod_client_operation(JBOD_READ_BLOCK, tempbuf);
         cache_insert(disk, block, tempbuf);
         block++;
       }
       else{
        block++;
        if (block <= 255){
          jbod_client_operation((block << 24) | JBOD_SEEK_TO_BLOCK, NULL);
        }
       }
       memcpy(buf + currentspot, tempbuf, 256);
 
       //checks if it reaches the end of the disk after every block iteration
       if(block > 255){
         jbod_client_operation((disk + 1)<< 20 | JBOD_SEEK_TO_DISK, NULL);
         jbod_client_operation(JBOD_SEEK_TO_BLOCK, NULL);
         disk += 1;
         block = 0;
       }
       currentspot += 256;
       remaininglen -= 256;
     }
   }
 
   //final block reading after loop terminates to read the remaining length
   retval = cache_lookup(disk, block, tempbuf);
   if (retval == -1){
     jbod_client_operation(JBOD_READ_BLOCK, tempbuf);
     cache_insert(disk, block, tempbuf);
     block++;
   }
   else{
    block++;
    if (block <= 255){
      jbod_client_operation((block << 24) | JBOD_SEEK_TO_BLOCK, NULL);
    }
   }
   memcpy(buf+currentspot, tempbuf, remaininglen);
 }
 
 //simple case of just reading one block
 else{
   int retval = cache_lookup(disk, block, tempbuf);
   if (retval == -1){
     jbod_client_operation(JBOD_READ_BLOCK, tempbuf);
     cache_insert(disk, block, tempbuf);
     block++;
   }
   else{
    block++;
    if (block <= 255){
      jbod_client_operation((block << 24) | JBOD_SEEK_TO_BLOCK, NULL);
    }
   }
   memcpy(buf, tempbuf, len);
 } 
 return len;
 }
 
 int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
  int disk = addr/65536;
  int returnVal;
  if (disk == 0){
    returnVal = jbod_client_operation((1<<24) | JBOD_SEEK_TO_DISK, NULL);
  }
  else{
    int op = disk << 20;
    returnVal = jbod_client_operation(op | JBOD_SEEK_TO_DISK, NULL);
  }
 
  //catch an error connecting to the disk (if initiated before mouting)
  if (returnVal == -1){
    return -1;
  }
 
  //Check base invalid cases
  if ((addr + len) > 1048576){
    return -1;
  }
  if (len > 1028){
    return -1;
  }
  if (len != 0 && buf == NULL){
    return -1;
  }
  if (len == 0 && buf == NULL){
    return len;
  }
 
  //find block number and which byte to write/read from the block
  int block = (addr%65536)/256;
  int blockpos = (addr%65536)%256;
  
  //declare temporary buffer and set block to read from
  uint8_t tempbuf[256];
  jbod_client_operation((block << 24) | JBOD_SEEK_TO_BLOCK, NULL);
  
  //read block into tempbuf
  int retval = cache_lookup(disk, block, tempbuf);
   if (retval == -1){
     jbod_client_operation(JBOD_READ_BLOCK, tempbuf);
     cache_insert(disk, block, tempbuf);
   }

  //retrack back to original block/disk position to write in the same position
  if(block == 255){
    if(disk == 0){
      jbod_client_operation((1<<24)|JBOD_SEEK_TO_DISK,NULL);
    }
    else{
      jbod_client_operation((disk<<20)|JBOD_SEEK_TO_DISK,NULL);
    }
  }
  jbod_client_operation((block << 24) | JBOD_SEEK_TO_BLOCK, NULL);
 
  //if multiple blocks are needed to be written to
  if ((blockpos + len) > 255){
    int remaininglen = len - (256-blockpos);
    int currentspot = 256 - blockpos;
    memcpy(&tempbuf[blockpos], buf,(256-blockpos));
    jbod_client_operation(JBOD_WRITE_BLOCK, tempbuf);
    uint8_t lookupbuf[256];
    if (cache_lookup(disk, block, lookupbuf) == 1){
      cache_update(disk, block, tempbuf);
    }
    block++;
 
    //check if you reached the end of the disk
    if(block > 255){
      jbod_client_operation((disk + 1)<< 20 | JBOD_SEEK_TO_DISK, NULL);
      jbod_client_operation(JBOD_SEEK_TO_BLOCK, NULL);
      block = 0;
      disk += 1;
    }
 
    //check if more than two blocks need to be written to (implements a while loop to write until one block is left to write)
    if (remaininglen > 256){
      while (remaininglen > 256){
        memcpy(tempbuf, buf + currentspot, 256);
        jbod_client_operation(JBOD_WRITE_BLOCK, tempbuf);
        if (cache_lookup(disk, block, lookupbuf) == 1){
          cache_update(disk, block, tempbuf);
        }
        block++;
 
        //checks if it reaches the end of the disk after every block iteration
        if(block > 255){
          jbod_client_operation((disk + 1)<< 20 | JBOD_SEEK_TO_DISK, NULL);
          jbod_client_operation(JBOD_SEEK_TO_BLOCK, NULL);
          block = 0;
          disk += 1;
        }
        currentspot += 256;
        remaininglen -= 256;
      }
    }
 
    //final block writing/reading after loop terminates to write the remaining length
    int retval = cache_lookup(disk, block, tempbuf);
    if (retval == -1){
      jbod_client_operation(JBOD_READ_BLOCK, tempbuf);
      cache_insert(disk, block, tempbuf);
    }

    //retrace back the same block/disk to write in the right location
    if(block == 255){
      if(disk == 0){
        jbod_client_operation((1<<24)|JBOD_SEEK_TO_DISK,NULL);
      }
      else{
        jbod_client_operation((disk<<20)|JBOD_SEEK_TO_DISK,NULL);
      }
    }
    jbod_client_operation((block << 24) | JBOD_SEEK_TO_BLOCK, NULL);
 
    memcpy(tempbuf, buf+currentspot, remaininglen);
    jbod_client_operation(JBOD_WRITE_BLOCK, tempbuf);
    if (cache_lookup(disk, block, lookupbuf) == 1){
      cache_update(disk, block, tempbuf);
    }
    block++;
  }
 
  //simple case of just writing to one block
  else{
    memcpy(&tempbuf[blockpos], buf, len);
    jbod_client_operation(JBOD_WRITE_BLOCK, tempbuf);
    uint8_t lookupbuf[256];
    if (cache_lookup(disk, block, lookupbuf) == 1){
      cache_update(disk, block, tempbuf);
    }
    block++;
 
  } 
  return len;
 }
