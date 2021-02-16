#include "kapi.hpp"
namespace karapo {
	Entity::~Entity() {}
	event::Command::~Command() {}

	namespace resource {
		Image::Image(ProgramInterface* pi) {
			Reload = [&]() { resource = pi->LoadImage(Path); };
		}

		Sound::Sound(ProgramInterface* pi) {
			Reload = [&]() { resource = pi->LoadSound(Path); };
		}
	}
}