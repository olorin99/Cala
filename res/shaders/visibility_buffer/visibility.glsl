#ifndef VISIBILITY_GLSL
#define VISIBILITY_GLSL

#define MESHLET_ID_BITS 26
#define PRIMITIVE_ID_BITS 6
#define MESHLET_MASK ((1u << MESHLET_ID_BITS) - 1u)
#define PRIMITIVE_MASK ((1u << PRIMITIVE_ID_BITS) - 1u)

uint setMeshletID(uint meshletID) {
    return meshletID << PRIMITIVE_ID_BITS;
}

uint setPrimitiveID(uint primitiveID) {
    return primitiveID & PRIMITIVE_MASK;
}

uint getMeshletID(uint visibility) {
    return (visibility >> PRIMITIVE_ID_BITS) & MESHLET_MASK;
}

uint getPrimitiveID(uint visibility) {
    return uint(visibility & PRIMITIVE_MASK);
}


struct BarycentricDeriv {
    vec3 lambda;
    vec3 ddx;
    vec3 ddy;
};

BarycentricDeriv calcDerivitives(vec4[3] clipSpace, vec2 pixelNdc, vec2 winSize)
{
    BarycentricDeriv result;

    vec3 invW = 1.0 / vec3(clipSpace[0].w, clipSpace[1].w, clipSpace[2].w);

    vec2 ndc0 = clipSpace[0].xy * invW[0];
    vec2 ndc1 = clipSpace[1].xy * invW[1];
    vec2 ndc2 = clipSpace[2].xy * invW[2];

    float invDet = 1.0 / determinant(mat2(ndc2 - ndc1, ndc0 - ndc1));

    result.ddx = vec3(ndc1.y - ndc2.y, ndc2.y - ndc0.y, ndc0.y - ndc1.y) * invDet * invW;
    result.ddy = vec3(ndc2.x - ndc1.x, ndc0.x - ndc2.x, ndc1.x - ndc0.x) * invDet * invW;

    float ddxSum = dot(result.ddx, vec3(1.0));
    float ddySum = dot(result.ddy, vec3(1.0));

    vec2 deltaVec = pixelNdc - ndc0;
    float interpInvW = invW.x + deltaVec.x * ddxSum + deltaVec.y * ddySum;
    float interpW = 1.0 / interpInvW;

    result.lambda.x = interpW * (deltaVec.x * result.ddx.x + deltaVec.y * result.ddy.x + invW[0]);
    result.lambda.y = interpW * (deltaVec.x * result.ddx.y + deltaVec.y * result.ddy.y);
    result.lambda.z = interpW * (deltaVec.x * result.ddx.z + deltaVec.y * result.ddy.z);

    result.ddx *= (2.0 / winSize.x);
    result.ddy *= (-2.0 / winSize.y);
    ddxSum *= (2.0 / winSize.x);
    ddySum *= (-2.0 / winSize.y);

    float interpDdxW = 1.0 / (interpInvW + ddxSum);
    float interpDdyW = 1.0 / (interpInvW + ddySum);

    result.ddx = interpDdxW * (result.lambda * interpInvW + result.ddx) - result.lambda;
    result.ddy = interpDdyW * (result.lambda * interpInvW + result.ddy) - result.lambda;
    return result;
}

vec3 analyticalDdx(BarycentricDeriv derivatives, vec3[3] values) {
    return vec3(
    dot(derivatives.ddx, vec3(values[0].x, values[1].x, values[2].x)),
    dot(derivatives.ddx, vec3(values[0].y, values[1].y, values[2].y)),
    dot(derivatives.ddx, vec3(values[0].z, values[1].z, values[2].z))
    );
}

vec3 analyticalDdy(BarycentricDeriv derivatives, vec3[3] values) {
    return vec3(
    dot(derivatives.ddy, vec3(values[0].x, values[1].x, values[2].x)),
    dot(derivatives.ddy, vec3(values[0].y, values[1].y, values[2].y)),
    dot(derivatives.ddy, vec3(values[0].z, values[1].z, values[2].z))
    );
}

vec3 interpolate(BarycentricDeriv derivatives, float[3] values)
{
    vec3 mergedV = vec3(values[0], values[1], values[2]);
    return vec3(
        dot(mergedV, derivatives.lambda),
        dot(mergedV, derivatives.ddx),
        dot(mergedV, derivatives.ddy)
    );
}

vec3 interpolateVec3(BarycentricDeriv derivatives, vec3[3] data) {
    return vec3(
        interpolate(derivatives, float[3](data[0].x, data[1].x, data[2].x)).x,
        interpolate(derivatives, float[3](data[0].y, data[1].y, data[2].y)).x,
        interpolate(derivatives, float[3](data[0].z, data[1].z, data[2].z)).x
    );
}

struct UVGradient {
    vec2 uv;
    vec2 ddx;
    vec2 ddy;
};

UVGradient calcUVDerivitives(BarycentricDeriv derivitives, vec2[3] uvs) {
    vec3[] interpUV = vec3[](
        interpolate(derivitives, float[](uvs[0].x, uvs[1].x, uvs[2].x)),
        interpolate(derivitives, float[](uvs[0].y, uvs[1].y, uvs[2].y))
    );
    UVGradient gradient;

    gradient.uv = vec2(interpUV[0].x, interpUV[1].x);
    gradient.ddx = vec2(interpUV[0].y, interpUV[1].y);
    gradient.ddy = vec2(interpUV[0].z, interpUV[1].z);

    return gradient;
}

struct InterpolatedValues {
    vec3 worldPosition;
    vec2 screenPosition;
    float depth;
    vec3 normal;
    UVGradient uvGrad;
    mat3 TBN;
};


#endif //VISIBILITY_GLSL