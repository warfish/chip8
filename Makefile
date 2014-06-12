OBJS = chip8.o

TEST = chip8-test
TEST_OBJS = $(OBJS) test.o

EMU = soft-chip8
EMU_OBJS = $(OBJS) main.o

GCC = gcc
CFLAGS = -std=c99 -pg -gdwarf-2 -Wall -I. -I../gtest/
LDFLAGS = -framework GLUT -framework OpenGL -framework Cocoa


ALL: $(EMU) Makefile

$(EMU): $(EMU_OBJS)
	gcc $(LDFLAGS) $(EMU_OBJS) -o $(EMU)

$(TEST): $(TEST_OBJS) 
	gcc -lcunit $(LDFLAGS) $(TEST_OBJS) -o $(TEST)
	./$(TEST)

%.o.: %.c
	gcc $(CFLAGS) -c $< -o $@

clean: 
	rm -rf $(EMU) $(TEST) *.o

