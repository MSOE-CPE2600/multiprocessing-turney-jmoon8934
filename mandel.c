/* 
* Based on example code found here:
* https://users.cs.fiu.edu/~cpoellab/teaching/cop4610_fall22/project3.html
*
* Converted to use jpg instead of BMP and other minor changes
*  
* Filename: mandel.c
* Modified By: Jeric Moon
* Date: 11/24/2025
*/
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <pthread.h>
#include "jpegrw.h"
#include "mandel_struct.h"

// local routines
static int iteration_to_color( int i, int max );
static int iterations_at_point( double x, double y, int max );
static void *compute_image(void *arg);
static void show_help();


int main( int argc, char *argv[] )
{

	char c;

	// These are the default configuration values used
	// if no command line arguments are given.
	char *outfile = "mandel.jpg";

	double xcenter = -1.4012874603271483;
	double ycenter = -0.0000286102294921875;
	double xscale = 4;
	double yscale = 0; // calc later
	int    image_width = 1000;
	int    image_height = 1000;
	int    max = 1000;
	int    num_imag = 1;
	int    active_proc = 0; //calc later

	//mmap for our output file saving. Wish for it to be the size of our outfile parameter
	char *output = mmap(NULL, (strlen(outfile)+1), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	//mmap for max proc, default value of 1.
	int *max_proc = mmap(NULL, 2*sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	max_proc[0] = 1;
	//default num threads
	int *max_threads = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	*max_threads = 2;
	//mandel_info struct
	mandel_info minfo;


	// For each command line argument given,
	// override the appropriate configuration value.

	while((c = getopt(argc,argv,"x:y:s:W:H:m:o:h:n:p:t:"))!=-1) {
		switch(c) 
		{
			case 'x':
				xcenter = atof(optarg);
				break;
			case 'y':
				ycenter = atof(optarg);
				break;
			case 's':
				xscale = atof(optarg);
				break;
			case 'W':
				image_width = atoi(optarg);
				break;
			case 'H':
				image_height = atoi(optarg);
				break;
			case 'm':
				max = atoi(optarg);
				break;
			case 'o':
				outfile = optarg;
				break;
			case 'h':
				show_help();
				exit(1);
				break;
			case 'n':
				num_imag = atoi(optarg);
				break;
			case 'p':

				max_proc[0] = atoi(optarg);
				break;
			case 't':
				*max_threads = atoi(optarg);
				if(*max_threads < 1 || *max_threads > 20)
				{
					printf("Invalid number of threads (%d) detected! Defaulting to 2 threads.\n", *max_threads);
					*max_threads = 2;
				}

		}
	}

	//copy outfile parameter into outfile mmap'd output for image proccessing 
	strcpy(output, outfile);

	// Calculate y scale based on x scale (settable) and image sizes in X and Y (settable)
	for(int i = 0; i < num_imag; i++)
	{
		if(active_proc >= max_proc[0])
        {
            //if at max capacity, wait for it to finish up to decriment and go agian
            //manages active processors to always be max processors or less.
            //possible for it to complete faster than it fills up
            wait(NULL); //wait(NULL) waits for ANY child to finish.
            active_proc--;
        }
		int pid = fork();
		if(pid == 0)
		{
			//Modify out-file saving
			//Declare temp str. array with length of output + 1 for \0, and then num_imag%10+1 for # of image deliminator. 
			//1-9 images = +1, 10-99 = +2, etc
			char outfile_ittr[strlen(output)+1+num_imag%10+1] = {};
			//Append up to the file extention into outfile_ittr
			strncat(outfile_ittr, output, (strlen(output)-4));
			//Temp itter string to process the current itteration
			//Length of this is num_images%10+1 (for deliminator) + strlen(".jpg") +1 for null temr
			char itter[num_imag%10+1+strlen(".jpg")+1];
			sprintf(itter, "%d.jpg", (i+1));
			strcat(outfile_ittr, itter);
			
			//adjust scale for each iteration.
			xscale = xscale*pow(0.75, num_imag-i);
			yscale = xscale / image_width * image_height;

			// Display the configuration of the image.
			printf("mandel: x=%lf y=%lf xscale=%lf yscale=%1f max=%d outfile=%s\n",xcenter,ycenter,xscale,yscale,max,outfile_ittr);

			// Create a raw image of the appropriate size.
			imgRawImage* img = initRawImage(image_width,image_height);

			// Fill it with a black
			setImageCOLOR(img,0);
			pthread_t threads[*max_threads];
			pthread_mutex_t mymutex;
			pthread_mutex_init(&mymutex, NULL);
		

			minfo.img = img;
			minfo.xmin = xcenter-xscale/2;
			minfo.xmax = xcenter+xscale/2;
			minfo.ymin = ycenter-yscale/2;
			minfo.ymax = ycenter+yscale/2;
			minfo.max = max;
			minfo.max_threads = *max_threads;
			minfo.num_threads = 0;
			minfo.mutex = &mymutex;


			//Run threads dependant on thread args
			for(int j = 0; j < *max_threads; j++)
			{
				pthread_create(&threads[j], NULL, compute_image, &minfo);
			}
			for(int j = 0; j < *max_threads; j++)
			{
				pthread_join(threads[j], NULL);
			}
			pthread_mutex_destroy(minfo.mutex);



			// Save the image in the stated file.
			storeJpegImageFile(img,outfile_ittr);

			// free the mallocs
			freeRawImage(img);
			exit(0);
		}
		else if(pid > 0)
		{
			active_proc++;
		}
	}
	//munmaps for shared memory
	munmap(output, (strlen(outfile)+1));
	munmap(max_proc, sizeof(int));
	return 0;
}




/*
Return the number of iterations at point x, y
in the Mandelbrot space, up to a maximum of max.
*/

int iterations_at_point( double x, double y, int max )
{
	double x0 = x;
	double y0 = y;

	int iter = 0;

	while( (x*x + y*y <= 4) && iter < max ) {

		double xt = x*x - y*y + x0;
		double yt = 2*x*y + y0;

		x = xt;
		y = yt;

		iter++;
	}

	return iter;
}

/*
Compute an entire Mandelbrot image, writing each point to the given bitmap.
Scale the image to the range (xmin-xmax,ymin-ymax), limiting iterations to "max"
*/

void *compute_image(void *arg)
{
	//cast arg back into mandel_info
	mandel_info *minfo = (mandel_info *)arg;

	//inherit offset = thread count
	int active_thread = -1;
	pthread_mutex_lock(minfo->mutex);
	if(minfo->num_threads < minfo->max_threads)
	{
		active_thread = minfo->num_threads;
		minfo->num_threads++;
	}
	pthread_mutex_unlock(minfo->mutex);

	//if offset hasn't changed, already at max threads
	if(active_thread == -1)
	{
		return NULL;
	}

	int i,j;

	int width = minfo->img->width;
	int height = minfo->img->height;

	// For every pixel in the image...
	//only opperate in bounds of active thread
	for(j=active_thread*height/minfo->max_threads;j<(active_thread+1)*height/minfo->max_threads;j++) {

		for(i=0;i<width;i++) {

			// Determine the point in x,y space for that pixel.
			double x = minfo->xmin + i*(minfo->xmax-minfo->xmin)/width;
			double y = minfo->ymin + j*(minfo->ymax-minfo->ymin)/height;

			// Compute the iterations at that point.
			int iters = iterations_at_point(x,y,minfo->max);

			// Set the pixel in the bitmap.
			setPixelCOLOR(minfo->img,i,j,iteration_to_color(iters,minfo->max));
		}
	}
	return NULL;
}


/*
Convert a iteration number to a color.
Here, we just scale to gray with a maximum of imax.
Modify this function to make more interesting colors.
*/
int iteration_to_color( int iters, int max )
{
	int color = 0xFFFFFF*iters/(double)max;
	return color;
}


// Show help message
void show_help()
{
	printf("Use: mandel [options]\n");
	printf("Where options are:\n");
	printf("-m <max>    The maximum number of iterations per point. (default=1000)\n");
	printf("-x <coord>  X coordinate of image center point. (default=0)\n");
	printf("-y <coord>  Y coordinate of image center point. (default=0)\n");
	printf("-s <scale>  Scale of the image in Mandlebrot coordinates (X-axis). (default=4)\n");
	printf("-W <pixels> Width of the image in pixels. (default=1000)\n");
	printf("-H <pixels> Height of the image in pixels. (default=1000)\n");
	printf("-o <file>   Set output file. (default=mandel.bmp)\n");
	printf("-h          Show this help text.\n");
	printf("-n          Set the number of images to be generated (default = 1)\n");
	printf("-p          Set the number of processors to be used (default = 1)\n");
	printf("-t          Set the number of threads to be used (default = 2)\n");
	printf("\nSome examples are:\n");
	printf("mandel -x -0.5 -y -0.5 -s 0.2\n");
	printf("mandel -x -.38 -y -.665 -s .05 -m 100\n");
	printf("mandel -x 0.286932 -y 0.014287 -s .0005 -m 1000\n\n");
}
