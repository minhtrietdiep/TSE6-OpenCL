// SobelMain.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include <windows.h>
#include <tchar.h>
#include <assert.h>
#include <string.h>

#include <cl/cl.h>
#include "OpenCLUtils.h"
#include "ImageUtils.h"
#include <math.h>


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

// Globals:
cl_int		ciErrNum;

void Cleanup(OCLResources* pOCL, int iExitCode, bool bExit, char* optionalErrorMessage)
{
	if (optionalErrorMessage) printf ("%s\n", optionalErrorMessage);

	memset(pOCL, 0, sizeof (OCLResources));

	// Release OpenCL cl_mem buffers, images:
	if (pOCL->buffIn)			{ clReleaseMemObject(pOCL->buffIn);				pOCL->buffIn=NULL; }
	if (pOCL->buffOut)			{ clReleaseMemObject(pOCL->buffOut);			pOCL->buffOut=NULL; }

	// Release all OpenCL kernels:
	for (uint k = 0; k < pOCL->numTests; k++)
		if (pOCL->pTests[k].kernelHdl)	{ clReleaseKernel(pOCL->pTests[k].kernelHdl);	pOCL->pTests[k].kernelHdl=NULL; }

	if (pOCL->programHdl)		{ clReleaseProgram(pOCL->programHdl);		pOCL->programHdl=NULL;	}
	if (pOCL->cmdQHdl)			{ clReleaseCommandQueue(pOCL->cmdQHdl);		pOCL->cmdQHdl=NULL;		}
	if (pOCL->contextHdl)		{ clReleaseContext(pOCL->contextHdl);		pOCL->contextHdl= NULL;	}

	if (bExit)
		exit (iExitCode);
}

void parseArgs(OCLResources* pOCL, int argc, char** argv, unsigned int* test_iterations, char* pDeviceStr, char* pVendorStr, unsigned int* widthReSz, unsigned int* heightReSz, bool* pbShowCL)
{	
	char*			pDeviceWStr = NULL;
	char*			pVendorWStr = NULL;
	const char sUsageString[BUFSIZ] = "Usage: Sobel [num test iterations] [cpu|gpu] [intel|amd|nvidia] [SurfWidth(^2 only)] [SurfHeight(^2 only)] [show_CL | no_show_CL]";
	
	if (argc != 7)
	{
		Cleanup (pOCL, -1, true, (char *) sUsageString);
	}
	else
	{
		*test_iterations	= atoi (argv[1]);
		pDeviceWStr			= argv[2];			// "cpu" or "gpu"	
		pVendorWStr			= argv[3];			// "intel" or "amd" or "nvidia"
		*widthReSz	= atoi (argv[4]);
		*heightReSz	= atoi (argv[5]);
		if (argv[6][0]=='s')
			*pbShowCL = true;
		else
			*pbShowCL = false;
	}
	sprintf_s(pDeviceStr, BUFSIZ, "%s", pDeviceWStr);
	sprintf_s(pVendorStr, BUFSIZ, "%s", pVendorWStr);
}

void AllocateOpenCLMemResources (OCLResources *pOCL, uint width, uint height, uint szGrayImgBytes, U8* pGrayImgData)
{	
	// Note: when allocation with clCreateBuffer & CL_MEM_USE_HOST_PTR be sure:
	// a) host memory allcoated with  _aligned_malloc (size, 4096)
	// b) total size of host memory is a multiple of pGfx cacheline size of 64B
	pOCL->buffIn		= clCreateBuffer(pOCL->contextHdl, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, szGrayImgBytes, pGrayImgData, &ciErrNum);
	CheckCLError (ciErrNum, "clCreateBuffer failed.", "clCreateBuffer.");

	pOCL->buffOut	= clCreateBuffer(pOCL->contextHdl, CL_MEM_WRITE_ONLY, szGrayImgBytes, NULL, &ciErrNum);
	CheckCLError (ciErrNum, "clCreateBuffer failed.", "clCreateBuffer.");
}

void InstantiateOpenCLKernels(OCLResources *pOCL)
{	
	int t = -1;   // initialize test count (t)

	// Buffer based buffer tests:
	++t; sprintf_s(pOCL->pTests[t].pKernelName, BUFSIZ, "Sobel_v1_uchar");		            pOCL->pTests[t].numBytesPerWorkItemRead = 1 * sizeof(unsigned char);	pOCL->pTests[t].testFlag = SINGLEREAD;
	++t; sprintf_s(pOCL->pTests[t].pKernelName, BUFSIZ, "Sobel_v2_uchar16");	            pOCL->pTests[t].numBytesPerWorkItemRead = 16 * sizeof(unsigned char);	pOCL->pTests[t].testFlag = SINGLEREAD;
	++t; sprintf_s(pOCL->pTests[t].pKernelName, BUFSIZ, "Sobel_v3_uchar16_to_float16");	    pOCL->pTests[t].numBytesPerWorkItemRead = 16 * sizeof(unsigned char);	pOCL->pTests[t].testFlag = SINGLEREAD;
	++t; sprintf_s(pOCL->pTests[t].pKernelName, BUFSIZ, "Sobel_v4_uchar16_to_float16_16");	pOCL->pTests[t].numBytesPerWorkItemRead = 16 * sizeof(unsigned char);	pOCL->pTests[t].testFlag = MEGABLOCK | NEWLINE;

	pOCL->numTests =	++t;

	// Instantiate kernels:
	for (uint i = 0; i < pOCL->numTests; i++)
	{
		pOCL->pTests[i].kernelHdl = clCreateKernel(pOCL->programHdl, pOCL->pTests[i].pKernelName, &ciErrNum);
		CheckCLError (ciErrNum, "Kernel creation failed.", "Kernel created.");
	}
}

void PerformSobelTests (OCLResources *pOCL,  uint iterations, uint wInit, uint h)
{
	LARGE_INTEGER beginClock, endClock, clockFreq;
	QueryPerformanceFrequency (&clockFreq);

	size_t		dimNDR[2];
	size_t		dimWG[2];

	printf ("\n");
	printf ("Test Image Size: %5d x %5d\n", wInit, h);
	printf ("Iterations, each test: %d\n", iterations);
	printf ("Sobel                                                                   Time/NDrng  StdDev-Time Speedup Estimated-BW StdDev-BW  Pixel-TPut\n");
	printf ("Name                                                                    [millisecs] [%%]                 [GB/sec]     [%%]        [Gpix/sec]\n");
	printf ("-----------                                                             ---------   ----------  ------- ----------   ---------- -----------\n");

	/////////////////////////////////////////////
	double firstKernelTime = 0;
	for (uint k = 0; k < pOCL->numTests; k++)
	{
		dimWG[0] = 32;
		dimWG[1] = 8;
		dimNDR[0]	= wInit / pOCL->pTests[k].numBytesPerWorkItemRead;

		if (pOCL->pTests[k].testFlag & SINGLEREAD)
		{
			dimNDR[1]	= h;	
		}
		else if(pOCL->pTests[k].testFlag & MEGABLOCK)
		{
			dimNDR[1]	= h/16;
		}

		// Make sure output buffer is cleared/zeroed  first, from last use:
		U8* pU8GrayData		= (U8*)_aligned_malloc (h*wInit, READ_ALIGNMENT);
		memset (pU8GrayData, 0, h*wInit);
		cl_event clearCompleted;
		ciErrNum = clEnqueueWriteBuffer(pOCL->cmdQHdl, pOCL->buffOut, true, 0, h*wInit, pU8GrayData, 0, NULL, &clearCompleted);
		CheckCLError(ciErrNum, "clEnqueueWriteBuffer failed.", "clEnqueueWriteBuffer");
		ciErrNum = clWaitForEvents (1, &clearCompleted);			// Catch completion event:
		CheckCLError(ciErrNum, "clWaitForEvents failed.", "clWaitForEvents");
		_aligned_free (pU8GrayData);

		ciErrNum |= clSetKernelArg(pOCL->pTests[k].kernelHdl, 0, sizeof(cl_mem), (void*) &pOCL->buffIn);
		ciErrNum |= clSetKernelArg(pOCL->pTests[k].kernelHdl, 1, sizeof(cl_mem), (void*) &pOCL->buffOut);
		CheckCLError(ciErrNum, "clSetKernelArg failed.", "clSetKernelArg");

		// Warmup
		for (uint i = 0; i < iterations; i++)
		{
			if (dimWG[0] != 0)
				ciErrNum = clEnqueueNDRangeKernel (pOCL->cmdQHdl, pOCL->pTests[k].kernelHdl, 2, NULL, dimNDR, dimWG, 0, NULL, 0);
			else 
				ciErrNum = clEnqueueNDRangeKernel (pOCL->cmdQHdl, pOCL->pTests[k].kernelHdl, 2, NULL, dimNDR, 0, 0, NULL, 0);
		}
		clFinish(pOCL->cmdQHdl);

        // now time for real
		const uint REPEATS = 30; 
		static double *averageTimes = new double[REPEATS];
		double smoothedAverageTime = 0;
		for (uint j = 0; j < REPEATS; j++) {
			QueryPerformanceCounter (&beginClock);
			for (uint i = 0; i < iterations; i++)
			{
				if (dimWG[0] != 0)
					ciErrNum = clEnqueueNDRangeKernel (pOCL->cmdQHdl, pOCL->pTests[k].kernelHdl, 2, NULL, dimNDR, dimWG, 0, NULL, 0);
				else 
					ciErrNum = clEnqueueNDRangeKernel (pOCL->cmdQHdl, pOCL->pTests[k].kernelHdl, 2, NULL, dimNDR, 0, 0, NULL, 0);
			}
			clFinish(pOCL->cmdQHdl);
			QueryPerformanceCounter (&endClock);
			double totalTime = double(endClock.QuadPart - beginClock.QuadPart) / clockFreq.QuadPart;
			averageTimes[j] = totalTime/iterations;
			smoothedAverageTime += averageTimes[j];
		}
		smoothedAverageTime /= REPEATS;
		double stddevSmoothedAverageTime = 0;
		for (uint j = 0; j < REPEATS; j++)
		{
			stddevSmoothedAverageTime += (averageTimes[j] - smoothedAverageTime)*(averageTimes[j] - smoothedAverageTime);
		}
		stddevSmoothedAverageTime /= (REPEATS - 1);
		stddevSmoothedAverageTime = sqrt(stddevSmoothedAverageTime);

		float dataSize = (float) wInit * h;
		
		static float *bandWidths = new float[iterations];
		float averageBandWidth = 0.0;
		float stddevBandWidth = 0.0;
		float	bandWidth			= (float) (dataSize / 1000000000.0 / smoothedAverageTime );  
		for(uint i = 0; i < REPEATS; i++)
		{
			bandWidths[i] = (float) (dataSize / 1000000000.0 / averageTimes[i] );
			averageBandWidth += bandWidths[i];
		}
		averageBandWidth /= REPEATS;
		for(uint i = 0; i < REPEATS; i++)
		{
			stddevBandWidth += (bandWidths[i] - averageBandWidth)*(bandWidths[i] - averageBandWidth);
		}
		stddevBandWidth /= (REPEATS - 1);
		stddevBandWidth = sqrt(stddevBandWidth);

		float	pixelTPut;
		pixelTPut			= (float) (dataSize / 1000000000.0 / smoothedAverageTime);

		if (k == 0)
			firstKernelTime = smoothedAverageTime;

		printf("%-70s %10.3f %10.1f %10.1f %10.3f %11.1f %11.3f  \n", 
			pOCL->pTests[k].pKernelName, smoothedAverageTime*1000, stddevSmoothedAverageTime/smoothedAverageTime*100.0, firstKernelTime/smoothedAverageTime, averageBandWidth, stddevBandWidth/averageBandWidth*100.0, pixelTPut);
		
		if (pOCL->pTests[k].testFlag & NEWLINE)
			printf ("\n");

		// VALIDATION:
		// dump a debug print of the output test image.
		{
			bool breakpoint = true;
			U8* pU8GrayData		= (U8*)_aligned_malloc (h*wInit, WRITE_ALIGNMENT);
			memset (pU8GrayData, 0, h*wInit);
			U8* pU8RGBData		= (U8*)_aligned_malloc (h*wInit*3, WRITE_ALIGNMENT);
			memset (pU8RGBData, 0, h*wInit*3);
			ciErrNum = clEnqueueReadBuffer(pOCL->cmdQHdl, pOCL->buffOut, true, 0, h*wInit, pU8GrayData, 0, NULL, NULL);
			CheckCLError(ciErrNum, "clEnqueueReadBuffer failed.", "clEnqueueReadBuffer");
			convertU8Gray_to_U8RGB (wInit, h, pU8GrayData, pU8RGBData);

			char				pValidationFileName[BUFSIZ];
			sprintf_s(pValidationFileName, BUFSIZ, "%s_validation.ppm", pOCL->pTests[k].pKernelName);
			saveppm_fromU8RGB(pValidationFileName, wInit, h, pU8RGBData);
			_aligned_free (pU8GrayData);
			_aligned_free (pU8RGBData);
		}
	}
}

int main(int argc, char** argv)
{
	OCLResources	myOCL;
	unsigned int	test_iterations;
	char			pDeviceStr[BUFSIZ];
	char			pVendorStr[BUFSIZ];
	const char*		pSourceFileStr	= "SobelKernels.cl";
	bool			bShowCL = false;

	// Image data:
	const char*		pInputImg1Str	= "Henri2048x2048.ppm";
	uint			height, width;
	uint			heightReSz, widthReSz;
	uint			szRGBImgPixelBytes;	
	uint			szGrayImgBytes, szGrayImgBytesReSz;
	U8*				pRGBImgData;
	U8*				pGrayImgData;
	U8*				pGrayImgDataReSz;

	parseArgs (&myOCL, argc, argv, &test_iterations, pDeviceStr, pVendorStr, &widthReSz, &heightReSz, &bShowCL);

	printf("\n\n\n--------------------------------------------------------------------\n");
	
	// Initialize OpenCL:
	InitializeOpenCL (pDeviceStr, pVendorStr, &myOCL.deviceID, &myOCL.contextHdl, &myOCL.cmdQHdl);
	CompileOpenCLProgram (myOCL.deviceID, myOCL.contextHdl, pSourceFileStr, &myOCL.programHdl);
	InstantiateOpenCLKernels (&myOCL);
	if (bShowCL)
		QueryPrintOpenCLDeviceInfo (myOCL.deviceID, myOCL.contextHdl);

	// Load rgb ppm image, convert to U8 grayscale:
	loadppm_toU8RGB (pInputImg1Str, &width, &height, &pRGBImgData, &szRGBImgPixelBytes);
	szGrayImgBytes	= width * height;
	pGrayImgData	= (U8*)_aligned_malloc (szGrayImgBytes, READ_ALIGNMENT);
	convertU8RGB_to_U8Gray (width, height, pRGBImgData, pGrayImgData);

	// Resize:
	szGrayImgBytesReSz = (widthReSz+32) * (heightReSz+2); // pad the image top, bottom and on the sides
	pGrayImgDataReSz = (U8*)_aligned_malloc (szGrayImgBytesReSz, READ_ALIGNMENT);
	resizeU8Gray (width, height, widthReSz+32, heightReSz+2, pGrayImgData, pGrayImgDataReSz);

	AllocateOpenCLMemResources (&myOCL, widthReSz+32, heightReSz+2, szGrayImgBytesReSz, pGrayImgDataReSz);

	PerformSobelTests (&myOCL, test_iterations, widthReSz, heightReSz);

	printf("-------done--------------------------------------------------------\n");
	getchar();
	Sleep(2000);

	Cleanup (&myOCL, 0, true, "Success");
	_aligned_free (pGrayImgData);
	_aligned_free (pGrayImgDataReSz);

	return 0;
}
