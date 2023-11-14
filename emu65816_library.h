
#ifndef __65816
#define __65816




#define dbg_printf(...)		if (DBG) printf(__VA_ARGS__);
#define chkIO(ad)			(((ad) >= IOB) && ((ad) < (IOB + IOS)))
#define aaa(opc)			((opc >> 5) & 7U)

#define MEM					(CPU->mem)
#define MES					(CPU->mem_size)
#define IOB					(CPU->io_base)
#define IOS					(CPU->io_size)
#define IOR(a)				(CPU->io_read(a))
#define IOW(a,v)			(CPU->io_write(a,v))
#define INT					(CPU->interrupt)
#define DBG					(CPU->dbg)

#define PC					(CPU->reg_pc)
#define SP					(CPU->reg_sp)
#define DP					(CPU->reg_dp)
#define DB					(CPU->reg_db)
#define PB					(CPU->reg_pb)
#define A					(CPU->reg_a)
#define X					(CPU->reg_x)
#define Y					(CPU->reg_y)

#define CF					(CPU->fl_c)
#define ZF					(CPU->fl_z)
#define IF					(CPU->fl_i)
#define DF					(CPU->fl_d)
#define XF					(CPU->fl_x)
#define MF					(CPU->fl_m)
#define VF					(CPU->fl_v)
#define NF					(CPU->fl_n)
#define EF					(CPU->fl_e)

#define setC(v)				(CF = !!(v))
#define setZ(v)				(ZF = !!(v))
#define setI(v)				(IF = !!(v))
#define setD(v)				(DF = !!(v))
#define setX(v)				(XF = !!(v))
#define setM(v)				(MF = !!(v))
#define setV(v)				(VF = !!(v))
#define setN(v)				(NF = !!(v))
#define setE(v)				(EF = !!(v))

#define SR_INIT				0b00110100
#define SR_BRK				0b00110000
#define SR_INT				0b00010000

#define VECT_E_NMI			(0x0000FFFA)
#define VECT_E_RES			(0x0000FFFC)
#define VECT_E_IRQ			(0x0000FFFE)
#define VECT_E_ABT			(0x0000FFF8)
#define VECT_E_COP			(0x0000FFF4)

#define VECT_N_IRQ			(0x0000FFEE)
#define VECT_N_NMI			(0x0000FFEA)
#define VECT_N_ABT			(0x0000FFE8)
#define VECT_N_BRK			(0x0000FFE6)
#define VECT_N_COP			(0x0000FFE4)





// Emulation Mode Interrupt Table
uint32_t interruptTableE[4] = {
	0,
	VECT_E_IRQ,
	VECT_E_NMI,
	VECT_E_ABT
};

// Native Mode Interrupt Table
uint32_t interruptTableN[4] = {
	0,
	VECT_N_IRQ,
	VECT_N_NMI,
	VECT_N_ABT,
};


typedef union{
	uint16_t w;				// 16-bit Word
	#ifdef __EMU_LITTLE_ENDIAN
	struct{
		uint8_t bl;			// Low Byte
		uint8_t bh;			// High Byte
	};
	#else
	struct{
		uint8_t bh;
		uint8_t bl;
	};
	#endif
} cint16_t;

typedef union{
	uint32_t l;				// 32-bit Word
	#ifdef __EMU_LITTLE_ENDIAN
	struct{
		union{
			uint16_t wl;	// Low Word
			int16_t swl;	// Low Word (Signed)
		};
		uint16_t wh;		// High Word
	};
	struct{
		union{
			uint8_t bl;		// Low Byte
			int8_t sbl;		// Low Byte (Signed)
		};
		uint8_t bm;			// Middle Byte
		uint8_t bh;			// High Byte
		uint8_t bx;			// eXtra Byte
	};
	#else
	struct{
		uint16_t wh;
		union{
			uint16_t wl;
			int16_t swl;
		};
	};
	struct{
		uint8_t bx;
		uint8_t bh;
		uint8_t bm;
		union{
			uint8_t bl;
			int8_t sbl;
		};
	};
	#endif
} cint32_t;



typedef struct{
	uint8_t *mem;							// Pointer to Memory
	uint8_t (*io_read)(uint32_t);			// Pointer to the IO Read Function
	void (*io_write)(uint32_t, uint8_t);	// Pointer to the IO Write Function
	uint32_t mem_size;						// Size of Memory
	uint32_t io_size;						// Size of the IO Page
	uint32_t io_base;						// Base Address of the IO Block in Memory
	
	cint16_t reg_pc;	// Program Counter
	cint16_t reg_sp;	// Stack Pointer
	cint16_t reg_dp;	// Direct Page
	cint16_t reg_a;		// Accumulator
	cint16_t reg_x;		// X Index Register
	cint16_t reg_y;		// Y Index Register
	uint8_t reg_pb;		// Program Bank
	uint8_t reg_db;		// Data Bank
	
	// Status Register
	bool fl_c;			// Carry
	bool fl_z;			// Zero
	bool fl_i;			// Interrupt
	bool fl_d;			// Decimal
	bool fl_x;			// Index Register Width
	bool fl_m;			// Accumulator/Memory Width
	bool fl_v;			// Overflow
	bool fl_n;			// Negative
	bool fl_e;			// Emulation
	
	bool dbg;			// debug flag
	bool wai;			// WAI Instruction
	bool stp;			// STP Instruction
	uint8_t interrupt;	// Interrupt value (0 = no interrupts pending, 1 = IRQ, 2 = NMI, 3 = ABORT)
	
} cpuState;


void cpuInit(cpuState* CPU, uint8_t* memory, uint32_t memSize, uint32_t ioAddress, uint32_t ioSize, uint8_t (*ioRead)(uint32_t), void (*ioWrite)(uint32_t, uint8_t));
int32_t cpuExecute(cpuState* CPU, int32_t cycles);


// --------------------------------------------------------------------- //

void static inline setNZ(cpuState* CPU, bool flag, uint16_t value){
	if (flag){	// 8-bit
		setN(value & 0x80);
		setZ(!(value & 0x00FF));
	}else{		// 16-bit
		setN(value & 0x8000);
		setZ(!(value & 0xFFFF));
	}
}

// --------------------------------------------------------------------- //

uint32_t static inline addrDP(cpuState* CPU, uint32_t addr){
	if (EF && !DP.bl){
		return (DP.w & 0x0000FF00) | (addr & 0x000000FF);
	}else{
		return (addr + DP.w) & 0x0000FFFF;
	}
}

uint32_t static inline addrStack(cpuState* CPU, uint32_t addr){
	if (EF){
		return 0x00000100 | ((addr + SP.bl) & 0x000000FF);
	}else{
		return (addr + SP.w) & 0x0000FFFF;
	}
}

uint32_t static inline addrAbs(cpuState* CPU, uint32_t addr){
	return (addr + ((uint32_t)DB << 16U)) & 0x00FFFFFF;
}

// --------------------------------------------------------------------- //

uint8_t static inline readDP(cpuState* CPU, uint32_t addr){
	uint32_t ad = addrDP(CPU, addr);
	
	if (ad >= MES) return 0;		// Prevent accessing out of bounds
	if (chkIO(ad)) return IOR(ad - IOB);
	return MEM[ad];
}

uint8_t static inline readStack(cpuState* CPU, uint32_t addr){
	uint32_t ad = addrStack(CPU, addr);
	
	if (ad >= MES) return 0;		// Prevent accessing out of bounds
	if (chkIO(ad)) return IOR(ad - IOB);
	return MEM[ad];
}

uint8_t static inline readAbs(cpuState* CPU, uint32_t addr){
	uint32_t ad = addrAbs(CPU, addr);
	
	if (ad >= MES) return 0;		// Prevent accessing out of bounds
	if (chkIO(ad)) return IOR(ad - IOB);
	return MEM[ad];
}

uint8_t static inline readMem(cpuState* CPU, uint32_t addr){
	uint32_t ad = addr & 0x00FFFFFF;
	
	if (ad >= MES) return 0;		// Prevent accessing out of bounds
	if (chkIO(ad)) return IOR(ad - IOB);
	return MEM[ad];
}

void static inline writeDP(cpuState* CPU, uint32_t addr, uint8_t in){
	uint32_t ad = addrDP(CPU, addr);
	
	if (ad >= MES) return;			// Prevent accessing out of bounds
	if (chkIO(ad)){
		IOW(ad - IOB, in);
		return;
	}
	MEM[ad] = in;
}

void static inline writeStack(cpuState* CPU, uint32_t addr, uint8_t in){
	uint32_t ad = addrStack(CPU, addr);
	
	if (ad >= MES) return;			// Prevent accessing out of bounds
	if (chkIO(ad)){
		IOW(ad - IOB, in);
		return;
	}
	MEM[ad] = in;
}

void static inline writeAbs(cpuState* CPU, uint32_t addr, uint8_t in){
	uint32_t ad = addrAbs(CPU, addr);
	
	if (ad >= MES) return;			// Prevent accessing out of bounds
	if (chkIO(ad)){
		IOW(ad - IOB, in);
		return;
	}
	MEM[ad] = in;
}

void static inline writeMem(cpuState* CPU, uint32_t addr, uint8_t in){
	uint32_t ad = addr & 0x00FFFFFF;
	
	if (ad >= MES) return;			// Prevent accessing out of bounds
	if (chkIO(ad)){
		IOW(ad - IOB, in);
		return;
	}
	MEM[ad] = in;
}

// --------------------------------------------------------------------- //

uint8_t static inline fetch(cpuState* CPU){
	uint32_t ad = (PC.w++ | (PB << 16U)) & 0x00FFFFFF;
	
	if (ad >= MES) return 0;		// Prevent accessing out of bounds
	if (chkIO(ad)) return IOR(ad - IOB);
	return MEM[ad];
}

uint8_t static inline pullStack(cpuState* CPU){
	uint32_t ad = (++SP.w) & 0x0000FFFF;
	
	if (EF){	// in Emulation Mode, force the SP's High Byte and the Address' Middle Byte to 0x01
		ad = 0x00000100 | (ad & 0x000000FF);
		SP.bh = 0x01;
	}
	
	if (ad >= MES) return 0;		// Prevent accessing out of bounds
	if (chkIO(ad)) return IOR(ad - IOB);
	
	return MEM[ad];
}

void static inline pushStack(cpuState* CPU, uint8_t in){
	uint32_t ad = (SP.w--) & 0x0000FFFF;
	
	if (EF){	// in Emulation Mode, force the SP's High Byte and the Address' Middle Byte to 0x01
		ad = 0x00000100 | (ad & 0x000000FF);
		SP.bh = 0x01;
	}
	
	if (ad >= MES) return;			// Prevent accessing out of bounds
	if (chkIO(ad)){
		IOW(ad - IOB, in);
		return;
	}
	
	MEM[ad] = in;
}

uint8_t static inline readSR(cpuState* CPU){
	uint8_t tmp = 0;
	tmp |= (CPU->fl_c) ? 0x01 : 0x00;
	tmp |= (CPU->fl_z) ? 0x02 : 0x00;
	tmp |= (CPU->fl_i) ? 0x04 : 0x00;
	tmp |= (CPU->fl_d) ? 0x08 : 0x00;
	tmp |= (CPU->fl_x) ? 0x10 : 0x00;
	tmp |= (CPU->fl_m) ? 0x20 : 0x00;
	tmp |= (CPU->fl_v) ? 0x40 : 0x00;
	tmp |= (CPU->fl_n) ? 0x80 : 0x00;
	return tmp;
}

void static inline writeSR(cpuState* CPU, uint8_t in){
	CPU->fl_c = !!(in & 0x01);
	CPU->fl_z = !!(in & 0x02);
	CPU->fl_i = !!(in & 0x04);
	CPU->fl_d = !!(in & 0x08);
	CPU->fl_x = !!(in & 0x10);
	CPU->fl_m = !!(in & 0x20);
	CPU->fl_v = !!(in & 0x40);
	CPU->fl_n = !!(in & 0x80);
}

// --------------------------------------------------------------------- //


enum {			// aaa - g1 ALU Operations
	ALU_ORA,	// 000 : ORA
	ALU_AND,	// 001 : AND
	ALU_XOR,	// 010 : XOR
	ALU_ADC,	// 011 : ADC
	ALU_Inv0,	// 100 : Invalid
	ALU_Inv1,	// 101 : Invalid
	ALU_CMP,	// 110 : CMP
	ALU_SBC		// 111 : SBC
};

const char *g1ALUNames[8] = {
	"ORA",
	"AND",
	"XOR",
	"ADC",
	"INVALID",
	"INVALID",
	"CMP",
	"SBC",
};

// 9's Complement Table
const uint8_t ncompl[256] = {
	0x99, 0x98, 0x97, 0x96, 0x95, 0x94, 0x93, 0x92,
	0x91, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	0x89, 0x88, 0x87, 0x86, 0x85, 0x84, 0x83, 0x82,
	0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x79, 0x78, 0x77, 0x76, 0x75, 0x74, 0x73, 0x72,
	0x71, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
	0x69, 0x68, 0x67, 0x66, 0x65, 0x64, 0x63, 0x62,
	0x61, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60,
	0x59, 0x58, 0x57, 0x56, 0x55, 0x54, 0x53, 0x52,
	0x51, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50,
	0x49, 0x48, 0x47, 0x46, 0x45, 0x44, 0x43, 0x42,
	0x41, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x39, 0x38, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32,
	0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
	0x29, 0x28, 0x27, 0x26, 0x25, 0x24, 0x23, 0x22,
	0x21, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12,
	0x11, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


void static inline g1ALU(cpuState* CPU, uint16_t in, uint8_t op){
	cint16_t tmpIn = {.w = in};
	cint32_t tmp0 = {.l = 0};
	cint32_t tmp1 = {.l = 0};
	
	switch(op & 7U){
		case ALU_SBC:
			if (DF){
				tmpIn.bl = ncompl[tmpIn.bl];
				tmpIn.bh = ncompl[tmpIn.bh];
			} else {
				tmpIn.w = ~tmpIn.w;
			}
		case ALU_ADC:
			
			switch(((DF) ? 2U : 0) | ((MF) ? 1U : 0)){
				case 0: // (16-bit Binary)
					tmp0.l = (uint32_t)A.w + (uint32_t)tmpIn.w + ((CF) ? 1U : 0U);
					setC(tmp0.wh);			// Set Carry if the upper Word is not 0
					setV(((~(A.w ^ tmpIn.w)) & (A.w ^ tmp0.wl)) & 0x8000);
					A.w = tmp0.wl;
				break;
				
				case 1: // ( 8-bit Binary)
					tmp0.wl = (uint16_t)A.bl + (uint16_t)tmpIn.bl + ((CF) ? 1U : 0U);
					setC(tmp0.bm);			// Set Carry if the upper Byte is not 0
					setV(((~(A.bl ^ tmpIn.bl)) & (A.bl ^ tmp0.bl)) & 0x80);
					A.bl = tmp0.bl;
				break;
				
				case 2: // (16-bit BCD)
					// Seperate the Nibbles into their own Bytes
					tmp0.bl = ((A.bl      ) & 0x0F) + ((tmpIn.bl      ) & 0x0F) + ((CF) ? 1U : 0U);
					tmp0.bm = ((A.bl >> 4U) & 0x0F) + ((tmpIn.bl >> 4U) & 0x0F);
					tmp0.bh = ((A.bh      ) & 0x0F) + ((tmpIn.bh      ) & 0x0F);
					tmp0.bx = ((A.bh >> 4U) & 0x0F) + ((tmpIn.bh >> 4U) & 0x0F);
					
					if (tmp0.bl > 9){		// If the lowest Nibble went over 9
						tmp0.bl += 6;		// Add 6 to it
						tmp0.bl &= 0x0F;	// Cut off the upper part
						tmp0.bm++;		// And add 1 to the next Nibble
					}
					if (tmp0.bm > 9){		// And so on...
						tmp0.bm += 6;
						tmp0.bm &= 0x0F;
						tmp0.bh++;
					}
					if (tmp0.bh > 9){		// And so on...
						tmp0.bh += 6;
						tmp0.bh &= 0x0F;
						tmp0.bx++;
					}
					if (tmp0.bx > 9){		// And so on...
						tmp0.bx += 6;
						tmp0.bx &= 0x0F;
						setC(true);			// Except this one also sets the Carry
					}else{
						setC(false);		// Otherwise it clears it
					}
					
					// Stitch the final result together
					tmp0.wl = tmp0.bl | (tmp0.bm << 4U) | (tmp0.bh << 8U) | (tmp0.bx << 12U);
					
					// Set the V Flag (broken? why?)
					//setV(((~(A.w ^ tmpIn.w)) & (A.w ^ tmp0.wl)) & 0x8000);
					setV(((A.w & 0x8000) == (tmpIn.w & 0x8000)) && ((A.w & 0x8000) != (tmp0.wl & 0x8000)));
					
					// And finally save the result
					A.w = tmp0.wl;
				break;
				
				case 3: // ( 8-bit BCD)
					tmp0.bl = ((A.bl      ) & 0x0F) + ((tmpIn.bl      ) & 0x0F) + ((CF) ? 1U : 0U);
					tmp0.bm = ((A.bl >> 4U) & 0x0F) + ((tmpIn.bl >> 4U) & 0x0F);
					
					if (tmp0.bl > 9){		// If the lowest Nibble went over 9
						tmp0.bl += 6;		// Add 6 to it
						tmp0.bl &= 0x0F;	// Cut off the upper part
						tmp0.bm++;      	// And add 1 to the next Nibble
					}
					if (tmp0.bm > 9){		// And so on...
						tmp0.bm += 6;
						tmp0.bm &= 0x0F;
						setC(true);			// Except this one also sets the Carry
					}else{
						setC(false);		// Otherwise it clears it
					}
					
					// Stitch the final result together
					tmp0.bl = tmp0.bl | (tmp0.bm << 4U);
					
					// Set the V Flag (broken? why?)
					//setV(((~(A.bl ^ tmpIn.bl)) & (A.bl ^ tmp0.bl)) & 0x80);
					setV(((A.bl & 0x80) == (tmpIn.bl & 0x80)) && ((A.bl & 0x80) != (tmp0.bl & 0x80)));
					
					// And finally save the result
					A.bl = tmp0.bl;
				break;
			}
			setNZ(CPU, MF, A.w);
		break;
		
		case ALU_ORA:
			if (MF){	// 8 bit
				A.bl |= tmpIn.bl;
			}else{		// 16 bit
				A.w |= tmpIn.w;
			}
			setNZ(CPU, MF, A.w);
		break;
		
		case ALU_AND:
			if (MF){	// 8 bit
				//printf(" !! $%02X = $%02X & $%02X, Flags: ", A.bl & tmpIn.bl, A.bl, tmpIn.bl);
				A.bl &= tmpIn.bl;
			}else{		// 16 bit
				A.w &= tmpIn.w;
			}
			//printf("%c%c%c%c%c%c%c%c - ", btoc(NF), btoc(VF), btoc(MF), btoc(XF), btoc(DF), btoc(IF), btoc(ZF), btoc(CF));
			setNZ(CPU, MF, A.w);
			//printf("%c%c%c%c%c%c%c%c !! ", btoc(NF), btoc(VF), btoc(MF), btoc(XF), btoc(DF), btoc(IF), btoc(ZF), btoc(CF));
		break;
		
		case ALU_XOR:
			if (MF){	// 8 bit
				A.bl ^= tmpIn.bl;
			}else{		// 16 bit
				A.w ^= tmpIn.w;
			}
			setNZ(CPU, MF, A.w);
		break;
		
		case ALU_CMP:
			if (MF){	// 8 bit
				tmp0.wl = A.bl - tmpIn.bl;
				setC(!tmp0.bm);
			}else{		// 16 bit
				tmp0.l = A.w - tmpIn.w;
				setC(!tmp0.wh);
			}
			setNZ(CPU, MF, tmp0.wl);
		break;
		
		default:
			
		break;
	}
}

enum {			// aaa - g2 ALU Operations
	ALU_ASL,	// 000 : ASL
	ALU_ROL,	// 001 : ROL
	ALU_LSR,	// 010 : LSR
	ALU_ROR,	// 011 : ROR
	ALU_Inv2,	// 100 : Invalid
	ALU_Inv3,	// 101 : Invalid
	ALU_DEC,	// 110 : DEC
	ALU_INC		// 111 : INC
};

const char *g2ALUNames[8] = {
	"ASL",
	"ROL",
	"LSR",
	"ROR",
	"INVALID",
	"INVALID",
	"DEC",
	"INC",
};

uint16_t static inline g2ALU(cpuState* CPU, uint16_t in, uint8_t op){
	cint16_t tmpIn = {.w = in};
	cint32_t tmp0 = {.l = 0};
	cint32_t tmp1 = {.l = 0};
	
	switch(op & 7U){
		case ALU_ASL:
			if (MF){	// 8 bit
				tmp0.wl = (uint16_t)tmpIn.bl << 1U;
				setC(tmp0.bm);
			}else{		// 16 bit
				tmp0.l = (uint32_t)tmpIn.w << 1U;
				setC(tmp0.wh);
			}
			setNZ(CPU, MF, tmp0.wl);
		break;
		
		case ALU_ROL:
			if (MF){	// 8 bit
				tmp0.wl  = (uint16_t)tmpIn.bl << 1U;
				tmp0.bl |= CF ? 0x01 : 0x00;
				setC(tmp0.bm);
			}else{		// 16 bit
				tmp0.l = (uint32_t)tmpIn.w << 1U;
				tmp0.wl |= CF ? 0x0001 : 0x0000;
				setC(tmp0.wh);
			}
			setNZ(CPU, MF, tmp0.wl);
		break;
		
		case ALU_LSR:
			if (MF){	// 8 bit
				tmp0.bl = (tmpIn.bl >> 1U) & 0x7F;
				setC(tmpIn.bl & 0x01);
			}else{		// 16 bit
				tmp0.wl = (tmpIn.w >> 1U) & 0x7FFF;
				setC(tmpIn.w & 0x0001);
			}
			setNZ(CPU, MF, tmp0.wl);
		break;
		
		case ALU_ROR:
			if (MF){	// 8 bit
				tmp0.bl  = (tmpIn.bl >> 1U) & 0x7F;
				tmp0.bl |= CF ? 0x80 : 0x00;
				setC(tmpIn.bl & 0x01);
			}else{		// 16 bit
				tmp0.wl = (tmpIn.w >> 1U) & 0x7FFF;
				tmp0.wl |= CF ? 0x8000 : 0x0000;
				setC(tmpIn.w & 0x0001);
			}
			setNZ(CPU, MF, tmp0.wl);
		break;
		
		case ALU_DEC:
			if (MF){	// 8 bit
				tmp0.bl = tmpIn.bl - 1;
				tmp0.bm = 0;
			}else{		// 16 bit
				tmp0.wl = tmpIn.w - 1;
			}
			setNZ(CPU, MF, tmp0.wl);
		break;
		
		case ALU_INC:
			if (MF){	// 8 bit
				tmp0.bl = tmpIn.bl + 1;
				tmp0.bm = 0;
			}else{		// 16 bit
				tmp0.wl = tmpIn.w + 1;
			}
			setNZ(CPU, MF, tmp0.wl);
		break;
		
		default:
			
		break;
	}
	
	return tmp0.wl;
}





// --------------------------------------------------------------------- //


// Table bit assignment: emxoooooooo (e = emulation, m = memory/accu, x = index registers, o = opcode)
// when e = 1, m and x are also always 1
uint8_t cycleTable[256*5] = {
	8, 7, 8, 5, 7, 4, 7, 7, 3, 3, 2, 4, 8, 5, 8, 6,		// e0m0x0
	2, 7, 6, 8, 7, 5, 8, 7, 2, 6, 2, 2, 8, 6, 9, 6,
	6, 7, 8, 5, 4, 4, 7, 7, 4, 3, 2, 5, 5, 5, 8, 6,
	2, 7, 6, 8, 5, 5, 8, 7, 2, 6, 2, 2, 6, 6, 9, 6,
	7, 7, 2, 5, 0, 4, 7, 7, 3, 3, 2, 3, 3, 5, 8, 6,
	2, 7, 6, 8, 8, 5, 8, 7, 2, 6, 4, 2, 4, 6, 9, 6,
	6, 7, 6, 5, 4, 4, 7, 7, 4, 3, 2, 6, 5, 5, 8, 6,
	2, 7, 6, 8, 5, 5, 8, 7, 2, 6, 5, 2, 6, 6, 9, 6,
	2, 7, 3, 5, 4, 4, 4, 7, 2, 3, 2, 3, 5, 5, 5, 6,
	2, 7, 6, 8, 5, 5, 5, 7, 2, 6, 2, 2, 5, 6, 6, 6,
	3, 7, 3, 5, 4, 4, 4, 7, 2, 3, 2, 4, 5, 5, 5, 6,
	2, 7, 6, 8, 5, 5, 5, 7, 2, 6, 2, 2, 5, 6, 5, 6,
	3, 7, 3, 5, 4, 4, 7, 7, 2, 3, 2, 3, 5, 5, 8, 6,
	2, 7, 6, 8, 6, 5, 8, 7, 2, 6, 4, 3, 6, 6, 9, 6,
	3, 7, 3, 5, 4, 4, 7, 7, 2, 3, 2, 3, 5, 5, 8, 6,
	2, 7, 6, 8, 5, 5, 8, 7, 2, 6, 5, 2, 6, 6, 9, 6,
	8, 7, 8, 5, 7, 4, 7, 7, 3, 3, 2, 4, 8, 5, 8, 6,		// e0m0x1
	2, 6, 6, 8, 7, 5, 8, 7, 2, 5, 2, 2, 8, 5, 9, 6,
	6, 7, 8, 5, 4, 4, 7, 7, 4, 3, 2, 5, 5, 5, 8, 6,
	2, 6, 6, 8, 5, 5, 8, 7, 2, 5, 2, 2, 5, 5, 9, 6,
	7, 7, 2, 5, 0, 4, 7, 7, 4, 3, 2, 3, 3, 5, 8, 6,
	2, 6, 6, 8, 8, 5, 8, 7, 2, 5, 3, 2, 4, 5, 9, 6,
	6, 7, 6, 5, 4, 4, 7, 7, 5, 3, 2, 6, 5, 5, 8, 6,
	2, 6, 6, 8, 5, 5, 8, 7, 2, 5, 4, 2, 6, 5, 9, 6,
	2, 7, 3, 5, 3, 4, 3, 7, 2, 3, 2, 3, 4, 5, 4, 6,
	2, 6, 6, 8, 4, 5, 4, 7, 2, 5, 2, 2, 5, 5, 5, 6,
	2, 7, 2, 5, 3, 4, 3, 7, 2, 3, 2, 4, 4, 5, 4, 6,
	2, 6, 6, 8, 4, 5, 4, 7, 2, 5, 2, 2, 4, 5, 4, 6,
	2, 7, 3, 5, 3, 4, 7, 7, 2, 3, 2, 3, 4, 5, 8, 6,
	2, 6, 6, 8, 6, 5, 8, 7, 2, 5, 3, 3, 6, 5, 9, 6,
	2, 7, 3, 5, 3, 4, 7, 7, 2, 3, 2, 3, 4, 5, 8, 6,
	2, 6, 6, 8, 5, 5, 8, 7, 2, 5, 4, 2, 6, 5, 9, 6,
	8, 6, 8, 4, 5, 3, 5, 6, 3, 2, 2, 4, 6, 4, 6, 5,		// e0m1x0
	2, 6, 5, 7, 5, 4, 6, 6, 2, 5, 2, 2, 6, 5, 7, 5,
	6, 6, 8, 4, 3, 3, 5, 6, 4, 2, 2, 5, 4, 4, 6, 5,
	2, 6, 5, 7, 4, 4, 6, 6, 2, 5, 2, 2, 5, 5, 7, 5,
	7, 6, 2, 4, 0, 3, 5, 6, 4, 2, 2, 3, 3, 4, 6, 5,
	2, 6, 5, 7, 7, 4, 6, 6, 2, 5, 4, 2, 4, 5, 7, 5,
	6, 6, 6, 4, 3, 3, 5, 6, 5, 2, 2, 6, 5, 4, 6, 5,
	2, 6, 5, 7, 4, 4, 6, 6, 2, 5, 5, 2, 6, 5, 7, 5,
	2, 6, 3, 4, 4, 3, 4, 6, 2, 2, 2, 3, 5, 4, 5, 5,
	2, 6, 5, 7, 5, 4, 5, 6, 2, 5, 2, 2, 4, 5, 5, 5,
	3, 6, 3, 4, 4, 3, 4, 6, 2, 2, 2, 4, 5, 4, 5, 5,
	2, 6, 5, 7, 5, 4, 5, 6, 2, 5, 2, 2, 5, 5, 5, 5,
	3, 6, 3, 4, 4, 3, 6, 6, 2, 2, 2, 3, 5, 4, 6, 5,
	2, 6, 5, 7, 6, 4, 8, 6, 2, 5, 4, 3, 6, 5, 7, 5,
	3, 6, 3, 4, 4, 3, 6, 6, 2, 2, 2, 3, 5, 4, 6, 5,
	2, 6, 5, 7, 5, 4, 8, 6, 2, 5, 5, 2, 6, 5, 7, 5,
	8, 6, 8, 4, 5, 3, 5, 6, 3, 2, 2, 4, 6, 4, 6, 5,		// e0m1x1
	2, 5, 5, 7, 5, 4, 6, 6, 2, 4, 2, 2, 6, 4, 7, 5,
	6, 6, 8, 4, 3, 3, 5, 6, 4, 2, 2, 5, 4, 4, 6, 5,
	2, 5, 5, 7, 4, 4, 6, 6, 2, 4, 2, 2, 4, 4, 7, 5,
	7, 6, 2, 4, 7, 3, 5, 6, 3, 2, 2, 3, 3, 4, 6, 5,
	2, 5, 5, 7, 7, 4, 6, 6, 2, 4, 3, 2, 4, 4, 7, 5,
	6, 6, 6, 4, 3, 3, 5, 6, 4, 2, 2, 6, 5, 4, 6, 5,
	2, 5, 5, 7, 4, 4, 6, 6, 2, 4, 4, 2, 6, 4, 7, 5,
	2, 6, 3, 4, 3, 3, 3, 6, 2, 2, 2, 3, 4, 4, 4, 5,
	2, 6, 5, 7, 4, 4, 4, 6, 2, 5, 2, 2, 4, 5, 5, 5,
	2, 6, 2, 4, 3, 3, 3, 6, 2, 2, 2, 4, 4, 4, 4, 5,
	2, 5, 5, 7, 4, 4, 4, 6, 2, 4, 2, 2, 4, 4, 4, 5,
	2, 6, 3, 4, 3, 3, 5, 6, 2, 2, 2, 3, 4, 4, 4, 5,
	2, 5, 5, 7, 6, 4, 6, 6, 2, 4, 3, 3, 6, 4, 7, 5,
	2, 6, 3, 4, 3, 3, 5, 6, 2, 2, 2, 3, 4, 4, 6, 5,
	2, 5, 5, 7, 5, 4, 6, 6, 2, 4, 4, 2, 6, 4, 7, 5,
	8, 6, 8, 4, 5, 3, 5, 6, 3, 2, 2, 4, 6, 4, 6, 5,		// e1m1x1
	2, 5, 5, 7, 5, 4, 6, 6, 2, 4, 2, 2, 6, 4, 7, 5,
	6, 6, 8, 4, 3, 3, 5, 6, 4, 2, 2, 5, 4, 4, 6, 5,
	2, 5, 5, 7, 4, 4, 6, 6, 2, 4, 2, 2, 4, 4, 7, 5,
	7, 6, 2, 4, 0, 3, 5, 6, 3, 2, 2, 3, 3, 4, 6, 5,
	2, 5, 5, 7, 7, 4, 6, 6, 2, 4, 3, 2, 4, 4, 7, 5,
	6, 6, 6, 4, 3, 3, 5, 6, 4, 2, 2, 6, 5, 4, 6, 5,
	2, 5, 5, 7, 4, 4, 6, 6, 2, 4, 4, 2, 6, 4, 7, 5,
	2, 6, 3, 4, 3, 3, 3, 6, 2, 2, 2, 3, 4, 4, 4, 5,
	2, 5, 5, 7, 4, 4, 4, 6, 2, 4, 2, 2, 4, 4, 4, 5,
	2, 6, 2, 4, 3, 3, 3, 6, 2, 2, 2, 4, 4, 4, 4, 5,
	2, 5, 5, 7, 4, 4, 4, 6, 2, 4, 2, 2, 4, 4, 4, 5,
	2, 6, 3, 4, 3, 3, 5, 6, 2, 2, 2, 3, 4, 4, 6, 5,
	2, 5, 5, 7, 6, 4, 6, 6, 2, 4, 3, 3, 6, 4, 7, 5,
	2, 6, 3, 4, 3, 3, 5, 6, 2, 2, 2, 3, 4, 4, 6, 5,
	2, 5, 5, 7, 5, 4, 6, 6, 2, 4, 4, 2, 6, 4, 7, 5,
};




enum{
	OP_BRK_IM,		// 0x00
	OP_ORA_DPXI,	// 0x01
	OP_COP_IM,		// 0x02
	OP_ORA_S,		// 0x03
	OP_TSB_DP,		// 0x04
	OP_ORA_DP,		// 0x05
	OP_ASL_DP,		// 0x06
	OP_ORA_DPIL,	// 0x07
	OP_PHP,			// 0x08
	OP_ORA_IM,		// 0x09
	OP_ASL,			// 0x0A
	OP_PHD,			// 0x0B
	OP_TSB_A,		// 0x0C
	OP_ORA_A,		// 0x0D
	OP_ASL_A,		// 0x0E
	OP_ORA_AL,		// 0x0F
	
	OP_BPL_R,		// 0x10
	OP_ORA_DPIY,	// 0x11
	OP_ORA_DPI,		// 0x12
	OP_ORA_SIY,		// 0x13
	OP_TRB_DP,		// 0x14
	OP_ORA_DPX,		// 0x15
	OP_ASL_DPX,		// 0x16
	OP_ORA_DPILY,	// 0x17
	OP_CLC,			// 0x18
	OP_ORA_AY,		// 0x19
	OP_INC,			// 0x1A
	OP_TCS,			// 0x1B
	OP_TRB_A,		// 0x1C
	OP_ORA_AX,		// 0x1D
	OP_ASL_AX,		// 0x1E
	OP_ORA_ALX,		// 0x1F
	
	OP_JSR_A,		// 0x20
	OP_AND_DPXI,	// 0x21
	OP_JSL_AL,		// 0x22
	OP_AND_S,		// 0x23
	OP_BIT_DP,		// 0x24
	OP_AND_DP,		// 0x25
	OP_ROL_DP,		// 0x26
	OP_AND_DPIL,	// 0x27
	OP_PLP,			// 0x28
	OP_AND_IM,		// 0x29
	OP_ROL,			// 0x2A
	OP_PLD,			// 0x2B
	OP_BIT_A,		// 0x2C
	OP_AND_A,		// 0x2D
	OP_ROL_A,		// 0x2E
	OP_AND_AL,		// 0x2F
	
	OP_BMI_R,		// 0x30
	OP_AND_DPIY,	// 0x31
	OP_AND_DPI,		// 0x32
	OP_AND_SIY,		// 0x33
	OP_BIT_DPX,		// 0x34
	OP_AND_DPX,		// 0x35
	OP_ROL_DPX,		// 0x36
	OP_AND_DPILY,	// 0x37
	OP_SEC,			// 0x38
	OP_AND_AY,		// 0x39
	OP_DEC,			// 0x3A
	OP_TSC,			// 0x3B
	OP_BIT_AX,		// 0x3C
	OP_AND_AX,		// 0x3D
	OP_ROL_AX,		// 0x3E
	OP_AND_ALX,		// 0x3F
	
	OP_RTI,			// 0x40
	OP_XOR_DPXI,	// 0x41
	OP_WDM,			// 0x42 (potential extensions)
	OP_XOR_S,		// 0x43
	OP_MVP_IM,		// 0x44
	OP_XOR_DP,		// 0x45
	OP_LSR_DP,		// 0x46
	OP_XOR_DPIL,	// 0x47
	OP_PHA,			// 0x48
	OP_XOR_IM,		// 0x49
	OP_LSR,			// 0x4A
	OP_PHK,			// 0x4B
	OP_JMP_A,		// 0x4C
	OP_XOR_A,		// 0x4D
	OP_LSR_A,		// 0x4E
	OP_XOR_AL,		// 0x4F
	
	OP_BVC_R,		// 0x50
	OP_XOR_DPIY,	// 0x51
	OP_XOR_DPI,		// 0x52
	OP_XOR_SIY,		// 0x53
	OP_MVN_IM,		// 0x54
	OP_XOR_DPX,		// 0x55
	OP_LSR_DPX,		// 0x56
	OP_XOR_DPILY,	// 0x57
	OP_CLI,			// 0x58
	OP_XOR_AY,		// 0x59
	OP_PHY,			// 0x5A
	OP_TCD,			// 0x5B
	OP_JML_AL,		// 0x5C
	OP_XOR_AX,		// 0x5D
	OP_LSR_AX,		// 0x5E
	OP_XOR_ALX,		// 0x5F
	
	OP_RTS,			// 0x60
	OP_ADC_DPXI,	// 0x61
	OP_PER_IM,		// 0x62
	OP_ADC_S,		// 0x63
	OP_STZ_DP,		// 0x64
	OP_ADC_DP,		// 0x65
	OP_ROR_DP,		// 0x66
	OP_ADC_DPIL,	// 0x67
	OP_PLA,			// 0x68
	OP_ADC_IM,		// 0x69
	OP_ROR,			// 0x6A
	OP_RTL,			// 0x6B
	OP_JMP_AI,		// 0x6C
	OP_ADC_A,		// 0x6D
	OP_ROR_A,		// 0x6E
	OP_ADC_AL,		// 0x6F
	
	OP_BVS_R,		// 0x70
	OP_ADC_DPIY,	// 0x71
	OP_ADC_DPI,		// 0x72
	OP_ADC_SIY,		// 0x73
	OP_STZ_DPX,		// 0x74
	OP_ADC_DPX,		// 0x75
	OP_ROR_DPX,		// 0x76
	OP_ADC_DPILY,	// 0x77
	OP_SEI,			// 0x78
	OP_ADC_AY,		// 0x79
	OP_PLY,			// 0x7A
	OP_TDC,			// 0x7B
	OP_JMP_AXI,		// 0x7C
	OP_ADC_AX,		// 0x7D
	OP_ROR_AX,		// 0x7E
	OP_ADC_ALX,		// 0x7F
	
	OP_BRA_R,		// 0x80
	OP_STA_DPXI,	// 0x81
	OP_BRL_R,		// 0x82
	OP_STA_S,		// 0x83
	OP_STY_DP,		// 0x84
	OP_STA_DP,		// 0x85
	OP_STX_DP,		// 0x86
	OP_STA_DPIL,	// 0x87
	OP_DEY,			// 0x88
	OP_BIT_IM,		// 0x89
	OP_TXA,			// 0x8A
	OP_PHB,			// 0x8B
	OP_STY_A,		// 0x8C
	OP_STA_A,		// 0x8D
	OP_STX_A,		// 0x8E
	OP_STA_AL,		// 0x8F
	
	OP_BCC_R,		// 0x90
	OP_STA_DPIY,	// 0x91
	OP_STA_DPI,		// 0x92
	OP_STA_SIY,		// 0x93
	OP_STY_DPX,		// 0x94
	OP_STA_DPX,		// 0x95
	OP_STX_DPY,		// 0x96
	OP_STA_DPILY,	// 0x97
	OP_TYA,			// 0x98
	OP_STA_AY,		// 0x99
	OP_TXS,			// 0x9A
	OP_TXY,			// 0x9B
	OP_STZ_A,		// 0x9C
	OP_STA_AX,		// 0x9D
	OP_STZ_AX,		// 0x9E
	OP_STA_ALX,		// 0x9F
	
	OP_LDY_IM,		// 0xA0
	OP_LDA_DPXI,	// 0xA1
	OP_LDX_IM,		// 0xA2
	OP_LDA_S,		// 0xA3
	OP_LDY_DP,		// 0xA4
	OP_LDA_DP,		// 0xA5
	OP_LDX_DP,		// 0xA6
	OP_LDA_DPIL,	// 0xA7
	OP_TAY,			// 0xA8
	OP_LDA_IM,		// 0xA9
	OP_TAX,			// 0xAA
	OP_PLB,			// 0xAB
	OP_LDY_A,		// 0xAC
	OP_LDA_A,		// 0xAD
	OP_LDX_A,		// 0xAE
	OP_LDA_AL,		// 0xAF
	
	OP_BCS_R,		// 0xB0
	OP_LDA_DPIY,	// 0xB1
	OP_LDA_DPI,		// 0xB2
	OP_LDA_SIY,		// 0xB3
	OP_LDY_DPX,		// 0xB4
	OP_LDA_DPX,		// 0xB5
	OP_LDX_DPY,		// 0xB6
	OP_LDA_DPILY,	// 0xB7
	OP_CLV,			// 0xB8
	OP_LDA_AY,		// 0xB9
	OP_TSX,			// 0xBA
	OP_TYX,			// 0xBB
	OP_LDY_AX,		// 0xBC
	OP_LDA_AX,		// 0xBD
	OP_LDX_AY,		// 0xBE
	OP_LDA_ALX,		// 0xBF
	
	OP_CPY_IM,		// 0xC0
	OP_CMP_DPXI,	// 0xC1
	OP_REP_IM,		// 0xC2
	OP_CMP_S,		// 0xC3
	OP_CPY_DP,		// 0xC4
	OP_CMP_DP,		// 0xC5
	OP_DEC_DP,		// 0xC6
	OP_CMP_DPIL,	// 0xC7
	OP_INY,			// 0xC8
	OP_CMP_IM,		// 0xC9
	OP_DEX,			// 0xCA
	OP_WAI,			// 0xCB
	OP_CPY_A,		// 0xCC
	OP_CMP_A,		// 0xCD
	OP_DEC_A,		// 0xCE
	OP_CMP_AL,		// 0xCF
	
	OP_BNE_R,		// 0xD0
	OP_CMP_DPIY,	// 0xD1
	OP_CMP_DPI,		// 0xD2
	OP_CMP_SIY,		// 0xD3
	OP_PEI_DP,		// 0xD4
	OP_CMP_DPX,		// 0xD5
	OP_DEC_DPX,		// 0xD6
	OP_CMP_DPILY,	// 0xD7
	OP_CLD,			// 0xD8
	OP_CMP_AY,		// 0xD9
	OP_PHX,			// 0xDA
	OP_STP,			// 0xDB
	OP_JML_AI,		// 0xDC
	OP_CMP_AX,		// 0xDD
	OP_DEC_AX,		// 0xDE
	OP_CMP_ALX,		// 0xDF
	
	OP_CPX_IM,		// 0xE0
	OP_SBC_DPXI,	// 0xE1
	OP_SEP_IM,		// 0xE2
	OP_SBC_S,		// 0xE3
	OP_CPX_DP,		// 0xE4
	OP_SBC_DP,		// 0xE5
	OP_INC_DP,		// 0xE6
	OP_SBC_DPIL,	// 0xE7
	OP_INX,			// 0xE8
	OP_SBC_IM,		// 0xE9
	OP_NOP,			// 0xEA
	OP_XBA,			// 0xEB
	OP_CPX_A,		// 0xEC
	OP_SBC_A,		// 0xED
	OP_INC_A,		// 0xEE
	OP_SBC_AL,		// 0xEF
	
	OP_BEQ_R,		// 0xF0
	OP_SBC_DPIY,	// 0xF1
	OP_SBC_DPI,		// 0xF2
	OP_SBC_SIY,		// 0xF3
	OP_PEA_IM,		// 0xF4
	OP_SBC_DPX,		// 0xF5
	OP_INC_DPX,		// 0xF6
	OP_SBC_DPILY,	// 0xF7
	OP_SED,			// 0xF8
	OP_SBC_AY,		// 0xF9
	OP_PLX,			// 0xFA
	OP_XCE,			// 0xFB
	OP_JSR_AXI,		// 0xFC
	OP_SBC_AX,		// 0xFD
	OP_INC_AX,		// 0xFE
	OP_SBC_ALX		// 0xFF
};



#endif