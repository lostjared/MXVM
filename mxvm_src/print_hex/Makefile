CC=cc
MXVMC=mxvmc
LIB_PATH=/usr/local/lib
OBJ_PATH=.
IO_MODULE_PATH=/usr/local/lib/modules/io
IO_LIB=-lmxvm_io_static

SRC_MXVM=HexFile.mxvm PrintHex.mxvm
OBJ_S=$(SRC_MXVM:.mxvm=.s)
OBJ_O=$(SRC_MXVM:.mxvm=.o)

TARGET=print-hex

.PHONY: all clean

%.s: %.mxvm
	$(MXVMC) $< --path $(LIB_PATH) --object-path $(OBJ_PATH) --action translate

%.o: %.s 
	$(CC) $< -c -o $@

all: $(TARGET)

$(TARGET): $(OBJ_O)
	$(CC) $(OBJ_O) -o $(TARGET) -L$(IO_MODULE_PATH) $(IO_LIB)

clean:
	rm -f $(OBJ_S) $(OBJ_O) $(TARGET)
