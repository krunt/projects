#version 410 core 
layout (binding = 0) uniform sampler2D sq_tex; 
layout (binding = 1) uniform sampler2D tex; 
layout (binding = 2) uniform samplerCube texCube; 
in VS_OUT 
{ 
 vec2 texcoord; 
 vec3 normal; 
 vec4 position; 
} fs_in; 
out vec4 color; 

uniform mat4 mvp_matrix; 
uniform mat4 model_matrix; 
uniform vec3 eye_pos;
uniform vec3 light_pos;
uniform vec3 light_dir;

void main(void) {
 //float light_coeff = 1.6;
 //vec4 texture_color = texture(tex, fs_in.texcoord);
 //color = texture_color * light_coeff;

 vec3 toEye = normalize( eye_pos - fs_in.position.xyz );

 float specularPower = 10.0f;
 float ambientCoeff = 0.6;
 float diffuseCoeff = dot( fs_in.normal, -light_dir );
 float specularCoeff = pow( dot( fs_in.normal, toEye ), specularPower );
 float colorCoeff = ambientCoeff + diffuseCoeff + specularCoeff;

 vec4 textureColor = texture(tex, fs_in.texcoord);
 color = ambientCoeff * textureColor;
 //color = vec4( 0, 0, clamp( fs_in.position.z / fs_in.position.w, 0, 1 ), 1 );
}
