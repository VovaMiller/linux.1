DIR_ROOT = ..
DIR_SRC = $(DIR_ROOT)/src
DIR_INCLUDE = $(DIR_ROOT)/include

FILES_SERVER = fs.c fs_core.c bitmap.c utils.c server.c
FILES_CLIENT = utils.c client.c
SRC_SERVER = $(addprefix $(DIR_SRC)/,$(FILES_SERVER))
SRC_CLIENT = $(addprefix $(DIR_SRC)/,$(FILES_CLIENT))
FILES_H_SERVER = fs.h fs_core.h bitmap.h utils.h
FILES_H_CLIENT = utils.h
H_SERVER = $(addprefix $(DIR_INCLUDE)/,$(FILES_H_SERVER))
H_CLIENT = $(addprefix $(DIR_INCLUDE)/,$(FILES_H_CLIENT))
OUT_SERVER = fs_server
OUT_CLIENT = fs_client

all: server client

server: $(SRC_SERVER) $(H_SERVER)
	gcc -o $(OUT_SERVER) -I$(DIR_INCLUDE) $(SRC_SERVER)

client: $(SRC_CLIENT) $(H_CLIENT)
	gcc -o $(OUT_CLIENT) -I$(DIR_INCLUDE) $(SRC_CLIENT)

clear:
	rm -f $(OUT_SERVER) ; rm -f $(OUT_CLIENT)

