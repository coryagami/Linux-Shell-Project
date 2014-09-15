valhalla.x: main.c arglist.c
	gcc -o valhalla.x main.c arglist.c -I.

clean:
	rm valhalla.x
