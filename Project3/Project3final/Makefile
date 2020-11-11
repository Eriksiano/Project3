DebugFlag=-g
ObjectFiles=main.o
Compile=gcc

main.o: main.c header.h
	$(Compile) -c main.c 

app: $(ObjectFiles)
	$(Compile) -o app $(ObjectFiles)

clean:
	-rm *.o