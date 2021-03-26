/*
 * RD Babiera
 * Li
 * CMSC 22200
 * 11/17/2020
 * Lab 4
 */

#include "pipe.h"
#include "bp.h"
#include <stdio.h>
#include <stdlib.h>

int debug_mode = 0;

void bp_init() {
    bpt.ghr = 0;
    int i;
    for (i=0; i<1024; i++){
        bpt.btb[i].address_tag = 0;
        bpt.btb[i].valid_bit = 0;
        bpt.btb[i].conditional_int = 0;
        bpt.btb[i].br_target = 0;
    }
    for (i=0; i<256; i++){
        bpt.pht_counter[i] = 0;
    }
}

void bp_predict(uint64_t PC) {
    int pht_index = ((PC >> 2) & 0xFF) ^ bpt.ghr;
    int btb_index = (PC >> 2) & 0x3FF;

    int valid_bit = bpt.btb[btb_index].valid_bit;
    uint64_t address_tag = bpt.btb[btb_index].address_tag;

    if (debug_mode) {
        printf("Predict:\n");
        printf("ghr: %x\n", bpt.ghr);
        printf("pht_index: %d, btb_index: %d\n", pht_index, btb_index);
        printf("pht: %d\n", bpt.pht_counter[pht_index]);
    }

    if (!valid_bit || address_tag != PC){
        CURRENT_STATE.PC = PC + 4;
    } else if (!bpt.btb[btb_index].conditional_int ||
               bpt.pht_counter[pht_index] >= 2) {
        CURRENT_STATE.PC = bpt.btb[btb_index].br_target;
    } else {
        CURRENT_STATE.PC = PC + 4;
    }
}

void bp_update(uint64_t PC, Instruction inst, Inst_Format inst_type, 
        uint64_t br_target, int branch_taken, int* correct_branch) {
    int pht_index = ((PC >> 2) & 0xFF) ^ bpt.ghr;
    int btb_index = (PC >> 2) & 0x3FF;

    if (debug_mode) {
        printf("Update:\n");
        printf("ghr: %x\n", bpt.ghr);
        printf("pht_index: %d, btb_index: %d\n", pht_index, btb_index);
        printf("pht: %d\n", bpt.pht_counter[pht_index]);
    }

    /* Update PHT and GHR*/
    if (bpt.btb[btb_index].conditional_int){
        if (branch_taken){
            if (bpt.pht_counter[pht_index] < 3){
                if (bpt.pht_counter[pht_index] < 2){
                    *correct_branch = 0;
                } else {
                    *correct_branch = 1;
                }
                bpt.pht_counter[pht_index]++;
            }
        } else {
            if (bpt.pht_counter[pht_index] > 0){
                if (bpt.pht_counter[pht_index] < 2){
                    *correct_branch = 1;
                } else {
                    *correct_branch = 0;
                }
                bpt.pht_counter[pht_index]--;
            }           
        }
    }

    if (inst_type == CB_TYPE){
        if (branch_taken){
            bpt.ghr = ((bpt.ghr << 1) + 1) & 0xFF;
        } else {
            bpt.ghr = (bpt.ghr << 1) & 0xFF;
        }
    }

    /* Update BTB */
    if (branch_taken && !bpt.btb[btb_index].valid_bit) {
        bpt.btb[btb_index].address_tag = PC;
        bpt.btb[btb_index].valid_bit = 1;

        if (inst_type == CB_TYPE){
            bpt.btb[btb_index].conditional_int = 1;
            if (!branch_taken){
                *correct_branch = 1;
            } else {
                *correct_branch = 0;
            }
        } else {
            bpt.btb[btb_index].conditional_int = 0;
            *correct_branch = 0;
        }

        bpt.btb[btb_index].br_target = pReg_EXMEM.nPC_m;
    } else if (branch_taken && bpt.btb[btb_index].valid_bit) {
        if (bpt.btb[btb_index].br_target != pReg_EXMEM.nPC_m){
            bpt.btb[btb_index].address_tag = PC;
            bpt.btb[btb_index].br_target = pReg_EXMEM.nPC_m;
            if (inst_type == CB_TYPE){
                bpt.btb[btb_index].conditional_int = 1;
            } else {
                bpt.btb[btb_index].conditional_int = 0;
            }
            *correct_branch = 0;
        }
    }

    if (debug_mode) {
        printf("Post Update:\n");
        printf("ghr: %x\n", bpt.ghr);
        printf("pht_index: %d, btb_index: %d\n", pht_index, btb_index);
        printf("pht: %d\n", bpt.pht_counter[pht_index]);
    }
}
