#include "Entity.hpp"

#include "Util.hpp"

#include "Engine.hpp"

#include <thread>
#include <future>
#include <forward_list>

karapo::Entity::~Entity(){}

namespace karapo::entity {
	void Manager::Update() noexcept {
		for (auto& group : entities) {
			std::thread th(&DefaultChunk::Update, &group);
			th.join();
		}
		return;
	}

	SmartPtr<Entity> Manager::GetEntity(const String& Name) noexcept {
		for (auto& group : entities) {
			auto result = group.Get(Name);
			if (result != nullptr)
				return result;
		}
		return nullptr;
	}

	SmartPtr<Entity> Manager::GetEntity(std::function<bool(SmartPtr<Entity>)> Condition) noexcept {
		std::vector<SmartPtr<Entity>> results;
		for (auto& group : entities) {
			auto async = std::async(std::launch::async, [&group, Condition] { return group.Get(Condition); });
			results.push_back(async.get());
		}
		auto it = std::find_if(results.begin(), results.end(), [](SmartPtr<Entity> se) { return se != nullptr; });
		return (it != results.end() ? *it : nullptr);
	}

	void Manager::Kill(const String& Name) noexcept {
		for (auto& group : entities) {
			group.Kill(Name);
		}
	}

	// Entityの登録
	void Manager::Register(SmartPtr<Entity> entity) noexcept {
		DefaultChunk *candidate = nullptr;
		for (auto& group : entities) {
			if (!group.IsFull()) {
				candidate = &group;
				break;
			}
		}

		if (candidate == nullptr) {
			entities.push_back(DefaultChunk());
			candidate = &entities.back();
		}
		candidate->Register(entity);
		GetProgram()->canvas.Register(entity, 0);
	}

	size_t Manager::Amount() const noexcept {
		size_t amount = 0;
		for (auto& group : entities) {
			amount += group.Size();
		}
		return amount;
	}

	WorldVector Object::Origin() const noexcept {
		return origin;
	}

	void Object::Teleport(WorldVector wv) {
		origin = wv;
	}

	void Image::Load(const String& Path) {
		image.Load(Path);
	}

	void Image::Draw(WorldVector, TargetRender) {
		GetProgram()->engine.DrawRect(Rect({ (LONG)Origin()[0], (LONG)Origin()[1], 0, 0 }), image);
	}

	bool Image::CanDelete() const noexcept {
		return can_delete;
	}

	void Image::Delete() {
		can_delete = true;
	}

	const Char *Image::Name() const noexcept {
		return image.Path.c_str();
	}

	int Sound::Main() {
		Play(PlayType::Normal);
		Delete();
		return 0;
	}

	const Char *Sound::Name() const noexcept {
		return sound.Path.c_str();
	}

	void Sound::Play(const PlayType pt) noexcept {
		GetProgram()->engine.PlaySound(sound, pt);
	}

	void Sound::Load(const String& Path) {
		sound.Load(Path);
	}

	bool Sound::CanDelete() const noexcept {
		return can_delete || GetProgram()->engine.IsPlayingSound(sound);
	}

	void Sound::Delete() {
		can_delete = true;
	}
}