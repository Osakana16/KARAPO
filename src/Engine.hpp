#pragma once
#include "Canvas.hpp"
#include <chrono>

namespace karapo {
	namespace variable {
		class Manager {
			std::unordered_map<String, std::any> vars;
		public:
			std::any& MakeNew(const String&);
			void Delete(const String&) noexcept;

			std::any& operator[](const String&) noexcept(false);
		};
	}

	namespace dll {
		class Manager {
			std::unordered_map<String, HMODULE> dlls;
		public:
			void Attach(const String&), Detach(const String&);
			HMODULE Get(const String&) noexcept;
		};
	}

#undef LoadImage
#undef PlaySound

	class Program {
		class Engine {
			TargetRender rendering_screen;
			Array<TargetRender> screens;
			std::unordered_map<String, resource::Resource> resources;

			bool fullscreen = false, synchronize = false, fixed = false;
		public:
			Engine() noexcept, ~Engine() noexcept;

			void OnInit(Program*) noexcept;
			bool Failed() const noexcept;

			resource::Resource LoadImage(const String&) noexcept, LoadSound(const String&) noexcept;

			void DrawLine(int, int, int, int, Color);
			void DrawRect(Rect, const resource::Image&) noexcept, DrawRect(Rect, const TargetRender) noexcept, DrawRect(Rect, Color, bool fill) noexcept;
			std::pair<int, int> GetImageLength(const resource::Image&) const noexcept;

			void PlaySound(const resource::Resource, PlayType), StopSound(const resource::Resource) noexcept;
			bool IsPlayingSound(const resource::Resource) const noexcept;
			
			TargetRender MakeScreen();
			void ChangeTargetScreen(TargetRender);
			TargetRender GetFrontScreen() const noexcept, GetBackScreen() const noexcept;
			void ClearScreen(), FlipScreen();
		};

		int width, height;
		HWND handler;

		int UpdateMessage();
		inline void Frame();
	public:
		Program();
		int Main();

		std::chrono::system_clock::time_point GetTime();

		Engine engine;
		Canvas canvas;
		event::Manager event_manager;
		entity::Manager entity_manager;
		dll::Manager dll_manager;
		variable::Manager var_manager;
		const int& Width = width, &Height = height;
	};

	karapo::Program* GetProgram();

	namespace key {
		KARAPO_NEWTYPE(Key, int);
		extern const Key
			F1,
			F2,
			F3,
			F4,
			F5,
			F6,
			F7,
			F8,
			F9,
			F1,
			F11,
			F12,
			Up,
			Left,
			Right,
			Down,
			App,
			Escape,
			Space,
			Backspace,
			Enter,
			Delete,
			Home,
			End,
			Insert,
			Page_Up,
			Page_Down,
			Tab,
			Caps,
			Pause,
			At,
			Colon,
			SemiColon,
			Plus,
			Minus,
			LBracket,
			RBracket,
			LCtrl,
			RCtrl,
			LShift,
			RShift,
			LAlt,
			RAlt,
			A,
			B,
			C,
			D,
			E,
			F,
			G,
			H,
			I,
			J,
			K,
			L,
			M,
			N,
			O,
			P,
			Q,
			R,
			S,
			T,
			U,
			V,
			W,
			X,
			Y,
			Z,
			N0,
			N1,
			N2,
			N3,
			N4,
			N5,
			N6,
			N7,
			N8,
			N9;
	}
}