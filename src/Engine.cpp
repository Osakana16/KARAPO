#include "Engine.hpp"

#include "DxLib/DxLib.h"

namespace karapo {
	Program::Engine::Engine() noexcept {
		SetOutApplicationLogValidFlag(FALSE);

		constexpr auto Process = L"process";
		constexpr auto Window = L"window";
		constexpr auto Config_File = L"Config/program.ini";

		wchar_t name[255]{ 0 };
		GetPrivateProfileStringW(Window, L"name", nullptr, name, 255u, Config_File);
		SetMainWindowText(name);
		SetGraphMode(GetPrivateProfileIntW(Window, L"width", 800, Config_File), GetPrivateProfileIntW(Window, L"height", 800, Config_File), 32);
		ChangeWindowMode(!GetPrivateProfileIntW(Window, L"fullscreen", 0, Config_File));
		SetAlwaysRunFlag(GetPrivateProfileIntW(Window, L"synchronize", 0, Config_File));
		SetWindowSizeChangeEnableFlag(GetPrivateProfileIntW(Window, L"fixed", 0, Config_File));

		SetWaitVSyncFlag(GetPrivateProfileIntW(Process, L"vsync", 0, Config_File));
		SetMultiThreadFlag(1);

		SetTransColor(255, 255, 255);
		SetUseMenuFlag(TRUE);

		DxLib_Init();

		screens.push_back(static_cast<TargetRender>(DX_SCREEN_FRONT));
		screens.push_back(static_cast<TargetRender>(DX_SCREEN_BACK));
	}

	void Program::Engine::OnInit(Program* program) noexcept {
		program->handler = GetMainWindowHandle();
	}

	void Program::Engine::DrawLine(int x1, int y1, int x2, int y2, Color c) {
		DxLib::DrawLine(x1, y1, x2, y2, GetColor(c.r, c.b, c.g));
	}

	void Program::Engine::DrawRect(RECT p, const resource::Image& Img) noexcept {
		int r = p. right, b = p.bottom;
		if (r <= 0 || b <= 0) {
			auto [i, j] = GetImageLength(Img);
			r = p.left + i;
			b = p.top + j;
		}
		DxLib::DrawExtendGraph(p.left, p.top, r, b, static_cast<raw::TargetRender>(static_cast<resource::Resource>(Img)), 1);
	}

	void Program::Engine::DrawRect(RECT p, const TargetRender t) noexcept {
		DxLib::DrawExtendGraph(p.left, p.top, p.right, p.bottom, static_cast<raw::TargetRender>(t), 1);
	}

	void Program::Engine::DrawRect(RECT p, Color c, bool fill) noexcept {
		DxLib::DrawBox(p.left, p.top, p.right, p.bottom, GetColor(c.r, c.b, c.g), fill);
	}

	std::pair<resource::Image::Length, resource::Image::Length> Program::Engine::GetImageLength(const resource::Image& I)  const noexcept {
		int lx, ly;
		DxLib::GetGraphSize(static_cast<raw::Resource>(static_cast<resource::Resource>(I)), &lx, &ly);
		return { lx, ly };
	}

	void Program::Engine::PlaySound(const resource::Resource R, PlayType pt) {
		DxLib::PlaySoundMem(static_cast<raw::TargetRender>(R), static_cast<raw::TargetRender>(pt));
	}

	void Program::Engine::StopSound(const resource::Resource R) noexcept {
		DxLib::StopSoundMem(static_cast<raw::TargetRender>(R));
	}

	bool Program::Engine::IsPlayingSound(const resource::Resource R) const noexcept {
		return DxLib::CheckSoundMem(static_cast<raw::TargetRender>(R));
	}

	bool Program::Engine::Failed() const noexcept {
		return !DxLib_IsInit();
	}

	resource::Resource Program::Engine::LoadImage(const String& Path) noexcept {
		resource::Resource r;
		try {
			r = resources.at(Path);
		} catch (...) {
			r = static_cast<resource::Resource>(LoadGraph(Path.c_str()));
			if (r != resource::Resource::Invalid) {
				resources[Path] = r;
			}
		}
		return r;
	}

	resource::Resource Program::Engine::LoadSound(const String& Path) noexcept {
		resource::Resource r;
		try {
			r = resources.at(Path);
		} catch (...) {
			r = static_cast<resource::Resource>(LoadSoundMem(Path.c_str()));
			if (r != resource::Resource::Invalid) {
				resources[Path] = r;
			}
		}
		return r;
	}

	Program::Engine::~Engine() noexcept {
		DxLib_End();
	}

	TargetRender Program::Engine::GetFrontScreen() const noexcept {
		return screens[0];
	}

	TargetRender Program::Engine::GetBackScreen() const noexcept {
		return screens[1];
	}

	TargetRender Program::Engine::MakeScreen() {
		auto raw_screen = DxLib::MakeScreen(GetProgram()->Width, GetProgram()->Height);
		auto screen = static_cast<TargetRender>(raw_screen);
		if (raw_screen > -1)
			screens.push_back(screen);
		return screen;
	}

	void Program::Engine::ChangeTargetScreen(TargetRender t) {
		if (std::find(screens.begin(), screens.end(), t) != screens.end())
			rendering_screen = t;
	}

	void Program::Engine::ClearScreen() {
		ClearDrawScreen();
	}

	void Program::Engine::FlipScreen() {
		ScreenFlip();
	}

	int Program::UpdateMessage() {
		return ProcessMessage();
	}

	namespace key {
		const Key
			F1 = (Key)KEY_INPUT_F1,
			F2 = (Key)KEY_INPUT_F2,
			F3 = (Key)KEY_INPUT_F3,
			F4 = (Key)KEY_INPUT_F4,
			F5 = (Key)KEY_INPUT_F5,
			F6 = (Key)KEY_INPUT_F6,
			F7 = (Key)KEY_INPUT_F7,
			F8 = (Key)KEY_INPUT_F8,
			F9 = (Key)KEY_INPUT_F9,
			F10 = (Key)KEY_INPUT_F10,
			F11 = (Key)KEY_INPUT_F11,
			F12 = (Key)KEY_INPUT_F12,
			Up = (Key)KEY_INPUT_UP,
			Left = (Key)KEY_INPUT_LEFT,
			Right = (Key)KEY_INPUT_RIGHT,
			Down = (Key)KEY_INPUT_DOWN,
			App = (Key)KEY_INPUT_APPS,
			Escape = (Key)KEY_INPUT_ESCAPE,
			Space = (Key)KEY_INPUT_SPACE,
			Backspace = (Key)KEY_INPUT_BACK,
			Enter = (Key)KEY_INPUT_ENTER,
			Delete = (Key)KEY_INPUT_DELETE,
			Home = (Key)KEY_INPUT_HOME,
			End = (Key)KEY_INPUT_END,
			Insert = (Key)KEY_INPUT_INSERT,
			Page_Up = (Key)KEY_INPUT_PGUP,
			Page_Down = (Key)KEY_INPUT_PGDN,
			Tab = (Key)KEY_INPUT_TAB,
			Caps = (Key)KEY_INPUT_CAPSLOCK,
			Pause = (Key)KEY_INPUT_PAUSE,
			At = (Key)KEY_INPUT_AT,
			Colon = (Key)KEY_INPUT_COLON,
			SemiColon = (Key)KEY_INPUT_SEMICOLON,
			Plus = (Key)KEY_INPUT_SEMICOLON,
			Minus = (Key)KEY_INPUT_MINUS,
			LBracket = (Key)KEY_INPUT_LBRACKET,
			RBracket = (Key)KEY_INPUT_RBRACKET,
			LCtrl = (Key)KEY_INPUT_LCONTROL,
			RCtrl = (Key)KEY_INPUT_RCONTROL,
			LShift = (Key)KEY_INPUT_LSHIFT,
			RShift = (Key)KEY_INPUT_RSHIFT,
			LAlt = (Key)KEY_INPUT_LALT,
			RAlt = (Key)KEY_INPUT_RALT,
			A = (Key)KEY_INPUT_A,
			B = (Key)KEY_INPUT_B,
			C = (Key)KEY_INPUT_C,
			D = (Key)KEY_INPUT_D,
			E = (Key)KEY_INPUT_E,
			F = (Key)KEY_INPUT_F,
			G = (Key)KEY_INPUT_G,
			H = (Key)KEY_INPUT_H,
			I = (Key)KEY_INPUT_I,
			J = (Key)KEY_INPUT_J,
			K = (Key)KEY_INPUT_K,
			L = (Key)KEY_INPUT_L,
			M = (Key)KEY_INPUT_M,
			N = (Key)KEY_INPUT_N,
			O = (Key)KEY_INPUT_O,
			P = (Key)KEY_INPUT_P,
			Q = (Key)KEY_INPUT_Q,
			R = (Key)KEY_INPUT_R,
			S = (Key)KEY_INPUT_S,
			T = (Key)KEY_INPUT_T,
			U = (Key)KEY_INPUT_U,
			V = (Key)KEY_INPUT_V,
			W = (Key)KEY_INPUT_W,
			X = (Key)KEY_INPUT_X,
			Y = (Key)KEY_INPUT_Y,
			Z = (Key)KEY_INPUT_Z,
			N0 = (Key)KEY_INPUT_0,
			N1 = (Key)KEY_INPUT_1,
			N2 = (Key)KEY_INPUT_2,
			N3 = (Key)KEY_INPUT_3,
			N4 = (Key)KEY_INPUT_4,
			N5 = (Key)KEY_INPUT_5,
			N6 = (Key)KEY_INPUT_6,
			N7 = (Key)KEY_INPUT_7,
			N8 = (Key)KEY_INPUT_8,
			N9 = (Key)KEY_INPUT_9;
	}
}


namespace karapo {
	// 指定されたキーの状態を監視するクラス
	template<int...keyargs>
	class KeyWatcher {
		static constexpr auto Keys_Length = sizeof...(keyargs);
		static_assert(Keys_Length <= 256, L"Too many keys are assigned.");
		static_assert(Keys_Length > 0, L"No keys are assigned. Please assign one or more keys.");

		static constexpr int Keys[Keys_Length] = { keyargs... };
	public:
		// 押されているキーを返す
		// (押されたタイミングは考慮しない、要素の順番はキーコードの数値に依存)
		int PressingKey() const noexcept {
			int state = 0;

			char chs[256];
			GetHitKeyStateAll(chs);

			if constexpr (Keys_Length > 1) {
				for (int i = 0; i < 256; i++) {
					// キーが押されている & そのキーが監視中の一つである時
					if (chs[i] == 1 && std::find(std::begin(Keys), std::end(Keys), i) != std::end(Keys)) {
						state = i;
						break;
					}
				}
			} else {
				if (chs[Keys[0]] == 1)
					state = Keys[0];
			}
			return state;
		}
	};
}