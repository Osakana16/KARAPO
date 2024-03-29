#include "Engine.hpp"

#include "DxLib/DxLib.h"

namespace karapo {
	Program::Engine::Engine() noexcept {
		SetOutApplicationLogValidFlag(FALSE);

		constexpr auto Process = L"process";
		constexpr auto Window = L"window";
		constexpr auto Config_File = L"./config.ini";

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

	void Program::Engine::SetBlend(const BlendMode Mode, const int potency) {
		constexpr int Modes[] = {
			DX_BLENDMODE_NOBLEND,
			DX_BLENDMODE_ADD,
			DX_BLENDMODE_SUB,
			DX_BLENDMODE_MUL,
			DX_BLENDMODE_XOR,
			DX_BLENDMODE_INVSRC
		};

		DxLib::SetDrawBlendMode(Modes[static_cast<int>(Mode)], potency % 256);
	}

	void Program::Engine::DrawLine(int x1, int y1, int x2, int y2, Color c) {
		DxLib::DrawLine(x1, y1, x2, y2, GetColor(c.r, c.g, c.b));
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
		DxLib::DrawBox(p.left, p.top, p.right, p.bottom, GetColor(c.r, c.g, c.b), fill);
	}

	void Program::Engine::DrawSentence(const std::wstring& Mes, const ScreenVector O, const int Font_Size, const Color C) {
		DxLib::SetFontSize(Font_Size);
		DxLib::DrawString(O[0], O[1], Mes.c_str(), GetColor(C.r, C.g, C.b));
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

	resource::Resource Program::Engine::LoadImage(const std::wstring& Path) noexcept {
		if (auto r = resources.find(Path); r != resources.end()) {
			return r->second;
		} else {
			auto i = static_cast<resource::Resource>(LoadGraph(Path.c_str()));
			if (i != resource::Resource::Invalid) {
				resources[Path] = i;
			}
			return i;
		}
	}

	resource::Resource Program::Engine::CopyImage(std::wstring* path, const ScreenVector Position, const ScreenVector Length) noexcept {
		auto r = resources.find(*path);
		if (r == resources.end()) {
			LoadImage(*path);
			r = resources.find(*path);
		}
		auto copied = static_cast<resource::Resource>(
			DxLib::DerivationGraph(Position[0], Position[1], Length[0], Length[1], static_cast<int>(r->second))
			);

		size_t count{};
		for (;; count++) {
			auto result = resources.count(*path + L'.' + std::to_wstring(count));
			if (result == 0)
				break;
		}
		*path = *path + L'.' + std::to_wstring(count);
		resources[*path] = copied;
		return copied;
	}

	resource::Resource Program::Engine::LoadSound(const std::wstring& Path) noexcept {
		if (auto r = resources.find(Path); r != resources.end()) {
			return r->second;
		} else {
			auto i = static_cast<resource::Resource>(LoadSoundMem(Path.c_str()));
			if (i != resource::Resource::Invalid) {
				resources[Path] = i;
			}
			return i;
		}
	}

	void Program::Engine::GetString(const ScreenVector& Position, wchar_t* to, const size_t Length) {
		DxLib::KeyInputString(Position[0], Position[1], Length, to, 1);
	}

	void Program::Engine::UpdateKeys() noexcept {
		char chs[256];
		DxLib::GetHitKeyStateAll(chs);

		for (int i = 0; i < 256; i++) {
			auto& key = keys_state[i];
			// キーが押されている場合
			if (chs[i]) 
				key++;
			else 
				key = 0;
		}

		for (int i = 0; i < 8; i++) {
			int mouse = DxLib::GetMouseInput();
			auto& key = mouse_state[i];
			if (mouse & i)
				key++;
			else
				key = 0;
		}
	}

	bool Program::Engine::IsPressedKey(const value::Key Any_Key) const noexcept {
		return (IsPressingKey(Any_Key) && keys_state[static_cast<int>(Any_Key)] <= 1);
	}

	bool Program::Engine::IsPressingKey(const value::Key Any_Key) const noexcept {
		return (keys_state[static_cast<int>(Any_Key)] > 0);
	}

	bool Program::Engine::IsPressedMouse(const value::Key Any_Key) const noexcept {
		return (IsPressingMouse(Any_Key) && mouse_state[static_cast<int>(Any_Key)] <= 1);
	}

	bool Program::Engine::IsPressingMouse(const value::Key Any_Key) const noexcept {
		return (mouse_state[static_cast<int>(Any_Key)] > 0);
	}

	value::Key Program::Engine::GetKeyValueByString(const std::wstring& Key_Name) {
		std::unordered_map<std::wstring, int> special_key{
			{ L"a", KEY_INPUT_A },
			{ L"b", KEY_INPUT_B },
			{ L"c", KEY_INPUT_C },
			{ L"d", KEY_INPUT_D },
			{ L"e", KEY_INPUT_E },
			{ L"f", KEY_INPUT_F },
			{ L"g", KEY_INPUT_G },
			{ L"h", KEY_INPUT_H },
			{ L"i", KEY_INPUT_I },
			{ L"j", KEY_INPUT_J },
			{ L"k", KEY_INPUT_K },
			{ L"l", KEY_INPUT_L },
			{ L"m", KEY_INPUT_M },
			{ L"n", KEY_INPUT_N },
			{ L"o", KEY_INPUT_O },
			{ L"p", KEY_INPUT_P },
			{ L"q", KEY_INPUT_Q },
			{ L"r", KEY_INPUT_R },
			{ L"s", KEY_INPUT_S },
			{ L"t", KEY_INPUT_T },
			{ L"u", KEY_INPUT_U },
			{ L"v", KEY_INPUT_V },
			{ L"w", KEY_INPUT_W },
			{ L"x", KEY_INPUT_X },
			{ L"y", KEY_INPUT_Y },
			{ L"z", KEY_INPUT_Z },
			{ L"tab", KEY_INPUT_TAB },
			{ L"space", KEY_INPUT_SPACE },
			{ L"enter", KEY_INPUT_RETURN },
			{ L"escape", KEY_INPUT_ESCAPE },
			{ L"insert", KEY_INPUT_INSERT },
			{ L"home", KEY_INPUT_HOME },
			{ L"end", KEY_INPUT_END },
			{ L"delete", KEY_INPUT_DELETE },
			{ L"pageup", KEY_INPUT_PGUP },
			{ L"pagedown", KEY_INPUT_PGDN },
			{ L"printscreen", KEY_INPUT_SYSRQ },
			{ L"-", KEY_INPUT_MINUS },
			{ L"^", KEY_INPUT_PREVTRACK },
			{ L"lctrl", KEY_INPUT_LCONTROL },
			{ L"rctrl", KEY_INPUT_RCONTROL },
			{ L"lshift", KEY_INPUT_LSHIFT },
			{ L"rshift", KEY_INPUT_RSHIFT },
			{ L"lalt", KEY_INPUT_LALT },
			{ L"ralt", KEY_INPUT_RALT },
			{ L"capslock", KEY_INPUT_CAPSLOCK },
			{ L"apps", KEY_INPUT_APPS },
			{ L"pause", KEY_INPUT_PAUSE },
			{ L"convert", KEY_INPUT_CONVERT },
			{ L"変換", KEY_INPUT_CONVERT },
			{ L"noconvert", KEY_INPUT_NOCONVERT },
			{ L"無変換", KEY_INPUT_NOCONVERT },
			{ L"/", KEY_INPUT_SLASH },
			{ L"\\", KEY_INPUT_BACKSLASH },
			{ L",", KEY_INPUT_COMMA },
			{ L".", KEY_INPUT_PERIOD },
			{ L";", KEY_INPUT_SEMICOLON },
			{ L":", KEY_INPUT_COLON },
			{ L"[", KEY_INPUT_LBRACKET },
			{ L"]", KEY_INPUT_RBRACKET },
			{ L"f1", KEY_INPUT_F1 },
			{ L"f2", KEY_INPUT_F2 },
			{ L"f3", KEY_INPUT_F3 },
			{ L"f4", KEY_INPUT_F4 },
			{ L"f5", KEY_INPUT_F5 },
			{ L"f6", KEY_INPUT_F6 },
			{ L"f7", KEY_INPUT_F7 },
			{ L"f8", KEY_INPUT_F8 },
			{ L"f9", KEY_INPUT_F9 },
			{ L"f10", KEY_INPUT_F10 },
			{ L"f11", KEY_INPUT_F11 },
			{ L"f12", KEY_INPUT_F12 },
			{ L"leftclick", MOUSE_INPUT_LEFT },
			{ L"左クリック", MOUSE_INPUT_LEFT },
			{ L"rightclick", MOUSE_INPUT_RIGHT },
			{ L"右クリック", MOUSE_INPUT_RIGHT },
			{ L"wheelclick", MOUSE_INPUT_MIDDLE },
			{ L"中央クリック", MOUSE_INPUT_MIDDLE }
		};

		if (iswdigit(Key_Name[0])) {
			if (Key_Name[0] == L'0')
				return static_cast<value::Key>(KEY_INPUT_0);
			else
				return static_cast<value::Key>(std::abs(L'1' - std::tolower(Key_Name[0])) + KEY_INPUT_1);
		} else {
			auto result = special_key.find(Key_Name);
			if (result != special_key.end())
				return static_cast<value::Key>(result->second);
		}
		return static_cast<value::Key>(-1);
	}

	void Program::Engine::UpdateBindedKeys() {
		for (auto& fk : functional_key) {
			if (fk.pressed != nullptr && (this->*fk.pressed)(fk.key))
				fk.func();
		}
	}

	void Program::Engine::BindKey(std::wstring key_name, std::function<void()> func) {
		bool is_one_press_mode = false;
		retry:
		if (key_name[0] == L'+' && key_name.size() > 1) {
			key_name.erase(key_name.begin());
			is_one_press_mode = true;
			goto retry;
		}
		int key_value = static_cast<int>(GetKeyValueByString(key_name));
		if (key_value > -1) {
			functional_key[key_value].func = func;
			functional_key[key_value].key = static_cast<value::Key>(key_value);
			functional_key[key_value].pressed = (is_one_press_mode ? &Program::Engine::IsPressedKey : &Program::Engine::IsPressingKey);
		}
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
		auto raw_screen = DxLib::MakeScreen(Program::Instance().WindowSize().first, Program::Instance().WindowSize().second, TRUE);
		auto screen = static_cast<TargetRender>(raw_screen);
		if (raw_screen > -1)
			screens.push_back(screen);
		return screen;
	}

	void Program::Engine::ChangeTargetScreen(TargetRender target) {
		if (std::find(screens.begin(), screens.end(), target) != screens.end()) {
			DxLib::SetDrawScreen(static_cast<int>(target));
		}
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
		.GetHandler = []() -> HWND { return Program::Instance().MainHandler(); },
		.GetWindowLength = []() -> std::pair<int, int> { return Program::Instance().WindowSize(); },
		.DrawLine = [](int x1, int y1, int x2, int y2, Color c) { Program::Instance().engine.DrawLine(x1, y1, x2, y2, c); },
		.DrawRect = [](const Rect RC, const Color C, const bool Fill) { Program::Instance().engine.DrawRect(RC, C, Fill); },
		.DrawRectImage = [](const Rect RC, const resource::Image& Img) { Program::Instance().engine.DrawRect(RC, Img); },
		.DrawRectScreen = [](const Rect RC, const karapo::TargetRender TR) { Program::Instance().engine.DrawRect(RC, TR); },
		.DrawSentence = [](const std::wstring& Mes, const ScreenVector O, const int Font_Size, const Color C) { Program::Instance().engine.DrawSentence(Mes, O, Font_Size, C); },
		.IsPlayingSound = [](const resource::Resource R) -> bool { return Program::Instance().engine.IsPlayingSound(R); },
		.LoadImage = [](const std::wstring& Path) -> resource::Resource { return Program::Instance().engine.LoadImage(Path); },
		.LoadSound = [](const std::wstring& Path) -> resource::Resource { return Program::Instance().engine.LoadSound(Path); },
		.CreateAbsoluteLayer = [](const std::wstring& Name) { return Program::Instance().canvas.CreateAbsoluteLayer(Name); },
		.CreateRelativeLayer = [](const std::wstring& Name) { return Program::Instance().canvas.CreateRelativeLayer(Name); },
		.InsertAbsoluteLayer = [](const std::wstring& Name, const int Index) { return Program::Instance().canvas.CreateAbsoluteLayer(Name, Index); },
		.InsertRelativeLayer = [](const std::wstring& Name, const int Index) { return Program::Instance().canvas.CreateRelativeLayer(Name, Index); },
		.DeleteLayerByIndex = [](const int Index) { return Program::Instance().canvas.DeleteLayer(Index); },
		.DeleteLayerByName = [](const std::wstring& Name) { return Program::Instance().canvas.DeleteLayer(Name); },
		.GetLayerInfo = [](const int Index) { return Program::Instance().canvas.GetLayerInfo(Index); },
		.MakeErrorClass = [](const wchar_t *Error_Title) { return error::UserErrorHandler::MakeErrorClass(Error_Title); },
		.MakeError = [](error::ErrorClass* error_class, const wchar_t* Error_Message, const int MB_Type, const unsigned Level) {
			return error::UserErrorHandler::MakeError(error_class, Error_Message, MB_Type, Level);
		},
		.SendGlobalError = [](error::ErrorContent* error_content, const std::wstring& Sub_Sentence, void(*func)(const int)) {
			error::UserErrorHandler::SendGlobalError(error_content, Sub_Sentence, func);
		},
		.RegisterEntity = [](std::shared_ptr<Entity> entity) { Program::Instance().entity_manager.Register(entity); },
		.RegisterEntityByLayerName = [](std::shared_ptr<Entity> entity, const std::wstring& Layer_Name) { Program::Instance().entity_manager.Register(entity, Layer_Name); },
		.KillEntity = [](const std::wstring& Name) { Program::Instance().entity_manager.Kill(Name); },
		.GetEntityByName = [](const std::wstring& Name) -> std::shared_ptr<Entity> { return Program::Instance().entity_manager.GetEntity(Name); },
		.GetEntityByFunc = [](std::function<bool(std::shared_ptr<Entity>)> cond) { return Program::Instance().entity_manager.GetEntity(cond); },
		.LoadEvent = [](const std::wstring& Path) { Program::Instance().event_manager.LoadEvent(Path); },
		.ExecuteEventByName = [](const std::wstring& Name) { Program::Instance().event_manager.Call(Name); },
		.ExecuteEventByOrigin = [](const WorldVector& Origin) { Program::Instance().event_manager.ExecuteEvent(Origin); },
		.MakeVar = [](const std::wstring& Var_Name) -> std::any& { return Program::Instance().var_manager.MakeNew(Var_Name); },
		.GetVar = [](const std::wstring& Var_Name) -> std::any& { return Program::Instance().var_manager.Get<false>(Var_Name); },
		.GetEventEditor = []() { return Program::Instance().MakeEventEditor(); },
		.MakeNewEvent = [](event::EventEditor* editor, const std::wstring& Event_Name) { editor->MakeNewEvent(Event_Name); },
		.SetTargetEvent = [](event::EventEditor* editor, const std::wstring& Event_Name) { editor->SetTarget(Event_Name); },
		.ChangeEventTriggerType = [](event::EventEditor* editor, const std::wstring& TT) { editor->ChangeTriggerType(TT); },
		.AddCommand = [](event::EventEditor* editor, const std::wstring& Sentence, const int Index) { editor->AddCommand(Sentence, Index); },
		.ChangeEventRange = [](event::EventEditor* editor, const WorldVector Min, const WorldVector Max) { editor->ChangeRange(Min, Max); },
		.FreeEventEditor = [](event::EventEditor* editor) { Program::Instance().FreeEventEditor(editor); },
		.GetParamInfo = [](const std::wstring& Param) -> std::pair<std::wstring, std::wstring> {
			// 引数の情報。
			const auto Index = Param.rfind(L':');
			const auto Var = (Index == Param.npos ? Param : Param.substr(0, Index));
			const auto Type = (Index == Param.npos ? L"" : Param.substr(Index + 1));
			return { Var, Type };
		},
		.IsStringType = [](const std::wstring& Param_Type) { return Param_Type == event::innertype::String; },
		.IsNumberType = [](const std::wstring& Param_Type) { return Param_Type == event::innertype::Number; },
		.IsNoType = [](const std::wstring& Param_Type) { return Param_Type == event::innertype::None; },
		.IsUndecidedType = [](const std::wstring& Param_Type) { return Param_Type == event::innertype::Undecided; },
		.GetMousePos = []() -> std::pair<int, int> { int x, y; GetMousePoint(&x, &y); return { x, y }; },
		.IsPressingKey = [](const value::Key Any_Key) noexcept -> bool { return Program::Instance().engine.IsPressingKey(Any_Key); },
		.IsPressedKey = [](const value::Key Any_Key) noexcept -> bool {return Program::Instance().engine.IsPressedKey(Any_Key); },
		.IsPressedMouse = [](const value::Key Any_Key) noexcept -> bool {return Program::Instance().engine.IsPressedMouse(Any_Key); },
		.IsPressingMouse = [](const value::Key Any_Key) noexcept -> bool {return Program::Instance().engine.IsPressingMouse(Any_Key); },
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
			.Enter = (value::Key)KEY_INPUT_RETURN,
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
			.N9 = (value::Key)KEY_INPUT_9,
			.Left_Click = (value::Key)MOUSE_INPUT_LEFT,
			.Right_Click = (value::Key)MOUSE_INPUT_RIGHT,
			.Wheel_Click = (value::Key)MOUSE_INPUT_MIDDLE
		}
	};
}