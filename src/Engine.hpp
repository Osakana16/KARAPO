#pragma once
#include "Canvas.hpp"
#include <chrono>

namespace karapo {
	namespace variable {
		class Manager {
			std::unordered_map<std::wstring, std::any> vars;
		public:
			Manager();
			std::any& MakeNew(const std::wstring&);
			void Delete(const std::wstring&) noexcept;

			template<bool throw_except>
			std::any& Get(const std::wstring& Var_Name) noexcept(!throw_except) {
				if constexpr (throw_except) {
					return vars.at(Var_Name);
				} else {
					try {
						return vars.at(Var_Name);
					} catch (std::out_of_range&) {
						return vars.at(L"null");
					}
				}
			}
		};
	}

	namespace dll {
		class Manager {
			struct DLL {
				using Initializer = void(WINAPI*)(ProgramInterface);
				using LoopableMain = bool(WINAPI*)();
				using EventRegister = void(WINAPI*)(std::unordered_map<std::wstring, event::GenerateFunc>*);

				HMODULE mod;
				Initializer Init;
				LoopableMain Update;
				EventRegister RegisterExternalCommand;
			};

			std::unordered_map<std::wstring, DLL> dlls;
			void Attach(const std::wstring&);
		public:
			void Detach(const std::wstring&);
			void Load(const std::wstring&);
			void Update();
			void RegisterExternalCommand(std::unordered_map<std::wstring, event::GenerateFunc>*);
			HMODULE Get(const std::wstring&) noexcept;
		};
	}

#undef LoadImage
#undef PlaySound

	class Program {
		class Engine {
			std::vector<TargetRender> screens;
			std::unordered_map<std::wstring, resource::Resource> resources;

			bool fullscreen = false, synchronize = false, fixed = false;
			unsigned keys_state[256];
		public:
			Engine() noexcept, ~Engine() noexcept;

			void OnInit(Program*) noexcept;
			bool Failed() const noexcept;

			resource::Resource LoadImage(const std::wstring&) noexcept, LoadSound(const std::wstring&) noexcept;

			void DrawLine(int, int, int, int, Color);
			void DrawRect(Rect, const resource::Image&) noexcept, DrawRect(Rect, const TargetRender) noexcept, DrawRect(Rect, Color, bool fill) noexcept;
			void DrawSentence(const std::wstring&, const ScreenVector, const int, const Color = { 255, 255, 255 });
			std::pair<int, int> GetImageLength(const resource::Image&) const noexcept;

			void PlaySound(const resource::Resource, PlayType), StopSound(const resource::Resource) noexcept;
			bool IsPlayingSound(const resource::Resource) const noexcept;
			
			TargetRender MakeScreen();
			void ChangeTargetScreen(TargetRender);
			TargetRender GetFrontScreen() const noexcept, GetBackScreen() const noexcept;
			void ClearScreen(), FlipScreen();

			void UpdateKeys() noexcept;
			bool IsPressingKey(const value::Key)const noexcept, IsPressedKey(const value::Key) const noexcept;
		};

		HWND handler;

		int UpdateMessage();
		inline void Frame();
	public:
		Program();
		int Main();
		HWND MainHandler() const noexcept;
		std::pair<int, int> WindowSize() const noexcept;

		std::chrono::system_clock::time_point GetTime();

		Engine engine;
		Canvas canvas;
		event::Manager event_manager;
		entity::Manager entity_manager;
		dll::Manager dll_manager;
		variable::Manager var_manager;
	};

	karapo::Program* GetProgram();
}