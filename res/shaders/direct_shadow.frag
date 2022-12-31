#version 450

layout (location = 0) in VsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 Normal;
    vec3 ViewPos;
    vec4 WorldPosLightSpace;
} fsIn;

layout (location = 0) out vec4 FragColour;

layout (set = 2, binding = 0) uniform sampler2D diffuseMap;
//layout (set = 2, binding = 0) uniform samplerCube diffuseMap;
layout (set = 2, binding = 1) uniform sampler2D normalMap;
layout (set = 2, binding = 2) uniform sampler2D specularMap;
layout (set = 2, binding = 3) uniform sampler2D shadowMap;

struct PointLight {
    vec3 direction;
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

float calcShadows(vec3 fragPosLight, vec2 offset, float bias) {
    float shadow = 1.0;
    if(texture(shadowMap, fragPosLight.xy + offset).r < fragPosLight.z - bias) {
        shadow = 0.0;
    }
    if (fragPosLight.z > 1.0) {
        shadow = 1.0;
    }
    return shadow;
}

float filterPCF(vec3 fragPosLight, float bias) {
    ivec2 texDim = textureSize(shadowMap, 0);
    float scale = 1.5;
    float dx = scale * 1.0 / float(texDim.x);
    float dy = scale * 1.0 / float(texDim.y);

    float shadowFactor = 0.0;
    int count = 0;
    int range = 1;

    for (int x = -range; x <= range; x++)
    {
        for (int y = -range; y <= range; y++)
        {
            shadowFactor += calcShadows(fragPosLight, vec2(dx*x, dy*y), bias);
            count++;
        }

    }
    return shadowFactor / count;
}

void main() {

    vec3 diffuseColour = texture(diffuseMap, fsIn.TexCoords).rgb;
    vec3 normalColour = texture(normalMap, fsIn.TexCoords).rgb;
    vec3 specularColour = texture(specularMap, fsIn.TexCoords).rgb;

    vec3 lightDir = normalize(-light.direction);

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

    float bias = max(0.01 * (1.0 - dot(fsIn.Normal, lightDir)), 0.00001);
//    float bias = 0.005*tan(acos(dot(normal, lightDir))); // cosTheta is dot( n,l ), clamped between 0 and 1
//    bias = clamp(bias, 0,0.01);
//    float shadow = calcShadows(fsIn.FragPosLightSpace, vec2(0), bias);
    float shadow = filterPCF(fsIn.WorldPosLightSpace.xyz / fsIn.WorldPosLightSpace.w, bias);

    vec3 colour = (diffuse + specular) * light.colour * (shadow);
    FragColour = vec4(colour, 1.0f);
}