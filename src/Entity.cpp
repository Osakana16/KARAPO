#include "Entity.hpp"

#include "Util.hpp"

#include "Engine.hpp"

#include <thread>
#include <future>
#include <forward_list>

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
		Register(entity, 0u);
	}

	void Manager::Register(SmartPtr<Entity> entity, const size_t Index) noexcept {
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
		GetProgram()->canvas.Register(entity, Index);
	}

	size_t Manager::Amount() const noexcept {
		size_t amount = 0;
		for (auto& group : entities) {
			amount += group.Size();
		}
		return amount;
	}

	Chunk::Chunk() {
		entities.clear();
		refs.clear();
	}

	void Chunk::Update() noexcept {
		Array<SmartPtr<Entity>*> deadmen;
		for (auto& ent : entities) {
			if (ent == nullptr)
				continue;

			if (!ent->CanDelete())
				ent->Main();
			else
				deadmen.push_back(&ent);
		}

		for (auto& dead : deadmen) {
			refs.erase((*dead)->Name());
			entities.erase(std::find(entities.begin(), entities.end(), (*dead)));
		}
	}

	SmartPtr<Entity> Chunk::Get(const String& Name) const noexcept {
		try {
			return SmartPtr<Entity>(refs.at(Name));
		} catch (std::out_of_range&) {
			return nullptr;
		}
	}

	SmartPtr<Entity> Chunk::Get(std::function<bool(SmartPtr<Entity>)> Condition) const noexcept {
		for (auto& ent : entities) {
			if (Condition(ent)) {
				return ent;
			}
		}
		return nullptr;
	}

	// 管理中の数を返す。
	size_t Chunk::Size() const noexcept {
		return refs.size();
	}

	// Entityを殺す。
	void Chunk::Kill(const String& name) noexcept {
		try {
			auto weak = refs.at(name);
			auto shared = weak.lock();
			if (shared) shared->Delete();
			else refs.erase(name);
		} catch (std::out_of_range& e) {
			// 発見されなければ何もしない。
		}
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