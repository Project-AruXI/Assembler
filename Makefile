CC = gcc
CFLAGS = -Wall
OUT = ./out
COMP = ./components
STRUCTS = ./structures
COMMON = ./common
COMMON_LIBDIR = $(COMMON)/lib
HEADERS = ./headers

INCLUDES = -I$(HEADERS) -I$(COMMON)/defs/instr -I$(COMMON)/defs -I$(COMMON_LIBDIR)/argparse \
			-I$(COMMON_LIBDIR)/sds -I$(COMMON_LIBDIR)/securedstring

SRCS = assembler.c $(COMP)/diagnostics.c $(COMP)/lexer.c $(COMP)/parser.c $(COMP)/instructionHandlers.c $(COMP)/directiveHandlers.c \
			 $(COMP)/expr.c $(COMP)/adecl.c $(COMP)/codegen.c $(COMP)/binwriter.c \
			 $(STRUCTS)/SymbolTable.c $(STRUCTS)/SectionTable.c $(STRUCTS)/StructTable.c  $(STRUCTS)/DataTable.c $(STRUCTS)/RelocTable.c $(STRUCTS)/ast.c
LIBS = $(COMMON_LIBDIR)/libargparse.a $(COMMON_LIBDIR)/libsds.a $(COMMON_LIBDIR)/libsecuredstring.a
TARGET = $(OUT)/arxsm

ifeq ($(MAKECMDGOALS),windows)
	TARGET = $(OUT)/arxsm.exe
	SRCS := $(SRCS) $(COMP)/getline.c
endif

OBJS = $(SRCS:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

all: arxsm

arxsm: $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

commonlibs:
	$(MAKE) -C $(COMMON_LIBDIR) all

liblexer: CFLAGS += -g -O0
liblexer:
	$(CC) -fPIC $(CFLAGS) -o $(COMP)/liblexer.o -c $(COMP)/lexer.c $(INCLUDES)
	$(CC) -fPIC $(CFLAGS) -o $(COMP)/diagnostics.o -c $(COMP)/diagnostics.c $(INCLUDES)
	$(CC) -shared -o $(OUT)/liblexer.so $(COMP)/liblexer.o $(COMP)/diagnostics.o $(COMMON_LIBDIR)/libsecuredstring.a $(COMMON_LIBDIR)/libsds.a $(INCLUDES)

libparser: CFLAGS += -g -O0
libparser:
	$(CC) -fPIC $(CFLAGS) -o $(COMP)/libparser.o -c $(COMP)/parser.c $(INCLUDES)
	$(CC) -fPIC $(CFLAGS) -o $(COMP)/diagnostics.o -c $(COMP)/diagnostics.c $(INCLUDES)
	$(CC) -fPIC $(CFLAGS) -o $(COMP)/instructionHandlers.o -c $(COMP)/instructionHandlers.c $(INCLUDES)
	$(CC) -fPIC $(CFLAGS) -o $(COMP)/directiveHandlers.o -c $(COMP)/directiveHandlers.c $(INCLUDES)
	$(CC) -fPIC $(CFLAGS) -o $(COMP)/expr.o -c $(COMP)/expr.c $(INCLUDES)
	$(CC) -fPIC $(CFLAGS) -o $(COMP)/adecl.o -c $(COMP)/adecl.c $(INCLUDES)
	$(CC) -fPIC $(CFLAGS) -o $(STRUCTS)/SymbolTable.o -c $(STRUCTS)/SymbolTable.c $(INCLUDES)
	$(CC) -fPIC $(CFLAGS) -o $(STRUCTS)/SectionTable.o -c $(STRUCTS)/SectionTable.c $(INCLUDES)
	$(CC) -fPIC $(CFLAGS) -o $(STRUCTS)/StructTable.o -c $(STRUCTS)/StructTable.c $(INCLUDES)
	$(CC) -fPIC $(CFLAGS) -o $(STRUCTS)/DataTable.o -c $(STRUCTS)/DataTable.c $(INCLUDES)
	$(CC) -fPIC $(CFLAGS) -o $(STRUCTS)/ast.o -c $(STRUCTS)/ast.c $(INCLUDES)
	$(CC) -shared -o $(OUT)/libparser.so $(COMP)/libparser.o $(COMMON_LIBDIR)/libsecuredstring.a $(COMMON_LIBDIR)/libsds.a $(INCLUDES)

libcodegen: CFLAGS += -g -O0
libcodegen:
	$(CC) -fPIC $(CFLAGS) -o $(COMP)/codegen.o -c $(COMP)/codegen.c $(INCLUDES)
	$(CC) -fPIC $(CFLAGS) -o $(STRUCTS)/DataTable.o -c $(STRUCTS)/DataTable.c $(INCLUDES)
	$(CC) -shared -o $(OUT)/libcodegen.so $(COMP)/codegen.o $(STRUCTS)/DataTable.o $(COMMON_LIBDIR)/libsecuredstring.a

windows: CC = zig cc
windows: CFLAGS += --target=x86_64-windows -g -O0
windows: LIBS = $(COMMON_LIBDIR)/libargparse-win.a $(COMMON_LIBDIR)/libsds-win.a $(COMMON_LIBDIR)/libsecuredstring-win.a
windows: arxsm

debug: CFLAGS += -g -DDEBUG -O0
debug: arxsm

clean:
	rm -f **/*.o
	rm -f out/*.so
	rm assembler.o
