/*
 * =====================================================================================
 *
 *       Filename:  chip8.h
 *
 *    Description:  chip8 cpu interface
 *
 *        Version:  1.0
 *        Created:  07/24/2012 15:24:07
 *
 * =====================================================================================
 */

#ifndef CHIP8_CPU_H
#define CHIP8_CPU_H

#include <stdint.h>
#include <assert.h>

// Memory map offsets
#define CHIP8_ROM_OFFSET 	0x0
#define CHIP8_FONT_OFFSET 	0x50
#define CHIP8_RAM_OFFSET 	0x200
#define CHIP8_STACK_OFFSET 	0xEA0
#define CHIP8_VIDEO_OFFSET 	0xF00

#define CHIP8_RAM_SIZE		(CHIP8_STACK_OFFSET - CHIP8_RAM_OFFSET)

// Depth of call stack
#define CHIP8_STACK_DEPTH 16

// Index of the VX register that doubles as carry flag
#define CHIP8_VF 15

// Size of a single chip8 opcode
#define CHIP8_OPCODE_SIZE sizeof(uint16_t)

// PC start address
#define CHIP8_INIT_PC 0x200

// Video resolution 64 x 32
#define CHIP8_VIDEO_WIDTH 		64
#define CHIP8_VIDEO_HEIGHT 		32
#define CHIP8_VIDEO_MEM_SIZE	(CHIP8_VIDEO_HEIGHT * (CHIP8_VIDEO_WIDTH >> 3)) // Video mem size in bytes

// Font resolution 4 x 5
#define CHIP8_FONT_WIDTH		4
#define CHIP8_FONT_HEIGHT		5
#define CHIP8_FONT_BYTES		CHIP8_FONT_HEIGHT			

// Input keys
#define CHIP8_KEY_0 0
#define CHIP8_KEY_1 1
#define CHIP8_KEY_2 2
#define CHIP8_KEY_3 3
#define CHIP8_KEY_4 4
#define CHIP8_KEY_5 5
#define CHIP8_KEY_6 6
#define CHIP8_KEY_7 7
#define CHIP8_KEY_8 8 
#define CHIP8_KEY_9 9
#define CHIP8_KEY_A A
#define CHIP8_KEY_B B
#define CHIP8_KEY_C C
#define CHIP8_KEY_D D
#define CHIP8_KEY_E E
#define CHIP8_KEY_F F
#define CHIP8_TOTAL_KEYS 16

// Mark/clear/check input key as pressed
#define CHIP8_MARK_KEY(__state__, __key__) 			((__state__) |= (1 << (__key__)))
#define CHIP8_CLEAR_KEY(__state__, __key__) 		((__state__) &= ~(1 << (__key__)))
#define CHIP8_IS_KEY_MARKED(__state__, __key__) 	(((__state__) & (1 << (__key__))) != 0)


// Chip8 state
struct chip8_t
{
	uint8_t V[16];	// 16 general purpose registers, CHIP8_VF doubles as a carry flag
	uint16_t I;		// Special address register
	uint16_t PC;	// Program counter
	uint16_t SP;	// Stack pointer

	// Both timers count at 60hz
	uint16_t delay_timer;	// Delay timer used in games for delay actions
	uint16_t sound_timer;	// Sound timer used in games to sync bleeps

	uint16_t input_state;	// Set of CHIP8_KEY_XXX flags to represent each of the 16 keys' states

	uint8_t mem[0xFFF]; 	// Raw memory

	uint8_t video_mem[CHIP8_VIDEO_HEIGHT][CHIP8_VIDEO_WIDTH];	// 
	uint16_t call_stack[CHIP8_STACK_DEPTH];
	// Below are flags for the client 
	int video_update; 		// Video memory has been updated a number of times. Throw this flag when you've seen it
};


/**
 *	Init chip8 state
 * 	Sets everything to default values.
 */
int chip8_init(struct chip8_t* chip8);


/**
 * 	Execute next instruction
 */
int chip8_tick(struct chip8_t* chip8);

/**
 * 	Manually decode and execute specific instruction 
 */
int chip8_exec(struct chip8_t* chip8, uint16_t opcode);

/**
 * 	Mark input key as pressed or released
 * 	@param key 			Input key index
 * 	@param is_pressed 	boolean key state value, true if pressed
 */
static inline void chip8_set_key_state(struct chip8_t* chip8, unsigned key, int is_pressed)
{
	assert (key < CHIP8_TOTAL_KEYS);
	is_pressed != 0 ? CHIP8_MARK_KEY(chip8->input_state, key) : CHIP8_CLEAR_KEY(chip8->input_state, key);
}

/**
 * 	Return boolean key state 
 * 	@param key			Input key index
 */
static inline int chip8_get_key_state(struct chip8_t* chip8, unsigned key)
{
	assert (key < CHIP8_TOTAL_KEYS);
	return CHIP8_IS_KEY_MARKED(chip8->input_state, key);
}

/**
 * 	Release chip8 state
 */
void chip8_release(struct chip8_t* chip8);


#endif

