#include "Engine.hpp"

namespace karapo {
	Program::Program() {
		if (engine.Failed())
			throw;

		engine.OnInit(this);
		canvas.CreateLayer<AbsoluteLayer*>();
	}

	int Program::Main() {
		event_manager.LoadEvent(L"main");

		while (UpdateMessage() == 0) {
			engine.ClearScreen();
			Frame();
			canvas.Update();
			event_manager.Update();
			entity_manager.Update();
			engine.FlipScreen();
		}
		return 0;
	}

	void Program::Frame() {
		static std::chrono::system_clock::time_point frame_start_time;        // 60‚e‚o‚rŒÅ’è—pAŠÔ•Û‘¶—p•Ï”
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
			dlls[Path] = LoadLibraryW(Path.c_str());
		}

		void Manager::Detach(const String& Path) {
			FreeLibrary(dlls[Path]);
			dlls.erase(Path);
		}

		HMODULE Manager::Get(const String& Name) noexcept {
			try {
				return dlls.at(Name);
			} catch (...) {
				return nullptr;
			}
		}
	}
}