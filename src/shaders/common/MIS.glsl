#ifndef MIS_GLSL
#define MIS_GLSL


// Veach-Guibas balance huestric - based on the numerically stable formula found here https://github.com/madmann91/sol/blob/master/include/sol/renderer.h#L57
float MISWeight(in float top, in float bottom) {
    return 1.0f / (1.0f + bottom / top);
}


#endif