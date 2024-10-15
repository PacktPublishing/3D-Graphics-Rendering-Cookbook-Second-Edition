#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <unordered_map>
#include <string>

struct AnimationKeyPosition {
  glm::vec3 pos;
  float time;
};

struct AnimationKeyRotation {
  glm::quat rot;
  float time;
};

struct AnimationKeyScale {
  glm::vec3 scale;
  float time;
};

struct AnimationChannel {
  std::vector<AnimationKeyPosition> pos;
  std::vector<AnimationKeyRotation> rot;
  std::vector<AnimationKeyScale> scale;
};

#define MAX_MORPH_WEIGHTS 8
#define MAX_MORPHS  100

struct MorphingChannelKey {
	float time;
	uint32_t mesh[MAX_MORPH_WEIGHTS] = { 0,0,0,0,0,0,0,0 };
	float weight[MAX_MORPH_WEIGHTS] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
};

struct MorphingChannel {
	std::string name;
	std::vector<MorphingChannelKey> key;
};

struct Animation {
  std::unordered_map<int, AnimationChannel> channels;
  std::vector<MorphingChannel> morphChannels;
  float duration;
  float ticksPerSecond;
  std::string name;
};

struct MorphState {
	uint32_t meshId = ~0;
	uint32_t morphTarget[MAX_MORPH_WEIGHTS] = {0,0,0,0,0,0,0,0};
	float weights[MAX_MORPH_WEIGHTS] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
};

struct AnimationState {
  uint32_t animId = ~0;
  float currentTime = 0.0f;
  bool playOnce = false;
  bool active = false;
};


struct GLTFContext;
struct aiScene;

void initAnimations(GLTFContext& glTF, const aiScene* scene);
void updateAnimation(GLTFContext& glTF, AnimationState& anim, float dt);
void updateAnimationBlending(GLTFContext& glTF, AnimationState& anim1, AnimationState& anim2, float weight, float dt);
