#include "UtilsAnim.h"

#include "UtilsGLTF.h"
#include "VulkanApp.h"
#include <assimp/GltfMaterial.h>
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <cmath>

using glm::quat;

AnimationChannel initChannel(const aiNodeAnim* anim)
{
  AnimationChannel channel;
  channel.pos.resize(anim->mNumPositionKeys);

  for (uint32_t i = 0; i < anim->mNumPositionKeys; ++i) {
    channel.pos[i] = { .pos = aiVector3DToVec3(anim->mPositionKeys[i].mValue), .time = (float)anim->mPositionKeys[i].mTime };
  }

  channel.rot.resize(anim->mNumRotationKeys);
  for (uint32_t i = 0; i < anim->mNumRotationKeys; ++i) {
    channel.rot[i] = { .rot = aiQuaternionToQuat(anim->mRotationKeys[i].mValue), .time = (float)anim->mRotationKeys[i].mTime };
  }

  channel.scale.resize(anim->mNumScalingKeys);
  for (uint32_t i = 0; i < anim->mNumScalingKeys; ++i) {
    channel.scale[i] = { .scale = aiVector3DToVec3(anim->mScalingKeys[i].mValue), .time = (float)anim->mScalingKeys[i].mTime };
  }

  return channel;
}

template <typename T> uint32_t getTimeIndex(const std::vector<T>& t, float time)
{
  return std::max(
      0,
      (int)std::distance(t.begin(), std::lower_bound(t.begin(), t.end(), time, [&](const T& lhs, float rhs) { return lhs.time < rhs; })) -
          1);
}

float interpolationVal(float lastTimeStamp, float nextTimeStamp, float animationTime)
{
  return (animationTime - lastTimeStamp) / (nextTimeStamp - lastTimeStamp);
}

vec3 interpolatePosition(const AnimationChannel& channel, float time)
{
  if (channel.pos.size() == 1)
    return channel.pos[0].pos;

  uint32_t start = getTimeIndex<>(channel.pos, time);
  uint32_t end   = start + 1;
  float mix = interpolationVal(channel.pos[start].time, channel.pos[end].time, time);
  return glm::mix(channel.pos[start].pos, channel.pos[end].pos, mix);
}

glm::quat interpolateRotation(const AnimationChannel& channel, float time)
{
  if (channel.rot.size() == 1)
    return channel.rot[0].rot;

  uint32_t start = getTimeIndex<>(channel.rot, time);
  uint32_t end   = start + 1;
  float mix = interpolationVal(channel.rot[start].time, channel.rot[end].time, time);
  return glm::slerp(channel.rot[start].rot, channel.rot[end].rot, mix);
}

vec3 interpolateScaling(const AnimationChannel& channel, float time)
{
  if (channel.scale.size() == 1)
    return channel.scale[0].scale;

  uint32_t start  = getTimeIndex<>(channel.scale, time);
  uint32_t end    = start + 1;
  float coef = interpolationVal(channel.scale[start].time, channel.scale[end].time, time);
  return glm::mix(channel.scale[start].scale, channel.scale[end].scale, coef);
}

mat4 animationTransform(const AnimationChannel& channel, float time)
{
  mat4 translation = glm::translate(mat4(1.0f), interpolatePosition(channel, time));
  mat4 rotation    = glm::toMat4(glm::normalize(interpolateRotation(channel, time)));
  mat4 scale       = glm::scale(mat4(1.0f), interpolateScaling(channel, time));
  return translation * rotation * scale;
}

mat4 animationTransformBlending(const AnimationChannel& channel1, float time1, const AnimationChannel& channel2, float time2, float weight)
{
  mat4 trans1      = glm::translate(mat4(1.0f), interpolatePosition(channel1, time1));
  mat4 trans2      = glm::translate(mat4(1.0f), interpolatePosition(channel2, time2));
  mat4 translation = glm::mix(trans1, trans2, weight);

  quat rot1 = interpolateRotation(channel1, time1);
  quat rot2 = interpolateRotation(channel2, time2);

  mat4 rotation = glm::toMat4(glm::normalize(glm::slerp(rot1, rot2, weight)));

  vec3 scl1  = interpolateScaling(channel1, time1);
  vec3 scl2  = interpolateScaling(channel2, time2);
  mat4 scale = glm::scale(mat4(1.0f), glm::mix(scl1, scl2, weight));

  return translation * rotation * scale;
}

MorphState morphTransform(const MorphTarget& target, const MorphingChannel& channel, float time)
{
  MorphState ms;
  ms.meshId = target.meshId;

  float mix = 0.0f;
  int start = 0;
  int end   = 0;

  if (channel.key.size() > 0) {
    start = getTimeIndex(channel.key, time);
    end   = start + 1;
    mix   = interpolationVal(channel.key[start].time, channel.key[end].time, time);
  }

  for (uint32_t i = 0; i < std::min((uint32_t)target.offset.size(), (uint32_t)MAX_MORPH_WEIGHTS); ++i) {
    ms.morphTarget[i] = target.offset[channel.key[start].mesh[i]];
    ms.weights[i]     = glm::mix(channel.key[start].weight[i], channel.key[end].weight[i], mix);
  }

  return ms;
}

void initAnimations(GLTFContext& glTF, const aiScene* scene)
{
  glTF.animations.resize(scene->mNumAnimations);

  for (uint32_t i = 0; i < scene->mNumAnimations; ++i) {
    Animation& anim     = glTF.animations[i];
    anim.name           = scene->mAnimations[i]->mName.C_Str();
    anim.duration       = scene->mAnimations[i]->mDuration;
    anim.ticksPerSecond = scene->mAnimations[i]->mTicksPerSecond;
    for (uint32_t c = 0; c < scene->mAnimations[i]->mNumChannels; c++) {
      const aiNodeAnim* channel = scene->mAnimations[i]->mChannels[c];
      const char* boneName      = channel->mNodeName.data;
      uint32_t boneId           = glTF.bonesByName[boneName].boneId;
      if (boneId == ~0u) {
        for (const GLTFNode& node : glTF.nodesStorage) {
          if (node.name != boneName)
            continue;
          boneId                      = node.modelMtxId;
          glTF.bonesByName[boneName] = {
            .boneId    = boneId,
            .transform = glTF.hasBones ? glm::inverse(node.transform) : mat4(1),
          };
          break;
        }
      }
      assert(boneId != ~0u);
      anim.channels[boneId] = initChannel(channel);
    }

    const uint32_t numMorphTargetChannels = scene->mAnimations[i]->mNumMorphMeshChannels;
    anim.morphChannels.resize(numMorphTargetChannels);

    for (uint32_t c = 0; c < numMorphTargetChannels; c++) {
      const aiMeshMorphAnim* channel = scene->mAnimations[i]->mMorphMeshChannels[c];

      MorphingChannel& morphChannel = anim.morphChannels[c];

      morphChannel.name = channel->mName.C_Str();
      morphChannel.key.resize(channel->mNumKeys);

      for (uint32_t k = 0; k < channel->mNumKeys; ++k) {
        MorphingChannelKey& key = morphChannel.key[k];
        key.time                = channel->mKeys[k].mTime;
        for (uint32_t v = 0; v < std::min((uint32_t)MAX_MORPH_WEIGHTS, channel->mKeys[k].mNumValuesAndWeights); ++v) {
          key.mesh[v]   = channel->mKeys[k].mValues[v];
          key.weight[v] = channel->mKeys[k].mWeights[v];
        }
      }
    }
  }
}

void updateAnimation(GLTFContext& glTF, AnimationState& anim, float dt)
{
  if (!anim.active || (anim.animId == ~0u)) {
    glTF.morphing = false;
    glTF.skinning = false;
    return;
  }

  const Animation& activeAnim = glTF.animations[anim.animId];
  anim.currentTime += activeAnim.ticksPerSecond * dt;

  if (anim.playOnce && anim.currentTime > activeAnim.duration) {
    anim.currentTime = activeAnim.duration;
    anim.active      = false;
  } else
    anim.currentTime = fmodf(anim.currentTime, activeAnim.duration);

  // Apply animations
  std::function<void(GLTFNodeRef gltfNode, const mat4& parentTransform)> traverseTree = [&](GLTFNodeRef gltfNode,
                                                                                            const mat4& parentTransform) {
    const GLTFBone& bone  = glTF.bonesByName[glTF.nodesStorage[gltfNode].name];
    const uint32_t boneId = bone.boneId;

    if (boneId != ~0u) {
      assert(boneId == glTF.nodesStorage[gltfNode].modelMtxId);
      auto channel                = activeAnim.channels.find(boneId);
      const bool hasActiveChannel = channel != activeAnim.channels.end();

      glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId] =
          parentTransform *
          (hasActiveChannel ? animationTransform(channel->second, anim.currentTime) : glTF.nodesStorage[gltfNode].transform);

      glTF.skinning = true;
    } else {
      glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId] = parentTransform * glTF.nodesStorage[gltfNode].transform;
    }

    for (uint32_t i = 0; i < glTF.nodesStorage[gltfNode].children.size(); i++) {
      const GLTFNodeRef child = glTF.nodesStorage[gltfNode].children[i];

      traverseTree(child, glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId]);
    }
  };

  traverseTree(glTF.root, mat4(1.0f));

  for (const std::pair<std::string, GLTFBone>& b : glTF.bonesByName) {
    if (b.second.boneId != ~0u) {
      glTF.matrices[b.second.boneId] = glTF.matrices[b.second.boneId] * b.second.transform;
    }
  }

  glTF.morphStates.resize(glTF.meshesStorage.size());
  // update morphing
  if (glTF.enableMorphing) {
    if (!activeAnim.morphChannels.empty()) {
      for (size_t i = 0; i < activeAnim.morphChannels.size(); ++i) {
        const MorphingChannel& channel = activeAnim.morphChannels[i];
        const uint32_t meshId          = glTF.meshesRemap[channel.name];
        const MorphTarget& morphTarget = glTF.morphTargets[meshId];

        if (morphTarget.meshId != ~0u) {
          glTF.morphStates[morphTarget.meshId] = morphTransform(morphTarget, channel, anim.currentTime);
        }
      }

      glTF.morphing = true;
    }
  }
}

void updateAnimationBlending(GLTFContext& glTF, AnimationState& anim1, AnimationState& anim2, float weight, float dt)
{
  if (anim1.active && anim2.active) {
    const Animation& activeAnim1 = glTF.animations[anim1.animId];
    anim1.currentTime += activeAnim1.ticksPerSecond * dt;

    if (anim1.playOnce && anim1.currentTime > activeAnim1.duration) {
      anim1.currentTime = activeAnim1.duration;
      anim1.active      = false;
    } else
      anim1.currentTime = fmodf(anim1.currentTime, activeAnim1.duration);

    const Animation& activeAnim2 = glTF.animations[anim2.animId];
    anim2.currentTime += activeAnim2.ticksPerSecond * dt;

    if (anim2.playOnce && anim2.currentTime > activeAnim2.duration) {
      anim2.currentTime = activeAnim2.duration;
      anim2.active      = false;
    } else
      anim2.currentTime = fmodf(anim2.currentTime, activeAnim2.duration);

    // Update skinning
    std::function<void(GLTFNodeRef gltfNode, const mat4& parentTransform)> traverseTree = [&](GLTFNodeRef gltfNode,
                                                                                              const mat4& parentTransform) {
      const GLTFBone& bone  = glTF.bonesByName[glTF.nodesStorage[gltfNode].name];
      const uint32_t boneId = bone.boneId;
      if (boneId != ~0u) {
        auto channel1 = activeAnim1.channels.find(boneId);
        auto channel2 = activeAnim2.channels.find(boneId);

        if (channel1 != activeAnim1.channels.end() && channel2 != activeAnim2.channels.end()) {
          glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId] =
              parentTransform *
              animationTransformBlending(channel1->second, anim1.currentTime, channel2->second, anim2.currentTime, weight);
        } else if (channel1 != activeAnim1.channels.end()) {
          glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId] = parentTransform * animationTransform(channel1->second, anim1.currentTime);
        } else if (channel2 != activeAnim2.channels.end()) {
          glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId] = parentTransform * animationTransform(channel2->second, anim2.currentTime);
        } else {
          glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId] = parentTransform * glTF.nodesStorage[gltfNode].transform;
        }
        glTF.skinning = true;
      }

      for (uint32_t i = 0; i < glTF.nodesStorage[gltfNode].children.size(); i++) {
        const uint32_t child = glTF.nodesStorage[gltfNode].children[i];

        traverseTree(child, glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId]);
      }
    };

    traverseTree(glTF.root, mat4(1.0f));

    for (const std::pair<std::string, GLTFBone>& b : glTF.bonesByName) {
      if (b.second.boneId != ~0u) {
        glTF.matrices[b.second.boneId] = glTF.matrices[b.second.boneId] * b.second.transform;
      }
    }

  } else {
    glTF.morphing = false;
    glTF.skinning = false;
  }
}
