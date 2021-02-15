/**
* Entity.hpp - �Q�[�����̃L�����N�^�[��I�u�W�F�N�g��\���ׂ̒�`�Q�B
*/
#pragma once
#include "Chunk.hpp"

namespace karapo::entity {
	class Object : public Entity {
	protected:
		WorldVector origin;
	public:
		WorldVector Origin() const noexcept override;
		void Teleport(WorldVector) override;
	};

	// �摜Entity�N���X
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

	// ��Entity�N���X
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

	using DefaultChunk = memory::FixedChunk<Entity, 1000>;

	// Entity���Ǘ�����N���X�B
	class Manager {
		std::vector<DefaultChunk> entities;
	public:
		// Entity���X�V����B
		void Update() noexcept;
		// �Y�����閼�O��Entity����肷��B
		SmartPtr<Entity> GetEntity(const String& Name) noexcept;
		// �֐����̏����ɓ��Ă͂܂�Entity����肷��B
		SmartPtr<Entity> GetEntity(std::function<bool(SmartPtr<Entity>)> Condition) noexcept;

		// �Y�����閼�O��Entity���E���B
		void Kill(const String&) noexcept;

		// �Ǘ����ɂ���Entity�̐���Ԃ��B
		size_t Amount() const noexcept;

		// Entity���Ǘ����ɒu���B
		void Register(SmartPtr<Entity>) noexcept;
	};
}