#include "karapo.hpp"
namespace karapo {
	Entity::~Entity() {}
	event::Command::~Command() {}

	namespace resource {
		ResourceType& ResourceType::operator=(const Resource res) {
			resource = res;
			return (*this);
		}

		bool ResourceType::IsValid() const noexcept {
			return resource != Resource::Invalid;
		}
	}
}