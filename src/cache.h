/*
 * RD Babiera
 * Li
 * CMSC 22200
 * 11/17/2020
 * Lab 4
 */

#ifndef _CACHE_H_
#define _CACHE_H_

#include <stdint.h>

typedef struct line{
    // Basic Parameters
    int valid;
    uint64_t tag;
    uint32_t* data;

    // for LRU, block swapping
    int dirty;
    int lru_counter;
} line;

typedef struct set{
    line* lines;
} set;

typedef struct cache_t{
    int b_sets;
    int b_ways;
    int b_block;
    char type;

    set* sets;
} cache_t;

/* Declare Caches */
cache_t* i_cache;
cache_t* d_cache;

int get_set(uint64_t addr, char type);
uint64_t get_tag(uint64_t addr, char type);
int get_way(cache_t *c, uint64_t addr);
int cache_reset(char type, uint64_t addr, int set, uint64_t tag);

cache_t *cache_new(int sets, int ways, int block);
void cache_destroy(cache_t *c);
void cache_updatemem(cache_t *c);

int cache_check_hit(cache_t *c, uint64_t addr);
void cache_insert_block(cache_t *c, uint64_t addr);

// int cache_update(cache_t *c, uint64_t addr, int* way_index, int* timer);

uint32_t cache_load(uint64_t addr, char type, int way_index);
void cache_store(uint64_t addr, char type, uint32_t value, int way_index);

#endif
