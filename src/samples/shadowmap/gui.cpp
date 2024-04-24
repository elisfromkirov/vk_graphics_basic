#include "shadowmap_render.h"

#include "../../render/render_gui.h"

void SimpleShadowmapRender::SetupGUIElements()
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  {
//    ImGui::ShowDemoWindow();
    ImGui::Begin("Simple render settings");

    const char *names[] = {
      "off",
      "exposure",
      "reinhard"
    };

    ImGui::Combo("tone mapping", reinterpret_cast<int *>(&m_ToneMappingMode), names, std::size(names));
    ImGui::SliderFloat("light intensity", &m_Intensity, 0.5f, 5.0f);
    ImGui::SliderFloat("gamma", &m_Gamma, 0.5f, 5.0f);
    ImGui::SliderFloat("exposure", &m_Exposure, 0.5f, 5.0f);

    ImGui::NewLine();

    ImGui::ColorEdit3("Meshes base color", m_uniforms.baseColor.M, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoInputs);
    ImGui::SliderFloat3("Light source position", m_uniforms.lightPos.M, -10.f, 10.f);

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::NewLine();

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),"Press 'B' to recompile and reload shaders");
    ImGui::End();
  }

  // Rendering
  ImGui::Render();
}
