#include "visual.h"

int main(int argc, char **argv) {
	image input, share, mask, out;
	double totalTime;
	int type;
	int numThreads;
	struct timeval start, end;
	if(argc < 3) {
		printf("Usage:  ./visual encrypt input num_threads\n\t./visual decrypt share mask num_threads\n");
		return -1;
	}

	if (strcmp(argv[1], "encrypt") == 0) {
		printf("\n=====Entering encryption mode======\n\n");
		type = readInput(argv[2], &input);

		
		if (type == -1) {
			printf("Error\n");
			return -1;
		}
		sscanf(argv[3], "%d", &numThreads);
		omp_set_num_threads(numThreads);

		totalTime = makeComponents(&input);

	} else if (strcmp(argv[1], "decrypt") == 0) {
		printf("\n=====Entering decryption mode======\n\n");
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

		sscanf(argv[4], "%d", &numThreads);
		omp_set_num_threads(numThreads);

		gettimeofday(&start, NULL);
		decrypt(&share, &mask, &out);
		gettimeofday(&end, NULL);
		totalTime = ((end.tv_sec  - start.tv_sec) * 1000000u + end.tv_usec - start.tv_usec) / 1.e6;
		writeData("decrypt.pnm", &out, 1);		
	} else {
		printf("encrypt/decrypt mode not selected\n");
		return -1;
	}

	printf("\n\nTIME: %f\n", totalTime);
	
	return 0;
}