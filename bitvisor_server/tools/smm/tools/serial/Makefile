# Really poor makefile, don't forget to use make install before run the test
# programs

CC = gcc
CFLAGS = -Werror -fno-common 
PROGRAMS =  serial_async serial_can

OBJS = serial_async.o
OBJS1 = serial_can.o

all:	$(PROGRAMS)	

.c.o:
	$(CC) $(CFLAGS) -c -o $@  $<

serial_async : $(OBJS)
	$(CC) -o $@ $(OBJS) 

serial_can : $(OBJS1)
	$(CC) -o $@ $(OBJS1) 

clean:
	rm -rf $(PROGRAMS) $(OBJS) $(OBJS1)

cleanall:
	make clean
