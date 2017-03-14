#include "mandelbrot_frame.h"
void create_colortable(void * table, int size ) 
{
	mandelbrot_color *colortable = (mandelbrot_color *)table;
	// Initialize color table values
	for(unsigned int i = 0; i < size; i++)
	{
		if (i < 256) {
			mandelbrot_color color_entry = {0,0,i%128+128};
			colortable[i] = color_entry;
		}

		else if (i < 512) {
			mandelbrot_color color_entry = {0,i%128+128,0};
			colortable[i] = color_entry;
		}

		else if (i < 768) {
			mandelbrot_color color_entry = {i%128+128,0,0};
			colortable[i] = color_entry;
		}

		else if (i < 1024) {
			mandelbrot_color color_entry = {255,0,i%128+128};
			colortable[i] = color_entry;
		}

		else if (i<1280){
			mandelbrot_color color_entry = {255,i%128+128,0};
			colortable[i] = color_entry;
		}

		else if (i<1536){
			mandelbrot_color color_entry = {0,255,i%128+128};
			colortable[i] = color_entry;
		}

		else if (i<1792){
			mandelbrot_color color_entry = {i%128+128,255,0};
			colortable[i] = color_entry;
		}

		else if (i<2048){
			mandelbrot_color color_entry = {0,i%128+128,255};
			colortable[i] = color_entry;
		}

		else if (i<2304){
			mandelbrot_color color_entry = {i%128+128,0,255};
			colortable[i] = color_entry;
		}

		else if (i<2560){
			mandelbrot_color color_entry = {255,255,i%128+128};
			colortable[i] = color_entry;
		}

		else if (i<2816){
			mandelbrot_color color_entry = {255,i%128+128,255};
			colortable[i] = color_entry;
		}

		else if (i<3072){
			mandelbrot_color color_entry = {i%128+128,255,255};
			colortable[i] = color_entry;
		}		
	}
}