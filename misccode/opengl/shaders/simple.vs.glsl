#version 410 core 

layout (location = 0) in vec3 position; 
layout (location = 1) in vec2 texcoord; 
layout (location = 2) in vec3 normal; 
out VS_OUT 
{ 
 vec2 texcoord; 
 vec3 normal; 
 vec4 position; 
} vs_out; 
 
uniform mat4 mvp_matrix; 
uniform mat4 model_matrix; 
uniform vec3 eye_pos;
uniform vec3 lightDir;
uniform float time;
 
void main(void) 
{ 
 gl_Position = mvp_matrix * vec4( position, 1.0f ); 

 vs_out.texcoord = texcoord; 
 vs_out.normal = normal; 
 vs_out.position = mvp_matrix * vec4( position, 1.0f );
} 
