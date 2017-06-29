__kernel void Sobel (__global uchar *pSrcImage, __global uchar *pDstImage)
{
	uint dstYStride = get_global_size(0);
	uint dstIndex   = get_global_id(1) * dstYStride + get_global_id(0);
	uint srcYStride = dstYStride + 32;
	uint srcIndex   = get_global_id(1) * srcYStride + get_global_id(0) + 16;

	uint a,		b,		c;
	uint d,	/*center*/  f;
	uint g,		h,		i;

	// Read data in	
	a = pSrcImage[srcIndex-1];		b = pSrcImage[srcIndex];	c = pSrcImage[srcIndex+1];
	srcIndex += srcYStride;
	d = pSrcImage[srcIndex-1];				/*center*/	        f = pSrcImage[srcIndex+1];	
	srcIndex += srcYStride;
	g = pSrcImage[srcIndex-1];		h = pSrcImage[srcIndex];	i = pSrcImage[srcIndex+1];	

	uint xVal =  	a* 1 +				c*-1	+
					d* 2 +	/*center*/	f*-2	+
					g* 1 +		    	i*-1;
			
	uint yVal =	    a* 1 + b* 2 + c* 1 +
						/*center*/	 
					g*-1 + h*-2 + i*-1;	

	// Write data out		
	pDstImage[dstIndex] =  min((uint)255, (uint)sqrt((float)(xVal*xVal + yVal*yVal)));	
}
