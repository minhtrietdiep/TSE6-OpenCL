typedef struct {
	unsigned char blue, green, red;
} mandelbrot_color;

void mandelbrot_frame (
	const float x0,
	const float y0,
	const float stepsize,
	const unsigned int max_iterations,
	//unsigned char *framebuffer,
	mandelbrot_color *framebuffer,
	//const unsigned char *colortable,
	const mandelbrot_color *colortable,
	const unsigned int window_width,
	const unsigned int window_height);