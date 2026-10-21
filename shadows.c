#include "shadows.h"
#include "rlgl.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Depth vertex: default MVP is auto-uploaded by rlgl at flush time.
static const char* kDepthVS =
"#version 330\n"
"layout(location = 0) in vec3 vertexPosition;\n"
"uniform mat4 mvp;\n"
"void main() { gl_Position = mvp*vec4(vertexPosition, 1.0); }\n";

// Store S = 1 - z01 packed across RGB with ALPHA = 1.0. The alpha MUST be
// 1.0: rlgl enables GL_BLEND globally, and packFloat's low-byte alpha would
// otherwise make every depth write alpha-blend with the background, silently
// destroying the packed value (map reads garbage -> everything "lit").
static const char* kDepthFS =
"#version 330\n"
"vec4 packFloat(float v) {\n"
"  vec4 enc = vec4(1.0, 255.0, 65025.0, 16581375.0)*v;\n"
"  enc = fract(enc);\n"
"  enc -= enc.yzww*vec4(1.0/255.0, 1.0/255.0, 1.0/255.0, 0.0);\n"
"  return enc;\n"
"}\n"
"void main() { gl_FragColor = vec4(packFloat(1.0 - gl_FragCoord.z).rgb, 1.0); }\n";

static const char* kLitVS =
"#version 330\n"
"layout(location = 0) in vec3 vertexPosition;\n"
"layout(location = 1) in vec2 vertexTexCoord;\n"
"layout(location = 2) in vec3 vertexNormal;\n"
"layout(location = 3) in vec4 vertexColor;\n"
"uniform mat4 mvp;\n"
"uniform mat4 lightVP;\n"
"uniform float normOffset;\n"   // world-space push along normal (anti-acne)
"out vec3 vNormal;\n"
"out vec4 vColor;\n"
"out vec4 vLightClip;\n"
"out vec3 vWorldPos;\n"
"void main() {\n"
"  vec3 N = normalize(vertexNormal);\n"
"  vNormal = N;\n"
"  vColor = vertexColor;\n"
"  vWorldPos = vertexPosition;\n"       // rlgl CPU-applies model transforms
"  vLightClip = lightVP*vec4(vertexPosition + N*normOffset, 1.0);\n"
"  gl_Position = mvp*vec4(vertexPosition, 1.0);\n"
"}\n";

static const char* kLitFS =
"#version 330\n"
"in vec3 vNormal;\n"
"in vec4 vColor;\n"
"in vec4 vLightClip;\n"
"in vec3 vWorldPos;\n"
"uniform vec3 sunDir;\n"        // world-space direction TO the sun
"uniform vec3 sunColor;\n"
"uniform vec3 ambientSky;\n"
"uniform vec3 ambientGround;\n"
"uniform vec3 camPos;\n"
"uniform sampler2D shadowMap;\n"
"uniform vec2 texelSize;\n"
"uniform float bias;\n"         // constant depth bias in packed-depth units
"uniform int softShadows;\n"    // 0 = 9-tap PCF, 1 = PCSS
"uniform float penK;\n"         // blocker-depth -> penumbra scale
"uniform float penMax;\n"       // max penumbra (texels)
"uniform float blkStride;\n"    // blocker search stride (texels)
"uniform float fogOn;\n"
"uniform vec3 fogColor;\n"
"uniform float fogStart;\n"
"uniform float fogRange;\n"
"uniform float fogMax;\n"
"uniform int lightCount;\n"
"uniform vec3 lightPos[8];\n"
"uniform vec3 lightColor[8];\n"
"uniform float lightRadius[8];\n"
"uniform sampler2D lampAtlas;\n"
"uniform int lampMap[8];\n"     // light idx -> baked lamp idx (-1 = none)
"uniform vec2 lampTiles;\n"     // atlas grid (cols, rows)
"uniform float lampNear;\n"
"uniform float lampFar;\n"
"uniform sampler2D lampDyn;\n"     // per-frame actor atlas (nearest 2 lamps)
"uniform int lampDynSlot[8];\n"   // light idx -> dynamic slot (0/1, -1 = none)
"uniform vec2 lampDynTiles;\n"
"uniform float specK;\n"
"uniform float sat;\n"
"uniform float contrast;\n"
"out vec4 fragColor;\n"
"float unpackFloat(vec4 c) {\n"
"  return dot(c, vec4(1.0, 1.0/255.0, 1.0/65025.0, 1.0/16581375.0));\n"
"}\n"
"float shadowTap(vec2 uv, float sCur) {\n"
"  float sMap = unpackFloat(texture(shadowMap, uv));\n"
"  return step(sMap - bias, sCur);\n"
"}\n"
"float pcf9(vec2 uv, float sCur) {\n"
"  float sum = 0.0;\n"
"  for (int y = -1; y <= 1; y++)\n"
"    for (int x = -1; x <= 1; x++)\n"
"      sum += shadowTap(uv + vec2(float(x), float(y))*texelSize, sCur);\n"
"  return sum/9.0;\n"
"}\n"
"float pcf25(vec2 uv, float sCur, float pen) {\n"
"  // per-pixel rotated 5x5 lattice: breaks banding, smooth penumbra\n"
"  float ang = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233)))*43758.5453)*6.2831853;\n"
"  vec2 cr = vec2(cos(ang), sin(ang));\n"
"  mat2 rot = mat2(cr.x, cr.y, -cr.y, cr.x);\n"
"  float sum = 0.0;\n"
"  for (int y = -2; y <= 2; y++)\n"
"    for (int x = -2; x <= 2; x++) {\n"
"      vec2 o = rot*vec2(float(x), float(y))*texelSize*pen;\n"
"      sum += shadowTap(uv + o, sCur);\n"
"    }\n"
"  return sum*0.04;\n"
"}\n"
"float pcss(vec2 uv, float sCur) {\n"
"  // cheap accept: if center + 4 cross taps are all lit, skip the search\n"
"  float c = shadowTap(uv, sCur);\n"
"  float q = shadowTap(uv + vec2(2.5, 0.0)*texelSize, sCur)\n"
"          + shadowTap(uv - vec2(2.5, 0.0)*texelSize, sCur)\n"
"          + shadowTap(uv + vec2(0.0, 2.5)*texelSize, sCur)\n"
"          + shadowTap(uv - vec2(0.0, 2.5)*texelSize, sCur);\n"
"  if (c + q >= 4.999) return 1.0;\n"
"  // blocker search: blockers sit closer to the light, which under our\n"
"  // packing means a LARGER stored s.\n"
"  float blk = 0.0, cnt = 0.0;\n"
"  for (int y = -1; y <= 1; y++)\n"
"    for (int x = -1; x <= 1; x++) {\n"
"      float sMap = unpackFloat(texture(shadowMap, uv + vec2(float(x), float(y))*texelSize*blkStride));\n"
"      if (sMap > sCur + bias) { blk += sMap - sCur; cnt += 1.0; }\n"
"    }\n"
"  if (cnt < 0.5) return 1.0;\n"
"  float pen = clamp((blk/cnt)*penK, 1.0, penMax);\n"   // contact-hardening
"  return pcf25(uv, sCur, pen);\n"
"}\n"
"float lampTapAt(sampler2D atlas, vec2 auv, float sCur, float b) {\n"
"  float sMap = unpackFloat(texture(atlas, auv));\n"
"  return step(sMap - b, sCur);\n"
"}\n"
"// Shared cube-face sampler: the baked static atlas and the per-frame actor\n"
"// atlas use identical projections, so one core serves both. tapOf is one\n"
"// texel step in tile-UV units (1.5/tileResolution).\n"
"float pointShadowCore(sampler2D atlas, vec2 tiles, float tileIndex, vec3 dir, float dist, float tapOf) {\n"
"  if (dist > lampFar - 0.4) return 1.0;\n"
"  float ax = abs(dir.x), ay = abs(dir.y), az = abs(dir.z);\n"
"  vec2 cuv; float face;\n"
"  if (ax >= ay && ax >= az) {\n"
"    face = dir.x > 0.0 ? 0.0 : 1.0;\n"
"    cuv = dir.x > 0.0 ? vec2(-dir.z, -dir.y)/ax : vec2(dir.z, -dir.y)/ax;\n"
"  } else if (ay >= az) {\n"
"    face = dir.y > 0.0 ? 2.0 : 3.0;\n"
"    cuv = dir.y > 0.0 ? vec2(dir.x, dir.z)/ay : vec2(dir.x, -dir.z)/ay;\n"
"  } else {\n"
"    face = dir.z > 0.0 ? 4.0 : 5.0;\n"
"    cuv = dir.z > 0.0 ? vec2(dir.x, -dir.y)/az : vec2(-dir.x, -dir.y)/az;\n"
"  }\n"
"  cuv = clamp(cuv*0.5 + 0.5, vec2(0.013), vec2(0.987));\n"
"  float tidx = tileIndex + face;\n"
"  vec2 tile = vec2(mod(tidx, tiles.x), floor(tidx/tiles.x));\n"
"  // The bake uses a 90-degree PERSPECTIVE per face, so the stored depth is\n"
"  // the distance along the face AXIS (major component), not the euclidean\n"
"  // distance. Using dist here would mark everything off-axis as occluded.\n"
"  float dmaj = max(ax, max(ay, az));\n"
"  float dn = max(dmaj, 0.2);\n"
"  float z01 = 0.5 + (lampFar + lampNear)/(2.0*(lampFar - lampNear))\n"
"            - (lampNear*lampFar)/((lampFar - lampNear)*dn);\n"
"  float sCur = 1.0 - z01;\n"
"  float b = 0.006/max(dist*dist, 0.30);\n"
"  vec2 o = vec2(tapOf, 0.0);\n"
"  float sum = lampTapAt(atlas, (tile + cuv + o)/tiles, sCur, b)\n"
"            + lampTapAt(atlas, (tile + cuv - o)/tiles, sCur, b)\n"
"            + lampTapAt(atlas, (tile + cuv + vec2(0.0, tapOf))/tiles, sCur, b)\n"
"            + lampTapAt(atlas, (tile + cuv - vec2(0.0, tapOf))/tiles, sCur, b);\n"
"  return sum*0.25;\n"
"}\n"
"float lampShadow(vec3 dir, float dist, int li) {\n"
"  return pointShadowCore(lampAtlas, lampTiles, float(li)*6.0, dir, dist, 1.5/256.0);\n"
"}\n"
"float dynShadow(vec3 dir, float dist, int ds) {\n"
"  return pointShadowCore(lampDyn, lampDynTiles, float(ds)*6.0, dir, dist, 1.5/128.0);\n"
"}\n"
"void main() {\n"
"  vec3 N = normalize(vNormal);\n"
"  vec3 ndc = vLightClip.xyz/max(vLightClip.w, 1e-5);\n"
"  vec2 uv = ndc.xy*0.5 + 0.5;\n"
"  float sCur = 1.0 - (ndc.z*0.5 + 0.5);\n"
"  float sh = 1.0;\n"
"  if (vLightClip.w > 0.0 && ndc.z <= 1.0 &&\n"
"      uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0) {\n"
"    sh = (softShadows == 1) ? pcss(uv, sCur) : pcf9(uv, sCur);\n"
"  }\n"
"  vec3 amb = mix(ambientGround, ambientSky, N.y*0.5 + 0.5);\n"
"  float ndl = max(dot(N, sunDir), 0.0);\n"
"  vec3 col = vColor.rgb*(amb + sunColor*ndl*sh);\n"
"  for (int i = 0; i < 8; i++) {\n"
"    if (i >= lightCount) break;\n"
"    vec3 dv = lightPos[i] - vWorldPos;\n"
"    float d2 = dot(dv, dv);\n"
"    float r = lightRadius[i];\n"
"    float att = clamp(1.0 - d2/(r*r), 0.0, 1.0);\n"
"    att *= att;\n"
"    if (att > 0.0025) {\n"
"      vec3 L = dv*inversesqrt(max(d2, 1e-4));\n"
"      float lsh = 1.0;\n"
"      int li = lampMap[i];\n"
"      if (li >= 0) lsh = lampShadow(-dv, sqrt(d2), li);\n"
"      int ds = lampDynSlot[i];\n"
"      if (ds >= 0) lsh = min(lsh, dynShadow(-dv, sqrt(d2), ds));\n"
"      col += vColor.rgb*lightColor[i]*att*(0.30 + 0.70*max(dot(N, L), 0.0))*lsh;\n"
"    }\n"
"  }\n"
"  {\n"
"    vec3 V = normalize(camPos - vWorldPos);\n"
"    vec3 H = normalize(V + sunDir);\n"
"    float sp = pow(max(dot(N, H), 0.0), 40.0)*specK*sh*step(0.001, ndl);\n"
"    col += sunColor*sp;\n"
"  }\n"
"  float dist = length(camPos - vWorldPos);\n"
"  float ff = clamp((dist - fogStart)/fogRange, 0.0, 1.0)*fogMax*fogOn;\n"
"  col = mix(col, fogColor, ff);\n"
"  {\n"
"    float l = dot(col, vec3(0.299, 0.587, 0.114));\n"
"    col = mix(vec3(l), col, sat);\n"
"    col = (col - 0.5)*contrast + 0.5;\n"
"    col = clamp(col, 0.0, 1.0);\n"
"  }\n"
"  fragColor = vec4(col, vColor.a);\n"
"}\n";

static void FillLocs(int* dst, unsigned int prog,
                      const char* mvpName, const char* viewName,
                      const char* projName, const char* modelName,
                      const char* normalName) {
    // CRITICAL: start with everything -1 (glUniform* on location -1 is a
    // guaranteed silent no-op). DO NOT copy the default program's locations
    // here: rlDrawRenderBatch force-uploads glUniform4f(COLOR_DIFFUSE) and
    // glUniform1i(MAP_DIFFUSE) UNGUARDED on every flush, and a copied-in
    // default location is meaningless in this program - it lands on an
    // unrelated uniform (they sit inside our lightVP matrix / samplers)
    // and silently corrupts it mid-frame. Only the batch attributes and the
    // MVP uniform must stay live; the guarded matrix slots stay -1.
    for (int i = 0; i < 32; i++) dst[i] = -1;
    dst[RL_SHADER_LOC_VERTEX_POSITION] = 0;
    dst[RL_SHADER_LOC_VERTEX_TEXCOORD01] = 1;
    dst[RL_SHADER_LOC_VERTEX_NORMAL] = 2;
    dst[RL_SHADER_LOC_VERTEX_COLOR] = 3;
    dst[RL_SHADER_LOC_MATRIX_MVP] = rlGetLocationUniform(prog, mvpName);
    dst[RL_SHADER_LOC_MATRIX_VIEW] = viewName ? rlGetLocationUniform(prog, viewName) : -1;
    dst[RL_SHADER_LOC_MATRIX_PROJECTION] = projName ? rlGetLocationUniform(prog, projName) : -1;
    dst[RL_SHADER_LOC_MATRIX_MODEL] = modelName ? rlGetLocationUniform(prog, modelName) : -1;
    dst[RL_SHADER_LOC_MATRIX_NORMAL] = normalName ? rlGetLocationUniform(prog, normalName) : -1;
}

// Build a texel-snapped light view for the current focus point.
static void BuildLightMatrices(ShadowMap* s, Vector3 focus) {
    Vector3 z = s->sunDir;
    Vector3 upHint = (fabsf(z.y) > 0.95f) ? (Vector3){ 0, 0, 1 } : (Vector3){ 0, 1, 0 };
    Vector3 x = Vector3Normalize(Vector3CrossProduct(upHint, z));
    Vector3 y = Vector3CrossProduct(z, x);

    // snap the focus to the shadow texel grid along the light's x/y so
    // moving receivers do not crawl across texel boundaries (edge shimmer)
    float texel = (2.0f*s->halfSize)/(float)s->mapSize;
    float lx = Vector3DotProduct(x, focus);
    float ly = Vector3DotProduct(y, focus);
    lx = floorf(lx/texel)*texel + texel*0.5f;
    ly = floorf(ly/texel)*texel + texel*0.5f;

    float D = s->halfSize*2.0f + 60.0f;      // eye distance along the sun
    float lz = Vector3DotProduct(z, focus);
    Vector3 eye = Vector3Add(Vector3Add(Vector3Scale(x, lx), Vector3Scale(y, ly)),
                             Vector3Scale(z, lz + D));

    Matrix view = { 0 };
    view.m0 = x.x; view.m1 = y.x; view.m2 = z.x; view.m3 = 0;
    view.m4 = x.y; view.m5 = y.y; view.m6 = z.y; view.m7 = 0;
    view.m8 = x.z; view.m9 = y.z; view.m10 = z.z; view.m11 = 0;
    view.m12 = -Vector3DotProduct(x, eye);
    view.m13 = -Vector3DotProduct(y, eye);
    view.m14 = -Vector3DotProduct(z, eye);
    view.m15 = 1.0f;

    float h = s->halfSize;
    float margin = h*2.0f + 20.0f;
    s->lightView = view;
    s->lightProj = MatrixOrtho(-h, h, -h, h, D - margin, D + margin);
    // rlgl convention: MVP = modelview * projection
    s->lightVP = MatrixMultiply(s->lightView, s->lightProj);
}

static void SetQualityUniforms(ShadowMap* s) {
    rlDrawRenderBatchActive();
    rlEnableShader(s->litProg);
    float texel[2] = { 1.0f/(float)s->mapSize, 1.0f/(float)s->mapSize };
    int soft = (s->quality >= SHADOW_QUAL_HIGH) ? 1 : 0;
    float penK = 260.0f, penMax = 6.0f, blkStride = 2.0f;
    if (s->quality == SHADOW_QUAL_ULTRA) { penK = 480.0f; penMax = 9.0f; blkStride = 3.0f; }
    if (s->locTexel >= 0) rlSetUniform(s->locTexel, texel, RL_SHADER_UNIFORM_VEC2, 1);
    if (s->locSoft >= 0) rlSetUniform(s->locSoft, &soft, RL_SHADER_UNIFORM_INT, 1);
    if (s->locPenK >= 0) rlSetUniform(s->locPenK, &penK, RL_SHADER_UNIFORM_FLOAT, 1);
    if (s->locPenMax >= 0) rlSetUniform(s->locPenMax, &penMax, RL_SHADER_UNIFORM_FLOAT, 1);
    if (s->locBlkStride >= 0) rlSetUniform(s->locBlkStride, &blkStride, RL_SHADER_UNIFORM_FLOAT, 1);
    rlDisableShader();
    rlSetShader(rlGetShaderIdDefault(), rlGetShaderLocsDefault());
}

void Shadow_Init(ShadowMap* s) {
    memset(s, 0, sizeof(*s));
    s->enabled = true;
    s->fogEnabled = true;
    s->fogColor = (Vector3){ 0.765f, 0.843f, 0.910f };
    s->fogStart = 42.0f;
    s->fogRange = 65.0f;
    s->fogMax = 0.78f;
    s->quality = SHADOW_QUAL_ULTRA;
    s->halfSize = 46.0f;
    s->focusDist = 16.0f;
    s->sunDir = Vector3Normalize((Vector3){ 90.0f, 70.0f, -90.0f });

    s->depthProg = rlLoadShaderProgram(kDepthVS, kDepthFS);
    s->litProg = rlLoadShaderProgram(kLitVS, kLitFS);
    if (s->depthProg == 0 || s->litProg == 0) {
        TraceLog(LOG_WARNING, "Shadow programs failed to compile; shadows disabled");
        s->available = false;
        s->enabled = false;
        return;
    }
    s->available = true;

    FillLocs(s->depthLocs, s->depthProg, "mvp", NULL, NULL, NULL, NULL);
    FillLocs(s->litLocs, s->litProg, "mvp", NULL, NULL, NULL, NULL);

    s->locLightVP = rlGetLocationUniform(s->litProg, "lightVP");
    s->locSunDir = rlGetLocationUniform(s->litProg, "sunDir");
    s->locSunColor = rlGetLocationUniform(s->litProg, "sunColor");
    s->locAmbSky = rlGetLocationUniform(s->litProg, "ambientSky");
    s->locAmbGnd = rlGetLocationUniform(s->litProg, "ambientGround");
    s->locCamPos = rlGetLocationUniform(s->litProg, "camPos");
    s->locShadowMap = rlGetLocationUniform(s->litProg, "shadowMap");
    s->locTexel = rlGetLocationUniform(s->litProg, "texelSize");
    s->locBias = rlGetLocationUniform(s->litProg, "bias");
    s->locNormOff = rlGetLocationUniform(s->litProg, "normOffset");
    s->locSoft = rlGetLocationUniform(s->litProg, "softShadows");
    s->locFogOn = rlGetLocationUniform(s->litProg, "fogOn");
    s->locFogColor = rlGetLocationUniform(s->litProg, "fogColor");
    s->locFogStart = rlGetLocationUniform(s->litProg, "fogStart");
    s->locFogRange = rlGetLocationUniform(s->litProg, "fogRange");
    s->locFogMax = rlGetLocationUniform(s->litProg, "fogMax");
    s->locLightCount = rlGetLocationUniform(s->litProg, "lightCount");
    s->locLightPos = rlGetLocationUniform(s->litProg, "lightPos");     // resolves [0]
    s->locLightColor = rlGetLocationUniform(s->litProg, "lightColor");
    s->locLightRadius = rlGetLocationUniform(s->litProg, "lightRadius");
    s->locSpecK = rlGetLocationUniform(s->litProg, "specK");
    s->locSat = rlGetLocationUniform(s->litProg, "sat");
    s->locCon = rlGetLocationUniform(s->litProg, "contrast");
    s->locLampAtlas = rlGetLocationUniform(s->litProg, "lampAtlas");
    s->locLampMap = rlGetLocationUniform(s->litProg, "lampMap");     // resolves [0]
    s->locLampTiles = rlGetLocationUniform(s->litProg, "lampTiles");
    s->locLampNear = rlGetLocationUniform(s->litProg, "lampNear");
    s->locLampFar = rlGetLocationUniform(s->litProg, "lampFar");
    s->locPenK = rlGetLocationUniform(s->litProg, "penK");
    s->locPenMax = rlGetLocationUniform(s->litProg, "penMax");
    s->locBlkStride = rlGetLocationUniform(s->litProg, "blkStride");
    s->locLampDynTex = rlGetLocationUniform(s->litProg, "lampDyn");
    s->locLampDynTiles = rlGetLocationUniform(s->litProg, "lampDynTiles");
    s->locLampDynSlot = rlGetLocationUniform(s->litProg, "lampDynSlot"); // [0]

    // per-frame actor atlas: SHADOW_DYN_LAMPS lamps x 6 faces, 3 tiles/row
    s->lampDynRT = LoadRenderTexture(SHADOW_DYN_TILE*3, SHADOW_DYN_TILE*2*SHADOW_DYN_LAMPS);
    if (s->lampDynRT.id) {
        SetTextureFilter(s->lampDynRT.texture, TEXTURE_FILTER_POINT);
        rlTextureParameters(s->lampDynRT.texture.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP);
        rlTextureParameters(s->lampDynRT.texture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
    }
    s->lampDynLamps[0] = s->lampDynLamps[1] = -1;

    // static uniforms (bound once; program must be active for upload)
    rlDrawRenderBatchActive();
    rlEnableShader(s->litProg);
    Vector3 sun = s->sunDir;
    Vector3 sunCol = { 0.98f, 0.93f, 0.84f };
    Vector3 ambSky = { 0.52f, 0.58f, 0.70f };
    Vector3 ambGnd = { 0.40f, 0.375f, 0.345f };
    Vector3 fogCol = s->fogColor;
    float bias = 0.0005f;
    float normOff = 0.06f;
    float fogStart = s->fogStart, fogRange = s->fogRange, fogMax = s->fogMax;
    float specK = 0.10f, sat = 1.05f, con = 1.03f;
    if (s->locSunDir >= 0) rlSetUniform(s->locSunDir, &sun, RL_SHADER_UNIFORM_VEC3, 1);
    if (s->locSunColor >= 0) rlSetUniform(s->locSunColor, &sunCol, RL_SHADER_UNIFORM_VEC3, 1);
    if (s->locAmbSky >= 0) rlSetUniform(s->locAmbSky, &ambSky, RL_SHADER_UNIFORM_VEC3, 1);
    if (s->locAmbGnd >= 0) rlSetUniform(s->locAmbGnd, &ambGnd, RL_SHADER_UNIFORM_VEC3, 1);
    if (s->locFogColor >= 0) rlSetUniform(s->locFogColor, &fogCol, RL_SHADER_UNIFORM_VEC3, 1);
    if (s->locBias >= 0) rlSetUniform(s->locBias, &bias, RL_SHADER_UNIFORM_FLOAT, 1);
    if (s->locNormOff >= 0) rlSetUniform(s->locNormOff, &normOff, RL_SHADER_UNIFORM_FLOAT, 1);
    if (s->locFogStart >= 0) rlSetUniform(s->locFogStart, &fogStart, RL_SHADER_UNIFORM_FLOAT, 1);
    if (s->locFogRange >= 0) rlSetUniform(s->locFogRange, &fogRange, RL_SHADER_UNIFORM_FLOAT, 1);
    if (s->locFogMax >= 0) rlSetUniform(s->locFogMax, &fogMax, RL_SHADER_UNIFORM_FLOAT, 1);
    if (s->locSpecK >= 0) rlSetUniform(s->locSpecK, &specK, RL_SHADER_UNIFORM_FLOAT, 1);
    if (s->locSat >= 0) rlSetUniform(s->locSat, &sat, RL_SHADER_UNIFORM_FLOAT, 1);
    if (s->locCon >= 0) rlSetUniform(s->locCon, &con, RL_SHADER_UNIFORM_FLOAT, 1);
    rlDisableShader();
    rlSetShader(rlGetShaderIdDefault(), rlGetShaderLocsDefault());

    if (s->locLightVP < 0 || s->locShadowMap < 0) {
        TraceLog(LOG_WARNING, "Shadow uniforms missing; shadows disabled");
        s->available = false;
        s->enabled = false;
        return;
    }

    Shadow_SetQuality(s, SHADOW_QUAL_ULTRA);
    BuildLightMatrices(s, (Vector3){ 0, 1, 0 });
}

// Bake static lamp shadows into a 3-wide x (2*lamp) atlas of 90-degree
// perspective faces (256px tiles). Static world geometry only, so this runs
// ONCE: the samplers see tiny lights casting real shadows at zero per-frame
// caster cost. The depth program packs 1 - gl_FragCoord.z exactly like the
// sun map, and the shader reconstructs the same value from the distance.
void Shadow_BakeLamps(ShadowMap* s, const Vector3* lampPos, int count,
                      ShadowCasterFn drawCasters, void* user) {
    if (!s->available || !drawCasters) return;
    if (count > SHADOW_MAX_BAKED_LAMPS) count = SHADOW_MAX_BAKED_LAMPS;
    if (count <= 0) return;

    const int TILE = 256;
    int W = TILE*3;
    int H = TILE*2*count;
    if (s->lampRT.id) UnloadRenderTexture(s->lampRT);
    s->lampRT = LoadRenderTexture(W, H);
    if (s->lampRT.id == 0) {
        TraceLog(LOG_WARNING, "Lamp shadow atlas allocation failed; lamp shadows off");
        s->lampShadows = false;
        return;
    }
    SetTextureFilter(s->lampRT.texture, TEXTURE_FILTER_POINT);
    rlTextureParameters(s->lampRT.texture.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP);
    rlTextureParameters(s->lampRT.texture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);

    s->lampCount = count;
    for (int i = 0; i < count; i++) s->lampPos[i] = lampPos[i];
    s->lampNear = 0.18f;
    s->lampFar = 24.0f;

    Matrix savedProj = rlGetMatrixProjection();
    Matrix savedView = rlGetMatrixModelview();

    rlDrawRenderBatchActive();
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlSetShader(s->depthProg, s->depthLocs);
    BeginTextureMode(s->lampRT);
    ClearBackground(BLACK);

    static const Vector3 fdirs[6] = {
        { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }
    };
    static const Vector3 fups[6] = {
        { 0, -1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 0, -1, 0 }, { 0, -1, 0 }
    };
    Matrix proj = MatrixPerspective(90.0*DEG2RAD, 1.0f, s->lampNear, s->lampFar);
    for (int L = 0; L < count; L++) {
        for (int face = 0; face < 6; face++) {
            int tile = L*6 + face;
            rlViewport((tile % 3)*TILE, (tile / 3)*TILE, TILE, TILE);
            ClearBackground(BLACK);
            Vector3 eye = s->lampPos[L];
            Vector3 tgt = Vector3Add(eye, fdirs[face]);
            Matrix view = MatrixLookAt(eye, tgt, fups[face]);
            rlSetMatrixProjection(proj);
            rlSetMatrixModelview(view);
            drawCasters(user);
        }
    }

    rlSetShader(rlGetShaderIdDefault(), rlGetShaderLocsDefault());
    EndTextureMode();
    rlSetMatrixProjection(savedProj);
    rlSetMatrixModelview(savedView);
    s->lampShadows = true;
    TraceLog(LOG_INFO, "Lamp shadows baked: %d lamps, atlas %dx%d", count, W, H);
}

int Shadow_LampDynSlot(const ShadowMap* s, int lampIndex) {
    if (!s->lampDynReady || lampIndex < 0) return -1;
    for (int i = 0; i < SHADOW_DYN_LAMPS; i++)
        if (s->lampDynLamps[i] == lampIndex) return i;
    return -1;
}

void Shadow_UpdateLampDyn(ShadowMap* s, Vector3 camPos, const Vector3* lampPos,
                          int lampCount, ShadowCasterFn drawActors, void* user) {
    s->lampDynReady = false;
    s->lampDynLamps[0] = s->lampDynLamps[1] = -1;
    if (!s->available || !s->enabled || !s->lampShadows || s->lampDynRT.id == 0) return;

    // pick the nearest SHADOW_DYN_LAMPS lamps inside SHADOW_DYN_RANGE:
    // beyond it the actor's shadow would be sub-pixel in the pool anyway
    float bd[SHADOW_DYN_LAMPS];
    for (int i = 0; i < SHADOW_DYN_LAMPS; i++) bd[i] = 1e30f;
    int n = lampCount < SHADOW_MAX_BAKED_LAMPS ? lampCount : SHADOW_MAX_BAKED_LAMPS;
    for (int i = 0; i < n; i++) {
        float dx = lampPos[i].x - camPos.x, dz = lampPos[i].z - camPos.z;
        float d2 = dx*dx + dz*dz;
        if (d2 > SHADOW_DYN_RANGE*SHADOW_DYN_RANGE) continue;
        for (int k = 0; k < SHADOW_DYN_LAMPS; k++) {
            if (d2 < bd[k]) {
                for (int m = SHADOW_DYN_LAMPS - 1; m > k; m--) {
                    bd[m] = bd[m - 1];
                    s->lampDynLamps[m] = s->lampDynLamps[m - 1];
                }
                bd[k] = d2;
                s->lampDynLamps[k] = i;
                break;
            }
        }
    }
    if (s->lampDynLamps[0] < 0) return;

    int T = SHADOW_DYN_TILE;
    Matrix savedProj = rlGetMatrixProjection();
    Matrix savedView = rlGetMatrixModelview();

    rlDrawRenderBatchActive();
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlSetShader(s->depthProg, s->depthLocs);
    BeginTextureMode(s->lampDynRT);
    ClearBackground(BLACK); // empty depth = lit (same convention as the bake)

    static const Vector3 fdirs[6] = {
        { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }
    };
    static const Vector3 fups[6] = {
        { 0, -1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 0, -1, 0 }, { 0, -1, 0 }
    };
    Matrix proj = MatrixPerspective(90.0*DEG2RAD, 1.0f, s->lampNear, s->lampFar);
    for (int slot = 0; slot < SHADOW_DYN_LAMPS; slot++) {
        if (s->lampDynLamps[slot] < 0) continue;
        Vector3 eye = lampPos[s->lampDynLamps[slot]];
        for (int face = 0; face < 6; face++) {
            int tile = slot*6 + face;
            rlViewport((tile % 3)*T, (tile / 3)*T, T, T);
            ClearBackground(BLACK);
            Matrix view = MatrixLookAt(eye, Vector3Add(eye, fdirs[face]), fups[face]);
            rlSetMatrixProjection(proj);
            rlSetMatrixModelview(view);
            drawActors(user);
        }
    }

    rlSetShader(rlGetShaderIdDefault(), rlGetShaderLocsDefault());
    EndTextureMode();
    rlSetMatrixProjection(savedProj);
    rlSetMatrixModelview(savedView);
    s->lampDynReady = true;
}

void Shadow_SetFog(ShadowMap* s, float start, float range, float max) {
    if (!s) return;
    if (start < 1.0f) start = 1.0f;
    if (range < 1.0f) range = 1.0f;
    if (max < 0.0f) max = 0.0f;
    if (max > 1.0f) max = 1.0f;
    s->fogStart = start;
    s->fogRange = range;
    s->fogMax = max;
}

void Shadow_Unload(ShadowMap* s) {
    if (s->depthProg) { rlUnloadShaderProgram(s->depthProg); s->depthProg = 0; }
    if (s->litProg) { rlUnloadShaderProgram(s->litProg); s->litProg = 0; }
    if (s->rt.id) UnloadRenderTexture(s->rt);
    if (s->lampRT.id) UnloadRenderTexture(s->lampRT);
    if (s->lampDynRT.id) UnloadRenderTexture(s->lampDynRT);
    s->available = false;
    s->enabled = false;
}

void Shadow_SetQuality(ShadowMap* s, int quality) {
    if (!s->available) return;
    if (quality < 0) quality = 0;
    if (quality > 3) quality = 3;
    s->quality = quality;
    s->enabled = (quality > 0);
    if (quality == 0) return;

    int size = 1024;
    if (quality == SHADOW_QUAL_HIGH) size = 2048;
    else if (quality == SHADOW_QUAL_ULTRA) size = 4096;

    if (size != s->mapSize) {
        if (s->rt.id) UnloadRenderTexture(s->rt);
        s->rt = LoadRenderTexture(size, size);
        if (s->rt.id == 0 && size > 1024) { // fall back on allocation failure
            s->rt = LoadRenderTexture(1024, 1024);
            size = 1024;
        }
        if (s->rt.id == 0) {
            TraceLog(LOG_WARNING, "Shadow map allocation failed; shadows disabled");
            s->available = false;
            s->enabled = false;
            return;
        }
        SetTextureFilter(s->rt.texture, TEXTURE_FILTER_POINT);
        rlTextureParameters(s->rt.texture.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP);
        rlTextureParameters(s->rt.texture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
        s->mapSize = size;
    }
    SetQualityUniforms(s);
}

void Shadow_UpdateLight(ShadowMap* s, Vector3 camPos, Vector3 camForward) {
    if (!s->available) return;
    Vector3 f = { camForward.x, 0.0f, camForward.z };
    if (Vector3Length(f) < 0.001f) f = (Vector3){ 0, 0, -1 };
    f = Vector3Normalize(f);
    Vector3 focus = {
        ClampF(camPos.x + f.x*s->focusDist, -38.0f, 38.0f),
        1.0f,
        ClampF(camPos.z + f.z*s->focusDist, -38.0f, 38.0f)
    };
    BuildLightMatrices(s, focus);
}

void Shadow_BeginCasterPass(ShadowMap* s) {
    if (!s->available || !s->enabled) return;
    // Save current matrices; they are restored verbatim at End (no stack).
    s->savedProj = rlGetMatrixProjection();
    s->savedView = rlGetMatrixModelview();
    // Hermetic depth state for the map: never inherit test/mask leftovers
    // from the previous frame's transparent draws (rlgl sets GL_LEQUAL in
    // rlglInit and nothing here ever changes the depth function).
    rlDrawRenderBatchActive();
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlSetShader(s->depthProg, s->depthLocs); // flushes pending draws first
    BeginTextureMode(s->rt);
    ClearBackground(BLACK); // = far depth under our packing convention
    // Light POV set directly: no push/pop, so rlgl's transform flag can
    // never get stuck on (a stuck flag bakes a stale matrix into every
    // later vertex and blanks the world+HUD - see git history).
    rlSetMatrixProjection(s->lightProj);
    rlSetMatrixModelview(s->lightView);
}

void Shadow_EndCasterPass(ShadowMap* s) {
    if (!s->available || !s->enabled) return;
    rlSetShader(rlGetShaderIdDefault(), rlGetShaderLocsDefault());
    EndTextureMode();
    rlSetMatrixProjection(s->savedProj);
    rlSetMatrixModelview(s->savedView);
}

void Shadow_ApplyLit(ShadowMap* s, Vector3 camPos, const PointLight* lights, int lightCount) {
    if (!s->available || !s->enabled) return;
    rlSetShader(s->litProg, s->litLocs); // flushes pending default draws
    rlEnableShader(s->litProg);
    if (s->locLightVP >= 0) rlSetUniformMatrix(s->locLightVP, s->lightVP);
    if (s->locShadowMap >= 0) {
        // rlSetUniformSampler registers the texture in rlgl's batch state
        // (activeTextureId[]) so every flush rebinds it to the sampler unit
        // rlgl picked before drawing. A raw rlActiveTextureSlot +
        // rlEnableTexture bind does NOT survive the batch pipeline.
        rlSetUniformSampler(s->locShadowMap, s->rt.texture.id);
    }
    float c[3] = { camPos.x, camPos.y, camPos.z };
    if (s->locCamPos >= 0) rlSetUniform(s->locCamPos, c, RL_SHADER_UNIFORM_VEC3, 1);
    float fogOn = s->fogEnabled ? 1.0f : 0.0f;
    if (s->locFogOn >= 0) rlSetUniform(s->locFogOn, &fogOn, RL_SHADER_UNIFORM_FLOAT, 1);
    if (s->locFogColor >= 0) rlSetUniform(s->locFogColor, &s->fogColor, RL_SHADER_UNIFORM_VEC3, 1);
    if (s->locFogStart >= 0) rlSetUniform(s->locFogStart, &s->fogStart, RL_SHADER_UNIFORM_FLOAT, 1);
    if (s->locFogRange >= 0) rlSetUniform(s->locFogRange, &s->fogRange, RL_SHADER_UNIFORM_FLOAT, 1);
    if (s->locFogMax >= 0) rlSetUniform(s->locFogMax, &s->fogMax, RL_SHADER_UNIFORM_FLOAT, 1);

    int n = lightCount;
    if (n < 0) n = 0;
    if (n > SHADOW_MAX_LIGHTS) n = SHADOW_MAX_LIGHTS;
    if (s->locLightCount >= 0) rlSetUniform(s->locLightCount, &n, RL_SHADER_UNIFORM_INT, 1);
    if (n > 0) {
        float pos[SHADOW_MAX_LIGHTS*3], col[SHADOW_MAX_LIGHTS*3], rad[SHADOW_MAX_LIGHTS];
        for (int i = 0; i < n; i++) {
            pos[i*3+0] = lights[i].pos.x; pos[i*3+1] = lights[i].pos.y; pos[i*3+2] = lights[i].pos.z;
            col[i*3+0] = lights[i].color.x; col[i*3+1] = lights[i].color.y; col[i*3+2] = lights[i].color.z;
            rad[i] = lights[i].radius;
        }
        if (s->locLightPos >= 0) rlSetUniform(s->locLightPos, pos, RL_SHADER_UNIFORM_VEC3, n);
        if (s->locLightColor >= 0) rlSetUniform(s->locLightColor, col, RL_SHADER_UNIFORM_VEC3, n);
        if (s->locLightRadius >= 0) rlSetUniform(s->locLightRadius, rad, RL_SHADER_UNIFORM_FLOAT, n);
    }

    // baked lamp shadows: bind atlas + per-light lamp mapping. Always upload
    // the map (all -1 when unavailable) so stale uniform values can never
    // make a light sample the atlas by accident.
    if (s->locLampMap >= 0) {
        int map[SHADOW_MAX_LIGHTS];
        for (int i = 0; i < SHADOW_MAX_LIGHTS; i++)
            map[i] = (s->lampShadows && i < n) ? lights[i].lampIndex : -1;
        rlSetUniform(s->locLampMap, map, RL_SHADER_UNIFORM_INT, SHADOW_MAX_LIGHTS);
    }
    if (s->lampShadows && s->locLampAtlas >= 0) {
        rlSetUniformSampler(s->locLampAtlas, s->lampRT.texture.id);
        float tiles[2] = { 3.0f, (float)(s->lampCount*2) };
        if (s->locLampTiles >= 0) rlSetUniform(s->locLampTiles, tiles, RL_SHADER_UNIFORM_VEC2, 1);
        if (s->locLampNear >= 0) rlSetUniform(s->locLampNear, &s->lampNear, RL_SHADER_UNIFORM_FLOAT, 1);
        if (s->locLampFar >= 0) rlSetUniform(s->locLampFar, &s->lampFar, RL_SHADER_UNIFORM_FLOAT, 1);
    }
    // per-frame actor shadows for the two nearest lamps: slot map + atlas
    if (s->locLampDynSlot >= 0) {
        int slots[SHADOW_MAX_LIGHTS];
        for (int i = 0; i < SHADOW_MAX_LIGHTS; i++)
            slots[i] = (s->lampDynReady && i < n) ? lights[i].dynSlot : -1;
        rlSetUniform(s->locLampDynSlot, slots, RL_SHADER_UNIFORM_INT, SHADOW_MAX_LIGHTS);
    }
    if (s->lampDynReady && s->lampDynRT.id && s->locLampDynTex >= 0) {
        rlSetUniformSampler(s->locLampDynTex, s->lampDynRT.texture.id);
        float dtiles[2] = { 3.0f, (float)(SHADOW_DYN_LAMPS*2) };
        if (s->locLampDynTiles >= 0) rlSetUniform(s->locLampDynTiles, dtiles, RL_SHADER_UNIFORM_VEC2, 1);
    }
}

void Shadow_Release(ShadowMap* s) {
    if (!s->available || !s->enabled) return;
    // flush lit draws under the default shader; the sampler registration
    // expires with the next flush, and all later texture binds (viewmodel
    // RT, font glyphs) use unit 0 as usual.
    rlActiveTextureSlot(0);
    rlSetShader(rlGetShaderIdDefault(), rlGetShaderLocsDefault());
}
