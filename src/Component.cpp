#include "Component.hpp"

#include "Engine.hpp"
namespace karapo::component {
	class EntityComponent : public Component {
		std::shared_ptr<Entity> owner{};
	protected:
		const std::shared_ptr<Entity>& Owner = owner;
	public:
		EntityComponent(std::shared_ptr<Entity>& ent) noexcept {
			owner = ent;
		}
	};

	class Shape final : public EntityComponent {
		std::shared_ptr<Entity> owner{};
	public:
		enum class State {
			Block,
			Circle
		} state{};

		Shape(std::shared_ptr<Entity>& ent) noexcept : EntityComponent(ent) {}

		~Shape() final {}

		void Run() final {}
	};

	// 衝突
	class Collision final : public EntityComponent {
		bool collided_enough{};
		WorldVector old_origin{ 0.0, 0.0 };
	public:
		Collision(std::shared_ptr<Entity>& ent) noexcept : EntityComponent(ent){}

		~Collision() final {}

		std::shared_ptr<Entity> Collide() {
			return Program::Instance().entity_manager.GetEntity([&](std::shared_ptr<Entity> candidate) -> bool {
				auto shape = static_cast<Shape*>(Manager::Instance().GetComponentFromEntity(candidate->Name(), L"shape").get());
				if (Owner != candidate && shape != nullptr) {
					switch (shape->state) {
						case Shape::State::Block:
						{
							const bool Is_Collided_Width = (Owner->Origin()[0] >= candidate->Origin()[0] || (Owner->Origin() + Owner->Length())[0] >= candidate->Origin()[0]) &&
								(Owner->Origin()[0] <= (candidate->Origin() + candidate->Length())[0] || (Owner->Origin() + Owner->Length())[0] <= (candidate->Origin() + candidate->Length())[0]);
							const bool Is_Collided_Height = (Owner->Origin()[1] >= candidate->Origin()[1] || Owner->Origin()[1] + Owner->Length()[1] >= candidate->Origin()[1]) &&
								(Owner->Origin()[1] <= candidate->Origin()[1] + candidate->Length()[1] || Owner->Origin()[1] + Owner->Length()[1] <= candidate->Origin()[1] + candidate->Length()[1]);

							if (Is_Collided_Width && Is_Collided_Height) {
								return true;
							}
							break;
						}
						case Shape::State::Circle:
							break;
						default:
							MYGAME_ASSERT(0);
					}
				}
				return false;
			});
		}

		void Run() final {
			std::wstring owner_name = Owner->Name();
			std::wstring colliding_kind_name{};
			if (decltype(auto) collider = Collide(); collider != nullptr) {
				WorldVector move = old_origin - Owner->Origin();
				colliding_kind_name = collider->KindName();
				if (colliding_kind_name == L"マウスポインタ" && Program::Instance().engine.IsPressingMouse(Default_ProgramInterface.keys.Left_Click)) {
					Program::Instance().event_manager.Push(owner_name + L".clicking");
				}

				if (!collided_enough) {
					collided_enough = true;
					Program::Instance().event_manager.Push(owner_name + L".collided");
				} else
					Program::Instance().event_manager.Push(owner_name + L".colliding");
			} else {
				collided_enough = false;
			}
			Program::Instance().var_manager.Get(owner_name + L".colliding_type") = colliding_kind_name;
			old_origin = Owner->Origin();
		}
	};

	class Action final : public EntityComponent {
		Collision *collision{};

		bool can_jump{};

		Dec *fall_speed{};
		Dec impulse{};
		Dec *base_accelation_x{}, *base_accelation_y{};
		int *go_up{}, *go_down{}, *go_left{}, *go_right{}, *do_jumping{};

		WorldVector move{ 0.0, 0.0 };
	public:
		Action(std::shared_ptr<Entity>& ent) noexcept : EntityComponent(ent) {
			const std::wstring&& Owner_Name = Owner->Name();
			{
				Component *com{};
				com = Manager::Instance().GetComponentFromEntity(Owner_Name, L"collision").get();
				if (com != nullptr) {
					collision = static_cast<Collision*>(com);
				} else {
					Manager::Instance().AddComponentToEntity(Owner_Name, L"collision");
					com = Manager::Instance().GetComponentFromEntity(Owner_Name, L"collision").get();
					collision = static_cast<Collision*>(com);
				}
			}

			Program::Instance().var_manager.MakeNew(Owner_Name + L".fall_speed") = 0.0;
			Program::Instance().var_manager.MakeNew(Owner_Name + L".accelation_x") = 0.0;
			Program::Instance().var_manager.MakeNew(Owner_Name + L".accelation_y") = 0.0;
			Program::Instance().var_manager.MakeNew(Owner_Name + L".go_up") = 0;
			Program::Instance().var_manager.MakeNew(Owner_Name + L".go_down") = 0;
			Program::Instance().var_manager.MakeNew(Owner_Name + L".go_left") = 0;
			Program::Instance().var_manager.MakeNew(Owner_Name + L".go_right") = 0;
			Program::Instance().var_manager.MakeNew(Owner_Name + L".do_jumping") = 0;
			Program::Instance().var_manager.MakeNew(Owner_Name + L".impulse") = 0.0;

			base_accelation_x = &std::any_cast<Dec&>(Program::Instance().var_manager.Get(Owner_Name + L".accelation_x"));
			base_accelation_y = &std::any_cast<Dec&>(Program::Instance().var_manager.Get(Owner_Name + L".accelation_y"));
			go_up = &std::any_cast<int&>(Program::Instance().var_manager.Get(Owner_Name + L".go_up"));
			go_down = &std::any_cast<int&>(Program::Instance().var_manager.Get(Owner_Name + L".go_down"));
			go_left = &std::any_cast<int&>(Program::Instance().var_manager.Get(Owner_Name + L".go_left"));
			go_right = &std::any_cast<int&>(Program::Instance().var_manager.Get(Owner_Name + L".go_right"));
			do_jumping = &std::any_cast<int&>(Program::Instance().var_manager.Get(Owner_Name + L".do_jumping"));
			fall_speed = &std::any_cast<Dec&>(Program::Instance().var_manager.Get(Owner_Name + L".fall_speed"));

			auto editor = Program::Instance().MakeEventEditor();
			editor->MakeNewEvent(Owner_Name + L".右移動");
			auto target = Owner_Name + L".go_right";
			editor->ChangeTriggerType(L"n");
			editor->AddCommand(L"{\n代入 " + target + L" 1\n}", 0);
			editor->MakeNewEvent(Owner_Name + L".左移動");
			editor->ChangeTriggerType(L"n");
			target = Owner_Name + L".go_left";
			editor->AddCommand(L"{\n代入 " + target + L" 1\n}", 0);
			editor->MakeNewEvent(Owner_Name + L".ジャンプ");
			target = Owner_Name + L".do_jumping";
			editor->ChangeTriggerType(L"n");
			editor->AddCommand(L"{\n代入 " + target + L" 1\n}", 0);
			Program::Instance().FreeEventEditor(editor);
		}

		~Action() final {}

		void Run() final {
			if (impulse > 0.0) {
				impulse -= 0.1;
				can_jump = false;
			} else {
				impulse = 0.0;
			}

			if (*go_up) {
				move[1] = -*base_accelation_y;
				*go_up = 0;
			}

			if (*go_down) {
				move[1] = *base_accelation_y;
				*go_down = 0;
			}

			if (*go_left) {
				move[0] = -*base_accelation_x;
				*go_left = 0;
			}

			if (*go_right) {
				move[0] = *base_accelation_x;
				*go_right = 0;
			}

			if (*do_jumping && can_jump) {
				*do_jumping = 0;
				impulse = std::any_cast<Dec>(Program::Instance().var_manager.Get(std::wstring(Owner->Name()) + L".impulse"));
			} else {
				*do_jumping = 0;
			}

			move[1] += *fall_speed - impulse;
			{
				auto temp = Owner->Origin();
				WorldVector moved[2] = { 
					{ Owner->Origin()[0] + move[0], Owner->Origin()[1] },
					{ Owner->Origin()[0], Owner->Origin()[1] + move[1] },
				};

				for (int i = 0; i < 2; i++) {
					Owner->Teleport(moved[i]);
					if (auto collider = collision->Collide(); collider != nullptr) {
						if (move[i] > 0.0) {
							move[i] = (collider->Origin() - Owner->Origin() - Owner->Length())[i];
							if (i == 1) {
								can_jump = true;
							}
						} else if (move[i] < 0.0) {
							move[i] = ((collider->Origin() + collider->Length()) - Owner->Origin())[i];
						}
					}
					Owner->Teleport(temp);
				}
			}
			auto result = Owner->Origin() + WorldVector{ move[0], move[1] };
			Owner->Teleport(result);
			move = 0.0;
		}
	};

	Manager& Manager::Instance() {
		static Manager manager;
		static bool first{};
		if (!first) {
			manager.RegisterComponent(L"collision", [](std::shared_ptr<Entity>& ent) -> std::unique_ptr<Component> {
				return std::make_unique<Collision>(ent);
			});

			manager.RegisterComponent(L"shape", [](std::shared_ptr<Entity>& ent) -> std::unique_ptr<Component> {
				return std::make_unique<Shape>(ent);
			});

			manager.RegisterComponent(L"action", [](std::shared_ptr<Entity>& ent) -> std::unique_ptr<Component> {
				return std::make_unique<Action>(ent);
			});
		}
		return manager;
	}

	void Manager::RunComponents(const std::wstring& Entity_Name) {
		for (auto& component : entity_component[Entity_Name]) {
			component.second->Run();
		}
	}

	void Manager::RegisterComponent(const std::wstring& Component_Name, std::unique_ptr<Component>(*generateFunc)(std::shared_ptr<Entity>&)) {
		managing_components[Component_Name] = generateFunc;
	}

	void Manager::AddComponentToEntity(const std::wstring& Entity_Name, const std::wstring& Component_Name) {
		auto ent = entity::Manager::Instance().GetEntity(Entity_Name);
		entity_component[Entity_Name].push_back({ Component_Name, std::move(managing_components[Component_Name](ent)) });
	}

	void Manager::RemoveComponentToEntity(const std::wstring& Entity_Name, const std::wstring& Component_Name) {
		auto it = std::find_if(
			entity_component[Entity_Name].begin(), entity_component[Entity_Name].end(), [&](std::pair<std::wstring, std::unique_ptr<Component>>& component) {
				return (component.first == Component_Name);
			}
		);
		if (it != entity_component[Entity_Name].end()) entity_component[Entity_Name].erase(it);
	}

	std::unique_ptr<Component>& Manager::GetComponentFromEntity(const std::wstring& Entity_Name, const std::wstring& Component_Name) {
		auto it = std::find_if(entity_component[Entity_Name].begin(), entity_component[Entity_Name].end(), [&](std::pair<std::wstring, std::unique_ptr<Component>>& component) {
			return (component.first == Component_Name);
		});
		if (it != entity_component[Entity_Name].end())
			return it->second;
		else {
			static std::unique_ptr<Component> null_component = nullptr;
			return null_component;
		}
	}
}