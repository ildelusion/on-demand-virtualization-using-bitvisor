# Really poor makefile, don't forget to use make install before run the test
# programs

CC = gcc
CFLAGS = -Werror -fno-common 
LDFLAGS = -lSMM -L./lib/ 
INCLUDES = -I. -I../include
PROGRAMS =  invoke_smi dump_smm copy_smm invoke_test serial_enable invoke_smi_revirt

OBJS = libSMM.o
OBJS2 = invoke_smi.o 
OBJS3 = dump_smm.o 
OBJS4 = copy_smm.o 
OBJS5 = serial_enable.o
OBJS6 = invoke_test.o
OBJS7 = invoke_smi_revirt.o

all:	$(PROGRAMS)	

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@  $<

invoke_smi : $(OBJS) $(OBJS2)
	$(CC) -o $@ $(OBJS) $(OBJS2)

dump_smm : $(OBJS)  $(OBJS3)
	$(CC) -o $@ $(OBJS) $(OBJS3)

copy_smm : $(OBJS) $(OBJS4)
	$(CC) -o $@ $(OBJS) $(OBJS4)

invoke_test : $(OBJS) $(OBJS6)
	$(CC) -o $@ $(OBJS) $(OBJS6)

#invoke_smi : $(OBJS) $(OBJS2)
#	$(CC) -L./xen-4.0.1/tools/libxc -lxenctrl -o $@ $(OBJS) $(OBJS2)

#dump_smm : $(OBJS)  $(OBJS3)
#	$(CC) -L./xen-4.0.1/tools/libxc -lxenctrl -o $@ $(OBJS) $(OBJS3)

#copy_smm : $(OBJS) $(OBJS4)
#	$(CC) -L./xen-4.0.1/tools/libxc -lxenctrl -o $@ $(OBJS) $(OBJS4)

serial_enable: $(OBJS) $(OBJS5)
	$(CC) -o $@ $(OBJS) $(OBJS5)

invoke_smi_revirt: $(OBJS) $(OBJS7)
	$(CC) -o $@ $(OBJS) $(OBJS7)

clean:
	rm -rf $(PROGRAMS) $(OBJS) $(OBJS2) $(OBJS3) $(OBJS4) $(OBJS5) $(OBJS6) $(OBJS7)

cleanall:
	make clean
