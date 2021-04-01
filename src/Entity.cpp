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

	std::shared_ptr<Entity> Manager::GetEntity(const std::wstring& Name) noexcept {
		for (auto& group : entities) {
			auto result = group.Get(Name);
			if (result != nullptr)
				return result;
		}
		return nullptr;
	}

	std::shared_ptr<Entity> Manager::GetEntity(std::function<bool(std::shared_ptr<Entity>)> Condition) noexcept {
		std::vector<std::shared_ptr<Entity>> results;
		for (auto& group : entities) {
			auto async = std::async(std::launch::async, [&group, Condition] { return group.Get(Condition); });
			results.push_back(async.get());
		}
		auto it = std::find_if(results.begin(), results.end(), [](std::shared_ptr<Entity> se) { return se != nullptr; });
		return (it != results.end() ? *it : nullptr);
	}

	void Manager::Kill(const std::wstring& Name) noexcept {
		for (auto& group : entities) {
			group.Kill(Name);
		}
	}

	// Entityの登録
	void Manager::Register(std::shared_ptr<Entity> entity) noexcept {
		Register(entity, 0u);
	}

	void Manager::Register(std::shared_ptr<Entity> entity, const size_t Index) noexcept {
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
		Program::Instance().canvas.Register(entity, Index);
		std::any_cast<std::wstring>(Program::Instance().var_manager.Get<false>(variable::Managing_Entity_Name)) 
			+= std::wstring(entity->Name()) + L"=" + std::wstring(entity->KindName()) + L"\n";
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
		std::vector<std::shared_ptr<Entity>*> deadmen;
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

	std::shared_ptr<Entity> Chunk::Get(const std::wstring& Name) const noexcept {
		try {
			return std::shared_ptr<Entity>(refs.at(Name));
		} catch (std::out_of_range&) {
			return nullptr;
		}
	}

	std::shared_ptr<Entity> Chunk::Get(std::function<bool(std::shared_ptr<Entity>)> Condition) const noexcept {
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
	void Chunk::Kill(const std::wstring& name) noexcept {
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

	void Image::Load(const std::wstring& Path) {
		image.Load(Path);
	}

	void Image::Draw(WorldVector) {
		Program::Instance().engine.DrawRect(Rect({ (LONG)Origin()[0], (LONG)Origin()[1], 0, 0 }), image);
	}

	bool Image::CanDelete() const noexcept {
		return can_delete;
	}

	void Image::Delete() {
		can_delete = true;
	}

	const wchar_t *Image::Name() const noexcept {
		return image.Path.c_str();
	}

	const wchar_t *Image::KindName() const noexcept {
		return L"画像";
	}

	int Sound::Main() {
		Play(PlayType::Normal);
		Delete();
		return 0;
	}

	const wchar_t *Sound::Name() const noexcept {
		return sound.Path.c_str();
	}

	const wchar_t *Sound::KindName() const noexcept {
		return L"効果音";
	}

	void Sound::Play(const PlayType pt) noexcept {
		Program::Instance().engine.PlaySound(sound, pt);
	}

	void Sound::Load(const std::wstring& Path) {
		sound.Load(Path);
	}

	bool Sound::CanDelete() const noexcept {
		return can_delete || Program::Instance().engine.IsPlayingSound(sound);
	}

	void Sound::Delete() {
		can_delete = true;
	}
}