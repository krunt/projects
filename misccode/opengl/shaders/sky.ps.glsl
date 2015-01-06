#version 410 core 
layout (binding = 0) uniform samplerCube skyBox; 
in VS_OUT 
{ 
 vec2 texcoord; 
 vec3 normal; 
 vec3 position; 
} fs_in; 
out vec4 color; 

uniform mat4 mvp_matrix; 
uniform mat4 model_matrix; 
uniform vec3 eye_pos;
uniform vec3 lightDir;
uniform float time;

void main(void) {
 float light_coeff = 1.6;
 //color = vec4( fs_in.normal, 1 ) * 0.2 + textureCube(skyBox, fs_in.normal) * light_coeff;
 //color = textureCube(skyBox, fs_in.normal) * light_coeff;
 //color = vec4( 1.0, 0, 0, 1 );
 //color = vec4( fs_in.normal, 1 );
 color = textureCube(skyBox, fs_in.position) * light_coeff;
}
