//

struct TransparentFragment {
  f16vec4 color;
  float depth;
  uint next;
};

layout(std430, buffer_reference) buffer TransparencyListsBuffer {
  TransparentFragment frags[];
};
