
# you know it, baby
CC = gcc -Wall -O3 -funroll-loops -fomit-frame-pointer
CXX = g++ -Wall -O3 -funroll-loops -fomit-frame-pointer
#CC = gcc -g -Wall -pedantic

# libsdl because i'm too cheap to do it myself
SDLFLAGS = `sdl-config --cflags`
SDLLIBS = `sdl-config --libs`

TTFFLAGS = -I/usr/include/freetype2
TTFLIBS = -lfreetype

PNGFLAGS =
PNGLIBS = -lpng

CFLAGS = -I. $(SDLFLAGS) $(TTFFLAGS) $(PNGFLAGS)
CXXFLAGS = -I. $(SDLFLAGS) $(TTFFLAGS)
LDFLAGS = $(SDLLIBS) $(TTFLIBS) $(PNGLIBS)

OBJS = frequencies.o mixer.o videoinput.o sdloutput.o rtctimer.o \
	videotools.o ttfont.o efs.o osd.o parser.o tvtimeconf.o \
	pngoutput.o tvtimeosd.o input.o cpu_accel.o speedy.o \
	pnginput.o menu.o deinterlace.o

PLUGINS = plugins/greedy2frame.so plugins/twoframe.so plugins/linear.so

all: tvtime

%.so: %.c
	$(CC) -shared $(CFLAGS) -o $@ $<

tvtime: $(OBJS) tvtime.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

plugins: $(PLUGINS)

timingtest: $(OBJS) timingtest.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean: 
	rm -f plugins/*.so *.o tvtime timingtest

