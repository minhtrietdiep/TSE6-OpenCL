kernel void kernel3(global int *gdata, local int *sdata) {
	unsigned int tid = get_local_id(0);
	unsigned int gid = get_global_id(0);
	unsigned int lsz = get_local_size(0);
	unsigned long long int i = get_group_id(0) * lsz * 2 + tid;

	// copy int’s from Global to Local memory
	sdata[tid] = gdata[i] + gdata[i + lsz];
	barrier(CLK_GLOBAL_MEM_FENCE);

	// do reduction in Local memory
	for (unsigned int long long s = 1; s < get_local_size(0); s *= 2) {
		int long long index = 2 * s*tid;
		if (index<get_local_size(0)) {
			sdata[index] += sdata[index + s];
		}

		barrier(CLK_LOCAL_MEM_FENCE);
	}

	// write result for this block to global mem
	if (tid == 0)
		gdata[get_group_id(0)] = sdata[0];
}
