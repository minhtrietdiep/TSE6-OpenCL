// Sobel Kernels:

////////////////////////////////////////////////////////////////////////////////////////

__kernel void Sobel_v1_uchar (__global uchar *pSrcImage, __global uchar *pDstImage)
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

__kernel void Sobel_v2_uchar16 (__global uchar16* pSrcImage, __global uchar16* pDstImage)
{
	uint dstYStride = get_global_size(0);
	uint dstIndex   = get_global_id(1) * dstYStride + get_global_id(0);
	uint srcYStride = dstYStride + 2;
	uint srcIndex   = get_global_id(1) * srcYStride + get_global_id(0) + 1;

	uint a;	uint16 b; uint c;
	uint d;	uint16 e; uint f;
	uint g;	uint16 h; uint i;

	// Read data in	
	a = ((__global uchar*)(pSrcImage+srcIndex))[-1]; b = convert_uint16(pSrcImage[srcIndex]); c = ((__global uchar*)(pSrcImage+srcIndex))[16];
	srcIndex += srcYStride;
	d = ((__global uchar*)(pSrcImage+srcIndex))[-1]; e = convert_uint16(pSrcImage[srcIndex]); f = ((__global uchar*)(pSrcImage+srcIndex))[16];
	srcIndex += srcYStride;
	g = ((__global uchar*)(pSrcImage+srcIndex))[-1]; h = convert_uint16(pSrcImage[srcIndex]); i = ((__global uchar*)(pSrcImage+srcIndex))[16];

	uint16 xVal, yVal; 

	xVal = (uint16)(a, b.s0123, b.s456789ab, b.scde) -      (uint16)(b.s123, b.s4567, b.s89abcdef, c) + 
	     2*(uint16)(d, e.s0123, e.s456789ab, e.scde) -    2*(uint16)(e.s123, e.s4567, e.s89abcdef, f) + 
		   (uint16)(g, h.s0123, h.s456789ab, h.scde) -      (uint16)(h.s123, h.s4567, h.s89abcdef, i);

	yVal = (uint16)(a, b.s0123, b.s456789ab, b.scde) + 2*b + (uint16)(b.s123, b.s4567, b.s89abcdef, c) - 
	       (uint16)(g, h.s0123, h.s456789ab, h.scde) - 2*h - (uint16)(h.s123, h.s4567, h.s89abcdef, i);

	// Write data out		
	pDstImage[dstIndex] = convert_uchar16(min((float16)255.0f, sqrt(convert_float16(xVal*xVal + yVal*yVal))));					
}

__kernel void Sobel_v3_uchar16_to_float16 (__global uchar16* pSrcImage, __global uchar16* pDstImage)
{
	uint dstYStride = get_global_size(0);
	uint dstIndex   = get_global_id(1) * dstYStride + get_global_id(0);
	uint srcYStride = dstYStride + 2;
	uint srcIndex   = get_global_id(1) * srcYStride + get_global_id(0) + 1;

	float a; float16 b; float c;
	float d; float16 e; float f;
	float g; float16 h; float i;

	// Read data in	
	a = convert_float(((__global uchar*)(pSrcImage+srcIndex))[-1]);		
	b = convert_float16(pSrcImage[srcIndex]);	 
	c = convert_float(((__global uchar*)(pSrcImage+srcIndex))[16]);	
	srcIndex += srcYStride;
	d = convert_float(((__global uchar*)(pSrcImage+srcIndex))[-1]);			    
	e = convert_float16(pSrcImage[srcIndex]);	         
	f = convert_float(((__global uchar*)(pSrcImage+srcIndex))[16]);
	srcIndex += srcYStride;
	g = convert_float(((__global uchar*)(pSrcImage+srcIndex))[-1]);		
	h = convert_float16(pSrcImage[srcIndex]);	 
	i = convert_float(((__global uchar*)(pSrcImage+srcIndex))[16]);

	float16 xVal, yVal; 

	xVal = (float16)(a, b.s0123, b.s456789ab, b.scde) -      (float16)(b.s123, b.s4567, b.s89abcdef, c) + 
	  2.0f*(float16)(d, e.s0123, e.s456789ab, e.scde) - 2.0f*(float16)(e.s123, e.s4567, e.s89abcdef, f) + 
		   (float16)(g, h.s0123, h.s456789ab, h.scde) -      (float16)(h.s123, h.s4567, h.s89abcdef, i);

	yVal = (float16)(a, b.s0123, b.s456789ab, b.scde) + 2.0f*b + (float16)(b.s123, b.s4567, b.s89abcdef, c) - 
	       (float16)(g, h.s0123, h.s456789ab, h.scde) - 2.0f*h - (float16)(h.s123, h.s4567, h.s89abcdef, i);

	// Write data out		
	pDstImage[dstIndex] = convert_uchar16(min((float16)255.0f, sqrt(xVal*xVal + yVal*yVal)));					
}

__kernel void Sobel_v4_uchar16_to_float16_16(__global uchar16* pSrcImage, __global uchar16* pDstImage)
{
	uint dstYStride = get_global_size(0);
	uint dstIndex = 16 * get_global_id(1) * dstYStride + get_global_id(0);
	uint srcYStride = dstYStride + 2;
	uint srcIndex = 16 * get_global_id(1) * srcYStride + get_global_id(0) + 1;

	float a; float16 b; float c;
	float d; float16 e; float f;
	float g; float16 h; float i;

	// Read data in	
	a = convert_float(((__global uchar*)(pSrcImage + srcIndex))[-1]);
	b = convert_float16(pSrcImage[srcIndex]);
	c = convert_float(((__global uchar*)(pSrcImage + srcIndex))[16]);
	srcIndex += srcYStride;
	d = convert_float(((__global uchar*)(pSrcImage + srcIndex))[-1]);
	e = convert_float16(pSrcImage[srcIndex]);
	f = convert_float(((__global uchar*)(pSrcImage + srcIndex))[16]);

	for (uint k = 0; k < 16; k++) {
		srcIndex += srcYStride;
		g = convert_float(((__global uchar*)(pSrcImage + srcIndex))[-1]);
		h = convert_float16(pSrcImage[srcIndex]);
		i = convert_float(((__global uchar*)(pSrcImage + srcIndex))[16]);

		float16 xVal, yVal;

		xVal = (float16)(a, b.s0123, b.s456789ab, b.scde) - (float16)(b.s123, b.s4567, b.s89abcdef, c) +
			2.0f*(float16)(d, e.s0123, e.s456789ab, e.scde) - 2.0f*(float16)(e.s123, e.s4567, e.s89abcdef, f) +
			(float16)(g, h.s0123, h.s456789ab, h.scde) - (float16)(h.s123, h.s4567, h.s89abcdef, i);

		yVal = (float16)(a, b.s0123, b.s456789ab, b.scde) + 2.0f*b + (float16)(b.s123, b.s4567, b.s89abcdef, c) -
			(float16)(g, h.s0123, h.s456789ab, h.scde) - 2.0f*h - (float16)(h.s123, h.s4567, h.s89abcdef, i);

		// Write data out		
		pDstImage[dstIndex] = convert_uchar16(min((float16)255.0f, sqrt(xVal*xVal + yVal*yVal)));

		a = d; b = e; c = f;
		d = g; e = h; f = i;
		dstIndex += dstYStride;
	}
}
