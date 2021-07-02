/**
* Entity.hpp - ゲーム内のキャラクターやオブジェクトを表す為の定義群。
*/
#pragma once

namespace karapo::entity {
	class Object : public Entity {
	protected:
		WorldVector origin;
	public:
		WorldVector Origin() const noexcept override;
		void Teleport(WorldVector) override;
	};

	// 画像Entityクラス
	class Image : public Object {
		resource::Image image;
		bool can_delete = false;
		std::wstring path{};
		animation::FrameRef *frame{};
	protected:
		WorldVector length{};
		const decltype(path)& Path() const noexcept;
		const decltype(frame)& Frame() const noexcept;
	public:
		Image(const WorldVector&, const WorldVector&);
		inline int Main() override { return 0; }
		const wchar_t *Name() const noexcept override;
		const wchar_t *KindName() const noexcept override;
		bool CanDelete() const noexcept override;
		void Delete() override;
		void Draw(WorldVector) override;
		void Load(const std::wstring&), Load(animation::FrameRef*);
		WorldVector Length() const noexcept final;
	};

	// 音Entityクラス
	class Sound : public Object {
		resource::Sound sound;
		bool can_delete = false;
		bool can_play = true;
		std::wstring path{};
		PlayType play_type{};
	public:
		inline Sound(PlayType pt, WorldVector WV) {
			play_type = pt;
			origin = WV;
		}

		int Main() override;
		inline void Draw(WorldVector) override {}

		const wchar_t *Name() const noexcept override;
		const wchar_t *KindName() const noexcept override;
		bool CanDelete() const noexcept override;
		void Delete() override;
		void Play(const int = 0) noexcept;
		int Stop() noexcept;
		void Load(const std::wstring&);
		WorldVector Length() const noexcept final { return { 0.0, 0.0 }; }
	};

	class Text : public Object {
		std::wstring text{}, name{};
		bool can_delete = false;
	public:
		Text(const std::wstring&, const WorldVector&) noexcept;
		~Text() noexcept;

		int Main() override;
		void Draw(WorldVector) override;

		const wchar_t *Name() const noexcept override;
		const wchar_t *KindName() const noexcept override;
		bool CanDelete() const noexcept override;
		void Delete() override;

		void Print(const std::wstring&);
		WorldVector Length() const noexcept final { return { 0.0, 0.0 }; }
	};

	class Mouse : public Object {
		bool can_delete{};
	public:
		Mouse() noexcept;
		~Mouse() final {}
		int Main() final;
		const wchar_t *Name() const noexcept final;
		const wchar_t *KindName() const noexcept final;
		bool CanDelete() const noexcept final { return can_delete; }
		void Delete() final { can_delete = true; }
		void Draw(WorldVector) final {}
		WorldVector Length() const noexcept final { return { 0.0, 0.0 }; }
	};

	// ボタン
	class Button : public Image {
		std::wstring name{};
		bool collided_enough{};

		void Update();
		// 衝突時の処理
		void Collide() noexcept;

	public:
		Button(const std::wstring&, const WorldVector&, const WorldVector&) noexcept;

		int Main() override;
		const wchar_t *Name() const noexcept override;
		const wchar_t *KindName() const noexcept override;
		bool CanDelete() const noexcept override;
		void Delete() override;
		void Draw(WorldVector) override;
	};

	class StringNameManager final {
		const std::wstring Manager_Name{};
		std::wstring *managing_variable{};
	public:
		StringNameManager(const std::wstring&) noexcept;
		void Add(const std::shared_ptr<Entity>&), Remove(const std::shared_ptr<Entity>&);
		bool IsRegistered(const std::shared_ptr<Entity>&) const noexcept;
	};

	// Entityを一つの塊(配列)で管理するクラス
	class Chunk {
		std::unordered_map<std::wstring, std::shared_ptr<Entity>> entities{};
	public:
		// Entityを更新する。
		void Update() noexcept;
		// Entityを登録する。
		void Register(std::shared_ptr<Entity>&) noexcept;
		// 該当する名前のEntityを取得する。
		std::shared_ptr<Entity> Get(const std::wstring& Name) const noexcept;
		// 該当する条件のEntityを取得する。
		std::shared_ptr<Entity> Get(std::function<bool(std::shared_ptr<Entity>)> Condition) const noexcept;
		// 管理中の数を返す。
		size_t Size() const noexcept;
		// Entityを殺す。
		void Kill(const std::wstring& Entity_Name) noexcept;
		// 該当する名前のEntityをChunkから除外する。
		void Remove(const std::shared_ptr<Entity>& Target) noexcept;
	};

	// Entityを管理するクラス。
	class Manager final : private Singleton {
		std::unordered_set<std::wstring> freezable_entity_kind{},
			killable_entities{};
		std::vector<Chunk> chunks{};	// Entityを更新するチャンク。
		Chunk idle_chunk{};				// Entityを更新しないチャンク。
		Chunk glacial_chunk{};			// Entityを更新せず、いかなる操作の対象にもしないチャンク。

		error::UserErrorHandler error_handler{};
		error::ErrorClass *entity_error_class{};
		error::ErrorContent *entity_already_registered_warning{};

		Manager();
		~Manager() = default;
	public:
		StringNameManager name_manager;

		// Entityを更新する。
		void Update() noexcept;
		// 該当する名前のEntityを入手する。
		std::shared_ptr<Entity> GetEntity(const std::wstring& Name) const noexcept;
		// 関数内の条件に当てはまるEntityを入手する。
		std::shared_ptr<Entity> GetEntity(std::function<bool(std::shared_ptr<Entity>)> Condition) const noexcept;

		// 該当する名前のEntityを殺す。
		void Kill(const std::wstring&) noexcept;

		// 管理下にあるEntityの数を返す。
		size_t Amount() const noexcept;

		// Entityを管理下に置く。
		void Register(std::shared_ptr<Entity>) noexcept, Register(std::shared_ptr<Entity>, const std::wstring&) noexcept;

		// 該当する名前のEntityを更新対象から外す。
		bool Deactivate(std::shared_ptr<Entity>& target) noexcept;
		// 該当するEntityの種類を更新対象から外す。
		bool Deactivate(const std::wstring& Entity_Name) noexcept;
		// 該当する名前のEntityを更新対象にする。
		bool Activate(std::shared_ptr<Entity>& target) noexcept;
		// 該当するEntityの種類を更新対象にする。
		bool Activate(const std::wstring& Entity_Name) noexcept;

		// 該当する名前のEntityを更新対象から外す。
		bool Freeze(std::shared_ptr<Entity>& target) noexcept;
		// 該当するEntityの種類を更新対象から外す。
		bool Freeze(const std::wstring& Entity_Name) noexcept;
		// 該当する名前のEntityを更新対象にする。
		bool Defrost(std::shared_ptr<Entity>& target) noexcept;
		// 該当するEntityの種類を更新対象にする。
		bool Defrost(const std::wstring& Entity_Name) noexcept;

		static Manager& Instance() noexcept {
			static Manager manager;
			return manager;
		}
	};
}