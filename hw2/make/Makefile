DIR_ROOT = ..
DIR_SRC = $(DIR_ROOT)/src
DIR_INCLUDE = $(DIR_ROOT)/include

SRC = $(wildcard $(DIR_SRC)/*.c)
OUT = disk

all:
	gcc -o $(OUT) -I$(DIR_INCLUDE) $(SRC)
