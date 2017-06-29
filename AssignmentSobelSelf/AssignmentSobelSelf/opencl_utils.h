#include "CL/opencl.h"

void printError(cl_int error);
void _checkError(int line,
				 const char *file,
				 cl_int error,
				 const char *msg,
				 ...); 
#define checkError(status, ...) _checkError(__LINE__, __FILE__, status, __VA_ARGS__)

cl_program build_program(cl_context context, cl_device_id device, const char* filename);
