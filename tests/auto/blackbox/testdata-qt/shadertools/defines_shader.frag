#version 440
precision mediump float;

#ifndef TEST_DEFINE_1
#error "TEST_DEFINE_1 is not defined"
#endif

#ifndef TEST_DEFINE_2
#error "TEST_DEFINE_2 is not defined"
#endif

layout(location = 0) out vec4 fragColor;

void main() {
    fragColor = vec4(1.0, 0.0, 0.0, 1.0);
}