
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
	remove("fractal_output.bmp");

	// Create the colortable and fill it with colors
	create_colortable();

	// Create an empty image
	bitmap_image image(window_width, window_height);
	mandelbrot_color * framebuffer = (mandelbrot_color *)image.data();

	/* Init OpenCL variables */
	cl_platform_id *platforms;
	cl_device_id device_id = NULL;
	cl_context context = NULL;
	cl_command_queue command_queue = NULL;

	cl_program program = NULL;
	cl_kernel kernel = NULL;
	cl_uint ret_num_devices;
	cl_uint ret_num_platforms;
	cl_int err;

	/* Init rest of variables */
	int devChoice = -1;
	LARGE_INTEGER freq, start_fractal, start_copy, end_fractal, end_read, end_copy;
	QueryPerformanceFrequency(&freq);

	int platformChoice = -1;
	char *deviceInfo;
	size_t deviceInfoSize;

	/* CPU/GPU choice */
	printf("Please choose a calculation device:\n[0] CPU\n[1] GPU\n");
	scanf_s("%d", &devChoice);
	auto cl_dev_type = (devChoice == 0 ? CL_DEVICE_TYPE_CPU : CL_DEVICE_TYPE_GPU);

	if (devChoice < 0 || devChoice > 1) {
		printf("Invalid CPU/GPU choice, defaulting to GPU");
	}

	/* Get Platform/Device Information */
	err = clGetPlatformIDs(0, 0, &ret_num_platforms);
	checkError(err, "Couldn't get platform count");

	/* GPU Platform choice */
	if (cl_dev_type == CL_DEVICE_TYPE_GPU) {
		getPlatforms();
		printf("Select platform\n");
		scanf_s("%d", &platformChoice);
		if (platformChoice >= ret_num_platforms) {
			platformChoice = 0;
			printf("Invalid index, selecting 0\n");
		}

	}
	else {
		platformChoice = 0;
	}

	platforms = new cl_platform_id[ret_num_platforms];
	err = clGetPlatformIDs(ret_num_platforms, platforms, &ret_num_platforms);
	checkError(err, "Couldn't get platform IDs");
	err = clGetDeviceIDs(platforms[platformChoice], cl_dev_type, 1, &device_id, &ret_num_devices);
	checkError(err, "Couldn't get device IDs");
	err = clGetDeviceInfo(device_id, CL_DEVICE_NAME, 0, NULL, &deviceInfoSize);
	checkError(err, "Couldn't get device info size");
	deviceInfo = new char[deviceInfoSize];
	err = clGetDeviceInfo(device_id, CL_DEVICE_NAME, deviceInfoSize, deviceInfo, NULL);
	checkError(err, "Couldn't get device info");
	printf("Using device %s\n", deviceInfo);

	/* Create OpenCL Context */
	context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &err);
	checkError(err, "Couldn't create OpenCL context");

	/*Create command queue */
	command_queue = clCreateCommandQueue(context, device_id, CL_QUEUE_PROFILING_ENABLE, &err);
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
	program = build_program(context, device_id, FILENAME);

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


	cl_event timingEvent;
	cl_ulong clStartTime, clEndTime;

	double elapsed = 0;

	// Get current time before calculating the fractal
	QueryPerformanceCounter(&start_fractal);
	err = clEnqueueNDRangeKernel(command_queue, kernel, 2, NULL, global_item_size, local_item_size, 0, NULL, &timingEvent);
	clFinish(command_queue);
	QueryPerformanceCounter(&end_fractal);
	checkError(err, "EnqueueNDRangeKernel error");

	err = clGetEventProfilingInfo(timingEvent, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &clStartTime, NULL);
	checkError(err, "Couldn't get start time");
	err = clGetEventProfilingInfo(timingEvent, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &clEndTime, NULL);
	checkError(err, "Couldn't get end time");
	elapsed += (clEndTime - clStartTime);

	err = clEnqueueReadBuffer(command_queue, cl_framebuffer, CL_TRUE, 0, sizeof(mandelbrot_color) * window_width * window_height, framebuffer, 0, NULL, NULL);
	checkError(err, "Couldn't read result");

	QueryPerformanceCounter(&end_read);
	
	printf("Copy to: %f msec\n", (double)(end_copy.QuadPart - start_copy.QuadPart) / freq.QuadPart * 1000.0);
	printf("Kernel : %f msec\n", elapsed / 1000000.0f);
	printf("K+Read : %f msec\n", (double)(end_read.QuadPart - start_fractal.QuadPart) / freq.QuadPart * 1000.0);

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
