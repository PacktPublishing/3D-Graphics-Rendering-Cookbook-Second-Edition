#include "shared/Scene/Scene.h"
#include "shared/Utils.h"

#include <algorithm>
#include <numeric>

int addNode(Scene& scene, int parent, int level)
{
  const int node = (int)scene.hierarchy.size();
  {
    // TODO: resize aux arrays (local/global etc.)
    scene.localTransform.push_back(glm::mat4(1.0f));
    scene.globalTransform.push_back(glm::mat4(1.0f));
  }
  scene.hierarchy.push_back({ .parent = parent, .lastSibling = -1 });
  if (parent > -1) {
    // find first item (sibling)
    const int s = scene.hierarchy[parent].firstChild;
    if (s == -1) {
      scene.hierarchy[parent].firstChild = node;
      scene.hierarchy[node].lastSibling  = node;
    } else {
      int dest = scene.hierarchy[s].lastSibling;
      if (dest <= -1) {
        // no cached lastSibling, iterate nextSibling indices
        for (dest = s; scene.hierarchy[dest].nextSibling != -1; dest = scene.hierarchy[dest].nextSibling)
          ;
      }
      scene.hierarchy[dest].nextSibling = node;
      scene.hierarchy[s].lastSibling    = node;
    }
  }
  scene.hierarchy[node].level       = level;
  scene.hierarchy[node].nextSibling = -1;
  scene.hierarchy[node].firstChild  = -1;
  return node;
}

void markAsChanged(Scene& scene, int node)
{
  const int level = scene.hierarchy[node].level;
  scene.changedAtThisFrame[level].push_back(node);

  // TODO: use non-recursive iteration with aux stack
  for (int s = scene.hierarchy[node].firstChild; s != -1; s = scene.hierarchy[s].nextSibling) {
    markAsChanged(scene, s);
  }
}

int findNodeByName(const Scene& scene, const std::string& name)
{
  // Extremely simple linear search without any hierarchy reference
  // To support DFS/BFS searches separate traversal routines are needed

  for (size_t i = 0; i < scene.localTransform.size(); i++)
    if (scene.nameForNode.contains(i)) {
      int strID = scene.nameForNode.at(i);
      if (strID > -1)
        if (scene.nodeNames[strID] == name)
          return (int)i;
    }

  return -1;
}

bool mat4IsIdentity(const glm::mat4& m);
void fprintfMat4(FILE* f, const glm::mat4& m);

// CPU version of global transform update []
bool recalculateGlobalTransforms(Scene& scene)
{
  bool wasUpdated = false;

  if (!scene.changedAtThisFrame[0].empty()) {
    const int c              = scene.changedAtThisFrame[0][0];
    scene.globalTransform[c] = scene.localTransform[c];
    scene.changedAtThisFrame[0].clear();
    wasUpdated = true;
  }

  for (int i = 1; i < MAX_NODE_LEVEL; i++) {
    for (int c : scene.changedAtThisFrame[i]) {
      const int p              = scene.hierarchy[c].parent;
      scene.globalTransform[c] = scene.globalTransform[p] * scene.localTransform[c];
    }
    wasUpdated |= !scene.changedAtThisFrame[i].empty();
    scene.changedAtThisFrame[i].clear();
  }

  return wasUpdated;
}

void loadMap(FILE* f, std::unordered_map<uint32_t, uint32_t>& map)
{
  std::vector<uint32_t> ms;

  uint32_t sz = 0;
  fread(&sz, 1, sizeof(sz), f);

  ms.resize(sz);
  fread(ms.data(), sizeof(uint32_t), sz, f);

  for (size_t i = 0; i < (sz / 2); i++)
    map[ms[i * 2 + 0]] = ms[i * 2 + 1];
}

void loadScene(const char* fileName, Scene& scene)
{
  FILE* f = fopen(fileName, "rb");

  if (!f) {
    printf("Cannot open scene file '%s'. Please run SceneConverter from Chapter7 and/or MergeMeshes from Chapter 9", fileName);
    return;
  }

  uint32_t sz = 0;
  fread(&sz, sizeof(sz), 1, f);

  scene.hierarchy.resize(sz);
  scene.globalTransform.resize(sz);
  scene.localTransform.resize(sz);
  // TODO: check > -1
  // TODO: recalculate changedAtThisLevel() - find max depth of a node [or save scene.maxLevel]
  fread(scene.localTransform.data(), sizeof(glm::mat4), sz, f);
  fread(scene.globalTransform.data(), sizeof(glm::mat4), sz, f);
  fread(scene.hierarchy.data(), sizeof(Hierarchy), sz, f);

  // Mesh for node [index to some list of buffers]
  loadMap(f, scene.materialForNode);
  loadMap(f, scene.meshForNode);

  if (!feof(f)) {
    loadMap(f, scene.nameForNode);
    loadStringList(f, scene.nodeNames);
    loadStringList(f, scene.materialNames);
  }

  fclose(f);

  markAsChanged(scene, 0);
  recalculateGlobalTransforms(scene);
}

void saveMap(FILE* f, const std::unordered_map<uint32_t, uint32_t>& map)
{
  std::vector<uint32_t> ms;
  ms.reserve(map.size() * 2);
  for (const auto& m : map) {
    ms.push_back(m.first);
    ms.push_back(m.second);
  }
  const uint32_t sz = static_cast<uint32_t>(ms.size());
  fwrite(&sz, sizeof(sz), 1, f);
  fwrite(ms.data(), sizeof(uint32_t), ms.size(), f);
}

void saveScene(const char* fileName, const Scene& scene)
{
  FILE* f = fopen(fileName, "wb");

  const uint32_t sz = (uint32_t)scene.hierarchy.size();
  fwrite(&sz, sizeof(sz), 1, f);

  fwrite(scene.localTransform.data(), sizeof(glm::mat4), sz, f);
  fwrite(scene.globalTransform.data(), sizeof(glm::mat4), sz, f);
  fwrite(scene.hierarchy.data(), sizeof(Hierarchy), sz, f);

  // Mesh for node [index to some list of buffers]
  saveMap(f, scene.materialForNode);
  saveMap(f, scene.meshForNode);

  if (!scene.nodeNames.empty() && !scene.nameForNode.empty()) {
    saveMap(f, scene.nameForNode);
    saveStringList(f, scene.nodeNames);
    saveStringList(f, scene.materialNames);
  }
  fclose(f);
}

bool mat4IsIdentity(const glm::mat4& m)
{
  return (
      m[0][0] == 1 && m[0][1] == 0 && m[0][2] == 0 && m[0][3] == 0 && m[1][0] == 0 && m[1][1] == 1 && m[1][2] == 0 && m[1][3] == 0 &&
      m[2][0] == 0 && m[2][1] == 0 && m[2][2] == 1 && m[2][3] == 0 && m[3][0] == 0 && m[3][1] == 0 && m[3][2] == 0 && m[3][3] == 1);
}

void fprintfMat4(FILE* f, const glm::mat4& m)
{
  if (mat4IsIdentity(m)) {
    fprintf(f, "Identity\n");
  } else {
    fprintf(f, "\n");
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++)
        fprintf(f, "%f ;", m[i][j]);
      fprintf(f, "\n");
    }
  }
}

void dumpTransforms(const char* fileName, const Scene& scene)
{
  FILE* f = fopen(fileName, "a+");
  for (size_t i = 0; i < scene.localTransform.size(); i++) {
    fprintf(f, "Node[%d].localTransform: ", (int)i);
    fprintfMat4(f, scene.localTransform[i]);
    fprintf(f, "Node[%d].globalTransform: ", (int)i);
    fprintfMat4(f, scene.globalTransform[i]);
    fprintf(
        f, "Node[%d].globalDet = %f; localDet = %f\n", (int)i, glm::determinant(scene.globalTransform[i]),
        glm::determinant(scene.localTransform[i]));
  }
  fclose(f);
}

void printChangedNodes(const Scene& scene)
{
  for (int i = 0; i < MAX_NODE_LEVEL && (!scene.changedAtThisFrame[i].empty()); i++) {
    printf("Changed at level(%d):\n", i);

    for (const int& c : scene.changedAtThisFrame[i]) {
      int p = scene.hierarchy[c].parent;
      // scene.globalTransform_[c] = scene.globalTransform_[p] * scene.localTransform_[c];
      printf(" Node %d. Parent = %d; LocalTransform: ", c, p);
      fprintfMat4(stdout, scene.localTransform[i]);
      if (p > -1) {
        printf(" ParentGlobalTransform: ");
        fprintfMat4(stdout, scene.globalTransform[p]);
      }
    }
  }
}

// Shift all hierarchy components in the nodes
void shiftNodes(Scene& scene, int startOffset, int nodeCount, int shiftAmount)
{
  auto shiftNode = [shiftAmount](Hierarchy& node) {
    if (node.parent > -1)
      node.parent += shiftAmount;
    if (node.firstChild > -1)
      node.firstChild += shiftAmount;
    if (node.nextSibling > -1)
      node.nextSibling += shiftAmount;
    if (node.lastSibling > -1)
      node.lastSibling += shiftAmount;
    // node->level does not require to be shifted
  };

  // If there are too many nodes, we can use std::execution::par with std::transform:
  //	 std::transform(scene.hierarchy_.begin() + startOffset,
  //                  scene.hierarchy_.begin() + nodeCount,
  //                  scene.hierarchy_.begin() + startOffset,
  //                  shiftNode);
  //	 for (auto i = scene.hierarchy_.begin() + startOffset ; i != scene.hierarchy_.begin() + nodeCount ; i++)
  //		 shiftNode(*i);

  for (int i = 0; i < nodeCount; i++)
    shiftNode(scene.hierarchy[i + startOffset]);
}

using ItemMap = std::unordered_map<uint32_t, uint32_t>;

// Add the items from otherMap shifting indices and values along the way
void mergeMaps(ItemMap& m, const ItemMap& otherMap, int indexOffset, int itemOffset)
{
  for (const auto& i : otherMap)
    m[i.first + indexOffset] = i.second + itemOffset;
}

/**
  There are different use cases for scene merging.
  The simplest one is the direct "gluing" of multiple scenes into one [all the material lists and mesh lists are merged and indices in all
  scene nodes are shifted appropriately] The second one is creating a "grid" of objects (or scenes) with the same material and mesh sets.
  For the second use case we need two flags: 'mergeMeshes' and 'mergeMaterials' to avoid shifting mesh indices
*/
void mergeScenes(
    Scene& scene, const std::vector<Scene*>& scenes, const std::vector<glm::mat4>& rootTransforms, const std::vector<uint32_t>& meshCounts,
    bool mergeMeshes, bool mergeMaterials)
{
  // Create new root node
  scene.hierarchy = {
    {
     .parent      = -1,
     .firstChild  = 1,
     .nextSibling = -1,
     .lastSibling = -1,
     .level       = 0,
     }
  };

  scene.nameForNode[0] = 0;
  scene.nodeNames      = { "NewRoot" };

  scene.localTransform.push_back(glm::mat4(1.f));
  scene.globalTransform.push_back(glm::mat4(1.f));

  if (scenes.empty())
    return;

  int offs        = 1;
  int meshOffs    = 0;
  int nameOffs    = (int)scene.nodeNames.size();
  int materialOfs = 0;
  auto meshCount  = meshCounts.begin();

  if (!mergeMaterials)
    scene.materialNames = scenes[0]->materialNames;

  // FIXME: too much logic (for all the components in a scene, though mesh data and materials go separately - they're dedicated data lists)
  for (const Scene* s : scenes) {
    mergeVectors(scene.localTransform, s->localTransform);
    mergeVectors(scene.globalTransform, s->globalTransform);

    mergeVectors(scene.hierarchy, s->hierarchy);

    mergeVectors(scene.nodeNames, s->nodeNames);
    if (mergeMaterials)
      mergeVectors(scene.materialNames, s->materialNames);

    const int nodeCount = (int)s->hierarchy.size();

    shiftNodes(scene, offs, nodeCount, offs);

    mergeMaps(scene.meshForNode, s->meshForNode, offs, mergeMeshes ? meshOffs : 0);
    mergeMaps(scene.materialForNode, s->materialForNode, offs, mergeMaterials ? materialOfs : 0);
    mergeMaps(scene.nameForNode, s->nameForNode, offs, nameOffs);

    offs += nodeCount;

    materialOfs += (int)s->materialNames.size();
    nameOffs += (int)s->nodeNames.size();

    if (mergeMeshes) {
      meshOffs += *meshCount;
      meshCount++;
    }
  }

  // fixing 'nextSibling' fields in the old roots (zero-index in all the scenes)
  offs    = 1;
  int idx = 0;
  for (const Scene* s : scenes) {
    const int nodeCount = (int)s->hierarchy.size();
    const bool isLast   = (idx == scenes.size() - 1);
    // calculate new next sibling for the old scene roots
    const int next = isLast ? -1 : offs + nodeCount;

    scene.hierarchy[offs].nextSibling = next;
    // attach to new root
    scene.hierarchy[offs].parent = 0;

    // transform old root nodes, if the transforms are given
    if (!rootTransforms.empty())
      scene.localTransform[offs] = rootTransforms[idx] * scene.localTransform[offs];

    offs += nodeCount;
    idx++;
  }

  // now, shift levels of all nodes below the root
  for (auto i = scene.hierarchy.begin() + 1; i != scene.hierarchy.end(); i++)
    i->level++;
}

void dumpSceneToDot(const char* fileName, const Scene& scene, int* visited)
{
  FILE* f = fopen(fileName, "w");
  fprintf(f, "digraph G\n{\n");
  for (size_t i = 0; i < scene.globalTransform.size(); i++) {
    std::string name  = "";
    std::string extra = "";
    if (scene.nameForNode.contains(i)) {
      int strID = scene.nameForNode.at(i);
      name      = scene.nodeNames[strID];
    }
    if (visited) {
      if (visited[i])
        extra = ", color = red";
    }
    fprintf(f, "n%d [label=\"%s\" %s]\n", (int)i, name.c_str(), extra.c_str());
  }
  for (size_t i = 0; i < scene.hierarchy.size(); i++) {
    int p = scene.hierarchy[i].parent;
    if (p > -1)
      fprintf(f, "\t n%d -> n%d\n", p, (int)i);
  }
  fprintf(f, "}\n");
  fclose(f);
}

// A rather long algorithm (and the auxiliary routines) to delete a number of scene nodes from the hierarchy

// Add an index to a sorted index array
static void addUniqueIdx(std::vector<uint32_t>& v, uint32_t index)
{
  if (!std::binary_search(v.begin(), v.end(), index))
    v.push_back(index);
}

// Recurse down from a node and collect all nodes which are already marked for deletion
static void collectNodesToDelete(const Scene& scene, int node, std::vector<uint32_t>& nodes)
{
  for (int n = scene.hierarchy[node].firstChild; n != -1; n = scene.hierarchy[n].nextSibling) {
    addUniqueIdx(nodes, n);
    collectNodesToDelete(scene, n, nodes);
  }
}

int findLastNonDeletedItem(const Scene& scene, const std::vector<int>& newIndices, int node)
{
  // we have to be more subtle:
  //   if the (newIndices[firstChild_] == -1), we should follow the link and extract the last non-removed item
  //   ..
  if (node == -1)
    return -1;

  return (newIndices[node] == -1) ? findLastNonDeletedItem(scene, newIndices, scene.hierarchy[node].nextSibling) : newIndices[node];
}

void shiftMapIndices(std::unordered_map<uint32_t, uint32_t>& items, const std::vector<int>& newIndices)
{
  std::unordered_map<uint32_t, uint32_t> newItems;
  for (const auto& m : items) {
    int newIndex = newIndices[m.first];
    if (newIndex != -1)
      newItems[newIndex] = m.second;
  }
  items = newItems;
}

// Approximately an O ( N * Log(N) * Log(M)) algorithm (N = scene.size, M = nodesToDelete.size) to delete a collection of nodes from scene
// graph
void deleteSceneNodes(Scene& scene, const std::vector<uint32_t>& nodesToDelete)
{
  // 0) Add all the nodes down below in the hierarchy
  auto indicesToDelete = nodesToDelete;
  for (uint32_t i : indicesToDelete)
    collectNodesToDelete(scene, i, indicesToDelete);

  // aux array with node indices to keep track of the moved ones [moved = [](node) { return (node != nodes[node]); ]
  std::vector<int> nodes(scene.hierarchy.size());
  std::iota(nodes.begin(), nodes.end(), 0);

  // 1.a) Move all the indicesToDelete to the end of 'nodes' array (and cut them off, a variation of swap'n'pop for multiple elements)
  const size_t oldSize = nodes.size();
  eraseSelected(nodes, indicesToDelete);

  // 1.b) Make a newIndices[oldIndex] mapping table
  std::vector<int> newIndices(oldSize, -1);
  for (int i = 0; i < nodes.size(); i++)
    newIndices[nodes[i]] = i;

  // 2) Replace all non-null parent/firstChild/nextSibling pointers in all the nodes by new positions
  auto nodeMover = [&scene, &newIndices](Hierarchy& h) {
    return Hierarchy{
      .parent      = (h.parent != -1) ? newIndices[h.parent] : -1,
      .firstChild  = findLastNonDeletedItem(scene, newIndices, h.firstChild),
      .nextSibling = findLastNonDeletedItem(scene, newIndices, h.nextSibling),
      .lastSibling = findLastNonDeletedItem(scene, newIndices, h.lastSibling),
    };
  };
  std::transform(scene.hierarchy.begin(), scene.hierarchy.end(), scene.hierarchy.begin(), nodeMover);

  // 3) Finally throw away the hierarchy items
  eraseSelected(scene.hierarchy, indicesToDelete);

  // 4) As in mergeScenes() routine we also have to adjust all the "components" (i.e., meshes, materials, names and transformations)

  // 4a) Transformations are stored in arrays, so we just erase the items as we did with the scene.hierarchy_
  eraseSelected(scene.localTransform, indicesToDelete);
  eraseSelected(scene.globalTransform, indicesToDelete);

  // 4b) All the maps should change the key values with the newIndices[] array
  shiftMapIndices(scene.meshForNode, newIndices);
  shiftMapIndices(scene.materialForNode, newIndices);
  shiftMapIndices(scene.nameForNode, newIndices);

  // 5) scene node names list is not modified, but in principle it can be (remove all non-used items and adjust the nameForNode_ map)
  // 6) Material names list is not modified also, but if some materials fell out of use
}
