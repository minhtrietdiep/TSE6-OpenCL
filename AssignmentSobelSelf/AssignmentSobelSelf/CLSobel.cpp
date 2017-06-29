#include <iostream>
#include <string>

#include <stdio.h>	
#include <windows.h>
#include <stdlib.h>

#include <CL/cl.h>
#include "opencl_utils.h"
#include "CLGetPlatforms.hpp"

#include "ImageUtils.h"


// Types:
typedef unsigned int uint;
typedef	uint SobelFlag;
#define NONE			0x0
#define SINGLEREAD		0x1
#define NONCOALESCED	0x2
#define BLOCKED			0x4
#define NEWLINE			0x8
#define MEGABLOCK		0x10

#define READ_ALIGNMENT  4096 // Intel recommended alignment
#define WRITE_ALIGNMENT 4096 // Intel recommended alignment

#define MAX_TEST_KERNELS 100

#define FILENAME "./KernelSobel.cl"
#define KERNELNAME "Sobel"

typedef struct
{
	char				pKernelName[BUFSIZ];
	uint				numBytesPerWorkItemRead;
	cl_kernel			kernelHdl;
	SobelFlag			testFlag;
} SobelKernel;

typedef struct
{
	// CL platform handles:
	cl_device_id		deviceID;
	cl_context			contextHdl;
	cl_program			programHdl;
	cl_command_queue	cmdQHdl;

	cl_mem buffIn;
	cl_mem buffOut;

	SobelKernel	    	pTests[MAX_TEST_KERNELS];
	uint				numTests;
} OCLResources;

int main(int argc, char **argv)
{
	/* Init OpenCL variables */
	cl_platform_id *platforms;
	cl_device_id device_id = nullptr;
	cl_context context;
	cl_command_queue command_queue;

	cl_program program;
	cl_kernel kernel;
	cl_uint ret_num_devices;
	cl_uint ret_num_platforms;
	cl_int err;

	/* CL Vars */
	cl_mem buffIn;
	cl_mem buffOut;

	// Image data:
	const char*		pInputImg1Str = "Henri2048x2048.ppm";
	uint			height, width;
	uint			heightReSz, widthReSz;
	uint			szRGBImgPixelBytes;
	uint			szGrayImgBytes, szGrayImgBytesReSz;
	U8*				pRGBImgData;
	U8*				pGrayImgData;
	U8*				pGrayImgDataReSz;

	heightReSz = 2048;
	widthReSz= 2048;

	/* Init devices*/
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
	context = clCreateContext(nullptr, 1, &device_id, NULL, NULL, &err);
	checkError(err, "Couldn't create OpenCL context");

	/*Create command queue */
	command_queue = clCreateCommandQueue(context, device_id, NULL, &err);
	checkError(err, "Couldn't create command queue");
	
	/* Build kernel */
	program = build_program(context, device_id, FILENAME);

	/* Create OpenCL kernel*/
	kernel = clCreateKernel(program, KERNELNAME, &err);
	checkError(err, "Couldn't create kernel");
	std::cout << "Kernel created" << std::endl;

	// Load rgb ppm image, convert to U8 grayscale:
	loadppm_toU8RGB (pInputImg1Str, &width, &height, &pRGBImgData, &szRGBImgPixelBytes);
	szGrayImgBytes	= width * height;
	pGrayImgData	= (U8*)_aligned_malloc (szGrayImgBytes, READ_ALIGNMENT);
	convertU8RGB_to_U8Gray (width, height, pRGBImgData, pGrayImgData);

	// Resize:
	szGrayImgBytesReSz = (widthReSz+32) * (heightReSz+2); // pad the image top, bottom and on the sides
	pGrayImgDataReSz = (U8*)_aligned_malloc (szGrayImgBytesReSz, READ_ALIGNMENT);
	resizeU8Gray (width, height, widthReSz+32, heightReSz+2, pGrayImgData, pGrayImgDataReSz);

	// Allocate resources
	buffIn = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, szGrayImgBytesReSz, pGrayImgDataReSz, &err);
	checkError(err, "clCreateBuffer failed.", "clCreateBuffer.");

	buffOut = clCreateBuffer(context, CL_MEM_WRITE_ONLY, szGrayImgBytesReSz, NULL, &err);
	checkError(err, "clCreateBuffer failed.", "clCreateBuffer.");

	// perform run
	size_t		dimNDR[2];
	size_t		dimWG[2];
	dimWG[0] = 32;
	dimWG[1] = 8;
	auto numBytesPerWorkItemRead = 1 * sizeof(unsigned char);	// for uchar / SINGLEREAD
	dimNDR[0] = widthReSz / numBytesPerWorkItemRead;
	dimNDR[1] = heightReSz;										// for uchar / SINGLEREAD

	// Make sure output buffer is cleared/zeroed  first, from last use:
	U8* pU8GrayData = (U8*)_aligned_malloc(heightReSz*widthReSz, READ_ALIGNMENT);
	memset(pU8GrayData, 0, heightReSz*widthReSz);
	cl_event clearCompleted;
	err = clEnqueueWriteBuffer(command_queue, buffOut, true, 0, heightReSz*widthReSz, pU8GrayData, 0, NULL, &clearCompleted);
	checkError(err, "clEnqueueWriteBuffer failed.", "clEnqueueWriteBuffer");
	err = clWaitForEvents(1, &clearCompleted);			// Catch completion event:
	checkError(err, "clWaitForEvents failed.", "clWaitForEvents");
	_aligned_free(pU8GrayData);

	err |= clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&buffIn);
	err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), (void*)&buffOut);
	checkError(err, "clSetKernelArg failed.", "clSetKernelArg");

	// Do the run
	err = clEnqueueNDRangeKernel(command_queue, kernel, 2, NULL, dimNDR, 0, 0, NULL, 0);
	clFinish(command_queue);
	std::cout << "cl Run done" << std::endl;
	
	{
		U8* pU8GrayDataWrite = (U8*)_aligned_malloc(heightReSz*widthReSz, WRITE_ALIGNMENT);
		memset(pU8GrayDataWrite, 0, heightReSz*widthReSz);
		U8* pU8RGBDataWrite = (U8*)_aligned_malloc(heightReSz*widthReSz * 3, WRITE_ALIGNMENT);
		memset(pU8RGBDataWrite, 0, heightReSz*widthReSz * 3);
		err = clEnqueueReadBuffer(command_queue, buffOut, true, 0, heightReSz*widthReSz, pU8GrayDataWrite, 0, NULL, NULL);
		checkError(err, "clEnqueueReadBuffer failed.");
		convertU8Gray_to_U8RGB(widthReSz, heightReSz, pU8GrayDataWrite, pU8RGBDataWrite);

		char				pValidationFileName[BUFSIZ];
		sprintf_s(pValidationFileName, BUFSIZ, "%s_validation.ppm", "output");
		saveppm_fromU8RGB(pValidationFileName, widthReSz, heightReSz, pU8RGBDataWrite);
		_aligned_free(pU8GrayDataWrite);
		_aligned_free(pU8RGBDataWrite);
	}
	
	_aligned_free(pGrayImgData);
	_aligned_free(pGrayImgDataReSz);

	std::cout << "Finished! Press Enter to continue." << std::endl;
	std::string dump;
	std::cin.ignore();
	std::getline(std::cin, dump);
	delete[] platforms;
	return 0;
}
