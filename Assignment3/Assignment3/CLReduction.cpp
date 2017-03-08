
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

bool isPow2(unsigned long long int x) {
	return (x != 0) && ((x & (x - 1)) == 0);
}

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
	size_t arraySizeContent = 1024;

	printf("Choose array size: \n");

	bool acceptableSize = false;
	while (!acceptableSize) {
		scanf_s("%d", &arraySizeContent);
		if (arraySizeContent > 1024 * 1024 * 128) {
			printf("Easy there m8\n");
		} else {
			acceptableSize = true;
		}
	}

	size_t arraySize = arraySizeContent;
	bool isPower2 = isPow2(arraySizeContent);

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

	char fileName[] = "./kernel1.cl";
	char kernelName[] = "kernel1";

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

	/* Create data */
	printf("Generating data...\n");
	
	// Oops, we probably want to round this or something...
	//http://developer.amd.com/resources/articles-whitepapers/opencl-optimization-case-study-simple-reductions/
	// Just pad everything with zeroes...
	if (!isPower2) {
		arraySize = roundUpPow2(arraySizeContent);
	}

	int *inputArray = new int[arraySize];
	int *result = new int[arraySize];
	
	srand(time(NULL));
	int min = -100;
	int max = 100;

	QueryPerformanceCounter(&start_generate);
	for (int i = 0; i < arraySize; i++)
	{
		if (i < arraySizeContent)
			inputArray[i] = rand() % (max - min + 1) + min;
		else
			inputArray[i] = 0;
	}
	QueryPerformanceCounter(&end_generate);
	printf("Generate  :    %f msec\n", (double)(end_generate.QuadPart - start_generate.QuadPart) / freq.QuadPart * 1000.0);
	
	/* Create Buffer Objects */
	cl_gdata = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(int) * arraySize, NULL, &err);
	checkError(err, "Couldn't create buffer");

	/* Copy input data to the memory buffer */
	err = clEnqueueWriteBuffer(command_queue, cl_gdata, CL_TRUE, 0, sizeof(int) * arraySize, inputArray, 0, NULL, NULL);
	checkError(err, "Couldn't enqueue write buffer");

	/* TODO: Kernel selection */
	// TODO: CODE

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
		printf("Iteration %d: Problems: %d Workgroup sz: %d\n", iterations, globalSize[0], localSize[0]);

		err = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL,
			globalSize, localSize, 0, NULL, &timingEvent);
		checkError(err, "Couldn't enqueue command queue");
		clFinish(command_queue);

		err = clGetEventProfilingInfo(timingEvent, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &clStartTime, NULL);
		checkError(err, "Couldn't get start time");
		err = clGetEventProfilingInfo(timingEvent, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &clEndTime, NULL);
		checkError(err, "Couldn't get end time");
		elapsed += (clEndTime - clStartTime);

		globalSize[0] /= localSize[0];
		if (globalSize[0] < localSize[0]) {
			localSize[0] = globalSize[0];
		}
		iterations++;
	}

	printf("Kernel called %d times.\n",iterations);


	err = clEnqueueReadBuffer(command_queue, cl_gdata, CL_TRUE, 0, 
		sizeof(int) * arraySize, result, 0, NULL, &timingEvent);

	printf("GPU time  :    %f msec\n", elapsed / 1000000.0f);
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
	delete[] inputArray;
	delete[] result;
		   
	printf("Press RETURN to exit\n");
	char ch;
	scanf("%c", &ch);
	getchar();

	return 0;
}
