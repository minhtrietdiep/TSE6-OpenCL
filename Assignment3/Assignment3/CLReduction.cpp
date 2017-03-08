
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

#define ARRAYSIZE   1 * 1024

int main() {
	/* Init OpenCL variables */
	cl_platform_id *platforms = nullptr;
	cl_device_id device_id = nullptr;
	cl_context context = nullptr;
	cl_command_queue command_queue = nullptr;
	cl_program program = nullptr;
	cl_kernel kernel = nullptr;
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
	int result[2];
	int *inputArray = new int(ARRAYSIZE);

	QueryPerformanceCounter(&start_generate);
	for (int i = 0; i < ARRAYSIZE; i++)
	{
		inputArray[i] = rand();
	}
	QueryPerformanceCounter(&end_generate);
	printf("Generate  :    %f msec\n", (double)(end_generate.QuadPart - start_generate.QuadPart) / freq.QuadPart * 1000.0);

	/* TODO: Kernel selection */
	// TODO: CODE
	
	/* Create kernel */
	program = build_program(context, device_id, fileName);

	/* Create OpenCL kernel*/
	kernel = clCreateKernel(program, kernelName, &err);
	checkError(err, "Couldn't create kernel");

	/* Create Buffer Objects */
	cl_gdata = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(inputArray) * ARRAYSIZE, NULL, &err);
	checkError(err, "Couldn't create buffer");

	/* Copy input data to the memory buffer */
	err = clEnqueueWriteBuffer(command_queue, cl_gdata, CL_TRUE, 0, sizeof(inputArray) * ARRAYSIZE, inputArray, 0, NULL, NULL);
	checkError(err, "Couldn't enqueue write buffer");

	size_t workgroupSize;
	err = clGetKernelWorkGroupInfo(kernel, device_id, CL_KERNEL_WORK_GROUP_SIZE,
		sizeof(size_t), &workgroupSize, NULL);
	checkError(err, "Couldn't get workgroup size");
	printf("Supported workgroup size: %zu\n", workgroupSize);
	
	// TODO: Workgroup choice
	workgroupSize = 256;
	printf("Using                   : %zu\n", workgroupSize);

	/* Set OpenCL kernel arguments */
	err = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&cl_gdata);
	checkError(err, "Couldn't set kernel argument 0");
	err = clSetKernelArg(kernel, 1, workgroupSize * sizeof(int), NULL);
	checkError(err, "Couldn't set kernel argument 1");

	/* CPU work / calculate reference */
	QueryPerformanceCounter(&start_cpu);
	long long int reference = 0;
	for (int i = 0; i < ARRAYSIZE; i++) {
		reference += inputArray[i];
	}
	QueryPerformanceCounter(&end_cpu);
	printf("CPU time  :    %f msec\n", (double)(end_cpu.QuadPart - start_cpu.QuadPart) / freq.QuadPart * 1000.0);
	printf("CPU Result:    %lld\n", reference);


	/* GPU work */
	size_t globalSize[1] = { ARRAYSIZE };
	size_t localSize[1] = { workgroupSize };

	cl_event timingEvent;
	cl_ulong clStartTime, clEndTime;
	while (globalSize[0] >= localSize[0] && localSize[0] > 1) {

		err = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL,
			globalSize, localSize, 0, NULL, &timingEvent);

		globalSize[0] /= localSize[0];
		if (globalSize[0] < localSize[0]) {
			localSize[0] = globalSize[0];
		}	
	}


	clFinish(command_queue);

	err = clEnqueueReadBuffer(command_queue, cl_gdata, CL_TRUE, 0, 
		sizeof(result), result, 0, NULL, &timingEvent);

	clFlush(command_queue);
	clFinish(command_queue);
	clReleaseKernel(kernel);
	clReleaseProgram(program);
	clReleaseMemObject(cl_gdata);
	clReleaseCommandQueue(command_queue);
	clReleaseContext(context);
	
	clGetEventProfilingInfo(timingEvent, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &clStartTime, NULL);
	clGetEventProfilingInfo(timingEvent, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &clEndTime, NULL);
	printf("GPU time  :    %llu msec\n", (clEndTime - clStartTime)/1000000);
	printf("GPU Result:    %d\n", result[0]);

	printf("Press RETURN to exit\n");
	char ch;
	scanf_s("%c", &ch);
	getchar();

	return 0;
}
