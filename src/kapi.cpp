#include "kapi.hpp"
namespace karapo {
	Entity::~Entity() {}
	event::Command::~Command() {}

	namespace resource {
		ResourceType::ResourceType(ProgramInterface* p) {
			pi = p;
		}

		void ResourceType::Load(const std::wstring& P) {
			path = P;
			if (Reload == nullptr) {
				Reload = [&]() { resource = pi->LoadImage(Path); };
			}
			Reload();
		}

		bool ResourceType::IsValid() const noexcept {
			return resource != Resource::Invalid;
		}
	}
}