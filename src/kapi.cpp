#include "kapi.hpp"
namespace karapo {
	Entity::~Entity() {}
	event::Command::~Command() {}

	// キーの状態を更新する。
	void Key::Update() {
		if (press_requested)
			pressed_count++;
		else
			pressed_count = 0;

		press_requested = false;
	}

	void Key::Press() noexcept {
		press_requested = true;
	}

	bool Key::IsPressed() const noexcept {
		return pressed_count == 2;
	}

	bool Key::IsPressing() const noexcept {
		return press_requested > 0;
	}

	namespace resource {
		ResourceType::ResourceType(ProgramInterface* p) {
			pi = p;
		}

		void ResourceType::Load(const String& P) {
			path = P;
			if (Reload == nullptr) {
				Reload = [&]() { resource = pi->LoadImage(Path); };
			}
			Reload();
		}
	}
}