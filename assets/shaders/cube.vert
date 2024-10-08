#version 330 core
layout (location = 0) in vec3 aPos; // the position variable has attribute position 0
layout (location = 1) in vec2 aTexCoord; // the texture coordinate has attribute position 1
layout (location = 2) in vec3 aNormal; // the normal has attribute position 2

out vec2 TexCoord; // output a texture coordinate to the fragment shader
out vec3 Normal; // output a normal to the fragment shader
out vec3 FragPos; // output a fragment position to the fragment shader

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TexCoord = aTexCoord; // pass the texture coordinate to the fragment shader
    Normal = mat3(transpose(inverse(model))) * aNormal; // pass the normal vector to the fragment shader
}