#ifndef RANDOM_GLSL
#define RANDOM_GLSL

/*
Taken from https://developer.nvidia.com/gpugems/gpugems3/part-vi-gpu-computing/chapter-37-efficient-random-number-generation-and-application
This GPU Gems chapter currently serves as the basis for my RNG

// S1, S2, S3, and M are all constants, and z is part of the    
// private per-thread generator state.    
unsigned TausStep(unsigned &z, int S1, int S2, int S3, unsigned M) {
   unsigned b=(((z << S1) ^ z) >> S2);
   return z = (((z & M) << S3) ^ b); 
} 

// A and C are constants    
unsigned LCGStep(unsigned &z, unsigned A, unsigned C) {
   return z=(A*z+C); 
}

unsigned z1, z2, z3, z4; 
float HybridTaus() {
   // Combined period is lcm(p1,p2,p3,p4)~ 2^121    
   return 2.3283064365387e-10 * (              
   // Periods     
   TausStep(z1, 13, 19, 12, 4294967294UL) ^  
   // p1=2^31-1     
   TausStep(z2, 2, 25, 4, 4294967288UL) ^    
   // p2=2^30-1     
   TausStep(z3, 3, 11, 17, 4294967280UL) ^   // p3=2^28-1     
   LCGStep(z4, 1664525, 1013904223UL)        // p4=2^32    ); 
}
*/

layout(std430) buffer randomState {
    uvec4 states[];
};

layout(std430) buffer ldSamplerStateTex {
    uint ldStates[];
};

uint stateIdx;
uvec4 state;

uint ldState;

uint TausStep(inout uint z, uint s1, uint s2, uint s3, uint m) {
    uint b = (((z << s1) ^ z) >> s2);
    z = (((z & m) << s3) ^ b);
    return z;
}

uint LCGStep(inout uint z, uint a, uint c) {
    z = a * z + c;
    return z;
}

uint HybridTausInteger() {
    return
        TausStep(state.x, 13, 19, 12, 4294967294U) ^
        TausStep(state.y, 2, 25, 4, 4294967288U) ^
        TausStep(state.z, 3, 11, 17, 4294967280U) ^
        LCGStep(state.w, 1664525, 1013904223U);
}

float HybridTaus() {
    return 2.3283064365387e-10 * float(
        HybridTausInteger()
    );
}

// https://blog.demofox.org/2017/05/29/when-random-numbers-are-too-random-low-discrepancy-sequences/
// might be a good source of info

#define rand() HybridTaus()

vec2 Random2D() {
    return vec2(rand(), rand());
}

#define NUM_LD_POINTS 24
vec2 ldPoints[NUM_LD_POINTS];
int ldNext = 0;

vec2 NextPointLD() {
    if (ldNext < NUM_LD_POINTS) {
        return ldPoints[ldNext++];
    }
    else {
        return Random2D();
    }
}

samplerBuffer stratifiedTex;
uniform int stratumIdx;

const int stride = 16;

vec2 NextStratifiedSample(int idx) {
    idx *= stride;
    return texelFetch(stratifiedTex, idx + stratumIdx).xy;
}


float VanDerCorput(uint n, uint base) {
#define MY_IMPL
#ifdef MY_IMPL
    float sum = 0.0f;
    float ibase = 1.0f / base;

    while (n > 0) {
        // What is remaining at this digit?
        uint remaining = (n % base);
        // Multiply by b^-i
        sum += remaining * ibase;
        // Bit shift by on
        n /= base;
        // Update to get our new b^-i
        ibase /= base;
    }

    return sum;
#else
    float invBase = 1.0 / float(base);
    float denom = 1.0;
    float result = 0.0;

    for (uint i = 0u; i < 32u; ++i)
    {
        if (n > 0u)
        {
            denom = mod(float(n), 2.0);
            result += denom * invBase;
            invBase = invBase / 2.0;
            n = uint(float(n) / 2.0);
        }
    }

    return result;
#endif
}

vec2 HaltonSequence(in uint n) {
    return vec2(VanDerCorput(n, 2), VanDerCorput(n, 3));
}

vec2 NextHalton() {
    return HaltonSequence(ldState++);
}

#define rand2() Random2D()

vec2 HammerslySequence(in uint n, in uint offset) {
    return vec2(float(n + rand()) / float(NUM_LD_POINTS), VanDerCorput(n + offset, 2));
}

void CreateHaltonSequenceSamples() {
    uint offset = HybridTausInteger();
    for (uint i = 0; i < NUM_LD_POINTS; i++) {
        ldPoints[i] = HaltonSequence(offset + i);
    }
}

void CreateHammerslySequenceSamples() {
    uint offset = HybridTausInteger();
    for (uint i = 0; i < NUM_LD_POINTS; i++) {
        ldPoints[i] = HammerslySequence(i, offset);
    }
}

// Implementation of "Golden Ratio Sequences For Low-Discrepancy Sampling"
// See https://www.graphics.rwth-aachen.de/media/papers/jgt.pdf
float NextGoldenRatio(inout uint seed) {
    seed += 2654435769;
    return 2.3283064365387e-10f * float(seed);
}

void CreateGoldenRatioSamples() {
    uvec2 seed = uvec2(HybridTausInteger(), HybridTausInteger());

    float minval = 1.0f;
    uint idx = 0;

    for (uint i = 0; i < NUM_LD_POINTS; i++) {
        float x = fract(NextGoldenRatio(seed.x) + rand());
        ldPoints[i].y = x;
        if (x < minval) {
            minval = x;
            idx = i;
        }
    }

    // Find our increment/decrement variables for our permuation
    uint f = 1, fp = 1, parity = 0;
    while (f + fp < NUM_LD_POINTS) {
        uint temp = f;
        f += fp;
        fp = temp;
        parity++;
    }
    uint inc = fp, dec = f;
    if (bool(parity & 1)) {
        inc = f;
        dec = fp;
    }

    // sigma(i) is originally the minimum position in the sequence
    ldPoints[0].x = ldPoints[idx].y;
    for (uint i = 1; i < NUM_LD_POINTS; i++) {
        // Choose next index
        if (idx < dec) {
            idx += inc;
            if (idx >= NUM_LD_POINTS) {
                idx -= dec;
            }
        }
        else {
            idx -= dec;
        }
        ldPoints[i].x = ldPoints[idx].y;
    }

    // Normal golden sequence for next set of points
    for (int i = 0; i < NUM_LD_POINTS; i++) {
        ldPoints[i].y = fract(NextGoldenRatio(seed.y) + rand());
    }
}

void CreateRandomHighDiscrepancySamples() {
    for (int i = 0; i < NUM_LD_POINTS; i++) {
        ldPoints[i] = Random2D();
    }
}

#define InitializeLD() ;//CreateGoldenRatioSamples()

const int kNumStrataPerSide = 4;
const int kNumStrata = kNumStrataPerSide * kNumStrataPerSide;
vec2 Random2DStratified() {
    // Choose a random strata
    int stratum = int(rand() * kNumStrata);
    // Transform our stratum index to an (x, y) offset
    // Calculations taken from here http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2008%20-%20variance%20reduction.pdf
    vec2 offset = vec2(
        stratum % kNumStrataPerSide,
        stratum / kNumStrataPerSide
    );
    vec2 r = Random2D() +offset;
    return r / kNumStrataPerSide;
}


void initRNG(uint ridx) {
    stateIdx = ridx;
    state = states[ridx];
    ldState = ldStates[ridx];
    InitializeLD();
}

void freeRNG() {
    states[stateIdx] = state;
    ldStates[stateIdx] = ldState;
}

#endif