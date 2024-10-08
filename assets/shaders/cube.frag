#version 330 core
out vec4 FragColor; // the output color of the fragment shader
in vec2 TexCoord; // the input texture coordinate from the vertex shader
in vec3 Normal; // the input normal from the vertex shader
in vec3 FragPos; // the input fragment position from the vertex shader

// texture sampler
uniform sampler2D texture1;
uniform vec3 lightPos;
uniform vec3 viewPos;

void main() {
    vec3 lightColor = vec3(0.2, 0.2, 1.0);
    // ambient
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * vec3(1.0, 1.0, 1.0);

    // diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;

    vec4 result = vec4((ambient + diffuse + specular),1.0) * texture(texture1, TexCoord).rgba;
    FragColor = result;
}