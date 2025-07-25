all:
	mxvmc StringPrint.mxvm --path .. --object-path . --action translate -c
	mxvmc Hello.mxvm --path .. --object-path . --action translate -c
	mxvmc testmain.mxvm --path .. --object-path . --action translate
	gcc StringPrint.s StringMain.s Hello.s -o StringPrint -no-pie -L../build/modules/io -lmxvm_io_static

