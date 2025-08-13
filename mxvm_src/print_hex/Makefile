### Created by MXVM

LFLAGS=""
all:
	LDFLAGS=$(LFLAGS) mxvmc  "PrintHex.mxvm" --path "/usr/local/lib" --object-path "." --action compile  --target linux
run:
	mxvmc --path "/usr/local/lib" object_path "." "PrintHex.mxvm"
