all: myshell

myshell: shell3.c
	gcc shell3.c -o myshell
	
clean:
	rm -f myshell *.o
