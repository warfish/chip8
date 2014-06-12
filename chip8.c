/*
 * =====================================================================================
 *
 *       Filename:  chip8.c
 *
 *    Description:  chip8 cpu implementation
 *
 *        Version:  1.0
 *        Created:  07/24/2012 15:34:58
 *
 * =====================================================================================
 */

#include "chip8.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>


// Fetch next opcode
//#define CHIP8_FETCH(__chip8__) 				((uint16_t)((__chip8__)->mem[(__chip8__)->PC++] << 8) | (__chip8__)->mem[(__chip8__)->PC++])

// Advace by a number of opcodes
#define CHIP8_SKIP(__chip8__, __ops__) 		((__chip8__)->PC += CHIP8_OPCODE_SIZE * (__ops__))
#define CHIP8_NEXT(__chip8__)				CHIP8_SKIP(__chip8__, 1)

// Opcode operand unpacking
#define CHIP8_REGX_OPERAND(__opcode__) 		(((__opcode__) & 0x0F00) >> 8)
#define CHIP8_REGY_OPERAND(__opcode__) 		(((__opcode__) & 0x00F0) >> 4)
#define CHIP8_ADDR_OPERAND(__opcode__) 		((__opcode__) & 0x0FFF)
#define CHIP8_CONST4_OPERAND(__opcode__)	((__opcode__) & 0x000F)
#define CHIP8_CONST8_OPERAND(__opcode__)	((__opcode__) & 0x00FF)

// Stack 
#define CHIP8_PUSH(__chip8__, __value__)	((__chip8__)->mem[(__chip8__)->SP++] = (__value__))
#define CHIP8_POP(__chip8__)				((__chip8__)->mem[(__chip8__)->SP--])


static uint8_t g_chip8_fontset[] =
{ 
  0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
  0x20, 0x60, 0x20, 0x20, 0x70, // 1
  0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
  0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
  0x90, 0x90, 0xF0, 0x10, 0x10, // 4
  0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
  0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
  0xF0, 0x10, 0x20, 0x40, 0x40, // 7
  0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
  0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
  0xF0, 0x90, 0xF0, 0x90, 0x90, // A
  0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
  0xF0, 0x80, 0x80, 0x80, 0xF0, // C
  0xE0, 0x90, 0x90, 0x90, 0xE0, // D
  0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
  0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};


// draw sprite at given location, with a given height (width is always 8 pixels).
// sprite data is stored at addr.
static void draw_sprite(struct chip8_t* chip8, unsigned x, unsigned y, unsigned height, uint16_t addr)
{
/* 
	uint8_t* src = chip8->mem + addr;
	uint8_t* dst = chip8->mem + CHIP8_VIDEO_OFFSET + y * 8 + x;

	chip8->V[CHIP8_VF] = 0; // Flag will be set if there is a collision
	
	while (heigth--)
	{
		// sprite lines are drawn by xoring existing video memory data with the new 8-bit pixel line. each pixel is represented by 1 bit.
		// if a bit at existing video memory will be flipped this means a collision that needs to be marked in VF.
		// XOR will flip a bit to 0 only if both bits are set so we will mask those out and see if any are left
		chip8->V[CHIP8_VF] = (*src & *dst) != 0;
		*dst++ ^= *src++;
	}
*/
	uint8_t* src = chip8->mem + addr;

	for (unsigned yline = 0; yline < height; ++yline)
	{
		for (unsigned xline = 0; xline < 8; ++xline)
		{
			if (*src & (0x80 >> xline))
			{
				chip8->V[CHIP8_VF] = (chip8->video_mem[yline + y][xline + x] == 1);
				chip8->video_mem[yline + y][xline + x] ^= 1;
			}
		}
	}

	chip8->video_update = 1;
/*
	for (unsigned y = 0; y < height; ++y)
  	{
    	line = chip8->mem[addr + y];
    	for(int xline = 0; xline < 8; xline++)
    	{
      		if((pixel & (0x80 >> xline)) != 0)
      		{
        		if(chip8->mem[CHIP8_VIDEO_OFFSET + (x + xline + ((y + yline) * 64))] == 1)
				{
          			chip8->V[CHIP8_VF] = 1;                                 
        		}

				chip8->mem[CHIP8_VIDEO_OFFSET + (x + xline + ((y + yline) * 64))] ^= 1;
      		}
    	}
  	}
*/
}


int chip8_init(struct chip8_t* chip8)
{
	memset(chip8, 0, sizeof(*chip8));

	// Set default register values	
	chip8->PC = CHIP8_INIT_PC;
	chip8->SP = 0;//CHIP8_STACK_OFFSET;
	
	// Set default key states
	for (unsigned i = 0; i < CHIP8_TOTAL_KEYS; ++i)
	{
		CHIP8_CLEAR_KEY(chip8->input_state, i);
	}

	// Load fontset
	memcpy(chip8->mem, g_chip8_fontset, sizeof(g_chip8_fontset));

	return 0;
}

int chip8_exec(struct chip8_t* chip8, uint16_t opcode)
{
	switch (opcode & 0xF000)
	{

	case 0x0000: /* various */
		switch (opcode & 0x00FF)
		{
		case 0x0000: /* Not used in modern interpreters */
			return ENOTSUP;

		case 0x00E0: /* clear screen */
			//memset(chip8->mem + CHIP8_VIDEO_OFFSET, 0, CHIP8_VIDEO_MEM_SIZE);
			memset(chip8->video_mem, 0, sizeof(chip8->video_mem));
			break;

		case 0x00EE: /* return */
	/*  
			chip8->PC = CHIP8_POP(chip8);
			chip8->PC |= CHIP8_POP(chip8) << 8;
	*/
			chip8->PC = chip8->call_stack[chip8->SP--];
			break;

		default:
			return EINVAL;
		}
		break;

	case 0x1000: /* jump to NNN */
		chip8->PC = CHIP8_ADDR_OPERAND(opcode);
		break;

	case 0x2000: /* call to NNN */
		// Check that we have enough stack depth left
	/*  
		if ((chip8->SP - CHIP8_STACK_OFFSET) >= (CHIP8_STACK_DEPTH * 2))
		{
			return ENOMEM;
		}

		printf("Calling 0x%x, return address 0x%x\n", CHIP8_ADDR_OPERAND(opcode), chip8->PC);

		CHIP8_PUSH(chip8, chip8->PC & 0x00FF);
		CHIP8_PUSH(chip8, (chip8->PC & 0xFF00) >> 8);
*/
		printf("Calling 0x%x, return address 0x%x\n", CHIP8_ADDR_OPERAND(opcode), chip8->PC);
		chip8->call_stack[++chip8->SP] = chip8->PC;
		chip8->PC = CHIP8_ADDR_OPERAND(opcode);
		break;

	case 0x3000: /* skip next insturction if VX == NN */
		CHIP8_SKIP(chip8, (chip8->V[CHIP8_REGX_OPERAND(opcode)] == CHIP8_CONST8_OPERAND(opcode)));
		break;

	case 0x4000: /* skip next instruction if VX != NN */
		CHIP8_SKIP(chip8, (chip8->V[CHIP8_REGX_OPERAND(opcode)] != CHIP8_CONST8_OPERAND(opcode)));
		break;

	case 0x5000: /* skip next instruction if VX == VY */
		CHIP8_SKIP(chip8, (chip8->V[CHIP8_REGX_OPERAND(opcode)] == chip8->V[CHIP8_REGY_OPERAND(opcode)]));
		break;

	case 0x9000: /* skip next instruction if VX != VY */
		CHIP8_SKIP(chip8, (chip8->V[CHIP8_REGX_OPERAND(opcode)] != chip8->V[CHIP8_REGY_OPERAND(opcode)]));
		break;

	case 0x6000: /* VX = NN */
		chip8->V[CHIP8_REGX_OPERAND(opcode)] = CHIP8_CONST8_OPERAND(opcode);
		break;

	case 0x7000: /* VX += NN, carry?? */
		chip8->V[CHIP8_REGX_OPERAND(opcode)] += CHIP8_CONST8_OPERAND(opcode);
		printf("Register %d[0x%x]\n", CHIP8_REGX_OPERAND(opcode), chip8->V[CHIP8_REGX_OPERAND(opcode)]);
		break;

	case 0x8000: /* various */
		switch (opcode & 0x000F)
		{
		case 0x0000: /* V[X] = V[Y] */
			chip8->V[CHIP8_REGX_OPERAND(opcode)] = chip8->V[CHIP8_REGY_OPERAND(opcode)];
			break;

		case 0x0001: /* V[X] |= V[Y] */
			chip8->V[CHIP8_REGX_OPERAND(opcode)] |= chip8->V[CHIP8_REGY_OPERAND(opcode)];
			break;

		case 0x0002: /* v[x] &= v[y] */
			chip8->V[CHIP8_REGX_OPERAND(opcode)] &= chip8->V[CHIP8_REGY_OPERAND(opcode)];
			break;
				
		case 0x0003: /* v[x] ^= v[y] */
			chip8->V[CHIP8_REGX_OPERAND(opcode)] ^= chip8->V[CHIP8_REGY_OPERAND(opcode)];
			break;

		case 0x0004: /* v[x] += v[y], carry */
		{		
			int vx = CHIP8_REGX_OPERAND(opcode);
			int vy = CHIP8_REGY_OPERAND(opcode);

			chip8->V[CHIP8_VF] = chip8->V[vx] > (0xFF - (chip8->V[vy]));
			chip8->V[vx] += chip8->V[vy];
			break;
		}

		case 0x0005: /* V[X] -= V[Y], borrow */
		{
			int vx = CHIP8_REGX_OPERAND(opcode);
			int vy = CHIP8_REGY_OPERAND(opcode);

			chip8->V[CHIP8_VF] = chip8->V[vx] >= chip8->V[vy];
			chip8->V[vx] -= chip8->V[vy];
			break;
		}

		case 0x0006: /* V[X] >> 1, shifted bit into VF */
		{
			int vx = CHIP8_REGX_OPERAND(opcode);

			chip8->V[CHIP8_VF] = chip8->V[vx] & 0x1;
			chip8->V[vx] >>= 1;
			break;
		}

		case 0x0007: /* V[X] = V[Y] - V[X], borrow */
		{
			int vx = (opcode & 0x0F00) >> 8;
			int vy = (opcode & 0x00F0) >> 4;
				
			chip8->V[CHIP8_VF] = chip8->V[vy] >= chip8->V[vx];
			chip8->V[vx] = chip8->V[vy] - chip8->V[vx];
			break;
		}

		case 0x000E: /* V[X] << 1, shifted bit into VF */
		{
			int vx = CHIP8_REGX_OPERAND(opcode);

			chip8->V[CHIP8_VF] = (0 != (chip8->V[vx] & 0x80));
			chip8->V[vx] <<= 1;
			break;
		}

		default:
			return EINVAL;
		} // switch 0x8XXX
		break;		

	case 0xA000: /* I = NNN */
		chip8->I = CHIP8_ADDR_OPERAND(opcode);
		break;

	case 0xB000: /* jmp NNN + V0 */
		chip8->PC = CHIP8_ADDR_OPERAND(opcode) + chip8->V[0];
		break;

	case 0xC000: /* V[X] = rand() % NN */
		chip8->V[CHIP8_REGX_OPERAND(opcode)] = rand() % (CHIP8_CONST8_OPERAND(opcode) + 1);
		break;

	case 0xD000: /* draw sprite stored at I as 8 by N pixels at screen coords V[X]:V[Y] */
		draw_sprite(chip8, chip8->V[CHIP8_REGX_OPERAND(opcode)], chip8->V[CHIP8_REGY_OPERAND(opcode)], CHIP8_CONST4_OPERAND(opcode), chip8->I);
		break;

	case 0xE000: /* various */
		switch (opcode & 0x00FF)
		{
		case 0x009E: /* next if X is pressed */
			chip8->PC += 2 * CHIP8_IS_KEY_MARKED(chip8->input_state, chip8->V[(opcode & 0x0F00) >> 8]);
			break;

		case 0x00A1: /* next if X is NOT pressed */
			chip8->PC += 2 * !CHIP8_IS_KEY_MARKED(chip8->input_state, chip8->V[(opcode & 0x0F00) >> 8]);
			break;

		default:
			return EINVAL;
		} // switch 0xE000
		break;

	case 0xF000: /* various */
		switch (opcode & 0x00FF)
		{
		case 0x0007: /* Sets VX to the value of the delay timer. */
			chip8->V[CHIP8_REGX_OPERAND(opcode)] = chip8->delay_timer;
			break;

		case 0x000A: /* A key press is awaited, and then stored in VX. */
		{
			uint16_t key_state = chip8->input_state;
			while (key_state == chip8->input_state) 
				;

			for (int i = 0; i < CHIP8_TOTAL_KEYS; ++i)
			{
				if (CHIP8_IS_KEY_MARKED(chip8->input_state, i) && !CHIP8_IS_KEY_MARKED(key_state, i))
				{
					chip8->V[CHIP8_REGX_OPERAND(opcode)] = i;
				}
			}

			break;
		}

		case 0x0015: /* Sets the delay timer to VX. */
			chip8->delay_timer = chip8->V[CHIP8_REGX_OPERAND(opcode)];
			break;

		case 0x0018: /* Sets the sound timer to VX. */
			chip8->sound_timer = chip8->V[CHIP8_REGX_OPERAND(opcode)];
			break;
				
		case 0x001E: /* Adds VX to I. VF if range overflow */
		{
			int vx = CHIP8_REGX_OPERAND(opcode);
			chip8->V[CHIP8_VF] = (chip8->V[vx] + chip8->I) > 0xFFF;
			chip8->I += chip8->V[vx];
			break;
		}
	
		case 0x0029: /* Sets I to the location of the sprite for the character in VX. 
						Characters 0-F (in hexadecimal) are represented by a 4x5 font. */
			chip8->I = chip8->V[CHIP8_REGX_OPERAND(opcode)] * 5;
			break;

		case 0x0033: /* Stores the Binary-coded decimal representation of VX, 
						with the most significant of three digits at the address in I, 
						the middle digit at I plus 1, and the least significant digit at I plus 2. */
		{
			uint8_t value = chip8->V[CHIP8_REGX_OPERAND(opcode)];
			chip8->mem[chip8->I + 2] 	= value % 10; value /= 10;
			chip8->mem[chip8->I + 1] 	= value % 10; value /= 10;
			chip8->mem[chip8->I] 		= value % 10;
			break;
		}
			
		case 0x0055: /* Stores V0 to VX in memory starting at address I. */
		{
			int vx = CHIP8_REGX_OPERAND(opcode);
			memcpy(&chip8->mem[chip8->I], &chip8->V[0], vx + 1);
			break;
		}

		case 0x0065: /* Fills V0 to VX with values from memory starting at address I. */
		{
			int vx = CHIP8_REGX_OPERAND(opcode);
			//memcpy(&chip8->V[0], &chip8->mem[chip8->I], vx + 1);
			for (unsigned i = 0; i <= vx; ++i)
			{
				chip8->V[i] = chip8->mem[chip8->I + i];
			}
			break;
		}

		default:
			return EINVAL;
		} // switch 0xF000
		break;

	default:
		return EINVAL;	
	} // switch opcode

	return 0;	

}

int chip8_tick(struct chip8_t* chip8)
{
	uint16_t opcode = (uint16_t) chip8->mem[chip8->PC++] << 8;
	opcode |= chip8->mem[chip8->PC++];

	printf("Executing 0x%x:0x%x\n", chip8->PC - 2, opcode);

	int rc = chip8_exec(chip8, opcode);
	
	if (rc == 0)
	{
		if (chip8->delay_timer)
			--chip8->delay_timer;

		if (chip8->sound_timer)
			--chip8->sound_timer;
	}

	return rc;
}

void chip8_release(struct chip8_t* chip8)
{
	memset(chip8, 0, sizeof(chip8));
}

