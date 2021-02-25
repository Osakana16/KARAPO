#include "Engine.hpp"

#include <fstream>
#include <filesystem>

namespace karapo {
	Program::Program() {
		if (engine.Failed())
			throw;

		engine.OnInit(this);
		canvas.CreateAbsoluteLayer();
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
			engine.UpdateKeys();
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

	HWND Program::MainHandler() const noexcept {
		return handler;
	}

	void Program::Frame() {
		static std::chrono::system_clock::time_point frame_start_time;
		frame_start_time = GetTime();
		while ((GetTime() - frame_start_time).count() < 1000 / 120) {}
		frame_start_time = GetTime();
	}

	std::chrono::system_clock::time_point Program::GetTime() {
		return std::chrono::system_clock::now();
	}

	namespace variable {
		Manager::Manager() {
			vars[L"null"] = nullptr;
		}

		std::any& Manager::MakeNew(const std::wstring& Name) {
			return vars[Name];
		}

		void Manager::Delete(const std::wstring& Name) noexcept {
			vars.erase(Name);
		}
	}

	namespace dll {
		void Manager::Attach(const std::wstring& Path) {
			dlls[Path].mod = LoadLibraryW(Path.c_str());
		}

		void Manager::Load(const std::wstring& Path) {
			Attach(Path);
			auto& dll = dlls.at(Path);
			dll.Init = reinterpret_cast<DLL::Initializer>(GetProcAddress(dll.mod, "KarapoDLLInit"));
			dll.Update = reinterpret_cast<DLL::LoopableMain>(GetProcAddress(dll.mod, "KarapoUpdate"));
			dll.RegisterExternalCommand = reinterpret_cast<DLL::EventRegister>(GetProcAddress(dll.mod, "RegisterCommand"));
			
			dll.Init(Default_ProgramInterface);
		}

		void Manager::Update() {
			std::vector<std::wstring> detach_lists;
			for (auto& dll : dlls) {
				if (!dll.second.Update()) {
					detach_lists.push_back(dll.first);
				}
			}

			while (!detach_lists.empty()) {
				auto path = detach_lists.back();
				Detach(path);
				detach_lists.pop_back();
			}
		}

		void Manager::RegisterExternalCommand(std::unordered_map<std::wstring, event::GenerateFunc>* words) {
			for (auto& dll : dlls) {
				dll.second.RegisterExternalCommand(words);
			}
		}

		void Manager::Detach(const std::wstring& Path) {
			FreeLibrary(dlls[Path].mod);
			dlls.erase(Path);
		}

		HMODULE Manager::Get(const std::wstring& Name) noexcept {
			try {
				return dlls.at(Name).mod;
			} catch (std::out_of_range&) {
				return nullptr;
			}
		}
	}
}