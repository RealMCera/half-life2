#version 450
layout(location = 0) in vec3 varying_rgb;
layout(location = 0) out vec4 final_rgba;
void main() {
    final_rgba = vec4(varying_rgb, 1.0);
}
