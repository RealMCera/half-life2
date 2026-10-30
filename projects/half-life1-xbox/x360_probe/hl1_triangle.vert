#version 450
layout(location = 0) in vec2 a_position;
layout(location = 1) in vec3 a_color;
layout(location = 0) out vec3 varying_rgb;
void main() {
    gl_Position = vec4(a_position, 0.0, 1.0);
    varying_rgb = a_color;
}
