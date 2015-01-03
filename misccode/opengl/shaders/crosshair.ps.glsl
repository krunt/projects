#version 410 core 
layout (binding = 1) uniform sampler2D crosshairTex; 
in VS_OUT 
{ 
 vec2 texcoord; 
} fs_in; 
out vec4 color; 

uniform mat4 mvp_matrix; 
uniform mat4 model_matrix; 
uniform vec3 eye_pos;
uniform vec3 light_pos;
uniform vec3 light_dir;

void main(void) {
 color = texture( crosshairTex, fs_in.texcoord );
}
