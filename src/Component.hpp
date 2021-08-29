#pragma once

namespace karapo::component {
	class Mode {
		std::wstring *selecting{};
		std::list<std::wstring> mode_names{};
	public:
		void AddMenu();
		void Select(const std::wstring&);
	};

	class Manager final : private Singleton {
		std::unordered_map<std::wstring, std::unique_ptr<Component>(*)(std::shared_ptr<Entity>&)> managing_components{};
		std::unordered_map<std::wstring, std::list<std::pair<std::wstring, std::unique_ptr<Component>>>> entity_component{};
	public:
		static Manager& Instance();
		void RunComponents(const std::wstring& Entity_Name);
		void RegisterComponent(const std::wstring&, std::unique_ptr<Component>(*)(std::shared_ptr<Entity>&));
		void AddComponentToEntity(const std::wstring& Entity_Name, const std::wstring& Component_Name);
		void RemoveComponentToEntity(const std::wstring& Entity_Name, const std::wstring& Component_Name);
		std::unique_ptr<Component>& GetComponentFromEntity(const std::wstring& Entity_Name, const std::wstring& Component_Name);
	};
}