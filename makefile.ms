# Makefile to compile LMM adapt as required. Under MS Windows system with installed Lsd follow these steps:
# 1. Open a dos shell and move in the root of Lsd system, where this makefile is located.
# 2. Run the batch file "run.bat". This will start LMM. Type Esc and quit the program. This will add the required path to the shell
# 3. type make -f makefile.ms. This will create a new LMM program using the source "modman.cpp" in directory "src"

CC=g++
TARGET=lmm
EXT=.exe
SWITCH_CC=-mthreads -mwindows -O3

# For MS Windows 32-bit systems, using the GNU GCC & Tcl/Tk in the Lsd installation.
TCL_VERSION=85
LSDROOT=.
PATH_TCL_HEADER=$(LSDROOT)/gnu/include
PATH_TK_HEADER=$(LSDROOT)/gnu/include
PATH_HEADER=$(LSDROOT)/gnu/include
PATH_TCL_LIB=$(LSDROOT)/gnu/lib
PATH_TK_LIB=$(LSDROOT)/gnu/lib
PATH_LIB=$(LSDROOT)/gnu/lib
LIBS=

# from here you should not need to modify anything
RM=rm -f
SRC=src
SRC_DIR=$(LSDROOT)/$(SRC)/

# Variable used to prepare the options to search for the tcl and tk headers
# Do not modify
INCLUDE=-I$(PATH_TCL_HEADER) -I$(PATH_TK_HEADER) -I$(PATH_HEADER) -I$(LSDROOT)/src

$(TARGET)$(EXT): $(SRC_DIR)modman.o
	$(CC) $(SWITCH_CC) $(SRC_DIR)modman.o -L$(PATH_TCL_LIB) -ltcl$(TCL_VERSION) -L$(PATH_TK_LIB) -ltk$(TCL_VERSION) -L$(PATH_LIB) $(LIBS) -o $(TARGET)

$(SRC_DIR)modman.o: $(SRC_DIR)modman.cpp 
	$(CC) $(SWITCH_CC) -c $(SRC_DIR)modman.cpp -o $(SRC_DIR)modman.o $(INCLUDE)

# Clean object files for building for another distribution
clean:
	$(RM) $(SRC_DIR)modman.o
