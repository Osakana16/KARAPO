#include "Entity.hpp"

#include "Util.hpp"

#include "Engine.hpp"

#include <thread>
#include <future>

namespace karapo::entity {
	Manager::Manager() : name_manager(L"entity::Manager") {
		entity_error_class = error::UserErrorHandler::MakeErrorClass(L"entityエラー");
		entity_already_registered_warning = error::UserErrorHandler::MakeError(entity_error_class, L"既に同じ名前のEntityが存在する為、新しく登録できません。", MB_OK | MB_ICONWARNING, 1);
	}

	void Manager::Update() noexcept {
		if (!killable_entities.empty()) {
			for (const auto& Name : killable_entities) {
				for (auto& group : chunks) {
					group.Kill(Name);
				}
				idle_chunk.Kill(Name);
				glacial_chunk.Kill(Name);
			}
			killable_entities.clear();
		}

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
		return idle_chunk.Get(Name);
	}

	std::shared_ptr<Entity> Manager::GetEntity(std::function<bool(std::shared_ptr<Entity>)> Condition) const noexcept {
		std::vector<std::shared_ptr<Entity>> results{};
		for (auto& group : chunks) {
			auto async = std::async(std::launch::async, [&group, Condition] { return group.Get(Condition); });
			results.push_back(async.get());
		}
		auto it = std::find_if(results.begin(), results.end(), [](std::shared_ptr<Entity> se) { return se != nullptr; });
		return (it != results.end() ? *it : idle_chunk.Get(Condition));
	}

	std::shared_ptr<Entity> Manager::GetEntity(std::function<bool(std::shared_ptr<Entity>)> Condition, const std::shared_ptr<Entity>& Base) const noexcept {
		std::vector<std::shared_ptr<Entity>> results{};
		for (auto& group : chunks) {
			auto async = std::async(std::launch::async, [&group, Condition, Base] { return group.Get(Condition, Base); });
			results.push_back(async.get());
		}
		auto it = std::find_if(results.begin(), results.end(), [](std::shared_ptr<Entity> se) { return se != nullptr; });
		return (it != results.end() ? *it : idle_chunk.Get(Condition, Base));
	}

	void Manager::Kill(const std::wstring& Name) noexcept {
		killable_entities.insert(Name);
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
				candidate = &idle_chunk;
			} else {
				candidate = &chunks.front();
			}
			candidate->Register(entity);
			name_manager.Add(entity);
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

	bool Manager::Freeze(std::shared_ptr<Entity>& target) noexcept {
		if (target != nullptr) {
			glacial_chunk.Register(target);
			idle_chunk.Remove(target);
			return true;
		}
		return false;
	}

	bool Manager::Freeze(const std::wstring& Name) noexcept {
		auto ent = idle_chunk.Get(Name);
		if (ent != nullptr) {
			Freeze(ent);
			return true;
		}
		return false;
	}

	bool Manager::Defrost(std::shared_ptr<Entity>& target) noexcept {
		if (target != nullptr) {
			glacial_chunk.Remove(target);
			idle_chunk.Register(target);
			return true;
		}
		return false;
	}

	bool Manager::Defrost(const std::wstring& Entity_Name) noexcept {
		auto ent = glacial_chunk.Get(Entity_Name);
		if (ent != nullptr) {
			Defrost(ent);
			return true;
		}
		return false;
	}

	// 該当する名前のEntityをchunksから除外し、idle_chunkに移動する。
	bool Manager::Deactivate(std::shared_ptr<Entity>& target) noexcept {
		if (target != nullptr) {
			idle_chunk.Register(target);
			for (auto& chunk : chunks) {
				chunk.Remove(target);
			}
			return true;
		}
		return false;
	}

	bool Manager::Deactivate(const std::wstring& Name) noexcept {
		auto ent = GetEntity(Name);
		if (ent == nullptr) {
			const auto& Kind_Name = Name;
			freezable_entity_kind.insert(Kind_Name);
			bool found_chunk{};
			for (auto&& chunk : chunks) {
				ent = chunk.Get([&Kind_Name](std::shared_ptr<Entity> ent) {
						return ent->KindName() == Kind_Name;
					}
				);

				if (ent != nullptr) {
					found_chunk = true;
					Deactivate(ent);
				}
			} 
			if (found_chunk) {
				goto end_of_function;
			}
		} else {
			Deactivate(ent);
		}
	end_of_function:
		return true;
	}

	// 該当する名前のEntityをidle_chunkから外し、chunksに移動する。
	bool Manager::Activate(std::shared_ptr<Entity>& target) noexcept {
		if (target != nullptr) {
			idle_chunk.Remove(target);
			chunks.begin()->Register(target);
			return true;
		}
		return false;
	}

	// 該当する名前のEntityをidle_chunkから外し、chunksに移動する。
	bool Manager::Activate(const std::wstring& Entity_Name) noexcept {
		freezable_entity_kind.erase(Entity_Name);
		return true;
	}

	StringNameManager::StringNameManager(const std::wstring& Name) noexcept : Manager_Name(Name) {
		variable::Manager::Instance().MakeNew(Manager_Name + L".__管理中") = std::wstring();
		managing_variable = &std::any_cast<std::wstring&>(variable::Manager::Instance().MakeNew(Manager_Name + L".__管理中"));
	}

	void StringNameManager::Add(const std::shared_ptr<Entity>& Ent) {
		*managing_variable += std::wstring(Ent->Name()) + L'=' + std::wstring(Ent->KindName()) + L'\\';
	}

	void StringNameManager::Remove(const std::shared_ptr<Entity>& Ent) {
		if (IsRegistered(Ent)) {
			const auto Position = managing_variable->find(Ent->Name());
			managing_variable->erase(Position, managing_variable->find(L'\\', Position) + 1);
		}
	}

	bool StringNameManager::IsRegistered(const std::shared_ptr<Entity>& Ent) const noexcept {
		return (managing_variable->find(Ent->Name()) != std::wstring::npos);
	}

	void Chunk::Update() noexcept {
		for (auto& ent : entities) {
			ent.second->Main();
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

	std::shared_ptr<Entity> Chunk::Get(std::function<bool(std::shared_ptr<Entity>)> Condition, const std::shared_ptr<Entity>& Base) const noexcept {
		std::unordered_multimap<int, std::shared_ptr<Entity>> entity_interval{};
		for (auto& ent : entities) {
			int i = static_cast<int>(std::sqrt(std::pow(ent.second->Origin()[0], 2) + std::pow(ent.second->Origin()[1], 2)));
			entity_interval.insert({ i / 1000, ent.second });
		}

		const int Index = static_cast<int>(std::sqrt(std::pow(Base->Origin()[0], 2) + std::pow(Base->Origin()[1], 2))) / 1000;
		auto interval = entity_interval.find(Index);
		if (interval != entity_interval.end()) {
			auto range = entity_interval.equal_range(Index);
			for (auto ent = range.first; ent != range.second; ent++) {
				if (Condition(ent->second)) {
					return ent->second;
				}
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
		if (ent != entities.end()) {
			Manager::Instance().name_manager.Remove(ent->second);
			Program::Instance().canvas.Remove(ent->second);
			entities.erase(ent);
		}
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

		auto& var = Program::Instance().var_manager.Get(std::wstring(Name()) + L".path");
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
		if (can_play && (play_type == PlayType::Normal || (play_type == PlayType::Loop && !Program::Instance().engine.IsPlayingSound(sound))))
			Play();
		return 0;
	}

	const wchar_t *Sound::Name() const noexcept {
		return path.c_str();
	}

	const wchar_t *Sound::KindName() const noexcept {
		return L"効果音";
	}

	void Sound::Play(const int Play_Position) noexcept {
		can_play = true;
		//Program::Instance().engine.SetSoundPosition(Play_Position, sound);
		Program::Instance().engine.PlaySound(sound, play_type, (Play_Position > 0));
	}

	int Sound::Stop() noexcept {
		can_play = false;
		const int Play_Position = Program::Instance().engine.GetSoundPosition(sound);
		Program::Instance().engine.StopSound(sound);
		return Play_Position;
	}

	void Sound::Load(const std::wstring& Path) {
		path = Path;
		sound = Program::Instance().engine.LoadSound(Path);
	}

	bool Sound::CanDelete() const noexcept {
		return can_delete;
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
		text = std::any_cast<std::wstring>(Program::Instance().var_manager.Get(name + L".text"));
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
		auto& font = Program::Instance().var_manager.Get(Name() + std::wstring(L".font"));
		auto& color_var = Program::Instance().var_manager.Get(Name() + std::wstring(L".rgb"));
		union { uint32_t value{}; uint8_t rgb[3]; } color_code;
		Color color{ .r=255, .g=255, .b=255 };
		if (color_var.type() == typeid(int)) {
			auto s = std::any_cast<int>(color_var);
			color_code.value = std::any_cast<int>(color_var);
			color.r = color_code.rgb[2];
			color.g = color_code.rgb[1];
			color.b = color_code.rgb[0];
		}

		if (font.type() == typeid(resource::Resource))
			Program::Instance().engine.DrawSentence(text, ScreenVector{ (int)x, (int)y }, std::any_cast<resource::Resource&>(font), color);
		else
			Program::Instance().engine.DrawSentence(text, ScreenVector{ (int)x, (int)y }, resource::Resource::Invalid, color);
	}

	void Text::Print(const std::wstring& Message) {
		Program::Instance().var_manager.Get(name + L".text") = Message;
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
		Program::Instance().var_manager.Get(L"マウスポインタ.左クリック") = 
			(int)Program::Instance().engine.IsPressingMouse(Default_ProgramInterface.keys.Left_Click);		
		Program::Instance().var_manager.Get(L"マウスポインタ.右クリック") = 
			(int)Program::Instance().engine.IsPressingMouse(Default_ProgramInterface.keys.Right_Click);
		Program::Instance().var_manager.Get(L"マウスポインタ.中央クリック") = 
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
			auto& path_var = Program::Instance().var_manager.Get(std::wstring(name) + L".path");
			
			if (path_var.type() == typeid(std::wstring) && std::any_cast<std::wstring&>(path_var) != Path()) {
				Load(std::any_cast<std::wstring&>(path_var));
			} else if (path_var.type() == typeid(animation::FrameRef) && &std::any_cast<animation::FrameRef&>(path_var) != Frame()) {
				Load(&std::any_cast<animation::FrameRef&>(path_var));
			}
		}

		length[0] = std::any_cast<int>(Program::Instance().var_manager.Get(std::wstring(name) + L".w"));
		length[1] = std::any_cast<int>(Program::Instance().var_manager.Get(std::wstring(name) + L".h"));
	}

	void Button::Collide() noexcept {
		const auto Colliding_Event_Name = std::wstring(Name()) + L".colliding",
			Collided_Event_Name = std::wstring(Name()) + L".collided",
			Clicking_Event_Name = std::wstring(Name()) + L".clicking",
			Released_Event_Name = std::wstring(Name()) + L".released";

		auto mouse = Program::Instance().entity_manager.GetEntity(L"マウスポインタ");
		if (Image::Origin()[0] < mouse->Origin()[0] && Image::Origin()[1] < mouse->Origin()[1] && 
			Image::Origin()[0] + Image::Length()[0] >= mouse->Origin()[0] && Image::Origin()[1] + Image::Length()[1] >= mouse->Origin()[1])
		{
			Program::Instance().event_manager.Call(Colliding_Event_Name);
			if (!collided_enough) {
				Program::Instance().event_manager.Call(Collided_Event_Name);
				collided_enough = true;
			}

			const bool Is_Clicking = std::any_cast<int>(Program::Instance().var_manager.Get(L"マウスポインタ.左クリック"));
			if (Is_Clicking) {
				Program::Instance().event_manager.Call(Clicking_Event_Name);
			}
		} else {
			if (collided_enough) {
				Program::Instance().event_manager.Call(Released_Event_Name);
				collided_enough = false;
			}
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