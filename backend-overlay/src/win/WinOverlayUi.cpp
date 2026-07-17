/////////////////////////////////////////////////////////
// File: WinOverlayUi.cpp
// Date: 2026-06-21
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements ImGui state and rendering for
//              Windows overlay notifications.
/////////////////////////////////////////////////////////

#include "WinOverlayUi.h"

#include <algorithm>
#include <utility>

/////////////////////////////////////////////////////////////////////

WinOverlayUi::WinOverlayUi()
{
    m_visible = false;
    m_fadingOut = false;
    m_alpha = 0.0f;
    m_slide = 40.0f;
    m_lastFrameMs = 0;
    m_iconGeneration = 0;
}

WinOverlayUi::~WinOverlayUi()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void WinOverlayUi::Update(const WinOverlayNotification* notification)
{
    const uint64_t now = GetTickCount64();

    if (!m_visible && !m_fadingOut)
    {
        if (notification)
        {
            m_notification = *notification;
            m_visible = true;
            m_fadingOut = false;
            m_alpha = 0.0f;
            m_slide = 40.0f;
            ++m_iconGeneration;
        }
    }

    if (!m_visible && !m_fadingOut)
    {
        return;
    }

    const float delta = m_lastFrameMs == 0 ? 0.0f : (std::min)(0.1f, static_cast<float>(now - m_lastFrameMs) / 1000.0f);
    m_lastFrameMs = now;
    constexpr float margin = 40.0f;
    constexpr float width = 480.0f;
    constexpr float height = 100.0f;
    constexpr float fadeSpeed = 4.0f;
    constexpr float slideSpeed = 200.0f;
    constexpr float exitSlideSpeed = 1200.0f;

    // Begin exit slide after notification duration expires
    if (!m_fadingOut && now - m_notification.shownAtMs >= m_notification.durationMs)
    {
        m_fadingOut = true;
        m_alpha = 1.0f;
    }
    if (m_fadingOut)
    {
        if (m_notification.exitAnimation == OverlayNotificationExitAnimation::FadeOut)
        {
            m_alpha = (std::max)(0.0f, m_alpha - delta * fadeSpeed);
            m_slide = (std::min)(40.0f, m_slide + delta * slideSpeed);
            if (m_alpha == 0.0f)
            {
                m_visible = false;
                m_fadingOut = false;
                m_slide = 40.0f;
            }
        }
        else
        {
            const bool centeredPosition = m_notification.position == OverlayNotificationPosition::TopCenter || m_notification.position == OverlayNotificationPosition::BottomCenter;
            const float exitSlideDistance = centeredPosition
                ? height + margin
                : width + margin;
            const float exitSlideProgress = exitSlideDistance > 0.0f ? (std::min)(1.0f, m_slide / (exitSlideDistance * 0.10f)) : 1.0f;
            const float maxSlideSpeed = centeredPosition ? exitSlideSpeed * 0.5f : exitSlideSpeed;
            const float currentSlideSpeed = slideSpeed + (maxSlideSpeed - slideSpeed) * exitSlideProgress * exitSlideProgress;
            m_alpha = 1.0f;
            m_slide = (std::min)(exitSlideDistance, m_slide + delta * currentSlideSpeed);
            if (m_slide >= exitSlideDistance)
            {
                m_visible = false;
                m_fadingOut = false;
                m_alpha = 0.0f;
                m_slide = 40.0f;
            }
        }
    }
    else
    {
        m_alpha = (std::min)(1.0f, m_alpha + delta * fadeSpeed);
        m_slide = (std::max)(0.0f, m_slide - delta * slideSpeed);
    }
}

/////////////////////////////////////////////////////////////////////

void WinOverlayUi::Draw(uint32_t framebufferWidth, uint32_t framebufferHeight, ImTextureID iconTexture)
{
    if (!m_visible && !m_fadingOut)
    {
        return;
    }

    constexpr float margin = 40.0f;
    constexpr float width = 480.0f;
    constexpr float height = 100.0f;
    constexpr float iconSize = 84.0f;
    float x = static_cast<float>(framebufferWidth) - width - margin;
    float y = static_cast<float>(framebufferHeight) - height - margin;
    float slideX = m_slide;
    float slideY = 0.0f;

    // Position and slide direction follow user-selected notification corner
    switch (m_notification.position)
    {
        case OverlayNotificationPosition::TopLeft:
            x = margin;
            y = margin;
            slideX = -m_slide;
            break;
        case OverlayNotificationPosition::TopCenter:
            x = (static_cast<float>(framebufferWidth) - width) * 0.5f;
            y = margin;
            slideX = 0.0f;
            slideY = -m_slide;
            break;
        case OverlayNotificationPosition::TopRight:
            x = static_cast<float>(framebufferWidth) - width - margin;
            y = margin;
            break;
        case OverlayNotificationPosition::BottomLeft:
            x = margin;
            slideX = -m_slide;
            break;
        case OverlayNotificationPosition::BottomCenter:
            x = (static_cast<float>(framebufferWidth) - width) * 0.5f;
            slideX = 0.0f;
            slideY = m_slide;
            break;
        case OverlayNotificationPosition::BottomRight:
            break;
    }

    ImGui::SetNextWindowPos(ImVec2(x + slideX, y + slideY));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::SetNextWindowBgAlpha(0.82f * m_alpha);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, m_alpha));
    if (ImGui::Begin("##lymalink_overlay", nullptr, flags))
    {
        const ImVec2 iconPos = ImGui::GetCursorScreenPos();
        if (iconTexture)
        {
            ImGui::Image(iconTexture, ImVec2(iconSize, iconSize));
        }
        else
        {
            ImGui::GetWindowDrawList()->AddRectFilled(iconPos, ImVec2(iconPos.x + iconSize, iconPos.y + iconSize), ImGui::GetColorU32(ImVec4(0.18f, 0.54f, 0.72f, 0.75f * m_alpha)), 6.0f);
            ImGui::Dummy(ImVec2(iconSize, iconSize));
        }
        ImGui::SameLine(0.0f, 10.0f);
        ImGui::BeginGroup();
        ImGui::TextWrapped("%s", m_notification.title.c_str());
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, m_alpha));
        ImGui::TextWrapped("%s", m_notification.description.c_str());
        ImGui::PopStyleColor();
        ImGui::EndGroup();
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}
