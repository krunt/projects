#version 410 core 
layout (location = 0) in vec3 position; 
layout (location = 1) in vec2 texcoord; 
layout (location = 2) in vec3 normal; 
out VS_OUT 
{ 
 vec2 texcoord; 
} vs_out; 
 
uniform mat4 mvp_matrix; 
uniform mat4 model_matrix; 
uniform vec3 eye_pos;
uniform vec3 light_pos;
uniform vec3 light_dir;
 
void main(void) 
{ 
 gl_Position = vec4( position.xy, 0, 1.0f ); 
 vs_out.texcoord = texcoord;
} 
