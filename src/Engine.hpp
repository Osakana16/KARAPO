#pragma once
#include "Canvas.hpp"
#include <chrono>

namespace karapo {
	namespace variable {
		class Manager final : private Singleton {
			std::unordered_map<std::wstring, std::any> vars;
			Manager();
			~Manager() = default;
		public:
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

			static Manager& Instance() noexcept {
				static Manager manager;
				return manager;
			}
		};
	}

	namespace dll {
		class Manager final : private Singleton {
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

			Manager() = default;
			~Manager() = default;
			void Attach(const std::wstring&);
		public:
			void Detach(const std::wstring&);
			void Load(const std::wstring&);
			void LoadedInit();
			void Update();
			void RegisterExternalCommand(std::unordered_map<std::wstring, event::GenerateFunc>*);
			HMODULE Get(const std::wstring&) noexcept;

			static Manager& Instance() noexcept {
				static Manager manager;
				return manager;
			}
		};
	}

#undef LoadImage
#undef PlaySound

	enum class BlendMode {
		None,
		Add,
		Sub,
		Mul,
		Xor,
		Reverse
	};

	class Program final : private Singleton {
		class Engine final : private Singleton {
			struct FunctionalKey final {
				std::function<void()> func{};
				value::Key key{};
				bool (Program::Engine::*pressed)(value::Key) const noexcept = nullptr;
			} functional_key[256];

			std::vector<TargetRender> screens;
			std::unordered_map<std::wstring, resource::Resource> resources;

			bool fullscreen = false, synchronize = false, fixed = false;
			unsigned keys_state[256];
			Engine() noexcept, ~Engine() noexcept;
		public:
			void OnInit(Program*) noexcept;
			bool Failed() const noexcept;

			resource::Resource LoadImage(const std::wstring&) noexcept, LoadSound(const std::wstring&) noexcept;

			void SetBlend(const BlendMode, const int);
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
			value::Key GetKeyValueByString(const std::wstring&);
			void UpdateBindedKeys(), BindKey(std::wstring, std::function<void()>);
			
			static Engine& Instance() noexcept {
				static Engine engine;
				return engine;
			}
		};

		HWND handler;

		int UpdateMessage();
		inline void Frame();
		Program() = default;
		~Program() = default;
		std::vector<event::EventEditor> editors;
	public:
		static inline Program& Instance() noexcept {
			static Program instance;
			return instance;
		}

		event::EventEditor* MakeEventEditor();
		void FreeEventEditor(event::EventEditor*);

		int Main();
		void OnInit();
		HWND MainHandler() const noexcept;
		std::pair<int, int> WindowSize() const noexcept;

		std::chrono::system_clock::time_point GetTime();

		Engine& engine = Engine::Instance();
		Canvas& canvas = Canvas::Instance();
		event::Manager& event_manager = event::Manager::Instance();
		entity::Manager& entity_manager = entity::Manager::Instance();
		dll::Manager& dll_manager = dll::Manager::Instance();
		variable::Manager& var_manager = variable::Manager::Instance();
	};
}