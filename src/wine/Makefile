CC= gcc
INCLUDES= -Idriver

# 
CFLAGS= -DSPEED -Wall -O3 -g -mcpu=i686  -march=i686 -malign-functions=4 
#CFLAGS= -DSPEED -Wall -O3 -mcpu=pentiumpro  -fomit-frame-pointer -finline-functions -fcaller-saves -ffast-math -fno-strength-reduce -fthread-jumps -funroll-loops

# 
#CFLAGS= -Wall -O3 -m486 -fomit-frame-pointer -finline-functions -fcaller-saves -ffast-math -fno-strength-reduce -fthread-jumps

WINOBJ = module.o \
	 ext.o \
	 win32.o \
	 ldt_keeper.o \
	 pe_image.o \
	 pe_resource.o \
	 resource.o \
	 registry.o \
	 elfdll.o \
	 vfl.o \
	 afl.o \
	 stubs.o \
	 driver.o


all: $(WINOBJ)
	ar r libwine.a *.o


clean:
	rm -f *~ *.o *.a

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $*.c


