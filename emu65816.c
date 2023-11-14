#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

// Comment out this #define if compiling on a Big Endian System/CPU
#define __EMU_LITTLE_ENDIAN

#include "emu65816_library.h"



// Initalizes the CPU struct
void cpuInit(cpuState* CPU, uint8_t* memory, uint32_t memSize, uint32_t ioAddress, uint32_t ioSize, uint8_t (*ioRead)(uint32_t), void (*ioWrite)(uint32_t, uint8_t)){
	
	CPU->wai = false;
	CPU->stp = false;
	MEM = memory;
	CPU->io_read = ioRead;
	CPU->io_write = ioWrite;
	IOB = ioAddress;
	IOS = ioSize;
	MES = memSize;
	INT = 0;
	DBG = false;
	
	A.w = 0;
	X.w = 0;
	Y.w = 0;
	PC.bl = readMem(CPU, VECT_E_RES);
	PC.bh = readMem(CPU, VECT_E_RES + 1);
	SP.w = 0x01FD;
	DP.w = 0;
	PB = 0;
	DB = 0;
	EF = true;		// Emulation Mode
	
	// printf("CPU starting execution at 0x%04X (read from Reset vector at: 0x%04X)\n", PC.w, VECT_E_RES);
	
	writeSR(CPU, SR_INIT);
	
}


// Executes instructions for a set amount of cycles
// Returns how many Cycles it didn't use, value is negative if it used more Cycles than requested
int32_t cpuExecute(cpuState* CPU, int32_t cycles){
	int32_t cycleRem = cycles;
	uint8_t opcode;
	bool _pre_debug = DBG;
	cint32_t tmp0, tmp1, tmp2;
	
	// If a STP instruction was executed, exit immediately
	if (CPU->stp) return 0;
	
	// If a WAI instruction was executed but no interrupt was issued, exit immediately
	if (CPU->wai) return cycleRem;
	
	// Handle Hardware Interrupts (IRQ, NMI, ABT)
	if(INT){
		if (EF){		// Emulation
			pushStack(CPU, PC.bh);
			pushStack(CPU, PC.bl);
			pushStack(CPU, readSR(CPU) | SR_INT);
			setD(false);
			setI(true);
			PC.bl = readMem(CPU, interruptTableE[INT]);
			PC.bh = readMem(CPU, interruptTableE[INT] + 1);
			PB = 0;
		}else{			// Native
			pushStack(CPU, PB);
			pushStack(CPU, PC.bh);
			pushStack(CPU, PC.bl);
			pushStack(CPU, readSR(CPU));
			setD(false);
			setI(true);
			PC.bl = readMem(CPU, interruptTableN[INT]);
			PC.bh = readMem(CPU, interruptTableN[INT] + 1);
			PB = 0;
		}
		INT = 0;
	}
	
	next:
	
	// Clear the high Bytes of X and Y when XF=1, in case they were changed somehow
	if (XF){
		X.bh = 0;
		Y.bh = 0;
	}
	
	// Update the Upper Byte of the SP when EF=1, in case it got changed somehow
	if (EF) SP.bh = 1;
	
	// Fetch an Opcode
	opcode = fetch(CPU);
	
	// Decode it
	// Run the correct operation
	// Subtract the amount of Cycles spend from the total
	// If the total reaches or supasses 0, get the difference and return it
	// Otherwise execute another Instruction
	
	dbg_printf("Executing Instruction (0x%02X at 0x%02X%04X): ", opcode, PB, PC.w - 1);
	//dbg_printf("\"%s\"", inst_names[opcode]);
	
	
	switch(opcode){
		// Jumps ----------------------------------------------------------------- //
		// Absolute
		case OP_JMP_A:
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			PC.w = tmp0.wl;
			dbg_printf("JMP $%04X", PC.w);
		break;
		
		// Absolute Indirect
		case OP_JMP_AI:
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			PC.bl = readMem(CPU, 0x0000FFFF & (tmp0.wl));
			PC.bh = readMem(CPU, 0x0000FFFF & (tmp0.wl + 1));
			dbg_printf("JMP ($%04X) (Value: $%04X)", tmp0.wl, PC.w);
		break;
		
		// Absolute X Indirect
		case OP_JMP_AXI:
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			PC.bl = readMem(CPU, 0x00FFFFFF & ((PB << 16U) | (((uint32_t)tmp0.wl + X.w) & 0x0000FFFF)));
			PC.bh = readMem(CPU, 0x00FFFFFF & ((PB << 16U) | (((uint32_t)tmp0.wl + X.w + 1) & 0x0000FFFF)));
			dbg_printf("JMP ($%04X,X) (Target: $%06X, Value: $%04X)", tmp0.wl, 0x00FFFFFF & ((PB << 16U) | (((uint32_t)tmp0.wl + X.w) & 0x0000FFFF)), PC.w);
		break;
		
		// Absolute Indirect Long
		case OP_JML_AI:
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			PC.bl = readMem(CPU, 0x0000FFFF & (tmp0.wl));
			PC.bh = readMem(CPU, 0x0000FFFF & (tmp0.wl + 1));
			PB = readMem(CPU, 0x0000FFFF & (tmp0.wl + 2));
			dbg_printf("JML ($%04X) (Value: $%02X%04X)", tmp0.wl, PB, PC.w);
		break;
		
		// Absolute Long
		case OP_JML_AL:
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			tmp0.bh = fetch(CPU);
			PC.w = tmp0.wl;
			PB = tmp0.bh;
			dbg_printf("JML $%02X%04X", PB, PC.w);
		break;
		
		// Absolute
		case OP_JSR_A:
			tmp0.bl = fetch(CPU);
			pushStack(CPU, PC.bh);
			pushStack(CPU, PC.bl);			// Push the PC before fetching the 2nd Operand
			tmp0.bm = fetch(CPU);
			PC.w = tmp0.wl;
			dbg_printf("JSR $%04X ------------------------------------------------", PC.w);
		break;
		
		// Absolute X Indirect
		case OP_JSR_AXI:
			tmp0.bl = fetch(CPU);
			pushStack(CPU, PC.bh);
			pushStack(CPU, PC.bl);			// Push the PC before fetching the 2nd Operand
			tmp0.bm = fetch(CPU);
			PC.bl = readMem(CPU, 0x00FFFFFF & ((PB << 16U) | (((uint32_t)tmp0.wl + X.w) & 0x0000FFFF)));
			PC.bh = readMem(CPU, 0x00FFFFFF & ((PB << 16U) | (((uint32_t)tmp0.wl + X.w + 1) & 0x0000FFFF)));
			dbg_printf("JSR ($%04X,X) (Target: $%06X, Value: $%04X)", tmp0.wl, (tmp0.wl + X.w) & 0x0000FFFF, PC.w);
		break;
		
		// Absolute Long
		case OP_JSL_AL:
			tmp2.bl = EF;			// Save the E Flag
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			setE(false);			// Then Clear it
			pushStack(CPU, PB);
			pushStack(CPU, PC.bh);
			pushStack(CPU, PC.bl);			// Push the PC before fetching the 3rd Operand
			setE(tmp2.bl);		// And afterwards restore it again
			tmp0.bh = fetch(CPU);
			PC.w = tmp0.wl;
			PB = tmp0.bh;
			dbg_printf("JSL $%02X%04X ----------------------------------------------", PB, PC.w);
		break;
		
		// Returns --------------------------------------------------------------- //
		// From Subroutine
		case OP_RTS:
			tmp0.bl = pullStack(CPU);
			tmp0.bm = pullStack(CPU);
			PC.w = tmp0.wl + 1;
			dbg_printf("RTS (Target: $%02X%04X)", PB, PC.w);
		break;
		
		// From Long Subroutine
		case OP_RTL:
			tmp2.bl = EF;			// Save the E Flag
			setE(false);			// Then Clear it
			tmp0.bl = pullStack(CPU);
			tmp0.bm = pullStack(CPU);
			PC.w = tmp0.wl + 1;
			PB = pullStack(CPU);
			setE(tmp2.bl);		// And afterwards restore it again
			dbg_printf("RTL (Target: $%02X%04X)", PB, PC.w);
		break;
		
		// From Interrupt
		case OP_RTI:
			if (EF){		// Emulation
				writeSR(CPU, pullStack(CPU) | SR_BRK);
				PC.bl = pullStack(CPU);
				PC.bh = pullStack(CPU);
			}else{			// Native
				writeSR(CPU, pullStack(CPU));
				PC.bl = pullStack(CPU);
				PC.bh = pullStack(CPU);
				PB = pullStack(CPU);
			}
			dbg_printf("RTI (Target: $%02X%04X)", PB, PC.w);
		break;
		
		// Branches -------------------------------------------------------------- //
		// Branch on Carry Clear/Set
		case OP_BCS_R:
		case OP_BCC_R:
			tmp0.bl = fetch(CPU);					// Relative Offset
			
			if (opcode & 0b00100000){
				dbg_printf("BCS (Target: $%02X%04X, ", PB, PC.w + tmp0.sbl);
			}else{
				dbg_printf("BCC (Target: $%02X%04X, ", PB, PC.w + tmp0.sbl);
			}
			
			if (!CF == !(opcode & 0b00100000)){ 	// If bit 5 of the opcode is the same as the Flag to check, take the branch
				PC.w += tmp0.sbl;
				
				dbg_printf("Taken)");
			}else{
				dbg_printf("Not Taken)");
			}
		break;
		
		// Branch on Zero Clear/Set
		case OP_BEQ_R:
		case OP_BNE_R:
			tmp0.bl = fetch(CPU);					// Relative Offset
			
			if (opcode & 0b00100000){
				dbg_printf("BEQ (Target: $%02X%04X, ", PB, PC.w + tmp0.sbl);
			}else{
				dbg_printf("BNE (Target: $%02X%04X, ", PB, PC.w + tmp0.sbl);
			}
			
			if (!ZF == !(opcode & 0b00100000)){ 	// If bit 5 of the opcode is the same as the Flag to check, take the branch
				PC.w += tmp0.sbl;
				
				dbg_printf("Taken)");
			}else{
				dbg_printf("Not Taken)");
			}
		break;
		
		// Branch on Interrupt Clear/Set
		case OP_BMI_R:
		case OP_BPL_R:
			tmp0.bl = fetch(CPU);					// Relative Offset
			
			if (opcode & 0b00100000){
				dbg_printf("BMI (Target: $%02X%04X, ", PB, PC.w + tmp0.sbl);
			}else{
				dbg_printf("BPL (Target: $%02X%04X, ", PB, PC.w + tmp0.sbl);
			}
			
			if (!NF == !(opcode & 0b00100000)){ 	// If bit 5 of the opcode is the same as the Flag to check, take the branch
				PC.w += tmp0.sbl;
				
				dbg_printf("Taken)");
			}else{
				dbg_printf("Not Taken)");
			}
		break;
		
		// Branch on Overflow Clear/Set
		case OP_BVS_R:
		case OP_BVC_R:
			tmp0.bl = fetch(CPU);					// Relative Offset
			
			if (opcode & 0b00100000){
				dbg_printf("BVS (Target: $%02X%04X, ", PB, PC.w + tmp0.sbl);
			}else{
				dbg_printf("BVC (Target: $%02X%04X, ", PB, PC.w + tmp0.sbl);
			}
			
			if (!VF == !(opcode & 0b00100000)){ 	// If bit 5 of the opcode is the same as the Flag to check, take the branch
				PC.w += tmp0.sbl;
				
				dbg_printf("Taken)");
			}else{
				dbg_printf("Not Taken)");
			}
		break;
		
		// Unconditional Branches
		case OP_BRA_R:
			tmp0.bl = fetch(CPU);					// Relative Offset
			PC.w += tmp0.sbl;
			dbg_printf("BRA (Target: $%02X%04X)", PB, PC.w);
		break;
		
		case OP_BRL_R:
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);					// 16-bit Relative Offset
			PC.w += tmp0.swl;
			dbg_printf("BRA (Target: $%02X%04X)", PB, PC.w);
		break;
		
		// Stack Operations ------------------------------------------------------ //
		// 16 bit Pushes
		// Immediate
		case OP_PEA_IM:
			tmp2.bl = EF;			// Save the E Flag
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			setE(false);			// Then Clear it
			pushStack(CPU, tmp0.bm);
			pushStack(CPU, tmp0.bl);
			setE(tmp2.bl);		// And afterwards restore it again
			dbg_printf("PEA #$%04X", tmp0.wl);
		break;
		
		// Direct Page
		case OP_PEI_DP:
			tmp2.bl = EF;			// Save the E Flag
			tmp0.bl = fetch(CPU);
			tmp1.bl = readDP(CPU, tmp0.bl);
			tmp1.bm = readDP(CPU, tmp0.bl + 1);
			setE(false);			// Then Clear it
			pushStack(CPU, tmp1.bm);
			pushStack(CPU, tmp1.bl);
			setE(tmp2.bl);		// And afterwards restore it again
			dbg_printf("PEI $%02X (Target: $%06X, Value: $%04X)", tmp0.bl, addrDP(CPU, tmp0.bl), tmp1.wl);
		break;
		
		// Immediate
		case OP_PER_IM:
			tmp2.bl = EF;			// Save the E Flag
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			tmp1.wl = tmp0.wl;
			tmp1.wl += PC.w;						// Add the Address of the next Instruction before pushing to the Stack
			setE(false);			// Then Clear it
			pushStack(CPU, tmp1.bm);
			pushStack(CPU, tmp1.bl);
			setE(tmp2.bl);		// And afterwards restore it again
			dbg_printf("PER #$%04X (Value: $%04X)", tmp0.wl, tmp1.wl);
		break;
		
		// Push Registers
		case OP_PHB:
			pushStack(CPU, DB);
			dbg_printf("PHB (Target: $%06X, Value: $%02X)", addrStack(CPU, -1), DB);
		break;
		
		case OP_PHD:
			tmp2.bl = EF;			// Save the E Flag
			setE(false);			// Then Clear it
			pushStack(CPU, DP.bh);
			pushStack(CPU, DP.bl);
			setE(tmp2.bl);		// And afterwards restore it again
			dbg_printf("PHD (Target: $%06X, Value: $%02X)", addrStack(CPU, -1), DP.w);
		break;
		
		case OP_PHK:
			pushStack(CPU, PB);
			dbg_printf("PHK (Target: $%06X, Value: $%02X)", addrStack(CPU, -1), PB);
		break;
		
		case OP_PHP:
			if (EF){		// Emulation
				pushStack(CPU, readSR(CPU) | SR_BRK);
				dbg_printf("PHP (Target: $%06X, Value: $%02X)", addrStack(CPU, -1), readSR(CPU) | SR_BRK);
			}else{			// Native
				pushStack(CPU, readSR(CPU));
				dbg_printf("PHP (Target: $%06X, Value: $%02X)", addrStack(CPU, -1), readSR(CPU));
			}
		break;
		
		case OP_PHA:
			if (!MF) pushStack(CPU, A.bh);
			pushStack(CPU, A.bl);
			
			if (MF){
				dbg_printf("PHA (Target: $%06X, Value: $%02X)", addrStack(CPU, -1), A.bl);
			}else{
				dbg_printf("PHA (Target: $%06X, Value: $%04X)", addrStack(CPU, -1), A.w);
			}
		break;
		
		case OP_PHX:
			if (!XF) pushStack(CPU, X.bh);
			pushStack(CPU, X.bl);
			
			if (XF){
				dbg_printf("PHX (Target: $%06X, Value: $%02X)", addrStack(CPU, -1), X.bl);
			}else{
				dbg_printf("PHX (Target: $%06X, Value: $%04X)", addrStack(CPU, -1), X.w);
			}
		break;
		
		case OP_PHY:
			if (!XF) pushStack(CPU, Y.bh);
			pushStack(CPU, Y.bl);
			
			if (XF){
				dbg_printf("PHY (Target: $%06X, Value: $%02X)", addrStack(CPU, -1), Y.bl);
			}else{
				dbg_printf("PHY (Target: $%06X, Value: $%04X)", addrStack(CPU, -1), Y.w);
			}
		break;
		
		// Pull Registers
		case OP_PLB:
			DB = pullStack(CPU);
			setNZ(CPU, true, DB);
			dbg_printf("PLB (Target: $%06X, Value: $%02X)", addrStack(CPU, -1), DB);
		break;
		
		case OP_PLD:
			tmp2.bl = EF;			// Save the E Flag
			setE(false);			// Then Clear it
			DP.bl = pullStack(CPU);
			DP.bh = pullStack(CPU);
			setE(tmp2.bl);		// And afterwards restore it again
			setNZ(CPU, false, DP.w);
			dbg_printf("PLD (Target: $%06X, Value: $%02X)", addrStack(CPU, -1), DP.w);
		break;
		
		case OP_PLP:
			if (EF){		// Emulation
				writeSR(CPU, pullStack(CPU) | SR_BRK);
			}else{
				writeSR(CPU, pullStack(CPU));
			}
			dbg_printf("PLP (Target: $%06X, Value: $%02X)", addrStack(CPU, -1), readSR(CPU));
		break;
		
		case OP_PLA:
			A.bl = pullStack(CPU);
			if (!MF) A.bh = pullStack(CPU);
			setNZ(CPU, MF, A.w);
			
			if (MF){
				dbg_printf("PLA (Target: $%06X, Value: $%02X)", addrStack(CPU, -1), A.bl);
			}else{
				dbg_printf("PLA (Target: $%06X, Value: $%04X)", addrStack(CPU, -1), A.w);
			}
		break;
		
		case OP_PLX:
			X.bl = pullStack(CPU);
			if (!XF) X.bh = pullStack(CPU);
			setNZ(CPU, XF, X.w);
			
			if (XF){
				dbg_printf("PLX (Target: $%06X, Value: $%02X)", addrStack(CPU, -1), X.bl);
			}else{
				dbg_printf("PLX (Target: $%06X, Value: $%04X)", addrStack(CPU, -1), X.w);
			}
		break;
		
		case OP_PLY:
			Y.bl = pullStack(CPU);
			if (!XF) Y.bh = pullStack(CPU);
			setNZ(CPU, XF, Y.w);
			
			if (XF){
				dbg_printf("PLY (Target: $%06X, Value: $%02X)", addrStack(CPU, -1), Y.bl);
			}else{
				dbg_printf("PLY (Target: $%06X, Value: $%04X)", addrStack(CPU, -1), Y.w);
			}
		break;
		
		// Load/Store Accumulator ------------------------------------------------ //
		// Load
		// Immediate
		case OP_LDA_IM:
			A.bl = fetch(CPU);
			if (!MF) A.bh = fetch(CPU);
			setNZ(CPU, MF, A.w);
			
			if (MF){
				dbg_printf("LDA #$%02X", A.bl);
			}else{
				dbg_printf("LDA #$%04X", A.w);
			}
		break;
		
		// Absolute
		case OP_LDA_A:
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			A.bl = readAbs(CPU, tmp0.wl);
			if (!MF) A.bh = readAbs(CPU, tmp0.wl + 1);
			setNZ(CPU, MF, A.w);
			
			if (MF){
				dbg_printf("LDA $%04X (Target: $%06X, Value: $%02X)", tmp0.wl, addrAbs(CPU, tmp0.wl), A.bl);
			}else{
				dbg_printf("LDA $%04X (Target: $%06X, Value: $%04X)", tmp0.wl, addrAbs(CPU, tmp0.wl), A.w);
			}
		break;
		
		// Absolute X
		case OP_LDA_AX:
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			A.bl = readAbs(CPU, tmp0.wl + X.w);
			if (!MF) A.bh = readAbs(CPU, tmp0.wl + X.w + 1);
			setNZ(CPU, MF, A.w);
			
			if (MF){
				dbg_printf("LDA $%04X,X (Target: $%06X, Value: $%02X)", tmp0.wl, addrAbs(CPU, tmp0.wl + X.w), A.bl);
			}else{
				dbg_printf("LDA $%04X,X (Target: $%06X, Value: $%04X)", tmp0.wl, addrAbs(CPU, tmp0.wl + X.w), A.w);
			}
		break;
		
		// Absolute Y
		case OP_LDA_AY:
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			A.bl = readAbs(CPU, tmp0.wl + Y.w);
			if (!MF) A.bh = readAbs(CPU, tmp0.wl + Y.w + 1);
			setNZ(CPU, MF, A.w);
			
			if (MF){
				dbg_printf("LDA $%04X,Y (Target: $%06X, Value: $%02X)", tmp0.wl, addrAbs(CPU, tmp0.wl + Y.w), A.bl);
			}else{
				dbg_printf("LDA $%04X,Y (Target: $%06X, Value: $%04X)", tmp0.wl, addrAbs(CPU, tmp0.wl + Y.w), A.w);
			}
		break;
		
		// Absolute Long
		case OP_LDA_AL:
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			tmp0.bh = fetch(CPU);
			tmp0.bx = 0;
			A.bl = readMem(CPU, tmp0.l);
			if (!MF) A.bh = readMem(CPU, tmp0.l + 1);
			setNZ(CPU, MF, A.w);
			
			if (MF){
				dbg_printf("LDA $%06X (Value: $%02X)", tmp0.l, A.bl);
			}else{
				dbg_printf("LDA $%06X (Value: $%04X)", tmp0.l, A.w);
			}
		break;
		
		// Absolute Long X
		case OP_LDA_ALX:
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			tmp0.bh = fetch(CPU);
			tmp0.bx = 0;
			A.bl = readMem(CPU, tmp0.l + X.w);
			if (!MF) A.bh = readMem(CPU, tmp0.l + X.w + 1);
			setNZ(CPU, MF, A.w);
			
			if (MF){
				dbg_printf("LDA $%06X,X (Target: $%06X, Value: $%02X)", tmp0.l, tmp0.l + X.w, A.bl);
			}else{
				dbg_printf("LDA $%06X,X (Target: $%06X, Value: $%04X)", tmp0.l, tmp0.l + X.w, A.w);
			}
		break;
		
		// Direct Page
		case OP_LDA_DP:
			tmp0.bl = fetch(CPU);
			A.bl = readDP(CPU, tmp0.bl);
			if (!MF) A.bh = readDP(CPU, tmp0.bl + 1);
			setNZ(CPU, MF, A.w);
			
			if (MF){
				dbg_printf("LDA $%02X (Target: $%06X, Value: $%02X)", tmp0.bl, addrDP(CPU, tmp0.bl), A.bl);
			}else{
				dbg_printf("LDA $%02X (Target: $%06X, Value: $%04X)", tmp0.bl, addrDP(CPU, tmp0.bl), A.w);
			}
		break;
		
		// Direct Page X
		case OP_LDA_DPX:
			tmp0.bl = fetch(CPU);
			A.bl = readDP(CPU, tmp0.bl + X.w);
			if (!MF) A.bh = readDP(CPU, tmp0.bl + X.w + 1);
			setNZ(CPU, MF, A.w);
			
			if (MF){
				dbg_printf("LDA $%02X,X (Target: $%06X, Value: $%02X)", tmp0.bl, addrDP(CPU, tmp0.bl + X.w), A.bl);
			}else{
				dbg_printf("LDA $%02X,X (Target: $%06X, Value: $%04X)", tmp0.bl, addrDP(CPU, tmp0.bl + X.w), A.w);
			}
		break;
		
		// Direct Page Indirect
		case OP_LDA_DPI:
			tmp0.bl = fetch(CPU);
			tmp1.bl = readDP(CPU, tmp0.bl);
			tmp1.bm = readDP(CPU, tmp0.bl + 1);
			A.bl = readAbs(CPU, tmp1.wl);
			if (!MF) A.bh = readAbs(CPU, tmp1.wl + 1);
			setNZ(CPU, MF, A.w);
			
			if (MF){
				dbg_printf("LDA ($%02X) (Target: $%06X, Value: $%02X)", tmp0.bl, addrAbs(CPU, tmp1.wl), A.bl);
			}else{
				dbg_printf("LDA ($%02X) (Target: $%06X, Value: $%04X)", tmp0.bl, addrAbs(CPU, tmp1.wl), A.w);
			}
		break;
		
		// Direct Page X Indirect
		case OP_LDA_DPXI:
			tmp0.bl = fetch(CPU);
			tmp1.bl = readDP(CPU, tmp0.bl + X.w);
			tmp1.bm = readDP(CPU, tmp0.bl + X.w + 1);
			A.bl = readAbs(CPU, tmp1.wl);
			if (!MF) A.bh = readAbs(CPU, tmp1.wl + 1);
			setNZ(CPU, MF, A.w);
			
			if (MF){
				dbg_printf("LDA ($%02X,X) (Target: $%06X, Value: $%02X)", tmp0.bl, addrAbs(CPU, tmp1.wl), A.bl);
			}else{
				dbg_printf("LDA ($%02X,X) (Target: $%06X, Value: $%04X)", tmp0.bl, addrAbs(CPU, tmp1.wl), A.w);
			}
		break;
		
		// Direct Page Indirect Y
		case OP_LDA_DPIY:
			tmp0.bl = fetch(CPU);
			tmp1.bl = readDP(CPU, tmp0.bl);
			tmp1.bm = readDP(CPU, tmp0.bl + 1);
			A.bl = readAbs(CPU, tmp1.wl + Y.w);
			if (!MF) A.bh = readAbs(CPU, tmp1.wl + Y.w + 1);
			setNZ(CPU, MF, A.w);
			
			if (MF){
				dbg_printf("LDA ($%02X),Y (Target: $%06X, Value: $%02X)", tmp0.bl, addrAbs(CPU, tmp1.wl + Y.w), A.bl);
			}else{
				dbg_printf("LDA ($%02X),Y (Target: $%06X, Value: $%04X)", tmp0.bl, addrAbs(CPU, tmp1.wl + Y.w), A.w);
			}
		break;
		
		// Direct Page Indirect Long
		case OP_LDA_DPIL:
			tmp0.bl = fetch(CPU);
			tmp1.bl = readDP(CPU, tmp0.bl);
			tmp1.bm = readDP(CPU, tmp0.bl + 1);
			tmp1.bh = readDP(CPU, tmp0.bl + 2);
			tmp1.bx = 0;
			A.bl = readMem(CPU, tmp1.l);
			if (!MF) A.bh = readMem(CPU, tmp1.l + 1);
			setNZ(CPU, MF, A.w);
			
			if (MF){
				dbg_printf("LDA [$%02X] (Target: $%06X, Value: $%02X)", tmp0.bl, tmp1.l, A.bl);
			}else{
				dbg_printf("LDA [$%02X] (Target: $%06X, Value: $%04X)", tmp0.bl, tmp1.l, A.w);
			}
		break;
		
		// Direct Page Indirect Long Y
		case OP_LDA_DPILY:
			tmp0.bl = fetch(CPU);
			tmp1.bl = readDP(CPU, tmp0.bl);
			tmp1.bm = readDP(CPU, tmp0.bl + 1);
			tmp1.bh = readDP(CPU, tmp0.bl + 2);
			tmp1.bx = 0;
			A.bl = readMem(CPU, tmp1.l + Y.w);
			if (!MF) A.bh = readMem(CPU, tmp1.l + Y.w + 1);
			setNZ(CPU, MF, A.w);
			
			if (MF){
				dbg_printf("LDA [$%02X],Y (Target: $%06X, Value: $%02X)", tmp0.bl, tmp1.l + Y.w, A.bl);
			}else{
				dbg_printf("LDA [$%02X],Y (Target: $%06X, Value: $%04X)", tmp0.bl, tmp1.l + Y.w, A.w);
			}
		break;
		
		// Stack
		case OP_LDA_S:
			tmp2.bl = EF;			// Save the E Flag
			tmp0.bl = fetch(CPU);
			setE(false);			// Then Clear it
			A.bl = readStack(CPU, tmp0.bl);
			if (!MF) A.bh = readStack(CPU, tmp0.bl + 1);
			setE(tmp2.bl);		// And afterwards restore it again
			setNZ(CPU, MF, A.w);
			
			if (MF){
				dbg_printf("LDA %u,S (Target: $%06X, Value: $%02X)", tmp0.bl, addrStack(CPU, tmp0.bl), A.bl);
			}else{
				dbg_printf("LDA %u,S (Target: $%06X, Value: $%04X)", tmp0.bl, addrStack(CPU, tmp0.bl), A.w);
			}
		break;
		
		// Stack Indirect Y
		case OP_LDA_SIY:
			tmp2.bl = EF;			// Save the E Flag
			tmp0.bl = fetch(CPU);
			setE(false);			// Then Clear it
			tmp1.bl = readStack(CPU, tmp0.bl);
			tmp1.bm = readStack(CPU, tmp0.bl + 1);
			setE(tmp2.bl);		// And afterwards restore it again
			A.bl = readAbs(CPU, tmp1.wl + Y.w);
			if (!MF) A.bh = readAbs(CPU, tmp1.wl + Y.w + 1);
			setNZ(CPU, MF, A.w);
			
			if (MF){
				dbg_printf("LDA (%u,S),Y (Target: $%06X, Value: $%02X)", tmp0.bl, addrAbs(CPU, tmp1.wl + Y.w), A.bl);
			}else{
				dbg_printf("LDA (%u,S),Y (Target: $%06X, Value: $%04X)", tmp0.bl, addrAbs(CPU, tmp1.wl + Y.w), A.w);
			}
		break;
		
		
		// Store
		// Absolute
		case OP_STA_A:
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			writeAbs(CPU, tmp0.wl, A.bl);
			if (!MF) writeAbs(CPU, tmp0.wl + 1, A.bh);
			
			if (MF){
				dbg_printf("STA $%04X (Target: $%06X, Value: $%02X)", tmp0.wl, addrAbs(CPU, tmp0.wl), A.bl);
			}else{
				dbg_printf("STA $%04X (Target: $%06X, Value: $%04X)", tmp0.wl, addrAbs(CPU, tmp0.wl), A.w);
			}
		break;
		
		// Absolute X
		case OP_STA_AX:
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			writeAbs(CPU, tmp0.wl + X.w, A.bl);
			if (!MF) writeAbs(CPU, tmp0.wl + X.w + 1, A.bh);
			
			if (MF){
				dbg_printf("STA $%04X,X (Target: $%06X, Value: $%02X)", tmp0.wl, addrAbs(CPU, tmp0.wl + X.w), A.bl);
			}else{
				dbg_printf("STA $%04X,X (Target: $%06X, Value: $%04X)", tmp0.wl, addrAbs(CPU, tmp0.wl + X.w), A.w);
			}
		break;
		
		// Absolute Y
		case OP_STA_AY:
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			writeAbs(CPU, tmp0.wl + Y.w, A.bl);
			if (!MF) writeAbs(CPU, tmp0.wl + Y.w + 1, A.bh);
			
			if (MF){
				dbg_printf("STA $%04X,Y (Target: $%06X, Value: $%02X)", tmp0.wl, addrAbs(CPU, tmp0.wl + Y.w), A.bl);
			}else{
				dbg_printf("STA $%04X,Y (Target: $%06X, Value: $%04X)", tmp0.wl, addrAbs(CPU, tmp0.wl + Y.w), A.w);
			}
		break;
		
		// Absolute Long
		case OP_STA_AL:
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			tmp0.bh = fetch(CPU);
			tmp0.bx = 0;
			writeMem(CPU, tmp0.l, A.bl);
			if (!MF) writeMem(CPU, tmp0.l + 1, A.bh);
			
			if (MF){
				dbg_printf("STA $%06X (Value: $%02X)", tmp0.l, A.bl);
			}else{
				dbg_printf("STA $%06X (Value: $%04X)", tmp0.l, A.w);
			}
		break;
		
		// Absolute Long X
		case OP_STA_ALX:
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			tmp0.bh = fetch(CPU);
			tmp0.bx = 0;
			writeMem(CPU, tmp0.l + X.w, A.bl);
			if (!MF) writeMem(CPU, tmp0.l + X.w + 1, A.bh);
			
			if (MF){
				dbg_printf("STA $%06X,X (Target: $%06X, Value: $%02X)", tmp0.l, tmp0.l + X.w, A.bl);
			}else{
				dbg_printf("STA $%06X,X (Target: $%06X, Value: $%04X)", tmp0.l, tmp0.l + X.w, A.w);
			}
		break;
		
		// Direct Page
		case OP_STA_DP:
			tmp0.bl = fetch(CPU);
			writeDP(CPU, tmp0.bl, A.bl);
			if (!MF) writeDP(CPU, tmp0.bl + 1, A.bh);
			
			if (MF){
				dbg_printf("STA $%02X (Target: $%06X, Value: $%02X)", tmp0.bl, addrDP(CPU, tmp0.bl), A.bl);
			}else{
				dbg_printf("STA $%02X (Target: $%06X, Value: $%04X)", tmp0.bl, addrDP(CPU, tmp0.bl), A.w);
			}
		break;
		
		// Direct Page X
		case OP_STA_DPX:
			tmp0.bl = fetch(CPU);
			writeDP(CPU, tmp0.bl + X.w, A.bl);
			if (!MF) writeDP(CPU, tmp0.bl + X.w + 1, A.bh);
			
			if (MF){
				dbg_printf("STA $%02X,X (Target: $%06X, Value: $%02X)", tmp0.bl, addrDP(CPU, tmp0.bl + X.w), A.bl);
			}else{
				dbg_printf("STA $%02X,X (Target: $%06X, Value: $%04X)", tmp0.bl, addrDP(CPU, tmp0.bl + X.w), A.w);
			}
		break;
		
		// Direct Page Indirect
		case OP_STA_DPI:
			tmp0.bl = fetch(CPU);
			tmp1.bl = readDP(CPU, tmp0.bl);
			tmp1.bm = readDP(CPU, tmp0.bl + 1);
			writeAbs(CPU, tmp1.wl, A.bl);
			if (!MF) writeAbs(CPU, tmp1.wl + 1, A.bh);
			
			if (MF){
				dbg_printf("STA ($%02X) (Target: $%06X, Value: $%02X)", tmp0.bl, addrAbs(CPU, tmp1.wl), A.bl);
			}else{
				dbg_printf("STA ($%02X) (Target: $%06X, Value: $%04X)", tmp0.bl, addrAbs(CPU, tmp1.wl), A.w);
			}
		break;
		
		// Direct Page X Indirect
		case OP_STA_DPXI:
			tmp0.bl = fetch(CPU);
			tmp1.bl = readDP(CPU, tmp0.bl + X.w);
			tmp1.bm = readDP(CPU, tmp0.bl + X.w + 1);
			writeAbs(CPU, tmp1.wl, A.bl);
			if (!MF) writeAbs(CPU, tmp1.wl + 1, A.bh);
			
			if (MF){
				dbg_printf("STA ($%02X,X) (Target: $%06X, Value: $%02X)", tmp0.bl, addrAbs(CPU, tmp1.wl), A.bl);
			}else{
				dbg_printf("STA ($%02X,X) (Target: $%06X, Value: $%04X)", tmp0.bl, addrAbs(CPU, tmp1.wl), A.w);
			}
		break;
		
		// Direct Page Indirect Y
		case OP_STA_DPIY:
			tmp0.bl = fetch(CPU);
			tmp1.bl = readDP(CPU, tmp0.bl);
			tmp1.bm = readDP(CPU, tmp0.bl + 1);
			writeAbs(CPU, tmp1.wl + Y.w, A.bl);
			if (!MF) writeAbs(CPU, tmp1.wl + Y.w + 1, A.bh);
			
			if (MF){
				dbg_printf("STA ($%02X),Y (Target: $%06X, Value: $%02X)", tmp0.bl, addrAbs(CPU, tmp1.wl + Y.w), A.bl);
			}else{
				dbg_printf("STA ($%02X),Y (Target: $%06X, Value: $%04X)", tmp0.bl, addrAbs(CPU, tmp1.wl + Y.w), A.w);
			}
		break;
		
		// Direct Page Indirect Long
		case OP_STA_DPIL:
			tmp0.bl = fetch(CPU);
			tmp1.bl = readDP(CPU, tmp0.bl);
			tmp1.bm = readDP(CPU, tmp0.bl + 1);
			tmp1.bh = readDP(CPU, tmp0.bl + 2);
			tmp1.bx = 0;
			writeMem(CPU, tmp1.l, A.bl);
			if (!MF) writeMem(CPU, tmp1.l + 1, A.bh);
			
			if (MF){
				dbg_printf("STA [$%02X] (Target: $%06X, Value: $%02X)", tmp0.bl, tmp1.l, A.bl);
			}else{
				dbg_printf("STA [$%02X] (Target: $%06X, Value: $%04X)", tmp0.bl, tmp1.l, A.w);
			}
		break;
		
		// Direct Page Indirect Long Y
		case OP_STA_DPILY:
			tmp0.bl = fetch(CPU);
			tmp1.bl = readDP(CPU, tmp0.bl);
			tmp1.bm = readDP(CPU, tmp0.bl + 1);
			tmp1.bh = readDP(CPU, tmp0.bl + 2);
			tmp1.bx = 0;
			writeMem(CPU, tmp1.l + Y.w, A.bl);
			if (!MF) writeMem(CPU, tmp1.l + Y.w + 1, A.bh);
			
			if (MF){
				dbg_printf("STA [$%02X],Y (Target: $%06X, Value: $%02X)", tmp0.bl, tmp1.l + Y.w, A.bl);
			}else{
				dbg_printf("STA [$%02X],Y (Target: $%06X, Value: $%04X)", tmp0.bl, tmp1.l + Y.w, A.w);
			}
		break;
		
		// Stack
		case OP_STA_S:
			tmp2.bl = EF;			// Save the E Flag
			setE(false);			// Then Clear it
			tmp0.bl = fetch(CPU);
			writeStack(CPU, tmp0.bl, A.bl);
			if (!MF) writeStack(CPU, tmp0.bl + 1, A.bh);
			setE(tmp2.bl);		// And afterwards restore it again
			
			if (MF){
				dbg_printf("STA %u,S (Target: $%06X, Value: $%02X)", tmp0.bl, addrStack(CPU, tmp0.bl), A.bl);
			}else{
				dbg_printf("STA %u,S (Target: $%06X, Value: $%04X)", tmp0.bl, addrStack(CPU, tmp0.bl), A.w);
			}
		break;
		
		// Stack Indirect Y
		case OP_STA_SIY:
			tmp2.bl = EF;			// Save the E Flag
			tmp0.bl = fetch(CPU);
			setE(false);			// Then Clear it
			tmp1.bl = readStack(CPU, tmp0.bl);
			tmp1.bm = readStack(CPU, tmp0.bl + 1);
			setE(tmp2.bl);		// And afterwards restore it again
			writeAbs(CPU, tmp1.wl + Y.w, A.bl);
			if (!MF) writeAbs(CPU, tmp1.wl + Y.w + 1, A.bh);
			
			if (MF){
				dbg_printf("STA (%u,S),Y (Target: $%06X, Value: $%02X)", tmp0.bl, tmp1.wl + Y.w, A.bl);
			}else{
				dbg_printf("STA (%u,S),Y (Target: $%06X, Value: $%04X)", tmp0.bl, tmp1.wl + Y.w, A.w);
			}
		break;
		
		// Load/Store X ---------------------------------------------------------- //
		// Immediate
		case OP_LDX_IM:
			X.bl = fetch(CPU);
			if (!XF) X.bh = fetch(CPU);
			setNZ(CPU, XF, X.w);
			
			if (XF){
				dbg_printf("LDX #$%02X", X.bl);
			}else{
				dbg_printf("LDX #$%04X", X.w);
			}
		break;
		
		// Absolute
		case OP_LDX_A:
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			X.bl = readAbs(CPU, tmp0.wl);
			if (!XF) X.bh = readAbs(CPU, tmp0.wl + 1);
			setNZ(CPU, XF, X.w);
			
			if (XF){
				dbg_printf("LDX $%04X (Target: $%06X, Value: $%02X)", tmp0.wl, addrAbs(CPU, tmp0.wl), X.bl);
			}else{
				dbg_printf("LDX $%04X (Target: $%06X, Value: $%04X)", tmp0.wl, addrAbs(CPU, tmp0.wl), X.w);
			}
		break;
		
		// Absolute Y
		case OP_LDX_AY:
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			X.bl = readAbs(CPU, tmp0.wl + Y.w);
			if (!XF) X.bh = readAbs(CPU, tmp0.wl + Y.w + 1);
			setNZ(CPU, XF, X.w);
			
			if (XF){
				dbg_printf("LDX $%04X,Y (Target: $%06X, Value: $%02X)", tmp0.wl, addrAbs(CPU, tmp0.wl + Y.w), X.bl);
			}else{
				dbg_printf("LDX $%04X,Y (Target: $%06X, Value: $%04X)", tmp0.wl, addrAbs(CPU, tmp0.wl + Y.w), X.w);
			}
		break;
		
		// Direct Page
		case OP_LDX_DP:
			tmp0.bl = fetch(CPU);
			X.bl = readDP(CPU, tmp0.bl);
			if (!XF) X.bh = readDP(CPU, tmp0.bl + 1);
			setNZ(CPU, XF, X.w);
			
			if (XF){
				dbg_printf("LDX $%02X (Target: $%06X, Value: $%02X)", tmp0.bl, addrDP(CPU, tmp0.bl), X.bl);
			}else{
				dbg_printf("LDX $%02X (Target: $%06X, Value: $%04X)", tmp0.bl, addrDP(CPU, tmp0.bl), X.w);
			}
		break;
		
		// Direct Page Y
		case OP_LDX_DPY:
			tmp0.bl = fetch(CPU);
			X.bl = readDP(CPU, tmp0.bl + Y.w);
			if (!XF) X.bh = readDP(CPU, tmp0.bl + Y.w + 1);
			setNZ(CPU, XF, X.w);
			
			if (XF){
				dbg_printf("LDX $%02X,Y (Target: $%06X, Value: $%02X)", tmp0.bl, addrDP(CPU, tmp0.bl + Y.w), X.bl);
			}else{
				dbg_printf("LDX $%02X,Y (Target: $%06X, Value: $%04X)", tmp0.bl, addrDP(CPU, tmp0.bl + Y.w), X.w);
			}
		break;
		
		// Absolute
		case OP_STX_A:
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			writeAbs(CPU, tmp0.wl, X.bl);
			if (!XF) writeAbs(CPU, tmp0.wl + 1, X.bh);
			
			if (XF){
				dbg_printf("STX $%04X (Target: $%06X, Value: $%02X)", tmp0.wl, addrAbs(CPU, tmp0.wl), X.bl);
			}else{
				dbg_printf("STX $%04X (Target: $%06X, Value: $%04X)", tmp0.wl, addrAbs(CPU, tmp0.wl), X.w);
			}
		break;
		
		// Direct Page
		case OP_STX_DP:
			tmp0.bl = fetch(CPU);
			writeDP(CPU, tmp0.bl, X.bl);
			if (!XF) writeDP(CPU, tmp0.bl + 1, X.bh);
			
			if (XF){
				dbg_printf("STX $%02X (Target: $%06X, Value: $%02X)", tmp0.bl, addrDP(CPU, tmp0.bl), X.bl);
			}else{
				dbg_printf("STX $%02X (Target: $%06X, Value: $%04X)", tmp0.bl, addrDP(CPU, tmp0.bl), X.w);
			}
		break;
		
		// Direct Page Y
		case OP_STX_DPY:
			tmp0.bl = fetch(CPU);
			writeDP(CPU, tmp0.bl + Y.w, X.bl);
			if (!XF) writeDP(CPU, tmp0.bl + Y.w + 1, X.bh);
			
			if (XF){
				dbg_printf("STX $%02X,Y (Target: $%06X, Value: $%02X)", tmp0.bl, addrDP(CPU, tmp0.bl + Y.w), X.bl);
			}else{
				dbg_printf("STX $%02X,Y (Target: $%06X, Value: $%04X)", tmp0.bl, addrDP(CPU, tmp0.bl + Y.w), X.w);
			}
		break;
		
		// Load/Store Y ---------------------------------------------------------- //
		// Immediate
		case OP_LDY_IM:
			Y.bl = fetch(CPU);
			if (!XF) Y.bh = fetch(CPU);
			setNZ(CPU, XF, Y.w);
			
			if (XF){
				dbg_printf("LDY #$%02X", Y.bl);
			}else{
				dbg_printf("LDY #$%04X", Y.w);
			}
		break;
		
		// Absolute
		case OP_LDY_A:
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			Y.bl = readAbs(CPU, tmp0.wl);
			if (!XF) Y.bh = readAbs(CPU, tmp0.wl + 1);
			setNZ(CPU, XF, Y.w);
			
			if (XF){
				dbg_printf("LDY $%04X (Target: $%06X, Value: $%02X)", tmp0.wl, addrAbs(CPU, tmp0.wl), Y.bl);
			}else{
				dbg_printf("LDY $%04X (Target: $%06X, Value: $%04X)", tmp0.wl, addrAbs(CPU, tmp0.wl), Y.w);
			}
		break;
		
		// Absolute X
		case OP_LDY_AX:
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			Y.bl = readAbs(CPU, tmp0.wl + X.w);
			if (!XF) Y.bh = readAbs(CPU, tmp0.wl + X.w + 1);
			setNZ(CPU, XF, Y.w);
			
			if (XF){
				dbg_printf("LDY $%04X,X (Target: $%06X, Value: $%02X)", tmp0.wl, addrAbs(CPU, tmp0.wl + X.w), Y.bl);
			}else{
				dbg_printf("LDY $%04X,X (Target: $%06X, Value: $%04X)", tmp0.wl, addrAbs(CPU, tmp0.wl + X.w), Y.w);
			}
		break;
		
		// Direct Page
		case OP_LDY_DP:
			tmp0.bl = fetch(CPU);
			Y.bl = readDP(CPU, tmp0.bl);
			if (!XF) Y.bh = readDP(CPU, tmp0.bl + 1);
			setNZ(CPU, XF, Y.w);
			
			if (XF){
				dbg_printf("LDY $%02X (Target: $%06X, Value: $%02X)", tmp0.bl, addrDP(CPU, tmp0.bl), Y.bl);
			}else{
				dbg_printf("LDY $%02X (Target: $%06X, Value: $%04X)", tmp0.bl, addrDP(CPU, tmp0.bl), Y.w);
			}
		break;
		
		// Direct Page X
		case OP_LDY_DPX:
			tmp0.bl = fetch(CPU);
			Y.bl = readDP(CPU, tmp0.bl + X.w);
			if (!XF) Y.bh = readDP(CPU, tmp0.bl + X.w + 1);
			setNZ(CPU, XF, Y.w);
			
			if (XF){
				dbg_printf("LDY $%02X,X (Target: $%06X, Value: $%02X)", tmp0.bl, addrDP(CPU, tmp0.bl + X.w), Y.bl);
			}else{
				dbg_printf("LDY $%02X,X (Target: $%06X, Value: $%04X)", tmp0.bl, addrDP(CPU, tmp0.bl + X.w), Y.w);
			}
		break;
		
		// Absolute
		case OP_STY_A:
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			writeAbs(CPU, tmp0.wl, Y.bl);
			if (!XF) writeAbs(CPU, tmp0.wl + 1, Y.bh);
			
			if (XF){
				dbg_printf("STY $%04X (Target: $%06X, Value: $%02X)", tmp0.wl, addrAbs(CPU, tmp0.wl), Y.bl);
			}else{
				dbg_printf("STY $%04X (Target: $%06X, Value: $%04X)", tmp0.wl, addrAbs(CPU, tmp0.wl), Y.w);
			}
		break;
		
		// Direct Page
		case OP_STY_DP:
			tmp0.bl = fetch(CPU);
			writeDP(CPU, tmp0.bl, Y.bl);
			if (!XF) writeDP(CPU, tmp0.bl + 1, Y.bh);
			
			if (XF){
				dbg_printf("STY $%02X (Target: $%06X, Value: $%02X)", tmp0.bl, addrDP(CPU, tmp0.bl), Y.bl);
			}else{
				dbg_printf("STY $%02X (Target: $%06X, Value: $%04X)", tmp0.bl, addrDP(CPU, tmp0.bl), Y.w);
			}
		break;
		
		// Direct Page X
		case OP_STY_DPX:
			tmp0.bl = fetch(CPU);
			writeDP(CPU, tmp0.bl + X.w, Y.bl);
			if (!XF) writeDP(CPU, tmp0.bl + X.w + 1, Y.bh);
			
			if (XF){
				dbg_printf("STY $%02X,X (Target: $%06X, Value: $%02X)", tmp0.bl, addrDP(CPU, tmp0.bl + X.w), Y.bl);
			}else{
				dbg_printf("STY $%02X,X (Target: $%06X, Value: $%04X)", tmp0.bl, addrDP(CPU, tmp0.bl + X.w), Y.w);
			}
		break;
		
		// Store Zero ------------------------------------------------------------ //
		// Absolute
		case OP_STZ_A:
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			writeAbs(CPU, tmp0.wl, 0);
			if (!MF) writeAbs(CPU, tmp0.wl + 1, 0);
			
			dbg_printf("STZ $%04X (Target: $%06X)", tmp0.wl, addrAbs(CPU, tmp0.wl));
		break;
		
		// Absolute X
		case OP_STZ_AX:
			tmp0.bl = fetch(CPU);
			tmp0.bm = fetch(CPU);
			writeAbs(CPU, tmp0.wl + X.w, 0);
			if (!MF) writeAbs(CPU, tmp0.wl + X.w + 1, 0);
			
			dbg_printf("STZ $%04X,X (Target: $%06X)", tmp0.wl, addrAbs(CPU, tmp0.wl + X.w));
		break;
		
		// Direct Page
		case OP_STZ_DP:
			tmp0.bl = fetch(CPU);
			writeDP(CPU, tmp0.bl, 0);
			if (!MF) writeDP(CPU, tmp0.bl + 1, 0);
			
			dbg_printf("STZ $%02X (Target: $%06X)", tmp0.bl, addrDP(CPU, tmp0.bl));
		break;
		
		// Direct Page X
		case OP_STZ_DPX:
			tmp0.bl = fetch(CPU);
			writeDP(CPU, tmp0.bl + X.w, 0);
			if (!MF) writeDP(CPU, tmp0.bl + X.w + 1, 0);
			
			dbg_printf("STZ $%02X,X (Target: $%06X)", tmp0.bl, addrDP(CPU, tmp0.bl + X.w));
		break;
		
		//Transfers -------------------------------------------------------------- //
		case OP_TAX:
			if (XF){
				X.bl = A.bl;
			}else{
				X.w = A.w;
			}
			setNZ(CPU, XF, X.w);
			
			dbg_printf("TAX");
		break;
		
		case OP_TXA:
			if (MF){
				A.bl = X.bl;
			}else{
				A.w = X.w;
			}
			setNZ(CPU, MF, A.w);
			
			dbg_printf("TXA");
		break;
		
		case OP_TAY:
			if (XF){
				Y.bl = A.bl;
			}else{
				Y.w = A.w;
			}
			setNZ(CPU, XF, Y.w);
			
			dbg_printf("TAY");
		break;
		
		case OP_TYA:
			if (MF){
				A.bl = Y.bl;
			}else{
				A.w = Y.w;
			}
			setNZ(CPU, MF, A.w);
			
			dbg_printf("TYA");
		break;
		
		case OP_TSX:
			if (XF){
				X.bl = SP.bl;
			}else{
				X.w = SP.w;
			}
			setNZ(CPU, XF, X.w);
			
			dbg_printf("TSX");
		break;
		
		case OP_TXS:
			SP.w = X.w;
			
			dbg_printf("TXS");
		break;
		
		case OP_TYX:
			if (XF){
				X.bl = Y.bl;
			}else{
				X.w = Y.w;
			}
			setNZ(CPU, XF, X.w);
			
			dbg_printf("TYX");
		break;
		
		case OP_TXY:
			if (XF){
				Y.bl = X.bl;
			}else{
				Y.w = X.w;
			}
			setNZ(CPU, XF, Y.w);
			
			dbg_printf("TXY");
		break;
		
		case OP_TCD:
			DP.w = A.w;
			setNZ(CPU, false, A.w);
			
			dbg_printf("TCD");
		break;
		
		case OP_TDC:
			A.w = DP.w;
			setNZ(CPU, false, A.w);
			
			dbg_printf("TDC");
		break;
		
		case OP_TCS:
			SP.w = A.w;
			
			dbg_printf("TCS");
		break;
		
		case OP_TSC:
			A.w = SP.w;
			setNZ(CPU, false, A.w);
			
			dbg_printf("TSC");
		break;
		
		case OP_XBA:
			tmp0.bl = A.bl;
			A.bl = A.bh;
			A.bh = tmp0.bl;
			setNZ(CPU, true, A.w);
			
			dbg_printf("XBA");
		break;
		
		// Misc. ----------------------------------------------------------------- //
		// NOP
		case OP_NOP:
			dbg_printf("NOP");
			// Literally nothing!
		break;
		
		// Wait for Interrupt
		case OP_WAI:
			dbg_printf("WAI");
			CPU->wai = true;
			return cycleRem;
		break;
		
		// Stop CPU
		case OP_STP:
			dbg_printf("STP");
			CPU->stp = true;
			return 0;
		break;
		
		// Expansion?
		case OP_WDM:
			dbg_printf("WDM");
			fetch(CPU);		// Currently does nothing
		break;
		
		// Move Bytes (Positive)
		case OP_MVP_IM:
			
		break;
		
		// Move Bytes (Negative)
		case OP_MVN_IM:
			
		break;
		
		// Software Interrupt (Break)
		case OP_BRK_IM:
			tmp0.bl = fetch(CPU);
			dbg_printf("BRK #$%02X", tmp0.bl);
			if (EF){		// Emulation
				pushStack(CPU, PC.bh);
				pushStack(CPU, PC.bl);
				pushStack(CPU, readSR(CPU) | SR_BRK);
				setD(false);
				setI(true);
				PC.bl = readMem(CPU, VECT_E_IRQ);
				PC.bh = readMem(CPU, VECT_E_IRQ + 1);
				PB = 0;
			}else{			// Native
				pushStack(CPU, PB);
				pushStack(CPU, PC.bh);
				pushStack(CPU, PC.bl);
				pushStack(CPU, readSR(CPU));
				setD(false);
				setI(true);
				PC.bl = readMem(CPU, VECT_N_BRK);
				PC.bh = readMem(CPU, VECT_N_BRK + 1);
				PB = 0;
			}
		break;
		
		// Software Interrupt (Co-Processor)
		case OP_COP_IM:
			tmp0.bl = fetch(CPU);
			dbg_printf("COP #$%02X", tmp0.bl);
			if (EF){		// Emulation
				pushStack(CPU, PC.bh);
				pushStack(CPU, PC.bl);
				pushStack(CPU, readSR(CPU) | SR_BRK);
				setD(false);
				setI(true);
				PC.bl = readMem(CPU, VECT_E_COP);
				PC.bh = readMem(CPU, VECT_E_COP + 1);
				PB = 0;
			}else{			// Native
				pushStack(CPU, PB);
				pushStack(CPU, PC.bh);
				pushStack(CPU, PC.bl);
				pushStack(CPU, readSR(CPU));
				setD(false);
				setI(true);
				PC.bl = readMem(CPU, VECT_N_COP);
				PC.bh = readMem(CPU, VECT_N_COP + 1);
				PB = 0;
			}
		break;
		
		// Bit Tests ------------------------------------------------------------- //
		// Direct Immediate
		case OP_BIT_IM:
			if (MF){
				tmp0.bl = fetch(CPU);
				setZ(!(tmp0.bl & A.bl));
				
				dbg_printf("BIT #$%02X", tmp0.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				setZ(!(tmp0.wl & A.w));
				
				dbg_printf("BIT #$%04X", tmp0.wl);
			}
		break;
		
		// Absolute
		case OP_BIT_A:
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp1.bl = readAbs(CPU, tmp0.wl);
				setN(tmp1.bl & 0x80);
				setV(tmp1.bl & 0x40);
				setZ(!(tmp1.bl & A.bl));
				
				dbg_printf("BIT $%04X (Target: $%06X, Value: $%02X)", tmp0.wl, addrAbs(CPU, tmp0.wl), tmp1.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp1.bl = readAbs(CPU, tmp0.wl);
				tmp1.bm = readAbs(CPU, tmp0.wl + 1);
				setN(tmp1.wl & 0x8000);
				setV(tmp1.wl & 0x4000);
				setZ(!(tmp1.wl & A.w));
				
				dbg_printf("BIT $%04X (Target: $%06X, Value: $%04X)", tmp0.wl, addrAbs(CPU, tmp0.wl), tmp1.wl);
			}
		break;
		
		// Absolute X
		case OP_BIT_AX:
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp1.bl = readAbs(CPU, tmp0.wl + X.w);
				setN(tmp1.bl & 0x80);
				setV(tmp1.bl & 0x40);
				setZ(!(tmp1.bl & A.bl));
				
				dbg_printf("BIT $%04X,X (Target: $%06X, Value: $%02X)", tmp0.wl, addrAbs(CPU, tmp0.wl + X.w), tmp1.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp1.bl = readAbs(CPU, tmp0.wl + X.w);
				tmp1.bm = readAbs(CPU, tmp0.wl + X.w + 1);
				setN(tmp1.wl & 0x8000);
				setV(tmp1.wl & 0x4000);
				setZ(!(tmp1.wl & A.w));
				
				dbg_printf("BIT $%04X,X (Target: $%06X, Value: $%04X)", tmp0.wl, addrAbs(CPU, tmp0.wl + X.w), tmp1.wl);
			}
		break;
		
		// Direct Page
		case OP_BIT_DP:
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl);
				setN(tmp1.bl & 0x80);
				setV(tmp1.bl & 0x40);
				setZ(!(tmp1.bl & A.bl));
				
				dbg_printf("BIT $%02X (Target: $%06X, Value: $%02X)", tmp0.bl, addrDP(CPU, tmp0.bl), tmp1.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl);
				tmp1.bm = readDP(CPU, tmp0.bl + 1);
				setN(tmp1.wl & 0x8000);
				setV(tmp1.wl & 0x4000);
				setZ(!(tmp1.wl & A.w));
				
				dbg_printf("BIT $%02X (Target: $%06X, Value: $%04X)", tmp0.bl, addrDP(CPU, tmp0.bl), tmp1.wl);
			}
		break;
		
		// Direct Page X
		case OP_BIT_DPX:
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl + X.w);
				setN(tmp1.bl & 0x80);
				setV(tmp1.bl & 0x40);
				setZ(!(tmp1.bl & A.bl));
				
				dbg_printf("BIT $%02X,X (Target: $%06X, Value: $%02X)", tmp0.bl, addrDP(CPU, tmp0.bl + X.w), tmp1.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl + X.w);
				tmp1.bm = readDP(CPU, tmp0.bl + X.w + 1);
				setN(tmp1.wl & 0x8000);
				setV(tmp1.wl & 0x4000);
				setZ(!(tmp1.wl & A.w));
				
				dbg_printf("BIT $%02X,X (Target: $%06X, Value: $%04X)", tmp0.bl, addrDP(CPU, tmp0.bl + X.w), tmp1.wl);
			}
		break;
		
		// Test and Reset Bits
		// Absolute
		case OP_TRB_A:
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp1.bl = readAbs(CPU, tmp0.wl);
				setZ(!(tmp1.bl & A.bl));
				tmp1.bl &= ~A.bl;
				writeAbs(CPU, tmp0.wl, tmp1.bl);
				
				dbg_printf("TRB $%04X (Target: $%06X, Value: $%02X)", tmp0.wl, addrAbs(CPU, tmp0.wl), tmp1.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp1.bl = readAbs(CPU, tmp0.wl);
				tmp1.bm = readAbs(CPU, tmp0.wl + 1);
				setZ(!(tmp1.wl & A.w));
				tmp1.wl &= ~A.w;
				writeAbs(CPU, tmp0.wl, tmp1.bl);
				writeAbs(CPU, tmp0.wl + 1, tmp1.bm);
				
				dbg_printf("TSB $%04X (Target: $%06X, Value: $%04X)", tmp0.wl, addrAbs(CPU, tmp0.wl), tmp1.wl);
			}
		break;
		
		// Direct Page
		case OP_TRB_DP:
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl);
				setZ(!(tmp1.bl & A.bl));
				tmp1.bl &= ~A.bl;
				writeDP(CPU, tmp0.bl, tmp1.bl);
				
				dbg_printf("TRB $%02X (Target: $%06X, Value: $%02X)", tmp0.bl, addrDP(CPU, tmp0.bl), tmp1.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl);
				tmp1.bm = readDP(CPU, tmp0.bl + 1);
				setZ(!(tmp1.wl & A.w));
				tmp1.wl &= ~A.w;
				writeDP(CPU, tmp0.bl, tmp1.bl);
				writeDP(CPU, tmp0.bl + 1, tmp1.bm);
				
				dbg_printf("TRB $%02X (Target: $%06X, Value: $%04X)", tmp0.bl, addrDP(CPU, tmp0.bl), tmp1.wl);
			}
		break;
		
		// Test and Set Bits
		// Absolute
		case OP_TSB_A:
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp1.bl = readAbs(CPU, tmp0.wl);
				setZ(!(tmp1.bl & A.bl));
				tmp1.bl |= A.bl;
				writeAbs(CPU, tmp0.wl, tmp1.bl);
				
				dbg_printf("TSB $%04X (Target: $%06X, Value: $%02X)", tmp0.wl, addrAbs(CPU, tmp0.wl), tmp1.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp1.bl = readAbs(CPU, tmp0.wl);
				tmp1.bm = readAbs(CPU, tmp0.wl + 1);
				setZ(!(tmp1.wl & A.w));
				tmp1.wl |= A.w;
				writeAbs(CPU, tmp0.wl, tmp1.bl);
				writeAbs(CPU, tmp0.wl + 1, tmp1.bm);
				
				dbg_printf("TSB $%04X (Target: $%06X, Value: $%04X)", tmp0.wl, addrAbs(CPU, tmp0.wl), tmp1.wl);
			}
		break;
		
		// Direct Page
		case OP_TSB_DP:
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl);
				setZ(!(tmp1.bl & A.bl));
				tmp1.bl |= A.bl;
				writeDP(CPU, tmp0.bl, tmp1.bl);
				
				dbg_printf("TSB $%02X (Target: $%06X, Value: $%02X)", tmp0.bl, addrDP(CPU, tmp0.bl), tmp1.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl);
				tmp1.bm = readDP(CPU, tmp0.bl + 1);
				setZ(!(tmp1.wl & A.w));
				tmp1.wl |= A.w;
				writeDP(CPU, tmp0.bl, tmp1.bl);
				writeDP(CPU, tmp0.bl + 1, tmp1.bm);
				
				dbg_printf("TSB $%02X (Target: $%06X, Value: $%04X)", tmp0.bl, addrDP(CPU, tmp0.bl), tmp1.wl);
			}
		break;
		
		// Clear/Set Flags ------------------------------------------------------- //
		case OP_CLC:
			setC(false);
			
			dbg_printf("CLC");
		break;
		
		case OP_CLD:
			setD(false);
			
			dbg_printf("CLD");
		break;
		
		case OP_CLI:
			setI(false);
			
			dbg_printf("CLI");
		break;
		
		case OP_CLV:
			setV(false);
			
			dbg_printf("CLV");
		break;
		
		case OP_SEC:
			setC(true);
			
			dbg_printf("SEC");
		break;
		
		case OP_SED:
			setD(true);
			
			dbg_printf("SED");
		break;
		
		case OP_SEI:
			setI(true);
			
			dbg_printf("SEI");
		break;
		
		case OP_REP_IM:
			tmp0.bl = fetch(CPU);
			tmp1.bl = readSR(CPU) & ~tmp0.bl;
			if (EF){		// Emulation
				writeSR(CPU, tmp1.bl | SR_BRK);
			}else{			// Native
				writeSR(CPU, tmp1.bl);
			}
			
			
			dbg_printf("REP #%%%c%c%c%c%c%c%c%c", (tmp0.bl & 0x80) ? 'N' : '-', (tmp0.bl & 0x40) ? 'V' : '-', (tmp0.bl & 0x20) ? 'M' : '-', (tmp0.bl & 0x10) ? 'X' : '-', (tmp0.bl & 0x08) ? 'D' : '-', (tmp0.bl & 0x04) ? 'I' : '-', (tmp0.bl & 0x02) ? 'Z' : '-', (tmp0.bl & 0x01) ? 'C' : '-');
		break;
		
		case OP_SEP_IM:
			tmp0.bl = fetch(CPU);
			tmp1.bl = readSR(CPU) | tmp0.bl;
			if (EF){		// Emulation
				writeSR(CPU, tmp1.bl | SR_BRK);
			}else{			// Native
				writeSR(CPU, tmp1.bl);
			}
			
			dbg_printf("SEP #%%%c%c%c%c%c%c%c%c", (tmp0.bl & 0x80) ? 'N' : '-', (tmp0.bl & 0x40) ? 'V' : '-', (tmp0.bl & 0x20) ? 'M' : '-', (tmp0.bl & 0x10) ? 'X' : '-', (tmp0.bl & 0x08) ? 'D' : '-', (tmp0.bl & 0x04) ? 'I' : '-', (tmp0.bl & 0x02) ? 'Z' : '-', (tmp0.bl & 0x01) ? 'C' : '-');
		break;
		
		case OP_XCE:
			tmp0.bl = CF;
			setC(EF);
			setE(tmp0.bl);
			
			if (EF){		// If the mode was changed to Emulation
				X.bh = 0;
				Y.bh = 0;
				SP.bh = 1;
				setM(true);
				setX(true);
			}
			
			dbg_printf("XCE");
		break;
		
		// Type 1 ALU Operation (ADC, SBC, AND, ORA, XOR, CMP) ------------------- //
		// Immediate
		case OP_ADC_IM:
		case OP_SBC_IM:
		case OP_AND_IM:
		case OP_ORA_IM:
		case OP_XOR_IM:
		case OP_CMP_IM:
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp1.bm = 0;
				
				dbg_printf("%s #$%02X (A = $%02X)", g1ALUNames[aaa(opcode)], tmp0.bl, A.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				
				dbg_printf("%s #$%04X (A = $%02X)", g1ALUNames[aaa(opcode)], tmp0.wl, A.w);
			}
			g1ALU(CPU, tmp0.wl, aaa(opcode));
		break;
		
		// Absolute
		case OP_ADC_A:
		case OP_SBC_A:
		case OP_AND_A:
		case OP_ORA_A:
		case OP_XOR_A:
		case OP_CMP_A:
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp1.bl = readAbs(CPU, tmp0.wl);
				tmp1.bm = 0;
				
				dbg_printf("%s $%04X (Target: $%06X, Value: $%02X, A = $%02X)", g1ALUNames[aaa(opcode)], tmp0.wl, addrAbs(CPU, tmp0.wl), tmp1.bl, A.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp1.bl = readAbs(CPU, tmp0.wl);
				tmp1.bm = readAbs(CPU, tmp0.wl + 1);
				
				dbg_printf("%s $%04X (Target: $%06X, Value: $%04X, A = $%04X)", g1ALUNames[aaa(opcode)], tmp0.wl, addrAbs(CPU, tmp0.wl), tmp1.wl, A.w);
			}
			g1ALU(CPU, tmp1.wl, aaa(opcode));
		break;
		
		// Absolute X
		case OP_ADC_AX:
		case OP_SBC_AX:
		case OP_AND_AX:
		case OP_ORA_AX:
		case OP_XOR_AX:
		case OP_CMP_AX:
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp1.bl = readAbs(CPU, tmp0.wl + X.w);
				tmp1.bm = 0;
				
				dbg_printf("%s $%04X,X (Target: $%06X, Value: $%02X, A = $%02X)", g1ALUNames[aaa(opcode)], tmp0.wl, addrAbs(CPU, tmp0.wl + X.w), tmp1.bl, A.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp1.bl = readAbs(CPU, tmp0.wl + X.w);
				tmp1.bm = readAbs(CPU, tmp0.wl + X.w + 1);
				
				dbg_printf("%s $%04X,X (Target: $%06X, Value: $%04X, A = $%04X)", g1ALUNames[aaa(opcode)], tmp0.wl, addrAbs(CPU, tmp0.wl + X.w), tmp1.wl, A.w);
			}
			g1ALU(CPU, tmp1.wl, aaa(opcode));
		break;
		
		// Absolute Y
		case OP_ADC_AY:
		case OP_SBC_AY:
		case OP_AND_AY:
		case OP_ORA_AY:
		case OP_XOR_AY:
		case OP_CMP_AY:
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp1.bl = readAbs(CPU, tmp0.wl + Y.w);
				tmp1.bm = 0;
				
				dbg_printf("%s $%04X,Y (Target: $%06X, Value: $%02X, A = $%02X)", g1ALUNames[aaa(opcode)], tmp0.wl, addrAbs(CPU, tmp0.wl + Y.w), tmp1.bl, A.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp1.bl = readAbs(CPU, tmp0.wl + Y.w);
				tmp1.bm = readAbs(CPU, tmp0.wl + Y.w + 1);
				
				dbg_printf("%s $%04X,Y (Target: $%06X, Value: $%04X, A = $%04X)", g1ALUNames[aaa(opcode)], tmp0.wl, addrAbs(CPU, tmp0.wl + Y.w), tmp1.wl, A.w);
			}
			g1ALU(CPU, tmp1.wl, aaa(opcode));
		break;
		
		// Absolute Long
		case OP_ADC_AL:
		case OP_SBC_AL:
		case OP_AND_AL:
		case OP_ORA_AL:
		case OP_XOR_AL:
		case OP_CMP_AL:
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp0.bh = fetch(CPU);
				tmp0.bx = 0;
				tmp1.bl = readMem(CPU, tmp0.l);
				tmp1.bm = 0;
				
				dbg_printf("%s $%06X (Value: $%02X, A = $%02X)", g1ALUNames[aaa(opcode)], tmp0.l, tmp1.bl, A.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp0.bh = fetch(CPU);
				tmp0.bx = 0;
				tmp1.bl = readMem(CPU, tmp0.l);
				tmp1.bm = readMem(CPU, tmp0.l + 1);
				
				dbg_printf("%s $%06X (Value: $%04X, A = $%04X)", g1ALUNames[aaa(opcode)], tmp0.l, tmp1.wl, A.w);
			}
			g1ALU(CPU, tmp1.wl, aaa(opcode));
		break;
		
		// Absolute Long X
		case OP_ADC_ALX:
		case OP_SBC_ALX:
		case OP_AND_ALX:
		case OP_ORA_ALX:
		case OP_XOR_ALX:
		case OP_CMP_ALX:
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp0.bh = fetch(CPU);
				tmp0.bx = 0;
				tmp1.bl = readMem(CPU, tmp0.l + X.w);
				tmp1.bm = 0;
				
				dbg_printf("%s $%06X,X (Target: $%06X, Value: $%02X, A = $%02X)", g1ALUNames[aaa(opcode)], tmp0.l, tmp0.l + X.w, tmp1.bl, A.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp0.bh = fetch(CPU);
				tmp0.bx = 0;
				tmp1.bl = readMem(CPU, tmp0.l + X.w);
				tmp1.bm = readMem(CPU, tmp0.l + X.w + 1);
				
				dbg_printf("%s $%06X,X (Target: $%06X, Value: $%04X, A = $%04X)", g1ALUNames[aaa(opcode)], tmp0.l, tmp0.l + X.w, tmp1.wl, A.w);
			}
			g1ALU(CPU, tmp1.wl, aaa(opcode));
		break;
		
		// Direct Page
		case OP_ADC_DP:
		case OP_SBC_DP:
		case OP_AND_DP:
		case OP_ORA_DP:
		case OP_XOR_DP:
		case OP_CMP_DP:
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl);
				tmp1.bm = 0;
				
				dbg_printf("%s $%02X (Target: $%06X, Value: $%02X, A = $%02X)", g1ALUNames[aaa(opcode)], tmp0.bl, addrDP(CPU, tmp0.bl), tmp1.bl, A.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl);
				tmp1.bm = readDP(CPU, tmp0.bl + 1);
				
				dbg_printf("%s $%02X (Target: $%06X, Value: $%04X, A = $%04X)", g1ALUNames[aaa(opcode)], tmp0.bl, addrDP(CPU, tmp0.bl), tmp1.wl, A.w);
			}
			g1ALU(CPU, tmp1.wl, aaa(opcode));
		break;
		
		// Direct Page X
		case OP_ADC_DPX:
		case OP_SBC_DPX:
		case OP_AND_DPX:
		case OP_ORA_DPX:
		case OP_XOR_DPX:
		case OP_CMP_DPX:
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl + X.w);
				tmp1.bm = 0;
				
				dbg_printf("%s $%02X (Target: $%06X, Value: $%02X, A = $%02X)", g1ALUNames[aaa(opcode)], tmp0.bl, addrDP(CPU, tmp0.bl + X.w), tmp1.bl, A.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl + X.w);
				tmp1.bm = readDP(CPU, tmp0.bl + X.w + 1);
				
				dbg_printf("%s $%02X (Target: $%06X, Value: $%04X, A = $%04X)", g1ALUNames[aaa(opcode)], tmp0.bl, addrDP(CPU, tmp0.bl + X.w), tmp1.wl, A.w);
			}
			g1ALU(CPU, tmp1.wl, aaa(opcode));
		break;
		
		// Direct Page Indirect
		case OP_ADC_DPI:
		case OP_SBC_DPI:
		case OP_AND_DPI:
		case OP_ORA_DPI:
		case OP_XOR_DPI:
		case OP_CMP_DPI:
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl);
				tmp1.bm = readDP(CPU, tmp0.bl + 1);
				tmp2.bl = readAbs(CPU, tmp1.wl);
				tmp2.bm = 0;
				
				dbg_printf("%s ($%02X) (Target: $%06X, Value: $%02X, A = $%02X)", g1ALUNames[aaa(opcode)], tmp0.bl, addrAbs(CPU, tmp1.wl), tmp2.bl, A.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl);
				tmp1.bm = readDP(CPU, tmp0.bl + 1);
				tmp2.bl = readAbs(CPU, tmp1.wl);
				tmp2.bm = readAbs(CPU, tmp1.wl + 1);
				
				dbg_printf("%s ($%02X) (Target: $%06X, Value: $%04X, A = $%04X)", g1ALUNames[aaa(opcode)], tmp0.bl, addrAbs(CPU, tmp1.bl), tmp2.wl, A.w);
			}
			g1ALU(CPU, tmp2.wl, aaa(opcode));
		break;
		
		// Direct Page X Indirect
		case OP_ADC_DPXI:
		case OP_SBC_DPXI:
		case OP_AND_DPXI:
		case OP_ORA_DPXI:
		case OP_XOR_DPXI:
		case OP_CMP_DPXI:
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl + X.w);
				tmp1.bm = readDP(CPU, tmp0.bl + X.w + 1);
				tmp2.bl = readAbs(CPU, tmp1.wl);
				tmp2.bm = 0;
				
				dbg_printf("%s ($%02X,X) (Target: $%06X, Value: $%02X, A = $%02X)", g1ALUNames[aaa(opcode)], tmp0.bl, addrAbs(CPU, tmp1.wl), tmp2.bl, A.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl + X.w);
				tmp1.bm = readDP(CPU, tmp0.bl + X.w + 1);
				tmp2.bl = readAbs(CPU, tmp1.wl);
				tmp2.bm = readAbs(CPU, tmp1.wl + 1);
				
				dbg_printf("%s ($%02X,X) (Target: $%06X, Value: $%04X, A = $%04X)", g1ALUNames[aaa(opcode)], tmp0.bl, addrAbs(CPU, tmp1.bl), tmp2.wl, A.w);
			}
			g1ALU(CPU, tmp2.wl, aaa(opcode));
		break;
		
		// Direct Page Indirect Y
		case OP_ADC_DPIY:
		case OP_SBC_DPIY:
		case OP_AND_DPIY:
		case OP_ORA_DPIY:
		case OP_XOR_DPIY:
		case OP_CMP_DPIY:
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl);
				tmp1.bm = readDP(CPU, tmp0.bl + 1);
				tmp2.bl = readAbs(CPU, tmp1.wl + Y.w);
				tmp2.bm = 0;
				
				dbg_printf("%s ($%02X),Y (Target: $%06X, Value: $%02X, A = $%02X)", g1ALUNames[aaa(opcode)], tmp0.bl, addrAbs(CPU, tmp1.wl + Y.w), tmp2.bl, A.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl);
				tmp1.bm = readDP(CPU, tmp0.bl + 1);
				tmp2.bl = readAbs(CPU, tmp1.wl + Y.w);
				tmp2.bm = readAbs(CPU, tmp1.wl + Y.w + 1);
				
				dbg_printf("%s ($%02X),Y (Target: $%06X, Value: $%04X, A = $%04X)", g1ALUNames[aaa(opcode)], tmp0.bl, addrAbs(CPU, tmp1.bl + Y.w), tmp2.wl, A.w);
			}
			g1ALU(CPU, tmp2.wl, aaa(opcode));
		break;
		
		// Direct Page Indirect Long
		case OP_ADC_DPIL:
		case OP_SBC_DPIL:
		case OP_AND_DPIL:
		case OP_ORA_DPIL:
		case OP_XOR_DPIL:
		case OP_CMP_DPIL:
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl);
				tmp1.bm = readDP(CPU, tmp0.bl + 1);
				tmp1.bh = readDP(CPU, tmp0.bl + 2);
				tmp1.bx = 0;
				tmp2.bl = readMem(CPU, tmp1.l);
				tmp2.bm = 0;
				
				dbg_printf("%s [$%02X] (Target: $%06X, Value: $%02X, A = $%02X)", g1ALUNames[aaa(opcode)], tmp0.bl, tmp1.l, tmp2.bl, A.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl);
				tmp1.bm = readDP(CPU, tmp0.bl + 1);
				tmp1.bh = readDP(CPU, tmp0.bl + 2);
				tmp1.bx = 0;
				tmp2.bl = readMem(CPU, tmp1.l);
				tmp2.bm = readMem(CPU, tmp1.l + 1);
				
				dbg_printf("%s [$%02X] (Target: $%06X, Value: $%04X, A = $%04X)", g1ALUNames[aaa(opcode)], tmp0.bl, tmp1.l, tmp2.wl, A.w);
			}
			g1ALU(CPU, tmp2.wl, aaa(opcode));
		break;
		
		// Direct Page Indirect Long Y
		case OP_ADC_DPILY:
		case OP_SBC_DPILY:
		case OP_AND_DPILY:
		case OP_ORA_DPILY:
		case OP_XOR_DPILY:
		case OP_CMP_DPILY:
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl);
				tmp1.bm = readDP(CPU, tmp0.bl + 1);
				tmp1.bh = readDP(CPU, tmp0.bl + 2);
				tmp1.bx = 0;
				tmp2.bl = readMem(CPU, tmp1.l + Y.w);
				tmp2.bm = 0;
				
				dbg_printf("%s [$%02X],Y (Target: $%06X, Value: $%02X, A = $%02X)", g1ALUNames[aaa(opcode)], tmp0.bl, tmp1.l + Y.w, tmp2.bl, A.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl);
				tmp1.bm = readDP(CPU, tmp0.bl + 1);
				tmp1.bh = readDP(CPU, tmp0.bl + 2);
				tmp1.bx = 0;
				tmp2.bl = readMem(CPU, tmp1.l + Y.w);
				tmp2.bm = readMem(CPU, tmp1.l + Y.w + 1);
				
				dbg_printf("%s [$%02X],Y (Target: $%06X, Value: $%04X, A = $%04X)", g1ALUNames[aaa(opcode)], tmp0.bl, tmp1.l + Y.w, tmp2.wl, A.w);
			}
			g1ALU(CPU, tmp2.wl, aaa(opcode));
		break;
		
		// Stack
		case OP_ADC_S:
		case OP_SBC_S:
		case OP_AND_S:
		case OP_ORA_S:
		case OP_XOR_S:
		case OP_CMP_S:
			tmp2.bl = EF;			// Save the E Flag
			setE(false);			// Then Clear it
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp1.bl = readStack(CPU, tmp0.bl);
				tmp1.bm = 0;
				
				dbg_printf("%s %u,S (Target: $%06X, Value: $%02X, A = $%02X)", g1ALUNames[aaa(opcode)], tmp0.bl, addrStack(CPU, tmp0.bl), tmp1.bl, A.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp1.bl = readStack(CPU, tmp0.bl);
				tmp1.bm = readStack(CPU, tmp0.bl + 1);
				
				dbg_printf("%s %u,S (Target: $%06X, Value: $%04X, A = $%04X)", g1ALUNames[aaa(opcode)], tmp0.bl, addrStack(CPU, tmp0.bl), tmp1.wl, A.w);
			}
			setE(tmp2.bl);		// And afterwards restore it again
			g1ALU(CPU, tmp1.wl, aaa(opcode));
		break;
		
		// Stack Indirect Y
		case OP_ADC_SIY:
		case OP_SBC_SIY:
		case OP_AND_SIY:
		case OP_ORA_SIY:
		case OP_XOR_SIY:
		case OP_CMP_SIY:
			tmp2.bl = EF;			// Save the E Flag
			setE(false);			// Then Clear it
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp1.bl = readStack(CPU, tmp0.bl);
				tmp1.bm = readStack(CPU, tmp0.bl + 1);
				tmp2.bl = readAbs(CPU, tmp1.wl + Y.w);
				tmp2.bm = 0;
				
				dbg_printf("%s (%u,S),Y (Target: $%06X, Value: $%02X, A = $%02X)", g1ALUNames[aaa(opcode)], tmp0.bl, addrAbs(CPU, tmp1.wl + Y.w), tmp2.bl, A.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp1.bl = readStack(CPU, tmp0.bl);
				tmp1.bm = readStack(CPU, tmp0.bl + 1);
				tmp2.bl = readAbs(CPU, tmp1.wl + Y.w);
				tmp2.bm = readAbs(CPU, tmp1.wl + Y.w + 1);
				
				dbg_printf("%s (%u,S),Y (Target: $%06X, Value: $%04X, A = $%04X)", g1ALUNames[aaa(opcode)], tmp0.bl, addrAbs(CPU, tmp1.bl + Y.w), tmp2.wl, A.w);
			}
			setE(tmp2.bl);		// And afterwards restore it again
			g1ALU(CPU, tmp2.wl, aaa(opcode));
		break;
		
		// X/Y Register Operations ----------------------------------------------- //
		case OP_INX:
			if (XF){
				X.bl++;
			}else{
				X.w++;
			}
			setNZ(CPU, XF, X.w);
			
			dbg_printf("INX");
		break;
		
		case OP_DEX:
			if (XF){
				X.bl--;
			}else{
				X.w--;
			}
			setNZ(CPU, XF, X.w);
			
			dbg_printf("DEX");
		break;
		
		// Immediate
		case OP_CPX_IM:
			if (XF){
				tmp0.bl = fetch(CPU);
				tmp1.wl = X.bl - tmp0.bl;
				setC(!tmp1.bm);
				
				dbg_printf("CPX #$%02X (X = $%02X)", tmp0.bl, X.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp1.l = X.w - tmp0.wl;
				setC(!tmp1.wh);
				
				dbg_printf("CPX #$%04X (X = $%04X)", tmp0.wl, X.w);
			}
			setNZ(CPU, XF, tmp1.wl);
		break;
		
		// Absolute
		case OP_CPX_A:
			if (XF){
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp1.bl = readAbs(CPU, tmp0.wl);
				tmp2.wl = X.bl - tmp1.bl;
				setC(!tmp2.bm);
				
				dbg_printf("CPX $%04X (Target: $%06X, Value: $%02X, X = $%02X)", tmp0.wl, addrAbs(CPU, tmp0.wl), tmp1.bl, X.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp1.bl = readAbs(CPU, tmp0.wl);
				tmp1.bm = readAbs(CPU, tmp0.wl + 1);
				tmp2.l = X.w - tmp1.wl;
				setC(!tmp2.wh);
				
				dbg_printf("CPX $%04X (Target: $%06X, Value: $%04X, X = $%04X)", tmp0.wl, addrAbs(CPU, tmp0.wl), tmp1.wl, X.w);
			}
			setNZ(CPU, XF, tmp2.wl);
		break;
		
		// Direct Page
		case OP_CPX_DP:
			if (XF){
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl);
				tmp2.wl = X.bl - tmp1.bl;
				setC(!tmp2.bm);
				
				dbg_printf("CPX $%02X (Target: $%06X, Value: $%02X, X = $%02X)", tmp0.bl, addrDP(CPU, tmp0.bl), tmp1.bl, X.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl);
				tmp1.bm = readDP(CPU, tmp0.bl + 1);
				tmp2.l = X.w - tmp1.wl;
				setC(!tmp2.wh);
				
				dbg_printf("CPX $%02X (Target: $%06X, Value: $%04X, X = $%04X)", tmp0.bl, addrDP(CPU, tmp0.bl), tmp1.wl, X.w);
			}
			setNZ(CPU, XF, tmp2.wl);
		break;
		
		case OP_INY:
			if (XF){
				Y.bl++;
			}else{
				Y.w++;
			}
			setNZ(CPU, XF, Y.w);
			
			dbg_printf("INY");
		break;
		
		case OP_DEY:
			if (XF){
				Y.bl--;
			}else{
				Y.w--;
			}
			setNZ(CPU, XF, Y.w);
			
			dbg_printf("DEY");
		break;
		
		// Immediate
		case OP_CPY_IM:
			if (XF){
				tmp0.bl = fetch(CPU);
				tmp1.wl = Y.bl - tmp0.bl;
				setC(!tmp1.bm);
				
				dbg_printf("CPY #$%02X (Y = $%02X)", tmp0.bl, Y.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp1.l = Y.w - tmp0.wl;
				setC(!tmp1.wh);
				
				dbg_printf("CPY #$%04X (Y = $%04X)", tmp0.wl, Y.w);
			}
			setNZ(CPU, XF, tmp1.wl);
		break;
		
		// Absolute
		case OP_CPY_A:
			if (XF){
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp1.bl = readAbs(CPU, tmp0.wl);
				tmp2.wl = Y.bl - tmp1.bl;
				setC(!tmp2.bm);
				
				dbg_printf("CPY $%04X (Target: $%06X, Value: $%02X, Y = $%02X)", tmp0.wl, addrAbs(CPU, tmp0.wl), tmp1.bl, Y.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp1.bl = readAbs(CPU, tmp0.wl);
				tmp1.bm = readAbs(CPU, tmp0.wl + 1);
				tmp2.l = Y.w - tmp1.wl;
				setC(!tmp2.wh);
				
				dbg_printf("CPY $%04X (Target: $%06X, Value: $%04X, Y = $%04X)", tmp0.wl, addrAbs(CPU, tmp0.wl), tmp1.wl, Y.w);
			}
			setNZ(CPU, XF, tmp2.wl);
		break;
		
		// Direct Page
		case OP_CPY_DP:
			if (XF){
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl);
				tmp2.wl = Y.bl - tmp1.bl;
				setC(!tmp2.bm);
				
				dbg_printf("CPY $%02X (Target: $%06X, Value: $%02X, Y = $%02X)", tmp0.bl, addrDP(CPU, tmp0.bl), tmp1.bl, Y.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl);
				tmp1.bm = readDP(CPU, tmp0.bl + 1);
				tmp2.l = Y.w - tmp1.wl;
				setC(!tmp2.wh);
				
				dbg_printf("CPY $%02X (Target: $%06X, Value: $%04X, Y = $%04X)", tmp0.bl, addrDP(CPU, tmp0.bl), tmp1.wl, Y.w);
			}
			setNZ(CPU, XF, tmp2.wl);
		break;
		
		// Type 2 ALU Operation (INC, DEC, ASL, LSR, ROL, ROR) ------------------- //
		// Accumulator
		case OP_INC:
			if (MF){	// 8 bit
				A.bl = A.bl + 1;
				dbg_printf("INC A (After Value: $%02X)", A.bl);
			}else{		// 16 bit
				A.w = A.w + 1;
				dbg_printf("INC A (After Value: $%04X)", A.w);
			}
			setNZ(CPU, MF, A.w);
		break;
		
		// Accumulator
		case OP_DEC:
			if (MF){	// 8 bit
				A.bl = A.bl - 1;
				dbg_printf("DEC A (Value: $%02X)", A.bl);
			}else{		// 16 bit
				A.w = A.w - 1;
				dbg_printf("DEC A (Value: $%04X)", A.w);
			}
			setNZ(CPU, MF, A.w);
		break;
		
		// Accumulator
		case OP_ASL:
		case OP_LSR:
		case OP_ROL:
		case OP_ROR:
			
			tmp0.wl = g2ALU(CPU, A.w, aaa(opcode));
			
			if (MF){
				A.bl = tmp0.bl;
				
				dbg_printf("%s A (Value: $%02X)", g2ALUNames[aaa(opcode)], A.bl);
			}else{
				A.w = tmp0.wl;
				
				dbg_printf("%s A (Value: $%04X)", g2ALUNames[aaa(opcode)], A.w);
			}
		break;
		
		// Absolute
		case OP_INC_A:
		case OP_DEC_A:
		case OP_ASL_A:
		case OP_LSR_A:
		case OP_ROL_A:
		case OP_ROR_A:
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp1.bl = readAbs(CPU, tmp0.wl);
				tmp1.bm = 0;
				
				dbg_printf("%s $%04X (Target: $%06X, Value: $%02X)", g2ALUNames[aaa(opcode)], tmp0.wl, addrAbs(CPU, tmp0.wl), tmp1.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp1.bl = readAbs(CPU, tmp0.wl);
				tmp1.bm = readAbs(CPU, tmp0.wl + 1);
				
				dbg_printf("%s $%04X (Target: $%06X, Value: $%04X)", g2ALUNames[aaa(opcode)], tmp0.wl, addrAbs(CPU, tmp0.wl), tmp1.wl);
			}
			
			tmp1.wl = g2ALU(CPU, tmp1.wl, aaa(opcode));
			
			if (MF){
				writeAbs(CPU, tmp0.wl, tmp1.bl);
			}else{
				writeAbs(CPU, tmp0.wl, tmp1.bl);
				writeAbs(CPU, tmp0.wl + 1, tmp1.bm);
			}
		break;
		
		// Absolute X
		case OP_INC_AX:
		case OP_DEC_AX:
		case OP_ASL_AX:
		case OP_LSR_AX:
		case OP_ROL_AX:
		case OP_ROR_AX:
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp1.bl = readAbs(CPU, tmp0.wl + X.w);
				tmp1.bm = 0;
				
				dbg_printf("%s $%04X,X (Target: $%06X, Value: $%02X)", g2ALUNames[aaa(opcode)], tmp0.wl, addrAbs(CPU, tmp0.wl + X.w), tmp1.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp0.bm = fetch(CPU);
				tmp1.bl = readAbs(CPU, tmp0.wl + X.w);
				tmp1.bm = readAbs(CPU, tmp0.wl + X.w + 1);
				
				dbg_printf("%s $%04X,X (Target: $%06X, Value: $%04X)", g2ALUNames[aaa(opcode)], tmp0.wl, addrAbs(CPU, tmp0.wl + X.w), tmp1.wl);
			}
			
			tmp1.wl = g2ALU(CPU, tmp1.wl, aaa(opcode));
			
			if (MF){
				writeAbs(CPU, tmp0.wl + X.w, tmp1.bl);
			}else{
				writeAbs(CPU, tmp0.wl + X.w, tmp1.bl);
				writeAbs(CPU, tmp0.wl + X.w + 1, tmp1.bm);
			}
		break;
		
		// Direct Page
		case OP_INC_DP:
		case OP_DEC_DP:
		case OP_ASL_DP:
		case OP_LSR_DP:
		case OP_ROL_DP:
		case OP_ROR_DP:
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl);
				tmp1.bm = 0;
				
				dbg_printf("%s $%02X (Target: $%06X, Value: $%02X)", g2ALUNames[aaa(opcode)], tmp0.wl, addrDP(CPU, tmp0.bl), tmp1.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl);
				tmp1.bm = readDP(CPU, tmp0.bl + 1);
				
				dbg_printf("%s $%02X (Target: $%06X, Value: $%04X)", g2ALUNames[aaa(opcode)], tmp0.wl, addrDP(CPU, tmp0.bl), tmp1.wl);
			}
			
			tmp1.wl = g2ALU(CPU, tmp1.wl, aaa(opcode));
			
			if (MF){
				writeDP(CPU, tmp0.bl, tmp1.bl);
			}else{
				writeDP(CPU, tmp0.bl, tmp1.bl);
				writeDP(CPU, tmp0.bl + 1, tmp1.bm);
			}
		break;
		
		// Direct Page X
		case OP_INC_DPX:
		case OP_DEC_DPX:
		case OP_ASL_DPX:
		case OP_LSR_DPX:
		case OP_ROL_DPX:
		case OP_ROR_DPX:
			if (MF){
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl + X.w);
				tmp1.bm = 0;
				
				dbg_printf("%s $%02X,X (Target: $%06X, Value: $%02X)", g2ALUNames[aaa(opcode)], tmp0.wl, addrDP(CPU, tmp0.bl + X.w), tmp1.bl);
			}else{
				tmp0.bl = fetch(CPU);
				tmp1.bl = readDP(CPU, tmp0.bl + X.w);
				tmp1.bm = readDP(CPU, tmp0.bl + X.w + 1);
				
				dbg_printf("%s $%02X,X (Target: $%06X, Value: $%04X)", g2ALUNames[aaa(opcode)], tmp0.wl, addrDP(CPU, tmp0.bl + X.w), tmp1.wl);
			}
			
			tmp1.wl = g2ALU(CPU, tmp1.wl, aaa(opcode));
			
			if (MF){
				writeDP(CPU, tmp0.bl + X.w, tmp1.bl);
			}else{
				writeDP(CPU, tmp0.bl + X.w, tmp1.bl);
				writeDP(CPU, tmp0.bl + X.w + 1, tmp1.bm);
			}
		break;
	}
	
	// Clear the high Bytes of X and Y when XF=1, in case they were changed somehow
	if (XF){
		X.bh = 0;
		Y.bh = 0;
	}
	
	// Update the Upper Byte of the SP when EF=1, in case it got changed somehow
	if (EF) SP.bh = 1;
	
	// Subtract the Cycles of the current instruction from the remainder
	cycleRem -= cycleTable[(EF ? 0x0400 : 0x0000) | (MF ? 0x0200 : 0x0000) | (XF ? 0x0100 : 0x0000) | opcode];
	
	dbg_printf(" (Cycles Remaining: %d)\n", cycleRem);
	
	// Update the Debug Flag
	DBG = _pre_debug;
	
	// Do another Instruction if there are still cycles left
	if (cycleRem > 0) goto next;
	
	return cycleRem;
}

