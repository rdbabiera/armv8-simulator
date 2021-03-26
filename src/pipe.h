/*
 * RD Babiera
 * Li
 * CMSC 22200
 * 11/17/2020
 * Lab 4
 */

#ifndef _PIPE_H_
#define _PIPE_H_

#include "shell.h"
#include "stdbool.h"
#include <limits.h>

/* Instruction-Related Enumerations */
typedef enum Instruction {
    ADD, ADD_I, ADDS, ADDS_I, CBNZ, CBZ, AND, ANDS, EOR, ORR,
    LDUR, LDURB, LDURH, LS, MOVZ, STUR, STURB, STURH, SUB, SUB_I,
    SUBS, SUBS_I, MUL, HLT, CMP, CMP_I, BR, B, BCOND, NONE
} Instruction;

typedef enum Inst_Format {
    R_TYPE, I_TYPE, D_TYPE, B_TYPE, CB_TYPE, IW_TYPE, NONE_TYPE
} Inst_Format;

/* Pipeline Register Structs */
typedef struct pipeReg_IFID {
	uint32_t IR_d;
	uint64_t PC_d;

	int passed_something;
	int is_bubble;
} pipeReg_IFID;

typedef struct pipeReg_IDEX {
	Instruction inst;
	Inst_Format inst_type;
	uint32_t IR_e;
	uint64_t PC_e;

	int64_t A_e;
	int64_t B_e;
	int32_t write_reg;
	int32_t A_reg;
	int32_t B_reg;
	uint32_t Imm_e;

    uint32_t flag_n;
    uint32_t flag_z;

	int passed_something;
	int is_bubble;
} pipeReg_IDEX;

typedef struct pipeReg_EXMEM {
	Instruction inst;
	Inst_Format inst_type;
	uint64_t nPC_m;
	uint32_t IR_m;

	int64_t Aout_m;
	int64_t B_m;
	int32_t write_reg;
	uint32_t flag_n;
	uint32_t flag_z;

    int32_t regWrite;

	int passed_something;
	int is_bubble;
} pipeReg_EXMEM;

typedef struct pipeReg_MEMWB {
	Instruction inst;
	Inst_Format inst_type;
    uint32_t IR_w;

	int64_t MDR_w;
	int64_t Aout_w;
	int32_t write_reg;
	uint32_t flag_n;
	uint32_t flag_z;

    int32_t regWrite;

	int passed_something;
	int is_bubble;
} pipeReg_MEMWB;

typedef struct pipeControl {
    int halt_decoded;
    int halt_timer;
    int load_management;
    int forwardFlags;
	int modifiedFlags;

    int branch_taken;
	int flush_pipeline;
	int flush_signal;

	int dcache_finished;
} pipeControl;

/* Initialization Begin */
pipeReg_IFID pReg_IFID;
pipeReg_IDEX pReg_IDEX;
pipeReg_EXMEM pReg_EXMEM;
pipeReg_EXMEM saved_EXMEM;
pipeReg_MEMWB pReg_MEMWB;
pipeControl pCon;

/* ------------------------------------------------ */

#include "bp.h"

bp_t bpt;

/* ------------------------------------------------ */

typedef struct CPU_State {
	/* register file state */
	int64_t REGS[ARM_REGS];
	int FLAG_N;        /* flag N */
	int FLAG_Z;        /* flag Z */

	/* program counter in fetch stage */
	uint64_t PC;
	
} CPU_State;

int RUN_BIT;

/* global variable -- pipeline state */
extern CPU_State CURRENT_STATE;

/* called during simulator startup */
void pReg_init();
void pipe_init();

/* this function calls the others */
void pipe_cycle();

/* each of these functions implements one stage of the pipeline */
void pipe_stage_fetch();
void pipe_stage_decode();
void pipe_stage_execute();
void pipe_stage_mem();
void pipe_stage_wb();

#endif
