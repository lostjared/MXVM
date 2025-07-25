all:
	vm HexFile.mxvm --path /usr/local/lib --object-path . --action translate -c
	vm print_hex.mxvm --path /usr/local/lib --object-path . -action translate 
	gcc HexFile.s PrintHex.s -o print-hex -L/usr/local/lib/modules/io -lmxvm_io_static
