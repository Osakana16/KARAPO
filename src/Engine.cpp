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
}