__kernel void convertARGBU8ToRGBint(__read_only image2d_t inARGB, __global uchar * dstptr, int dst_cols, int channelSz)
{
int i = get_global_id(0); // range [0, width]
int j = get_global_id(1); // range [0, height]

const sampler_t smp = CLK_FILTER_NEAREST | CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE;

uint4 bgr = read_imageui(inARGB, smp, (int2)(i, j)).xyzw;

// Note: BGR
__global uchar* pB = dstptr + dst_cols * j + i;
__global uchar* pG = pB + channelSz;
__global uchar* pR = pG + channelSz;

*pR = bgr.x;
*pG = bgr.y;
*pB = bgr.z;
}



__kernel void convertRGBintToARGB(__write_only image2d_t outARGB, __global float * srcptr,  int dst_cols, int channelSz)
{
int i = get_global_id(0); // range [0, width]
int j = get_global_id(1); // range [0, height]


// Note: RGBP -> RRGGBB
__global float* pB = srcptr + dst_cols * j + i;
__global float* pG = pB + channelSz;
__global float* pR = pG + channelSz;

// Note: RRGGBB -> RRGGBB
//__global uchar* pR = srcptr + dst_cols * j * 3+ i*3; 
//__global uchar* pG = srcptr + dst_cols * j * 3+ i*3+1;
//__global uchar* pB = srcptr + dst_cols * j * 3+ i*3+2;

uint4 rgba;

rgba.z = convert_int((*pR+1)/2 * 255);
rgba.y = convert_int((*pG+1)/2 * 255);
rgba.x = convert_int((*pB+1)/2 * 255);
//rgba.w = 0;
rgba.w = 1;  //for png output

write_imageui(outARGB, (int2)(i, j), rgba);
}