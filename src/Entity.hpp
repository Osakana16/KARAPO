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
		resource::Image image = resource::Image(&Default_ProgramInterface);
		bool can_delete = false;
	public:
		inline Image(WorldVector WV) { origin = WV; }
		inline int Main() override { return 0; }
		const wchar_t *Name() const noexcept override;
		const wchar_t *KindName() const noexcept override;
		bool CanDelete() const noexcept override;
		void Delete() override;
		void Draw(WorldVector) override;
		void Load(const std::wstring&);
	};

	// 音Entityクラス
	class Sound : public Object {
		resource::Sound sound = resource::Sound(&Default_ProgramInterface);
		bool can_delete = false;
	public:
		inline Sound(WorldVector WV) { origin = WV; }

		int Main() override;
		inline void Draw(WorldVector) override {}

		const wchar_t *Name() const noexcept override;
		const wchar_t *KindName() const noexcept override;
		bool CanDelete() const noexcept override;
		void Delete() override;
		void Play(const PlayType) noexcept;
		void Load(const std::wstring&);
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
		void Kill(const std::wstring& name) noexcept;
	};

	// Entityを管理するクラス。
	class Manager final : private Singleton {
		std::vector<Chunk> chunks;

		Manager() = default;
		~Manager() = default;
	public:
		// Entityを更新する。
		void Update() noexcept;
		// 該当する名前のEntityを入手する。
		std::shared_ptr<Entity> GetEntity(const std::wstring& Name) noexcept;
		// 関数内の条件に当てはまるEntityを入手する。
		std::shared_ptr<Entity> GetEntity(std::function<bool(std::shared_ptr<Entity>)> Condition) noexcept;

		// 該当する名前のEntityを殺す。
		void Kill(const std::wstring&) noexcept;

		// 管理下にあるEntityの数を返す。
		size_t Amount() const noexcept;

		// Entityを管理下に置く。
		void Register(std::shared_ptr<Entity>) noexcept, Register(std::shared_ptr<Entity>, const std::wstring&) noexcept;

		static Manager& Instance() noexcept {
			static Manager manager;
			return manager;
		}
	};
}