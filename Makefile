CC     = gcc
CFLAGS = -Wall

EXE    = image_tagger
OBJ    = image_tagger.c

$ (EXE): $(OBJ)
	$(CC) $(CFLAGS) -o $(EXE) $(OBJ)

clean:
	rm -f $(EXE)


###########################  Save for future use: #############################

# CC     = gcc
# CFLAGS = -Wall -std=c99 -lm -O2 -fomit-frame-pointer

# EXE    = image_tagger
# OBJ    = image_tagger.o 


# # top (default) target
# all: $(EXE)

# # how to link executable
# $(EXE): $(OBJ)
# 	$(CC) $(CFLAGS) -o $(EXE) $(OBJ)

# # other dependencies
# image_tagger.o: linkedlist.h http_request.h
# linkedlist.o:
# http_request.o:

# # phony targets (these targets do not represent actual files)
# .PHONY: clean cleanly all CLEAN

# # `make clean` to remove all object files
# # `make CLEAN` to remove all object and executable files
# # `make cleanly` to `make` then immediately remove object files (inefficient)
# clean:
# 	rm -f $(OBJ)
# CLEAN: clean
# 	rm -f $(EXE)
# cleanly: all clean
