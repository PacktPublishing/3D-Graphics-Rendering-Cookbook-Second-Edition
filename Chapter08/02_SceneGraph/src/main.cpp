#include "shared/VulkanApp.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "shared/LineCanvas.h"
#include "shared/Scene/Scene.h"
#include "shared/Scene/VtxData.h"

#include "Chapter08/VKMesh08.h"
#include "Chapter08/SceneUtils.h"

#include <ImGuizmo/ImGuizmo.h>

int renderSceneTreeUI(const Scene& scene, int node, int selectedNode)
{
  const std::string name  = getNodeName(scene, node);
  const std::string label = name.empty() ? (std::string("Node") + std::to_string(node)) : name;

  const bool isLeaf        = scene.hierarchy[node].firstChild < 0;
  ImGuiTreeNodeFlags flags = isLeaf ? ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet : 0;
  if (node == selectedNode) {
    flags |= ImGuiTreeNodeFlags_Selected;
  }

  ImVec4 color = isLeaf ? ImVec4(0, 1, 0, 1) : ImVec4(1, 1, 1, 1);

  // open some interesting nodes by default
  if (name == "RootNode (gltf orientation matrix)" || name == "RootNode (model correction matrix)" || name == "Root") {
    flags |= ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;
    color = ImVec4(0.9f, 0.6f, 0.6f, 1);
  }
  if (name == "sun" || name == "sun_0" || name.ends_with(".stk") || name.starts_with("p3.earth")) {
    flags |= ImGuiTreeNodeFlags_DefaultOpen;
  }

  ImGui::PushStyleColor(ImGuiCol_Text, color);
  const bool isOpened = ImGui::TreeNodeEx(&scene.hierarchy[node], flags, "%s", label.c_str());
  ImGui::PopStyleColor();

  ImGui::PushID(node);
  {
    if (ImGui::IsItemHovered() && isLeaf) {
      printf("Selected node: %d (%s)\n", node, label.c_str());
      selectedNode = node;
    }

    if (isOpened) {
      for (int ch = scene.hierarchy[node].firstChild; ch != -1; ch = scene.hierarchy[ch].nextSibling) {
        if (int subNode = renderSceneTreeUI(scene, ch, selectedNode); subNode > -1)
          selectedNode = subNode;
      }
      ImGui::TreePop();
    }
  }
  ImGui::PopID();

  return selectedNode;
}

bool editTransformUI(const mat4& view, const mat4& projection, mat4& matrix)
{
  static ImGuizmo::OPERATION gizmoOperation(ImGuizmo::TRANSLATE);

  ImGui::Text("Transforms:");

  if (ImGui::RadioButton("Translate", gizmoOperation == ImGuizmo::TRANSLATE))
    gizmoOperation = ImGuizmo::TRANSLATE;

  if (ImGui::RadioButton("Rotate", gizmoOperation == ImGuizmo::ROTATE))
    gizmoOperation = ImGuizmo::ROTATE;

  ImGuiIO& io = ImGui::GetIO();
  ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
  return ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), gizmoOperation, ImGuizmo::WORLD, glm::value_ptr(matrix));
}

int* textureToEdit = nullptr;

bool editMaterialUI(Scene& scene, MeshData& meshData, int node, int& outUpdateMaterialIndex, TextureCache& textureCache)
{
  if (!scene.materialForNode.contains(node))
    return false;

  const uint32_t matIdx = scene.materialForNode[node];
  Material& material    = meshData.materials[matIdx];

  bool updated = false;

  updated |= ImGui::ColorEdit3("Emissive color", glm::value_ptr(material.emissiveFactor));
  updated |= ImGui::ColorEdit3("Base color", glm::value_ptr(material.baseColorFactor));

  const char* ImagesGalleryName = "Images Gallery";

  auto drawTextureUI = [&textureCache, ImagesGalleryName](const char* name, int& texture) {
    if (texture == -1)
      return;
    ImGui::Text(name);
    ImGui::Image(textureCache[texture].indexAsVoid(), ImVec2(512, 512), ImVec2(0, 1), ImVec2(1, 0));
    if (ImGui::IsItemClicked()) {
      textureToEdit = &texture;
      ImGui::OpenPopup(ImagesGalleryName);
    }
  };

  drawTextureUI("Base texture:", material.baseColorTexture);
  drawTextureUI("Emissive texture:", material.emissiveTexture);
  drawTextureUI("Normal texture:", material.normalTexture);
  drawTextureUI("Opacity texture:", material.opacityTexture);

  if (const ImGuiViewport* v = ImGui::GetMainViewport()) {
    ImGui::SetNextWindowPos(ImVec2(v->WorkSize.x * 0.5f, v->WorkSize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  }
  if (ImGui::BeginPopupModal(ImagesGalleryName, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    for (int i = 0; i != textureCache.size(); i++) {
      if (i && i % 4 != 0)
        ImGui::SameLine();
      ImGui::Image(textureCache[i].indexAsVoid(), ImVec2(256, 256), ImVec2(0, 1), ImVec2(1, 0));
      if (ImGui::IsItemHovered()) {
        ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), 0x66ffffff);
      }
      if (ImGui::IsItemClicked()) {
        *textureToEdit = i;
        updated        = true;
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::EndPopup();
  }

  if (updated) {
    outUpdateMaterialIndex = static_cast<int>(matIdx);
  }

  return updated;
}

void editNodeUI(
    Scene& scene, MeshData& meshData, const mat4& view, const mat4 proj, int node, int& outUpdateMaterialIndex, TextureCache& textureCache)
{
  ImGuizmo::SetOrthographic(false);
  ImGuizmo::BeginFrame();

  std::string name  = getNodeName(scene, node);
  std::string label = name.empty() ? (std::string("Node") + std::to_string(node)) : name;
  label             = "Node: " + label;

  if (const ImGuiViewport* v = ImGui::GetMainViewport()) {
    ImGui::SetNextWindowPos(ImVec2(v->WorkSize.x * 0.83f, 200));
    ImGui::SetNextWindowSize(ImVec2(v->WorkSize.x / 6, v->WorkSize.y - 210));
  }
  ImGui::Begin("Editor", nullptr, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
  if (!name.empty())
    ImGui::Text("%s", label.c_str());

  if (node >= 0) {
    ImGui::Separator();
    ImGuizmo::SetID(1);

    glm::mat4 globalTransform = scene.globalTransform[node]; // fetch global transform
    glm::mat4 srcTransform    = globalTransform;
    glm::mat4 localTransform  = scene.localTransform[node];

    if (editTransformUI(view, proj, globalTransform)) {
      glm::mat4 deltaTransform   = glm::inverse(srcTransform) * globalTransform; // calculate delta for edited global transform
      scene.localTransform[node] = localTransform * deltaTransform;              // modify local transform
      markAsChanged(scene, node);
    }

    ImGui::Separator();
    ImGui::Text("%s", "Material");

    editMaterialUI(scene, meshData, node, outUpdateMaterialIndex, textureCache);
  }
  ImGui::End();
}

const char* fileNameCachedMeshes = ".cache/ch08_orrery.meshes";
const char* fileNameCachedMaterials = ".cache/ch08_orrery.materials";
const char* fileNameCachedHierarchy = ".cache/ch08_orrery.scene";

int main()
{
  if (!isMeshDataValid(fileNameCachedMeshes) || !isMeshHierarchyValid(fileNameCachedHierarchy) ||
      !isMeshMaterialsValid(fileNameCachedMaterials)) {
    printf("No cached mesh data found. Precaching...\n\n");

    MeshData meshData;
    Scene ourScene;

    loadMeshFile("data/meshes/orrery/scene.gltf", meshData, ourScene, true);

    saveMeshData(fileNameCachedMeshes, meshData);
    saveMeshDataMaterials(fileNameCachedMaterials, meshData);
    saveScene(fileNameCachedHierarchy, ourScene);
  }

  MeshData meshData;
  const MeshFileHeader header = loadMeshData(fileNameCachedMeshes, meshData);
  loadMeshDataMaterials(fileNameCachedMaterials, meshData);

  Scene scene;
  loadScene(fileNameCachedHierarchy, scene);

  VulkanApp app({
      .initialCameraPos    = vec3(-0.875f, 1.257f, 1.070f),
      .initialCameraTarget = vec3(0, -0.6f, 0),
  });

  LineCanvas3D canvas3d;

  app.positioner_.maxSpeed_ = 1.0f;

  std::unique_ptr<lvk::IContext> ctx(app.ctx_.get());

  bool drawWireframe = false;
  int selectedNode = -1;

  {
    const VKMesh mesh(ctx, header, meshData, scene, app.getDepthFormat());

    app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
      const mat4 proj = glm::perspective(45.0f, aspectRatio, 0.01f, 100.0f);

      const lvk::RenderPass renderPass = {
        .color = { { .loadOp = lvk::LoadOp_Clear, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
        .depth = { .loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f }
      };

      const lvk::Framebuffer framebuffer = {
        .color        = { { .texture = ctx->getCurrentSwapchainTexture() } },
        .depthStencil = { .texture = app.getDepthTexture() },
      };

      const mat4 view = app.camera_.getViewMatrix();

      int updateMaterialIndex = -1;

      lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
      {
        buf.cmdBeginRendering(renderPass, framebuffer);
        buf.cmdPushDebugGroupLabel("Mesh", 0xff0000ff);
        mesh.draw(*ctx.get(), buf, header, view, proj, {}, drawWireframe);
        buf.cmdPopDebugGroupLabel();
        app.drawGrid(buf, proj, vec3(0, -1.0f, 0));
        app.imgui_->beginFrame(framebuffer);
        app.drawFPS();
        app.drawMemo();

        canvas3d.clear();
        canvas3d.setMatrix(proj * view);
        // render all bounding boxes (red)
        for (auto& p : scene.meshForNode) {
          const BoundingBox box = meshData.boxes[p.second];
          canvas3d.box(scene.globalTransform[p.first], box, vec4(1, 0, 0, 1));
        }

        // render UI
        {
          const ImGuiViewport* v = ImGui::GetMainViewport();
          ImGui::SetNextWindowPos(ImVec2(10, 200));
          ImGui::SetNextWindowSize(ImVec2(v->WorkSize.x / 6, v->WorkSize.y - 210));
          ImGui::Begin(
              "Scene graph", nullptr, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
          ImGui::Checkbox("Draw wireframe", &drawWireframe);
          ImGui::Separator();
          const int node = renderSceneTreeUI(scene, 0, selectedNode);
          if (node > -1) {
            selectedNode = node;
          }
          ImGui::End();

          editNodeUI(scene, meshData, view, proj, selectedNode, updateMaterialIndex, mesh.textureCache_);

          // render one selected bounding box (green)
          if (selectedNode > -1 && scene.hierarchy[selectedNode].firstChild < 0) {
            const uint32_t meshId = scene.meshForNode[selectedNode];
            const BoundingBox box = meshData.boxes[meshId];
            canvas3d.box(scene.globalTransform[selectedNode], box, vec4(0, 1, 0, 1));
          }
        }

        canvas3d.render(*ctx.get(), framebuffer, buf, width, height);

        app.imgui_->endFrame(buf);

        buf.cmdEndRendering();
      }
      ctx->submit(buf, ctx->getCurrentSwapchainTexture());

      if (recalculateGlobalTransforms(scene)) {
        mesh.updateGlobalTransforms(scene.globalTransform.data(), scene.globalTransform.size());
      }
      if (updateMaterialIndex > -1) {
        mesh.updateMaterial(meshData.materials.data(), updateMaterialIndex);
      }
    });
  }

  ctx.release();

  return 0;
}
