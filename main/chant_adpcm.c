#include "chant_adpcm.h"
static const int steps[89] = {
 7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,
 88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,
 598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,
 2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,
 11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767
};
static const int changes[8] = {-1,-1,-1,-1,2,4,6,8};
void chant_decode(chant_decoder_t *s, const uint8_t *data,
                  size_t first, size_t count, int16_t *out) {
    for (size_t i = 0; i < count; i++) {
        size_t pos = first + i;
        int nibble = (pos & 1) ? (data[pos / 2] & 15) : (data[pos / 2] >> 4);
        int step = steps[s->index];
        int delta = step >> 3;
        if (nibble & 4) delta += step;
        if (nibble & 2) delta += step >> 1;
        if (nibble & 1) delta += step >> 2;
        s->predictor += (nibble & 8) ? -delta : delta;
        if (s->predictor > 32767) s->predictor = 32767;
        if (s->predictor < -32768) s->predictor = -32768;
        s->index += changes[nibble & 7];
        if (s->index < 0) s->index = 0;
        if (s->index > 88) s->index = 88;
        out[i] = (int16_t)s->predictor;
    }
}

unsigned chant_encode_sample(int sample,int *predictor,int *index){int step=steps[*index],diff=sample-*predictor,n=diff<0?8:0;if(diff<0)diff=-diff;int delta=step>>3;if(diff>=step){n|=4;diff-=step;delta+=step;}step>>=1;if(diff>=step){n|=2;diff-=step;delta+=step;}step>>=1;if(diff>=step){n|=1;delta+=step;}*predictor+=(n&8)?-delta:delta;if(*predictor>32767)*predictor=32767;if(*predictor< -32768)*predictor=-32768;*index+=changes[n&7];if(*index<0)*index=0;if(*index>88)*index=88;return n;}
