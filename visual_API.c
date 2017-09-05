#include "visual.h"

int readInput(const char * fileName, image *img) {
	int type;
	char buff[20];

	FILE *fp;
	int c, rgb_comp_color;
	fp = fopen(fileName, "rb");

	if (!fp) {
		printf("Cannot open file\n");
		return -1;
	} //opens the ppm and checks to make sure it can be opened

	if (!fgets(buff, sizeof(buff), fp)) {
		perror(fileName);
		return -1;
	} //read the format of the image


	if (buff[0] == 'P') {
		if(buff[1] == '6'){
			type = 1;
		}
		else if(buff[1] == '5'){
			type = 0;	
		}
		else{
			return -1;	
		}
	} else{
		printf("Not a valid image format\n");
		return -1;
	} //checks to see if the format is ppm



	c = getc(fp);
	while (c == '#') {
		while (getc(fp) != '\n') ;
	 		c = getc(fp);
    }//checks for comments

    ungetc(c, fp);

    if(type == 1){
    	img->density = 3;
    } else{
    	img->density = 1;
    }

    if (fscanf(fp, "%d %d", &img->x, &img->y) != 2) {
    	printf("Error reading size\n");
    	return -1;
	} //reads the size of the image, height becomes img->y, and width becomes img->x
	if (fscanf(fp, "%d", &rgb_comp_color) != 1) {
		printf("Error reading colors\n");
		return -1;
	} //reads how much of each color there is

	if (rgb_comp_color!= 255) {
		printf("Oh noes\n");
		return -1;
	} //makes sure the the component is 8 bits

	while (fgetc(fp) != '\n') ;

	if(type == 1){
		img->dataColor = (Pixel*)malloc(img->x * img->y * img->density * sizeof(Pixel));	
	}
	else{
		img->dataGray = (unsigned char*)malloc(img->x * img->y * img->density * sizeof(unsigned char));
	}

	if (!img) {
		printf("Image structure is null\n");
		return -1;
	} //allocates the memory need for the pixel data

	if(type == 1){
		fread(img->dataColor, img->density, img->x * img->y, fp);	
	}
	else{
		fread(img->dataGray, img->density, img->x * img->y, fp);
	}

	fclose(fp);

	return type;
}

void writeData(const char * fileName, image *img, int type) {
	FILE *fp;

	fp = fopen(fileName, "wb");
	
	if (!fp) {
		exit(1);
	} //opens the file for output

    //write the header file
    //image format
	if(type == 1){
		fprintf(fp, "P6\n");
	}
	else{
		fprintf(fp, "P5\n");
	}
	
    //image size
	fprintf(fp, "%d %d\n",img->x,img->y);

    // rgb component depth
	fprintf(fp, "255\n");

    // pixel data
	if(type == 1){
		fwrite(img->dataColor, img->density, img->x * img->y, fp);	
	}
	else{
		fwrite(img->dataGray, img->density, img->x * img->y, fp);
	}
	fclose(fp);		
}

int getClosest(unsigned char a) {
	if(a <= 64)
		return 0;
	else if(a > 64 && a <= 160)
		return 128;
	else if(a > 160 && a <= 223)
		return 192;
	else
		return 255;
}

Pixel getClosestPixel(Pixel a, int type) {
	Pixel aux;


	aux.r = getClosest(a.r);
	aux.g = getClosest(a.g);
	aux.b = getClosest(a.b);

	return aux;
}

void correctNeighbors(Pixel *img, int x, int y, int x1, int y1, int errorR, int errorG, int errorB) {
	int pos;

	if (y1 + 1 < y) {
		pos = (y1 + 1) * x + x1;
		img[pos].r +=  errorR * 5 / 16;
		img[pos].g +=  errorG * 5 / 16;
		img[pos].b +=  errorB * 5 / 16;
		if (x1 + 1 < x) {
			img[pos + 1].r += errorR * 1 / 16; 
			img[pos + 1].g += errorG * 1 / 16; 
			img[pos + 1].b += errorB * 1 / 16; 
		}
		if(x1 - 1 > 0) {
			img[pos - 1].r += errorR * 3 / 16; 
			img[pos - 1].g += errorG * 3 / 16; 
			img[pos - 1].b += errorB * 3 / 16; 
		}
	}

	if (x1 + 1 < x) {
		pos = y1 * x + x1 + 1;
		img[pos].r += errorR * 7 / 16;
		img[pos].g += errorG * 7 / 16;
		img[pos].b += errorB * 7 / 16;
	}
}

void shuffle_arr(int *shuffle, unsigned int seed)
{
    int i, j, t;
    for(i = 3; i > 0; i--) {
    	j = rand_r(&seed) % (i + 1);	
    	t = shuffle[i];
    	shuffle[i] = shuffle[j];
    	shuffle[j] = t;
    }
}	


