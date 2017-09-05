all: visual mpi_visual pthreads_visual hybrid_visual

visual: new_dither.c main.c visual_API.c
	gcc -Wall -g -fopenmp -o visual new_dither.c visual_API.c main.c -lm
	
mpi_visual: mpi_visual.c visual_API.c 
	mpicc -Wall -g -o mpi_visual mpi_visual.c visual_API.c -lm

pthreads_visual: pthreads_visual.c visual_API.c
	gcc -Wall -pthread -o pthreads_visual pthreads_visual.c visual_API.c

hybrid_visual: hibrid.c visual_API.c
	mpicc -Wall -fopenmp -o hibrid hibrid.c visual_API.c

clean:
	rm visual mpi_visual mask.pnm dither.pnm share*.pnm decrypt.pnm pthreads_visual hibrid
