#include "Engine.hpp"

#include <fstream>
#include <filesystem>

namespace karapo {
	ProgramInterface Default_ProgramInterface = {
		.DrawLine = [](int x1, int y1, int x2, int y2, Color c) { GetProgram()->engine.DrawLine(x1, y1, x2, y2, c); },
		.DrawRectImage = [](const Rect RC, const resource::Image& Img) { GetProgram()->engine.DrawRect(RC, Img); },
		.DrawRectScreen = [](const Rect RC, const karapo::TargetRender TR) { GetProgram()->engine.DrawRect(RC, TR); },
		.IsPlayingSound = [](const resource::Resource R) -> bool { return GetProgram()->engine.IsPlayingSound(R); },
		.LoadImage = [](const String& Path) -> resource::Resource { return GetProgram()->engine.LoadImage(Path); },
		.LoadSound = [](const String& Path) -> resource::Resource { return GetProgram()->engine.LoadSound(Path); },
		.RegisterEntity = [](SmartPtr<Entity> entity) { GetProgram()->entity_manager.Register(entity); },
		.KillEntity = [](const String& Name) { GetProgram()->entity_manager.Kill(Name); },
		.GetEntity = [](const String& Name) -> SmartPtr<Entity> { return GetProgram()->entity_manager.GetEntity(Name); },
		.ExecuteEventByName = [](const String& Name) { GetProgram()->event_manager.ExecuteEvent(Name); },
		.ExecuteEventByOrigin = [](const WorldVector& Origin) { GetProgram()->event_manager.ExecuteEvent(Origin); }
	};

	Program::Program() {
		if (engine.Failed())
			throw;

		engine.OnInit(this);
		canvas.CreateLayer<AbsoluteLayer*>();
	}

	int Program::Main() {
		try {
			// DLLフォルダ内のDLLを全て読み込む。
			for (const auto& dll_dir : std::filesystem::directory_iterator(L"DLL")) {
				auto& path = dll_dir.path();
				if (In(path.c_str(), L".dll"))
					dll_manager.Load(path);
			}
		} catch (std::filesystem::filesystem_error& error) {
			MessageBoxA(nullptr, error.what(), "エラー", MB_OK | MB_ICONERROR);
		}

		while (UpdateMessage() == 0) {
			engine.ClearScreen();
			Frame();
			dll_manager.Update();
			canvas.Update();
			event_manager.Update();
			entity_manager.Update();
			engine.FlipScreen();
		}
		return 0;
	}

	void Program::Frame() {
		static std::chrono::system_clock::time_point frame_start_time;        // 60ＦＰＳ固定用、時間保存用変数
		frame_start_time = GetTime();
		while ((GetTime() - frame_start_time).count() < 1000 / 120) {}
		frame_start_time = GetTime();
	}

	std::chrono::system_clock::time_point Program::GetTime() {
		return std::chrono::system_clock::now();
	}

	namespace variable {
		std::any& Manager::MakeNew(const String& Name) {
			return vars[Name];
		}

		void Manager::Delete(const String& Name) noexcept {
			vars.erase(Name);
		}

		std::any& Manager::operator[](const String& Name) noexcept(false) {
			return vars.at(Name);
		}
	}

	namespace dll {
		void Manager::Attach(const String& Path) {
			dlls[Path].mod = LoadLibraryW(Path.c_str());
		}

		void Manager::Load(const String& Path) {
			Attach(Path);
			auto& dll = dlls.at(Path);
			dll.Init = reinterpret_cast<DLL::Initializer>(GetProcAddress(dll.mod, "KarapoDLLInit"));
			dll.Update = reinterpret_cast<DLL::LoopableMain>(GetProcAddress(dll.mod, "KarapoUpdate"));
			
			dll.Init(Default_ProgramInterface);
		}

		void Manager::Update() {
			std::queue<String> detach_lists;
			for (auto& dll : dlls) {
				if (!dll.second.Update()) {
					detach_lists.push(dll.first);
				}
			}

			while (!detach_lists.empty()) {
				auto path = detach_lists.front();
				Detach(path);
				detach_lists.pop();
			}
		}

		void Manager::Detach(const String& Path) {
			FreeLibrary(dlls[Path].mod);
			dlls.erase(Path);
		}

		HMODULE Manager::Get(const String& Name) noexcept {
			try {
				return dlls.at(Name).mod;
			} catch (...) {
				return nullptr;
			}
		}
	}
}