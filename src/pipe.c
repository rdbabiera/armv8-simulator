/*
 * RD Babiera
 * Li
 * CMSC 22200
 * 11/17/2020
 * Lab 4
 */

#include "pipe.h"
#include "bp.h" 
#include "cache.h"
#include "shell.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>


/* Cache Management Counters */
int i_cache_delay = 0;
int d_cache_delay = 0;

int d_cache_stall = 0;

/* global pipeline state */
CPU_State CURRENT_STATE;

int debug = 0;
int debug_cache = 0;
int debug_cycle = 1;

void pReg_init() {
    pReg_IFID.is_bubble = 1;

    pReg_IDEX.inst = NONE;
    pReg_IDEX.inst_type = NONE_TYPE;
    pReg_IDEX.passed_something = 0;
    pReg_IDEX.is_bubble = 1;

    pReg_EXMEM.inst = NONE;
    pReg_EXMEM.inst_type = NONE_TYPE;
    pReg_EXMEM.passed_something = 0;
    pReg_EXMEM.is_bubble = 1;

    pReg_MEMWB.inst = NONE;
    pReg_MEMWB.inst_type = NONE_TYPE;
    pReg_MEMWB.passed_something = 0;
    pReg_MEMWB.is_bubble = 1;

    pCon.halt_decoded = 0;
    pCon.forwardFlags = 0;
    pCon.modifiedFlags = 0;

    pCon.branch_taken = 0;
    pCon.flush_pipeline = 0;
    pCon.flush_signal = 0;
    pCon.dcache_finished = 0;
}

void pipe_init()
{
    pReg_init();
    bp_init();
    i_cache = cache_new(64, 4, 32);
    d_cache = cache_new(256, 8, 32);

    memset(&CURRENT_STATE, 0, sizeof(CPU_State));
    CURRENT_STATE.PC = 0x00400000;
}

void pipe_cycle()
{
    // printf("Cycle: %i\n", debug_cycle);
	pipe_stage_wb();
	pipe_stage_mem();
	pipe_stage_execute();
	pipe_stage_decode();
	pipe_stage_fetch();
    debug_cycle++;
}

/* ---------- Instruction Fetch ---------- */
uint64_t temp_block;
int use_temp_block = 0;

uint64_t emergency_add;
int use_emergency_add;

int cache_hit;
int fetch_set;
int fetch_way;

int send_instruction = 1;

void pipe_stage_fetch() {
    if (pCon.load_management){
        pCon.load_management = 0;
        return;
    }

    if (d_cache_delay > 0 || pCon.dcache_finished || d_cache_stall){
        if (pCon.dcache_finished){
            pCon.dcache_finished = 0;
        }
        if (d_cache_stall){
            d_cache_stall = 0;
        }
        if (!send_instruction){
            return;
        }
    }

    if (pCon.halt_decoded){
        return;
    }

    /* Non d_cache cache mechanisms*/
    if (pCon.flush_signal){
        int reset_cache;

        uint64_t prebranch_addr = CURRENT_STATE.PC;
        int prebranch_set = get_set(prebranch_addr, 'i');
        uint64_t prebranch_tag = get_tag(prebranch_addr, 'i');

        CURRENT_STATE.PC = pReg_EXMEM.nPC_m;
        if (i_cache_delay > 0){
            reset_cache = cache_reset('i', CURRENT_STATE.PC, prebranch_set, 
                prebranch_tag);
            if (reset_cache){
                i_cache_delay = 0;
            } else {
                if (i_cache_delay == 1){
                    cache_insert_block(i_cache, prebranch_addr);
                    i_cache_delay--;
                } else {
                    i_cache_delay--;
                    temp_block = prebranch_addr;
                    use_temp_block = 1;
                    if (debug_cache){
                        printf("icache bubble (%d)\n", i_cache_delay + 1);
                    }
                }
            }
        }
        pReg_IFID.passed_something = 0;
        return;
    }

    if (i_cache_delay == 1) {
        if (debug_cache){
            printf("icache fill at cycle %i\n", debug_cycle);
        }
        if (use_temp_block){
            cache_insert_block(i_cache, temp_block);
            use_temp_block = 0;
        } else {
            cache_insert_block(i_cache, CURRENT_STATE.PC);
        }
        i_cache_delay--;
        pReg_IFID.passed_something = 0;
        return;
    } else if (i_cache_delay > 0){
        i_cache_delay--;
        if (debug_cache){
            printf("icache bubble (%d)\n", i_cache_delay + 1);
        }
        pReg_IFID.passed_something = 0;
        return;
    }

    cache_hit = cache_check_hit(i_cache, CURRENT_STATE.PC);
    if (!cache_hit){
        if (debug_cache){
            printf("icache miss (0x%lx) at cycle %i\n", CURRENT_STATE.PC, debug_cycle);
        }
        i_cache_delay = 49;
        pReg_IFID.passed_something = 0;
        return;
    } else {
        if (debug_cache){
            printf("icache hit (0x%lx) at cycle %i\n", CURRENT_STATE.PC, debug_cycle);
        }
    }

    /* Get Fetch Set */
    fetch_set = get_set(CURRENT_STATE.PC, 'i');
    fetch_way = get_way(i_cache, CURRENT_STATE.PC);
    

    pReg_IFID.IR_d = cache_load(CURRENT_STATE.PC, 'i', fetch_way);
    pReg_IFID.PC_d = CURRENT_STATE.PC;
    pReg_IFID.passed_something = 1;

    if (!pCon.halt_decoded){
        if (debug){
            printf("halt not decoded\n");
        }
        bp_predict(CURRENT_STATE.PC);
    } 

    if (debug){
        printf("Instruction: %x\n", pReg_IFID.IR_d);
        printf("Current State: %lx\n", CURRENT_STATE.PC);
    }
}

/* ---------- Instruction Decode ---------- */
void instruction_identify() {

	int inst_found = 0;

	Instruction INST_TYPE = NONE;
	Inst_Format OpcodeFormat = NONE_TYPE;
	uint32_t CURRENT_INST = pReg_IFID.IR_d;

	if (!inst_found){
        uint32_t r_opcode = (CURRENT_INST >> 21) & 0x7FF;
        switch(r_opcode){
            case 0x458 ... 0x459:
                INST_TYPE = ADD;
                break;
            case 0x558 ... 0x559:
                INST_TYPE = ADDS;
                break;
            case 0x450:
            case 0x452:
            case 0x454:
            case 0x456:
                INST_TYPE = AND;
                break;
            case 0x750 ... 0x756:
                INST_TYPE = ANDS;
                break;
            case 0x650:
            case 0x652:
            case 0x654:
            case 0x656:
                INST_TYPE = EOR;
                break;
            case 0x550:
            case 0x552:
            case 0x554:
            case 0x556:
                INST_TYPE = ORR;
                break;
            case 0x658 ... 0x659:
                INST_TYPE = SUB;
                break;
            case 0x758:
                INST_TYPE = SUBS;
                break;
            case 0x759:
                if ((CURRENT_INST & 0x1F) == 0x1F){
                    INST_TYPE = CMP;
                } else {
                    INST_TYPE = SUBS;
                }
                break;
            case 0x4D8:
                INST_TYPE = MUL;
                break;
            case 0x6B0:
                INST_TYPE = BR;
                break;
        }
        if (INST_TYPE != NONE){
            inst_found = 1;
            OpcodeFormat = R_TYPE;
        }
    }

    /* I Types */
    if (!inst_found){
        uint32_t i_opcode = (CURRENT_INST >> 22) & 0x3FF;
        switch(i_opcode){
            case 0x244 ... 0x245:
                INST_TYPE = ADD_I;
                break;
            case 0x2C4 ... 0x2C5:
                INST_TYPE = ADDS_I;
                break;
            case 0x34D:
                INST_TYPE = LS;
                break;
            case 0x344 ... 0x345:
                INST_TYPE = SUB_I;
                break;
            case 0x3C4 ... 0x3C5:
                if ((CURRENT_INST & 0x1F) == 0x1F){
                    INST_TYPE = CMP_I;
                } else {
                    INST_TYPE = SUBS_I;
                }
                break;
        }
        if (INST_TYPE != NONE){
            inst_found = 1;
            OpcodeFormat = I_TYPE;
        }
    }

    /* D Types */
    if (!inst_found){
        uint32_t d_opcode = (CURRENT_INST >> 21) & 0x7FF;
        switch(d_opcode){
            case 0x7C2:
            case 0x5C2:
                INST_TYPE = LDUR;
                break;
            case 0x1C2:
                INST_TYPE = LDURB;
                break;
            case 0x3C2:
                INST_TYPE = LDURH;
                break;
            case 0x5C0:
            case 0x7C0:
                INST_TYPE = STUR;
                break;
            case 0x1C0:
                INST_TYPE = STURB;
                break;
            case 0x3C0:
                INST_TYPE = STURH;
                break;
        }
        if (INST_TYPE != NONE){
            inst_found = 1;
            OpcodeFormat = D_TYPE;
        }
    }

    /* B Types */
    if (!inst_found){
        uint32_t b_opcode = (CURRENT_INST >> 26) & 0x3F;
        switch(b_opcode){
            case 0x5:
                INST_TYPE = B;
                break;
        }
        if (INST_TYPE != NONE){
            inst_found = 1;
            OpcodeFormat = B_TYPE;
        }
    }

    /* CB Types */
    if (!inst_found){
        uint32_t cb_opcode = (CURRENT_INST >> 24) & 0xFF;
        switch(cb_opcode){
            case 0xB5:
                INST_TYPE = CBNZ;
                break;
            case 0xB4:
                INST_TYPE = CBZ;
                break;
            case 0x54:
                INST_TYPE = BCOND;
				break;
        }
        if (INST_TYPE != NONE){
            inst_found = 1;
            OpcodeFormat = CB_TYPE;
        }
    }

    /* IW Types */
    if (!inst_found){
        uint32_t iw_opcode = (CURRENT_INST >> 21) & 0x7FF;
        switch(iw_opcode){
            case 0x694 ... 0x697:
                INST_TYPE = MOVZ;
                break;
            case 0x6A2:
                INST_TYPE = HLT;
                break;
        }
        if (INST_TYPE != NONE){
            inst_found = 1;
            OpcodeFormat = IW_TYPE;
        }
    }

	pReg_IDEX.inst = INST_TYPE;
	pReg_IDEX.inst_type = OpcodeFormat;

    if (debug && pReg_IDEX.inst == NONE){
        printf("Decode did not find an instruction\n");
    }
}

void pipe_stage_decode() {

    if (d_cache_delay == 49){
        if (!pReg_IFID.passed_something){
            pReg_IDEX.is_bubble = 1;
            send_instruction = 1;
            return;
        } else {
            pReg_IDEX.is_bubble = 0;
        }
    }

    if ((d_cache_delay > 0 && d_cache_delay != 49) || pCon.dcache_finished || d_cache_stall){
        if (pReg_IDEX.is_bubble){
            
        } else {
            send_instruction = 0;
            return;
        }
    }

    if (pCon.halt_decoded){
        return;
    }

    if (pCon.load_management){
        return;
    }

    if (pCon.flush_signal){
        return;
    }

    if (pCon.flush_pipeline) {
        pCon.flush_pipeline--;
        pReg_IDEX.inst = NONE;
        pReg_IDEX.inst_type = NONE_TYPE;
        return;
    }

    if (!pReg_IFID.passed_something){
        pReg_IDEX.passed_something = 0;
        pReg_IDEX.inst = NONE;
        pReg_IDEX.inst_type = NONE_TYPE;
        return;
    }

    instruction_identify();

    int32_t rd, rn, shamt, rm;
    int32_t imm12;
    int32_t rt, imm9;
    int32_t br_add, cond_br_add;
    int32_t mov_imm;

    switch (pReg_IDEX.inst_type){
        case R_TYPE:
            rd = pReg_IFID.IR_d & 0x1F;
            rn = (pReg_IFID.IR_d >> 5) & 0x1F;
            shamt = (pReg_IFID.IR_d >> 10) & 0x3F;
            rm = (pReg_IFID.IR_d >> 16) & 0x1F;

	        pReg_IDEX.write_reg = rd;
	        pReg_IDEX.A_reg = rn;
	        pReg_IDEX.B_reg = rm;
	        pReg_IDEX.Imm_e = shamt;
	        pReg_IDEX.A_e = CURRENT_STATE.REGS[rn];
	        pReg_IDEX.B_e = CURRENT_STATE.REGS[rm];
            break;
        case I_TYPE:
            rd = pReg_IFID.IR_d & 0x1F;
            rn = (pReg_IFID.IR_d >> 5) & 0x1F;
            imm12 = (pReg_IFID.IR_d >> 10) & 0xFFF;

	        pReg_IDEX.write_reg = rd;
	        pReg_IDEX.A_reg = rn;
	        pReg_IDEX.Imm_e = imm12;
	        pReg_IDEX.A_e = CURRENT_STATE.REGS[rn];

            pReg_IDEX.B_reg = 32; //need some arbitrary out of bounds
            break;
        case D_TYPE:
            rt = pReg_IFID.IR_d & 0x1F; //load register, or data store value
            rn = (pReg_IFID.IR_d >> 5) & 0x1F;
            imm9 = (pReg_IFID.IR_d >> 12) & 0x1FF;

            pReg_IDEX.A_e = CURRENT_STATE.REGS[rn];
            pReg_IDEX.Imm_e = imm9;
            pReg_IDEX.A_reg = rn;

	        if (pReg_IDEX.inst == LDUR || pReg_IDEX.inst == LDURB || 
                    pReg_IDEX.inst == LDURH) {
		        pReg_IDEX.write_reg = rt;
                pReg_IDEX.B_reg = 32;
	        } else {
		        pReg_IDEX.B_reg = rt;
		        pReg_IDEX.B_e = CURRENT_STATE.REGS[rt];
                pReg_IDEX.write_reg = 32;
	        }
            break;
        case B_TYPE:
            br_add = pReg_IFID.IR_d & 0x3FFFFFF;

	        pReg_IDEX.Imm_e = br_add;
            pReg_IDEX.A_reg = 32;
            pReg_IDEX.B_reg = 32;
            break;
        case CB_TYPE:
            rt = pReg_IFID.IR_d & 0x1F;
            cond_br_add = (pReg_IFID.IR_d >> 5) & 0x7FFFF;

	        pReg_IDEX.A_e = CURRENT_STATE.REGS[rt];
	        pReg_IDEX.A_reg = rt;
	        pReg_IDEX.Imm_e = cond_br_add;

            pReg_IDEX.B_reg = 32;
            break;
        case IW_TYPE:
            rd = pReg_IFID.IR_d & 0x1F;
            mov_imm = (pReg_IFID.IR_d >> 5) & 0xFFFF;
	
	        pReg_IDEX.Imm_e = mov_imm;
	        pReg_IDEX.write_reg = rd;
            pReg_IDEX.A_reg = 32;
            pReg_IDEX.B_reg = 32;
            break;
        case NONE_TYPE:
            pReg_IDEX.write_reg = 32;
            break;
    }
    pReg_IDEX.IR_e = pReg_IFID.IR_d;
	pReg_IDEX.PC_e = pReg_IFID.PC_d;

    if (pCon.forwardFlags){
        pCon.forwardFlags--;
    } else {
        pReg_IDEX.flag_n = CURRENT_STATE.FLAG_N;
	    pReg_IDEX.flag_z = CURRENT_STATE.FLAG_Z;
    }

    pReg_IDEX.passed_something = 1;

    if ((d_cache_delay > 0 && d_cache_delay != 49) || pCon.dcache_finished || d_cache_stall){
        if (pReg_EXMEM.is_bubble){
            pReg_IDEX.is_bubble = 1;
        } else {
            pReg_IDEX.is_bubble = 0;
        }
    }
    
    /* Debugging */
    if (debug) {
        printf("Decode Instruction: %d, Decode Type: %d\n", pReg_IDEX.inst, pReg_IDEX.inst_type);
    }
}

/* ---------- Instruction Execute ---------- */

void set_flags(int64_t value){
    if (value < 0){
        pReg_EXMEM.flag_n = 1;
    } else {
        pReg_EXMEM.flag_n = 0;
    }
    if (value == 0){
        pReg_EXMEM.flag_z = 1;
    } else {
        pReg_EXMEM.flag_z = 0;
    }
    pCon.modifiedFlags = 1;
}

void execute_R() {
    switch(pReg_IDEX.inst){
        case ADD:
            pReg_EXMEM.Aout_m = pReg_IDEX.A_e + pReg_IDEX.B_e;
            break;
        case ADDS:
            pReg_EXMEM.Aout_m = pReg_IDEX.A_e + pReg_IDEX.B_e;
            set_flags(pReg_EXMEM.Aout_m);
            break;
        case AND:
            pReg_EXMEM.Aout_m = pReg_IDEX.A_e & pReg_IDEX.B_e;
            break;
        case ANDS:
            pReg_EXMEM.Aout_m = pReg_IDEX.A_e & pReg_IDEX.B_e;
            set_flags(pReg_EXMEM.Aout_m);
            break;
        case EOR:
            pReg_EXMEM.Aout_m = pReg_IDEX.A_e ^ pReg_IDEX.B_e;
            break;
        case ORR:
            pReg_EXMEM.Aout_m = pReg_IDEX.A_e | pReg_IDEX.B_e;
            break;
        case SUB:
            pReg_EXMEM.Aout_m = pReg_IDEX.A_e - pReg_IDEX.B_e;
            break;
        case SUBS:
        case CMP:
            pReg_EXMEM.Aout_m = pReg_IDEX.A_e - pReg_IDEX.B_e;
            set_flags(pReg_EXMEM.Aout_m);
            break;
        case MUL:
            pReg_EXMEM.Aout_m = pReg_IDEX.A_e * pReg_IDEX.B_e;
            break;
        case BR:
            pReg_EXMEM.nPC_m = pReg_IDEX.A_e;
            pCon.branch_taken = 1;
            break;
    }
    pReg_EXMEM.regWrite = 1;
    if (pReg_IDEX.inst == BR){
        pReg_EXMEM.regWrite = 0;
    }
}

void execute_I() {
    int imms, immr;
    int64_t mask;
    switch(pReg_IDEX.inst){
        case ADD_I:
            pReg_EXMEM.Aout_m = pReg_IDEX.A_e + (int64_t)pReg_IDEX.Imm_e;
            break;
        case ADDS_I:
            pReg_EXMEM.Aout_m = pReg_IDEX.A_e + (int64_t)pReg_IDEX.Imm_e;
            set_flags(pReg_EXMEM.Aout_m);
            break;
        case LS:
            imms = pReg_IDEX.Imm_e & 0x3F;
            immr = (pReg_IDEX.Imm_e >> 6) & 0x3F;

            // LSR
            if (imms == 0x3F){
                mask = ~0;
                mask = ~(mask << (64 - immr));
                pReg_EXMEM.Aout_m = (pReg_IDEX.A_e >> immr) & mask;
            }

            // LSL
            else if (imms != 0x3F){
                int shiftLeft = 63 - imms;
                pReg_EXMEM.Aout_m = pReg_IDEX.A_e << shiftLeft;
            }

            break;
        case SUB_I:
            pReg_EXMEM.Aout_m = pReg_IDEX.A_e - (int64_t)pReg_IDEX.Imm_e;
            break;
        case SUBS_I:
        case CMP_I:
            pReg_EXMEM.Aout_m = pReg_IDEX.A_e - (int64_t)pReg_IDEX.Imm_e;
            set_flags(pReg_EXMEM.Aout_m);
            break;
    }

    pReg_EXMEM.regWrite = 1;
}

void execute_D() {
    int64_t load_offset, read_write_add;
    Instruction inst = pReg_IDEX.inst;
    switch(inst){
        case LDUR:
        case LDURB:
        case LDURH:
        case STUR:
        case STURB:
        case STURH:
            load_offset = (((int64_t) pReg_IDEX.Imm_e) << 55) >> 55;
            read_write_add = pReg_IDEX.A_e + load_offset;
            pReg_EXMEM.Aout_m = read_write_add;
            pReg_EXMEM.B_m = pReg_IDEX.B_e;
    }
    if (inst != STUR && inst != STURB && inst != STURH){
        pReg_EXMEM.regWrite = 1;
    } else {
        pReg_EXMEM.regWrite = 0;
    }
    
}

void execute_B() {
    int64_t br_offset;
    switch(pReg_IDEX.inst){
        case B:
            br_offset = (((((int64_t) pReg_IDEX.Imm_e) << 38) >> 38)) * 4;
            pReg_EXMEM.nPC_m = pReg_IDEX.PC_e + br_offset;
            pCon.branch_taken = 1;
    }
}

void execute_CB() {
    int64_t cbr_offset = (((((int64_t) pReg_IDEX.Imm_e) << 45) >> 45)) * 4;
    int32_t bcond_code;
    switch(pReg_IDEX.inst){
        case CBNZ:
            if (pReg_IDEX.A_e != 0) {
                pReg_EXMEM.nPC_m = pReg_IDEX.PC_e + cbr_offset;
                pCon.branch_taken = 1;
            }
            break;
        case CBZ:
            if (pReg_IDEX.A_e == 0) {
                pReg_EXMEM.nPC_m = pReg_IDEX.PC_e + cbr_offset;
                pCon.branch_taken = 1;
            }
            break;
        case BCOND:
            bcond_code = pReg_IDEX.IR_e & 0x1F;
            switch (bcond_code){
                case 0x0: // BEQ
                    if (pReg_IDEX.flag_z){
                        pReg_EXMEM.nPC_m = pReg_IDEX.PC_e + cbr_offset;
                        pCon.branch_taken = 1;
                    }
                    break;
                case 0x1: // BNE
                    if (!pReg_IDEX.flag_z){
                        pReg_EXMEM.nPC_m = pReg_IDEX.PC_e + cbr_offset;
                        pCon.branch_taken = 1;
                    }
                    break;
                case 0xC: // BGT
                    if (!pReg_IDEX.flag_n && !pReg_IDEX.flag_z){
                        pReg_EXMEM.nPC_m = pReg_IDEX.PC_e + cbr_offset;
                        pCon.branch_taken = 1;
                    }
                    break;
                case 0xB: // BLT
                    if (pReg_IDEX.flag_n && !pReg_IDEX.flag_z){
                        pReg_EXMEM.nPC_m = pReg_IDEX.PC_e + cbr_offset;
                        pCon.branch_taken = 1;
                    }
                    break;
                case 0xA: // BGE
                    if (!pReg_IDEX.flag_n || pReg_IDEX.flag_z){
                        pReg_EXMEM.nPC_m = pReg_IDEX.PC_e + cbr_offset;
                        pCon.branch_taken = 1;
                    }
                    break;
                case 0xD: // BLE
                    if (pReg_IDEX.flag_n || pReg_IDEX.flag_z){
                        pReg_EXMEM.nPC_m = pReg_IDEX.PC_e + cbr_offset;
                        pCon.branch_taken = 1;
                    }
                    break;
            }
    }
    if (!pCon.branch_taken){
        pReg_EXMEM.nPC_m = pReg_IDEX.PC_e + 4;
    }
    pReg_EXMEM.regWrite = 0;
}

void execute_IW() {
    int64_t mov_imm;
    switch(pReg_IDEX.inst){
        case MOVZ:
            mov_imm = (int64_t) pReg_IDEX.Imm_e;
            pReg_EXMEM.Aout_m = mov_imm;
            pReg_EXMEM.regWrite = 1;
            break;
        case HLT:
            if (debug){
                printf("executed halt\n");
            }
            if (!pCon.halt_decoded) {
                pCon.halt_decoded = 1;
                pCon.halt_timer = 1;
                pReg_EXMEM.regWrite = 0;
                break;
            }
            if (pCon.halt_timer){
                if (debug){
                printf("executed halt 2\n");
                }
                pCon.halt_timer--;
                break;
            }
            if (debug){
                printf("executed halt 3\n");
            }
            cache_updatemem(d_cache);
            cache_destroy(d_cache);
            cache_destroy(i_cache);
            RUN_BIT = 0;
            break;
    }
}

void pipe_stage_execute() {

    if (d_cache_delay == 49){
        if (!pReg_IDEX.passed_something){
            pReg_EXMEM.is_bubble = 1;
            return;
        } else {
            pReg_EXMEM.is_bubble = 0;
        }
    }

    if ((d_cache_delay > 0 && d_cache_delay != 49) || pCon.dcache_finished || d_cache_stall){
        if (pReg_EXMEM.is_bubble){

        } else {
            return;
        }
    }

    if (!pReg_IDEX.passed_something){
        pReg_EXMEM.passed_something = 0;
        return;
    }

    if (!pCon.halt_decoded){
        pReg_EXMEM.inst = pReg_IDEX.inst;
    }

    if (pCon.load_management){
        pReg_EXMEM.inst = NONE;
        pReg_EXMEM.passed_something = 0;
        return;
    }

    if (pCon.flush_pipeline) {
        pReg_EXMEM.inst = NONE;
        pReg_EXMEM.inst_type = NONE_TYPE;
        pReg_EXMEM.regWrite = 0;
        return;
    }

    if (pReg_EXMEM.inst == NONE){
        pReg_EXMEM.regWrite = 0;
        return;
    }

    pReg_EXMEM.inst_type = pReg_IDEX.inst_type;
    pReg_EXMEM.nPC_m = pReg_IDEX.PC_e;
    pReg_EXMEM.IR_m = pReg_IDEX.IR_e;
    pReg_EXMEM.write_reg = pReg_IDEX.write_reg;

    pCon.branch_taken = 0;

    switch (pReg_EXMEM.inst_type){
        case R_TYPE:
            execute_R();
            break;
        case I_TYPE:
            execute_I();
            break;
        case D_TYPE:
            execute_D();
            break;
        case B_TYPE:
            execute_B();
            break;
        case CB_TYPE:
            execute_CB();
            break;
        case IW_TYPE:
            execute_IW();
            break;
    }

    // Branch Handling and Mispredictions
    if (pReg_IDEX.inst_type == CB_TYPE || pReg_IDEX.inst_type == B_TYPE || 
            pReg_IDEX.inst == BR){
        int correct_branch = -1;

        bp_update(pReg_IDEX.PC_e, pReg_IDEX.inst, pReg_IDEX.inst_type, 
            pReg_EXMEM.nPC_m, pCon.branch_taken, &correct_branch);

        // Prediction does not Match Target
        // Branch but BTB Miss
        if (!correct_branch) {
            pCon.flush_signal= 1;
        }
    }

    if (pCon.modifiedFlags) {
        pCon.forwardFlags = 1;
        pReg_IDEX.flag_n = pReg_EXMEM.flag_n;
        pReg_IDEX.flag_z = pReg_EXMEM.flag_z;
    }

    pReg_EXMEM.passed_something = 1;

    if ((d_cache_delay > 0 && d_cache_delay != 49) || pCon.dcache_finished || d_cache_stall){
        pReg_EXMEM.is_bubble = 0;
    }

    if (debug) {
        printf("Execute Instruction: %d, Decode Type: %d\n", pReg_EXMEM.inst, pReg_EXMEM.inst_type);
    }
}

/* ---------- Memory Access ---------- */

/* Saved parameters */
int use_saved = 0;

void pipe_stage_mem() {

    int local_regWrite, local_write_reg;
    int32_t local_n, local_z, local_IR;
    Instruction local_inst;
    Inst_Format local_instf;
    int64_t local_bm, local_aout;

    if (d_cache_delay == 1){
        if (debug_cache){
            printf("dcache fill at cycle %i\n", debug_cycle);
        }
        
        cache_insert_block(d_cache, pReg_EXMEM.Aout_m);
        d_cache_delay--;
        pReg_MEMWB.passed_something = 0;
        pCon.dcache_finished = 1;
        return;
    } else if (d_cache_delay > 0){
        d_cache_delay--;
        if (debug_cache){
            printf("dcache stall (%d)\n", d_cache_delay + 1);
        }
        pReg_MEMWB.passed_something = 0;
        return;
    }

    if (pCon.flush_signal){
        pCon.flush_signal--;
        pCon.flush_pipeline = 1;
    }

    if (!pReg_EXMEM.passed_something){
        if (!use_saved){
            pReg_MEMWB.passed_something = 0;
            return;
        }
    }

    if (use_saved){
        local_aout = saved_EXMEM.Aout_m;
        local_write_reg = saved_EXMEM.write_reg;
        local_regWrite = saved_EXMEM.regWrite;
        local_n = saved_EXMEM.flag_n;
        local_z = saved_EXMEM.flag_z;
        local_IR = saved_EXMEM.IR_m;
        local_inst = saved_EXMEM.inst;
        local_instf = saved_EXMEM.inst_type;
        local_bm = saved_EXMEM.B_m;
    } else {
        local_write_reg = pReg_EXMEM.write_reg;
        local_regWrite = pReg_EXMEM.regWrite;
        local_n = pReg_EXMEM.flag_n;
        local_z = pReg_EXMEM.flag_z;
        local_IR = pReg_EXMEM.IR_m;
        local_inst = pReg_EXMEM.inst;
        local_instf = pReg_EXMEM.inst_type;
        local_bm = pReg_EXMEM.B_m;
        local_aout = pReg_EXMEM.Aout_m;
    }  

    if (local_regWrite && (local_write_reg < 31)){
        if (local_write_reg == pReg_IDEX.A_reg){
            pReg_IDEX.A_e = pReg_EXMEM.Aout_m;
        }
        if (local_write_reg == pReg_IDEX.B_reg){
            pReg_IDEX.B_e = pReg_EXMEM.Aout_m;
        }
    }

    if (local_inst == LDUR || local_inst == LDURH || local_inst == LDURB){
        if (local_write_reg == pReg_IDEX.A_reg || local_write_reg == pReg_IDEX.B_reg){
            pCon.load_management = 1;
        }
    }

    if (local_instf == D_TYPE || use_saved){
        uint32_t size, lsb, msb = 0;
        int lsb_way, msb_way;
        int64_t read_write_add;
        int64_t B_m;

        if (use_saved){
            read_write_add = saved_EXMEM.Aout_m;
            B_m =  saved_EXMEM.B_m;
        } else {
            read_write_add = pReg_EXMEM.Aout_m;
            B_m =  pReg_EXMEM.B_m;
        }
        
        int64_t value;
        int cache_hit = cache_check_hit(d_cache, read_write_add);
        int cache_hit2;
        if (!cache_hit){
            if (debug_cache){
                printf("dcache miss (0x%lx) at cycle %i\n", read_write_add, debug_cycle);

            }
            d_cache_delay = 49;
            pReg_MEMWB.passed_something = 0;
            use_saved = 1;
            saved_EXMEM.Aout_m = pReg_EXMEM.Aout_m;
            saved_EXMEM.B_m = pReg_EXMEM.B_m;
            saved_EXMEM.inst = pReg_EXMEM.inst;
            saved_EXMEM.inst_type = pReg_EXMEM.inst_type;
            saved_EXMEM.IR_m = pReg_EXMEM.IR_m;
            saved_EXMEM.flag_n = pReg_EXMEM.flag_n;
            saved_EXMEM.flag_z = pReg_EXMEM.flag_z;
            saved_EXMEM.regWrite = pReg_EXMEM.regWrite;
            saved_EXMEM.write_reg = pReg_EXMEM.write_reg;
            return;
        } else {
            if (debug_cache){
                printf("dcache hit (0x%lx) at cycle %i\n", read_write_add, debug_cycle);
            }
        }

        switch (local_inst) {
            case LDUR:
                size = (pReg_EXMEM.IR_m >> 30) & 0x3;

                lsb_way = get_way(d_cache, read_write_add);
                lsb = cache_load(read_write_add, 'd', lsb_way);
                value = lsb;

                if (size == 3){
                    cache_hit2 = cache_check_hit(d_cache, read_write_add + 4);
                    if (!cache_hit2){
                        msb = mem_read_32(read_write_add + 4);
                    } else {
                        msb_way = get_way(d_cache, read_write_add + 4);
                        msb = cache_load(read_write_add + 4, 'd', msb_way);
                    }              
                    value = value | ((int64_t)msb << 32);
                }

                pReg_MEMWB.MDR_w = value;

                if (debug) {
                    printf("LDUR lsb: %x\n", lsb);
                    printf("LDUR msb: %x\n", msb);
                    printf("Loading from Memory %lx: %lx\n", read_write_add, value);
                }

                break;
            case LDURB:
                lsb_way = get_way(d_cache, read_write_add);
                lsb = cache_load(read_write_add, 'd', lsb_way);
                value = lsb & 0xFF;

                pReg_MEMWB.MDR_w = value;
                if (debug) {
                    printf("Loading from Memory %lx: %lx\n", read_write_add, value);
                }

                break;
            case LDURH:
                lsb_way = get_way(d_cache, read_write_add);
                lsb = cache_load(read_write_add, 'd', lsb_way);
                value = lsb & 0xFFFF;
                
                pReg_MEMWB.MDR_w = value;
                if (debug) {
                    printf("Loading from Memory %lx: %lx\n", read_write_add, value);
                }

                break;
            case STUR:
                size = (pReg_EXMEM.IR_m >> 30) & 0x3;

                lsb_way = get_way(d_cache, read_write_add);
                lsb = B_m & 0xFFFFFFFF;
                msb = (B_m >> 32) & 0xFFFFFFFF;

                cache_store(read_write_add, 'd', lsb, lsb_way);
                if (size == 3){
                    cache_hit2 = cache_check_hit(d_cache, read_write_add + 4);
                    if (!cache_hit2){
                        mem_write_32(read_write_add + 4, msb);
                    } else {
                        cache_store(read_write_add + 4, 'd', msb, msb_way);
                    }
                }

                if (debug) {
                    printf("Storing in Memory %lx: %x\n", read_write_add, lsb);
                }

                break;
            case STURB:
                lsb_way = get_way(d_cache, read_write_add);
                lsb = B_m & 0xFF;
                
                value = cache_load(read_write_add, 'd', lsb_way);
                value = (value & ~0xFF) | lsb;
                cache_store(read_write_add, 'd', lsb, lsb_way);

                if (debug) {
                    printf("Storing in Memory %lx: %lx\n", read_write_add, value);
                }
                break;
            case STURH:
                lsb_way = get_way(d_cache, read_write_add);
                lsb = B_m & 0xFFFF;
                
                value = cache_load(read_write_add, 'd', lsb_way);
                value = (value & ~0xFFFF) | lsb;
                cache_store(read_write_add, 'd', lsb, lsb_way);

                if (debug) {
                    printf("Storing in Memory %lx: %lx\n", read_write_add, value);
                }
                break;
        }
        pReg_MEMWB.Aout_w = read_write_add;
    } else {
        pReg_MEMWB.Aout_w = local_aout;
        if (pCon.modifiedFlags) {
            pCon.modifiedFlags--;
            pReg_MEMWB.flag_n = local_n;
            pReg_MEMWB.flag_z = local_z;
        }
    }

    pReg_MEMWB.inst = local_inst;
    pReg_MEMWB.inst_type = local_instf;

    pReg_MEMWB.write_reg = local_write_reg;
    pReg_MEMWB.regWrite = local_regWrite;
    pReg_MEMWB.IR_w = local_IR;

    pReg_MEMWB.passed_something = 1;

    if (use_saved){
        d_cache_stall = 1;
        use_saved = 0;
    }

    if (debug) {
        printf("Memory Instruction: %d, Decode Type: %d\n", 
            pReg_MEMWB.inst, pReg_MEMWB.inst_type);
    }
}

/* ---------- Write-Back ---------- */

void pipe_stage_wb() {
    if (debug) {
        printf("\nWrite-Back Instruction: %d, Decode Type: %d\n", 
            pReg_MEMWB.inst, pReg_MEMWB.inst_type);
    }

    if (pReg_MEMWB.inst == NONE || !pReg_MEMWB.passed_something){
        return;
    }

    if (pReg_MEMWB.regWrite && (pReg_MEMWB.write_reg < 31)){
        if (pReg_MEMWB.write_reg == pReg_IDEX.A_reg){
            pReg_IDEX.A_e = pReg_MEMWB.Aout_w;
        }
        if (pReg_MEMWB.write_reg == pReg_IDEX.B_reg){
            pReg_IDEX.B_e = pReg_MEMWB.Aout_w;
        }
    }

    if ((pReg_MEMWB.inst == LDUR || pReg_MEMWB.inst == LDURH || 
            pReg_MEMWB.inst == LDURB) && (pReg_MEMWB.write_reg < 31)){
        if (pReg_MEMWB.write_reg == pReg_IDEX.A_reg){
            pReg_IDEX.A_e = pReg_MEMWB.MDR_w;
        }
        if (pReg_MEMWB.write_reg == pReg_IDEX.B_reg){
            pReg_IDEX.B_e = pReg_MEMWB.MDR_w;
        }
    }

    if ((pReg_MEMWB.write_reg < 31) && pReg_MEMWB.regWrite) {
        if (pReg_MEMWB.inst_type != D_TYPE){
            if (debug) {
                printf("Writing to reg %d: %lx\n", pReg_MEMWB.write_reg, pReg_MEMWB.Aout_w);
            }
            CURRENT_STATE.REGS[pReg_MEMWB.write_reg] = pReg_MEMWB.Aout_w;
        } else {
            if (debug) {
                printf("Loading to reg %d: %lx\n", pReg_MEMWB.write_reg, pReg_MEMWB.MDR_w);
            }
            CURRENT_STATE.REGS[pReg_MEMWB.write_reg] = pReg_MEMWB.MDR_w;
        }
    }

    // Write-Back Flags
    CURRENT_STATE.FLAG_Z = pReg_MEMWB.flag_z;
    CURRENT_STATE.FLAG_N = pReg_MEMWB.flag_n;

    if (pReg_MEMWB.inst != NONE){
        stat_inst_retire++;
    }
}
