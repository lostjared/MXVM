all:
	../build/src/vm/vm StringPrint.mxvm --path .. --object-path . --action translate -c
	../build/src/vm/vm Hello.mxvm --path .. --object-path . --action translate -c
	../build/src/vm/vm testmain.mxvm --path .. --object-path . --action translate
	gcc StringPrint.s StringMain.s Hello.s -o StringPrint -no-pie -L../build/modules/io -lmxvm_io_static

