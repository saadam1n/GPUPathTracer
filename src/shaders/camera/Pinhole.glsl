#ifndef PINHOLE_CAMERA_GLSL
#define PINHOLE_CAMERA_GLSL

struct CameraParameters {
    vec3 Corner[2][2];

    vec3 Position;
};

vec3 ComputeRayDirection(in CameraParameters PinholeCamera, in vec2 UV) {

    vec3 Direction;

    vec3 Y[2];

    Y[0] = mix(PinholeCamera.Corner[0][0], PinholeCamera.Corner[0][1], UV.x);
    Y[1] = mix(PinholeCamera.Corner[1][0], PinholeCamera.Corner[1][1], UV.x);

    Direction = mix(Y[0], Y[1], UV.y);

    Direction = normalize(Direction);

    return Direction;
}

#endif