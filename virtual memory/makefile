app: MAIN.o mem_sim.o
	gcc MAIN.o mem_sim.o -o app


MAIN.o: MAIN.c mem_sim.h
		gcc -c MAIN.c	

mem_sim.o: mem_sim.c mem_sim.h
		gcc -c mem_sim.c		