#include "kapi.hpp"
namespace karapo {
	Entity::~Entity() {}

	namespace resource {
		Image::Image(ProgramInterface* pi) {
			Reload = [&]() { resource = pi->LoadImage(Path); };
		}

		Sound::Sound(ProgramInterface* pi) {
			Reload = [&]() { resource = pi->LoadSound(Path); };
		}
	}
}