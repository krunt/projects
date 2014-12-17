#version 410 core 
layout (binding = 0) uniform sampler2D sq_tex; 
layout (binding = 1) uniform sampler2D tex; 
in VS_OUT 
{ 
 vec2 texcoord; 
 vec3 normal; 
 vec3 position; 
} fs_in; 
out vec4 color; 

uniform vec3 eye_pos;

void main(void) {
 float light_coeff = 0.6;
 vec4 texture_color = texture(tex, fs_in.texcoord);
 color = texture_color * light_coeff;
}
