#include "visual.h"

void makeMask(int x, int y, unsigned char **mask) {
	int i, j;

	int x1 = 2 * x;
	int y1 = 2 * y;

	(*mask) = (unsigned char*)malloc(x1 * y1 * sizeof(unsigned char));

	int shuffle[4];
	#pragma omp parallel private(shuffle)
	{
		shuffle[0] = 0;
		shuffle[1] = 128;
		shuffle[2] = 192;
		shuffle[3] = 255;
		int line1, line2;

		#pragma omp for private(i, j, line1, line2)
		for(i = 0; i < y1; i+= 2) {
			for(j = 0; j < x1; j+= 2) {
				int seed = i * j;
				line1 = i * x1 + j;
				line2 = (i + 1) * x1 + j;
				shuffle_arr(shuffle, seed);

				(*mask)[line1] = shuffle[0];
				(*mask)[line1 + 1] = shuffle[1];

				(*mask)[line2] = shuffle[2];
				(*mask)[line2 + 1] = shuffle[3];
			}
		}
	}

}

void makeShare(image *img, image *mask, image *share) {
	int i, j, line1, line2, pos, k;
	int p[4];

	share->x = mask->x;
	share->y = mask->y;
	
	share->density = img->density;

	share->dataColor = (Pixel*)calloc(share->x * share->y * share->density, sizeof(Pixel));

	#pragma omp parallel for private(i, j, pos, line1, line2, p, k)
		for(i = 0; i < img->y; i++) {
			line1 = 2 * i * mask->x;
			line2 = (2 * i + 1) * mask->x;
			for(j = 0; j < img->x; j++) {
				pos = i * img->x + j;
				p[0] = line1 + 2 * j;
				p[1] = p[0] + 1;
				p[2] = line2 + 2 * j;
				p[3] = p[2] + 1;

				for(k = 0; k < 4; k++) {
					if (mask->dataGray[p[k]] == img->dataColor[pos].r) {
						share->dataColor[p[k]].r = 255;
					}

					if(mask->dataGray[p[k]] == img->dataColor[pos].g) {
						share->dataColor[p[k]].g = 255;
					}

					if(mask->dataGray[p[k]] == img->dataColor[pos].b) {
						share->dataColor[p[k]].b = 255;
					}
	 			}

			}
		}
}

void dither(image *img) {
	int i, j, pos, errorR, errorG, errorB;
	Pixel newVal;
	
	#pragma omp parallel for private(i, j, pos, newVal, errorR, errorG, errorB) schedule(dynamic, img->x)
 	for(i = 0; i < img->y; i++) {
		for(j = 0; j < img->x; j++) {
			pos = img->x * i + j;
			
			newVal = getClosestPixel(img->dataColor[pos], 0);
			errorR = img->dataColor[pos].r - newVal.r;
			errorG = img->dataColor[pos].g - newVal.g;
			errorB = img->dataColor[pos].b - newVal.b;
			img->dataColor[pos] = newVal;
			correctNeighbors(img->dataColor, img->x, img->y, j, i, errorR, errorG, errorB);
		}
	}
}

void decrypt(image *img, image *mask, image *out) {
	int curr = 0, i, j, k;
	int line1, line2;
	int p[4];

	out->x = mask->x / 2;
	out->y = mask->y / 2;
	out->density = img->density;

	out->dataColor = (Pixel*)malloc(out->x * out->y * out->density * sizeof(Pixel));

	#pragma omp parallel for private(i, j, line1, line2, p, k, curr)
	for(i = 0; i < mask->y; i+= 2) {
		line1 = i * mask->x;
		line2 = (i + 1) * mask->x;
		for(j = 0; j < mask->x; j+= 2) {
			p[0] = line1 + j;
			p[1] = p[0] + 1;
			p[2] = line2 + j;
			p[3] = p[2] + 1;
			curr = (i * mask->x) / 4 + j / 2;
			for(k = 0; k < 4; k++) {
				if(img->dataColor[p[k]].r == 255) {
					out->dataColor[curr].r = mask->dataGray[p[k]];
				}
				if(img->dataColor[p[k]].g == 255) {
					out->dataColor[curr].g = mask->dataGray[p[k]];
				}
				if(img->dataColor[p[k]].b == 255) {
					out->dataColor[curr].b = mask->dataGray[p[k]];
				}

			}
		}
	}

}


double makeComponents(image *img) {
	double totalTime = 0;
	struct timeval start, end;
	image mask, share1;
	
	mask.x = 2 * img->x;
	mask.y = 2 * img->y;
	mask.density = 1;

	gettimeofday(&start, NULL);
	makeMask(img->x, img->y, &mask.dataGray);
	gettimeofday(&end, NULL);
	totalTime += ((end.tv_sec  - start.tv_sec) * 1000000u + end.tv_usec - start.tv_usec) / 1.e6;
	writeData("mask.pnm", &mask, 0);

	gettimeofday(&start, NULL);
	dither(img);
	gettimeofday(&end, NULL);
	totalTime += ((end.tv_sec  - start.tv_sec) * 1000000u + end.tv_usec - start.tv_usec) / 1.e6;
	writeData("dither.pnm", img, 1);

	gettimeofday(&start, NULL);
	makeShare(img, &mask, &share1);
	gettimeofday(&end, NULL);
	totalTime += ((end.tv_sec  - start.tv_sec) * 1000000u + end.tv_usec - start.tv_usec) / 1.e6;
	writeData("share.pnm", &share1, 1);

	return totalTime;	
}