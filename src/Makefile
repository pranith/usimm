
OUT = usimm
BINDIR = ../bin
OBJDIR = ../obj
OBJS = $(OBJDIR)/main.o $(OBJDIR)/memory_controller.o $(OBJDIR)/scheduler.o
CC = gcc
DEBUG = -g
CFLAGS = -std=c99 -Wall -c $(DEBUG)
LFLAGS = -Wall $(DEBUG)


$(BINDIR)/$(OUT): $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) -o $(BINDIR)/$(OUT)
	chmod 777 $(BINDIR)/$(OUT)

$(OBJDIR)/main.o: main.c processor.h configfile.h memory_controller.h scheduler.h params.h
	$(CC) $(CFLAGS) main.c -o $(OBJDIR)/main.o
	chmod 777 $(OBJDIR)/main.o

$(OBJDIR)/memory_controller.o: memory_controller.c utlist.h utils.h params.h memory_controller.h scheduler.h processor.h
	$(CC) $(CFLAGS) memory_controller.c -o $(OBJDIR)/memory_controller.o
	chmod 777 $(OBJDIR)/memory_controller.o

$(OBJDIR)/scheduler.o: scheduler.c scheduler.h utlist.h utils.h memory_controller.h params.h
	$(CC) $(CFLAGS) scheduler.c -o $(OBJDIR)/scheduler.o
	chmod 777 $(OBJDIR)/scheduler.o

clean:
	rm -f $(BINDIR)/$(OUT) $(OBJS)

