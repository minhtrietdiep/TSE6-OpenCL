
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
#include "color_table.h"
#include "OpenGL_functions.h"

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

#include <GL/freeglut.h>

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
cl_mem cl_stepsize;

size_t global_item_size[2];
size_t local_item_size[2];

void display() {
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f);
	glVertex3f(-1.0f, -1.0f, 0.1f);
	glTexCoord2f(1.0f, 0.0f);
	glVertex3f(1.0f, -1.0f, 0.1f);
	glTexCoord2f(1.0f, 1.0f);
	glVertex3f(1.0f, 1.0f, 0.1f);
	glTexCoord2f(0.0f, 1.0f);
	glVertex3f(-1.0f, 1.0f, 0.1f);
	glEnd();

	clSetKernelArg(kernel, 4, sizeof(cl_mem), NULL);

	//err = clEnqueueWriteBuffer(command_queue, cl_stepsize, CL_TRUE, 0, sizeof(float), (void *)&stepsize, 0, NULL, NULL);
	//checkError(err, "Couldn't enqueue write buffer");

	//err = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *)&cl_stepsize);
	//checkError(err, "Couldn't set kernel argument");
	//auto tex_width = window_width;
	//auto tex_height = window_height;
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex_width, tex_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	//GLuint texture;
	//glGenTextures(1, &texture);
	//glBindTexture(GL_TEXTURE_2D, texture);

	//cl_mem cl_tex = clCreateFromGLTexture2D(context, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, texture, &err);
	//// Acquire OpenCL access to the texture   
	//clEnqueueAcquireGLObjects(command_queue, 1, &cl_tex, 0, NULL, NULL);

	//clSetKernelArg(kernel, 8, sizeof(cl_mem), (void *)&cl_tex);
	//cl_int err = clEnqueueNDRangeKernel(command_queue, kernel, 2, NULL, global_item_size, local_item_size, 0, NULL, NULL);
	//checkError(err, "EnqueueNDRangeKernel error");

	//// Give the texture back to OpenGL
	//clEnqueueReleaseGLObjects(command_queue, 1, &cl_tex, 0, NULL, NULL);

	////Wait until object is actually released by OpenCL
	//clFinish(command_queue);
	
	draw_quad();

	// Draw quad, with the texture mapped on it in OPENGL
	//......

	// Request another redisplay of the window
	glutPostRedisplay();
}


//int main(int argc, char** argv) {
//	glutInit(&argc, argv);
//	//glutInitDisplayMode(GLUT_SINGLE);
//	glutInitWindowSize(300, 300);
//	//glutInitWindowPosition(100, 100);
//	glutCreateWindow("Hello world");
//	glutDisplayFunc(display);
//	glutMainLoop();
//	return 0;
//}


int main()
{
	init_gl(window_width, window_height);

	// Create the colortable and fill it with colors
	create_colortable(colortable2, COLORTABLE_SIZE);

	// Create an empty image
	mandelbrot_color * framebuffer = (mandelbrot_color *)(window_width * window_height);

	/* Init rest of variables */
	int devChoice = -1;
	LARGE_INTEGER freq, start_fractal, start_copy, end_fractal, end_read, end_copy;
	QueryPerformanceFrequency(&freq);

	int platformChoice = -1;
	char *deviceInfo;
	size_t deviceInfoSize;

	/* CPU/GPU choice */
	printf("Please choose a calculation device:\n[0] CPU\n[1] GPU\n");
	//scanf_s("%d", &devChoice);
	devChoice = 1;
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
		//scanf_s("%d", &platformChoice);
		platformChoice = 1;
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
	/*cl_mem */cl_stepsize		= clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float), NULL, &err);
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

	global_item_size[0] = window_width;
	global_item_size[1] = window_height;
	// How to find optimal size? 25*30 or 40*20 is slow for example, while that
	// is the closest to the reported 1024 reported work group size (GT630M)
	local_item_size[0] = 25;
	local_item_size[1] = 20;

	err = clSetKernelArg(kernel, 4, sizeof(cl_mem), (void *)&cl_framebuffer);
	checkError(err, "Couldn't set kernel argument");
	
	glutCreateWindow("Hello world");
	glutDisplayFunc(display);
	glutMainLoop();

	return 0;
}
