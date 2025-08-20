#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

int cache_create(int num_entries) {
  //checks if cache is empty
  if (cache != NULL){
    return -1;
  }

  //checks invalid entry size
  if (num_entries < 2 || num_entries > 4096){
    return -1;
  }

  //sets cache size to num entries and allocates enough space for cache
  cache_size = num_entries;
  cache = malloc(num_entries * sizeof(cache_entry_t));
  for(int i = 0; i < cache_size; i++){
    cache[i].valid = 0; 
  }
  return 1;
}

int cache_destroy(void) {
  //doesn't free cache if it is already NULL
  if (cache == NULL){
    return -1;
  }

  //frees cache and sets its size to 0 and makes cache NULL again
  cache_size = 0;
  free(cache);
  cache = NULL;
  return 1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  //checking initial invalid conditions before looking up
  if (cache == NULL){
    return -1;
  }
  if (buf == NULL){
    return -1;
  }
  if(disk_num < 0 || disk_num > 15){
    return -1;
  }
  if(block_num < 0 || block_num > 255){
    return -1;
  }

  num_queries += 1;
  for (int i = 0; i < cache_size; i++){
    //if cache is valid, and disk number and block number matches requested info, copy into provided buffer and return
    if (cache[i].valid == true && cache[i].disk_num == disk_num && cache[i].block_num == block_num){
      num_hits += 1;
      clock += 1;
      memcpy(buf, cache[i].block, JBOD_BLOCK_SIZE);
      cache[i].access_time = clock;
      return 1;
    }
  }
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  //checking invalid conditions 
  if (cache == NULL){
    return;
  }
  if (buf == NULL){
    return;
  }
  if (disk_num < 0 || disk_num > 15){
    return;
  }
  if (block_num < 0 || block_num > 255){
    return;
  }
  //if cache is valid and block matches and disk matches, copy buffer contents into cached block
  for(int i = 0; i < cache_size; i++){
    if(cache[i].valid == true && cache[i].disk_num == disk_num && cache[i].block_num == block_num){
      clock += 1;
      memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);
      cache[i].access_time = clock;
      return;
    }
  }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  //check invalid conditions
  if (cache == NULL){
    return -1;
  }
  if (buf == NULL){
    return -1;
  }
  if(disk_num < 0 || disk_num > 15){
    return -1;
  }
  if(block_num < 0 || block_num > 255){
    return -1;
  }

  //if already in cache, return
  for (int i = 0; i < cache_size; i++){
    if(cache[i].valid == true && cache[i].disk_num == disk_num && cache[i].block_num == block_num){
        return -1;
    }
  }

  //if there is an empty spot in cache, insert it there
  for (int i = 0; i < cache_size; i++){
    if (!cache[i].valid){
      cache[i].valid = true; 
      cache[i].disk_num = disk_num;
      cache[i].block_num = block_num;
      memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);
      cache[i].access_time = clock;
      return 1;
    }
  }

  //keep track of LRU cache entry
  int leastRecent = cache[0].access_time;
  int pos = 0;

  //check for LRU entry in cache
  for(int i = 1; i < cache_size; i++){
    if (cache[i].access_time < leastRecent){
      leastRecent = cache[i].access_time;
      pos = i;
    }
  }

  //replace LRU entry with new entry 
  cache[pos].disk_num = disk_num;
  cache[pos].block_num = block_num;
  memcpy(cache[pos].block, buf, JBOD_BLOCK_SIZE);
  cache[pos].access_time = clock;

  return 1;
}

bool cache_enabled(void) {
  if(cache_size > 2){
    return true;
  }
  return false;
}

void cache_print_hit_rate(void) {
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}