#version 450

layout (location = 0) in VsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 Normal;
    vec3 ViewPos;
    vec4 FragPosLightSpace;
} fsIn;

layout (location = 0) out vec4 FragColour;

layout (set = 2, binding = 0) uniform sampler2D diffuseMap;
//layout (set = 2, binding = 0) uniform samplerCube diffuseMap;
layout (set = 2, binding = 1) uniform sampler2D normalMap;
layout (set = 2, binding = 2) uniform sampler2D specularMap;
layout (set = 2, binding = 3) uniform sampler2D shadowMap;

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

float calcShadows(vec4 fragPosLight, float bias) {
    vec4 projCoords = fragPosLight / fragPosLight.w;
    projCoords = projCoords * 0.5 + 0.5;
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;
    return shadow;
//    float shadow = 1;
//    vec4 shadowCoord = fragPosLight / fragPosLight.w;
//    if (shadowCoord.z > -1.0 && shadowCoord.z < 1.0) {
//        float dist = texture(shadowMap, shadowCoord.xy).r;
//        if (shadowCoord.w > 0.0 && dist < shadowCoord.z) {
//            shadow = 0;
//        }
//    }
//    return shadow;
}

void main() {

    vec3 diffuseColour = texture(diffuseMap, fsIn.TexCoords).rgb;
    vec3 normalColour = texture(normalMap, fsIn.TexCoords).rgb;
    vec3 specularColour = texture(specularMap, fsIn.TexCoords).rgb;

    vec3 lightDir = normalize(-light.position);

    vec3 normal = normalize(normalColour * 2.0 - 1.0);
    normal = normalize(fsIn.TBN * normal);
//    vec3 normal = fsIn.Normal;

    float diff = max(dot(lightDir, normal), 0.0) * light.intensity;
    vec3 diffuse = diff * diffuseColour;

    vec3 viewDir = normalize(fsIn.ViewPos - fsIn.FragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    vec3 halfWayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfWayDir), 0.0), 32.0);
    vec3 specular = vec3(0.3) * spec * specularColour;

    float bias = max(0.01 * (1.0 - dot(fsIn.Normal, lightDir)), 0.005);
    float shadow = calcShadows(fsIn.FragPosLightSpace, bias);

    vec3 colour = (diffuse + specular) * light.colour * (1 - shadow);
    //FragColour = vec4(shadow, shadow, shadow, 1.0f);
    FragColour = vec4(colour, 1.0f);
}