arch := $(shell uname -p)

ifeq ($(arch), x86_64)
arch := -m64
else
arch := -m32
endif

CC := g++
CFLAGS += -fno-asynchronous-unwind-tables -nocpp -fuse-cxa-atexit -fpermissive	\
		  -O3 -Wall $(arch) -fgnu-unique -fstack-protector-all 					\
		  -fgnu-tm -ffast-math -fexpensive-optimizations -ffunction-sections 	\
		  -fdata-sections -std=gnu++11 -Werror


OBJS += chip8.o
prog: all
	$(CC) $(CFLAGS) -o $@ $(OBJS) -lSDL2 -lSDL2_mixer

all: $(OBJS)
$(OBJS): %.o : %.cpp
	$(CC) $(CFLAGS) -c $*.cpp
