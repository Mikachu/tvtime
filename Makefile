
# you know it, baby
#CC = gcc -Wall -pedantic -O3 -funroll-loops -fomit-frame-pointer
CC = gcc -g -Wall -pedantic

# libsdl because i'm too cheap to do it myself
SDLFLAGS = `sdl-config --cflags`
SDLLIBS = `sdl-config --libs`

TTFFLAGS =
TTFLIBS = -lfreetype

PNGFLAGS =
PNGLIBS = -lpng

CFLAGS = -Wall -I. $(SDLFLAGS) $(TTFFLAGS) $(PNGFLAGS)
CXXFLAGS = -Wall -I. $(SDLFLAGS) $(TTFFLAGS) -I/usr/include/freetype2
LDFLAGS = $(SDLLIBS) $(TTFLIBS) $(PNGLIBS)

OBJS = frequencies.o mixer.o videoinput.o sdloutput.o rtctimer.o \
	videotools.o ttfont.o efs.o osd.o parser.o tvtimeconf.o \
	pngoutput.o tvtimeosd.o input.o cpu_accel.o speedy.o

all: tvtime

tvtime: $(OBJS) tvtime.o
	gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

timingtest: $(OBJS) timingtest.o
	gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean: 
	rm -f *.o *.png tvtime timingtest

