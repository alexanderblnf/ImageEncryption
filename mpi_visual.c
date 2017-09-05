#include "visual.h"
#include <mpi.h>

void dither_mpi(Pixel *img, int x, int y) {
	int i, j, pos, errorR, errorG, errorB;
	Pixel newVal;
	
 	for(i = 0; i < y; i++) {
		for(j = 0; j < x; j++) {
			pos = x * i + j;
			
			newVal = getClosestPixel(img[pos], 0);
			errorR = img[pos].r - newVal.r;
			errorG = img[pos].g - newVal.g;
			errorB = img[pos].b - newVal.b;
			img[pos] = newVal;
			correctNeighbors(img, x, y, j, i, errorR, errorG, errorB);
		}
	}
}

void makeMask_mpi(int x, int y, unsigned char **mask) {
	int i, j;

	int x1 = 2 * x;
	int y1 = 2 * y;

	int shuffle[4] = {0, 128, 192, 255};
	int line1, line2;


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

void makeShare_mpi(Pixel *img, int xImg, int yImg, unsigned char *mask, int xMask, int yMask, Pixel *share) {
	int i, j, line1, line2, pos, k;
	int p[4];

	for(i = 0; i < yImg; i++) {
		line1 = 2 * i * xMask;
		line2 = (2 * i + 1) * xMask;
		for(j = 0; j < xImg; j++) {
			pos = i * xImg + j;
			p[0] = line1 + 2 * j;
			p[1] = p[0] + 1;
			p[2] = line2 + 2 * j;
			p[3] = p[2] + 1;

			for(k = 0; k < 4; k++) {
				if (mask[p[k]] == img[pos].r) {
					share[p[k]].r = 255;
				}

				if(mask[p[k]] == img[pos].g) {
					share[p[k]].g = 255;
				}

				if(mask[p[k]] == img[pos].b) {
					share[p[k]].b = 255;
				}
 			}

		}
	}
}

void encrypt_mpi(char *inputFile, MPI_Datatype mpi_pixel) {
	MPI_Status status;
	int rank, size, type;

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	if(rank == 0) {
		image input, mask, share;
		int i;

		type = readInput(inputFile, &input);
		
		if (type == -1) {
			printf("Error\n");
			return ;
		}

		mask.dataGray = (unsigned char*)malloc(4 * input.x * input.y * sizeof(unsigned char));
		mask.x = 2 * input.x;
		mask.y = 2 * input.y;
		mask.density = 1;

		share.dataColor = (Pixel*)malloc(mask.x * mask.y * 3 * sizeof(Pixel));
		share.x = mask.x;
		share.y = mask.y;
		share.density = 3;

		int rows = input.y / size;
		int sizes[2] = {input.x, input.y};
		int remainder = input.y % size;
		int chunkSize = rows * input.x;
		int maxSize = (rows + remainder) * input.x;
		int maskMaxSize = 4 * maxSize, maskChunkSize = 4 * chunkSize;
		int id;
		unsigned char *maskBuf;


		for(i = 1; i < size; i++) {
			MPI_Send(&sizes, 2, MPI_INT, i, 0, MPI_COMM_WORLD);
		}
		
		maskBuf = (unsigned char*)malloc(maskMaxSize * sizeof(unsigned char));

		makeMask_mpi(input.x, rows, &mask.dataGray);

		for(i = 1; i < size; i++) {
			MPI_Recv(maskBuf, maskMaxSize, MPI_UNSIGNED_CHAR, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
			id = status.MPI_SOURCE;
			if(id == size - 1) {
				memcpy(mask.dataGray + maskChunkSize * id, maskBuf, maskMaxSize * sizeof(unsigned char));
			} else {
				memcpy(mask.dataGray + maskChunkSize * id, maskBuf, maskChunkSize * sizeof(unsigned char));
			}

		}

		for(i = 1; i < size; i++) {
			if(i == size - 1) {
				MPI_Send(input.dataColor + chunkSize * i, maxSize, mpi_pixel, i, 0, MPI_COMM_WORLD);
			} else {
				MPI_Send(input.dataColor + chunkSize * i, chunkSize, mpi_pixel, i, 0, MPI_COMM_WORLD);
			}
		}

		dither_mpi(input.dataColor, input.x, rows);
		makeShare_mpi(input.dataColor, input.x, rows, mask.dataGray, 2 * input.x, 2 * rows, share.dataColor);

		Pixel *buf;
		buf = (Pixel*)malloc(maskMaxSize * 3 * sizeof(Pixel));
		
		for(i = 1; i < size; i++) {
			MPI_Recv(buf, maskMaxSize, mpi_pixel, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
			id = status.MPI_SOURCE;
			if(id == size - 1) {
				memcpy(share.dataColor + maskChunkSize * id, buf, maskMaxSize * sizeof(Pixel));
			} else {
				memcpy(share.dataColor + maskChunkSize * id, buf, maskChunkSize * sizeof(Pixel));
			}
		}

		writeData("mask.pnm", &mask, 0);
		writeData("share.pnm", &share, 1);
	
	} else { // not process 0
		int sizes[2];
		Pixel *buf, *share;
		unsigned char *maskPart;
		int totalSize, maskTotalSize;
		int rows;

		MPI_Recv(&sizes, 2, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
		
		rows = sizes[1] / size;
		if(rank == size - 1) {
			rows += sizes[1] % size;
		}

		totalSize = rows * sizes[0]; 
		buf = (Pixel*)malloc(3 * totalSize * sizeof(Pixel));

		maskTotalSize = 4 * rows * sizes[0];
		maskPart = (unsigned char*)malloc(maskTotalSize * sizeof(unsigned char));

		makeMask_mpi(sizes[0], rows, &maskPart);

		MPI_Send(maskPart, maskTotalSize, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD);
		
		MPI_Recv(buf, totalSize, mpi_pixel, 0, 0, MPI_COMM_WORLD, &status);

		dither_mpi(buf, sizes[0], rows);

		share = (Pixel*)malloc(maskTotalSize * 3 * sizeof(Pixel));
		makeShare_mpi(buf, sizes[0], rows, maskPart, 2 * sizes[0], 2 * rows, share);

		MPI_Send(share, maskTotalSize, mpi_pixel, 0, 0, MPI_COMM_WORLD);
	}
}

void decrypt_func(Pixel *img, unsigned char *mask, int xMask, int yMask, Pixel *out) {
	int curr = 0, i, j, k;
	int line1, line2;
	int p[4];

	for(i = 0; i < yMask; i+= 2) {
		line1 = i * xMask;
		line2 = (i + 1) * xMask;
		for(j = 0; j < xMask; j+= 2) {
			p[0] = line1 + j;
			p[1] = p[0] + 1;
			p[2] = line2 + j;
			p[3] = p[2] + 1;
			curr = (i * xMask) / 4 + j / 2;
			for(k = 0; k < 4; k++) {
				if(img[p[k]].r == 255) {
					out[curr].r = mask[p[k]];
				}
				if(img[p[k]].g == 255) {
					out[curr].g = mask[p[k]];
				}
				if(img[p[k]].b == 255) {
					out[curr].b = mask[p[k]];
				}

			}
		}
	}

}

void decrypt_mpi(char *shareFile, char *maskFile, MPI_Datatype mpi_pixel) {
	MPI_Status status;
	int rank, size, type;

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	if(rank == 0) {
		image share, mask, out;
		int i;

		type = readInput(shareFile, &share);
	
		if (type == -1) {
			printf("Error\n");
			return ;
		}

		type = readInput(maskFile, &mask);
	
		if (type == -1) {
			printf("Error\n");
			return ;
		}

		int rows = share.y / size;
		int sizes[2] = {share.x, share.y};
		int remainder = share.y % size;
		int chunkSize = rows * share.x;
		int maxSize = (rows + remainder) * share.x;
		int outSize, outChunkSize, outMaxChunkSize;
		int id;

		for(i = 1; i < size; i++) {
			if (i == size - 1) {
				MPI_Send(&sizes, 2, MPI_INT, i, 0, MPI_COMM_WORLD);
				MPI_Send(share.dataColor + chunkSize * i, maxSize, mpi_pixel, i, 1, MPI_COMM_WORLD);
				MPI_Send(mask.dataGray + chunkSize * i, maxSize, MPI_UNSIGNED_CHAR, i, 2, MPI_COMM_WORLD);
			} else {
				MPI_Send(&sizes, 2, MPI_INT, i, 0, MPI_COMM_WORLD);
				MPI_Send(share.dataColor + chunkSize * i, chunkSize, mpi_pixel, i, 1, MPI_COMM_WORLD);
				MPI_Send(mask.dataGray + chunkSize * i, chunkSize, MPI_UNSIGNED_CHAR, i, 2, MPI_COMM_WORLD);
			}
		}

		out.x = share.x / 2;
		out.y = share.y / 2;
		out.density = 3;

		rows /= 2;
		outSize = out.x * out.y;
		outChunkSize = rows * out.x;
		outMaxChunkSize = (rows + rows % size) * out.x;
		out.dataColor = (Pixel*)malloc(outSize * 3 * sizeof(Pixel));

		decrypt_func(share.dataColor, mask.dataGray, share.x, rows * 2, out.dataColor);

		Pixel *buf;
		buf = (Pixel*)malloc(outMaxChunkSize * 3 * sizeof(Pixel));

		for(i = 1; i < size; i++) {
			MPI_Recv(buf, outSize, mpi_pixel, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
			id = status.MPI_SOURCE;
			if(id == size - 1) {
				memcpy(out.dataColor + outChunkSize * id, buf, outMaxChunkSize * sizeof(Pixel));
			} else {
				memcpy(out.dataColor + outChunkSize * id, buf, outChunkSize * sizeof(Pixel));
			}
		}
		writeData("decrypt.pnm", &out, 1);

    } else {
    	int sizes[2];
		Pixel *sharePart, *outPart;
		unsigned char *maskPart;
		int totalSize, outSize;
		int rows;

		MPI_Recv(&sizes, 2, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);

		rows = sizes[1] / size;
		if(rank == size - 1) {
			rows += sizes[1] % size;
		}

		totalSize = rows * sizes[0];
		outSize = totalSize / 4;

		sharePart = (Pixel*)malloc(3 * totalSize * sizeof(Pixel));

		maskPart = (unsigned char*)malloc(totalSize * sizeof(unsigned char));

		outPart = (Pixel*)malloc(3 * outSize * sizeof(Pixel));

		MPI_Recv(sharePart, totalSize, mpi_pixel, 0, 1, MPI_COMM_WORLD, &status);
		MPI_Recv(maskPart, totalSize, mpi_pixel, 0, 2, MPI_COMM_WORLD, &status);

		decrypt_func(sharePart, maskPart, sizes[0], rows, outPart);

		MPI_Send(outPart, outSize, mpi_pixel, 0, 0, MPI_COMM_WORLD);
    }
}


int main(int argc, char **argv) {

	MPI_Init(&argc, &argv);

	/* Create Pixel MPI Struct */
	const int nitems = 3;
	int blocklengths[3] = {1, 1, 1};

	MPI_Datatype types[3] = {MPI_UNSIGNED_CHAR, MPI_UNSIGNED_CHAR, MPI_UNSIGNED_CHAR};
	MPI_Datatype mpi_pixel;
	MPI_Aint offsets[3];

	offsets[0] = offsetof(Pixel, r);
	offsets[1] = offsetof(Pixel, g);
	offsets[2] = offsetof(Pixel, b);

	MPI_Type_create_struct(nitems, blocklengths, offsets, types, &mpi_pixel);
    MPI_Type_commit(&mpi_pixel);

    if(strcmp(argv[1], "encrypt") == 0) {
    	encrypt_mpi(argv[2], mpi_pixel);
    } else if(strcmp(argv[1], "decrypt") == 0){
    	decrypt_mpi(argv[2], argv[3], mpi_pixel);
    }

	MPI_Finalize();
}