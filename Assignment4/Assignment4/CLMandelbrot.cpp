
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

double OFFSET_X = 0;
double OFFSET_Y = 0;
const unsigned int ZOOMFACTOR = 200;
const unsigned int MAX_ITERATIONS = 512; // because slow laptop :(
const unsigned int COLORTABLE_SIZE = 2048;

float stepsize = 1.0f / ZOOMFACTOR;

mandelbrot_color colortable[COLORTABLE_SIZE];

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

size_t global_item_size[2];
size_t local_item_size[2];

GLuint gl_tex;
cl_mem cl_tex = NULL;
cl_mem cl_stepsize = NULL;

cl_mem cl_offset_x = NULL;
cl_mem cl_offset_y = NULL;
cl_mem cl_maxiterations = NULL;
cl_mem cl_window_width = NULL;
cl_mem cl_window_height = NULL;
cl_mem cl_colortable = NULL;

unsigned long long curr, prev;
double walksize = 0.1;
int runs = 0;
double zoomspeed = 0.05;

void handle_keys(unsigned char key, int x, int y) {
	switch(key) {
	case '=':
	case '+':
		zoomspeed *= 1.10;
		break;
	case '-':
	case '_':
		zoomspeed *= 0.90;
		break;
	case 'r':
		OFFSET_X = 0;
		OFFSET_Y = 0;
		stepsize = 1.0f / ZOOMFACTOR;
		zoomspeed = 0.05;
		break;
	}
}

bool zoom_in;

bool zoom_out;

void handle_mouse(int button, int state,
	int x, int y) {
	if (button == 3) { // scroll up
		zoomspeed *= 1.10;
	}
	if (button == 4) { // scroll dn
		zoomspeed *= 0.90;
	}
	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
		zoom_in = true;
	}
	if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
		zoom_in = false;
	}
	if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) {
		zoom_out = true;
	}
	if (button == GLUT_RIGHT_BUTTON && state == GLUT_UP) {
		zoom_out = false;
	}
	if (button == GLUT_MIDDLE_BUTTON && state == GLUT_DOWN) {
		printf("    x: %d,     y: %d\n", x, y);
		double newX = ((double)x - (double)window_width / 2.0) / ((double)window_width / 2.0);
		double newY = ((double)y - (double)window_height / 2.0) / ((double)window_height / 2.0);
		printf("new_x: %f, new_y: %f\n", newX, newY);
		OFFSET_X += -newX * stepsize*ZOOMFACTOR;
		OFFSET_Y += newY * stepsize*ZOOMFACTOR;
	}
}

void display() {
	glFinish();
	curr = glutGet(GLUT_ELAPSED_TIME); 
	if (zoom_in) {
		stepsize = stepsize * pow(1.0-zoomspeed, (curr - prev) / 100.0);
	}
	if (zoom_out) {
		stepsize = stepsize * pow(1.0+zoomspeed, (curr - prev) / 100.0);
	}
	prev = curr;

	err = clEnqueueWriteBuffer(command_queue, cl_offset_x, CL_TRUE, 0, sizeof(double), &OFFSET_X, 0, NULL, NULL);
	checkError(err, "Couldn't enqueue off_x");
	err = clEnqueueWriteBuffer(command_queue, cl_offset_y, CL_TRUE, 0, sizeof(double), &OFFSET_Y, 0, NULL, NULL);
	checkError(err, "Couldn't enqueue off_y");
	err = clEnqueueWriteBuffer(command_queue, cl_stepsize, CL_TRUE, 0, sizeof(float), &stepsize, 0, NULL, NULL);
	checkError(err, "Couldn't write step size");

	// Acquire OpenCL access to the texture   
	err = clEnqueueAcquireGLObjects(command_queue, 1, &cl_tex, 0, NULL, NULL);
	checkError(err, "Couldn't acquire OpenCL access to the texture");

	// set args
	err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &cl_offset_x);
	checkError(err, "Couldn't set kernel argument");
	err = clSetKernelArg(kernel, 1, sizeof(cl_mem), &cl_offset_y);
	checkError(err, "Couldn't set kernel argument");
	err = clSetKernelArg(kernel, 2, sizeof(cl_mem), &cl_stepsize);
	checkError(err, "Couldn't set step size");

	global_item_size[0] = window_width;
	global_item_size[1] = window_height;
	// How to find optimal size? 25*30 or 40*20 is slow for example, while that
	// is the closest to the reported 1024 reported work group size (GT630M)
	local_item_size[0] = 25;
	local_item_size[1] = 20;

	// OpenCL Kernel is started
	cl_int err = clEnqueueNDRangeKernel(command_queue, kernel, 2, NULL, global_item_size, local_item_size, 0, NULL, NULL);
	checkError(err, "clEnqueueNDRangeKernel error");
	clFinish(command_queue);

	// Give the texture back to OpenGL from OpenCL
	clEnqueueReleaseGLObjects(command_queue, 1, &cl_tex, 0, NULL, NULL);
	checkError(err, "clEnqueueReleaseGLObjects error");

	//Wait until object is actually released by OpenCL
	clFinish(command_queue);
	
	// Draw quad, with the texture mapped on it in OPENGL
	draw_quad();

	// Request another redisplay of the window
	glutPostRedisplay();
}

int main(int argc, char **argv)
{
	init_gl(window_width, window_height);

	// Create the colortable and fill it with colors
	create_colortable(colortable, COLORTABLE_SIZE);

	glutInit(&argc, argv);
	glutInitWindowSize(window_width, window_height);
	glutInitWindowPosition(100, 100);
	glutCreateWindow("GLCLMandelBrot");

	/* Init rest of variables */
	//LARGE_INTEGER freq, start_fractal, start_copy, end_fractal, end_read, end_copy;
	//QueryPerformanceFrequency(&freq);
	int devChoice = -1;
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
		if (platformChoice >= (int)ret_num_platforms) {
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
	cl_context_properties properties[] = {
		CL_GL_CONTEXT_KHR, reinterpret_cast<cl_context_properties>(wglGetCurrentContext()),
		CL_WGL_HDC_KHR, reinterpret_cast<cl_context_properties>(wglGetCurrentDC()),
		0
	};

	context = clCreateContext(properties, 1, &device_id, NULL, NULL, &err);
	checkError(err, "Couldn't create OpenCL context");

	/*Create command queue */
	command_queue = clCreateCommandQueue(context, device_id, NULL, &err);
	checkError(err, "Couldn't create command queue");

	/*Create Buffer Objects */
	cl_offset_x		= clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(double), NULL, &err);
	checkError(err, "Couldn't create buffer");
	cl_offset_y		= clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(double), NULL, &err);
	checkError(err, "Couldn't create buffer");
	cl_stepsize		= clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(float), NULL, &err);
	checkError(err, "Couldn't create buffer");
	cl_maxiterations	= clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(unsigned int), NULL, &err);
	checkError(err, "Couldn't create buffer");
	cl_window_width		= clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(unsigned int), NULL, &err);
	checkError(err, "Couldn't create buffer");
	cl_window_height	= clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(unsigned int), NULL, &err);
	checkError(err, "Couldn't create buffer");

	// Create new table on device
	cl_colortable = clCreateBuffer(context, CL_MEM_READ_ONLY,
		COLORTABLE_SIZE * sizeof(mandelbrot_color), NULL, &err);
	checkError(err, "Couldn't create colortable on device");

	/* Copy input data to the memory buffer */
	err = clEnqueueWriteBuffer(command_queue, cl_offset_x, CL_TRUE, 0, sizeof(float), &OFFSET_X, 0, NULL, NULL);
	checkError(err, "Couldn't enqueue off_x");
	err = clEnqueueWriteBuffer(command_queue, cl_offset_y, CL_TRUE, 0, sizeof(float), &OFFSET_Y, 0, NULL, NULL);
	checkError(err, "Couldn't enqueue off_y");
	err = clEnqueueWriteBuffer(command_queue, cl_maxiterations, CL_TRUE, 0, sizeof(unsigned int),&MAX_ITERATIONS, 0, NULL, NULL);
	checkError(err, "Couldn't enqueue max_it");
	err = clEnqueueWriteBuffer(command_queue, cl_window_width, CL_TRUE, 0, sizeof(unsigned int), &window_width, 0, NULL, NULL);
	checkError(err, "Couldn't enqueue window_w");
	err = clEnqueueWriteBuffer(command_queue, cl_window_height, CL_TRUE, 0, sizeof(unsigned int),&window_height, 0, NULL, NULL);
	checkError(err, "Couldn't enqueue window_h");
	err = clEnqueueWriteBuffer(command_queue, cl_colortable, CL_TRUE, 0, COLORTABLE_SIZE * sizeof(mandelbrot_color), colortable, 0, NULL, NULL);
	checkError(err, "Couldn't write colortable to device");
	err = clEnqueueWriteBuffer(command_queue, cl_stepsize, CL_TRUE, 0, sizeof(float), &stepsize, 0, NULL, NULL);
	checkError(err, "Couldn't write step size");

	/* Build kernel */
	program = build_program(context, device_id, FILENAME);

	/* Create OpenCL kernel*/
	kernel = clCreateKernel(program, KERNELNAME, &err);
	checkError(err, "Couldn't create kernel");

	gl_tex = init_gl(window_width, window_height);
	cl_tex = clCreateFromGLTexture2D(context, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, gl_tex, &err);
	checkError(err, "Couldn't create cl_tex");

	err = clSetKernelArg(kernel, 3, sizeof(cl_mem), &cl_maxiterations);
	checkError(err, "Couldn't set kernel argument");
	err = clSetKernelArg(kernel, 4, sizeof(cl_mem), &cl_tex);
	checkError(err, "Couldn't set kernel argument");
	err = clSetKernelArg(kernel, 5, sizeof(cl_mem), &cl_colortable);
	checkError(err, "Couldn't set kernel argument");
	err = clSetKernelArg(kernel, 6, sizeof(cl_mem), &cl_window_width);
	checkError(err, "Couldn't set kernel argument");
	err = clSetKernelArg(kernel, 7, sizeof(cl_mem), &cl_window_height);
	checkError(err, "Couldn't set kernel argument");


	glutKeyboardFunc(handle_keys);
	glutMouseFunc(handle_mouse);
	glutDisplayFunc(display);
	glutMainLoop();

	return 0;
}
