#include "visual.h"
#include <string.h>

struct thread_info{
	image img;
	image mask;
	image out;
	int size;
	int threadId;
};

void *dither_pthreads(void *arg) {
	struct thread_info *threadInfo = (struct thread_info*) arg;
	int i, j, pos, errorR, errorG, errorB;
	Pixel newVal;

 	for(i = 0; i < threadInfo->img.y; i++) {
		for(j = 0; j < threadInfo->img.x; j++) {
			pos = threadInfo->img.x * i + j;
			
			newVal = getClosestPixel(threadInfo->img.dataColor[pos], 0);
			errorR = threadInfo->img.dataColor[pos].r - newVal.r;
			errorG = threadInfo->img.dataColor[pos].g - newVal.g;
			errorB = threadInfo->img.dataColor[pos].b - newVal.b;
			threadInfo->img.dataColor[pos] = newVal;
			correctNeighbors(threadInfo->img.dataColor, threadInfo->img.x, threadInfo->img.y, j, i, errorR, errorG, errorB);
		}
	}

	return NULL;
}

void *makeMask_pthreads(void *arg) {
	int i, j;

	struct thread_info *threadInfo = (struct thread_info*) arg;


	int shuffle[4];
	shuffle[0] = 0;
	shuffle[1] = 128;
	shuffle[2] = 192;
	shuffle[3] = 255;
	int line1, line2;

	for(i = 0; i < threadInfo->mask.y; i+= 2) {
		for(j = 0; j < threadInfo->mask.x; j+= 2) {
			int seed = i * j;
			line1 = i * threadInfo->mask.x + j;
			line2 = (i + 1) * threadInfo->mask.x + j;
			shuffle_arr(shuffle, seed);

			threadInfo->mask.dataGray[line1] = shuffle[0];
			threadInfo->mask.dataGray[line1 + 1] = shuffle[1];

			threadInfo->mask.dataGray[line2] = shuffle[2];
			threadInfo->mask.dataGray[line2 + 1] = shuffle[3];
		}
	}

	return NULL;
}

void *makeShare_pthreads(void *arg) {
	int i, j, line1, line2, pos, k;
	int p[4];

	struct thread_info *threadInfo = (struct thread_info*) arg;


	for(i = 0; i < threadInfo->img.y; i++) {
		line1 = 2 * i * threadInfo->mask.x;
		line2 = (2 * i + 1) * threadInfo->mask.x;
		for(j = 0; j < threadInfo->img.x; j++) {
			pos = i * threadInfo->img.x + j;
			p[0] = line1 + 2 * j;
			p[1] = p[0] + 1;
			p[2] = line2 + 2 * j;
			p[3] = p[2] + 1;

			for(k = 0; k < 4; k++) {
				if (threadInfo->mask.dataGray[p[k]] == threadInfo->img.dataColor[pos].r) {
					threadInfo->out.dataColor[p[k]].r = 255;
				}

				if(threadInfo->mask.dataGray[p[k]] == threadInfo->img.dataColor[pos].g) {
					threadInfo->out.dataColor[p[k]].g = 255;
				}

				if(threadInfo->mask.dataGray[p[k]] == threadInfo->img.dataColor[pos].b) {
					threadInfo->out.dataColor[p[k]].b = 255;
				}
 			}

		}
	}

	return NULL;
}

void *decrypt_pthreads(void *arg) {
	int i, j, k, curr;
	int line1, line2;
	int p[4];

	struct thread_info *threadInfo = (struct thread_info*) arg;


	for(i = 0; i < threadInfo->mask.y; i+= 2) {
		line1 = i * threadInfo->mask.x;
		line2 = (i + 1) * threadInfo->mask.x;
		for(j = 0; j < threadInfo->mask.x; j+= 2) {
			p[0] = line1 + j;
			p[1] = p[0] + 1;
			p[2] = line2 + j;
			p[3] = p[2] + 1;
			curr = (i * threadInfo->mask.x) / 4 + j / 2;
			for(k = 0; k < 4; k++) {
				if(threadInfo->img.dataColor[p[k]].r == 255) {
					threadInfo->out.dataColor[curr].r = threadInfo->mask.dataGray[p[k]];
				}
				if(threadInfo->img.dataColor[p[k]].g == 255) {
					threadInfo->out.dataColor[curr].g = threadInfo->mask.dataGray[p[k]];
				}
				if(threadInfo->img.dataColor[p[k]].b == 255) {
					threadInfo->out.dataColor[curr].b = threadInfo->mask.dataGray[p[k]];
				}

			}
		}
	}

	return NULL;
}


int main(int argc, char **argv) {
	int type, i, nrOfThreads;
	image input, mask, share, out;
	struct thread_info *threadData;
	pthread_t *threads;

	if(strcmp(argv[1], "encrypt") == 0) {
		
		type = readInput(argv[2], &input);
		if (type == -1) {
			printf("Error\n");
			return -1;
		}

		sscanf(argv[3], "%d", &nrOfThreads);
		
		threads = (pthread_t *)malloc(nrOfThreads * sizeof(pthread_t));
		mask.x = 2 * input.x;
		mask.y = 2 * input.y;
		mask.density = 1;
		mask.dataGray = (unsigned char *)malloc(mask.x * mask.y * sizeof(unsigned char));

		share.x = mask.x;
		share.y = mask.y;
		share.density = 3;
		share.dataColor = (Pixel *)malloc(share.x * share.y * sizeof(Pixel)); 


		int rows = input.y / nrOfThreads;
		int chunk = rows * input.x;
		threadData = (struct thread_info *)malloc(nrOfThreads * sizeof(struct thread_info));

		for(i = 0; i < nrOfThreads; i++) {
			threadData[i].img.dataColor = (Pixel*)malloc(chunk * sizeof(Pixel));
			threadData[i].img.x = input.x;
			threadData[i].img.y = rows;
			threadData[i].img.density = 3;
			memcpy(threadData[i].img.dataColor, input.dataColor + chunk * i, chunk * sizeof(Pixel));
			threadData[i].size = nrOfThreads;
			threadData[i].threadId = i;
			pthread_create(&threads[i], NULL, dither_pthreads, (void *)&threadData[i]);
		}

		for(i = 0; i < nrOfThreads; i++) {
			pthread_join(threads[i], NULL);
		}

		for(i = 0; i < nrOfThreads; i++) {
			threadData[i].mask.dataGray = (unsigned char*)malloc(chunk * 4 * sizeof(unsigned char*));
			threadData[i].mask.x = 2 * input.x;
			threadData[i].mask.y = rows * 2;
			pthread_create(&threads[i], NULL, makeMask_pthreads, (void *)&threadData[i]);
		}

		for(i = 0; i < nrOfThreads; i++) {
			pthread_join(threads[i], NULL);
		}

		for(i = 0; i < nrOfThreads; i++) {
			threadData[i].out.dataColor = (Pixel *)malloc(chunk * 4 * sizeof(Pixel));
			threadData[i].out.x = mask.x;
			threadData[i].out.y = 2 * rows;
			threadData[i].out.density = 3;
			pthread_create(&threads[i], NULL, makeShare_pthreads, (void *)&threadData[i]);
		}


		for(i = 0; i < nrOfThreads; i++) {
			pthread_join(threads[i], NULL);
		}


		for(i = 0; i < nrOfThreads; i++) {
			memcpy(input.dataColor + chunk * i, threadData[i].img.dataColor, chunk * sizeof(Pixel));
			memcpy(mask.dataGray + chunk * 4 * i, threadData[i].mask.dataGray, chunk * 4 * sizeof(unsigned char));
			memcpy(share.dataColor + chunk * 4 * i, threadData[i].out.dataColor, chunk * 4 * sizeof(Pixel));
		}


		writeData("dither.pnm", &input, 1);
		writeData("mask.pnm", &mask, 0);
		writeData("share.pnm", &share, 1);
	} else if(strcmp(argv[1], "decrypt") == 0) {
		type = readInput(argv[2], &share);
		if (type == -1) {
			printf("Error\n");
			return -1;
		}	

		type = readInput(argv[3], &mask);
		if (type == -1) {
			printf("Error\n");
			return -1;
		}

		sscanf(argv[4], "%d", &nrOfThreads);
		
		threads = (pthread_t *)malloc(nrOfThreads * sizeof(pthread_t));

		int rows = share.y / nrOfThreads;
		int chunk = rows * share.x;
		threadData = (struct thread_info *)malloc(nrOfThreads * sizeof(struct thread_info));

		out.x = share.x / 2;
		out.y = share.y / 2;
		out.density = 3;
		out.dataColor = (Pixel *)malloc(out.x * out.y * sizeof(Pixel));

		for(i = 0; i < nrOfThreads; i++) {
			threadData[i].img.dataColor = (Pixel *)malloc(chunk * sizeof(Pixel));
			threadData[i].img.x = share.x;
			threadData[i].img.y = rows;

			threadData[i].mask.dataGray = (unsigned char *)malloc(chunk * sizeof(unsigned char));
			threadData[i].mask.x = share.x;
			threadData[i].mask.y = rows;

			threadData[i].out.dataColor = (Pixel *)malloc((chunk / 4) * sizeof(Pixel));
			threadData[i].out.x = share.x / 2;
			threadData[i].out.y = rows / 2;

			memcpy(threadData[i].img.dataColor, share.dataColor + chunk * i, chunk * sizeof(Pixel));
			memcpy(threadData[i].mask.dataGray, mask.dataGray + chunk * i, chunk * sizeof(unsigned char));

			pthread_create(&threads[i], NULL, decrypt_pthreads, (void *)&threadData[i]);
		}

		for(i = 0; i < nrOfThreads; i++) {
			pthread_join(threads[i], NULL);
		}

		for(i = 0; i < nrOfThreads; i++) {
			memcpy(out.dataColor + (chunk / 4) * i, threadData[i].out.dataColor, (chunk / 4) * sizeof(Pixel));
		}

		writeData("decrypt.pnm", &out, 1);
	}

	


}