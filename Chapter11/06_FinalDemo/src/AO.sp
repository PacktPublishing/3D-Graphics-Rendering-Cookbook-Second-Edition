//
// Ray-traced ambient occlusion with world-space spatial hashing.
// Ported from LightweightVK sample RTX_002_AO.cpp.
// Gautron 2020: "Real-Time Ray-Traced Ambient Occlusion of Complex Scenes using Spatial Hashing"
//
// Requires (auto-injected by LVK for the fragment stage):
//   GL_EXT_ray_query, GL_EXT_shader_explicit_arithmetic_types_int64, GL_EXT_shader_atomic_int64
// The 'kTLAS[' token below triggers LVK to inject the bindless acceleration-structure array.
//
// This file must be included AFTER common.sp (it uses the push constant 'pc').

// 64-bit slot: [63:56] frameLow | [55:32] checksum (0 = empty) | [31:16] hits | [15:0] samples
layout(std430, buffer_reference) coherent buffer HashSlot { uint64_t v[]; };

void computeTBN(in vec3 n, out vec3 x, out vec3 y) {
  float yz = -n.y * n.z;
  y = normalize(((abs(n.z) > 0.9999) ? vec3(-n.x * n.y, 1.0 - n.y * n.y, yz) : vec3(-n.x * n.z, yz, 1.0 - n.z * n.z)));
  x = cross(y, n);
}

float traceAO(rayQueryEXT rq, vec3 origin, vec3 dir) {
  rayQueryInitializeEXT(rq, kTLAS[pc.light.tlas], gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, origin, 0.0f, dir, pc.light.aoRadius);
  while (rayQueryProceedEXT(rq)) {}
  return (rayQueryGetIntersectionTypeEXT(rq, true) != gl_RayQueryCommittedIntersectionNoneEXT) ? 1.0 : 0.0;
}

// Numerical Recipes LCG
uint lcg(inout uint prev) {
  uint LCG_A = 1664525u;
  uint LCG_C = 1013904223u;
  prev = (LCG_A * prev + LCG_C);
  return prev & 0x00FFFFFF;
}

float rnd(inout uint seed) {
  return (float(lcg(seed)) / float(0x01000000));
}

// Tiny Encryption Algorithm
uint tea(uint val0, uint val1) {
  uint v0 = val0;
  uint v1 = val1;
  uint s0 = 0;
  for (uint n = 0; n < 16; n++) {
    s0 += 0x9e3779b9;
    v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
    v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
  }
  return v0;
}

vec3 sampleCosineHemisphere(inout uint seed, vec3 tangent, vec3 bitangent, vec3 n) {
  float r1 = rnd(seed);
  float r2 = rnd(seed);
  float sq = sqrt(1.0 - r2);
  float phi = 2.0 * 3.141592653589 * r1;
  vec3 d = vec3(cos(phi) * sq, sin(phi) * sq, sqrt(r2));
  return d.x * tangent + d.y * bitangent + d.z * n;
}

// PCG hash (https://www.pcg-random.org/)
uint pcg(uint v) {
  uint state = v * 747796405u + 2891336453u;
  uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  return (word >> 22u) ^ word;
}

// xxHash32 for collision detection checksum
uint xxhash32(uint p) {
  const uint PRIME32_2 = 2246822519u;
  const uint PRIME32_3 = 3266489917u;
  const uint PRIME32_4 = 668265263u;
  const uint PRIME32_5 = 374761393u;
  uint h32 = p + PRIME32_5;
  h32 = PRIME32_4 * ((h32 << 17u) | (h32 >> 15u));
  h32 = PRIME32_2 * (h32 ^ (h32 >> 15u));
  h32 = PRIME32_3 * (h32 ^ (h32 >> 13u));
  return h32 ^ (h32 >> 16u);
}

void bumpFrame(uint cellIndex, uint frameLow) {
  HashSlot hs = HashSlot(pc.light.hashSlot);
  uint64_t cur = atomicCompSwap(hs.v[cellIndex], 0, 0);
  for (uint attempt = 0u; attempt < 4u; attempt++) {
    if (uint(cur >> 56) == frameLow) return;
    uint64_t fresh = (cur << 8) >> 8; // clear high byte
    fresh |= uint64_t(frameLow) << 56;
    uint64_t prev = atomicCompSwap(hs.v[cellIndex], cur, fresh);
    if (prev == cur) return;
    cur = prev;
  }
}

// CAS-loop halve of samples and hits
void halve(uint cellIndex, uint64_t cur) {
  HashSlot hs = HashSlot(pc.light.hashSlot);
  for (uint attempt = 0u; attempt < 4u; attempt++) {
    uint s = uint(cur) & 0xFFFFu;
    uint h = min((uint(cur) >> 16u) & 0xFFFFu, s);
    uint newLow = ((h >> 1u) << 16u) | (s >> 1u);
    uint64_t newSlot = (cur & 0xFFFFFFFF00000000ul) | uint64_t(newLow);
    uint64_t prev = atomicCompSwap(hs.v[cellIndex], cur, newSlot);
    if (prev == cur) return;
    cur = prev;
  }
}

// CAS-loop undo of our atomicAdd: subtract from low 32 bits only, preserve high 32
void undoAdd(uint cellIndex, uint64_t cur, uint hit) {
  HashSlot hs = HashSlot(pc.light.hashSlot);
  for (uint attempt = 0u; attempt < 4u; attempt++) {
    uint cur_hits = (uint(cur) >> 16u) & 0xFFFFu;
    uint dec_hits = (hit > 0u && cur_hits > 0u) ? 1u : 0u;
    uint dec = 1u | (dec_hits << 16u);
    uint64_t newSlot = (cur & 0xFFFFFFFF00000000ul) | uint64_t(uint(cur) - dec);
    uint64_t prev = atomicCompSwap(hs.v[cellIndex], cur, newSlot);
    if (prev == cur) return;
    cur = prev;
  }
}

void accumulateAndMaybeHalve(uint cellIndex, uint hit) {
  HashSlot hs = HashSlot(pc.light.hashSlot);
  uint64_t addend = uint64_t((hit << 16u) + 1u);
  uint64_t prevData = atomicAdd(hs.v[cellIndex], addend);
  uint prevSamples = uint(prevData) & 0xFFFFu;
  if (prevSamples >= pc.light.maxSamples) {
    undoAdd(cellIndex, prevData, hit);
    return;
  }
  if (prevSamples + 1u == pc.light.maxSamples) halve(cellIndex, prevData);
}

bool isCellReady(uint dataLow) {
  return (dataLow & 0xFFFFu) >= 4u;
}

// Returns -1.0 when the cell isn't ready yet, otherwise visibility in [0,1] (1 = visible).
float cellVisibility(uint dataLow) {
  uint samples = dataLow & 0xFFFFu;
  uint hits = min((dataLow >> 16u) & 0xFFFFu, samples);
  if (samples < 4u) return -1.0;
  return float(samples - hits) / float(samples);
}

void computeBucket(vec3 position, float cellSize, uint normalHashPCG, uint normalHashXXH, out uint baseCell, out uint checksum) {
  ivec3 p = ivec3(floor(position / cellSize));
  uint cs = uint(cellSize * 10000.0);
  uint hashKey = pcg(cs + pcg(uint(p.x) + pcg(uint(p.y) + pcg(uint(p.z) + normalHashPCG))));
  baseCell = (hashKey & ((pc.light.hashMapSize >> 2u) - 1u)) << 2u;
  checksum = max(xxhash32(cs + xxhash32(uint(p.x) + xxhash32(uint(p.y) + xxhash32(uint(p.z) + normalHashXXH)))) & 0xFFFFFFu, 1u);
}

uint spatialHashFindOrInsert(vec3 position, float cellSize, uint normalHashPCG, uint normalHashXXH) {
  HashSlot hs = HashSlot(pc.light.hashSlot);
  uint baseCell, checksum;
  computeBucket(position, cellSize, normalHashPCG, normalHashXXH, baseCell, checksum);
  // Phase 1: look for our key already in the bucket
  for (uint i = 0u; i < 4u; i++) {
    if ((uint(hs.v[baseCell + i] >> 32) & 0xFFFFFFu) == checksum) return baseCell + i;
  }
  // Phase 2: install empty slot first, evict a stale cell otherwise
  uint frameLow = pc.light.frameId & 0xFFu;
  uint64_t newSlot = (uint64_t(frameLow) << 56) | (uint64_t(checksum) << 32);
  uint64_t empty = uint64_t(0);
  for (uint i = 0u; i < 4u; i++) {
    uint cellIndex = baseCell + i;
    uint64_t prev = atomicCompSwap(hs.v[cellIndex], empty, newSlot);
    if (prev == empty || (uint(prev >> 32) & 0xFFFFFFu) == checksum)
      return cellIndex;
    uint storedFrame = uint(prev >> 56);
    uint age = (frameLow - storedFrame) & 0xFFu;
    if (age > 3u) {
      uint64_t claim = atomicCompSwap(hs.v[cellIndex], prev, newSlot);
      if (claim == prev || (uint(claim >> 32) & 0xFFFFFFu) == checksum)
        return cellIndex;
    }
  }
  return 0xFFFFFFFFu;
}

// Read-only hash lookup: no allocation, no atomics
uint spatialHashFind(vec3 position, float cellSize, uint normalHashPCG, uint normalHashXXH) {
  HashSlot hs = HashSlot(pc.light.hashSlot);
  uint baseCell, checksum;
  computeBucket(position, cellSize, normalHashPCG, normalHashXXH, baseCell, checksum);
  for (uint i = 0u; i < 4u; i++) {
    uint stored = uint(hs.v[baseCell + i] >> 32) & 0xFFFFFFu;
    if (stored == checksum) return baseCell + i;
    if (stored == 0u) return 0xFFFFFFFFu;
  }
  return 0xFFFFFFFFu;
}

// Adaptive cell size (Gautron 2020, Eq. 2-3): projects to ~sp pixels on screen, quantized to power-of-2.
float computeCellSize(vec3 worldPos) {
  float dist = distance(pc.cameraPos.xyz, worldPos);
  float h = dist / pc.light.projScaleY; // h = dist * tan(fov/2)
  float sw = pc.light.sp * (h * 2.0) / pc.light.resolutionY;
  return exp2(floor(log2(max(sw / pc.light.smin, 1.0)))) * pc.light.smin;
}

// Returns ambient occlusion in [0,1] (1 = fully lit). 'n' should be the geometric normal.
float computeAO(vec3 worldPos, vec3 n, vec4 fragCoord) {
  HashSlot hs = HashSlot(pc.light.hashSlot);

  float occlusion = 1.0;

  vec3 origin = worldPos + n * 0.001; // avoid self-occlusion

  vec3 tangent, bitangent;
  computeTBN(n, tangent, bitangent);

  uint seed = tea(uint(fragCoord.y * 4003.0 + fragCoord.x), pc.light.frameId);

  if (pc.light.enableSpatialHash != 0u) {
    float swd = computeCellSize(worldPos);

    // precompute normal hash (shared across all LODs)
    ivec3 nn = ivec3(floor(n * 3.0));
    uint nhPCG = pcg(uint(nn.x) + pcg(uint(nn.y) + pcg(uint(nn.z))));
    uint nhXXH = xxhash32(uint(nn.x) + xxhash32(uint(nn.y) + xxhash32(uint(nn.z))));

    uint frameLow = pc.light.frameId & 0xFFu;
    // always find LOD 0
    uint cell0 = spatialHashFindOrInsert(worldPos, swd, nhPCG, nhXXH);
    uint data0 = (cell0 != 0xFFFFFFFFu) ? uint(hs.v[cell0]) : 0u;
    uint samples0 = data0 & 0xFFFFu;
    bool ready0 = isCellReady(data0);

    // trace one ray, shared across all LODs that need a sample this frame
    uint hit = 0u;
    bool lod0NeedsRay = cell0 != 0xFFFFFFFFu && samples0 < pc.light.maxSamples;
    if (!ready0 || lod0NeedsRay) {
      vec3 direction = sampleCosineHemisphere(seed, tangent, bitangent, n);
      rayQueryEXT rayQuery;
      hit = traceAO(rayQuery, origin, direction) > 0.0 ? 1u : 0u;
    }

    // accumulate to LOD 0
    if (cell0 != 0xFFFFFFFFu) {
      if (lod0NeedsRay) accumulateAndMaybeHalve(cell0, hit);
      bumpFrame(cell0, frameLow);
    }

    // LOD 0 not converged: also fill coarser LODs so the fallback render has data
    uint cachedData[4] = uint[4](data0, 0u, 0u, 0u);
    if (!ready0) {
      float lodSize = swd * 2.0;
      for (int lod = 1; lod < 4; lod++) {
        uint ci = spatialHashFindOrInsert(worldPos, lodSize, nhPCG, nhXXH);
        if (ci != 0xFFFFFFFFu) {
          cachedData[lod] = uint(hs.v[ci]);
          if ((cachedData[lod] & 0xFFFFu) < pc.light.maxSamples)
            accumulateAndMaybeHalve(ci, hit);
          bumpFrame(ci, frameLow);
        }
        lodSize *= 2.0;
      }
    }

    if (ready0 && pc.light.enableFiltering != 0u) {
      // trilinear interpolation across 8 neighboring cells to smooth cell boundaries
      vec3 cellPos = worldPos / swd - 0.5;
      ivec3 base = ivec3(floor(cellPos));
      vec3 f = fract(cellPos);
      float totalWeight = 0.0;
      float totalAO = 0.0;
      for (int dz = 0; dz < 2; dz++) {
        for (int dy = 0; dy < 2; dy++) {
          for (int dx = 0; dx < 2; dx++) {
            vec3 neighborPos = (vec3(base + ivec3(dx, dy, dz)) + 0.5) * swd;
            uint ci = spatialHashFind(neighborPos, swd, nhPCG, nhXXH);
            if (ci != 0xFFFFFFFFu) {
              float vis = cellVisibility(uint(hs.v[ci]));
              if (vis >= 0.0) {
                float w = (dx == 0 ? (1.0 - f.x) : f.x) *
                          (dy == 0 ? (1.0 - f.y) : f.y) *
                          (dz == 0 ? (1.0 - f.z) : f.z);
                totalWeight += w;
                totalAO += w * vis;
              }
            }
          }
        }
      }
      occlusion = (totalWeight > 0.0) ? (totalAO / totalWeight) : 1.0;
    } else {
      // render from finest LOD with enough samples (coarse-to-fine)
      uint renderData = ready0 ? data0 : 0u;
      if (!ready0) {
        for (int lod = 3; lod >= 0; lod--) {
          if (isCellReady(cachedData[lod]))
            renderData = cachedData[lod];
        }
      }
      if (renderData != 0u) {
        float vis = cellVisibility(renderData);
        if (vis >= 0.0) occlusion = vis;
      }
    }
  } else {
    // per-pixel AO (spatial hash disabled)
    float occl = 0.0;
    int numSamples = max(int(pc.light.aoSamples), 1);
    for (int i = 0; i < numSamples; i++) {
      vec3 direction = sampleCosineHemisphere(seed, tangent, bitangent, n);
      rayQueryEXT rayQuery;
      occl += traceAO(rayQuery, origin, direction);
    }
    occlusion = 1.0 - (occl / float(numSamples));
  }

  return pow(clamp(occlusion, 0.0, 1.0), pc.light.aoPower);
}
