#version 410 core 
layout (binding = 0) uniform sampler2D screenTex; 
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
 //color = vec4( fs_in.texcoord.xy, 0, 0.1 );
 color = texture( screenTex, fs_in.texcoord );
}
