#version 450

layout (location = 0) in VsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 ViewPos;
} fsIn;

layout (location = 0) out vec4 FragColour;

layout (set = 2, binding = 0) uniform sampler2D diffuseMap;
layout (set = 2, binding = 1) uniform sampler2D normalMap;
layout (set = 2, binding = 2) uniform sampler2D specularMap;

struct PointLight {
    vec3 position;
    vec3 colour;
    float intensity;
    float constant;
    float linear;
    float quadratic;
    float radius;
};

layout (set = 3, binding = 0) uniform LightData {
    PointLight light;
};

void main() {

    vec3 diffuseColour = texture(diffuseMap, fsIn.TexCoords).rgb;
    vec3 normalColour = texture(normalMap, fsIn.TexCoords).rgb;
    vec3 specularColour = texture(specularMap, fsIn.TexCoords).rgb;

    vec3 norm = normalize(normalColour * 2.0 - 1.0);
    norm = normalize(fsIn.TBN * norm);

    vec3 lightDir = normalize(light.position - fsIn.FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * diffuseColour;

    float specularStrength = 0.5;
    vec3 viewDir = normalize(fsIn.ViewPos - fsIn.FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * specularColour;

    float distance = length(light.position - fsIn.FragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);

    vec3 colour = (diffuse + specular) * attenuation * light.colour;
    FragColour = vec4(colour, 1.0f);
}