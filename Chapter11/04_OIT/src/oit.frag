//

#include <Chapter11/04_OIT/src/common_oit.sp>

layout (set = 0, binding = 2, r32ui) uniform uimage2D kTextures2DIn[];

layout (location=0) in vec2 uv;

layout (location=0) out vec4 out_FragColor;

layout(push_constant) uniform PushConstants {
  TransparencyListsBuffer oitLists;
  uint texColor;
  uint texHeadsOIT;
  float time;
  float opacityBoost;
  uint showHeatmap;
} pc;

#define MAX_FRAGMENTS 64

void main() {
  TransparentFragment frags[MAX_FRAGMENTS];

  uint numFragments = 0;
  uint idx = imageLoad(kTextures2DIn[pc.texHeadsOIT], ivec2(gl_FragCoord.xy)).r;

  // copy the linked list for this fragment into an array
  while (idx != 0xFFFFFFFF && numFragments < MAX_FRAGMENTS) {
    frags[numFragments] = pc.oitLists.frags[idx];
    numFragments++;
    idx = pc.oitLists.frags[idx].next;
  }

  // sort the array by depth using insertion sort (largest to smallest)
  for (int i = 1; i < numFragments; i++) {
    TransparentFragment toInsert = frags[i];
    uint j = i;
    while (j > 0 && toInsert.depth > frags[j-1].depth) {
      frags[j] = frags[j-1];
      j--;
    }
    frags[j] = toInsert;
  }

  // get the color of the closest non-transparent object from the frame buffer
  vec4 color = textureBindless2D(pc.texColor, 0, uv);

  // traverse the array, and combine the colors using the alpha channel
  for (uint i = 0; i < numFragments; i++) {
    color = mix( color, vec4(frags[i].color), clamp(float(frags[i].color.a+pc.opacityBoost), 0.0, 1.0) );
  }

  if (pc.showHeatmap > 0 && numFragments > 0)
    color = (1.0+sin(5.0*pc.time)) * vec4(vec3(numFragments+1, numFragments+1, 0), 0.0) / 16.0;

  out_FragColor = color;
}
