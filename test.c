/*
 * =====================================================================================
 *
 *       Filename:  test.c
 *
 *    Description:  chip8 cpu emulation unit tests.
 *    				tests each instruction execution and basic cpu emulation interface
 *
 *        Version:  1.0
 *        Created:  07/26/2012 20:40:38
 *
 * =====================================================================================
 */

#include "chip8.h"

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include <CUnit/Basic.h>


//////////////////////////////////////////////////////////////
//
//	Utils
//
//////////////////////////////////////////////////////////////


static int memisset(const void* mem, uint8_t value, size_t size)
{
	uint8_t* ptr = (uint8_t*) mem;
	while (size--)
	{
		if (*ptr++ != value)
		{
			return 0;
		}
	}

	return 1;
}


static uint8_t read_bcd(struct chip8_t* chip8, uint16_t addr)
{
	return (chip8->mem[addr] * 100) + (chip8->mem[addr + 1] * 10) + chip8->mem[addr + 2];
}


static void fill_with_random(struct chip8_t* chip8, unsigned last_v)
{
	for (unsigned i = 0; i <= last_v; ++i)
	{
		chip8->V[i] = rand() % 0xFF;
	}
}


//////////////////////////////////////////////////////////////
//
//	chip8.h interface tests
//
//////////////////////////////////////////////////////////////


// tests valid chip8 initial state
static void test_init(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	CU_ASSERT_EQUAL(CHIP8_INIT_PC, chip8.PC);
	CU_ASSERT_EQUAL(CHIP8_STACK_OFFSET, chip8.SP);
	CU_ASSERT_EQUAL(0, chip8.I);
	
	CU_ASSERT_EQUAL(0, chip8.delay_timer);
	CU_ASSERT_EQUAL(0, chip8.sound_timer);

	CU_ASSERT_FALSE(chip8.video_update);

	for (unsigned i = 0; i < 16; ++i)
	{	
		CU_ASSERT_EQUAL(0, chip8.V[i]);
	}
	
	for (unsigned i = 0; i < CHIP8_TOTAL_KEYS; ++i)
	{	
		CU_ASSERT_FALSE(CHIP8_IS_KEY_MARKED(chip8.input_state, i));
	}

	chip8_release(&chip8);
}


//////////////////////////////////////////////////////////////
//
//	chip8 opcode tests
//
//////////////////////////////////////////////////////////////


static void test_invalid_opcode(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));

	uint16_t PC = chip8.PC;
	CU_ASSERT_EQUAL(EINVAL, chip8_exec(&chip8, 0xFFFF));
	CU_ASSERT_EQUAL(PC, chip8.PC);
	
	chip8_release(&chip8);
}


// Not supported by modern interpreters
static void test_0000(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	CU_ASSERT_EQUAL(ENOTSUP, chip8_exec(&chip8, 0x0));

	chip8_release(&chip8);
}

// Clear video memory
static void test_00E0(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	memset(chip8.mem + CHIP8_VIDEO_OFFSET, 0xA, CHIP8_VIDEO_MEM_SIZE);
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, 0x00E0));
	CU_ASSERT_TRUE(memisset(chip8.mem + CHIP8_VIDEO_OFFSET, 0, CHIP8_VIDEO_MEM_SIZE));

	chip8_release(&chip8);
}

// Return
static void test_00EE(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	uint16_t SP = chip8.SP;
	uint16_t PC = chip8.PC;

	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, 0x2ABC)); // Call to 0xABC
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, 0x00EE)); // Return

	CU_ASSERT_EQUAL(SP, chip8.SP);
	CU_ASSERT_EQUAL(PC, chip8.PC);

	chip8_release(&chip8);
}

//case 0x1000: /* jump to NNN */
static void test_1NNN(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));

	uint16_t opcode = 0x1ABC;
	chip8_exec(&chip8, opcode);
	CU_ASSERT_EQUAL(0xABC, chip8.PC);

	chip8_release(&chip8);
}

static void test_2NNN(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));

	for (unsigned i = 0; i < CHIP8_STACK_DEPTH; ++i)
	{
		uint16_t SP = chip8.SP;
		uint16_t PC = chip8.PC;
		
		uint16_t opcode = 0x2F00 | i;
		CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
		CU_ASSERT_EQUAL(opcode & 0x0FFF, chip8.PC);
		CU_ASSERT_EQUAL(SP + 2, chip8.SP);
		CU_ASSERT_EQUAL(*(uint16_t*) (chip8.mem + SP), PC);
	}
	
	// No more stack space
	uint16_t opcode = 0x2F00;
	uint16_t SP = chip8.SP;
	uint16_t PC = chip8.PC;

	CU_ASSERT_EQUAL(ENOMEM, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL(PC, chip8.PC);
	CU_ASSERT_EQUAL(SP, chip8.SP);

	chip8_release(&chip8);
}

static void test_3XNN(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));

	uint16_t PC = chip8.PC;
	uint16_t opcode = 0x3AFF;

	// V[X] != NN
	chip8.V[0xA] = 0;
	chip8_exec(&chip8, opcode);
	CU_ASSERT_EQUAL(PC, chip8.PC);

	// V[X] == NN
	chip8.V[0xA] = 0xFF;
	chip8_exec(&chip8, opcode);
	CU_ASSERT_EQUAL(PC + 2, chip8.PC);

	chip8_release(&chip8);
}

static void test_4XNN(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));

	uint16_t PC = chip8.PC;
	uint16_t opcode = 0x4AFF;

	// V[X] == NN
	chip8.V[0xA] = 0xFF;
	chip8_exec(&chip8, opcode);
	CU_ASSERT_EQUAL(PC, chip8.PC);

	// V[X] != NN
	chip8.V[0xA] = 0;
	chip8_exec(&chip8, opcode);
	CU_ASSERT_EQUAL(PC + 2, chip8.PC);

	chip8_release(&chip8);
}

static void test_5XY0(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));

	uint16_t PC = chip8.PC;
	uint16_t opcode = 0x51A0;

	// V[X] != V[Y]
	chip8.V[1] = 1;
	chip8.V[0xA] = 2;
	chip8_exec(&chip8, opcode);
	CU_ASSERT_EQUAL(PC, chip8.PC);
	
	// V[X] == V[Y]
	chip8.V[1] = 2;
	chip8.V[0xA] = 2;
	chip8_exec(&chip8, opcode);
	CU_ASSERT_EQUAL(PC + 2, chip8.PC);

	chip8_release(&chip8);
}

static void test_9XY0(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));

	uint16_t PC = chip8.PC;
	uint16_t opcode = 0x91A0;

	// V[X] == V[Y]
	chip8.V[1] = 2;
	chip8.V[0xA] = 2;
	chip8_exec(&chip8, opcode);
	CU_ASSERT_EQUAL(PC, chip8.PC);

	// V[X] != V[Y]
	chip8.V[1] = 1;
	chip8.V[0xA] = 2;
	chip8_exec(&chip8, opcode);
	CU_ASSERT_EQUAL(PC + 2, chip8.PC);

	chip8_release(&chip8);
}

static void test_6XNN(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));

	for (unsigned i = 0; i < 16; ++i)
	{
		uint16_t opcode = 0x6000 | (i << 8) | i;
		CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
		CU_ASSERT_EQUAL(i, chip8.V[i]);
	}

	chip8_release(&chip8);
}

static void test_7XNN(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));

	uint16_t opcode = 0x7A23;
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL(0x23, chip8.V[0xA]);

	opcode = 0x7A32;
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL(0x23 + 0x32, chip8.V[0xA]);

	chip8_release(&chip8);
}

// V[X] = V[Y]
static void test_8XY0(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	chip8.V[0xA] = 0xA;
	chip8.V[0x1] = 0x0;
	uint16_t opcode = 0x81A0; // V[1] = V[A]
	
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));

	for (unsigned i = 0; i < 16; ++i)
	{
		if ((i == 0x1) || (i == 0xA))
		{
			CU_ASSERT_EQUAL(0xA, chip8.V[i]);
		}
		else 
		{
			CU_ASSERT_EQUAL(0, chip8.V[i]);
		}	
	}
	
	chip8_release(&chip8);
}

// V[X] |= V[Y]
static void test_8XY1(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	chip8.V[0xA] = 0x3;
	chip8.V[0x1] = 0x4;
	uint16_t opcode = 0x81A1; // V[1] |= V[A]
	
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));

	for (unsigned i = 0; i < 16; ++i)
	{
		if (i == 0x1)
		{
			CU_ASSERT_EQUAL(0x3 | 0x4, chip8.V[i]);
		}
		else if (i == 0xA)
		{
			CU_ASSERT_EQUAL(0x3, chip8.V[i]);
		}
		else 
		{
			CU_ASSERT_EQUAL(0, chip8.V[i]);
		}	
	}
	
	chip8_release(&chip8);
}

// V[X] &= V[Y]
static void test_8XY2(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	chip8.V[0xA] = 0x3;
	chip8.V[0x1] = 0x4;
	uint16_t opcode = 0x81A2; // V[1] &= V[A]
	
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));

	for (unsigned i = 0; i < 16; ++i)
	{
		if (i == 0x1)
		{
			CU_ASSERT_EQUAL(0x3 & 0x4, chip8.V[i]);
		}
		else if (i == 0xA)
		{
			CU_ASSERT_EQUAL(0x3, chip8.V[i]);
		}
		else 
		{
			CU_ASSERT_EQUAL(0, chip8.V[i]);
		}	
	}
	
	chip8_release(&chip8);
}

// V[X] ^= V[Y]
static void test_8XY3(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	chip8.V[0xA] = 0x3;
	chip8.V[0x1] = 0xFF;
	uint16_t opcode = 0x81A3; // V[1] ^= V[A]
	
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));

	for (unsigned i = 0; i < 16; ++i)
	{
		if (i == 0x1)
		{
			CU_ASSERT_EQUAL(0x3 ^ 0xFF, chip8.V[i]);
		}
		else if (i == 0xA)
		{
			CU_ASSERT_EQUAL(0x3, chip8.V[i]);
		}
		else 
		{
			CU_ASSERT_EQUAL(0, chip8.V[i]);
		}	
	}
	
	chip8_release(&chip8);
}

// V[X] += V[Y], VF set if carry
static void test_8XY4(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	// try without carry first
	chip8.V[0xA] = 0x1;
	chip8.V[0x1] = 0xF;
	uint16_t opcode = 0x81A4; // V[1] += V[A]
	
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL(0x1 + 0xF, chip8.V[1]);
	CU_ASSERT_EQUAL(0x1, chip8.V[0xA]);
	CU_ASSERT_EQUAL(chip8.V[CHIP8_VF], 0);


	// try with carry
	chip8.V[0xA] = 0x1;
	chip8.V[0x1] = 0xFF;
	
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL((uint8_t)(0x1 + 0xFF), chip8.V[1]);
	CU_ASSERT_EQUAL(0x1, chip8.V[0xA]);
	CU_ASSERT_EQUAL(chip8.V[CHIP8_VF], 1);


	// try without carry to see the flag get dropped
	chip8.V[0xA] = 0xC;
	chip8.V[0x1] = 0xE;
	
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL(0xC + 0xE, chip8.V[1]);
	CU_ASSERT_EQUAL(0xC, chip8.V[0xA]);
	CU_ASSERT_EQUAL(chip8.V[CHIP8_VF], 0);

	chip8_release(&chip8);
}

// V[X] -= V[Y], VF _NOT_ set if borrow
static void test_8XY5(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	// try without borrow
	chip8.V[0x1] = 0xF;
	chip8.V[0xA] = 0xA;
	uint16_t opcode = 0x81A5; // V[1] += V[A]
	
	CU_ASSERT_EQUAL(chip8.V[CHIP8_VF], 0);
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL(0xF - 0xA, chip8.V[1]);
	CU_ASSERT_EQUAL(0xA, chip8.V[0xA]);
	CU_ASSERT_EQUAL(chip8.V[CHIP8_VF], 1); // flag is set when there was no borrow

	// try with borrow
	chip8.V[0x1] = 0xA;
	chip8.V[0xA] = 0xF;
	
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL((uint8_t)(0xA - 0xF), chip8.V[1]);
	CU_ASSERT_EQUAL(0xF, chip8.V[0xA]);
	CU_ASSERT_EQUAL(chip8.V[CHIP8_VF], 0);

	chip8_release(&chip8);
}

// V[X] >>= 1, VF gets the lsb shifted off
static void test_8XY6(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	uint16_t opcode = 0x8A06;

	chip8.V[0xA] = 0x4; // LSB is 0	
	CU_ASSERT_EQUAL(chip8.V[CHIP8_VF], 0);
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL(0x4 >> 1, chip8.V[0xA]);
	CU_ASSERT_EQUAL(chip8.V[CHIP8_VF], 0);

	chip8.V[0xA] = 0xF; // LSB is 1
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL(0xF >> 1, chip8.V[0xA]);
	CU_ASSERT_EQUAL(chip8.V[CHIP8_VF], 1);

	chip8_release(&chip8);
}

// V[X] = V[Y] - V[X], VF _NOT_ set if borrow
static void test_8XY7(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	// try without borrow
	chip8.V[0x1] = 0xA;
	chip8.V[0xA] = 0xF;
	uint16_t opcode = 0x81A7; // V[1] += V[A]
	
	CU_ASSERT_EQUAL(chip8.V[CHIP8_VF], 0);
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL(0xF - 0xA, chip8.V[1]);
	CU_ASSERT_EQUAL(0xF, chip8.V[0xA]);
	CU_ASSERT_EQUAL(chip8.V[CHIP8_VF], 1); // flag is set when there was no borrow

	// try with borrow
	chip8.V[0x1] = 0xF;
	chip8.V[0xA] = 0xA;
	
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL((uint8_t)(0xA - 0xF), chip8.V[1]);
	CU_ASSERT_EQUAL(0xA, chip8.V[0xA]);
	CU_ASSERT_EQUAL(chip8.V[CHIP8_VF], 0);

	chip8_release(&chip8);
}

// V[X] <<= 1, VF gets the msb shifted off
static void test_8XYE(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	uint16_t opcode = 0x8A0E;

	chip8.V[0xA] = 0x0F; // MSB is 0	
	CU_ASSERT_EQUAL(chip8.V[CHIP8_VF], 0);
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL((uint8_t)(0x0F << 1), chip8.V[0xA]);
	CU_ASSERT_EQUAL(chip8.V[CHIP8_VF], 0);

	chip8.V[0xA] = 0xF0; // LSB is 1
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL((uint8_t)(0xF0 << 1), chip8.V[0xA]);
	CU_ASSERT_EQUAL(chip8.V[CHIP8_VF], 1);

	chip8_release(&chip8);
}

static void test_AXXX(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	uint16_t opcode = 0xAF00;
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL(0xF00, chip8.I);

	chip8_release(&chip8);
}

static void test_BXXX(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	uint16_t opcode = 0xBF00;
	chip8.V[0] = 0xAB;
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL(0xF00 + 0xAB, chip8.PC);

	chip8_release(&chip8);
}

static void test_CXXX(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	uint16_t opcode = 0xCA00;
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_TRUE(chip8.V[0xA] <= 0);

	opcode = 0xCA0F;
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_TRUE(chip8.V[0xA] <= 0xF);
	
	chip8_release(&chip8);
}

static void test_DXYN(void)
{

}

static void test_EX9E(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	uint16_t opcode = 0xEA9E;
	uint16_t PC = chip8.PC;
	chip8.V[0xA] = 0xA;
	
	// Key not pressed
	chip8_set_key_state(&chip8, 0xA, 0);
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL(PC, chip8.PC);

	// Key pressed
	chip8_set_key_state(&chip8, 0xA, 1);
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL(PC + 2, chip8.PC);
	
	chip8_release(&chip8);
}

static void test_EXA1(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	uint16_t opcode = 0xEAA1;
	uint16_t PC = chip8.PC;
	chip8.V[0xA] = 0xA;
	
	// Key pressed
	chip8_set_key_state(&chip8, 0xA, 1);
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL(PC, chip8.PC);

	// Key not pressed
	chip8_set_key_state(&chip8, 0xA, 0);
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL(PC + 2, chip8.PC);
	
	chip8_release(&chip8);
}

static void test_FX07(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	uint16_t opcode = 0xFA07;
	chip8.delay_timer = 0xA;
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL(chip8.V[0xA], chip8.delay_timer);

	chip8_release(&chip8);
}

static struct chip8_t* g_FX0A_chip8;
static void alarm_handler(int signo)
{
	chip8_set_key_state(g_FX0A_chip8, 0xA, 1);
}

// FX0A - wait for a key press and register it in VX
static void test_FX0A(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	// Instruction will block for a key press event, use alarm signal to actually send it
	g_FX0A_chip8 = &chip8;
	signal(SIGALRM, alarm_handler);
	alarm(1);

	uint16_t opcode = 0xFA0A;
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL(chip8.V[0xA], 0xA);	

	chip8_release(&chip8);
}

static void test_FX15(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	uint16_t opcode = 0xFA15;
	chip8.V[0xA] = 0xA;
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL(chip8.V[0xA], chip8.delay_timer);

	chip8_release(&chip8);
}

static void test_FX18(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	uint16_t opcode = 0xFA18;
	chip8.V[0xA] = 0xA;
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL(chip8.V[0xA], chip8.sound_timer);

	chip8_release(&chip8);
}

static void test_FX1E(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	uint16_t opcode = 0xFA1E;

	// Overflow range
	chip8.V[0xA] = 0xA;
	chip8.I = 0xFFF;
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL((uint16_t)(chip8.V[0xA] + 0xFFF), chip8.I);
	CU_ASSERT_EQUAL(chip8.V[CHIP8_VF], 1);

	// No overflow
	chip8.V[0xA] = 0xA;
	chip8.I = 0x0FF;
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	CU_ASSERT_EQUAL((uint16_t)(chip8.V[0xA] + 0x0FF), chip8.I);
	CU_ASSERT_EQUAL(chip8.V[CHIP8_VF], 0);

	chip8_release(&chip8);
}

static void test_FX29(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	for (unsigned i = 0; i < CHIP8_TOTAL_KEYS; ++i)
	{	
		uint16_t opcode = 0xF029 | (i << 8);
		CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
		CU_ASSERT_EQUAL(chip8.I, CHIP8_FONT_OFFSET + i * CHIP8_FONT_BYTES);
	}

	chip8_release(&chip8);
}

static void test_FX33(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	uint16_t opcode = 0xFA33;
	uint8_t dec = 0;
	chip8.I = 0x200;

	// Store 0
	chip8.V[0xA] = 0;
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	dec = read_bcd(&chip8, chip8.I);
	CU_ASSERT_EQUAL(dec, chip8.V[0xA]);

	// Store single digit value
	chip8.V[0xA] = 7;
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	dec = read_bcd(&chip8, chip8.I);
	CU_ASSERT_EQUAL(dec, chip8.V[0xA]);
	
	// Store double digit value
	chip8.V[0xA] = 42;
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	dec = read_bcd(&chip8, chip8.I);
	CU_ASSERT_EQUAL(dec, chip8.V[0xA]);

	// Store triple digit value
	chip8.V[0xA] = 201;
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	dec = read_bcd(&chip8, chip8.I);
	CU_ASSERT_EQUAL(dec, chip8.V[0xA]);

	// Store maximum value
	chip8.V[0xA] = 0xFF;
	CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));
	dec = read_bcd(&chip8, chip8.I);
	CU_ASSERT_EQUAL(dec, chip8.V[0xA]);

	chip8_release(&chip8);
}

static void test_FX55_FX65(void)
{
	struct chip8_t chip8;
	CU_ASSERT_EQUAL(0, chip8_init(&chip8));
	
	chip8.I = 0x200;
	for (unsigned i = 0; i < 16; ++i)
	{
		// FX55 - put register values in memory
		uint16_t opcode = 0xF055 | (i << 8);

		fill_with_random(&chip8, i);
		CU_ASSERT_EQUAL(0, chip8_exec(&chip8, opcode));

		for (unsigned j = 0; j < 16; ++j)
		{
			if (j <= i)
			{
				CU_ASSERT_EQUAL(chip8.V[j], chip8.mem[chip8.I + j]);
			} 
			else
			{	
				CU_ASSERT_EQUAL(chip8.V[j], 0);
			}
		}


		// FX65 - read values back from the memory
		struct chip8_t copy;
		memcpy(&copy, &chip8, sizeof(copy));
		memset(copy.V, 0, sizeof(copy.V));

		opcode = 0xF065 | (i << 8);
		CU_ASSERT_EQUAL(0, chip8_exec(&copy, opcode));

		for (unsigned j = 0; j < 16; ++j)
		{
			if (j <= i)
			{
				CU_ASSERT_EQUAL(copy.V[j], chip8.V[j]);
			}
			else
			{	
				CU_ASSERT_EQUAL(copy.V[j], 0);
			}
		}
	}
	
	chip8_release(&chip8);
}

int main(void)
{
	CU_pSuite pSuite = NULL;

   	/* initialize the CUnit test registry */
   	if (CUE_SUCCESS != CU_initialize_registry())
    	return CU_get_error();

   	/* add a suite to the registry */
   	pSuite = CU_add_suite("chip8 test suite", NULL, NULL);
   	if (NULL == pSuite) 
	{
      	CU_cleanup_registry();
      	return CU_get_error();
   	}

   	/* add the tests to the suite */
   	/* NOTE - ORDER IS IMPORTANT - MUST TEST fread() AFTER fprintf() */
   	(void)CU_add_test(pSuite, "chip8_init", test_init);
   	(void)CU_add_test(pSuite, "chip8_invalid_opcode", test_invalid_opcode);

	(void)CU_add_test(pSuite, "chip8_0000", test_0000);
	(void)CU_add_test(pSuite, "chip8_00E0", test_00E0);
	(void)CU_add_test(pSuite, "chip8_00EE", test_00EE);
	(void)CU_add_test(pSuite, "chip8_1NNN", test_1NNN);
	(void)CU_add_test(pSuite, "chip8_2NNN", test_2NNN);
	(void)CU_add_test(pSuite, "chip8_3XNN", test_3XNN);
	(void)CU_add_test(pSuite, "chip8_4XNN", test_4XNN);
	(void)CU_add_test(pSuite, "chip8_5XY0", test_5XY0);
	(void)CU_add_test(pSuite, "chip8_6XNN", test_6XNN);
	(void)CU_add_test(pSuite, "chip8_7XNN", test_7XNN);
	(void)CU_add_test(pSuite, "chip8_8XY0", test_8XY0);
	(void)CU_add_test(pSuite, "chip8_8XY1", test_8XY1);
	(void)CU_add_test(pSuite, "chip8_8XY2", test_8XY2);
	(void)CU_add_test(pSuite, "chip8_8XY3", test_8XY3);
	(void)CU_add_test(pSuite, "chip8_8XY4", test_8XY4);
	(void)CU_add_test(pSuite, "chip8_8XY5", test_8XY5);
	(void)CU_add_test(pSuite, "chip8_8XY6", test_8XY6);
	(void)CU_add_test(pSuite, "chip8_8XY7", test_8XY7);
	(void)CU_add_test(pSuite, "chip8_8XYE", test_8XYE);
	(void)CU_add_test(pSuite, "chip8_9XY0", test_9XY0);
	(void)CU_add_test(pSuite, "chip8_AXXX", test_AXXX);
	(void)CU_add_test(pSuite, "chip8_BXXX", test_BXXX);
	(void)CU_add_test(pSuite, "chip8_CXXX", test_CXXX);
	(void)CU_add_test(pSuite, "chip8_EX9E", test_EX9E);
	(void)CU_add_test(pSuite, "chip8_EXA1", test_EXA1);
	(void)CU_add_test(pSuite, "chip8_FX07", test_FX07);
	(void)CU_add_test(pSuite, "chip8_FX0A", test_FX0A); 
	(void)CU_add_test(pSuite, "chip8_FX15", test_FX15);
	(void)CU_add_test(pSuite, "chip8_FX18", test_FX18);
	(void)CU_add_test(pSuite, "chip8_FX1E", test_FX1E);
	(void)CU_add_test(pSuite, "chip8_FX29", test_FX29);
	(void)CU_add_test(pSuite, "chip8_FX33", test_FX33);
	(void)CU_add_test(pSuite, "chip8_FX55_FX65", test_FX55_FX65);

   	/* Run all tests using the CUnit Basic interface */
   	CU_basic_set_mode(CU_BRM_VERBOSE);
   	CU_basic_run_tests();
   	CU_cleanup_registry();
   	return CU_get_error();
}

