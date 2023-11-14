#include <conio.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#define __EMU_LITTLE_ENDIAN

#include "65816.h"




#define CLOCK		(160000U)				// Cycles per 10 Millisecond			(Currently 16MHz (16 * 10000 Cycles per 10 milliseconds)

#define MEM_SIZE	(1024U * 1024U *  4)	// Amount of RAM starting at 0x000000	(Currently 4MB)
#define ROM_START	(0x00008000)			// Starting Address of the ROM			(Example value)
#define IO_START	(0x0000FF00)			// Starting Address of IO Space			(Example value)
#define IO_SIZE		(256U)					// Size of IO Space						(Example value)



// File IO Global Variables
cint32_t filePtr = {.l = 0};	// Pointer to Emulator Memory containing the Command structure
uint8_t fileCmd = 0;			// Command ID
uint8_t fileResponse = 0;		// Return Value/Error Code
FILE *fp;
void fileIO(cpuState *CPU);


// User Functions
uint8_t cpu0ReadIO(uint32_t addr);
void cpu0WriteIO(uint32_t addr, uint8_t val);
cint32_t timer, tmpTimer;


// Opens and Reads a file as binary and stores it's contents into a memory Array at a specified address
// Returns the amount of Bytes read
int32_t readROM(uint8_t* memory, uint32_t memSize, uint32_t address, const char* str){
	FILE *fp;
	size_t fileSize;
	
	fp = fopen(str, "rb");
	if (!fp){
		printf("ROM file not found!\n");
		return -1;
	}
	
	fseek(fp, 0, SEEK_END);
	fileSize = ftell(fp);
	rewind(fp);
	
	if (fileSize > memSize){
		printf("File too large!\n");
		fclose(fp);
		return -1;
	}
	
	fread((memory + address), sizeof(uint8_t), fileSize, fp);
	fclose(fp);
	
	return fileSize;
}




int main(int argc, char* argv[]){
	cpuState CPU0;			// the CPU Struct
	
	printf("CPU Struct is %llu Bytes large!\n", sizeof(cpuState));
	
	if (argc != 3){
		printf("only 2 Arguments are allowed (found %u)!\n", (argc - 1));
		return -1;
	}
	
	if (chdir(argv[2])){
		printf("Couldn't open Path to \"%s\"!\n", argv[2]);
	}
	
	// Create the System's Memory and load a ROM file into it
	printf("Allocating System Memory!\n");
	uint8_t *memory = malloc(MEM_SIZE * sizeof(uint8_t));
	if (memory == NULL) return -1;
	
	uint32_t ROMsize = readROM(memory, MEM_SIZE, ROM_START, argv[1]);
	if (!ROMsize) return -1;	// If no ROM was loaded, exit immediately
	
	// Initialize the CPU struct
	cpuInit(&CPU0, memory, MEM_SIZE, IO_START, IO_SIZE, cpu0ReadIO, cpu0WriteIO);
	
	// Then run the CPU for 10"ms" at a time
	int32_t rcyc = CLOCK;
	while(1){
		// Run for a set amount of Cycles
		rcyc = cpuExecute(&CPU0, rcyc);
		
		// If a STP Instruction was executed, exit the program
		if (chkSTP(CPU0)) break;
		
		// If the "fileCmd" Byte was set, handle it
		if (fileCmd) fileIO(&CPU0);
		
		// Send the periodic IRQ and increment the timer
		cpuSendIRQ(CPU0);
		timer.l++;
		
		// Then calculate how many cycles to run for the next iteration
		rcyc = CLOCK + rcyc;
		
	}
	
	free(memory);
	return 0;
}








uint8_t cpu0ReadIO(uint32_t addr){
	uint8_t ad = addr & 0x000000FF;
	uint8_t tmp;
	
	//printf("IO Read at 0x%02X!\n", ad);
	switch(ad){
		case 0:		// CTRL Register
		return (_kbhit()) ? 0x00 : 0x80;
		
		case 1:		// UART
			if (!_kbhit()) return 0;
		return _getch();
		
		case 4:		// 32-bit Timer, reading the low Byte saves the whole Timer value
			tmpTimer.l = timer.l;
		return tmpTimer.bl;
		
		case 5:
		return tmpTimer.bm;
		
		case 6:
		return tmpTimer.bh;
		
		case 7:
		return tmpTimer.bx;
		
		case 0x81:	// File IO Response Byte (0x00 = pending, 0x01 = success, >0x7F = error)
			tmp = fileResponse;
			fileResponse = 0;
		return tmp;
		
		case 0x82:	// Low Byte of structure Pointer
		return filePtr.bl;
		
		case 0x83:	// Middle Byte of structure Pointer
		return filePtr.bm;
		
		case 0x84:	// High Byte of structure Pointer
		return filePtr.bh;
		
		case 0x85:	// Over High Byte of structure Pointer
		return filePtr.bx;
		
		default:
		return 0;
	}
}

void cpu0WriteIO(uint32_t addr, uint8_t val){
	uint8_t ad = addr & 0x000000FF;
	
	// printf("  (IO Write at 0x%02X, value: 0x%02X)  ", ad, val);
	switch(ad){
		case 1:		// UART
			putch(val);
		break;
		
		case 0x80:	// Debug Printing
			//setDebug(val);
		break;
		
		// File IO Control Registers
		case 0x81:	// Control Byte for File IO
			fileCmd = val;
		break;
		
		case 0x82:	// Low Byte of structure Pointer
			filePtr.bl = val;
		break;
		
		case 0x83:	// Middle Byte of structure Pointer
			filePtr.bm = val;
		break;
		
		case 0x84:	// High Byte of structure Pointer
			filePtr.bh = val;
		break;
		
		case 0x85:	// Over High Byte of structure Pointer
			filePtr.bx = val;
		break;
		
		default:
			
		break;
	}
}

enum{
	FCMD_NONE,
	FCMD_OPEN,
	FCMD_CLOSE,
	FCMD_DELETE,
	FCMD_TELL,
	FCMD_SEEK0,
	FCMD_SEEK1,
	FCMD_READ,
	FCMD_WRITE
};

#define FIO_SUCCESS		1U
#define FIO_EOF			2U
#define FIO_ERROR		255U

void fileIO(cpuState *CPU){
	cint32_t tmp0, tmp1, tmp2;
	
	switch(fileCmd){
		
		// Open a File
		// "filePtr" points to 2 consecutive strings, the first specifies the mode to open the File in,
		// and the second is the path of the File to be opened
		case FCMD_OPEN:
			tmp0.l = filePtr.l & 0x00FFFFFF;
			
			// Calculate the address of the second string
			tmp1.l = (filePtr.l + strlen((char*)&MEM[tmp0.l]) + 1) & 0x00FFFFFF;
			
			// printf("[FIO] Opening File \"%s\" with mode \"%s\"\n", (char*)&MEM[tmp1.l], (char*)&MEM[tmp0.l]);
			
			fp = fopen((char*)&MEM[tmp1.l], (char*)&MEM[tmp0.l]);
			
			if (fp == NULL){
				fileResponse = FIO_ERROR;
			}else{
				fileResponse = FIO_SUCCESS;
			}
		break;
		
		// Close a File
		// No pointer necessary
		case FCMD_CLOSE:
			// printf("[FIO] Closing File\n");
			fclose(fp);
			fileResponse = FIO_SUCCESS;
		break;
		
		// Delete a File
		// "filePtr" points to a string containg the path of the File to be deleted
		case FCMD_DELETE:
			tmp0.l = filePtr.l & 0x00FFFFFF;
			
			// printf("[FIO] Deleting File \"%s\"\n", (char*)&MEM[tmp0.l]);
			
			if (remove((char*)&MEM[tmp0.l])){
				fileResponse = FIO_ERROR;
			}else{
				fileResponse = FIO_SUCCESS;
			}
		break;
		
		// Get the current File Pointer
		// This value is directly written to "filePtr"
		case FCMD_TELL:
			ftell(fp);
			// printf("[FIO] Get File Pointer (%u)\n", filePtr.l);
			fileResponse = FIO_SUCCESS;
		break;
		
		// Set the File Pointer (relative to the start of the file)
		// This value is directly read from "filePtr"
		case FCMD_SEEK0:
			// printf("[FIO] Set File Pointer (%u from Start)\n", filePtr.l);
			if (fseek(fp, filePtr.l, SEEK_SET)){
				fileResponse = FIO_ERROR;
			}else{
				fileResponse = FIO_SUCCESS;
			}
		break;
		
		// Set the File Pointer (relative to the end of the file)
		// This value is directly read from "filePtr"
		case FCMD_SEEK1:
			// printf("[FIO] Set File Pointer (%u from End)\n", filePtr.l);
			filePtr.l = fseek(fp, filePtr.l, SEEK_END);
			fileResponse = FIO_SUCCESS;
		break;
		
		// Read from File
		// "filePtr" points to a 24-bit word, specifying how many bytes to read,
		// Followed by a 24-bit Pointer to where the data should be written to
		case FCMD_READ:
			// Amount of Bytes to read
			tmp0.bl = MEM[filePtr.l];
			tmp0.bm = MEM[filePtr.l + 1];
			tmp0.bh = MEM[filePtr.l + 2];
			tmp0.bx = 0;
			
			// Where to write them to
			tmp1.bl = MEM[filePtr.l + 3];
			tmp1.bm = MEM[filePtr.l + 4];
			tmp1.bh = MEM[filePtr.l + 5];
			tmp1.bx = 0;
			
			// printf("[FIO] Read %u Bytes from File to Address: $%06X\n", tmp0.l, tmp1.l);
			
			tmp2.l = fread(&MEM[tmp1.l], 1, tmp0.l, fp);
			
			// Also write back the read amount of Bytes to filePtr
			filePtr.l = tmp2.l;
			
			if (tmp2.l == tmp0.l){
				fileResponse = FIO_SUCCESS;
			}else{
				fileResponse = FIO_EOF;
			}
		break;
		
		// Write to File
		// "filePtr" points to a 24-bit word, specifying how many bytes to write,
		// Followed by a 24-bit Pointer to where the data should be read from
		case FCMD_WRITE:
			// Amount of Bytes to writes
			tmp0.bl = MEM[filePtr.l];
			tmp0.bm = MEM[filePtr.l + 1];
			tmp0.bh = MEM[filePtr.l + 2];
			tmp0.bx = 0;
			
			// Where to read them from
			tmp1.bl = MEM[filePtr.l + 3];
			tmp1.bm = MEM[filePtr.l + 4];
			tmp1.bh = MEM[filePtr.l + 5];
			tmp1.bx = 0;
			
			// printf("[FIO] Writes %u Bytes from Address: $%06X to File\n", tmp0.l, tmp1.l);
			
			tmp2.l = fwrite(&MEM[tmp1.l], 1, tmp0.l, fp);
			
			// Also write back the written amount of Bytes to filePtr
			filePtr.l = tmp2.l;
			
			if (tmp2.l == tmp0.l){
				fileResponse = FIO_SUCCESS;
			}else{
				fileResponse = FIO_ERROR;
			}
		break;
		
		default:
			fileResponse = FIO_ERROR;
		break;
	}
	
	fileCmd = 0;
}









