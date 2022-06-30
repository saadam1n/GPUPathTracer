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

uint stateIdx;
uvec4 state;

uint TausStep(inout uint z, uint s1, uint s2, uint s3, uint m) {
    uint b = (((z << s1) ^ z) >> s2);
    z = (((z & m) << s3) ^ b);
    return z;
}

uint LCGStep(inout uint z, uint a, uint c) {
    z = a * z + c;
    return z;
}

float HybridTaus() {
    return 2.3283064365387e-10 * float(
        TausStep(state.x, 13, 19, 12, 4294967294U) ^ 
        TausStep(state.y, 2, 25, 4, 4294967288U) ^ 
        TausStep(state.z, 3, 11, 17, 4294967280U) ^ 
        LCGStep(state.w, 1664525, 1013904223U)
    );
}
#define rand() HybridTaus()

vec2 Random2D() {
    return vec2(rand(), rand());
}

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

#define rand2() Random2D()

void initRNG(uint ridx) {
    stateIdx = ridx;
    state = states[ridx];
}

void freeRNG() {
    states[stateIdx] = state;
}

#endif