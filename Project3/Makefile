DebugFlag=-g
ObjectFiles=main.o 
Compile=gcc

main.o: main.c
	$(Compile) -c main.c 

app: main.o
	$(Compile) -o app $(ObjectFiles)

clean:
	-rm *.o *~ core paper