#include "UtilsAnim.h"

#include "UtilsGLTF.h"
#include "VulkanApp.h"
#include <assimp/GltfMaterial.h>
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <cmath>

AnimationChannel initChannel(const aiNodeAnim* anim)
{
  AnimationChannel channel;
  channel.pos.resize(anim->mNumPositionKeys);

  for (int i = 0; i < anim->mNumPositionKeys; ++i) {
    channel.pos[i] = { .pos = aiVector3DToVec3(anim->mPositionKeys[i].mValue), .time = (float)anim->mPositionKeys[i].mTime };
  }

  channel.rot.resize(anim->mNumRotationKeys);
  for (int i = 0; i < anim->mNumRotationKeys; ++i) {
    channel.rot[i] = { .rot = aiQuaternionToQuat(anim->mRotationKeys[i].mValue), .time = (float)anim->mRotationKeys[i].mTime };
  }

  channel.scale.resize(anim->mNumScalingKeys);
  for (int i = 0; i < anim->mNumScalingKeys; ++i) {
    channel.scale[i] = { .scale = aiVector3DToVec3(anim->mScalingKeys[i].mValue), .time = (float)anim->mScalingKeys[i].mTime };
  }

  return channel;
}

template <typename T> int getTimeIndex(const std::vector<T>& t, float time)
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

glm::vec3 interpolatePosition(const AnimationChannel& channel, float time)
{
  if (channel.pos.size() == 1)
    return channel.pos[0].pos;

  int start = getTimeIndex<>(channel.pos, time);
  int end   = start + 1;
  float mix = interpolationVal(channel.pos[start].time, channel.pos[end].time, time);
  return glm::mix(channel.pos[start].pos, channel.pos[end].pos, mix);
}

glm::quat interpolateRotation(const AnimationChannel& channel, float time)
{
  if (channel.rot.size() == 1)
    return channel.rot[0].rot;

  int start = getTimeIndex<>(channel.rot, time);
  int end   = start + 1;
  float mix = interpolationVal(channel.rot[start].time, channel.rot[end].time, time);
  return glm::slerp(channel.rot[start].rot, channel.rot[end].rot, mix);
}

glm::vec3 interpolateScaling(const AnimationChannel& channel, float time)
{
  if (channel.scale.size() == 1)
    return channel.scale[0].scale;

  int start = getTimeIndex<>(channel.scale, time);
  int end   = start + 1;
  float mix = interpolationVal(channel.scale[start].time, channel.scale[end].time, time);
  return glm::mix(channel.scale[start].scale, channel.scale[end].scale, mix);
}

glm::mat4 animationTransform(const AnimationChannel& channel, float time)
{
  glm::mat4 translation = glm::translate(glm::mat4(1.0f), interpolatePosition(channel, time));
  glm::mat4 rotation    = glm::toMat4(glm::normalize(interpolateRotation(channel, time)));
  glm::mat4 scale       = glm::scale(glm::mat4(1.0f), interpolateScaling(channel, time));
  return translation * rotation * scale;
}

glm::mat4 animationTransformBlending(
    const AnimationChannel& channel1, float time1, const AnimationChannel& channel2, float time2, float weight)
{
  auto trans1           = glm::translate(glm::mat4(1.0f), interpolatePosition(channel1, time1));
  auto trans2           = glm::translate(glm::mat4(1.0f), interpolatePosition(channel2, time2));
  glm::mat4 translation = glm::mix(trans1, trans2, weight);

  auto rot1 = interpolateRotation(channel1, time1);
  auto rot2 = interpolateRotation(channel2, time2);

  glm::mat4 rotation = glm::toMat4(glm::normalize(glm::slerp(rot1, rot2, weight)));

  auto scl1       = interpolateScaling(channel1, time1);
  auto scl2       = interpolateScaling(channel2, time2);
  glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::mix(scl1, scl2, weight));

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
    start = getTimeIndex<>(channel.key, time);
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

  for (int i = 0; i < scene->mNumAnimations; ++i) {
    Animation& anim     = glTF.animations[i];
    anim.name           = scene->mAnimations[i]->mName.C_Str();
    anim.duration       = scene->mAnimations[i]->mDuration;
    anim.ticksPerSecond = scene->mAnimations[i]->mTicksPerSecond;
    int channels        = scene->mAnimations[i]->mNumChannels;
    for (int c = 0; c < channels; c++) {
      auto channel         = scene->mAnimations[i]->mChannels[c];
      std::string boneName = channel->mNodeName.data;

      uint32_t boneId = glTF.bonesStorage[boneName].boneId;
      if (boneId == ~0u) {
        for (auto node : glTF.nodesStorage) {
          if (node.name == boneName) {
            boneId                      = node.modelMtxId;
            glTF.bonesStorage[boneName] = { .boneId = boneId, .transform = glm::inverse(node.transform) };
            break;
          }
        }
      }
      assert(boneId != ~0u);
      anim.channels[boneId] = initChannel(channel);
    }

    int numMorphTargetChannels = scene->mAnimations[i]->mNumMorphMeshChannels;
    anim.morphChannels.resize(numMorphTargetChannels);

    for (int c = 0; c < numMorphTargetChannels; c++) {
      auto channel = scene->mAnimations[i]->mMorphMeshChannels[c];

      auto& morphChannel = anim.morphChannels[c];

      morphChannel.name = channel->mName.C_Str();
      morphChannel.key.resize(channel->mNumKeys);

      for (int k = 0; k < channel->mNumKeys; ++k) {
        auto& key = morphChannel.key[k];
        key.time  = channel->mKeys[k].mTime;
        for (int v = 0; v < std::min((uint32_t)MAX_MORPH_WEIGHTS, channel->mKeys[k].mNumValuesAndWeights); ++v) {
          key.mesh[v]   = channel->mKeys[k].mValues[v];
          key.weight[v] = channel->mKeys[k].mWeights[v];
        }
      }
    }
  }
}

void updateAnimation(GLTFContext& glTF, AnimationState& anim, float dt)
{
  if (!anim.active) {
    glTF.morphing = false;
    glTF.skinning = false;
    return;
  }

  if (anim.animId == ~0) {
	  glTF.morphing = false;
	  glTF.skinning = false;
	  return;
  }

  auto activeAnim = glTF.animations[anim.animId];
  anim.currentTime += activeAnim.ticksPerSecond * dt;

  if (anim.playOnce && anim.currentTime > activeAnim.duration) {
    anim.currentTime = activeAnim.duration;
    anim.active      = false;
  } else
    anim.currentTime = fmodf(anim.currentTime, activeAnim.duration);

  // Apply animations
  std::function<void(GLTFNodeRef gltfNode, const glm::mat4& parentTransform)> traverseTree = [&](GLTFNodeRef gltfNode,
                                                                                                 const glm::mat4& parentTransform) {
    auto bone   = glTF.bonesStorage[glTF.nodesStorage[gltfNode].name];
    auto boneId = bone.boneId;

    if (boneId != ~0) {
      assert(boneId == glTF.nodesStorage[gltfNode].modelMtxId);
      auto channel = activeAnim.channels.find(boneId);
      if (channel != activeAnim.channels.end()) {
        glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId] = parentTransform * animationTransform(channel->second, anim.currentTime);
      } else {
        glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId] = parentTransform * glTF.nodesStorage[gltfNode].transform;
      }

		glTF.skinning = true;
    }

    for (uint32_t i = 0; i < glTF.nodesStorage[gltfNode].children.size(); i++) {
      auto child = glTF.nodesStorage[gltfNode].children[i];

      traverseTree(child, glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId]);
    }
  };

  traverseTree(glTF.root, glm::mat4(1.0f));

  for (auto b : glTF.bonesStorage) {
    if (b.second.boneId != ~0) {
      glTF.matrices[b.second.boneId] = glTF.matrices[b.second.boneId] * b.second.transform;
    }
  }

  glTF.morphStates.clear();
  // update morphing
  if (!activeAnim.morphChannels.empty()) {
    for (size_t i = 0; i < activeAnim.morphChannels.size(); ++i) {
      auto channel     = activeAnim.morphChannels[i];
      auto meshId      = glTF.meshesRemap[channel.name];
      auto morphTarget = glTF.morphTargets[meshId];

      if (morphTarget.meshId != ~0) {
        glTF.morphStates.push_back(morphTransform(morphTarget, channel, anim.currentTime));
      }
    }

    glTF.morphing = true;
  }
}

void updateAnimationBlending(GLTFContext& glTF, AnimationState& anim1, AnimationState& anim2, float weight, float dt)
{
  if (anim1.active && anim2.active) {
    auto activeAnim1 = glTF.animations[anim1.animId];
    anim1.currentTime += activeAnim1.ticksPerSecond * dt;

    if (anim1.playOnce && anim1.currentTime > activeAnim1.duration) {
      anim1.currentTime = activeAnim1.duration;
      anim1.active      = false;
    } else
      anim1.currentTime = fmodf(anim1.currentTime, activeAnim1.duration);

	 auto activeAnim2 = glTF.animations[anim2.animId];
	 anim2.currentTime += activeAnim2.ticksPerSecond * dt;

    if (anim2.playOnce && anim2.currentTime > activeAnim2.duration) {
      anim2.currentTime = activeAnim2.duration;
      anim2.active      = false;
    } else
      anim2.currentTime = fmodf(anim2.currentTime, activeAnim2.duration);

    // Update skinning
    std::function<void(GLTFNodeRef gltfNode, const glm::mat4& parentTransform)> traverseTree = [&](GLTFNodeRef gltfNode,
                                                                                                   const glm::mat4& parentTransform) {
      auto bone   = glTF.bonesStorage[glTF.nodesStorage[gltfNode].name];
      auto boneId = bone.boneId;
      if (boneId != ~0) {
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
        auto child = glTF.nodesStorage[gltfNode].children[i];

        traverseTree(child, glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId]);
      }
    };

    traverseTree(glTF.root, glm::mat4(1.0f));

    for (auto b : glTF.bonesStorage) {
      if (b.second.boneId != ~0) {
        glTF.matrices[b.second.boneId] = glTF.matrices[b.second.boneId] * b.second.transform;
      }
    }

  } else {
    glTF.morphing = false;
    glTF.skinning = false;
  }
}
