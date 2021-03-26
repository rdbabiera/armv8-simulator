/*
 * RD Babiera
 * Li
 * CMSC 22200
 * 11/17/2020
 * Lab 4
 */

#include "cache.h"
#include "shell.h"
#include <stdlib.h>
#include <stdio.h>

/* Basic cache notes:
 * Write Back: Dirty bit manages whether or not the 
 * block is written back on replacement.
 * 
 * Write Allocate: Block load on a write miss, then the
 * write-hit action follows.
 * 
 * Instruction Cache:
 * Seems like each miss essentially gives you the next 8 instructions in
 * memory.
*/

int get_set(uint64_t addr, char type){
    if (type == 'i'){
        return (addr >> 5) & 0x3F;
    } else if (type == 'd'){
        return (addr >> 5) & 0xFF;
    } else {
        fprintf(stderr, "Code doesn't support the type of cache passed\n");
    }
    return -1;
}

uint64_t get_tag(uint64_t addr, char type) {
    if (type == 'i'){
        return (addr >> 11);
    } else if (type == 'd'){
        return (addr >> 13);
    } else {
        fprintf(stderr, "Code doesn't support the type of cache passed\n");
    }
    return 0;
}

/* get_way() should only be used on hit */
int get_way(cache_t *c, uint64_t addr){
    int set_mask;
    uint64_t addr_tag;
    if (c->type == 'i'){
        set_mask = 0x3F;
        addr_tag = (addr >> 11);
    } else if (c->type == 'd'){
        set_mask = 0xFF;
        addr_tag = (addr >> 13);
    } else {
        fprintf(stderr, "Code doesn't support the type of cache passed\n");
        return -1;
    }
    int set_index = (addr >> 5) & set_mask;

    /* Handle Cache Hits */
    // If you find the block, then you need to manage what happens.
    int i;
    int set_line;
    for (i=0; i<c->b_ways; i++){
        if (c->sets[set_index].lines[i].valid && 
                (c->sets[set_index].lines[i].tag == addr_tag)){
            return i;
        }
    }
    return -1;
}

int cache_reset(char type, uint64_t addr, int set, uint64_t tag){
    int set_mask;
    uint64_t addr_tag;
    if (type == 'i'){
        set_mask = 0x3F;
        addr_tag = (addr >> 11);
    } else if (type == 'd'){
        set_mask = 0xFF;
        addr_tag = (addr >> 13);
    } else {
        fprintf(stderr, "Code doesn't support the type of cache passed\n");
    }
    int set_index = (addr >> 5) & set_mask;

    if ((set_index != set) || (addr_tag != tag)){
        return 1;
    } else {
        return 0;
    }
}

// Initializaton and Completion Functions Start
cache_t *cache_new(int sets, int ways, int block) {
    int i, j;
    int block_array_size = block / 4;
    cache_t* cache = (cache_t*)malloc(sizeof(cache_t));
    cache->b_sets = sets;
    cache->b_ways = ways;
    cache->b_block = block_array_size;
    if (ways == 4){
        cache->type = 'i';
    } else {
        cache->type = 'd';
    }
    cache->sets = (set*)malloc(sizeof(set) * sets);

    for (i=0; i<sets; i++){
        cache->sets[i].lines = (line*)malloc(sizeof(line) * ways);
        for (j=0; j<ways; j++){
            cache->sets[i].lines[j].valid = 0;
            cache->sets[i].lines[j].tag = 0;
            cache->sets[i].lines[j].data = (uint32_t*)malloc(sizeof(uint32_t) * block_array_size);

            cache->sets[i].lines[j].dirty = 0;
            cache->sets[i].lines[j].lru_counter = 0;
        }
    }

    return cache;
}

void cache_destroy(cache_t *c) {
    int i, j;
    for (i=0; i<c->b_sets; i++){
        for (j=0; j<c->b_ways; j++){
            free(c->sets[i].lines[j].data);
        }
        free(c->sets[i].lines); 
    }
    free(c);
}

void cache_updatemem(cache_t *c){
    int i, j, k;
    uint64_t temp_addr;
    for (i=0; i < c->b_sets; i++){
        for (j=0; j < c->b_ways; j++){
            if (c->sets[i].lines[j].dirty){
                temp_addr = (c->sets[i].lines[j].tag << 13) | (i << 5);
                for (k=0; k<8; k++){
                    mem_write_32(temp_addr, c->sets[i].lines[j].data[k]);
                    temp_addr += 4;
                }
            }
        }
    }
}

// Initializaton and Completion Functions End

int cache_check_hit(cache_t *c, uint64_t addr) {
    int hit = 0;

    int set_mask;
    uint64_t addr_tag;
    if (c->type == 'i'){
        set_mask = 0x3F;
        addr_tag = (addr >> 11);
    } else if (c->type == 'd'){
        set_mask = 0xFF;
        addr_tag = (addr >> 13);
    } else {
        fprintf(stderr, "Code doesn't support the type of cache passed\n");
    }
    int set_index = (addr >> 5) & set_mask;

    /* Handle Cache Hits */
    // If you find the block, then you need to manage what happens.
    int i;
    int set_line;
    for (i=0; i<c->b_ways; i++){
        if (c->sets[set_index].lines[i].valid && 
                (c->sets[set_index].lines[i].tag == addr_tag)){
            c->sets[set_index].lines[i].lru_counter = 0;        
            hit = 1;
        } else if (c->sets[set_index].lines[i].valid) {
            c->sets[set_index].lines[i].lru_counter++;
        }
    }
    return hit;
}

void cache_insert_block(cache_t *c, uint64_t addr) {
    int set_mask;
    uint64_t addr_tag;
    if (c->type == 'i'){
        set_mask = 0x3F;
        addr_tag = (addr >> 11);
    } else if (c->type == 'd'){
        set_mask = 0xFF;
        addr_tag = (addr >> 13);
    } else {
        fprintf(stderr, "Code doesn't support the type of cache passed\n");
    }
    int set_index = (addr >> 5) & set_mask;

    /* Handle Set Misses, Load Data */
    int i;
    int empty_exists;
    int empty_index;
    int big_lru = c->sets[set_index].lines[0].lru_counter;
    int big_lru_index = 0;
    uint64_t eviction_addr;

    for (i=0; i<c->b_ways; i++) {
        if (c->sets[set_index].lines[i].lru_counter > big_lru){
            big_lru = c->sets[set_index].lines[i].lru_counter;
            big_lru_index = i;
    }
        if (!c->sets[set_index].lines[i].valid){
            empty_exists = 1;
            empty_index= i;
            break;
        }
    }

    uint64_t temp_addr = addr & ((~0) << 5);
    //printf("%lx\n", temp_addr);
    if (empty_exists){
        c->sets[set_index].lines[empty_index].valid = 1;
        c->sets[set_index].lines[empty_index].tag = addr_tag;
        c->sets[set_index].lines[empty_index].lru_counter = 0;
        c->sets[set_index].lines[empty_index].dirty = 0;
        for (i=0;i<c->b_block;i++){
            c->sets[set_index].lines[empty_index].data[i] = 
                mem_read_32(temp_addr);
            temp_addr += 4;
        }
        //printf("Cache fill - empty\n");
    } else {
        // Eviction currently wrong
        if (c->type == 'i'){
            
        } else if (c->type == 'd'){
            eviction_addr = (c->sets[set_index].lines[big_lru_index].tag) << 13;
            eviction_addr = eviction_addr | (set_index << 5);
            if (c->sets[set_index].lines[big_lru_index].dirty){
                for (i=0;i<c->b_block;i++){
                    mem_write_32(eviction_addr, 
                        c->sets[set_index].lines[big_lru_index].data[i]);
                    eviction_addr += 4;
                }
            }
        }

        temp_addr = addr;
        c->sets[set_index].lines[big_lru_index].valid = 1;
        c->sets[set_index].lines[big_lru_index].tag = addr_tag;
        c->sets[set_index].lines[big_lru_index].lru_counter = 0;
        c->sets[set_index].lines[big_lru_index].dirty = 0;
        for (i=0;i<c->b_block;i++){
            c->sets[set_index].lines[big_lru_index].data[i] = 
                mem_read_32(temp_addr);
            temp_addr += 4;
        }
        //printf("Cache fill - eviction\n");
    }
}

uint32_t cache_load(uint64_t addr, char type, int way_index) {
    int block_offset = (addr & 0x1F) / 4;
    int set_index;

    int set_mask;
    uint64_t addr_tag;
    if (type == 'i'){
        set_mask = 0x3F;
        addr_tag = (addr >> 11);
    } else if (type == 'd'){
        set_mask = 0xFF;
        addr_tag = (addr >> 13);
    } else {
        fprintf(stderr, "Code doesn't support the type of cache passed\n");
    }
    set_index = (addr >> 5) & set_mask;

    if (type == 'i'){
        return(i_cache->sets[set_index].lines[way_index].data[block_offset]);
    } else if (type == 'd'){
        return(d_cache->sets[set_index].lines[way_index].data[block_offset]);
    } else {
        fprintf(stderr, "Code doesn't support the type of cache passed\n");
        return -1;
    }
}

void cache_store(uint64_t addr, char type, uint32_t value, int way_index) {
    int block_offset = (addr & 0x1F) / 4;
    int set_index;

    int set_mask;
    uint64_t addr_tag;
    if (type == 'i'){
        set_mask = 0x3F;
        addr_tag = (addr >> 11);
    } else if (type == 'd'){
        set_mask = 0xFF;
        addr_tag = (addr >> 13);
    } else {
        fprintf(stderr, "Code doesn't support the type of cache passed\n");
    }
    set_index = (addr >> 5) & set_mask;

    if (type == 'i'){
        i_cache->sets[set_index].lines[way_index].data[block_offset] = value;
    } else if (type == 'd'){
        d_cache->sets[set_index].lines[way_index].data[block_offset] = value;
        d_cache->sets[set_index].lines[way_index].dirty = 1;
    } else {
        fprintf(stderr, "Code doesn't support the type of cache passed\n");
    }
}

