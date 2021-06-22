#pragma once
#include "Canvas.hpp"
#include <chrono>

namespace karapo {
	namespace variable {
		// 構造体型
		struct Record final {
			std::unordered_map<std::wstring, std::any> members{};
		};

		class Manager final : private Singleton {
			std::unordered_map<std::wstring, std::any> vars;
			Manager();
			~Manager() = default;

			bool IsRecord(const std::wstring&) const noexcept;
		public:
			std::any& MakeNew(std::wstring);
			void Delete(const std::wstring&) noexcept;

			template<bool throw_except>
			std::any& Get(const std::wstring& Var_Name) noexcept(!throw_except) {
				if constexpr (throw_except) {
					return vars.at(Var_Name);
				} else {
					if (IsRecord(Var_Name)) {
						const auto&& Record_Name = Var_Name.substr(0, Var_Name.find(L'.'));
						auto var = vars.find(Record_Name);
						if (var != vars.end()) {
							const auto&& Member_Name = Var_Name.substr(Var_Name.find(L'.') + 1);
							auto& record = (var->second.type() == typeid(Record) ? std::any_cast<Record&>(var->second) : std::any_cast<Record&>(std::any_cast<std::reference_wrapper<std::any>&>(var->second).get()));
							auto mvar = record.members.find(Member_Name);
							if (mvar == record.members.end()) {
								goto no_member;
							}
							return mvar->second;
						} else {
							// 変数が見つからなかった場合、
							// ローカル変数を探す。

							auto event_name = std::any_cast<std::wstring>(Get<false>(variable::Executing_Event_Name));
							event_name.pop_back();
							auto pos = event_name.rfind(L'\n');
							if (pos != std::wstring::npos)
								event_name = event_name.substr(pos + 1);

							var = vars.find(event_name + L'@' + Record_Name);
							if (var == vars.end())
								goto no_member;

							const auto&& Member_Name = Var_Name.substr(Var_Name.find(L'.') + 1);
							auto& record = (var->second.type() == typeid(Record) ? std::any_cast<Record&>(var->second) : std::any_cast<Record&>(std::any_cast<std::reference_wrapper<std::any>&>(var->second).get()));			
							auto mvar = record.members.find(Member_Name);
							if (mvar == record.members.end()) {
								goto no_member;
							}
							return mvar->second;
						}
					} else {
						auto var = vars.find(Var_Name);
						if (var == vars.end()) {
							auto event_name = std::any_cast<std::wstring>(Get<false>(variable::Executing_Event_Name));
							event_name.pop_back();
							auto pos = event_name.rfind(L'\n');
							if (pos != std::wstring::npos)
								event_name = event_name.substr(pos + 1);

							// 変数が見つからなかった場合、
							// ローカル変数を探す。
							var = vars.find(event_name + L'@' + Var_Name);
							if (var == vars.end())
								goto no_member;
						}
						return var->second;
					}
				no_member:
					return vars[L"null"];
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
			unsigned keys_state[256], mouse_state[8];
			Engine() noexcept, ~Engine() noexcept;
		public:
			void OnInit(Program*) noexcept;
			bool Failed() const noexcept;

			resource::Resource LoadImage(const std::wstring&) noexcept, LoadSound(const std::wstring&) noexcept;
			// 指定領域を元の画像からコピーする。
			// コピー後、コピー元を示すpathをコピー先の名前に書き換える。
			resource::Resource CopyImage(std::wstring* path, const ScreenVector Position, const ScreenVector Length) noexcept;

			void SetBlend(const BlendMode, const int);
			void DrawLine(int, int, int, int, Color);
			void DrawRect(Rect, const resource::Image&) noexcept, DrawRect(Rect, const TargetRender) noexcept, DrawRect(Rect, Color, bool fill) noexcept;
			void DrawSentence(const std::wstring&, const ScreenVector, const int, const Color = { 255, 255, 255 });
			std::pair<int, int> GetImageLength(const resource::Image&) const noexcept;

			void PlaySound(const resource::Resource, PlayType, const bool = false), StopSound(const resource::Resource) noexcept;
			bool IsPlayingSound(const resource::Resource) const noexcept;
			int GetSoundPosition(const resource::Resource) const noexcept;
			void SetSoundPosition(const int, const resource::Resource) noexcept;
			
			TargetRender MakeScreen();
			void ChangeTargetScreen(TargetRender);
			TargetRender GetFrontScreen() const noexcept, GetBackScreen() const noexcept;
			void ClearScreen(), FlipScreen();

			void GetString(const ScreenVector&, wchar_t*, const size_t = 0u);

			void UpdateKeys() noexcept;
			bool IsPressingKey(const value::Key) const noexcept, IsPressedKey(const value::Key) const noexcept;
			bool IsPressedMouse(const value::Key) const noexcept, IsPressingMouse(const value::Key) const noexcept;
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
		std::list<event::EventEditor> editors{};
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