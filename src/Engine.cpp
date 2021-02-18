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

	void Program::Engine::UpdateKeys() noexcept {
		char chs[256];
		DxLib::GetHitKeyStateAll(chs);

		for (int i = 0; i < 256; i++) {
			auto& key = keys_state[i];
			// ƒL[‚ª‰Ÿ‚³‚ê‚Ä‚¢‚éê‡
			if (chs[i]) 
				key++;
			else 
				key = 0;
		}
	}

	bool Program::Engine::IsPressedKey(const value::Key Any_Key) const noexcept {
		return (keys_state[static_cast<int>(Any_Key)] <= 2);
	}

	bool Program::Engine::IsPressingKey(const value::Key Any_Key) const noexcept {
		return (keys_state[static_cast<int>(Any_Key)] > 0);
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
		auto raw_screen = DxLib::MakeScreen(GetProgram()->WindowSize().first, GetProgram()->WindowSize().second);
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

	std::pair<int, int> Program::WindowSize() const noexcept {
		int w, h;
		DxLib::GetWindowSize(&w, &h);
		return { w, h };
	}

	ProgramInterface Default_ProgramInterface = {
		.GetHandler = []() -> HWND { return GetProgram()->MainHandler(); },
		.GetWindowLength = []() -> std::pair<int, int> { return GetProgram()->WindowSize(); },
		.DrawLine = [](int x1, int y1, int x2, int y2, Color c) { GetProgram()->engine.DrawLine(x1, y1, x2, y2, c); },
		.DrawRectImage = [](const Rect RC, const resource::Image& Img) { GetProgram()->engine.DrawRect(RC, Img); },
		.DrawRectScreen = [](const Rect RC, const karapo::TargetRender TR) { GetProgram()->engine.DrawRect(RC, TR); },
		.IsPlayingSound = [](const resource::Resource R) -> bool { return GetProgram()->engine.IsPlayingSound(R); },
		.LoadImage = [](const String& Path) -> resource::Resource { return GetProgram()->engine.LoadImage(Path); },
		.LoadSound = [](const String& Path) -> resource::Resource { return GetProgram()->engine.LoadSound(Path); },
		.CreateAbsoluteLayer = []()  -> size_t { return GetProgram()->canvas.CreateAbsoluteLayer(); },
		.CreateRelativeLayer = [](SmartPtr<Entity> entity) -> size_t { return GetProgram()->canvas.CreateRelativeLayer(entity); },
		.RegisterEntity = [](SmartPtr<Entity> entity, const size_t Index) { GetProgram()->entity_manager.Register(entity, Index); },
		.KillEntity = [](const String& Name) { GetProgram()->entity_manager.Kill(Name); },
		.GetEntityByName = [](const String& Name) -> SmartPtr<Entity> { return GetProgram()->entity_manager.GetEntity(Name); },
		.GetEntityByFunc = [](std::function<bool(SmartPtr<Entity>)> cond) { return GetProgram()->entity_manager.GetEntity(cond); },
		.ExecuteEventByName = [](const String& Name) { GetProgram()->event_manager.ExecuteEvent(Name); },
		.ExecuteEventByOrigin = [](const WorldVector& Origin) { GetProgram()->event_manager.ExecuteEvent(Origin); },
		.IsPressingKey = [](const value::Key Any_Key) noexcept -> bool { return GetProgram()->engine.IsPressingKey(Any_Key); },
		.IsPressedKey = [](const value::Key Any_Key) noexcept -> bool {return GetProgram()->engine.IsPressedKey(Any_Key); },
		.keys = {
			.F1 = (value::Key)KEY_INPUT_F1,
			.F2 = (value::Key)KEY_INPUT_F2,
			.F3 = (value::Key)KEY_INPUT_F3,
			.F4 = (value::Key)KEY_INPUT_F4,
			.F5 = (value::Key)KEY_INPUT_F5,
			.F6 = (value::Key)KEY_INPUT_F6,
			.F7 = (value::Key)KEY_INPUT_F7,
			.F8 = (value::Key)KEY_INPUT_F8,
			.F9 = (value::Key)KEY_INPUT_F9,
			.F10 = (value::Key)KEY_INPUT_F10,
			.F11 = (value::Key)KEY_INPUT_F11,
			.F12 = (value::Key)KEY_INPUT_F12,
			.Up = (value::Key)KEY_INPUT_UP,
			.Left = (value::Key)KEY_INPUT_LEFT,
			.Right = (value::Key)KEY_INPUT_RIGHT,
			.Down = (value::Key)KEY_INPUT_DOWN,
			.App = (value::Key)KEY_INPUT_APPS,
			.Escape = (value::Key)KEY_INPUT_ESCAPE,
			.Space = (value::Key)KEY_INPUT_SPACE,
			.Backspace = (value::Key)KEY_INPUT_BACK,
			.Enter = (value::Key)KEY_INPUT_ENTER,
			.Delete = (value::Key)KEY_INPUT_DELETE,
			.Home = (value::Key)KEY_INPUT_HOME,
			.End = (value::Key)KEY_INPUT_END,
			.Insert = (value::Key)KEY_INPUT_INSERT,
			.Page_Up = (value::Key)KEY_INPUT_PGUP,
			.Page_Down = (value::Key)KEY_INPUT_PGDN,
			.Tab = (value::Key)KEY_INPUT_TAB,
			.Caps = (value::Key)KEY_INPUT_CAPSLOCK,
			.Pause = (value::Key)KEY_INPUT_PAUSE,
			.At = (value::Key)KEY_INPUT_AT,
			.Colon = (value::Key)KEY_INPUT_COLON,
			.SemiColon = (value::Key)KEY_INPUT_SEMICOLON,
			.Plus = (value::Key)KEY_INPUT_SEMICOLON,
			.Minus = (value::Key)KEY_INPUT_MINUS,
			.LBracket = (value::Key)KEY_INPUT_LBRACKET,
			.RBracket = (value::Key)KEY_INPUT_RBRACKET,
			.LCtrl = (value::Key)KEY_INPUT_LCONTROL,
			.RCtrl = (value::Key)KEY_INPUT_RCONTROL,
			.LShift = (value::Key)KEY_INPUT_LSHIFT,
			.RShift = (value::Key)KEY_INPUT_RSHIFT,
			.LAlt = (value::Key)KEY_INPUT_LALT,
			.RAlt = (value::Key)KEY_INPUT_RALT,
			.A = (value::Key)KEY_INPUT_A,
			.B = (value::Key)KEY_INPUT_B,
			.C = (value::Key)KEY_INPUT_C,
			.D = (value::Key)KEY_INPUT_D,
			.E = (value::Key)KEY_INPUT_E,
			.F = (value::Key)KEY_INPUT_F,
			.G = (value::Key)KEY_INPUT_G,
			.H = (value::Key)KEY_INPUT_H,
			.I = (value::Key)KEY_INPUT_I,
			.J = (value::Key)KEY_INPUT_J,
			.K = (value::Key)KEY_INPUT_K,
			.L = (value::Key)KEY_INPUT_L,
			.M = (value::Key)KEY_INPUT_M,
			.N = (value::Key)KEY_INPUT_N,
			.O = (value::Key)KEY_INPUT_O,
			.P = (value::Key)KEY_INPUT_P,
			.Q = (value::Key)KEY_INPUT_Q,
			.R = (value::Key)KEY_INPUT_R,
			.S = (value::Key)KEY_INPUT_S,
			.T = (value::Key)KEY_INPUT_T,
			.U = (value::Key)KEY_INPUT_U,
			.V = (value::Key)KEY_INPUT_V,
			.W = (value::Key)KEY_INPUT_W,
			.X = (value::Key)KEY_INPUT_X,
			.Y = (value::Key)KEY_INPUT_Y,
			.Z = (value::Key)KEY_INPUT_Z,
			.N0 = (value::Key)KEY_INPUT_0,
			.N1 = (value::Key)KEY_INPUT_1,
			.N2 = (value::Key)KEY_INPUT_2,
			.N3 = (value::Key)KEY_INPUT_3,
			.N4 = (value::Key)KEY_INPUT_4,
			.N5 = (value::Key)KEY_INPUT_5,
			.N6 = (value::Key)KEY_INPUT_6,
			.N7 = (value::Key)KEY_INPUT_7,
			.N8 = (value::Key)KEY_INPUT_8,
			.N9 = (value::Key)KEY_INPUT_9
		}
	};
}