#include "Precompiled.h"
#include "ALManager.h"
#include "ALSoundNode.h"
#include "Graphics/Camera/Camera.h"
#include "Utilities/TimeStep.h"
#include "Scene/Component/SoundComponent.h"
#include "Scene/Scene.h"
#include "Maths/Transform.h"

#include <imgui/imgui.h>
#include <entt/entity/registry.hpp>

namespace Lumos
{
    namespace Audio
    {
        ALManager::ALManager(int numChannels)
            : m_Context(nullptr)
            , m_Device(nullptr)
            , m_NumChannels(numChannels)
        {
            m_DebugName = "OpenAL Audio";
        }

        ALManager::~ALManager()
        {
            alcDestroyContext(m_Context);
            alcCloseDevice(m_Device);
        }

        bool ALManager::OnInit()
        {
            LUMOS_PROFILE_FUNCTION();
            m_Device  = alcOpenDevice(nullptr);
            m_Context = alcCreateContext(m_Device, nullptr);

            if(!m_Device)
            {
                LINFO("Failed to Initialise AudioManager! (No valid device!)");
                return false;
            }

            alcMakeContextCurrent(m_Context);
            alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);

            LINFO("Initialised AudioManager - %s", alcGetString(m_Device, ALC_DEVICE_SPECIFIER));
            return true;
        }

        void ALManager::OnUpdate(const TimeStep& dt, Scene* scene)
        {
            LUMOS_PROFILE_FUNCTION();
            m_LatestNodeCount = 0;
            auto& registry    = scene->GetRegistry();
            auto listenerView = registry.view<Listener, Maths::Transform>();
            if(listenerView.size_hint() > 0)
            {
                auto& listenerTransform = registry.get<Maths::Transform>(listenerView.front());
                UpdateListener(listenerTransform);
            }

            auto soundsView = registry.view<SoundComponent, Maths::Transform>();

            for(auto entity : soundsView)
            {
                auto soundNode = soundsView.get<SoundComponent>(entity).GetSoundNode();
                soundNode->SetPosition(soundsView.get<Maths::Transform>(entity).GetWorldPosition());
                soundNode->OnUpdate((float)dt.GetMillis());
                m_LatestNodeCount++;
            }
        }

        void ALManager::UpdateListener(Scene* scene)
        {
            auto& registry    = scene->GetRegistry();
            auto listenerView = registry.view<Listener, Maths::Transform>();
            if(listenerView.size_hint() > 0)
            {
                auto& listenerTransform = registry.get<Maths::Transform>(listenerView.front());
                UpdateListener(listenerTransform);
            }
        }

        // Pass Cameras transform
        void ALManager::UpdateListener(Maths::Transform& listenerTransform)
        {
            LUMOS_PROFILE_FUNCTION();
            {
                Vec3 worldPos = listenerTransform.GetWorldPosition();
                Vec3 velocity = Vec3(0.0f); // TODO: m_Listener->GetVelocity();

                ALfloat direction[6];

                Quat orientation = listenerTransform.GetWorldOrientation();

                direction[0] = -2 * (orientation.w * orientation.y + orientation.x * orientation.z);
                direction[1] = 2 * (orientation.x * orientation.w - orientation.z * orientation.y);
                direction[2] = 2 * (orientation.x * orientation.x + orientation.y * orientation.y) - 1;
                direction[3] = 2 * (orientation.x * orientation.y - orientation.w * orientation.z);
                direction[4] = 1 - 2 * (orientation.x * orientation.x + orientation.z * orientation.z);
                direction[5] = 2 * (orientation.w * orientation.x + orientation.y * orientation.z);

                alListenerfv(AL_POSITION, reinterpret_cast<float*>(&worldPos));
                alListenerfv(AL_VELOCITY, reinterpret_cast<float*>(&velocity));
                alListenerfv(AL_ORIENTATION, direction);
            }
        }

        void ALManager::OnImGui()
        {
            LUMOS_PROFILE_FUNCTION();
            ImGui::TextUnformatted("OpenAL Audio");

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
            ImGui::Columns(2);
            ImGui::Separator();

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Number Of Audio Sources");
            ImGui::NextColumn();
            ImGui::PushItemWidth(-1);
            ImGui::Text("%5.2lu", m_LatestNodeCount);
            ImGui::PopItemWidth();
            ImGui::NextColumn();

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Number Of Channels");
            ImGui::NextColumn();
            ImGui::PushItemWidth(-1);
            ImGui::Text("%5.2i", m_NumChannels);
            ImGui::PopItemWidth();
            ImGui::NextColumn();

            ImGui::Columns(1);
            ImGui::Separator();
            ImGui::PopStyleVar();
        }
    }
}
