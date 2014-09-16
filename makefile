thereaper.x: main.c arglist.c
	gcc -o thereaper.x main.c arglist.c -I.

clean:
	rm thereaper.x
