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
 vec3 distv = light_pos - fs_in.position;
 float dist = length(distv);

 float att = dot( vec3(0.002, 0.003, 0.001), vec3(1.0f, dist, dist * dist) );

 float light_coeff = 0.6;
 float diffuse_factor = dot(fs_in.normal, light_dir);
 if ( diffuse_factor > 0 ) {
     light_coeff += diffuse_factor / att;

     vec3 v = reflect(light_dir, fs_in.normal);
     float spec_factor = pow(max(dot(v, eye_pos), 0.0f), 0.2f);

     light_coeff += spec_factor / att;
 }

 vec4 texture_color = texture(tex, fs_in.texcoord);
 color = texture_color * light_coeff;
} 
