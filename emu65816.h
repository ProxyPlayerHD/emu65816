
#ifndef __65816
#define __65816


#define setDebug(c,v)		(c.dbg = !!v)
#define chkSTP(c)			(c.stp)
#define chkWAI(c)			(c.wai)

#define cpuSendIRQ(c)		c.wai = 0; if (!(c.interrupt || c.fl_i)) c.interrupt = 1
#define cpuSendNMI(c)		c.wai = 0; if (c.interrupt < 2) c.interrupt = 2
// #define cpuSendABT(c)		c.wai = 0; if (c.interrupt < 3) c.interrupt = 3


#define MEM					(CPU->mem)
#define MES					(CPU->mem_size)
#define IOP					(CPU->io_page)
#define IOS					(CPU->io_size)
#define IOR(a)				(CPU->io_read(a))
#define IOW(a,v)			(CPU->io_write(a,v))
#define INT					(CPU->interrupt)

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


#endif
