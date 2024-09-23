#include "shared/VulkanApp.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "shared/LineCanvas.h"
#include "shared/Scene/MergeUtil.h"
#include "shared/Scene/Scene.h"
#include "shared/Scene/VtxData.h"

#include "Chapter08/SceneUtils.h"
#include "Chapter08/VKMesh08.h"

int renderSceneTreeUI(const Scene& scene, int node, int selectedNode)
{
  const std::string name  = getNodeName(scene, node);
  const std::string label = name.empty() ? (std::string("Node") + std::to_string(node)) : name;

  const bool isLeaf        = scene.hierarchy_[node].firstChild_ < 0;
  ImGuiTreeNodeFlags flags = isLeaf ? ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet : 0;
  if (node == selectedNode) {
    flags |= ImGuiTreeNodeFlags_Selected;
  }

  ImVec4 color = isLeaf ? ImVec4(0, 1, 0, 1) : ImVec4(1, 1, 1, 1);

  // open some interesting nodes by default
  if (name == "NewRoot") {
    flags |= ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;
    color = ImVec4(0.9f, 0.6f, 0.6f, 1);
  }

  ImGui::PushStyleColor(ImGuiCol_Text, color);
  const bool isOpened = ImGui::TreeNodeEx(&scene.hierarchy_[node], flags, "%s", label.c_str());
  ImGui::PopStyleColor();

  ImGui::PushID(node);
  {
    if (ImGui::IsItemHovered() && isLeaf) {
      printf("Selected node: %d (%s)\n", node, label.c_str());
      selectedNode = node;
    }

    if (isOpened) {
      for (int ch = scene.hierarchy_[node].firstChild_; ch != -1; ch = scene.hierarchy_[ch].nextSibling_) {
        if (int subNode = renderSceneTreeUI(scene, ch, selectedNode); subNode > -1)
          selectedNode = subNode;
      }
      ImGui::TreePop();
    }
  }
  ImGui::PopID();

  return selectedNode;
}

const char* fileNameCachedMeshes    = ".cache/ch08_bistro.meshes";
const char* fileNameCachedMaterials = ".cache/ch08_bistro.materials";
const char* fileNameCachedHierarchy = ".cache/ch08_bistro.scene";

int main()
{
  if (!isMeshDataValid(fileNameCachedMeshes) || !isMeshHierarchyValid(fileNameCachedHierarchy) ||
      !isMeshMaterialsValid(fileNameCachedMaterials)) {
    printf("No cached mesh data found. Precaching...\n\n");

    MeshData meshData_Exterior;
    MeshData meshData_Interior;
    Scene ourScene_Exterior;
    Scene ourScene_Interior;

    // don't generate LODs because meshoptimizer fails on the Bistro mesh
    loadMeshFile("deps/src/bistro/Exterior/exterior.obj", meshData_Exterior, ourScene_Exterior, false);
    loadMeshFile("deps/src/bistro/Interior/interior.obj", meshData_Interior, ourScene_Interior, false);

    // merge some meshes
    printf("[Unmerged] scene items: %d\n", (int)ourScene_Exterior.hierarchy_.size());
    mergeNodesWithMaterial(ourScene_Exterior, meshData_Exterior, "Foliage_Linde_Tree_Large_Orange_Leaves");
    printf("[Merged orange leaves] scene items: %d\n", (int)ourScene_Exterior.hierarchy_.size());
    mergeNodesWithMaterial(ourScene_Exterior, meshData_Exterior, "Foliage_Linde_Tree_Large_Green_Leaves");
    printf("[Merged green leaves]  scene items: %d\n", (int)ourScene_Exterior.hierarchy_.size());
    mergeNodesWithMaterial(ourScene_Exterior, meshData_Exterior, "Foliage_Linde_Tree_Large_Trunk");
    printf("[Merged trunk]  scene items: %d\n", (int)ourScene_Exterior.hierarchy_.size());

    // merge everything into one big scene
    MeshData meshData;
    Scene ourScene;

    mergeScenes(
        ourScene,
        {
            &ourScene_Exterior,
            &ourScene_Interior,
        },
        {},
        {
            static_cast<uint32_t>(meshData_Exterior.meshes.size()),
            static_cast<uint32_t>(meshData_Interior.meshes.size()),
        });
    mergeMeshData(meshData, { &meshData_Exterior, &meshData_Interior });
    mergeMaterialLists(
        {
            &meshData_Exterior.materials,
            &meshData_Interior.materials,
        },
        {
            &meshData_Exterior.textureFiles,
            &meshData_Interior.textureFiles,
        },
        meshData.materials, meshData.textureFiles);

    recalculateBoundingBoxes(meshData);

    saveMeshData(fileNameCachedMeshes, meshData);
    saveMeshDataMaterials(fileNameCachedMaterials, meshData);
    saveScene(fileNameCachedHierarchy, ourScene);
  }

  MeshData meshData;
  const MeshFileHeader header = loadMeshData(fileNameCachedMeshes, meshData);
  loadMeshDataMaterials(fileNameCachedMaterials, meshData);

  Scene scene;
  loadScene(fileNameCachedHierarchy, scene);

  scene.localTransform_[0] = glm::scale(vec3(0.01f)); // scale the Bistro
  markAsChanged(scene, 0);

  VulkanApp app({
      .initialCameraPos    = vec3(-19.261f, 8.465f, -7.317f),
      .initialCameraTarget = vec3(0, +2.5f, 0),
  });

  LineCanvas3D canvas3d;

  app.positioner_.maxSpeed_ = 1.5f;

  std::unique_ptr<lvk::IContext> ctx(app.ctx_.get());

  lvk::Holder<lvk::TextureHandle> texSkybox           = loadTexture(ctx, "data/immenstadter_horn_2k_prefilter.ktx", lvk::TextureType_Cube);
  lvk::Holder<lvk::TextureHandle> texSkyboxIrradiance = loadTexture(ctx, "data/immenstadter_horn_2k_irradiance.ktx", lvk::TextureType_Cube);
  lvk::Holder<lvk::ShaderModuleHandle> vertSkybox     = loadShaderModule(ctx, "Chapter08/02_SceneGraph/src/skybox.vert");
  lvk::Holder<lvk::ShaderModuleHandle> fragSkybox     = loadShaderModule(ctx, "Chapter08/02_SceneGraph/src/skybox.frag");
  lvk::Holder<lvk::RenderPipelineHandle> pipelineSkybox = ctx->createRenderPipeline({
      .smVert      = vertSkybox,
      .smFrag      = fragSkybox,
      .color       = { { .format = ctx->getSwapchainFormat() } },
      .depthFormat = app.getDepthFormat(),
  });

  bool drawWireframe     = false;
  bool drawBoundingBoxes = false;
  int selectedNode       = -1;

  {
    const VKMesh mesh(ctx, header, meshData, scene, app.getDepthFormat());

    app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
      const mat4 proj = glm::perspective(45.0f, aspectRatio, 0.01f, 1000.0f);

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
        {
          buf.cmdPushDebugGroupLabel("Skybox", 0xff0000ff);
          buf.cmdBindRenderPipeline(pipelineSkybox);
          const struct {
            mat4 view;
            mat4 proj;
            uint32_t texSkybox;
          } pc = {
            .view      = view,
            .proj      = proj,
            .texSkybox = texSkybox.index(),
          };
          buf.cmdPushConstants(pc);
          buf.cmdDraw(36);
          buf.cmdPopDebugGroupLabel();
        }
        {
          buf.cmdPushDebugGroupLabel("Mesh", 0xff0000ff);
          mesh.draw(*ctx.get(), buf, header, view, proj, texSkyboxIrradiance, drawWireframe);
          buf.cmdPopDebugGroupLabel();
        }

        app.drawGrid(buf, proj, vec3(0, -1.0f, 0));
        app.imgui_->beginFrame(framebuffer);
        app.drawFPS();
        app.drawMemo();

        canvas3d.clear();
        canvas3d.setMatrix(proj * view);
        // render all bounding boxes (red)
        if (drawBoundingBoxes) {
          for (auto& node : scene.meshes_) {
            const BoundingBox box = meshData.boxes[node.second];
            canvas3d.box(scene.globalTransform_[node.first], box, vec4(1, 0, 0, 1));
          }
        }

        // render UI
        {
          const ImGuiViewport* v = ImGui::GetMainViewport();
          ImGui::SetNextWindowPos(ImVec2(10, 200));
          ImGui::SetNextWindowSize(ImVec2(v->WorkSize.x / 6, v->WorkSize.y - 210));
          ImGui::Begin(
              "Scene graph", nullptr, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
          ImGui::Checkbox("Draw wireframe", &drawWireframe);
          ImGui::Checkbox("Draw bounding boxes", &drawBoundingBoxes);
          ImGui::Separator();
          const int node = renderSceneTreeUI(scene, 0, selectedNode);
          if (node > -1) {
            selectedNode = node;
          }
          ImGui::End();
          // render one selected bounding box (green)
          if (selectedNode > -1 && scene.hierarchy_[selectedNode].firstChild_ < 0) {
            const uint32_t meshId = scene.meshes_[selectedNode];
            const BoundingBox box = meshData.boxes[meshId];
            canvas3d.box(scene.globalTransform_[selectedNode], box, vec4(0, 1, 0, 1));
          }
        }

        canvas3d.render(*ctx.get(), framebuffer, buf, width, height);

        app.imgui_->endFrame(buf);

        buf.cmdEndRendering();
      }
      ctx->submit(buf, ctx->getCurrentSwapchainTexture());

      if (recalculateGlobalTransforms(scene)) {
        mesh.updateGlobalTransforms(scene.globalTransform_.data(), scene.globalTransform_.size());
      }
      if (updateMaterialIndex > -1) {
        mesh.updateMaterial(meshData.materials.data(), updateMaterialIndex);
      }
    });
  }

  ctx.release();

  return 0;
}
