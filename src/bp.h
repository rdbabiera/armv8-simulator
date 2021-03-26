/*
 * RD Babiera
 * Li
 * CMSC 22200
 * 11/17/2020
 * Lab 4
 */


#ifndef _BP_H_
#define _BP_H_

#include "pipe.h"

typedef struct btb_entry {
    uint64_t address_tag;
    int valid_bit;
    int conditional_int;
    uint64_t br_target;
} btb_entry;

typedef struct bp_t {
    /* gshare */
    int ghr;
    int pht_counter[256];

    /* BTB */
    btb_entry btb[1024];
} bp_t;

void bp_init();
void bp_predict(uint64_t PC);
void bp_update(uint64_t PC, Instruction inst, Inst_Format inst_type, uint64_t br_target, int branch_taken, int* correct_branch);
#endif
