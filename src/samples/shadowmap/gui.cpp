#include "shadowmap_render.h"

#include "../../render/render_gui.h"

void SimpleShadowmapRender::SetupGUIElements()
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  {
    ImGui::Begin("Simple render settings");

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::NewLine();

    ImGui::Checkbox("enable reflective shadow map", &m_Enabled);
    ImGui::SliderFloat("reflection intensity", &m_Intensity, 0.01f, 0.2f);
    ImGui::SliderFloat("importance sampling radius", &m_Radius, 0.15f, 0.3f);

    ImGui::End();
  }

  // Rendering
  ImGui::Render();
}
