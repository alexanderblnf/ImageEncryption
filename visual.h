#ifndef visual_h
#define visual_h

#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>

typedef struct{
	unsigned char r, g, b;
}Pixel;
typedef struct{
	int x, y;
	int density;
	Pixel *dataColor;
	unsigned char *dataGray;
}image;

int readInput(const char * fileName, image *img);
void writeData(const char * fileName, image *img, int type);
double makeComponents(image *img);
void decrypt(image *img, image *mask, image *out);
Pixel getClosestPixel(Pixel a, int type);
int getClosest(unsigned char a);
void shuffle_arr(int *shuffle, unsigned int seed);
void makeMask(int x, int y, unsigned char **mask);
void correctNeighbors(Pixel *img, int x, int y, int x1, int y1, int errorR, int errorG, int errorB);

#endif