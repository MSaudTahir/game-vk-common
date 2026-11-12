// Shared FXAA pass (NVIDIA's FXAA whitepaper version) for game post chains.  glslangValidator needs the include path of this directory (-I <this dir>); the
// game CMake files pass it automatically.
//
// `level`: 1 = light (2 px span), 2 = medium (4 px), 3 = full (8 px, closest to the original whitepaper
// constants).  Only pixels on a luma edge above the level's threshold are touched, along the edge
// direction.  The caller decides whether to call this at all (e.g. an r_fxaa / FSAA dvar) and which
// sampler/UV to use; textureLod 0 keeps the result independent of the sampler's mip state.

vec3 gvc_fxaa(sampler2D sceneTex, vec2 uv, int level) {
    vec2 rcp = 1.0 / vec2(textureSize(sceneTex, 0));
    vec3 rgbM = textureLod(sceneTex, uv, 0.0).rgb;
    vec3 rgbNW = textureLod(sceneTex, uv + vec2(-1.0, -1.0) * rcp, 0.0).rgb;
    vec3 rgbNE = textureLod(sceneTex, uv + vec2(1.0, -1.0) * rcp, 0.0).rgb;
    vec3 rgbSW = textureLod(sceneTex, uv + vec2(-1.0, 1.0) * rcp, 0.0).rgb;
    vec3 rgbSE = textureLod(sceneTex, uv + vec2(1.0, 1.0) * rcp, 0.0).rgb;
    const vec3 L = vec3(0.299, 0.587, 0.114);
    float lM = dot(rgbM, L), lNW = dot(rgbNW, L), lNE = dot(rgbNE, L), lSW = dot(rgbSW, L), lSE = dot(rgbSE, L);
    float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));
    float lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));
    float thresh = level == 1 ? 0.25 : (level == 2 ? 0.166 : 0.125);        // relative edge threshold
    float threshMin = level == 1 ? 0.0833 : (level == 2 ? 0.0625 : 0.0417); // absolute minimum (dark areas)
    if (lMax - lMin < max(threshMin, lMax * thresh)) return rgbM;
    float span = level == 1 ? 2.0 : (level == 2 ? 4.0 : 8.0);
    vec2 dir = vec2(-((lNW + lNE) - (lSW + lSE)), (lNW + lSW) - (lNE + lSE));
    float dirReduce = max((lNW + lNE + lSW + lSE) * 0.25 * (1.0 / 8.0), 1.0 / 128.0);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, vec2(-span), vec2(span)) * rcp;
    vec3 rgbA = 0.5 * (textureLod(sceneTex, uv + dir * (1.0 / 3.0 - 0.5), 0.0).rgb +
                       textureLod(sceneTex, uv + dir * (2.0 / 3.0 - 0.5), 0.0).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (textureLod(sceneTex, uv + dir * -0.5, 0.0).rgb +
                                     textureLod(sceneTex, uv + dir * 0.5, 0.0).rgb);
    float lB = dot(rgbB, L);
    return (lB < lMin || lB > lMax) ? rgbA : rgbB;
}
