
#include <stdio.h>	
#include <stdlib.h>
#include "opencl_utils.h"
#include "CLGetPlatforms.hpp"
#include "bitmap_image.hpp"
#include "mandelbrot_frame.h"

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>	
#endif

#include <windows.h>

const unsigned int MAX_SOURCE_SIZE = (0x100000);	
const unsigned int window_width = 800;
const unsigned int window_height = 600;

const unsigned int OFFSET_X = 0;
const unsigned int OFFSET_Y = 0;
const unsigned int ZOOMFACTOR = 200;
const unsigned int MAX_ITERATIONS = 1024;
const unsigned int COLORTABLE_SIZE = 1024;

float stepsize = 1.0f / ZOOMFACTOR;

mandelbrot_color colortable2[COLORTABLE_SIZE];

#define FILENAME "./KernelMandelbrot.cl"
#define KERNELNAME "mandelbrot_frame"

void create_colortable()
{
	// Initialize color table values
	for (unsigned int i = 0; i < COLORTABLE_SIZE; i++)
	{
		if (i < 64) {
			mandelbrot_color color_entry = { 0, 0, (5 * i + 20<255) ? 5 * i + 20 : 255 };
			colortable2[i] = color_entry;
		}

		else if (i < 128) {
			mandelbrot_color color_entry = { 0, 2 * i, 255 };
			colortable2[i] = color_entry;
		}

		else if (i < 512) {
			mandelbrot_color color_entry = { 0, (i / 4<255) ? i / 4 : 255, (i / 4<255) ? i / 4 : 255 };
			colortable2[i] = color_entry;
		}

		else if (i < 768) {
			mandelbrot_color color_entry = { 0, (i / 4<255) ? i / 4 : 255, (i / 4<255) ? i / 4 : 255 };
			colortable2[i] = color_entry;
		}

		else {
			mandelbrot_color color_entry = { 0,(i / 10<255) ? i / 10 : 255,(i / 10<255) ? i / 10 : 255 };
			colortable2[i] = color_entry;
		}
	}
}


int main()
{
	/* Mandelbrot init stuff */
	remove("fractal_output.bmp");

	// Create the colortable and fill it with colors
	create_colortable();

	// Create an empty image
	bitmap_image image(window_width, window_height);
	mandelbrot_color * framebuffer = (mandelbrot_color *)image.data();

	printf("Please choose calculation device:\n[0] CPU\n[1] GPU\n");
	int dev_choice = -1;
	scanf("%d", &dev_choice);
	auto cl_dev_type = (dev_choice == 0 ? CL_DEVICE_TYPE_CPU : CL_DEVICE_TYPE_GPU);

	if (dev_choice < 0 || dev_choice > 1)
	{
		printf("Invalid CPU/GPU choice, defaulting to GPU");
	}

	if (cl_dev_type == CL_DEVICE_TYPE_GPU)
	{
		getPlatforms();
	}

	/* Stuff for OpenCL variable inits */

	cl_platform_id *platforms;
	cl_device_id device_id = NULL;
	cl_context context = NULL;
	cl_command_queue command_queue = NULL;

	cl_mem returnObj = NULL;

	cl_program program = NULL;
	cl_kernel kernel = NULL;
	cl_uint ret_num_devices;
	cl_uint ret_num_platforms;
	cl_uint platformCount;
	cl_int err;

	/* Get platform count */
	err = clGetPlatformIDs(0, NULL, &platformCount);
	checkError(err, "Couldn't get platform count");
	
	/* Get Platform/Device Information */
	platforms = (cl_platform_id*)malloc(sizeof(cl_platform_id) * platformCount);
	err = clGetPlatformIDs(platformCount, platforms, &ret_num_platforms);
	checkError(err, "Couldn't get platform IDs");

	unsigned int plat_id;

	if (cl_dev_type == CL_DEVICE_TYPE_GPU) {
		printf("Select platform\n");
		scanf("%d", &plat_id);
		if (plat_id >= ret_num_platforms)
		{
			plat_id = 0;
			printf("Invalid index, selecting 0\n");
		}

	} 
	else {
		plat_id = 0;
	}
	
	cl_platform_id platform_id = platforms[plat_id];

	err = clGetDeviceIDs(platform_id, cl_dev_type, 1, &device_id, &ret_num_devices);
	checkError(err, "Couldn't get device IDs");

	char dev_info_buff[10240];
	err = clGetDeviceInfo(device_id, CL_DEVICE_NAME, sizeof(dev_info_buff), dev_info_buff, NULL);
	checkError(err, "Couldn't get device info");
	printf("Using device %s\n", dev_info_buff);
	
	/* Create OpenCL Context */
	context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &err);
	checkError(err, "Couldn't create OpenCL context");

	/*Create command queue */
	command_queue = clCreateCommandQueue(context, device_id, 0, &err);
	checkError(err, "Couldn't create command queue");

	/*Create Buffer Object */
	cl_mem cl_offset_x		= clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float), NULL, &err);
	checkError(err, "Couldn't create buffer");
	cl_mem cl_offset_y		= clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float), NULL, &err);
	checkError(err, "Couldn't create buffer");
	cl_mem cl_stepsize		= clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float), NULL, &err);
	checkError(err, "Couldn't create buffer");
	cl_mem cl_maxiterations = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(unsigned int), NULL, &err);
	checkError(err, "Couldn't create buffer");
	cl_mem cl_framebuffer	= clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(mandelbrot_color) * window_width * window_height, NULL, &err);
	checkError(err, "Couldn't create buffer");
	cl_mem cl_window_width	= clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(unsigned int), NULL, &err);
	checkError(err, "Couldn't create buffer");
	cl_mem cl_window_height	= clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(unsigned int), NULL, &err);
	checkError(err, "Couldn't create buffer");

	// Create new table on device
	cl_mem dev_colortable = clCreateBuffer(context, CL_MEM_READ_ONLY,
		COLORTABLE_SIZE * sizeof(mandelbrot_color), NULL, &err);
	checkError(err, "Couldn't create colortable on device");

	LARGE_INTEGER freq, start_fractal, start_copy, end_fractal, end_read, end_copy;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&start_copy);

	/* Copy input data to the memory buffer */
	err = clEnqueueWriteBuffer(command_queue, cl_offset_x, CL_TRUE, 0, sizeof(float), (void *)&OFFSET_X, 0, NULL, NULL);
	checkError(err, "Couldn't enqueue write buffer");
	err = clEnqueueWriteBuffer(command_queue, cl_offset_y, CL_TRUE, 0, sizeof(float), (void *)&OFFSET_Y, 0, NULL, NULL);
	checkError(err, "Couldn't enqueue write buffer");
	err = clEnqueueWriteBuffer(command_queue, cl_stepsize, CL_TRUE, 0, sizeof(float), (void *)&stepsize, 0, NULL, NULL);
	checkError(err, "Couldn't enqueue write buffer");
	err = clEnqueueWriteBuffer(command_queue, cl_maxiterations, CL_TRUE, 0, sizeof(unsigned int), (void *)&MAX_ITERATIONS, 0, NULL, NULL);
	checkError(err, "Couldn't enqueue write buffer");
	err = clEnqueueWriteBuffer(command_queue, cl_window_width, CL_TRUE, 0, sizeof(unsigned int), (void *)&window_width, 0, NULL, NULL);
	checkError(err, "Couldn't enqueue write buffer");
	err = clEnqueueWriteBuffer(command_queue, cl_window_height, CL_TRUE, 0, sizeof(unsigned int), (void *)&window_height, 0, NULL, NULL);
	checkError(err, "Couldn't enqueue write buffer");

	// Write the color table data to the device
	err = clEnqueueWriteBuffer(command_queue, dev_colortable, CL_TRUE, 0,
		COLORTABLE_SIZE * sizeof(mandelbrot_color),
		colortable2, 0, NULL, NULL);
	checkError(err, "Couldn't write colortable to device");

	QueryPerformanceCounter(&end_copy);

	/* Verify kernel */
	build_program(context, device_id, FILENAME);

	/* Do the kernel open thing */

	FILE *fp;
	const char fileName[] = FILENAME;
	size_t source_size;
	char *source_str;

	/* Load kernel source file */
	fp = fopen(fileName, "r");
	if (!fp) {
		fprintf(stderr, "Failed to load kernel.\n");
		exit(1);
	}
	source_str = (char *)malloc(MAX_SOURCE_SIZE);
	source_size = fread(source_str, 1, MAX_SOURCE_SIZE, fp);
	fclose(fp);

	/* Create kernel program from source file*/
	program = clCreateProgramWithSource(context, 1, (const char **)&source_str, (const size_t *)&source_size, &err);
	checkError(err, "Couldn't create kernel program from source");

	err = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);
	checkError(err, "Couldn't build kernel program");

	/* Create OpenCL kernel*/
	kernel = clCreateKernel(program, KERNELNAME, &err);
	checkError(err, "Couldn't create kernel");

	err = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&cl_offset_x);
	checkError(err, "Couldn't set kernel argument");
	err = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&cl_offset_y);
	checkError(err, "Couldn't set kernel argument");
	err = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *)&cl_stepsize);
	checkError(err, "Couldn't set kernel argument");
	err = clSetKernelArg(kernel, 3, sizeof(cl_mem), (void *)&cl_maxiterations);
	checkError(err, "Couldn't set kernel argument");
	err = clSetKernelArg(kernel, 5, sizeof(cl_mem), (void *)&dev_colortable);
	checkError(err, "Couldn't set kernel argument");
	err = clSetKernelArg(kernel, 6, sizeof(cl_mem), (void *)&cl_window_width);
	checkError(err, "Couldn't set kernel argument");
	err = clSetKernelArg(kernel, 7, sizeof(cl_mem), (void *)&cl_window_height);
	checkError(err, "Couldn't set kernel argument");


	size_t workgroup_size;
	clGetKernelWorkGroupInfo(kernel, device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &workgroup_size, NULL);
	printf("Workgroup sz: %zu\n", workgroup_size);

	size_t global_item_size[2] = { window_width, window_height };
	// How to find optimal size? 25*30 or 40*20 is slow for example, while that
	// is the closest to the reported 1024 reported work group size (GT630M)
	size_t local_item_size[2] = { 25 , 20 }; 

	err = clSetKernelArg(kernel, 4, sizeof(cl_mem), (void *)&cl_framebuffer);
	checkError(err, "Couldn't set kernel argument");

	// Kernel stuff's done :)

	// Get current time before calculating the fractal
	QueryPerformanceCounter(&start_fractal);
	err = clEnqueueNDRangeKernel(command_queue, kernel, 2, NULL, global_item_size, local_item_size, 0, NULL, NULL);
	clFinish(command_queue);
	QueryPerformanceCounter(&end_fractal);
	checkError(err, "EnqueueNDRangeKernel error");

	err = clEnqueueReadBuffer(command_queue, cl_framebuffer, CL_TRUE, 0, sizeof(mandelbrot_color) * window_width * window_height, framebuffer, 0, NULL, NULL);
	checkError(err, "Couldn't read result");

	QueryPerformanceCounter(&end_read);
	
	printf("Copy to: %f msec\n", (double)(end_copy.QuadPart - start_copy.QuadPart) / freq.QuadPart * 1000.0);
	printf("Fractal: %f msec\n", (double)(end_fractal.QuadPart - start_fractal.QuadPart) / freq.QuadPart * 1000.0);
	printf("F+Read : %f msec\n", (double)(end_read.QuadPart - start_fractal.QuadPart) / freq.QuadPart * 1000.0);

	printf("Press RETURN to show result\n");
	char ch;
	scanf("%c", &ch);
	getchar();

	// Write image to file
	image.save_image("fractal_output.bmp");

	// Show image in mspaint
	WinExec("mspaint fractal_output.bmp", SW_MAXIMIZE);

	return 0;
}
