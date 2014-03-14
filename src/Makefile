OUT1 = usimm-open 
OUT2 = usimm-close
OUT3 = usimm-orcw
BINDIR = ../bin
OBJDIR = ../obj
OBJS1 = $(OBJDIR)/main.o $(OBJDIR)/memory_controller.o $(OBJDIR)/scheduler-fcfs.o 
OBJS2 = $(OBJDIR)/main.o $(OBJDIR)/memory_controller.o $(OBJDIR)/scheduler-close.o
OBJS3 = $(OBJDIR)/main.o $(OBJDIR)/memory_controller.o $(OBJDIR)/scheduler-orcw.o
CC = gcc
DEBUG = -g
CFLAGS = -std=c99 -Wall -c $(DEBUG)
LFLAGS = -Wall $(DEBUG)

.PHONY: all

all: $(BINDIR)/$(OUT1) $(BINDIR)/$(OUT2) $(BINDIR)/$(OUT3)

$(BINDIR)/$(OUT1): $(OBJS1)
	$(CC) $(LFLAGS) $(OBJS1) -o $(BINDIR)/$(OUT1)
	chmod 777 $(BINDIR)/$(OUT1)

$(BINDIR)/$(OUT2): $(OBJS2)
	$(CC) $(LFLAGS) $(OBJS2) -o $(BINDIR)/$(OUT2)
	chmod 777 $(BINDIR)/$(OUT2)

$(BINDIR)/$(OUT3): $(OBJS3)
	$(CC) $(LFLAGS) $(OBJS3) -o $(BINDIR)/$(OUT3)
	chmod 777 $(BINDIR)/$(OUT3)

$(OBJDIR)/main.o: main.c processor.h configfile.h memory_controller.h scheduler.h params.h
	$(CC) $(CFLAGS) main.c -o $(OBJDIR)/main.o
	chmod 777 $(OBJDIR)/main.o

$(OBJDIR)/memory_controller.o: memory_controller.c utlist.h utils.h params.h memory_controller.h scheduler.h processor.h
	$(CC) $(CFLAGS) memory_controller.c -o $(OBJDIR)/memory_controller.o
	chmod 777 $(OBJDIR)/memory_controller.o

$(OBJDIR)/scheduler-fcfs.o: scheduler-fcfs.c scheduler.h utlist.h utils.h memory_controller.h params.h
	$(CC) $(CFLAGS) scheduler-fcfs.c -o $(OBJDIR)/scheduler-fcfs.o
	chmod 777 $(OBJDIR)/scheduler-fcfs.o

$(OBJDIR)/scheduler-close.o: scheduler-close.c scheduler.h utlist.h utils.h memory_controller.h params.h
	$(CC) $(CFLAGS) scheduler-close.c -o $(OBJDIR)/scheduler-close.o
	chmod 777 $(OBJDIR)/scheduler-close.o

$(OBJDIR)/scheduler-orcw.o: scheduler-orcw.c scheduler.h utlist.h utils.h memory_controller.h params.h
	$(CC) $(CFLAGS) scheduler-orcw.c -o $(OBJDIR)/scheduler-orcw.o
	chmod 777 $(OBJDIR)/scheduler-orcw.o

clean:
	rm -f $(BINDIR)/$(OUT1) $(BINDIR)/$(OUT2) $(BINDIR)/$(OUT3) $(OBJS1) $(OBJS2) $(OBJS3)

