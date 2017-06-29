#ifndef OPENCLUTILS_DOT_H
#define OPENCLUTILS_DOT_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <CL/cl.h>


// Utils for timing:
static struct _timeb start, end;
static void reset_and_start_timer()
{
	_ftime_s(&start);
}
static double get_elapsed_seconds()
{
	_ftime_s(&end);
	return (1000 * (end.time - start.time) + (end.millitm - start.millitm)) / 1000.0;
}


// Util for error checking:
//#undef __OCL_NO_ERROR_CHECKING
#define __OCL_NO_ERROR_CHECKING

#ifdef __OCL_NO_ERROR_CHECKING
#define CheckCLError(__errNum__, __failMsg__, __passMsg__)	\
	assert (CL_SUCCESS == __errNum__);
#else
#define CheckCLError(__errNum__, __failMsg__, __passMsg__)	\
if (CL_SUCCESS != __errNum__)								\
{															\
		char __msgBuf__[256];								\
		sprintf (__msgBuf__, "CL Error num %d: %s at line %d, file %s in function %s().\n", __errNum__, __failMsg__, __LINE__, __FILE__, __FUNCTION__);	\
		printf (__msgBuf__);								\
		getchar();											\
		printf("Failed on OpenCLError\n");					\
		assert (CL_SUCCESS != __errNum__);					\
		exit(0);											\
} else if (__passMsg__)										\
{															\
	printf("CL Success: %s\n", __passMsg__);				\
}				
#endif


// Util for OpenCL build log:
void BuildFailLog( cl_program program,
                  cl_device_id device_id )
{
    size_t paramValueSizeRet = 0;
    clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &paramValueSizeRet);

    char* buildLogMsgBuf = (char *)malloc(sizeof(char) * paramValueSizeRet + 1);
	if( buildLogMsgBuf )
	{
		clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, paramValueSizeRet, buildLogMsgBuf, &paramValueSizeRet);
		buildLogMsgBuf[paramValueSizeRet] = '\0';	//mark end of message string

		printf("\nOpenCL C Program Build Log:\n");
		puts(buildLogMsgBuf);
		fflush(stdout);

		free(buildLogMsgBuf);
	}
}


void InitializeOpenCL(char* pDeviceStr, char* pVendorStr, cl_device_id* pDeviceID, cl_context* pContextHdl, cl_command_queue* pCmdQHdl)
{	
	// OpenCL System Initialization

	*pDeviceID		= NULL;
	*pContextHdl	= NULL;

	// First query for all of the available platforms (e.g. NV, Intel, AMD, etc)
	// Choose the one that matches the command line request
	cl_uint			numPlatforms= 0;
	cl_int			ciErrNum	= clGetPlatformIDs(0, NULL, &numPlatforms);

	CheckCLError (ciErrNum, "No platforms Found.", "OpenCL platforms found.");

	char				pPlatformVendor[256];
	cl_platform_id		platformID	= NULL;
	cl_platform_id		pPlatformIDs[10];
	cl_device_id		deviceID;
	cl_context			contextHdl;
	cl_command_queue	cmdQueueHdl;

	if (0 < numPlatforms) 
	{
		
 		ciErrNum = clGetPlatformIDs(numPlatforms, pPlatformIDs, NULL);
		CheckCLError (ciErrNum, "Could not get platform IDs.", "Got platform IDs.");
		unsigned i;

		for (i = 0; i < numPlatforms; ++i) 
		{
			ciErrNum = clGetPlatformInfo(pPlatformIDs[i],CL_PLATFORM_VENDOR,sizeof(pPlatformVendor),pPlatformVendor,NULL);
			CheckCLError (ciErrNum, "Could not get platform info.", "Got platform info.");
			
			platformID = pPlatformIDs[i];

			if ((!strcmp(pPlatformVendor, "Intel Corporation")|| !strcmp(pPlatformVendor, "Intel(R) Corporation")) && !strcmp(pVendorStr, "intel") && !strcmp(pDeviceStr, "gpu") )
			{
				if(CL_SUCCESS == clGetDeviceIDs(platformID, CL_DEVICE_TYPE_GPU, 1, &deviceID, NULL))
					break;
			}
			if ((!strcmp(pPlatformVendor, "Intel Corporation")|| !strcmp(pPlatformVendor, "Intel(R) Corporation")) && !strcmp(pVendorStr, "intel") && !strcmp(pDeviceStr, "cpu"))
			{
				if(CL_SUCCESS == clGetDeviceIDs(platformID, CL_DEVICE_TYPE_CPU, 1, &deviceID, NULL))
					break;
			}
			if (!strcmp(pPlatformVendor, "Advanced Micro Devices, Inc.") && !strcmp(pVendorStr, "amd") && !strcmp(pDeviceStr, "gpu") )
			{
				if(CL_SUCCESS == clGetDeviceIDs(platformID, CL_DEVICE_TYPE_GPU, 1, &deviceID, NULL))
					break;
			}
			if (!strcmp(pPlatformVendor, "NVIDIA Corporation") && !strcmp(pVendorStr, "nvidia") && !strcmp(pDeviceStr, "gpu") )
			{
				if(CL_SUCCESS == clGetDeviceIDs(platformID, CL_DEVICE_TYPE_GPU, 1, &deviceID, NULL))
					break;
			}
		}

		if(i == numPlatforms) 
		{
			printf("Error didn't find platform that matches requested platform: %s\n", pVendorStr);
			//Cleanup (-1, true, "Error didn't find platform that matches requested platform.");
		}		
	}
	else 
	{
		printf("numPlatforms is %d\n", numPlatforms);
		exit(-1);
	}



	
    // Create the OpenCL context
    contextHdl = clCreateContext(0, 1, &deviceID, NULL, NULL, &ciErrNum);
	CheckCLError (ciErrNum, "Could not create CL context.", "Created CL context.");

    // Create a command-queue
    cmdQueueHdl = clCreateCommandQueue(contextHdl, deviceID, 0, &ciErrNum);
	CheckCLError (ciErrNum, "Could not create CL command queue.", "Created CL command queue.");
	
	// Output parameters:
	*pDeviceID		= deviceID;
	*pContextHdl	= contextHdl;
	*pCmdQHdl		= cmdQueueHdl;

}

void CreateOCLProgramFromSourceFile(char const *pSrcFilePath, cl_context hClContext, cl_program *pCLProgram )
{
	FILE* fp;
	errno_t err = fopen_s(&fp, pSrcFilePath, "rb");
	if (!fp)
	{
		printf("Failed to find OpenCL source program: %s\n", pSrcFilePath);
		//Cleanup (-1, true, "Failed to open CL Source file.\n");
		exit(0);
	}

	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	char* buf = new char[size + 1];
	buf[size] = '\0';
	fread(buf, size, 1, fp);
	fclose(fp);

	size_t szKernelLength = size;
	cl_int ciErrNum;
	*pCLProgram = clCreateProgramWithSource(hClContext, 1, (const char **)&buf, &szKernelLength, &ciErrNum);
	CheckCLError(ciErrNum, "Failed to create program.", "Created program.");

	delete[] buf;
}


void CompileOpenCLProgram(cl_device_id oclDeviceID, cl_context oclContextHdl, const char* pSourceFileStr, cl_program* pOclProgramHdl)
{
	//printf (".cl source file: %s\n", pSourceFileStr);
	cl_int		ciErrNum;
	cl_program	oclProgramHdl;

	*pOclProgramHdl = NULL;

	CreateOCLProgramFromSourceFile(pSourceFileStr, oclContextHdl, &oclProgramHdl);

	char flags[1024];
	sprintf_s(flags, "-cl-mad-enable -cl-fast-relaxed-math");//this caused artifacts
	sprintf_s(flags, "-cl-mad-enable ");

    ciErrNum = clBuildProgram(oclProgramHdl, 0, NULL, flags, NULL, NULL);
	if (ciErrNum != CL_SUCCESS)
	{
		printf("ERROR: Failed to build program... ciErrNum = %d\n", ciErrNum);
		BuildFailLog(oclProgramHdl, oclDeviceID);
	}
	CheckCLError (ciErrNum, "Program building failed.", "Built Program");
	if (ciErrNum != CL_SUCCESS)
	{
		getchar();
		exit(0);
	}

	// Output parameters:
	*pOclProgramHdl = oclProgramHdl;
}

// Util for OpenCL info queries:
void QueryPrintOpenCLDeviceInfo(cl_device_id deviceID, cl_context contextHdl)
{
	cl_uint		uMaxComputeUnits			= 0;
	cl_uint		uMaxWorkItemDim				= 0;
	size_t		uMaxWorkItemSizes[3];
	cl_uint		uMaxNumSamplers				= 0;
	cl_uint		uMinBaseAddrAlignSizeBits	= 0;		// CL_DEVICE_MEM_BASE_ADDR_ALIGN
	cl_uint		uMinBaseAddrAlignSizeBytes	= 0;		// CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE
	//cl_uint	uLocalMemType;				// CL_LOCAL, CL_GLOBAL.
	//cl_ubool	uHostUnifiedMemory		= false;
	size_t		uNumBytes					= 0;
	char		pDeviceVenderString[512];		// CL_DEVICE_VENDOR
	char		pDeviceNameString[512];			// CL_DEVICE_NAME
	char		pDriverVersionString[512];		// CL_DRIVER_VERSION
	char		pDeviceProfileString[512];		// CL_DEVICE_PROFILE
	char		pDeviceVersionString[512];		// CL_DEVICE_VERSION
	char		pOpenCLCVersionString[512];		// CL_DEVICE_OPENCL_C_VERSION
	cl_int		ciErrNum;

	// Device Property Queries:
	ciErrNum = clGetDeviceInfo(deviceID, CL_DEVICE_VENDOR, sizeof(char[512]), &pDeviceVenderString, &uNumBytes);
	CheckCLError (ciErrNum, "clGetDeviceInfo() query failed.", "clGetDeviceinfo() query success");
	ciErrNum = clGetDeviceInfo(deviceID, CL_DEVICE_NAME, sizeof(char[512]), &pDeviceNameString, &uNumBytes);
	CheckCLError (ciErrNum, "clGetDeviceInfo() query failed.", "clGetDeviceinfo() query success");

	printf("Using platform: %s and device: %s.\n", pDeviceVenderString, pDeviceNameString);
	printf ("OpenCL Device info:\n");
	printf ("CL_DEVICE_VENDOR			:%s\n", pDeviceVenderString);
	printf ("CL_DEVICE_NAME				:%s\n", pDeviceNameString);

	ciErrNum = clGetDeviceInfo(deviceID, CL_DRIVER_VERSION, sizeof(char[512]), &pDriverVersionString, &uNumBytes);
	CheckCLError (ciErrNum, "clGetDeviceInfo() query failed.", "clGetDeviceinfo() query success");
	printf ("CL_DRIVER_VERSION			:%s\n", pDriverVersionString);

	ciErrNum = clGetDeviceInfo(deviceID, CL_DEVICE_PROFILE, sizeof(char[512]), &pDeviceProfileString, &uNumBytes);
	CheckCLError (ciErrNum, "clGetDeviceInfo() query failed.", "clGetDeviceinfo() query success");
	printf ("CL_DEVICE_PROFILE			:%s\n", pDeviceProfileString);

	ciErrNum = clGetDeviceInfo(deviceID, CL_DEVICE_VERSION, sizeof(char[512]), &pDeviceVersionString, &uNumBytes);
	CheckCLError (ciErrNum, "clGetDeviceInfo() query failed.", "clGetDeviceinfo() query success");
	printf ("CL_DEVICE_VERSION			:%s\n", pDeviceVersionString);
	
	ciErrNum = clGetDeviceInfo(deviceID, CL_DEVICE_OPENCL_C_VERSION, sizeof(char[512]), &pOpenCLCVersionString, &uNumBytes);
	CheckCLError (ciErrNum, "clGetDeviceInfo() query failed.", "clGetDeviceinfo() query success");
	printf ("CL_DEVICE_OPENCL_C_VERSION		:%s\n", pOpenCLCVersionString);


	ciErrNum = clGetDeviceInfo(deviceID, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &uMaxComputeUnits, &uNumBytes);
	CheckCLError (ciErrNum, "clGetDeviceInfo() query failed.", "clGetDeviceinfo() query success");
	printf ("CL_DEVICE_MAX_COMPUTE_UNITS		:%8d\n", uMaxComputeUnits);

	ciErrNum = clGetDeviceInfo(deviceID, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(cl_uint), &uMaxWorkItemDim, &uNumBytes);
	CheckCLError (ciErrNum, "clGetDeviceInfo() query failed.", "clGetDeviceinfo() query success");
	printf ("CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS	:%8d\n", uMaxWorkItemDim);

	ciErrNum = clGetDeviceInfo(deviceID, CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(size_t[3]), &uMaxWorkItemSizes, &uNumBytes);
	CheckCLError (ciErrNum, "clGetDeviceInfo() query failed.", "clGetDeviceinfo() query success");
	printf ("CL_DEVICE_MAX_WORK_ITEM_SIZES		:    (%5d, %5d, %5d)%\n", 
					uMaxWorkItemSizes[0],uMaxWorkItemSizes[1], uMaxWorkItemSizes[2]);
	
	size_t	uMaxWorkGroupSize;
	ciErrNum = clGetDeviceInfo(deviceID, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), &uMaxWorkGroupSize, &uNumBytes);
	CheckCLError (ciErrNum, "clGetDeviceInfo() query failed.", "clGetDeviceinfo() query success");
	printf ("CL_DEVICE_MAX_WORK_GROUP_SIZE		:%8d\n", uMaxWorkGroupSize);

	ciErrNum = clGetDeviceInfo(deviceID, CL_DEVICE_MEM_BASE_ADDR_ALIGN, sizeof(cl_uint), &uMinBaseAddrAlignSizeBits, &uNumBytes);
	CheckCLError (ciErrNum, "clGetDeviceInfo() query failed.", "clGetDeviceinfo() query success");
	printf ("CL_DEVICE_MEM_BASE_ADDR_ALIGN		:%8d\n", uMinBaseAddrAlignSizeBits);

	ciErrNum = clGetDeviceInfo(deviceID, CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE, sizeof(cl_uint), &uMinBaseAddrAlignSizeBytes, &uNumBytes);
	CheckCLError (ciErrNum, "clGetDeviceInfo() query failed.", "clGetDeviceinfo() query success");
	printf ("CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE	:%8d\n", uMinBaseAddrAlignSizeBytes);

	cl_uint	uMaxDeviceFrequency;
	ciErrNum = clGetDeviceInfo(deviceID, CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(cl_uint), &uMaxDeviceFrequency, &uNumBytes);
	CheckCLError (ciErrNum, "clGetDeviceInfo() query failed.", "clGetDeviceinfo() query success");
	printf ("CL_DEVICE_MAX_CLOCK_FREQUENCY		:%8d\n", uMaxDeviceFrequency);

	cl_uint	uMaxImage2DWidth;
	ciErrNum = clGetDeviceInfo(deviceID, CL_DEVICE_IMAGE2D_MAX_WIDTH, sizeof(cl_uint), &uMaxImage2DWidth, &uNumBytes);
	CheckCLError (ciErrNum, "clGetDeviceInfo() query failed.", "clGetDeviceinfo() query success");
	printf ("CL_DEVICE_IMAGE2D_MAX_WIDTH		:%8d\n", uMaxImage2DWidth);

	cl_ulong	uLocalMemSize;
	float		fLocalMemSize;
	ciErrNum = clGetDeviceInfo(deviceID, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(cl_ulong), &uLocalMemSize, &uNumBytes);
	CheckCLError (ciErrNum, "clGetDeviceInfo() query failed.", "clGetDeviceinfo() query success");
	fLocalMemSize = (float) uLocalMemSize;
	printf ("CL_DEVICE_LOCAL_MEM_SIZE		:%12.1f\n", fLocalMemSize);

	cl_long	uMaxMemAllocSize;
	float fMaxMemAllocSize;  
	ciErrNum = clGetDeviceInfo(deviceID, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(cl_long), &uMaxMemAllocSize, &uNumBytes);
	CheckCLError (ciErrNum, "clGetDeviceInfo() query failed.", "clGetDeviceinfo() query success");
	fMaxMemAllocSize = (float) uMaxMemAllocSize;
	printf ("CL_DEVICE_MAX_MEM_ALLOC_SIZE		:%12.1f\n", fMaxMemAllocSize);
	
#define MAX_NUM_FORMATS 500
	cl_uint numFormats;
	cl_image_format myFormats[MAX_NUM_FORMATS];

	ciErrNum = clGetSupportedImageFormats(contextHdl, CL_MEM_READ_ONLY, CL_MEM_OBJECT_IMAGE2D, 255, myFormats, &numFormats);
	CheckCLError (ciErrNum, "clGetSupportedImageFormats() query failed.", "clGetSupportedImageFormats() query success");
	//printf ("Supported Image Formats (CL_MEM_READ_ONLY | CL_MEM_OBJECT_IMAGE2D):\n");
	//printImageFormats (numFormats, myFormats);
	//printf ("\n");


	// Kernel Queries:
	/**
	size_t	uKernelWGSize;
	ciErrNum = clGetKernelWorkGroupInfo(ckSobelKernelU8, deviceID, CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &uKernelWGSize, &uNumBytes);
	CheckCLError (ciErrNum, "clGetKernelWorkGroupInfo() query failed.", "clGetKernelWorkGroupInfo() query success");
	printf ("CL_KERNEL_WORK_GROUP_SIZE		:%8d\n", uKernelWGSize);

	size_t	uKernelWGCompileSize[3];
	ciErrNum = clGetKernelWorkGroupInfo(ckSobelKernelU8, deviceID, CL_KERNEL_COMPILE_WORK_GROUP_SIZE, sizeof(size_t[3]), uKernelWGCompileSize, &uNumBytes);
	CheckCLError (ciErrNum, "clGetKernelWorkGroupInfo() query failed.", "clGetKernelWorkGroupInfo() query success");
	printf ("CL_KERNEL_COMPILE_WORK_GROUP_SIZE	: (%5d, %5d, %5d)\n", uKernelWGCompileSize[0], uKernelWGCompileSize[1], uKernelWGCompileSize[2]);

	size_t	uKernelWGPreferredSize;
	ciErrNum = clGetKernelWorkGroupInfo(ckSobelKernelU8, deviceID, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(size_t), &uKernelWGPreferredSize, &uNumBytes);
	CheckCLError (ciErrNum, "clGetKernelWorkGroupInfo() query failed.", "clGetKernelWorkGroupInfo() query success");
	printf ("CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE	:%8d\n", uKernelWGPreferredSize);
	**/
}

#endif