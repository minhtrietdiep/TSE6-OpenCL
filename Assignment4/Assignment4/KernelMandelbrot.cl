#pragma OPENCL EXTENSION cl_khr_fp64 : enable

typedef struct {
	unsigned char blue, green, red;
} mandelbrot_color; 

__kernel void mandelbrot_frame( __global double *offset_x,
								__global double *offset_y,
								__global float *stepsize,
								__global unsigned int *max_iterations,
								__write_only image2d_t framebuffer,
								__global mandelbrot_color *colortable,
								__global unsigned int *window_width,
								__global unsigned int *window_height) {
	int windowPosX = get_global_id(0);
	int windowPosY = get_global_id(1);

	double center_X = (double)(-((*stepsize)*(*window_width)  / 2.0));
	double center_Y = (double)(((*stepsize)*(*window_height) / 2.0));
	const double stepPosX = (double)(center_X - *offset_x + (windowPosX * (*stepsize)));
	const double stepPosY = (double)(center_Y + *offset_y - (windowPosY * (*stepsize)));

	// Variables for the calculation
	double x = 0.0;
	double y = 0.0;
	double xSqr = 0.0;
	double ySqr = 0.0;

	unsigned int iterations = 0;

	// Perform up to the maximum number of iterations to solve
	// the current pixel's position in the image
	while ((xSqr + ySqr < 4.0) && (iterations < *max_iterations))
	{
		// Perform the current iteration
		xSqr = x*x;
		ySqr = y*y;

		y = 2 * x*y + stepPosY;
		x = xSqr - ySqr + stepPosX;

		// Increment iteration count
		iterations++;
	}

	// Output black if we never finished, and a color from the look up table otherwise
	float4 black = { 0.0f, 0.0f, 0.0f, 1.0f };
	float4 color = {	colortable[iterations].red / 255.0f,
						colortable[iterations].green / 255.0f,
						colortable[iterations].blue / 255.0f,
						1.0f };
	int2 windowPos = { windowPosX, windowPosY };
	float4 finalcolor =	(iterations == (*max_iterations)) ? black : color;
	write_imagef(framebuffer, windowPos, finalcolor);
}

