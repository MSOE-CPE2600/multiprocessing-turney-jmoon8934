/***************************************************************************
* Filename: mandel_struct.h
* Author: Jeric Moon
* Date: 11/25/2025
* Desc: Provides declaration of a struct to be used for mandel image creation
****************************************************************************/
#include <pthread.h>
#include "jpegrw.h"

#ifndef MANDEL_STRUCT_H
#define MANDEL_STRUCT_H

typedef struct mandel_info
{
    imgRawImage* img;
    double xmin;
    double xmax;
    double ymin;
    double ymax;
    int max;
    int max_threads;
    int num_threads;
    pthread_mutex_t* mutex;
} mandel_info;

#endif