#include "Precompiled.h"
#include "Scene.h"
#include "Core/OS/Input.h"
#include "Core/Application.h"
#include "Graphics/RHI/GraphicsContext.h"
#include "Graphics/Renderers/SceneRenderer.h"
#include "Graphics/Camera/Camera.h"
#include "Graphics/Sprite.h"
#include "Graphics/AnimatedSprite.h"
#include "Utilities/TimeStep.h"
#include "Audio/AudioManager.h"
#include "Physics/LumosPhysicsEngine/LumosPhysicsEngine.h"
#include "Physics/LumosPhysicsEngine/CollisionShapes/SphereCollisionShape.h"
#include "Physics/LumosPhysicsEngine/CollisionShapes/CuboidCollisionShape.h"
#include "Physics/LumosPhysicsEngine/CollisionShapes/PyramidCollisionShape.h"
#include "Physics/LumosPhysicsEngine/CollisionShapes/HullCollisionShape.h"
#include "Physics/LumosPhysicsEngine/CollisionShapes/CapsuleCollisionShape.h"

#include "Events/Event.h"
#include "Events/ApplicationEvent.h"

#include "Maths/Transform.h"
#include "Maths/Random.h"
#include "Core/OS/FileSystem.h"
#include "Scene/Component/Components.h"
#include "Scripting/Lua/LuaScriptComponent.h"
#include "Scripting/Lua/LuaManager.h"
#include "Graphics/MeshFactory.h"
#include "Graphics/Light.h"
#include "Graphics/Model.h"
#include "Graphics/ParticleManager.h"
#include "Graphics/Environment.h"
#include "Scene/EntityManager.h"
#include "Scene/Component/SoundComponent.h"
#include "Scene/Component/ModelComponent.h"
#include "SceneGraph.h"
#include "Serialisation/SerialisationImplementation.h"

#include "Scene/Component/SoundComponent.h"
#include "Scene/Component/TextureMatrixComponent.h"
#include "Scene/Component/RigidBody2DComponent.h"
#include "Scene/Component/RigidBody3DComponent.h"
#include "Scene/Component/AIComponent.h"

#include <cereal/types/polymorphic.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <entt/entity/registry.hpp>
#include <entt/entity/snapshot.hpp>
#include <sol/sol.hpp> //For deleting sol::basic_environment<sol::basic_reference<false>>
#include <fstream>
#include <ostream>

CEREAL_REGISTER_TYPE(Lumos::SphereCollisionShape);
CEREAL_REGISTER_TYPE(Lumos::CuboidCollisionShape);
CEREAL_REGISTER_TYPE(Lumos::PyramidCollisionShape);
CEREAL_REGISTER_TYPE(Lumos::HullCollisionShape);
CEREAL_REGISTER_TYPE(Lumos::CapsuleCollisionShape);

CEREAL_REGISTER_POLYMORPHIC_RELATION(Lumos::CollisionShape, Lumos::SphereCollisionShape);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Lumos::CollisionShape, Lumos::CuboidCollisionShape);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Lumos::CollisionShape, Lumos::PyramidCollisionShape);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Lumos::CollisionShape, Lumos::HullCollisionShape);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Lumos::CollisionShape, Lumos::CapsuleCollisionShape);

#define MIN_SCENE_VERSION 24

#if MIN_SCENE_VERSION < 21
#pragma warning(push, 0)
// Legacy version of entt snapshot loaded to load older scene versions saved before updating entt
namespace entt
{

    /**
     * @brief Utility class to restore a snapshot as a whole.
     *
     * A snapshot loader requires that the destination registry be empty and loads
     * all the data at once while keeping intact the identifiers that the entities
     * originally had.<br/>
     * An example of use is the implementation of a save/restore utility.
     *
     * @tparam Entity A valid entity type (see entt_traits for more details).
     */
    template <typename Entity>
    class basic_snapshot_loader_legacy
    {
        /*! @brief A registry is allowed to create snapshot loaders. */
        friend class basic_registry<Entity>;

        using traits_type = entt_traits<Entity>;

        template <typename Type, typename Archive>
        void assign(Archive& archive) const
        {
            typename traits_type::entity_type length {};
            archive(length);

            entity_type entt {};

            if constexpr(std::is_empty_v<Type>)
            {
                while(length--)
                {
                    archive(entt);
                    const auto entity = reg->valid(entt) ? entt : reg->create(entt);
                    // ENTT_ASSERT(entity == entt);
                    reg->template emplace<Type>(entity);
                }
            }
            else
            {
                Type instance {};

                while(length--)
                {
                    archive(entt, instance);
                    const auto entity = reg->valid(entt) ? entt : reg->create(entt);
                    // ENTT_ASSERT(entity == entt);
                    reg->template emplace<Type>(entity, std::move(instance));
                }
            }
        }

    public:
        /*! @brief Underlying entity identifier. */
        using entity_type = Entity;

        /**
         * @brief Constructs an instance that is bound to a given registry.
         * @param source A valid reference to a registry.
         */
        basic_snapshot_loader_legacy(basic_registry<entity_type>& source) noexcept
            : reg { &source }
        {
            // restoring a snapshot as a whole requires a clean registry
            // ENTT_ASSERT(reg->empty());
        }

        /*! @brief Default move constructor. */
        basic_snapshot_loader_legacy(basic_snapshot_loader_legacy&&) = default;

        /*! @brief Default move assignment operator. @return This loader. */
        basic_snapshot_loader_legacy& operator=(basic_snapshot_loader_legacy&&) = default;

        /**
         * @brief Restores entities that were in use during serialization.
         *
         * This function restores the entities that were in use during serialization
         * and gives them the versions they originally had.
         *
         * @tparam Archive Type of input archive.
         * @param archive A valid reference to an input archive.
         * @return A valid loader to continue restoring data.
         */
        template <typename Archive>
        const basic_snapshot_loader_legacy& entities(Archive& archive) const
        {
            typename traits_type::entity_type length {};

            archive(length);
            std::vector<entity_type> all(length);

            for(decltype(length) pos {}; pos < length; ++pos)
            {
                archive(all[pos]);
            }

            reg->assign(all.cbegin(), all.cend(), 0);

            return *this;
        }

        /**
         * @brief Restores components and assigns them to the right entities.
         *
         * The template parameter list must be exactly the same used during
         * serialization. In the event that the entity to which the component is
         * assigned doesn't exist yet, the loader will take care to create it with
         * the version it originally had.
         *
         * @tparam Component Types of components to restore.
         * @tparam Archive Type of input archive.
         * @param archive A valid reference to an input archive.
         * @return A valid loader to continue restoring data.
         */
        template <typename... Component, typename Archive>
        const basic_snapshot_loader_legacy& component(Archive& archive) const
        {
            (assign<Component>(archive), ...);
            return *this;
        }

        /**
         * @brief Destroys those entities that have no components.
         *
         * In case all the entities were serialized but only part of the components
         * was saved, it could happen that some of the entities have no components
         * once restored.<br/>
         * This functions helps to identify and destroy those entities.
         *
         * @return A valid loader to continue restoring data.
         */
        const basic_snapshot_loader_legacy& orphans() const
        {
            /*        reg->orphans([this](const auto entt) {
                        reg->destroy(entt);
                        });*/
            LWARN("May need to fix this - basic_snapshot_loader_legacy::orphans()");

            return *this;
        }

    private:
        basic_registry<entity_type>* reg;
    };
}
#pragma warning(pop)
#endif

namespace Lumos
{
    Scene::Scene(const std::string& name)
        : m_SceneName(name)
        , m_ScreenWidth(0)
        , m_ScreenHeight(0)
    {
        m_EntityManager = CreateUniquePtr<EntityManager>(this);
        m_EntityManager->AddDependency<RigidBody3DComponent, Maths::Transform>();
        m_EntityManager->AddDependency<RigidBody2DComponent, Maths::Transform>();
        m_EntityManager->AddDependency<Camera, Maths::Transform>();
        m_EntityManager->AddDependency<Graphics::ModelComponent, Maths::Transform>();
        m_EntityManager->AddDependency<Graphics::Light, Maths::Transform>();
        m_EntityManager->AddDependency<Graphics::Sprite, Maths::Transform>();
        m_EntityManager->AddDependency<Graphics::AnimatedSprite, Maths::Transform>();
        m_EntityManager->AddDependency<TextComponent, Maths::Transform>();
        m_EntityManager->AddDependency<ParticleEmitter, Maths::Transform>();

        m_SceneGraph = CreateUniquePtr<SceneGraph>();
        m_SceneGraph->Init(m_EntityManager->GetRegistry());
    }

    Scene::~Scene()
    {
        m_EntityManager->Clear();
    }

    entt::registry& Scene::GetRegistry()
    {
        return m_EntityManager->GetRegistry();
    }

    void Scene::OnInit()
    {
        LUMOS_PROFILE_FUNCTION();

        // Physics setup
        auto physics3DSytem = Application::Get().GetSystem<LumosPhysicsEngine>();
        physics3DSytem->SetDampingFactor(m_Settings.Physics3DSettings.Dampening);
        physics3DSytem->SetIntegrationType((IntegrationType)m_Settings.Physics3DSettings.IntegrationTypeIndex);
        physics3DSytem->SetMaxUpdatesPerFrame(m_Settings.Physics3DSettings.MaxUpdatesPerFrame);
        physics3DSytem->SetPositionIterations(m_Settings.Physics3DSettings.PositionIterations);
        physics3DSytem->SetVelocityIterations(m_Settings.Physics3DSettings.VelocityIterations);
        physics3DSytem->SetBroadphaseType(BroadphaseType(m_Settings.Physics3DSettings.BroadPhaseTypeIndex));

        LuaManager::Get().OnInit(this);
    }

    void Scene::OnCleanupScene()
    {
        LUMOS_PROFILE_FUNCTION();
        m_EntityManager->Clear();
        LuaManager::Get().CollectGarbage();
    };

    void Scene::OnUpdate(const TimeStep& timeStep)
    {
        LUMOS_PROFILE_FUNCTION();
        const Vec2& mousePos = Input::Get().GetMousePosition();

        auto defaultCameraControllerView = m_EntityManager->GetEntitiesWithType<DefaultCameraController>();
        auto cameraView                  = m_EntityManager->GetEntitiesWithType<Camera>();
        Camera* camera                   = nullptr;
        if(!cameraView.Empty())
        {
            camera = &cameraView.Front().GetComponent<Camera>();
        }

        if(!defaultCameraControllerView.Empty())
        {
            auto& cameraController = defaultCameraControllerView.Front().GetComponent<DefaultCameraController>();
            auto trans             = defaultCameraControllerView.Front().TryGetComponent<Maths::Transform>();
            if(Application::Get().GetSceneActive() && trans && cameraController.GetController())
            {
                cameraController.GetController()->SetCamera(camera);
                cameraController.GetController()->HandleMouse(*trans, (float)timeStep.GetSeconds(), mousePos.x, mousePos.y);
                cameraController.GetController()->HandleKeyboard(*trans, (float)timeStep.GetSeconds());
            }
        }

        m_SceneGraph->Update(m_EntityManager->GetRegistry());

        auto animatedSpriteView = m_EntityManager->GetEntitiesWithType<Graphics::AnimatedSprite>();

        for(auto entity : animatedSpriteView)
        {
            auto& animSprite = entity.GetComponent<Graphics::AnimatedSprite>();
            animSprite.OnUpdate((float)timeStep.GetSeconds());
        }

        auto group = m_EntityManager->GetRegistry().group<Graphics::ModelComponent>(entt::get<Maths::Transform>);
        for(auto entity : group)
        {
            if(!Entity(entity, this).Active())
                continue;

            const auto& [model, trans] = group.get<Graphics::ModelComponent, Maths::Transform>(entity);

            if(!model.ModelRef)
                continue;

            const auto& meshes = model.ModelRef->GetMeshes();
            model.ModelRef->UpdateAnimation(Engine::GetTimeStep());
        }
    }

    void Scene::OnEvent(Event& e)
    {
        LUMOS_PROFILE_FUNCTION();
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(Scene::OnWindowResize));
    }

    bool Scene::OnWindowResize(WindowResizeEvent& e)
    {
        LUMOS_PROFILE_FUNCTION();
        if(!Application::Get().GetSceneActive())
            return false;

        auto cameraView = m_EntityManager->GetRegistry().view<Camera>();
        if(!cameraView.empty())
        {
            m_EntityManager->GetRegistry().get<Camera>(cameraView.front()).SetAspectRatio(static_cast<float>(e.GetWidth()) / static_cast<float>(e.GetHeight()));
        }

        return false;
    }

    void Scene::SetScreenSize(uint32_t width, uint32_t height)
    {
        m_ScreenWidth  = width;
        m_ScreenHeight = height;

        auto cameraView = m_EntityManager->GetRegistry().view<Camera>();
        if(!cameraView.empty())
        {
            m_EntityManager->GetRegistry().get<Camera>(cameraView.front()).SetAspectRatio(static_cast<float>(m_ScreenWidth) / static_cast<float>(m_ScreenHeight));
        }
    }

#define ALL_COMPONENTSV1 Maths::Transform, NameComponent, ActiveComponent, Hierarchy, Camera, LuaScriptComponent, Graphics::Model, Graphics::Light, RigidBody3DComponent, Graphics::Environment, Graphics::Sprite, RigidBody2DComponent, DefaultCameraController

#define ALL_COMPONENTSV2 ALL_COMPONENTSV1, Graphics::AnimatedSprite
#define ALL_COMPONENTSV3 ALL_COMPONENTSV2, SoundComponent
#define ALL_COMPONENTSV4 ALL_COMPONENTSV3, Listener
#define ALL_COMPONENTSV5 ALL_COMPONENTSV4, IDComponent
#define ALL_COMPONENTSV6 ALL_COMPONENTSV5, Graphics::ModelComponent
#define ALL_COMPONENTSV7 ALL_COMPONENTSV6, AxisConstraintComponent
#define ALL_COMPONENTSV8 ALL_COMPONENTSV7, TextComponent

#define ALL_COMPONENTSLISTV8 ALL_COMPONENTSV8
#define ALL_COMPONENTSENTTV8(input) get<Maths::Transform>(input).get<NameComponent>(input).get<ActiveComponent>(input).get<Hierarchy>(input).get<Camera>(input).get<LuaScriptComponent>(input).get<Graphics::Model>(input).get<Graphics::Light>(input).get<RigidBody3DComponent>(input).get<Graphics::Environment>(input).get<Graphics::Sprite>(input).get<RigidBody2DComponent>(input).get<DefaultCameraController>(input).get<Graphics::AnimatedSprite>(input).get<SoundComponent>(input).get<Listener>(input).get<IDComponent>(input).get<Graphics::ModelComponent>(input).get<AxisConstraintComponent>(input).get<TextComponent>(input)

#define ALL_COMPONENTSENTTV9(input) get<Maths::Transform>(input).get<NameComponent>(input).get<ActiveComponent>(input).get<Hierarchy>(input).get<Camera>(input).get<LuaScriptComponent>(input).get<Graphics::Model>(input).get<Graphics::Light>(input).get<RigidBody3DComponent>(input).get<Graphics::Environment>(input).get<Graphics::Sprite>(input).get<RigidBody2DComponent>(input).get<DefaultCameraController>(input).get<Graphics::AnimatedSprite>(input).get<SoundComponent>(input).get<Listener>(input).get<IDComponent>(input).get<Graphics::ModelComponent>(input).get<AxisConstraintComponent>(input).get<TextComponent>(input).get<ParticleEmitter>(input)
#define ALL_COMPONENTSENTTV10(input) get<Maths::Transform>(input).get<NameComponent>(input).get<ActiveComponent>(input).get<Hierarchy>(input).get<Camera>(input).get<LuaScriptComponent>(input).get<Graphics::Model>(input).get<Graphics::Light>(input).get<RigidBody3DComponent>(input).get<Graphics::Environment>(input).get<Graphics::Sprite>(input).get<RigidBody2DComponent>(input).get<DefaultCameraController>(input).get<Graphics::AnimatedSprite>(input).get<SoundComponent>(input).get<Listener>(input).get<IDComponent>(input).get<Graphics::ModelComponent>(input).get<AxisConstraintComponent>(input).get<TextComponent>(input).get<ParticleEmitter>(input).get<SpringConstraintComponent>(input)

    void Scene::Serialise(const std::string& filePath, bool binary)
    {
        LUMOS_PROFILE_FUNCTION();
        LINFO("Scene saved - %s", filePath.c_str());
        std::string path = filePath;
        path += m_SceneName; // StringUtilities::RemoveSpaces(m_SceneName);

        m_SceneSerialisationVersion = SceneSerialisationVersion;

        if(binary)
        {
            path += std::string(".bin");

            std::ofstream file(path, std::ios::binary);

            {
                // output finishes flushing its contents when it goes out of scope
                cereal::BinaryOutputArchive output { file };
                output(*this);
                entt::snapshot { m_EntityManager->GetRegistry() }.get<entt::entity>(output).ALL_COMPONENTSENTTV10(output);
            }
            file.close();
        }
        else
        {
            std::stringstream storage;
            path += std::string(".lsn");

            {
                // output finishes flushing its contents when it goes out of scope
                cereal::JSONOutputArchive output { storage };
                output(*this);
                entt::snapshot { m_EntityManager->GetRegistry() }.get<entt::entity>(output).ALL_COMPONENTSENTTV10(output);
            }
            FileSystem::WriteTextFile(path, storage.str());
        }
    }

    void Scene::Deserialise(const std::string& filePath, bool binary)
    {
        LUMOS_PROFILE_FUNCTION();
        m_EntityManager->Clear();
        m_SceneGraph->DisableOnConstruct(true, m_EntityManager->GetRegistry());
        std::string path = filePath;
        path += m_SceneName; // StringUtilities::RemoveSpaces(m_SceneName);

        if(binary)
        {
            path += std::string(".bin");

            if(!FileSystem::FileExists(path))
            {
                LERROR("No saved scene file found %s", path.c_str());
                return;
            }

            try
            {
                std::ifstream file(path, std::ios::binary);
                cereal::BinaryInputArchive input(file);
                input(*this);
                if(m_SceneSerialisationVersion == 0)
                    LERROR("Invalid Scene Version");
                else if(m_SceneSerialisationVersion < MIN_SCENE_VERSION)
                    LERROR("Invalid Scene Version - Version too low %d. Minimum version supported %d", m_SceneSerialisationVersion, MIN_SCENE_VERSION);
#if MIN_SCENE_VERSION <= 2
                else if(m_SceneSerialisationVersion < 2)
                    entt::basic_snapshot_loader_legacy { m_EntityManager->GetRegistry() }.entities(input).component<ALL_COMPONENTSV1>(input).orphans();
#endif
#if MIN_SCENE_VERSION <= 3
                else if(m_SceneSerialisationVersion == 3)
                    entt::basic_snapshot_loader_legacy { m_EntityManager->GetRegistry() }.entities(input).component<ALL_COMPONENTSV2>(input).orphans();
#endif
#if MIN_SCENE_VERSION <= 4
                else if(m_SceneSerialisationVersion == 4)
                    entt::basic_snapshot_loader_legacy { m_EntityManager->GetRegistry() }.entities(input).component<ALL_COMPONENTSV3>(input).orphans();
#endif
#if MIN_SCENE_VERSION <= 5
                else if(m_SceneSerialisationVersion == 5)
                    entt::basic_snapshot_loader_legacy { m_EntityManager->GetRegistry() }.entities(input).component<ALL_COMPONENTSV4>(input);
#endif
#if MIN_SCENE_VERSION <= 6
                else if(m_SceneSerialisationVersion == 6)
                    entt::basic_snapshot_loader_legacy { m_EntityManager->GetRegistry() }.entities(input).component<ALL_COMPONENTSV5>(input);
#endif
#if MIN_SCENE_VERSION <= 7
                else if(m_SceneSerialisationVersion == 7)
                    entt::basic_snapshot_loader_legacy { m_EntityManager->GetRegistry() }.entities(input).component<ALL_COMPONENTSV6>(input);
#endif
#if MIN_SCENE_VERSION <= 13
                else if(m_SceneSerialisationVersion >= 8 && m_SceneSerialisationVersion < 14)
                    entt::basic_snapshot_loader_legacy { m_EntityManager->GetRegistry() }.entities(input).component<ALL_COMPONENTSV7>(input);
#endif
#if MIN_SCENE_VERSION <= 20
                else if(m_SceneSerialisationVersion >= 14 && m_SceneSerialisationVersion < 21)
                    entt::basic_snapshot_loader_legacy { m_EntityManager->GetRegistry() }.entities(input).component<ALL_COMPONENTSLISTV8>(input);
#endif
#if MIN_SCENE_VERSION <= 21
                else if(m_SceneSerialisationVersion >= 21 && m_SceneSerialisationVersion < 22)
                    entt::snapshot_loader { m_EntityManager->GetRegistry() }.get<entt::entity>(input).ALL_COMPONENTSENTTV8(input);
#endif
#if MIN_SCENE_VERSION <= 25
                else if(m_SceneSerialisationVersion >= 22 && m_SceneSerialisationVersion < 25)
                    entt::snapshot_loader { m_EntityManager->GetRegistry() }.get<entt::entity>(input).ALL_COMPONENTSENTTV9(input);
#endif
                else if(m_SceneSerialisationVersion >= 25)
                    entt::snapshot_loader { m_EntityManager->GetRegistry() }.get<entt::entity>(input).ALL_COMPONENTSENTTV10(input);

#if MIN_SCENE_VERSION <= 6
                if(m_SceneSerialisationVersion < 6)
                {
                    // m_EntityManager->GetRegistry().each([&](auto entity)
                    for(auto [entity] : m_EntityManager->GetRegistry().storage<entt::entity>().each())
                    {
                        m_EntityManager->GetRegistry().emplace<IDComponent>(entity, Random64::Rand(0, std::numeric_limits<uint64_t>::max()));
                    }
                }
#endif

#if MIN_SCENE_VERSION <= 7
                if(m_SceneSerialisationVersion < 7)
                {
                    // m_EntityManager->GetRegistry().each([&](auto entity)
                    for(auto [entity] : m_EntityManager->GetRegistry().storage<entt::entity>().each())
                    {
                        Graphics::Model* model;
                        if(model = m_EntityManager->GetRegistry().try_get<Graphics::Model>(entity))
                        {
                            Graphics::Model* modelCopy = new Graphics::Model(*model);
                            m_EntityManager->GetRegistry().emplace<Graphics::ModelComponent>(entity, SharedPtr<Graphics::Model>(modelCopy));
                            m_EntityManager->GetRegistry().remove<Graphics::Model>(entity);
                        }
                    }
                }
#endif
            }
            catch(...)
            {
                LERROR("Failed to load scene - %s", path.c_str());
            }
        }
        else
        {
            path += std::string(".lsn");

            if(!FileSystem::FileExists(path))
            {
                LERROR("No saved scene file found %s", path.c_str());
                return;
            }
            try
            {
                std::string data = FileSystem::ReadTextFile(path);
                std::istringstream istr;
                istr.str(data);
                cereal::JSONInputArchive input(istr);
                input(*this);

                if(m_SceneSerialisationVersion == 0)
                    LERROR("Invalid Scene Version - Invalid Scene Version");
                else if(m_SceneSerialisationVersion < MIN_SCENE_VERSION)
                    LERROR("Invalid Scene Version - Version too low %d. Minimum version supported %d", m_SceneSerialisationVersion, MIN_SCENE_VERSION);
#if MIN_SCENE_VERSION <= 2
                else if(m_SceneSerialisationVersion < 2)
                    entt::basic_snapshot_loader_legacy { m_EntityManager->GetRegistry() }.entities(input).component<ALL_COMPONENTSV1>(input).orphans();
#endif
#if MIN_SCENE_VERSION <= 3
                else if(m_SceneSerialisationVersion == 3)
                    entt::basic_snapshot_loader_legacy { m_EntityManager->GetRegistry() }.entities(input).component<ALL_COMPONENTSV2>(input).orphans();
#endif
#if MIN_SCENE_VERSION <= 4
                else if(m_SceneSerialisationVersion == 4)
                    entt::basic_snapshot_loader_legacy { m_EntityManager->GetRegistry() }.entities(input).component<ALL_COMPONENTSV3>(input).orphans();
#endif
#if MIN_SCENE_VERSION <= 5
                else if(m_SceneSerialisationVersion == 5)
                    entt::basic_snapshot_loader_legacy { m_EntityManager->GetRegistry() }.entities(input).component<ALL_COMPONENTSV4>(input);
#endif
#if MIN_SCENE_VERSION <= 6
                else if(m_SceneSerialisationVersion == 6)
                    entt::basic_snapshot_loader_legacy { m_EntityManager->GetRegistry() }.entities(input).component<ALL_COMPONENTSV5>(input);
#endif
#if MIN_SCENE_VERSION <= 7
                else if(m_SceneSerialisationVersion == 7)
                    entt::basic_snapshot_loader_legacy { m_EntityManager->GetRegistry() }.entities(input).component<ALL_COMPONENTSV6>(input);
#endif
#if MIN_SCENE_VERSION <= 13
                else if(m_SceneSerialisationVersion >= 8 && m_SceneSerialisationVersion < 14)
                    entt::basic_snapshot_loader_legacy { m_EntityManager->GetRegistry() }.entities(input).component<ALL_COMPONENTSV7>(input);
#endif
#if MIN_SCENE_VERSION <= 20
                else if(m_SceneSerialisationVersion >= 14 && m_SceneSerialisationVersion < 21)
                    entt::basic_snapshot_loader_legacy { m_EntityManager->GetRegistry() }.entities(input).component<ALL_COMPONENTSLISTV8>(input);
#endif
#if MIN_SCENE_VERSION <= 21
                else if(m_SceneSerialisationVersion >= 21 && m_SceneSerialisationVersion < 22)
                    entt::snapshot_loader { m_EntityManager->GetRegistry() }.get<entt::entity>(input).ALL_COMPONENTSENTTV8(input);
#endif
#if MIN_SCENE_VERSION <= 24
                else if(m_SceneSerialisationVersion >= 22 && m_SceneSerialisationVersion < 25)
                    entt::snapshot_loader { m_EntityManager->GetRegistry() }.get<entt::entity>(input).ALL_COMPONENTSENTTV9(input);
#endif
                else if(m_SceneSerialisationVersion >= 25)
                    entt::snapshot_loader { m_EntityManager->GetRegistry() }.get<entt::entity>(input).ALL_COMPONENTSENTTV10(input);
#if MIN_SCENE_VERSION <= 6
                if(m_SceneSerialisationVersion < 6)
                {
                    // m_EntityManager->GetRegistry().each([&](auto entity)
                    for(auto [entity] : m_EntityManager->GetRegistry().storage<entt::entity>().each())
                    {
                        m_EntityManager->GetRegistry().emplace<IDComponent>(entity, Random64::Rand(0, std::numeric_limits<uint64_t>::max()));
                    }
                }
#endif
#if MIN_SCENE_VERSION <= 7
                if(m_SceneSerialisationVersion < 7)
                {
                    // m_EntityManager->GetRegistry().each([&](auto entity)
                    for(auto [entity] : m_EntityManager->GetRegistry().storage<entt::entity>().each())
                    {
                        Graphics::Model* model;
                        if(model = m_EntityManager->GetRegistry().try_get<Graphics::Model>(entity))
                        {
                            Graphics::Model* modelCopy = new Graphics::Model(*model);
                            m_EntityManager->GetRegistry().emplace<Graphics::ModelComponent>(entity, SharedPtr<Graphics::Model>(modelCopy));
                            m_EntityManager->GetRegistry().remove<Graphics::Model>(entity);
                        }
                    }
                }
#endif
            }
            catch(...)
            {
                LERROR("Failed to load scene - %s", path.c_str());
            }
        }

        m_SceneGraph->DisableOnConstruct(false, m_EntityManager->GetRegistry());
        Application::Get().OnNewScene(this);
    }

    void Scene::UpdateSceneGraph()
    {
        LUMOS_PROFILE_FUNCTION();
        m_SceneGraph->Update(m_EntityManager->GetRegistry());
    }

    template <typename T>
    static void CopyComponentIfExists(entt::entity dst, entt::entity src, entt::registry& registry)
    {
        if(registry.all_of<T>(src))
        {
            auto srcComponent = registry.get<T>(src);
            registry.emplace_or_replace<T>(dst, srcComponent);
        }
    }

    template <typename... Component>
    static void CopyEntity(entt::entity dst, entt::entity src, entt::registry& registry)
    {
        (CopyComponentIfExists<Component>(dst, src, registry), ...);
    }

    Entity Scene::CreateEntity()
    {
        return m_EntityManager->Create();
    }

    Entity Scene::CreateEntity(const std::string& name)
    {
        LUMOS_PROFILE_FUNCTION();
        return m_EntityManager->Create(name);
    }

    Entity Scene::GetEntityByUUID(uint64_t id)
    {
        LUMOS_PROFILE_FUNCTION();
        return m_EntityManager->GetEntityByUUID(id);
    }

    bool Scene::EntityExists(u64 id)
    {
        LUMOS_PROFILE_FUNCTION();
        return m_EntityManager->EntityExists(id);
    }

    namespace
    {
        void _DestroyEntity(entt::entity entity, entt::registry& registry)
        {
            LUMOS_PROFILE_FUNCTION();
            auto hierarchyComponent = registry.try_get<Hierarchy>(entity);
            if(hierarchyComponent)
            {
                entt::entity child = hierarchyComponent->First();
                while(child != entt::null)
                {
                    auto hierarchyComponent = registry.try_get<Hierarchy>(child);
                    auto next               = hierarchyComponent ? hierarchyComponent->Next() : entt::null;
                    _DestroyEntity(child, registry);
                    child = next;
                }
            }
            registry.destroy(entity);
        }
    }

    void Scene::DestroyEntity(Entity entity)
    {
        _DestroyEntity(entity.GetHandle(), GetRegistry());
    }

    void Scene::DuplicateEntity(Entity entity)
    {
        LUMOS_PROFILE_FUNCTION();
        DuplicateEntity(entity, Entity(entt::null, nullptr));
    }

    void Scene::DuplicateEntity(Entity entity, Entity parent)
    {
        LUMOS_PROFILE_FUNCTION();
        m_SceneGraph->DisableOnConstruct(true, m_EntityManager->GetRegistry());

        Entity newEntity = m_EntityManager->Create();

        CopyEntity<ALL_COMPONENTSLISTV8>(newEntity.GetHandle(), entity.GetHandle(), m_EntityManager->GetRegistry());
        newEntity.GetComponent<IDComponent>().ID = UUID();

        auto hierarchyComponent = newEntity.TryGetComponent<Hierarchy>();
        if(hierarchyComponent)
        {
            hierarchyComponent->m_First  = entt::null;
            hierarchyComponent->m_Parent = entt::null;
            hierarchyComponent->m_Next   = entt::null;
            hierarchyComponent->m_Prev   = entt::null;
        }

        auto children  = entity.GetChildrenTemp();
        u32 childCount = entity.GetChildCount();

        for(u32 i = 0; i < childCount; i++)
        {
            DuplicateEntity(children[i], newEntity);
        }

        if(parent)
            newEntity.SetParent(parent);

        m_SceneGraph->DisableOnConstruct(false, m_EntityManager->GetRegistry());
    }

    static int PrefabVersion = 2;

    template <typename T>
    static void DeserialiseComponentIfExists(Entity entity, cereal::JSONInputArchive& archive)
    {
        LUMOS_PROFILE_FUNCTION();
        bool hasComponent;
        archive(hasComponent);
        if(hasComponent)
            archive(entity.GetOrAddComponent<T>());
    }

    template <typename... Component>
    static void DeserialiseEntity(Entity entity, cereal::JSONInputArchive& archive)
    {
        (DeserialiseComponentIfExists<Component>(entity, archive), ...);
    }

    void DeserializeEntityHierarchy(Entity entity, cereal::JSONInputArchive& archive, int version)
    {
        // Serialize the current entity
        if(version == 2)
            DeserialiseEntity<ALL_COMPONENTSLISTV8>(entity, archive);
        entity.ClearChildren();

        // Serialize the children recursively
        int children; // = entity.GetChildren();
        archive(children);

        for(int i = 0; i < children; i++)
        {
            auto child = entity.GetScene()->GetEntityManager()->Create();
            DeserializeEntityHierarchy(child, archive, version);
            child.SetParent(entity);
        }
    }

    Entity Scene::InstantiatePrefab(const std::string& path)
    {
        std::string prefabData = FileSystem::Get().ReadTextFileVFS(path);

        if(prefabData.empty())
        {
            LERROR("Failed to load prefab %s", path.c_str());
            return Entity();
        }

        std::stringstream storage(prefabData);
        cereal::JSONInputArchive input(storage);

        int version;
        int SceneVersion;
        input(cereal::make_nvp("Version", version));
        input(cereal::make_nvp("Scene Version", SceneVersion));

        int cachedSceneVersion             = Serialisation::CurrentSceneVersion;
        Serialisation::CurrentSceneVersion = SceneVersion;

        Entity entity = m_EntityManager->Create();
        DeserializeEntityHierarchy(entity, input, version);

        std::string relativePath;
        if(FileSystem::Get().AbsolutePathToFileSystem(path, relativePath))
            entity.AddComponent<PrefabComponent>(relativePath);
        else
            entity.AddComponent<PrefabComponent>(path);

        Serialisation::CurrentSceneVersion = cachedSceneVersion;

        return entity;
    }

    template <typename T>
    static void SerialiseComponentIfExists(Entity entity, cereal::JSONOutputArchive& archive)
    {
        LUMOS_PROFILE_FUNCTION();
        bool hasComponent = entity.HasComponent<T>();
        archive(hasComponent);
        if(hasComponent)
            archive(entity.GetComponent<T>());
    }

    template <typename... Component>
    static void SerialiseEntity(Entity entity, cereal::JSONOutputArchive& archive)
    {
        (SerialiseComponentIfExists<Component>(entity, archive), ...);
    }

    void SerializeEntityHierarchy(Entity entity, cereal::JSONOutputArchive& archive)
    {
        // Serialize the current entity
        SerialiseEntity<ALL_COMPONENTSLISTV8>(entity, archive);

        // Serialize the children recursively
        auto children  = entity.GetChildrenTemp();
        u32 childCount = entity.GetChildCount();
        archive((int)childCount);

        for(u32 i = 0; i < childCount; i++)
        {
            SerializeEntityHierarchy(children[i], archive);
        }
    }

    void Scene::SavePrefab(Entity entity, const std::string& path)
    {
        std::stringstream storage;

        {
            // output finishes flushing its contents when it goes out of scope
            cereal::JSONOutputArchive output { storage };

            output(cereal::make_nvp("Version", PrefabVersion));
            output(cereal::make_nvp("Scene Version", SceneSerialisationVersion));

            // Serialize a single entity
            SerializeEntityHierarchy(entity, output);
        }

        FileSystem::WriteTextFile(path, storage.str());
        std::string relativePath;
        if(FileSystem::Get().AbsolutePathToFileSystem(path, relativePath))
            entity.GetOrAddComponent<PrefabComponent>(relativePath);
    }
}
