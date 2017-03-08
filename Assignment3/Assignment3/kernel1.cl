__kernel void kernel1(__global int *gdata, __local int *sdata) {
	unsigned int tid = get_local_id(0);
	unsigned int gid = get_global_id(0);

	// copy int’s from Global to Local memory
	sdata[tid] = gdata[gid];
	barrier(CLK_GLOBAL_MEM_FENCE);

	// do reduction in Local memory
	for (unsigned int s = 1; s < get_local_size(0); s *= 2) {
		if (tid % (2 * s) == 0) { // if ThreadID is a multiple of 2*s
			sdata[tid] += sdata[tid + s];
		}
		barrier(CLK_LOCAL_MEM_FENCE);
	}

	// write result for this block to global mem
	if (tid == 0)
		gdata[get_group_id(0)] = sdata[0];
}
