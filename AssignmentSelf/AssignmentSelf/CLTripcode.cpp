
#include <stdio.h>	
#include <stdlib.h>
#include "opencl_utils.h"
#include "CLGetPlatforms.hpp"

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>	
#endif

#include <windows.h>
#include <algorithm>
#include <ctime>

//http://stackoverflow.com/questions/1640258/need-a-fast-random-generator-for-c
static unsigned long x = 123456789, y = 362436069, z = 521288629;
unsigned long xorshf96(void) {          //period 2^96-1
	unsigned long t;
	x ^= x << 16;
	x ^= x >> 5;
	x ^= x << 1;

	t = x;
	x = y;
	y = z;
	z = t ^ x ^ y;

	return z;
}

bool isPow2(unsigned long long int x) {
	return (x != 0) && ((x & (x - 1)) == 0);
}

//2x faster
//http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
unsigned long long roundUpPow2(unsigned long long v) {
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v |= v >> 32;
	v++;
	return v;
}

int main() {
	/* Init OpenCL variables */
	cl_platform_id *platforms;
	cl_device_id device_id = NULL;
	cl_context context;
	cl_command_queue command_queue;
	cl_program program;
	cl_kernel kernel;
	cl_uint ret_num_devices;
	cl_uint ret_num_platforms;
	cl_int err;
	cl_mem cl_gdata;

	/* Init rest of variables */
	int devChoice = -1;
	LARGE_INTEGER	freq, 
		//start_gpu, end_gpu, 
		start_cpu, end_cpu,
		start_generate, end_generate;
	QueryPerformanceFrequency(&freq);

	int platformChoice = -1;
	char *deviceInfo;
	size_t deviceInfoSize;

	char fileName[128]; //= "./kernel1.cl";
	char kernelName[128]; //= "kernel1";

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

	/* Create command queue */
	command_queue = clCreateCommandQueue(context, device_id, CL_QUEUE_PROFILING_ENABLE, &err);
	checkError(err, "Couldn't create command queue");

	/* Create Buffer Objects */
	cl_gdata = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(int) * arraySize, NULL, &err);
	checkError(err, "Couldn't create buffer");

	/* Copy input data to the memory buffer */
	err = clEnqueueWriteBuffer(command_queue, cl_gdata, CL_TRUE, 0, sizeof(int) * arraySize, inputArray, 0, NULL, NULL);
	checkError(err, "Couldn't enqueue write buffer");

	/* Kernel choice */
	printf("Choose kernel 1, 2, or 3\n");
	int kernelChoice = -1;
	while (kernelChoice != 1 && kernelChoice != 2 && kernelChoice != 3) {
		scanf_s("%d", &kernelChoice);
		if (kernelChoice != 1 && kernelChoice != 2 && kernelChoice != 3) {
			printf("Invalid selection\n");
		}
	}

	snprintf(fileName, sizeof(fileName), "./kernel%d.cl", kernelChoice);
	snprintf(kernelName, sizeof(kernelName), "kernel%d", kernelChoice);

	/* Create kernel */
	program = build_program(context, device_id, fileName);

	/* Create OpenCL kernel*/
	kernel = clCreateKernel(program, kernelName, &err);
	checkError(err, "Couldn't create kernel");

	size_t workgroupSize;
	size_t maxWorkgroupSize;
	err = clGetKernelWorkGroupInfo(kernel, device_id, CL_KERNEL_WORK_GROUP_SIZE,
		sizeof(size_t), &maxWorkgroupSize, NULL);
	checkError(err, "Couldn't get workgroup size");

	printf("Array size              : %zu\n", arraySize);
	printf("Max workgroup size      : %zu\n", maxWorkgroupSize);
	
	bool correctWorkgroupSize = false;
	while (!correctWorkgroupSize) {
		printf("Choose workgroup size   : ");
		scanf_s("%zu", &workgroupSize);

		if (workgroupSize > maxWorkgroupSize) {
			printf("Workgroup size must be <= %zu!\n", maxWorkgroupSize);
		} 
		else if (!(arraySize % workgroupSize == 0)) {
			printf("Array size %% workgroup size must be 0!\n");
		}
		else {
			correctWorkgroupSize = true;
		}
		
	}
	printf("Using workgroup size    : %zu\n", workgroupSize);

	/* Set OpenCL kernel arguments */
	err = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&cl_gdata);
	checkError(err, "Couldn't set kernel argument 0");
	err = clSetKernelArg(kernel, 1, workgroupSize * sizeof(int), NULL);
	checkError(err, "Couldn't set kernel argument 1");

	/* CPU work / calculate reference */
	QueryPerformanceCounter(&start_cpu);
	long long int reference = 0;
	for (int i = 0; i < arraySize; i++) {
		reference += inputArray[i];
	}
	QueryPerformanceCounter(&end_cpu);
	printf("CPU time  :    %f msec\n", (double)(end_cpu.QuadPart - start_cpu.QuadPart) / freq.QuadPart * 1000.0);
	printf("CPU Result:    %lld\n", reference);


	/* GPU work */
	size_t globalSize[1] = { arraySize };
	size_t localSize[1] = { workgroupSize };

	cl_event timingEvent;
	cl_ulong clStartTime, clEndTime;

	unsigned int iterations = 0;

	double elapsed = 0;

	while (globalSize[0] >= localSize[0] && localSize[0] > 1) {
		printf("Iteration %d: Problems: %llu Workgroup sz: %llu\n", iterations, globalSize[0], localSize[0]);

		err = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL,
			globalSize, localSize, 0, NULL, &timingEvent);
		checkError(err, "Couldn't enqueue command queue");
		clFinish(command_queue);

		/*err = clGetEventProfilingInfo(timingEvent, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &clStartTime, NULL);
		checkError(err, "Couldn't get start time");
		err = clGetEventProfilingInfo(timingEvent, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &clEndTime, NULL);
		checkError(err, "Couldn't get end time");
		elapsed += (clEndTime - clStartTime);*/

		globalSize[0] /= localSize[0];
		if (kernelChoice == 3) {
			globalSize[0] /= 2;
		}

		if (globalSize[0] < localSize[0]) {
			localSize[0] = globalSize[0];
		}
		//printf("Iteration %d: Problems: %llu Workgroup sz: %llu\n", iterations, globalSize[0], localSize[0]);

		iterations++;
	}

	printf("Kernel called %d times.\n",iterations);


	err = clEnqueueReadBuffer(command_queue, cl_gdata, CL_TRUE, 0, 
		sizeof(int) * arraySize, result, 0, NULL, &timingEvent);

	printf("Kernel time:   %f msec\n", elapsed / 1000000.0f);
	printf("GPU Result:    %d\n", result[0]);


	clFlush(command_queue);
	clFinish(command_queue);
	clReleaseKernel(kernel);
	clReleaseProgram(program);
	clReleaseMemObject(cl_gdata);
	clReleaseCommandQueue(command_queue);
	clReleaseContext(context);

	delete[] platforms;
	delete[] deviceInfo;
		   
	printf("Press RETURN to exit\n");
	char ch;
	scanf_s("%c", &ch);
	getchar();

	return 0;
}