CC = gcc
CFLAGS = -Wall
OUT = ./out
COMP = ./components
STRUCTS = ./structures
COMMON = ./common
HEADERS = ./headers

INCLUDES = -I$(HEADERS) -I$(COMMON)

SRCS = assembler.c $(COMP)/diagnostics.c $(COMP)/lexer.c $(COMP)/parser.c $(COMP)/instructionHandlers.c $(COMP)/directiveHandlers.c \
			 $(COMP)/expr.c \
			 $(STRUCTS)/SymbolTable.c $(STRUCTS)/SectionTable.c $(STRUCTS)/StructTable.c $(STRUCTS)/ast.c
LIBS = libargparse.a $(COMMON)/libsds.a $(COMMON)/libsecuredstring.a
TARGET = $(OUT)/arxsm

OBJS = $(SRCS:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

all: arxsm

arxsm: $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

liblexer: CFLAGS += -g -O0
liblexer:
	$(CC) -fPIC $(CFLAGS) -o $(COMP)/liblexer.o -c $(COMP)/lexer.c $(INCLUDES)
	$(CC) -fPIC $(CFLAGS) -o $(COMP)/diagnostics.o -c $(COMP)/diagnostics.c $(INCLUDES)
	$(CC) -shared -o $(OUT)/liblexer.so $(COMP)/liblexer.o $(COMP)/diagnostics.o $(COMMON)/libsecuredstring.a $(COMMON)/libsds.a $(INCLUDES)

# libparser:
# libcodegen:

debug: CFLAGS += -g -DDEBUG -O0
debug: arxsm

clean:
	rm -f **/*.o