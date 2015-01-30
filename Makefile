OBJS = chip8.o

TEST = chip8-test
TEST_OBJS = $(OBJS) test.o

EMU = soft-chip8
EMU_OBJS = $(OBJS) main.o

CC = gcc
CFLAGS = -std=c99 -pg -gdwarf-2 -Wall -I.


ALL: $(EMU) Makefile

$(EMU): $(EMU_OBJS)
	$(CC) $(LDFLAGS) $(EMU_OBJS) -o $(EMU)

$(TEST): $(TEST_OBJS) 
	$(CC) $(LDFLAGS) $(TEST_OBJS) -lcunit -o $(TEST)
	./$(TEST)

%.o.: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean: 
	rm -rf $(EMU) $(TEST) *.o

