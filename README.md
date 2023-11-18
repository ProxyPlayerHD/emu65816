# emu65816
An Emulator Library for the 65816 written in C

# Note

This Emulator is mostly functional.<br>
Instructions like `MVP` and `MVN` are not implemented, Abort is also not supported. and Decimal Mode is only partly functional (ADC works fine but SBC doesn't, plus the V flag is often incorrect in Decimal Mode).<br>
There could also still be some issues in Emulation mode, though the tests haven't shown anything. Still i'd recommend just switching to Native mode as soon as you can.

Besides those, everything else should work fine.

# General Description

This Library is intended to be used to implement 65816 based Emulators, be that of either existing systems or custom ones.<br>
For that the library handles the entire processor, including things like interrupt handling, and allows for custom IO handlers that get called when the processor accesses memory in a specific (user defined) region.<br>
The entire processor and all it's internals is stored in a single struct. this is to avoid the need for any blobal variables, which makes it much easier to run multiple processors at once in the same program.

# Function list
Plus small description

`void cpuInit(cpuState* CPU, uint8_t* memory, uint32_t memSize, uint32_t ioAddress, uint32_t ioSize, uint8_t (*ioRead)(uint32_t), void (*ioWrite)(uint32_t, uint8_t))`<br>
Initializes the CPU struct, should be done AFTER loading a ROM/binary image into memory as this function also fetches the reset vector to preload the PC.

`int32_t cpuExecute(cpuState* CPU, int32_t cycles)`<br>
Runs the specified CPU for a specified amount of cycles, returns the difference between the requested amount of cycles and how many it actually ran for.<br>
Positive return value means it ran fewer cycles than requested, negative return value means it ran more cycles than requested.

`chkSTP(cpuState CPU)`<br>
Returns the value of the STP flag of the specified CPU (note it's not a pointer to the CPU struct).<br>
This flag is only set if the CPU executed a STP instruction. After which it can only be reset by calling `cpuInit` again.

`chkWAI(cpuState CPU)`<br>
Returns the value of the WAI flag of the specified CPU (note it's not a pointer to the CPU struct).<br>
This flag is set if the CPU executed a WAI instruction with no pending interrupt. Simply sending the CPU an interrupt will clear this flag and continue execution.

`cpuSendIRQ(c)`<br>
`cpuSendNMI(c)`<br>
These 2 functions send an interrupt to the specified CPU (note it's not a pointer to the CPU struct).<br>
As expected, once `cpuExecute` is run afterwards it will handle the interrupt like on real hardware (IRQ only if the I flag is cleared).<br>
Do note that NMI has a higher priority, meaning if you send an IRQ and then an NMI, the NMI will overwrite the IRQ completely. and sending an IRQ after an MNI will have no effect.

`__EMU_LITTLE_ENDIAN`<br>
Not a function, but this symbol should be defined before including the emu65816.h file if the Library is used on a Little Endian System (like x86).<br>
This is only important for the 2 new data types called `cint16_t` and `cint32_t`. which are just `uin16_t` and `uint32_t` but with unions to access indivitual Bytes and change signees without casting or bit shifting and masking.<br>
Also note that the pre-made `emu65816.a` uses little endian, so if you need a big endian version you have remove that define from `emu65816.c` and build it yourself.

# Building

So simple i didn't even bother to create a Makefile for it:

```
gcc emu65816.c -Wall -O2 -c -o emu65816.o
ar rcs emu65816.a emu65816.o
```

And linking it with any program you do, just include it using `-l:emu65816.a`<br>
Though do note that `emu65816_library.h` is only intended for creating the library, user programs should only use the `emu65816.h` file.

# Basic Setup

This section show how to create a very bare bones setup using the library.

---
### Step 1: Make some IO handler functions

The library requires 2 user defined functions for handling IO accesses, it also needs to know where in memory the IO block is located and how large it is.<br>
The IO block is simply a region of memory where if the processor accesses an address within that region, one of the IO handler functions is called and given the accessed address relative to the start of the IO block. (ie: accessed address - IO block base address)
the name of these functions is unimportant but the input parameters and return types have to be as shown:<br>
for IO Read it should be: `uint8_t funcName(uint32_t address);`<br>
and for IO Write it's: `void funcName(uint32_t address, uint8_t inputValue)`<br>
Some example functions with a very simple UART like interface to the terminal:

```
#include <conio.h>                      // Requires conio

#define ioBase        0x010000          // IO Starts at the start of Bank 1
#define ioSize        0x000100          // And is 256 Bytes large

uint8_t readIO(uint32_t addr){
    switch(addr){
        case 0:                         // Status Register, returns 0 if there are characters to read from the "UART"
        return (_kbhit()) ? 0x00 : 0xFF;
        
        case 1:                         // "UART" Input
            if (!_kbhit()) return 0;    // Return early if there is nothing to read, otherwise it'll just get stuck
        return _getch();                // If there is something to read, return it
        
        default:                        // Everything else just returns 0
        return 0;
    }
}

void writeIO(uint32_t addr, uint8_t val){
    
    switch(addr){
        case 1:                         // "UART" Output
            putch(val);                 // Print the character
        break;
        
        default:                        // Everything else just doesn't do anything
        break;
    }
}
```
---
### Step 2: Allocate some Memory and preload it with code

the Emulator needs RAM to work and code to execute, so you need to give it some Memory (in form a pointer) and load a ROM image or similar into it.<br>
You can either use a static array of type `uint8_t` or use malloc/calloc.<br>
I feel like i shouldn't need to show how to allocate memory and load a file into it. But for the sake of completion:

```
#define memSize            (1024U * 1024U)    // 1MB of RAM
#define romAddress         0x00C000           // "ROM" starts at 0xC000
#define romSize            16384U             // 16kB of ROM

uint8_t *memory = malloc(memSize);
FILE *fp = fopen("path/to/rom.bin", "rb");
fread((memory + romAddress), 1U, romSize, fp);

// Note that there is no error checking/handling, which you should absolutely do
```
---
### Step 3: Declare the cpuState struct and initialize it

the Emulated CPU needs a home, that being the CPU struct.<br>
The struct holds all necessary parts of the processor for it to function, like a pointer to memory, all registers, etc.<br>
But since you never want the user to manually poke around complex structs like this, the `cputInit` function exists.

It takes the following input parameters in order:<br>
* A pointer to a CPU struct
* A pointer to memory
* The size of the memory array in Bytes
* The starting address for the IO block
* The size of the IO block in Bytes
* A pointer to an IO Read function
* And a pointer to an IO Write function.

so for our little example it would look something like this:

```
cpuState CPU;

cpuInit(&CPU, memory, memSize, ioBase, ioSize, readIO, writeIO);
```
---
### Step 4: Run the Emulator

Now all that's left is running the Emulated CPU, which is done through the `cpuExecute` function.<br>
This function takes a pointer to a CPU struct and some amount of CPU cycles it should run for.<br>
It returns the difference between how many cycles it was supposed to run for, and how many cycles it actually ran for. If the return value is negative it ran longer than requested, and if it's positive it ran for fewer cycles than requested.<br>
This is intended to be used to have it run at a specific "Clock speed" by running it for an average amount of cycles per some amount of real time.<br>
Also this allows the rest of the Emulator to run every once in a while, which is useful for drawing graphics, playing sound, sending periodic interrupts, or just checking if the CPU is even running anymore. (check for a STP instruction using `chkSTP(cpuState)`)

Example of a 10MHz CPU with a 1ms periodic interrupt:

```
// Note that this will defineitly run faster than 10MHz due to lack of time keeping function in this example
// but to the Emulated CPU, it will feel like 10MHz (assuming the code knows that interrupts happen every millisecond)

#define speed        10000U        // Cycles per millisecond

int32_t retCycles; cycles = speed;

while(chkSTP(CPU)){
    retCycles = cpuExecute(&CPU, cycles);
    
    cpuSendIRQ(CPU);
    
    cycles = speed + retCycles;    // If the CPU executed fewer cycles than required last time, execute more next time. and vice versa
}
```

And that's basically it!








