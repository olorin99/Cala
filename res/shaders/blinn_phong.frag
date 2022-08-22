#version 450

layout (location = 0) in VsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 ViewPos;
} fsIn;

layout (location = 0) out vec4 FragColour;

//layout (set = 2, binding = 0) uniform sampler2D diffuseMap;
layout (set = 2, binding = 0) uniform samplerCube diffuseMap;
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

    vec3 diffuseColour = texture(diffuseMap, fsIn.FragPos).rgb;
    vec3 normalColour = texture(normalMap, fsIn.TexCoords).rgb;
    vec3 specularColour = texture(specularMap, fsIn.TexCoords).rgb;

    vec3 lightDir = normalize(light.position - fsIn.FragPos);

    vec3 normal = normalize(normalColour * 2.0 - 1.0);
    normal = normalize(fsIn.TBN * normal);

    float diff = max(dot(lightDir, normal), 0.0) * light.intensity;
    vec3 diffuse = diff * diffuseColour;

    vec3 viewDir = normalize(fsIn.ViewPos - fsIn.FragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    vec3 halfWayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfWayDir), 0.0), 32.0);
    vec3 specular = vec3(0.3) * spec * specularColour;

    float distance = length(light.position - fsIn.FragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);

    vec3 colour = (diffuse + specular) * attenuation * light.colour;
    FragColour = vec4(colour, 1.0f);
}