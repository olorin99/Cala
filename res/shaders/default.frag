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

vec3 calcPointLight(PointLight light, vec3 diffuse, vec3 normal, float specular, vec3 fragPos, vec3 viewDir) {
    float distance = length(light.position - fragPos);
    vec3 lightDir = normalize(light.position - fragPos);
    vec3 diffuseColour = max(dot(normal, lightDir), 0.0) * diffuse * light.colour;
    vec3 halfWayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfWayDir), 0.0), light.intensity);
    vec3 specColour = light.colour * spec * specular;
    float attenuation = 1.0 / (1.0 + light.linear * distance + light.quadratic * distance * distance);
    return (diffuseColour + specColour) * attenuation;
}

layout (set = 3, binding = 0) uniform LightData {
    PointLight light;
};

void main() {
    vec3 diffuseColour = texture(diffuseMap, fsIn.TexCoords).rgb;
    vec3 normalColour = texture(normalMap, fsIn.TexCoords).rgb;
    vec3 specularColour = texture(specularMap, fsIn.TexCoords).rgb;

    normalColour = normalize(normalColour * 2.0 - 1.0);
    normalColour = normalize(fsIn.TBN * normalColour);

    vec3 viewDir = normalize(fsIn.ViewPos - fsIn.FragPos);
    vec3 colour = calcPointLight(light, diffuseColour, normalColour, specularColour.r, fsIn.FragPos, viewDir);

    FragColour = vec4(colour, 1.0f);
}