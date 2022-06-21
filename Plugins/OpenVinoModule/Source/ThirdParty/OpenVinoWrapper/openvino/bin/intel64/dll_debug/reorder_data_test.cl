__kernel void convertARGBU8ToRGBint(__read_only image2d_t inARGB, __global uchar * dstptr, int dst_cols, int channelSz)
{
int i = get_global_id(0); // range [0, width]
int j = get_global_id(1); // range [0, height]

const sampler_t smp = CLK_FILTER_NEAREST | CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE;

float4 bgr = read_imagef(inARGB, smp, (int2)(i, j)).xyzw;

// Note: BGR
__global uchar* pB = dstptr + dst_cols * j + i;
__global uchar* pG = pB + channelSz;
__global uchar* pR = pG + channelSz;

*pR = bgr.x * 255;
*pG = bgr.y * 255;
*pB = bgr.z * 255;
}



__kernel void convertRGBintToARGB(__write_only image2d_t outARGB, __global uchar * srcptr,  int dst_cols, int channelSz)
{
int i = get_global_id(0); // range [0, width]
int j = get_global_id(1); // range [0, height]


// Note: BGR
__global uchar* pB = srcptr + dst_cols * j + i;
__global uchar* pG = pB + channelSz;
__global uchar* pR = pG + channelSz;

// Note: BGR
//__global uchar* pR = srcptr + dst_cols * j * 3+ i*3; 
//__global uchar* pG = srcptr + dst_cols * j * 3+ i*3+1;
//__global uchar* pB = srcptr + dst_cols * j * 3+ i*3+2;

float4 rgba;

rgba.z = *pR * (1.0f / 255.0f);
rgba.y = *pG * (1.0f / 255.0f);
rgba.x = *pB * (1.0f / 255.0f);
//rgba.w = 0;
rgba.w = 1;  //for png output

write_imagef(outARGB, (int2)(i, j), rgba);
}