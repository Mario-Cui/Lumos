#include "HierarchyPanel.h"
#include "Editor.h"
#include "InspectorPanel.h"

#include <Lumos/Core/OS/Input.h>
#include <Lumos/Core/Application.h>
#include <Lumos/Scene/SceneManager.h>
#include <Lumos/ImGui/ImGuiUtilities.h>
#include <Lumos/Scene/SceneGraph.h>
#include <Lumos/Maths/Transform.h>
#include <Lumos/Scene/Entity.h>
#include <Lumos/Graphics/Camera/Camera.h>
#include <Lumos/Graphics/Light.h>
#include <Lumos/Graphics/Environment.h>
#include <Lumos/Graphics/Sprite.h>
#include <Lumos/Scene/Component/Components.h>
#include <Lumos/Scripting/Lua/LuaScriptComponent.h>
#include <Lumos/ImGui/IconsMaterialDesignIcons.h>
#include <Lumos/Utilities/StringUtilities.h>
#include <Lumos/Maths/MathsUtilities.h>
#include <Lumos/Scene/Component/ModelComponent.h>
#include <Lumos/Scene/EntityFactory.h>
#include <Lumos/Core/String.h>
#include <Lumos/Scene/Component/SoundComponent.h>
#include <Lumos/Scene/Component/RigidBody2DComponent.h>
#include <Lumos/Scene/Component/RigidBody3DComponent.h>
#include <Lumos/Physics/LumosPhysicsEngine/CollisionShapes/CollisionShape.h>
#include <Lumos/Scene/Entity.h>
#include <typeinfo>
#include <imgui/imgui_internal.h>
#include <sol/sol.hpp>
#include <entt/entt.hpp>

namespace Lumos
{
    ImGuiTextFilter m_HierarchyFilter;
    Entity m_DoubleClicked;
    Entity m_HadRecentDroppedEntity;
    Entity m_CurrentPrevious;

    HierarchyPanel::HierarchyPanel()
    {
        m_Name        = ICON_MDI_FILE_TREE " Hierarchy###hierarchy";
        m_SimpleName  = "Hierarchy";
        m_StringArena = ArenaAlloc(Kilobytes(256));

        m_HadRecentDroppedEntity = {};
        m_DoubleClicked          = {};
    }

    HierarchyPanel::~HierarchyPanel()
    {
        ArenaRelease(m_StringArena);
    }

    void HierarchyPanel::DrawNode(Entity node)
    {
        LUMOS_PROFILE_FUNCTION_LOW();
        bool show = true;

        if(!node.Valid())
            return;
        bool visible = ImGui::IsRectVisible(ImVec2(ImGui::GetContentRegionMax().x, ImGui::GetTextLineHeightWithSpacing()));
        if(!visible)
        {
            ImGui::NewLine();
            return;
        }

        Entity nodeEntity = { node, Application::Get().GetSceneManager()->GetCurrentScene() };

        String8 name = PushStr8Copy(m_StringArena, node.GetName().c_str());

        if(visible && m_HierarchyFilter.IsActive())
        {
            if(!m_HierarchyFilter.PassFilter((const char*)name.str))
            {
                show = false;
            }
        }

        if(show)
        {
            // ImGui::PushID((int)node);
            ImGuiUtilities::PushID();
            auto hierarchyComponent = node.TryGetComponent<Hierarchy>();
            bool noChildren         = true;

            if(hierarchyComponent != nullptr && hierarchyComponent->First() != entt::null)
                noChildren = false;

            ImGuiTreeNodeFlags nodeFlags = ((m_Editor->IsSelected(node)) ? ImGuiTreeNodeFlags_Selected : 0);

            nodeFlags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth;

            if(noChildren)
            {
                nodeFlags |= ImGuiTreeNodeFlags_Leaf;
            }

            bool active = visible && Entity(node, m_Editor->GetCurrentScene()).Active();

            if(!active)
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

            bool dragging      = false;
            bool doubleClicked = false;
            if(node == m_DoubleClicked)
            {
                doubleClicked = true;
            }

            if(doubleClicked)
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 1.0f, 2.0f });

            if(m_HadRecentDroppedEntity == node)
            {
                ImGui::SetNextItemOpen(true);
                m_HadRecentDroppedEntity = {};
            }

            String8 icon  = Str8C((char*)ICON_MDI_CUBE_OUTLINE);
            auto& iconMap = m_Editor->GetComponentIconMap();

            if(!visible)
            {
                icon = Str8C((char*)ICON_MDI_CUBE_OUTLINE);
            }
            else if(node.HasComponent<Camera>())
            {
                if(iconMap.find(typeid(Camera).hash_code()) != iconMap.end())
                    icon = Str8C((char*)iconMap[typeid(Camera).hash_code()]);
            }
            else if(node.HasComponent<LuaScriptComponent>())
            {
                if(iconMap.find(typeid(LuaScriptComponent).hash_code()) != iconMap.end())
                    icon = Str8C((char*)iconMap[typeid(LuaScriptComponent).hash_code()]);
            }
            else if(node.HasComponent<SoundComponent>())
            {
                if(iconMap.find(typeid(SoundComponent).hash_code()) != iconMap.end())
                    icon = Str8C((char*)iconMap[typeid(SoundComponent).hash_code()]);
            }
            else if(node.HasComponent<RigidBody2DComponent>())
            {
                if(iconMap.find(typeid(RigidBody2DComponent).hash_code()) != iconMap.end())
                    icon = Str8C((char*)iconMap[typeid(RigidBody2DComponent).hash_code()]);
            }
            else if(node.HasComponent<Graphics::Light>())
            {
                if(iconMap.find(typeid(Graphics::Light).hash_code()) != iconMap.end())
                    icon = Str8C((char*)iconMap[typeid(Graphics::Light).hash_code()]);
            }
            else if(node.HasComponent<Graphics::Environment>())
            {
                if(iconMap.find(typeid(Graphics::Environment).hash_code()) != iconMap.end())
                    icon = Str8C((char*)iconMap[typeid(Graphics::Environment).hash_code()]);
            }
            else if(node.HasComponent<Graphics::Sprite>())
            {
                if(iconMap.find(typeid(Graphics::Sprite).hash_code()) != iconMap.end())
                    icon = Str8C((char*)iconMap[typeid(Graphics::Sprite).hash_code()]);
            }
            else if(node.HasComponent<TextComponent>())
            {
                if(iconMap.find(typeid(TextComponent).hash_code()) != iconMap.end())
                    icon = Str8C((char*)iconMap[typeid(TextComponent).hash_code()]);
            }

            ImGui::PushStyleColor(ImGuiCol_Text, ImGuiUtilities::GetIconColour());

            bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)node.GetID(), nodeFlags, "%s", (const char*)icon.str);
            {
                if(ImGui::BeginDragDropSource())
                {

                    if(!m_Editor->IsSelected(node))
                    {
                        /*      m_Editor->ClearSelected();
                              m_Editor->SetSelected(node);*/

                        auto selected = node.GetHandle();
                        ImGui::TextUnformatted(Entity(node, Application::Get().GetSceneManager()->GetCurrentScene()).GetName().c_str());
                        ImGui::SetDragDropPayload("Drag_Entity", &selected, sizeof(entt::entity));
                    }
                    else
                    {
                        auto selected = m_Editor->GetSelected();
                        for(auto e : selected)
                        {
                            ImGui::TextUnformatted(Entity(e, Application::Get().GetSceneManager()->GetCurrentScene()).GetName().c_str());
                        }

                        ImGui::SetDragDropPayload("Drag_Entity", selected.data(), selected.size() * sizeof(entt::entity));
                    }

                    ImGui::EndDragDropSource();
                }

                // Allow clicking of icon and text. Need twice as they are separated
                if(ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::IsItemHovered() && !ImGui::IsItemToggledOpen())
                {
                    bool ctrlDown = Input::Get().GetKeyHeld(Lumos::InputCode::Key::LeftControl) || Input::Get().GetKeyHeld(Lumos::InputCode::Key::RightControl) || Input::Get().GetKeyHeld(Lumos::InputCode::Key::LeftSuper);
                    if(!ctrlDown)
                        m_Editor->ClearSelected();

                    if(!m_Editor->IsSelected(node))
                        m_Editor->SetSelected(node);
                    else
                        m_Editor->UnSelect(node);
                }
                else if(m_DoubleClicked == node && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsItemHovered(ImGuiHoveredFlags_None))
                    m_DoubleClicked = {};
            }

            if(visible && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered(ImGuiHoveredFlags_None))
            {
                m_DoubleClicked = node;
                if(Application::Get().GetEditorState() == EditorState::Preview)
                {
                    auto transform = node.TryGetComponent<Maths::Transform>();
                    if(transform)
                        m_Editor->FocusCamera(transform->GetWorldPosition(), 2.0f, 2.0f);
                }
            }

            bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_None);

            ImGui::PopStyleColor();
            ImGui::SameLine();
            if(!doubleClicked)
            {
                bool isPrefab = false;
                if(node.HasComponent<PrefabComponent>())
                    isPrefab = true;
                else
                {
                    auto Parent = nodeEntity.GetParent();
                    while(Parent && Parent.Valid())
                    {
                        if(Parent.HasComponent<PrefabComponent>())
                        {
                            isPrefab = true;
                            Parent   = {};
                        }
                        else
                        {
                            Parent = Parent.GetParent();
                        }
                    }
                }

                if(isPrefab)
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
                ImGui::TextUnformatted((const char*)name.str);
                if(isPrefab)
                    ImGui::PopStyleColor();
            }

            if(doubleClicked)
            {
                String8 nameBuffer = { 0 };
                nameBuffer.str     = PushArray(m_StringArena, uint8_t, INPUT_BUF_SIZE);
                nameBuffer.size    = INPUT_BUF_SIZE;

                MemoryCopy(nameBuffer.str, name.str, name.size);

                ImGui::PushItemWidth(-1);
                if(ImGui::InputText("##Name", (char*)nameBuffer.str, INPUT_BUF_SIZE, 0))
                    node.GetOrAddComponent<NameComponent>((const char*)nameBuffer.str);
                ImGui::PopStyleVar();
            }

            if(!active)
                ImGui::PopStyleColor();

            bool deleteEntity = false;

            if(ImGui::BeginPopupContextItem((const char*)name.str))
            {
                if(ImGui::Selectable("Copy"))
                {
                    if(!m_Editor->IsSelected(node))
                    {
                        m_Editor->SetCopiedEntity(node);
                    }
                    for(auto entity : m_Editor->GetSelected())
                        m_Editor->SetCopiedEntity(entity);
                }

                if(ImGui::Selectable("Cut"))
                {
                    for(auto entity : m_Editor->GetSelected())
                        m_Editor->SetCopiedEntity(entity, true);
                }

                if(m_Editor->GetCopiedEntity().size() > 0 && ((Entity)m_Editor->GetCopiedEntity().front()).Valid())
                {
                    if(ImGui::Selectable("Paste"))
                    {
                        for(auto entity : m_Editor->GetCopiedEntity())
                        {
                            auto scene          = Application::Get().GetSceneManager()->GetCurrentScene();
                            Entity copiedEntity = { entity, scene };
                            if(!copiedEntity.Valid())
                            {
                                m_Editor->SetCopiedEntity(Entity());
                            }
                            else
                            {
                                scene->DuplicateEntity(copiedEntity, { node, scene });

                                if(m_Editor->GetCutCopyEntity())
                                    deleteEntity = true;
                            }
                        }
                    }
                }
                else
                {
                    ImGui::TextDisabled("Paste");
                }

                ImGui::Separator();

                if(ImGui::Selectable("Duplicate"))
                {
                    auto scene = Application::Get().GetSceneManager()->GetCurrentScene();
                    scene->DuplicateEntity({ node, scene });
                }
                if(ImGui::Selectable("Delete"))
                    deleteEntity = true;
                // if(m_Editor->IsSelected(node))
                //   m_Editor->UnSelect(node);
                ImGui::Separator();
                if(ImGui::Selectable("Rename"))
                    m_DoubleClicked = node;
                ImGui::Separator();

                if(ImGui::Selectable("Add Child"))
                {
                    auto scene = Application::Get().GetSceneManager()->GetCurrentScene();
                    auto child = scene->CreateEntity();
                    child.SetParent({ node, scene });
                }

                if(ImGui::Selectable("Zoom to"))
                {
                    // if(Application::Get().GetEditorState() == EditorState::Preview)
                    {
                        auto transform = node.TryGetComponent<Maths::Transform>();
                        if(transform)
                            m_Editor->FocusCamera(transform->GetWorldPosition(), 2.0f, 2.0f);
                    }
                }
                ImGui::EndPopup();
            }

            auto scene     = Application::Get().GetSceneManager()->GetCurrentScene();
            auto& registry = scene->GetRegistry();

            if(ImGui::BeginDragDropTarget())
            {
                const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Drag_Entity");
                if(payload)
                {
                    size_t count = payload->DataSize / sizeof(entt::entity);

                    auto currentScene = m_Editor->GetCurrentScene();
                    for(size_t i = 0; i < count; i++)
                    {
                        entt::entity droppedEntityID = *(((entt::entity*)payload->Data) + i);
                        if(droppedEntityID != node.GetHandle())
                        {
                            auto hierarchyComponent = registry.try_get<Hierarchy>(droppedEntityID);
                            if(hierarchyComponent)
                                Hierarchy::Reparent(droppedEntityID, node, registry, *hierarchyComponent);
                            else
                            {
                                registry.emplace<Hierarchy>(droppedEntityID, node);
                            }
                        }
                    }

                    m_HadRecentDroppedEntity = node;
                }
                ImGui::EndDragDropTarget();
            }

            if(ImGui::IsItemClicked() && !deleteEntity)
                m_Editor->SetSelected(node);
            else if(m_DoubleClicked == node && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsItemHovered(ImGuiHoveredFlags_None))
                m_DoubleClicked = {};

            if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered(ImGuiHoveredFlags_None))
            {
                m_DoubleClicked = node;
                if(Application::Get().GetEditorState() == EditorState::Preview)
                {
                    auto transform = node.TryGetComponent<Maths::Transform>();
                    if(transform)
                        m_Editor->FocusCamera(transform->GetWorldPosition(), 2.0f, 2.0f);
                }
            }

            if(deleteEntity)
            {
                for(auto entity : m_Editor->GetSelected())
                    nodeEntity.GetScene()->DestroyEntity(Entity(entity, Application::Get().GetSceneManager()->GetCurrentScene()));
                if(nodeOpen)
                    ImGui::TreePop();

                ImGuiUtilities::PopID();
                return;
            }

            if(m_SelectUp)
            {
                if(!m_Editor->GetSelected().empty() && m_Editor->GetSelected().front() == node && m_CurrentPrevious.Valid())
                {
                    m_SelectUp = false;
                    m_Editor->SetSelected(m_CurrentPrevious);
                }
            }

            if(m_SelectDown)
            {
                if(!m_Editor->GetSelected().empty() && m_CurrentPrevious.Valid() && m_CurrentPrevious == m_Editor->GetSelected().front())
                {
                    m_SelectDown = false;
                    m_Editor->SetSelected(node);
                }
            }

            m_CurrentPrevious = node;

#if 1
            bool showButton = true; // hovered || !active;

            if(showButton)
            {
                ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(ICON_MDI_EYE).x * 2.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.7f, 0.7f, 0.0f));
                if(ImGui::Button(active ? ICON_MDI_EYE : ICON_MDI_EYE_OFF))
                {
                    auto& activeComponent = node.GetOrAddComponent<ActiveComponent>();

                    activeComponent.active = !active;
                }
                ImGui::PopStyleColor();
            }
#endif

            if(nodeOpen == false)
            {
                ImGuiUtilities::PopID();
                return;
            }

            const ImColor TreeLineColor = ImColor(128, 128, 128, 128);
            const float SmallOffsetX    = 6.0f * Application::Get().GetWindowDPI();
            ImDrawList* drawList        = ImGui::GetWindowDrawList();

            ImVec2 verticalLineStart = ImGui::GetCursorScreenPos();
            verticalLineStart.x += SmallOffsetX; // to nicely line up with the arrow symbol
            ImVec2 verticalLineEnd = verticalLineStart;

            if(visible && !noChildren)
            {
                entt::entity child = hierarchyComponent->First();
                while(child != entt::null && registry.valid(child))
                {
                    float HorizontalTreeLineSize = 20.0f * Application::Get().GetWindowDPI(); // chosen arbitrarily
                    auto currentPos              = ImGui::GetCursorScreenPos();
                    ImGui::Indent(10.0f);

                    auto childHerarchyComponent = registry.try_get<Hierarchy>(child);

                    if(childHerarchyComponent)
                    {
                        entt::entity firstChild = childHerarchyComponent->First();
                        if(firstChild != entt::null && registry.valid(firstChild))
                        {
                            HorizontalTreeLineSize *= 0.4f;
                        }
                    }
                    auto scene = Application::Get().GetSceneManager()->GetCurrentScene();
                    DrawNode(Entity(child, scene));
                    ImGui::Unindent(10.0f);

                    const ImRect childRect = ImRect(currentPos, currentPos + ImVec2(0.0f, ImGui::GetFontSize() + (ImGui::GetStyle().FramePadding.y * 2)));

                    const float midpoint = (childRect.Min.y + childRect.Max.y) * 0.5f;
                    drawList->AddLine(ImVec2(verticalLineStart.x, midpoint), ImVec2(verticalLineStart.x + HorizontalTreeLineSize, midpoint), TreeLineColor);
                    verticalLineEnd.y = midpoint;

                    if(registry.valid(child))
                    {
                        auto hierarchyComponent = registry.try_get<Hierarchy>(child);
                        child                   = hierarchyComponent ? hierarchyComponent->Next() : entt::null;
                    }
                }
            }

            drawList->AddLine(verticalLineStart, verticalLineEnd, TreeLineColor);

            ImGui::TreePop();
            ImGuiUtilities::PopID();
        }
    }

    bool HierarchyPanel::IsParentOfEntity(Entity entity, Entity child)
    {
        LUMOS_PROFILE_FUNCTION();
        return entity.IsParent(child);
        //        auto nodeHierarchyComponent = child.TryGetComponent<Hierarchy>();
        //        if(nodeHierarchyComponent)
        //        {
        //            auto parent = nodeHierarchyComponent->Parent();
        //            while(parent != entt::null)
        //            {
        //                if(parent == entity)
        //                {
        //                    return true;
        //                }
        //                else
        //                {
        //                    nodeHierarchyComponent = registry.try_get<Hierarchy>(parent);
        //                    parent                 = nodeHierarchyComponent ? nodeHierarchyComponent->Parent() : entt::null;
        //                }
        //            }
        //        }
        //
        //        return false;
    }

    void HierarchyPanel::OnImGui()
    {
        LUMOS_PROFILE_FUNCTION();
        auto flags        = ImGuiWindowFlags_NoCollapse;
        m_CurrentPrevious = {};
        m_SelectUp        = false;
        m_SelectDown      = false;

        m_SelectUp   = Input::Get().GetKeyPressed(Lumos::InputCode::Key::Up);
        m_SelectDown = Input::Get().GetKeyPressed(Lumos::InputCode::Key::Down);

        ArenaClear(m_StringArena);

        if(ImGui::Begin(m_Name.c_str(), &m_Active, flags))
        {
            ImRect windowRect = { ImGui::GetWindowContentRegionMin(), ImGui::GetWindowContentRegionMax() };

            auto scene = Application::Get().GetSceneManager()->GetCurrentScene();

            if(!scene)
            {
                ImGui::End();
                return;
            }
            auto& registry = scene->GetRegistry();
            auto editor    = m_Editor;
            auto AddEntity = [scene, editor]()
            {
                if(ImGui::BeginMenu("Add"))
                {
                    if(ImGui::Selectable("Empty Entity"))
                    {
                        scene->CreateEntity();
                    }

                    if(ImGui::Selectable("Light"))
                    {
                        auto entity = scene->CreateEntity("Light");
                        entity.AddComponent<Graphics::Light>();
                        entity.GetOrAddComponent<Maths::Transform>();
                        editor->ClearSelected();
                        editor->SetSelected(entity);
                    }

                    if(ImGui::Selectable("Rigid Body"))
                    {
                        auto entity = scene->CreateEntity("RigidBody");
                        entity.AddComponent<RigidBody3DComponent>();
                        entity.GetOrAddComponent<Maths::Transform>();
                        entity.AddComponent<Graphics::ModelComponent>(Graphics::PrimitiveType::Cube);
                        // entity.AddComponent<AxisConstraintComponent>(entity, Axes::XZ);
                        entity.GetComponent<RigidBody3DComponent>().GetRigidBody()->SetCollisionShape(CollisionShapeType::CollisionCuboid);
                        editor->ClearSelected();
                        editor->SetSelected(entity);
                    }

                    if(ImGui::Selectable("Camera"))
                    {
                        auto entity = scene->CreateEntity("Camera");
                        entity.AddComponent<Camera>();
                        entity.GetOrAddComponent<Maths::Transform>();
                        editor->ClearSelected();
                        editor->SetSelected(entity);
                    }

                    if(ImGui::Selectable("Sprite"))
                    {
                        auto entity = scene->CreateEntity("Sprite");
                        entity.AddComponent<Graphics::Sprite>();
                        entity.GetOrAddComponent<Maths::Transform>();
                        editor->ClearSelected();
                        editor->SetSelected(entity);
                    }

                    if(ImGui::Selectable("Lua Script"))
                    {
                        auto entity = scene->CreateEntity("LuaScript");
                        entity.AddComponent<LuaScriptComponent>();
                        editor->ClearSelected();
                        editor->SetSelected(entity);
                    }

                    if(ImGui::BeginMenu("Primitive"))
                    {

                        if(ImGui::MenuItem("Cube"))
                        {
                            auto entity = scene->CreateEntity("Cube");
                            entity.AddComponent<Graphics::ModelComponent>(Graphics::PrimitiveType::Cube);
                            editor->ClearSelected();
                            editor->SetSelected(entity);
                        }

                        if(ImGui::MenuItem("Sphere"))
                        {
                            auto entity = scene->CreateEntity("Sphere");
                            entity.AddComponent<Graphics::ModelComponent>(Graphics::PrimitiveType::Sphere);
                            editor->ClearSelected();
                            editor->SetSelected(entity);
                        }

                        if(ImGui::MenuItem("Pyramid"))
                        {
                            auto entity = scene->CreateEntity("Pyramid");
                            entity.AddComponent<Graphics::ModelComponent>(Graphics::PrimitiveType::Pyramid);
                            editor->ClearSelected();
                            editor->SetSelected(entity);
                        }

                        if(ImGui::MenuItem("Plane"))
                        {
                            auto entity = scene->CreateEntity("Plane");
                            entity.AddComponent<Graphics::ModelComponent>(Graphics::PrimitiveType::Plane);
                            editor->ClearSelected();
                            editor->SetSelected(entity);
                        }

                        if(ImGui::MenuItem("Cylinder"))
                        {
                            auto entity = scene->CreateEntity("Cylinder");
                            entity.AddComponent<Graphics::ModelComponent>(Graphics::PrimitiveType::Cylinder);
                            editor->ClearSelected();
                            editor->SetSelected(entity);
                        }

                        if(ImGui::MenuItem("Capsule"))
                        {
                            auto entity = scene->CreateEntity("Capsule");
                            entity.AddComponent<Graphics::ModelComponent>(Graphics::PrimitiveType::Capsule);
                            editor->ClearSelected();
                            editor->SetSelected(entity);
                        }

                        if(ImGui::MenuItem("Terrain"))
                        {
                            auto entity = scene->CreateEntity("Terrain");
                            entity.AddComponent<Graphics::ModelComponent>(Graphics::PrimitiveType::Terrain);
                            editor->ClearSelected();
                            editor->SetSelected(entity);
                        }

                        if(ImGui::MenuItem("Light Cube"))
                        {
                            EntityFactory::AddLightCube(Application::Get().GetSceneManager()->GetCurrentScene(), Vec3(0.0f), Vec3(0.0f));
                        }

                        ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }
            };

            ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImGui::GetStyleColorVec4(ImGuiCol_TabActive));

            if(ImGui::Button(ICON_MDI_PLUS))
            {
                // Add Entity Menu
                ImGui::OpenPopup("AddEntity");
            }

            if(ImGui::BeginPopup("AddEntity"))
            {
                AddEntity();
                ImGui::EndPopup();
            }

            ImGui::SameLine();
            ImGui::TextUnformatted(ICON_MDI_MAGNIFY);
            ImGui::SameLine();

            {
                ImGuiUtilities::ScopedFont boldFont(ImGui::GetIO().Fonts->Fonts[1]);
                ImGuiUtilities::ScopedStyle frameBorder(ImGuiStyleVar_FrameBorderSize, 0.0f);
                ImGuiUtilities::ScopedColour frameColour(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
                m_HierarchyFilter.Draw("##HierarchyFilter", ImGui::GetContentRegionAvail().x - ImGui::GetStyle().IndentSpacing);
                ImGuiUtilities::DrawItemActivityOutline(2.0f, false);
            }

            if(!m_HierarchyFilter.IsActive())
            {
                ImGui::SameLine();
                ImGuiUtilities::ScopedFont boldFont(ImGui::GetIO().Fonts->Fonts[1]);
                ImGui::SetCursorPosX(ImGui::GetFontSize() * 4.0f);
                ImGuiUtilities::ScopedStyle padding(ImGuiStyleVar_FramePadding, ImVec2(0.0f, ImGui::GetStyle().FramePadding.y));
                ImGui::TextUnformatted("Search...");
            }

            ImGui::PopStyleColor();
            ImGui::Unindent();

            // For background colour
            /*
ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_TitleBg) * 1.1f);
const float backup_border_size    = ImGui::GetStyle().ChildBorderSize;
ImGui::GetStyle().ChildBorderSize = 6.0f;
ImGui::BeginChild("Nodes", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_AlwaysUseWindowPadding);
ImGui::GetStyle().ChildBorderSize = backup_border_size;
*/

            // Right click popup
            if(ImGui::BeginPopupContextWindow())
            {
                if(!m_Editor->GetCopiedEntity().empty() && registry.valid(m_Editor->GetCopiedEntity().front()))
                {
                    if(ImGui::Selectable("Paste"))
                    {
                        for(auto entity : m_Editor->GetCopiedEntity())
                        {
                            auto scene          = Application::Get().GetSceneManager()->GetCurrentScene();
                            Entity copiedEntity = { entity, scene };
                            if(!copiedEntity.Valid())
                            {
                                m_Editor->SetCopiedEntity({});
                            }
                            else
                            {
                                scene->DuplicateEntity(copiedEntity);

                                if(m_Editor->GetCutCopyEntity())
                                {
                                    copiedEntity.GetScene()->DestroyEntity(copiedEntity);
                                }
                            }
                        }
                    }
                }
                else
                {
                    ImGui::TextDisabled("Paste");
                }

                ImGui::Separator();

                AddEntity();

                ImGui::EndPopup();
            }
            {
                /*        if (ImGui::BeginDragDropTargetCustom(windowRect, ImGui::GetCurrentWindow()->ID))
                        {
                            if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Drag_Entity"))
                            {
                                ASSERT(payload->DataSize == sizeof(entt::entity*), "Error ImGUI drag entity");
                                auto entity             = *reinterpret_cast<entt::entity*>(payload->Data);
                                auto hierarchyComponent = registry.try_get<Hierarchy>(entity);
                                if(hierarchyComponent)
                                {
                                    Hierarchy::Reparent(entity, entt::null, registry, *hierarchyComponent);
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }*/

                ImGui::Indent();

                auto scene = Application::Get().GetSceneManager()->GetCurrentScene();

                for(auto [entity] : registry.storage<entt::entity>().each())
                {
                    if(registry.valid(entity))
                    {
                        auto hierarchyComponent = registry.try_get<Hierarchy>(entity);

                        if(!hierarchyComponent || hierarchyComponent->Parent() == entt::null)
                            DrawNode({ entity, scene });
                    }
                }

                // Only supports one scene
                ImVec2 min_space = ImGui::GetWindowContentRegionMin();
                ImVec2 max_space = ImGui::GetWindowContentRegionMax();

                float yOffset = Maths::Max(45.0f, ImGui::GetScrollY()); // Dont include search bar
                min_space.x += ImGui::GetWindowPos().x + 1.0f;
                min_space.y += ImGui::GetWindowPos().y + 1.0f + yOffset;
                max_space.x += ImGui::GetWindowPos().x - 1.0f;
                max_space.y += ImGui::GetWindowPos().y - 1.0f + ImGui::GetScrollY();
                ImRect bb { min_space, max_space };

                if(ImGui::BeginDragDropTargetCustom(windowRect, ImGui::GetCurrentWindow()->ID))
                {
                    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Drag_Entity", ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
                    if(payload)
                    {
                        size_t count = payload->DataSize / sizeof(entt::entity);

                        auto currentScene = m_Editor->GetCurrentScene();
                        for(size_t i = 0; i < count; i++)
                        {
                            entt::entity droppedEntityID = *(((entt::entity*)payload->Data) + i);
                            auto hierarchyComponent      = registry.try_get<Hierarchy>(droppedEntityID);
                            if(hierarchyComponent)
                            {
                                Hierarchy::Reparent(droppedEntityID, entt::null, registry, *hierarchyComponent);
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
            }
            // ImGui::EndChild();
            // ImGui::PopStyleColor();

            if(ImGui::IsWindowFocused() && Input::Get().GetKeyPressed(Lumos::InputCode::Key::Delete))
            {
                auto* scene = Application::Get().GetCurrentScene();
                for(auto entity : m_Editor->GetSelected())
                    scene->DestroyEntity(Entity(entity, scene));
            }
        }
        ImGui::End();
    }
}
