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
		const Char *Name() const noexcept override;
		bool CanDelete() const noexcept override;
		void Delete() override;
		void Draw(WorldVector, TargetRender) override;
		void Load(const String&);
	};

	// 音Entityクラス
	class Sound : public Object {
		resource::Sound sound = resource::Sound(&Default_ProgramInterface);
		bool can_delete = false;
	public:
		inline Sound(WorldVector WV) { origin = WV; }

		int Main() override;
		inline void Draw(WorldVector, TargetRender) override {}

		const Char *Name() const noexcept override;
		bool CanDelete() const noexcept override;
		void Delete() override;
		void Play(const PlayType) noexcept;
		void Load(const String&);
	};

	// Entityを一つの塊(配列)で管理するクラス
	class Chunk {
		std::vector<SmartPtr<Entity>> entities;
		std::unordered_map<String, std::weak_ptr<Entity>> refs;
	public:
		Chunk();
		// Entityを更新する。
		void Update() noexcept;
		// 該当する名前のEntityを取得する。
		SmartPtr<Entity> Get(const String& Name) const noexcept;
		// 該当する条件のEntityを取得する。
		SmartPtr<Entity> Get(std::function<bool(SmartPtr<Entity>)> Condition) const noexcept;
		// 管理中の数を返す。
		size_t Size() const noexcept;
		// Entityを殺す。
		void Kill(const String& name) noexcept;
	};

	// Entityを一つの塊(配列)で管理するクラス(固定長版)
	template<uint N>
	class FixedChunk {
		SmartPtr<Entity> entities[N]{ nullptr };
		std::unordered_map<String, std::weak_ptr<Entity>> refs;
	public:
		FixedChunk() noexcept {
			refs.clear();
		}

		// Entityを更新する。
		auto Update() noexcept {
			for (auto& ent : entities) {
				if (ent == nullptr)
					continue;

				if (!ent->CanDelete())
					ent->Main();
				else {
					refs.erase(ent->Name());
					ent = nullptr;
				}
			}
		}

		// 管理中の数を返す。
		size_t Size() const noexcept {
			return refs.size();
		}

		// この塊が満タンかを返す。
		bool IsFull() const noexcept {
			for (auto& ent : entities) {
				if (ent == nullptr)
					return false;
			}
			return true;
		}

		// Entityを登録する。
		auto Register(SmartPtr<Entity> ent) {
			Register(ent, ent->Name());
		}

		// Entityを登録する。
		auto Register(SmartPtr<Entity> ent, const String& Name) {
			if (ent == nullptr)
				return;

			refs[ent->Name()] = ent;
			for (int i = 0; i < N; i++) {
				if (entities[i] == nullptr) {
					entities[i] = ent;
					break;
				}
			}
		}

		// Entityを殺す。
		auto Kill(const String& name) noexcept {
			try {
				auto weak = refs.at(name);
				auto shared = weak.lock();
				if (shared) shared->Delete();
				else refs.erase(name);
			} catch (std::out_of_range& e) {
				// 発見されなければ何もしない。
			}
		}

		// 該当する名前のEntityを取得する。
		SmartPtr<Entity> Get(const String& Name) const noexcept {
			SmartPtr<Entity> ent = nullptr;
			try {
				std::weak_ptr<Entity> weak = refs.at(Name);
				ent = weak.lock();
			} catch (...) {}
			return ent;
		}

		// 該当する条件のEntityを取得する。
		SmartPtr<Entity> Get(std::function<bool(SmartPtr<Entity>)> Condition) const noexcept {
			for (auto& ent : entities) {
				if (Condition(ent)) {
					return ent;
				}
			}
			return nullptr;
		}
	};

	using DefaultChunk = FixedChunk<1000>;

	// Entityを管理するクラス。
	class Manager {
		std::vector<DefaultChunk> entities;
	public:
		// Entityを更新する。
		void Update() noexcept;
		// 該当する名前のEntityを入手する。
		SmartPtr<Entity> GetEntity(const String& Name) noexcept;
		// 関数内の条件に当てはまるEntityを入手する。
		SmartPtr<Entity> GetEntity(std::function<bool(SmartPtr<Entity>)> Condition) noexcept;

		// 該当する名前のEntityを殺す。
		void Kill(const String&) noexcept;

		// 管理下にあるEntityの数を返す。
		size_t Amount() const noexcept;

		// Entityを管理下に置く。
		void Register(SmartPtr<Entity>) noexcept, Register(SmartPtr<Entity>, const size_t) noexcept;
	};
}