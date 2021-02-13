/**
* Entity.hpp - ゲーム内のキャラクターやオブジェクトを表す為の定義群。
*/
#pragma once
#include "Chunk.hpp"

namespace karapo::entity {
	// ゲームのキャラクターやオブジェクトの元となるクラス。
	class Entity {
	public:
		virtual ~Entity() = 0;

		// Entity実行関数
		virtual int Main() = 0;
		// 自身の位置
		virtual WorldVector Origin() const noexcept = 0;
		// 自身の名前
		virtual const Char* Name() const noexcept = 0;

		// 自身が削除できる状態にあるかを返す。
		virtual bool CanDelete() const noexcept = 0;
		// 自身を削除できる状態にする。
		virtual void Delete() = 0;
		// 描写する。
		virtual void Draw(WorldVector, TargetRender) = 0;
		// 指定した座標へテレポートする。
		virtual void Teleport(WorldVector) = 0;
	};

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
		resource::Sound sound;
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

	using DefaultChunk = memory::FixedChunk<Entity, 1000>;

	// Entityを管理するクラス。
	class Manager {
		std::vector<DefaultChunk> entities;
		SmartPtr<Entity> SpawnCreature(const String&);
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
		void Register(SmartPtr<Entity>) noexcept;
	};
}