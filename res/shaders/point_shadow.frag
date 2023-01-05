#version 450

layout (location = 0) in VsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 ViewPos;
    vec4 WorldPosLightSpace;
} fsIn;

layout (location = 0) out vec4 FragColour;

layout (set = 2, binding = 0) uniform sampler2D diffuseMap;
//layout (set = 2, binding = 0) uniform samplerCube diffuseMap;
layout (set = 2, binding = 1) uniform sampler2D normalMap;
layout (set = 2, binding = 2) uniform sampler2D specularMap;
layout (set = 2, binding = 3) uniform samplerCube shadowMap;

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

float calcShadows(vec3 fragPosLight, vec3 offset, float bias) {
    float shadow = 1.0;
    if(texture(shadowMap, fragPosLight + offset).r < length(fragPosLight) / 100.f - bias) {
        shadow = 0.0;
    }
    return shadow;
}

vec3 sampleOffsetDirections[20] = vec3[](
    vec3(1, 1, 1), vec3(1, -1, 1), vec3(-1, -1, 1), vec3(-1, 1, 1),
    vec3(1, 1, -1), vec3(1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
    vec3(1, 1, 0), vec3(1, -1, 0), vec3(-1, -1, 0), vec3(-1, 1, 0),
    vec3(1, 0, 1), vec3(-1, 0, 1), vec3(1, 0, -1), vec3(-1, 0, -1),
    vec3(0, 1, 1), vec3(0, -1, 1), vec3(0, -1, -1), vec3(0, 1, -1)
);

float filterPCF(vec3 fragPosLight, float bias) {
	float shadow = 0;
	int samples = 20;
	float viewDistance = length(light.position - fsIn.FragPos);
	float diskRadius = (1.0 + (viewDistance / 100.f)) / 25.0;
	
	for (int i = 0; i < samples; i++) {
		shadow += calcShadows(fragPosLight, sampleOffsetDirections[i] * diskRadius, bias);	
	}
	return shadow / samples;
}

void main() {

    vec3 diffuseColour = texture(diffuseMap, fsIn.TexCoords).rgb;
    vec3 normalColour = texture(normalMap, fsIn.TexCoords).rgb;
    vec3 specularColour = texture(specularMap, fsIn.TexCoords).rgb;

    vec3 lightDir = normalize(light.position - fsIn.FragPos);

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

    float distance = length(light.position - fsIn.FragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);

    float bias = max(0.01 * (1.0 - dot(normal, lightDir)), 0.00001);
//    float bias = 0.005*tan(acos(dot(normal, lightDir))); // cosTheta is dot( n,l ), clamped between 0 and 1
//    bias = clamp(bias, 0,0.01);
//    float shadow = calcShadows(fsIn.FragPos - light.position, vec3(0), bias);
//    float shadow = filterPCF(normalize(vec3(fsIn.FragPosLightSpace / fsIn.FragPosLightSpace.w) - light.position), bias);
//    float shadow = calcShadows(normalize(vec3(fsIn.FragPosLightSpace / fsIn.FragPosLightSpace.w) - light.position), vec3(0), bias);
    float shadow = filterPCF(fsIn.FragPos - light.position, bias);

    vec3 colour = (diffuse + specular) * light.colour * attenuation * shadow;
    FragColour = vec4(colour, 1.0f);
}
