/*
  $Id$
*/
/*
 * This version is derived from the original implementation of FreeSec
 * (release 1.1) by David Burren.  I've reviewed the changes made in
 * OpenBSD (as of 2.7) and modified the original code in a similar way
 * where applicable.  I've also made it reentrant and made a number of
 * other changes.
 * - Solar Designer <solar at openwall.com>
 */

/*
 * FreeSec: libcrypt for NetBSD
 *
 * Copyright (c) 1994 David Burren
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of other contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Owl: Owl/packages/glibc/crypt_freesec.c,v 1.4 2005/11/16 13:08:32 solar Exp $
 *	$Id$
 *
 * This is an original implementation of the DES and the crypt(3) interfaces
 * by David Burren <davidb at werj.com.au>.
 *
 * An excellent reference on the underlying algorithm (and related
 * algorithms) is:
 *
 *	B. Schneier, Applied Cryptography: protocols, algorithms,
 *	and source code in C, John Wiley & Sons, 1994.
 *
 * Note that in that book's description of DES the lookups for the initial,
 * pbox, and final permutations are inverted (this has been brought to the
 * attention of the author).  A list of errata for this book has been
 * posted to the sci.crypt newsgroup by the author and is available for FTP.
 *
 * ARCHITECTURE ASSUMPTIONS:
 *	This code used to have some nasty ones, but these have been removed
 *	by now.  The code requires a 32-bit integer type, though.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <CL/cl.h>
#include <assert.h>
//#include <sys/time.h>

#include "triperino_kernel.h"

#include <stdint.h>
#include <winsock.h>
#include "time.h"

#define CBUFSIZ 512
static const uint32_t trips_per_item = 2;
typedef enum {IntelCPU, NvidiaGPU} hardware_t; 

#define UNSAFE_SPACE 256
#define OPENCL1_0

hardware_t target_platform = NvidiaGPU;
const size_t global_worksize[1] = {131072};
size_t local_worksize[1];
cl_int status;
cl_uint num_platforms;
cl_platform_id *platforms = NULL;
int platform_index = -1;

cl_uint num_devices;
cl_device_id *devices;
cl_context context;
cl_command_queue cmd_queue;
cl_program triperino_program;
cl_kernel triperino_kernel;

static u_char	IP[64] = {
	58, 50, 42, 34, 26, 18, 10,  2, 60, 52, 44, 36, 28, 20, 12,  4,
	62, 54, 46, 38, 30, 22, 14,  6, 64, 56, 48, 40, 32, 24, 16,  8,
	57, 49, 41, 33, 25, 17,  9,  1, 59, 51, 43, 35, 27, 19, 11,  3,
	61, 53, 45, 37, 29, 21, 13,  5, 63, 55, 47, 39, 31, 23, 15,  7
};

static u_char	key_perm[56] = {
	57, 49, 41, 33, 25, 17,  9,  1, 58, 50, 42, 34, 26, 18,
	10,  2, 59, 51, 43, 35, 27, 19, 11,  3, 60, 52, 44, 36,
	63, 55, 47, 39, 31, 23, 15,  7, 62, 54, 46, 38, 30, 22,
	14,  6, 61, 53, 45, 37, 29, 21, 13,  5, 28, 20, 12,  4
};

static u_char	comp_perm[48] = {
	14, 17, 11, 24,  1,  5,  3, 28, 15,  6, 21, 10,
	23, 19, 12,  4, 26,  8, 16,  7, 27, 20, 13,  2,
	41, 52, 31, 37, 47, 55, 30, 40, 51, 45, 33, 48,
	44, 49, 39, 56, 34, 53, 46, 42, 50, 36, 29, 32
};

/*
 *	No E box is used, as it's replaced by some ANDs, shifts, and ORs.
 */

static u_char	sbox[8][64] = {
	{
		14,  4, 13,  1,  2, 15, 11,  8,  3, 10,  6, 12,  5,  9,  0,  7,
		 0, 15,  7,  4, 14,  2, 13,  1, 10,  6, 12, 11,  9,  5,  3,  8,
		 4,  1, 14,  8, 13,  6,  2, 11, 15, 12,  9,  7,  3, 10,  5,  0,
		15, 12,  8,  2,  4,  9,  1,  7,  5, 11,  3, 14, 10,  0,  6, 13
	},
	{
		15,  1,  8, 14,  6, 11,  3,  4,  9,  7,  2, 13, 12,  0,  5, 10,
		 3, 13,  4,  7, 15,  2,  8, 14, 12,  0,  1, 10,  6,  9, 11,  5,
		 0, 14,  7, 11, 10,  4, 13,  1,  5,  8, 12,  6,  9,  3,  2, 15,
		13,  8, 10,  1,  3, 15,  4,  2, 11,  6,  7, 12,  0,  5, 14,  9
	},
	{
		10,  0,  9, 14,  6,  3, 15,  5,  1, 13, 12,  7, 11,  4,  2,  8,
		13,  7,  0,  9,  3,  4,  6, 10,  2,  8,  5, 14, 12, 11, 15,  1,
		13,  6,  4,  9,  8, 15,  3,  0, 11,  1,  2, 12,  5, 10, 14,  7,
		 1, 10, 13,  0,  6,  9,  8,  7,  4, 15, 14,  3, 11,  5,  2, 12
	},
	{
		 7, 13, 14,  3,  0,  6,  9, 10,  1,  2,  8,  5, 11, 12,  4, 15,
		13,  8, 11,  5,  6, 15,  0,  3,  4,  7,  2, 12,  1, 10, 14,  9,
		10,  6,  9,  0, 12, 11,  7, 13, 15,  1,  3, 14,  5,  2,  8,  4,
		 3, 15,  0,  6, 10,  1, 13,  8,  9,  4,  5, 11, 12,  7,  2, 14
	},
	{
		 2, 12,  4,  1,  7, 10, 11,  6,  8,  5,  3, 15, 13,  0, 14,  9,
		14, 11,  2, 12,  4,  7, 13,  1,  5,  0, 15, 10,  3,  9,  8,  6,
		 4,  2,  1, 11, 10, 13,  7,  8, 15,  9, 12,  5,  6,  3,  0, 14,
		11,  8, 12,  7,  1, 14,  2, 13,  6, 15,  0,  9, 10,  4,  5,  3
	},
	{
		12,  1, 10, 15,  9,  2,  6,  8,  0, 13,  3,  4, 14,  7,  5, 11,
		10, 15,  4,  2,  7, 12,  9,  5,  6,  1, 13, 14,  0, 11,  3,  8,
		 9, 14, 15,  5,  2,  8, 12,  3,  7,  0,  4, 10,  1, 13, 11,  6,
		 4,  3,  2, 12,  9,  5, 15, 10, 11, 14,  1,  7,  6,  0,  8, 13
	},
	{
		 4, 11,  2, 14, 15,  0,  8, 13,  3, 12,  9,  7,  5, 10,  6,  1,
		13,  0, 11,  7,  4,  9,  1, 10, 14,  3,  5, 12,  2, 15,  8,  6,
		 1,  4, 11, 13, 12,  3,  7, 14, 10, 15,  6,  8,  0,  5,  9,  2,
		 6, 11, 13,  8,  1,  4, 10,  7,  9,  5,  0, 15, 14,  2,  3, 12
	},
	{
		13,  2,  8,  4,  6, 15, 11,  1, 10,  9,  3, 14,  5,  0, 12,  7,
		 1, 15, 13,  8, 10,  3,  7,  4, 12,  5,  6, 11,  0, 14,  9,  2,
		 7, 11,  4,  1,  9, 12, 14,  2,  0,  6, 10, 13, 15,  3,  5,  8,
		 2,  1, 14,  7,  4, 10,  8, 13, 15, 12,  9,  0,  3,  5,  6, 11
	}
};

static u_char	pbox[32] = {
	16,  7, 20, 21, 29, 12, 28, 17,  1, 15, 23, 26,  5, 18, 31, 10,
	 2,  8, 24, 14, 32, 27,  3,  9, 19, 13, 30,  6, 22, 11,  4, 25
};

static uint32_t bits32[32] =
{
	0x80000000, 0x40000000, 0x20000000, 0x10000000,
	0x08000000, 0x04000000, 0x02000000, 0x01000000,
	0x00800000, 0x00400000, 0x00200000, 0x00100000,
	0x00080000, 0x00040000, 0x00020000, 0x00010000,
	0x00008000, 0x00004000, 0x00002000, 0x00001000,
	0x00000800, 0x00000400, 0x00000200, 0x00000100,
	0x00000080, 0x00000040, 0x00000020, 0x00000010,
	0x00000008, 0x00000004, 0x00000002, 0x00000001
};

static u_char	bits8[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

static u_char m_sbox[4][4096];
static uint32_t psbox[4][256];
static uint32_t ip_maskl[8][256], ip_maskr[8][256];
static uint32_t fp_maskl[8][256], fp_maskr[8][256];
static uint32_t key_perm_maskl[8][128], key_perm_maskr[8][128];
static uint32_t comp_maskl[8][128], comp_maskr[8][128];

static u_char m_sbox_flat[4*4096];
static uint32_t psbox_flat[4*256];
static uint32_t ip_maskl_flat[8*256], ip_maskr_flat[8*256];
static uint32_t fp_maskl_flat[8*256], fp_maskr_flat[8*256];
static uint32_t key_perm_maskl_flat[8*128], key_perm_maskr_flat[8*128];
static uint32_t comp_maskl_flat[8*128], comp_maskr_flat[8*128];

/* implicitly flat */
cl_mem buf_m_sbox;
cl_mem buf_psbox;
cl_mem buf_ip_maskl, buf_ip_maskr;
cl_mem buf_fp_maskl, buf_fp_maskr;
cl_mem buf_key_perm_maskl, buf_key_perm_maskr;
cl_mem buf_comp_maskl, buf_comp_maskr;

/* get the results back from the kernel */
cl_mem buf_pw;
cl_mem buf_hash;

/* configuration */
cl_mem buf_config;

/* flatten _crypt_extended_init data for sending to device memory */
static void flatten_init(void)
{
    int i;
    for(i = 0; i < 4; i++)
    {
        memcpy(&m_sbox_flat[i*4096], m_sbox[i], 4096 * sizeof(u_char));
        memcpy(&psbox_flat[i*256], psbox[i], 256 * sizeof(uint32_t));
        memcpy(&ip_maskl_flat[i*256], ip_maskl[i], 256 * sizeof(uint32_t));
        memcpy(&ip_maskr_flat[i*256], ip_maskr[i], 256 * sizeof(uint32_t));
        memcpy(&fp_maskl_flat[i*256], fp_maskl[i], 256 * sizeof(uint32_t));
        memcpy(&fp_maskr_flat[i*256], fp_maskr[i], 256 * sizeof(uint32_t));
        memcpy(&key_perm_maskl_flat[i*128], key_perm_maskl[i],\
            128 * sizeof(uint32_t));
        memcpy(&key_perm_maskr_flat[i*128], key_perm_maskr[i],\
            128 * sizeof(uint32_t));
        memcpy(&comp_maskl_flat[i*128], comp_maskl[i], 128 * sizeof(uint32_t));
        memcpy(&comp_maskr_flat[i*128], comp_maskr[i], 128 * sizeof(uint32_t));
    }
    for(i = 4; i < 8; i++)
    {
        memcpy(&ip_maskl_flat[i*256], ip_maskl[i], 256 * sizeof(uint32_t));
        memcpy(&ip_maskr_flat[i*256], ip_maskr[i], 256 * sizeof(uint32_t));
        memcpy(&fp_maskl_flat[i*256], fp_maskl[i], 256 * sizeof(uint32_t));
        memcpy(&fp_maskr_flat[i*256], fp_maskr[i], 256 * sizeof(uint32_t));
        memcpy(&key_perm_maskl_flat[i*128], key_perm_maskl[i],\
            128 * sizeof(uint32_t));
        memcpy(&key_perm_maskr_flat[i*128], key_perm_maskr[i],\
            128 * sizeof(uint32_t));
        memcpy(&comp_maskl_flat[i*128], comp_maskl[i], 128 * sizeof(uint32_t));
        memcpy(&comp_maskr_flat[i*128], comp_maskr[i], 128 * sizeof(uint32_t));
    }

    int j;
    for(i = 0; i < 4; i++)
        for(j = 0; j < 4096; j++)
            assert(m_sbox[i][j] == m_sbox_flat[i*4096+j]);
    for(i = 0; i < 4; i++)
        for(j = 0; j < 256; j++)
            assert(psbox[i][j] == psbox_flat[i*256+j]);
    for(i = 0; i < 8; i++)
        for(j = 0; j < 256; j++)
        {
            assert(ip_maskl[i][j] == ip_maskl_flat[i*256+j]);
            assert(ip_maskr[i][j] == ip_maskr_flat[i*256+j]);
            assert(fp_maskl[i][j] == fp_maskl_flat[i*256+j]);
            assert(fp_maskr[i][j] == fp_maskr_flat[i*256+j]);
        }
    for(i = 0; i < 8; i++)
        for(j = 0; j < 128; j++)
        {
            assert(key_perm_maskl[i][j] == key_perm_maskl_flat[i*128+j]);
            assert(key_perm_maskr[i][j] == key_perm_maskr_flat[i*128+j]);
            assert(comp_maskl[i][j] == comp_maskl_flat[i*128+j]);
            assert(comp_maskr[i][j] == comp_maskr_flat[i*128+j]);
        }
}

static void _crypt_extended_init(void)
{
	int i, j, b, k, inbit, obit;
	uint32_t *p, *il, *ir, *fl, *fr;
	uint32_t *bits28, *bits24;
	u_char inv_key_perm[64];
    u_char u_key_perm[56];
	u_char inv_comp_perm[56];
	u_char init_perm[64], final_perm[64];
	u_char u_sbox[8][64];
	u_char un_pbox[32];

	bits24 = (bits28 = bits32 + 4) + 4;

	/*
	 * Invert the S-boxes, reordering the input bits.
	 */
	for (i = 0; i < 8; i++)
		for (j = 0; j < 64; j++) {
			b = (j & 0x20) | ((j & 1) << 4) | ((j >> 1) & 0xf);
			u_sbox[i][j] = sbox[i][b];
		}

	/*
	 * Convert the inverted S-boxes into 4 arrays of 8 bits.
	 * Each will handle 12 bits of the S-box input.
	 */
	for (b = 0; b < 4; b++)
		for (i = 0; i < 64; i++)
			for (j = 0; j < 64; j++)
				m_sbox[b][(i << 6) | j] =
					(u_sbox[(b << 1)][i] << 4) |
					u_sbox[(b << 1) + 1][j];

	/*
	 * Set up the initial & final permutations into a useful form, and
	 * initialise the inverted key permutation.
	 */
	for (i = 0; i < 64; i++) {
		init_perm[final_perm[i] = IP[i] - 1] = i;
		inv_key_perm[i] = 255;
	}

	/*
	 * Invert the key permutation and initialise the inverted key
	 * compression permutation.
	 */
	for (i = 0; i < 56; i++) {
		u_key_perm[i] = key_perm[i] - 1;
		inv_key_perm[key_perm[i] - 1] = i;
		inv_comp_perm[i] = 255;
	}

	/*
	 * Invert the key compression permutation.
	 */
	for (i = 0; i < 48; i++) {
		inv_comp_perm[comp_perm[i] - 1] = i;
	}

	/*
	 * Set up the OR-mask arrays for the initial and final permutations,
	 * and for the key initial and compression permutations.
	 */
	for (k = 0; k < 8; k++) {
		for (i = 0; i < 256; i++) {
			*(il = &ip_maskl[k][i]) = 0;
			*(ir = &ip_maskr[k][i]) = 0;
			*(fl = &fp_maskl[k][i]) = 0;
			*(fr = &fp_maskr[k][i]) = 0;
			for (j = 0; j < 8; j++) {
				inbit = 8 * k + j;
				if (i & bits8[j]) {
					if ((obit = init_perm[inbit]) < 32)
						*il |= bits32[obit];
					else
						*ir |= bits32[obit-32];
					if ((obit = final_perm[inbit]) < 32)
						*fl |= bits32[obit];
					else
						*fr |= bits32[obit - 32];
				}
			}
		}
		for (i = 0; i < 128; i++) {
			*(il = &key_perm_maskl[k][i]) = 0;
			*(ir = &key_perm_maskr[k][i]) = 0;
			for (j = 0; j < 7; j++) {
				inbit = 8 * k + j;
				if (i & bits8[j + 1]) {
					if ((obit = inv_key_perm[inbit]) == 255)
						continue;
					if (obit < 28)
						*il |= bits28[obit];
					else
						*ir |= bits28[obit - 28];
				}
			}
			*(il = &comp_maskl[k][i]) = 0;
			*(ir = &comp_maskr[k][i]) = 0;
			for (j = 0; j < 7; j++) {
				inbit = 7 * k + j;
				if (i & bits8[j + 1]) {
					if ((obit=inv_comp_perm[inbit]) == 255)
						continue;
					if (obit < 24)
						*il |= bits24[obit];
					else
						*ir |= bits24[obit - 24];
				}
			}
		}
	}

	/*
	 * Invert the P-box permutation, and convert into OR-masks for
	 * handling the output of the S-box arrays setup above.
	 */
	for (i = 0; i < 32; i++)
		un_pbox[pbox[i] - 1] = i;

	for (b = 0; b < 4; b++)
		for (i = 0; i < 256; i++) {
			*(p = &psbox[b][i]) = 0;
			for (j = 0; j < 8; j++) {
				if (i & bits8[j])
					*p |= bits32[un_pbox[8 * b + j]];
			}
		}
}

void extended_init_flat(void)
{
    _crypt_extended_init();    
    flatten_init();
    /* allocate device memory */
    buf_m_sbox = clCreateBuffer(context, CL_MEM_READ_ONLY,\
        4*4096*sizeof(u_char), NULL, &status);
    assert(!status);
    buf_psbox = clCreateBuffer(context, CL_MEM_READ_ONLY,\
        4*256*sizeof(uint32_t), NULL, &status);
    assert(!status);
    buf_ip_maskl = clCreateBuffer(context, CL_MEM_READ_ONLY,\
        8*256*sizeof(uint32_t), NULL, &status);
    assert(!status);
    buf_ip_maskr = clCreateBuffer(context, CL_MEM_READ_ONLY,\
        8*256*sizeof(uint32_t), NULL, &status);
    assert(!status);
    buf_fp_maskl = clCreateBuffer(context, CL_MEM_READ_ONLY,\
        8*256*sizeof(uint32_t), NULL, &status);
    assert(!status);
    buf_fp_maskr = clCreateBuffer(context, CL_MEM_READ_ONLY,\
        8*256*sizeof(uint32_t), NULL, &status);
    assert(!status);
    buf_key_perm_maskl = clCreateBuffer(context, CL_MEM_READ_ONLY,\
        8*128*sizeof(uint32_t), NULL, &status);
    assert(!status);
    buf_key_perm_maskr = clCreateBuffer(context, CL_MEM_READ_ONLY,\
        8*128*sizeof(uint32_t), NULL, &status);
    assert(!status);
    buf_comp_maskl = clCreateBuffer(context, CL_MEM_READ_ONLY,\
        8*128*sizeof(uint32_t), NULL, &status);
    assert(!status);
    buf_comp_maskr = clCreateBuffer(context, CL_MEM_READ_ONLY,\
        8*128*sizeof(uint32_t), NULL, &status);
    assert(!status);

    buf_pw = clCreateBuffer(context, CL_MEM_READ_WRITE,\
        trips_per_item*global_worksize[0]*9*sizeof(char), NULL, &status);
    assert(!status);
    buf_hash = clCreateBuffer(context, CL_MEM_READ_WRITE,\
        trips_per_item*global_worksize[0]*11*sizeof(char), NULL, &status);
    assert(!status);

    buf_config = clCreateBuffer(context, CL_MEM_READ_ONLY,\
        32*sizeof(char), NULL, &status);
    assert(!status);
    
    /* write to device memory */
    status = clEnqueueWriteBuffer(cmd_queue, buf_m_sbox, CL_TRUE, 0,\
        4*4096*sizeof(u_char), m_sbox_flat, 0, NULL, NULL);
    assert(!status);
    status = clEnqueueWriteBuffer(cmd_queue, buf_psbox, CL_TRUE, 0,\
        4*256*sizeof(uint32_t), psbox_flat, 0, NULL, NULL);
    assert(!status);
    status = clEnqueueWriteBuffer(cmd_queue, buf_ip_maskl, CL_TRUE, 0,\
        8*256*sizeof(uint32_t), ip_maskl_flat, 0, NULL, NULL);
    assert(!status);
    status = clEnqueueWriteBuffer(cmd_queue, buf_ip_maskr, CL_TRUE, 0,\
        8*256*sizeof(uint32_t), ip_maskr_flat, 0, NULL, NULL);
    assert(!status);
    status = clEnqueueWriteBuffer(cmd_queue, buf_fp_maskl, CL_TRUE, 0,\
        8*256*sizeof(uint32_t), fp_maskl_flat, 0, NULL, NULL);
    assert(!status);
    status = clEnqueueWriteBuffer(cmd_queue, buf_fp_maskr, CL_TRUE, 0,\
        8*256*sizeof(uint32_t), fp_maskr_flat, 0, NULL, NULL);
    assert(!status);
    status = clEnqueueWriteBuffer(cmd_queue, buf_key_perm_maskl, CL_TRUE, 0,\
        8*128*sizeof(uint32_t), key_perm_maskl_flat, 0, NULL, NULL);
    assert(!status);
    status = clEnqueueWriteBuffer(cmd_queue, buf_key_perm_maskr, CL_TRUE, 0,\
        8*128*sizeof(uint32_t), key_perm_maskr_flat, 0, NULL, NULL);
    assert(!status);
    status = clEnqueueWriteBuffer(cmd_queue, buf_comp_maskl, CL_TRUE, 0,\
        8*128*sizeof(uint32_t), comp_maskl_flat, 0, NULL, NULL);
    assert(!status);
    status = clEnqueueWriteBuffer(cmd_queue, buf_comp_maskr, CL_TRUE, 0,\
        8*128*sizeof(uint32_t), comp_maskr_flat, 0, NULL, NULL);
    assert(!status);
    
    //char test[11] = "TESTERINO";
    char test[11] = "";
    status = clEnqueueWriteBuffer(cmd_queue, buf_config, CL_TRUE, 20,\
        11*sizeof(char), test, 0, NULL, NULL);
    assert(!status);
    /* write trips per item */
    clEnqueueWriteBuffer(cmd_queue, buf_config, CL_TRUE, 0,\
    sizeof(uint32_t), (char *) &trips_per_item, 0, NULL, NULL);

    assert(!status);
}

void setup_compute(void)
{
    status = clGetPlatformIDs(0, NULL, &num_platforms);
    assert(!status);
    platforms = malloc(num_platforms*sizeof(cl_platform_id));
    status = clGetPlatformIDs(num_platforms, platforms, NULL);
    assert(!status);

    UINT i;
    char c_buffer[CBUFSIZ];
    for (i = 0; i < num_platforms; i++)
    {
        clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, CBUFSIZ, c_buffer,\
        NULL);
        printf("Found Platform %s\n", c_buffer);
        if (target_platform == IntelCPU) 
        {
            if (strstr(c_buffer, "Intel") != NULL)
                platform_index = i;
        }
        else if (target_platform == NvidiaGPU)
        {
            if (strstr(c_buffer, "NVIDIA") != NULL)
                platform_index = i;
        }
    }
    
    assert(platform_index >= 0);
    status = clGetDeviceIDs(platforms[platform_index], CL_DEVICE_TYPE_ALL, 0,\
        NULL, &num_devices);
    assert(!status);
    devices = malloc(sizeof(cl_device_id)*num_devices);     
    status = clGetDeviceIDs(platforms[platform_index], CL_DEVICE_TYPE_ALL,\
        num_devices, devices, NULL);
    assert(!status);
    context = clCreateContext(NULL, num_devices, devices, NULL, NULL, &status);
    assert(!status);
    
    /* Nvidia only implements the older OpenCL API */
    #ifdef OPENCL2_0
    cmd_queue = clCreateCommandQueueWithProperties(context, devices[0], 0, &status);
    assert(!status);
    #else
    cmd_queue = clCreateCommandQueue(context, devices[0], 0, &status);
    assert(!status);
    #endif 
   
    ULONG size;
    clGetDeviceInfo(devices[0], CL_DEVICE_LOCAL_MEM_SIZE, sizeof(cl_ulong),\
    &size, 0);
    printf("Local Memory Limit is %lu bytes\n", size);
    
    extended_init_flat();

    /* ugly hack to pass a pointer to the array containing the kernel source
     * since it is declared as an array by xxd */ 
    const char *kernel_loc = (const char *) triperino_kernel_cl;
    size_t kernel_len = triperino_kernel_cl_len; 
    triperino_program = clCreateProgramWithSource(context, 1,\
    (const char **) &kernel_loc, &kernel_len, &status);
    assert(!status);

    status = clBuildProgram(triperino_program, num_devices, devices, NULL,\
        NULL, NULL);    
    if(status)
    printf("%i\n", status);

    char log[4096];
    triperino_kernel = clCreateKernel(triperino_program, "triperino", &status);
    status = clGetProgramBuildInfo(triperino_program, devices[0],\
    CL_PROGRAM_BUILD_LOG, 4096, &log, NULL);
    if(status)
    printf("%s\n", log);
    assert(!status);
    
    /*set kernel arguments */
    status = clSetKernelArg(triperino_kernel, 0, sizeof(cl_mem), &buf_m_sbox);
    assert(!status);
    status = clSetKernelArg(triperino_kernel, 1, sizeof(cl_mem), &buf_psbox);
    assert(!status);
    status = clSetKernelArg(triperino_kernel, 2, sizeof(cl_mem), &buf_ip_maskl);
    assert(!status);
    status = clSetKernelArg(triperino_kernel, 3, sizeof(cl_mem), &buf_ip_maskr);
    assert(!status);
    status = clSetKernelArg(triperino_kernel, 4, sizeof(cl_mem), &buf_fp_maskl);
    assert(!status);
    status = clSetKernelArg(triperino_kernel, 5, sizeof(cl_mem), &buf_fp_maskr);
    assert(!status);
    status = clSetKernelArg(triperino_kernel, 6, sizeof(cl_mem), &buf_key_perm_maskl);
    assert(!status);
    status = clSetKernelArg(triperino_kernel, 7, sizeof(cl_mem), &buf_key_perm_maskr);
    assert(!status);
    status = clSetKernelArg(triperino_kernel, 8, sizeof(cl_mem), &buf_comp_maskl);
    assert(!status);
    status = clSetKernelArg(triperino_kernel, 9, sizeof(cl_mem), &buf_comp_maskr);
    assert(!status);
    status = clSetKernelArg(triperino_kernel, 10, sizeof(cl_mem), &buf_pw);
    assert(!status);
    status = clSetKernelArg(triperino_kernel, 11, sizeof(cl_mem), &buf_hash);
    assert(!status);
    status = clSetKernelArg(triperino_kernel, 12, sizeof(cl_mem), &buf_config);
    assert(!status);

}

void execute_compute(int seed_offset, char pat[11], char case_sens)
{
    /* write the seed */
    clEnqueueWriteBuffer(cmd_queue, buf_config, CL_TRUE, 4,\
    sizeof(int), (char *) &seed_offset, 0, NULL, NULL);
    assert(!status);
    if (!case_sens)
    {
        int i;
        for(i = 0; pat[i]; i++)
        {
           pat[i] = tolower(pat[i]);
        }
    }
    /* write pattern and case_sensitive flag */
    clEnqueueWriteBuffer(cmd_queue, buf_config, CL_TRUE, 8,\
    11*sizeof(char), (char *) pat, 0, NULL, NULL);
    assert(!status);
    clEnqueueWriteBuffer(cmd_queue, buf_config, CL_TRUE, 19,\
    1*sizeof(char), &case_sens, 0, NULL, NULL);
    assert(!status);

    local_worksize[0] = 256;
    status = clEnqueueNDRangeKernel(cmd_queue, triperino_kernel, 1, NULL,\
        global_worksize, NULL, 0, NULL, NULL);
    if(status)
    printf("%i\n", status);
    assert(!status);

    char *hash = malloc(trips_per_item*global_worksize[0]*11*sizeof(char));
    char *pw = malloc(trips_per_item*global_worksize[0]*9*sizeof(char));
    
    status = clEnqueueReadBuffer(cmd_queue, buf_hash, CL_TRUE, 0,\
    trips_per_item*global_worksize[0]*11*sizeof(char), hash, 0, NULL, NULL);
    if(status)
    printf("%i\n", status);
    assert(!status);

    status = clEnqueueReadBuffer(cmd_queue, buf_pw, CL_TRUE, 0,\
    trips_per_item*global_worksize[0]*9*sizeof(char), pw, 0, NULL, NULL);
    assert(!status);
    
    UINT i;
    for (i = 0; i < global_worksize[0]; i++)
    {
        UINT j = 0;
        while(strlen(&hash[i*trips_per_item*11 + j*11]) > 0 && j < trips_per_item)
        {
            printf("%-8s....%s\n", &pw[i*trips_per_item*9 + j*9],\
             &hash[i*trips_per_item*11 + j*11]);
            j++;
        }
    }

    //printf("%s\n", pw);  
    free(pw);
    free(hash);
}

void cleanup_compute(void)
{
    clReleaseKernel(triperino_kernel);
    clReleaseProgram(triperino_program);
    clReleaseCommandQueue(cmd_queue);
    clReleaseContext(context);


    clReleaseMemObject(buf_m_sbox);
    clReleaseMemObject(buf_psbox);
    clReleaseMemObject(buf_ip_maskl);
    clReleaseMemObject(buf_ip_maskr);
    clReleaseMemObject(buf_fp_maskl);
    clReleaseMemObject(buf_fp_maskr);
    clReleaseMemObject(buf_key_perm_maskl);
    clReleaseMemObject(buf_key_perm_maskr);
    clReleaseMemObject(buf_comp_maskl);
    clReleaseMemObject(buf_comp_maskr);

    clReleaseMemObject(buf_pw);
    clReleaseMemObject(buf_hash);

    free(platforms); 

    /*I've never actually seen this used in practice*/
    /*
    uint i;
    for (i = 0; i < num_devices; i++)
    {
        clReleaseDevice(devices[i]);
    }
    */
    free(devices);
}

int main()
{
    struct timeval_ m_time;
    struct timeval_ old_time;
    char pat[11] = "OPENCL";
    setup_compute();
    int i;
    for (i = 0; i < 8; i++)
    {
        gettimeofday(&old_time, NULL);
        execute_compute(global_worksize[0]*i + 9999, pat, 0);
        gettimeofday(&m_time, NULL);
        printf("%f\n", (global_worksize[0]*trips_per_item*UNSAFE_SPACE)/\
       ((m_time.tv_sec + m_time.tv_usec*1e-6) -\
       (old_time.tv_sec + old_time.tv_usec*1e-6)));
    }
    cleanup_compute();
    return 0;
}
