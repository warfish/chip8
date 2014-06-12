/*
 * =====================================================================================
 *
 *       Filename:  main.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08/03/2012 15:28:27
 *
 * =====================================================================================
 */

#include "chip8.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/fcntl.h>

#include <GLUT/glut.h> 

static struct chip8_t g_state;


////////////////////////////////////////////////////////////////////
//
//	Display and input
//
////////////////////////////////////////////////////////////////////


// Size of a single chip8 pixel in terms of square size
#define CHIP8_PIXEL_SIZE 10
#define CHIP8_screen_width CHIP8_VIDEO_WIDTH * CHIP8_PIXEL_SIZE
#define CHIP8_screen_height CHIP8_VIDEO_HEIGHT * CHIP8_PIXEL_SIZE

// Screen texture data
static uint8_t g_screen_buffer[CHIP8_VIDEO_HEIGHT][CHIP8_VIDEO_WIDTH][3]; 


// prepare screen
void setup_texture(void)
{
	// Clear screen
	for(int y = 0; y < CHIP8_VIDEO_HEIGHT; ++y)		
		for(int x = 0; x < CHIP8_VIDEO_WIDTH; ++x)
			g_screen_buffer[y][x][0] = g_screen_buffer[y][x][1] = g_screen_buffer[y][x][2] = 0;

	// Create a texture 
	glTexImage2D(GL_TEXTURE_2D, 0, 3, CHIP8_VIDEO_WIDTH, CHIP8_VIDEO_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, (GLvoid*)g_screen_buffer);

	// Set up the texture
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP); 

	// Enable textures
	glEnable(GL_TEXTURE_2D);
}

void update_texture(void)
{	
	// Update pixels
	memset(g_screen_buffer, 0, sizeof(g_screen_buffer));

	for(int y = 0; y < 32; ++y)	
	{	
		for(int x = 0; x < 64; ++x)
		{
			if (g_state.video_mem[y][x] != 0)
			{
				g_screen_buffer[y][x][0] = g_screen_buffer[y][x][1] = g_screen_buffer[y][x][2] = 255;  // Enabled
			}
		}
	}

	// Update Texture
	glTexSubImage2D(GL_TEXTURE_2D, 0 ,0, 0, CHIP8_VIDEO_WIDTH, CHIP8_VIDEO_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, (GLvoid*)g_screen_buffer);

	glBegin( GL_QUADS );
		glTexCoord2d(0.0, 0.0);	glVertex2d(0.0, 0.0);
		glTexCoord2d(1.0, 0.0); glVertex2d(CHIP8_screen_width, 0.0);
		glTexCoord2d(1.0, 1.0); glVertex2d(CHIP8_screen_width, CHIP8_screen_height);
		glTexCoord2d(0.0, 1.0); glVertex2d(0.0, CHIP8_screen_height);
	glEnd();
}

// glut display and idle handler
void display(void)
{
	// Clear framebuffer
	glClear(GL_COLOR_BUFFER_BIT);
	update_texture();
	glutSwapBuffers();    
	g_state.video_update = 0;
}

// 
void tick(void)
{
	int error = chip8_tick(&g_state);
	if (error)
	{
		uint16_t opcode = (uint16_t)(g_state.mem[g_state.PC - 2] << 8) | (g_state.mem[g_state.PC - 1]);
		printf("Execution exception at 0x%x (0x%x): %s\n", g_state.PC, opcode, strerror(error));
		exit(error);
	}

	if(g_state.video_update > 0)
	{ 
		glutPostRedisplay();
	}

	usleep(2000);
}

void reshape_window(GLsizei w, GLsizei h)
{
	glClearColor(0.0f, 0.0f, 0.5f, 0.0f);
	glMatrixMode(GL_PROJECTION);
    	glLoadIdentity();
    	gluOrtho2D(0, w, h, 0);        
    	glMatrixMode(GL_MODELVIEW);
    	glViewport(0, 0, w, h);
}

static char g_key_map[CHIP8_TOTAL_KEYS] = 
{
	'1', '2', '3', '4',
	'q', 'w', 'e', 'r',
	'a', 's', 'd', 'f',
	'z', 'x', 'c', 'v',
};

static uint8_t get_mapped_key(char glut_key)
{
	for (unsigned i = 0; i < CHIP8_TOTAL_KEYS; ++i)
	{
		if (g_key_map[i] == glut_key)
		{
			return i;
		}
	}

	return CHIP8_TOTAL_KEYS;
}

void keyboardDown(unsigned char key, int x, int y)
{
	if(key == 27)    // esc
		exit(0);

	uint8_t mapped_key = get_mapped_key(key);
	if (mapped_key < CHIP8_TOTAL_KEYS)
	{
		printf("Marking key %d\n", mapped_key);
		chip8_set_key_state(&g_state, mapped_key, 1);
	}
}

void keyboardUp(unsigned char key, int x, int y)
{
	uint8_t mapped_key = get_mapped_key(key);
	if (mapped_key < CHIP8_TOTAL_KEYS)
	{
		chip8_set_key_state(&g_state, mapped_key, 0);
	}
}


////////////////////////////////////////////////////////////////////
//
//	Utils and entry
//
////////////////////////////////////////////////////////////////////


static void usage()
{
	printf("soft-chip8 image\n");
}

// Load app image
static int load_image(const char* path)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0)
	{
		return errno;
	}

	// check image size
	off_t fsize = lseek(fd, 0, SEEK_END);
	if (fsize > CHIP8_RAM_SIZE)
	{
		return ENOSPC;
	}

	lseek(fd, 0, SEEK_SET);

	if (fsize != read(fd, g_state.mem + CHIP8_INIT_PC, fsize))
	{
		return errno;
	}

	return 0;
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		usage();
		return EXIT_FAILURE;
	}

	int error = chip8_init(&g_state);
	if (error)
	{
		printf("Failed to initialize chip8 state: %s\n", strerror(error));
		return error;
	}

	printf("Loading image %s\n", argv[1]);

	error = load_image(argv[1]);
	if (error)
	{
		printf("Failed loading image %s: %s\n", argv[1], strerror(error));
		return error;
	}

	// Setup OpenGL
	glutInit(&argc, argv);          
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);

	glutInitWindowSize(CHIP8_screen_width, CHIP8_screen_height);
	glutInitWindowPosition(320, 320);
	glutCreateWindow("soft-chip8");
	
	glutDisplayFunc(display);
	glutIdleFunc(tick);
    	glutReshapeFunc(reshape_window);        
	glutKeyboardFunc(keyboardDown);
	glutKeyboardUpFunc(keyboardUp); 

	setup_texture();			

	glutMainLoop(); 

	return 0;
}

