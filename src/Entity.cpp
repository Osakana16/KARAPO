#include "Entity.hpp"

#include "Util.hpp"

#include "Engine.hpp"

#include <thread>
#include <future>
#include <forward_list>

namespace karapo::entity {
	void Manager::Update() noexcept {
		for (auto& group : chunks) {
			std::thread th(&Chunk::Update, &group);
			th.join();
		}
		return;
	}

	std::shared_ptr<Entity> Manager::GetEntity(const std::wstring& Name) noexcept {
		for (auto& group : chunks) {
			auto result = group.Get(Name);
			if (result != nullptr)
				return result;
		}
		return nullptr;
	}

	std::shared_ptr<Entity> Manager::GetEntity(std::function<bool(std::shared_ptr<Entity>)> Condition) noexcept {
		std::vector<std::shared_ptr<Entity>> results;
		for (auto& group : chunks) {
			auto async = std::async(std::launch::async, [&group, Condition] { return group.Get(Condition); });
			results.push_back(async.get());
		}
		auto it = std::find_if(results.begin(), results.end(), [](std::shared_ptr<Entity> se) { return se != nullptr; });
		return (it != results.end() ? *it : nullptr);
	}

	void Manager::Kill(const std::wstring& Name) noexcept {
		for (auto& group : chunks) {
			group.Kill(Name);
		}
	}

	// Entityの登録
	void Manager::Register(std::shared_ptr<Entity> entity) noexcept {
		Register(entity, L"");
	}

	void Manager::Register(std::shared_ptr<Entity> entity, const std::wstring& Layer_Name) noexcept {
		decltype(chunks)::iterator candidate;
		if (chunks.empty()) {
			chunks.push_back(Chunk());
		}
		candidate = chunks.begin();
		candidate->Register(entity);
		auto var = std::any_cast<std::wstring>(Program::Instance().var_manager.Get<false>(variable::Managing_Entity_Name));
		var += std::wstring(entity->Name()) + L"=" + std::wstring(entity->KindName()) + L"\n";
		Program::Instance().var_manager.Get<false>(variable::Managing_Entity_Name) = var;
		if (!Layer_Name.empty())
			Program::Instance().canvas.Register(entity, Layer_Name);
		else
			Program::Instance().canvas.Register(entity);
	}

	size_t Manager::Amount() const noexcept {
		size_t amount = 0;
		for (auto& group : chunks) {
			amount += group.Size();
		}
		return amount;
	}

	void Chunk::Update() noexcept {
		std::vector<const std::wstring*> deadmen;
		for (auto& ent : entities) {
			if (!ent.second->CanDelete())
				ent.second->Main();
			else
				deadmen.push_back(&ent.first);
		}

		for (auto& dead : deadmen) {
			entities.erase(entities.find(*dead));
		}
	}

	void Chunk::Register(std::shared_ptr<Entity>& ent) noexcept {
		if (ent != nullptr)
			entities[ent->Name()] = ent;
	}

	std::shared_ptr<Entity> Chunk::Get(const std::wstring& Name) const noexcept {
		auto ent = entities.find(Name);
		return (ent != entities.end() ? ent->second : nullptr);
	}

	std::shared_ptr<Entity> Chunk::Get(std::function<bool(std::shared_ptr<Entity>)> Condition) const noexcept {
		for (auto& ent : entities) {
			if (Condition(ent.second)) {
				return ent.second;
			}
		}
		return nullptr;
	}

	// 管理中の数を返す。
	size_t Chunk::Size() const noexcept {
		return entities.size();
	}

	// Entityを殺す。
	void Chunk::Kill(const std::wstring& Name) noexcept {
		auto ent = entities.find(Name);
		if (ent != entities.end())
			ent->second->Delete();
	}

	WorldVector Object::Origin() const noexcept {
		return origin;
	}

	void Object::Teleport(WorldVector wv) {
		origin = wv;
	}

	Image::Image(const WorldVector& WV, const WorldVector& Size) {
		origin = WV;
		length = Size;
	}

	void Image::Load(const std::wstring& Path) {
		image.Load(Path);
	}

	WorldVector Image::Length() const noexcept {
		return length;
	}

	void Image::Draw(WorldVector base) {
		auto relative_origin = Origin() - base;
		relative_origin += { Program::Instance().WindowSize().first / 2.0, Program::Instance().WindowSize().second / 2.0 };
		auto& x = relative_origin[0], &y = relative_origin[1];
		if (image.IsValid()) {
			Program::Instance().engine.DrawRect(Rect({
					static_cast<int>(x),
					static_cast<int>(y),
					static_cast<int>(x + Length()[0]),
					static_cast<int>(y + Length()[1])
				}),
				image);
		} else {
			Program::Instance().engine.DrawRect(Rect({
					static_cast<int>(x),
					static_cast<int>(y),
					static_cast<int>(x + Length()[0]),
					static_cast<int>(y + Length()[1])
				}),
				{ .r = 0xEC, .g = 0x00, .b = 0x8C },
				true);
		}
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

	Mouse::Mouse() noexcept {
		Program::Instance().var_manager.MakeNew(L"マウスポインタ.左クリック") = 0;
		Program::Instance().var_manager.MakeNew(L"マウスポインタ.右クリック") = 0;
		Program::Instance().var_manager.MakeNew(L"マウスポインタ.中央クリック") = 0;
		origin = WorldVector{ (Dec)0, (Dec)0 };
	}

	int Mouse::Main() {
		Program::Instance().var_manager.Get<false>(L"マウスポインタ.左クリック") = 
			(int)Program::Instance().engine.IsPressingMouse(Default_ProgramInterface.keys.Left_Click);		
		Program::Instance().var_manager.Get<false>(L"マウスポインタ.右クリック") = 
			(int)Program::Instance().engine.IsPressingMouse(Default_ProgramInterface.keys.Right_Click);
		Program::Instance().var_manager.Get<false>(L"マウスポインタ.中央クリック") = 
			(int)Program::Instance().engine.IsPressingMouse(Default_ProgramInterface.keys.Wheel_Click);
		
		auto [x, y] = Default_ProgramInterface.GetMousePos();
		origin = WorldVector{ (Dec)x, (Dec)y };
		return 0;
	}

	const wchar_t *Mouse::Name() const noexcept {
		return L"マウスポインタ";
	}

	const wchar_t *Mouse::KindName() const noexcept {
		return L"マウスポインタ";
	}

	Button::Button(const std::wstring& N, const WorldVector& O, const WorldVector& S) noexcept : Image(O, S) {
		name = N;
	}

	int Button::Main() {
		Collide();
		return 0;
	}

	void Button::Collide() noexcept {
		const auto Colliding_Event_Name = std::wstring(Name()) + L".colliding",
			Collided_Event_Name = std::wstring(Name()) + L".collided",
			Clicking_Event_Name = std::wstring(Name()) + L".clicking";

		auto mouse = Program::Instance().entity_manager.GetEntity(L"マウスポインタ");
		auto dist = std::abs(mouse->Origin() - Image::Origin());
		if (dist[0] < Length()[0] && dist[1] < Length()[1]) {
			Program::Instance().event_manager.Call(Colliding_Event_Name);
			if (!collided_enough) {
				Program::Instance().event_manager.Call(Collided_Event_Name);
				collided_enough = true;
			}

			const bool Is_Clicking = std::any_cast<int>(Program::Instance().var_manager.Get<false>(L"マウスポインタ.左クリック"));
			if (Is_Clicking) {
				Program::Instance().event_manager.Call(Clicking_Event_Name);
			}
		} else {
			collided_enough = false;
		}
	}

	const wchar_t* Button::Name() const noexcept {
		return name.c_str();
	}

	const wchar_t* Button::KindName() const noexcept {
		return L"ボタン";
	}

	void Button::Delete() {
		Image::Delete();
	}

	bool Button::CanDelete() const noexcept {
		return Image::CanDelete();
	}

	void Button::Draw(WorldVector base) {
		Image::Draw(base);
	}
}