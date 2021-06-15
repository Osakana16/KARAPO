#include "Entity.hpp"

#include "Util.hpp"

#include "Engine.hpp"

#include <thread>
#include <future>
#include <forward_list>

namespace karapo::entity {
	Manager::Manager() {
		entity_error_class = error::UserErrorHandler::MakeErrorClass(L"entityエラー");
		entity_already_registered_warning = error::UserErrorHandler::MakeError(entity_error_class, L"既に同じ名前のEntityが存在する為、新しく登録できません。", MB_OK | MB_ICONWARNING, 1);
	}

	void Manager::Update() noexcept {
		for (auto& group : chunks) {
			std::thread th(&Chunk::Update, &group);
			th.join();
		}
		return;
	}

	std::shared_ptr<Entity> Manager::GetEntity(const std::wstring& Name) const noexcept {
		for (auto& group : chunks) {
			auto result = group.Get(Name);
			if (result != nullptr)
				return result;
		}
		return glacial_chunk.Get(Name);
	}

	std::shared_ptr<Entity> Manager::GetEntity(std::function<bool(std::shared_ptr<Entity>)> Condition) const noexcept {
		std::vector<std::shared_ptr<Entity>> results{};
		for (auto& group : chunks) {
			auto async = std::async(std::launch::async, [&group, Condition] { return group.Get(Condition); });
			results.push_back(async.get());
		}
		auto it = std::find_if(results.begin(), results.end(), [](std::shared_ptr<Entity> se) { return se != nullptr; });
		return (it != results.end() ? *it : glacial_chunk.Get(Condition));
	}

	void Manager::Kill(const std::wstring& Name) noexcept {
		for (auto& group : chunks) {
			group.Kill(Name);
		}
		glacial_chunk.Kill(Name);
	}

	// Entityの登録
	void Manager::Register(std::shared_ptr<Entity> entity) noexcept {
		Register(entity, L"");
	}

	void Manager::Register(std::shared_ptr<Entity> entity, const std::wstring& Layer_Name) noexcept {
		if (chunks.empty()) {
			chunks.push_back(Chunk());
		}

		if (GetEntity(entity->Name()) == nullptr) {
			Chunk *candidate{};
			if (freezable_entity_kind.find(entity->KindName()) != freezable_entity_kind.end()) {
				candidate = &glacial_chunk;
			} else {
				candidate = &chunks.front();
			}
			candidate->Register(entity);
			auto var = std::any_cast<std::wstring>(Program::Instance().var_manager.Get<false>(variable::Managing_Entity_Name));
			var += std::wstring(entity->Name()) + L"=" + std::wstring(entity->KindName()) + L"\n";
			Program::Instance().var_manager.Get<false>(variable::Managing_Entity_Name) = var;
			if (!Layer_Name.empty())
				Program::Instance().canvas.Register(entity, Layer_Name);
			else
				Program::Instance().canvas.Register(entity);
		} else {
			error_handler.SendLocalError(entity_already_registered_warning, std::wstring(L"登録しようとしたEntity名: ") + entity->Name());
			error_handler.ShowLocalError(4);
		}
	}

	size_t Manager::Amount() const noexcept {
		size_t amount = 0;
		for (auto& group : chunks) {
			amount += group.Size();
		}
		return amount;
	}

	// 該当する名前のEntityをchunksから除外し、glacial_chunkに移動する。
	bool Manager::Freeze(std::shared_ptr<Entity>& target) noexcept {
		if (target != nullptr) {
			glacial_chunk.Register(target);
			for (auto& chunk : chunks) {
				chunk.Remove(target);
			}
			return true;
		}
		return false;
	}


	bool Manager::Freeze(const std::wstring& Entity_Name) noexcept {
		freezable_entity_kind.insert(Entity_Name);
		return true;
	}

	// 該当する名前のEntityをglacial_chunkから外し、chunksに移動する。
	bool Manager::Defrost(std::shared_ptr<Entity>& target) noexcept {
		if (target != nullptr) {
			glacial_chunk.Remove(target);
			chunks.begin()->Register(target);
			return true;
		}
		return false;
	}

	// 該当する名前のEntityをglacial_chunkから外し、chunksに移動する。
	bool Manager::Defrost(const std::wstring& Entity_Name) noexcept {
		freezable_entity_kind.erase(Entity_Name);
		return true;
	}

	void Chunk::Update() noexcept {
		std::vector<const std::wstring*> deadmen;
		for (auto& ent : entities) {
			if (!ent.second->CanDelete())
				ent.second->Main();
			else
				deadmen.push_back(&ent.first);
		}

		if (!deadmen.empty()) {
			auto& var = Program::Instance().var_manager.Get<false>(variable::Managing_Entity_Name);
			auto str = std::any_cast<std::wstring>(var);
			for (auto& dead : deadmen) {
				auto pos = str.find(*dead);
				if (pos != str.npos) {
					auto name_begin = str.substr(0, pos - 1);
					auto name_end = str.substr(pos + dead->size());
					str = name_begin + name_end;
				}
				entities.erase(entities.find(*dead));
			}
			var = str;
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

	// 該当する名前のEntityをChunkの更新対象から外す。
	// Entity::Deleteは実行しない為、ゲームから削除するものではない。
	void Chunk::Remove(const std::shared_ptr<Entity>& Target) noexcept {
		entities.erase(Target->Name());
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
		path = Path;

		auto& var = Program::Instance().var_manager.Get<false>(std::wstring(Name()) + L".path");
		if (var.type() != typeid(std::nullptr_t)) {
			var = path;
		} else {
			Program::Instance().var_manager.MakeNew(std::wstring(Name()) + L".path") = path;
		}
		image = Program::Instance().engine.LoadImage(Path);
	}
	
	void Image::Load(animation::FrameRef* frame_reference) {
		frame = frame_reference;
	}

	WorldVector Image::Length() const noexcept {
		return length;
	}

	const decltype(Image::path)& Image::Path() const noexcept {
		return path;
	}

	const decltype(Image::frame)& Image::Frame() const noexcept {
		return frame;
	}

	void Image::Draw(WorldVector base) {
		Dec x, y;
		if (base[0] > .0 || base[1] > .0) {
			auto relative_origin = Origin() - base;
			relative_origin += { Program::Instance().WindowSize().first / 2.0, Program::Instance().WindowSize().second / 2.0 };
			x = relative_origin[0];
			y = relative_origin[1];
		} else {
			x = Origin()[0];
			y = Origin()[1];
		}

		if (frame != nullptr) {
			Program::Instance().engine.DrawRect(Rect({
						static_cast<int>(x),
						static_cast<int>(y),
						static_cast<int>(x + Length()[0]),
						static_cast<int>(y + Length()[1])
				}),
				*(*frame));
		} else {
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
	}

	bool Image::CanDelete() const noexcept {
		return can_delete;
	}

	void Image::Delete() {
		can_delete = true;
	}

	const wchar_t *Image::Name() const noexcept {
		return path.c_str();
	}

	const wchar_t *Image::KindName() const noexcept {
		return L"画像";
	}

	int Sound::Main() {
		Play(PlayType::Normal);
		return 0;
	}

	const wchar_t *Sound::Name() const noexcept {
		return path.c_str();
	}

	const wchar_t *Sound::KindName() const noexcept {
		return L"効果音";
	}

	void Sound::Play(const PlayType pt) noexcept {
		Program::Instance().engine.PlaySound(sound, pt);
	}

	void Sound::Load(const std::wstring& Path) {
		path = Path;
		sound = Program::Instance().engine.LoadSound(Path);
	}

	bool Sound::CanDelete() const noexcept {
		return can_delete || Program::Instance().engine.IsPlayingSound(sound);
	}

	void Sound::Delete() {
		can_delete = true;
	}

	Text::Text(const std::wstring& Name, const WorldVector& WV) noexcept {
		name = Name;
		origin = WV;
	}

	Text::~Text() noexcept {}

	int Text::Main() {
		text = std::any_cast<std::wstring>(Program::Instance().var_manager.Get<false>(name + L".text"));
		return 0;
	}

	void Text::Draw(const WorldVector Base) {
		Dec x, y;
		if (Base[0] > .0 || Base[1] > .0) {
			auto relative_origin = Origin() - Base;
			relative_origin += { Program::Instance().WindowSize().first / 2.0, Program::Instance().WindowSize().second / 2.0 };
			x = relative_origin[0];
			y = relative_origin[1];
		} else {
			x = Origin()[0];
			y = Origin()[1];
		}
		Program::Instance().engine.DrawSentence(text, ScreenVector{ (int)x, (int)y }, 30);
	}

	void Text::Print(const std::wstring& Message) {
		Program::Instance().var_manager.Get<false>(name + L".text") = Message;
	}

	const wchar_t *Text::Name() const noexcept {
		return name.c_str();
	}

	const wchar_t *Text::KindName() const noexcept {
		return L"テキスト";
	}

	bool Text::CanDelete() const noexcept {
		return can_delete;
	}

	void Text::Delete() {
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

		Program::Instance().var_manager.MakeNew(std::wstring(name) + L".w") = (int)std::lround(S[0]);
		Program::Instance().var_manager.MakeNew(std::wstring(name) + L".h") = (int)std::lround(S[1]);
	}

	int Button::Main() {
		Update();
		Collide();
		return 0;
	}

	void Button::Update() {
		{
			auto& path_var = Program::Instance().var_manager.Get<false>(std::wstring(name) + L".path");
			
			if (path_var.type() == typeid(std::wstring) && std::any_cast<std::wstring&>(path_var) != Path()) {
				Load(std::any_cast<std::wstring&>(path_var));
			} else if (path_var.type() == typeid(animation::FrameRef) && &std::any_cast<animation::FrameRef&>(path_var) != Frame()) {
				Load(&std::any_cast<animation::FrameRef&>(path_var));
			}
		}

		length[0] = std::any_cast<int>(Program::Instance().var_manager.Get<false>(std::wstring(name) + L".w"));
		length[1] = std::any_cast<int>(Program::Instance().var_manager.Get<false>(std::wstring(name) + L".h"));
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