/**
* Entity.hpp - �Q�[�����̃L�����N�^�[��I�u�W�F�N�g��\���ׂ̒�`�Q�B
*/
#pragma once
#include "Chunk.hpp"

namespace karapo::entity {
	// �Q�[���̃L�����N�^�[��I�u�W�F�N�g�̌��ƂȂ�N���X�B
	class Entity {
	public:
		virtual ~Entity() = 0;

		// Entity���s�֐�
		virtual int Main() = 0;
		// ���g�̈ʒu
		virtual WorldVector Origin() const noexcept = 0;
		// ���g�̖��O
		virtual const Char* Name() const noexcept = 0;

		// ���g���폜�ł����Ԃɂ��邩��Ԃ��B
		virtual bool CanDelete() const noexcept = 0;
		// ���g���폜�ł����Ԃɂ���B
		virtual void Delete() = 0;
		// �`�ʂ���B
		virtual void Draw(WorldVector, TargetRender) = 0;
		// �w�肵�����W�փe���|�[�g����B
		virtual void Teleport(WorldVector) = 0;
	};

	class Object : public Entity {
	protected:
		WorldVector origin;
	public:
		WorldVector Origin() const noexcept override;
		void Teleport(WorldVector) override;
	};

	// �摜Entity�N���X
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

	// ��Entity�N���X
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

	// Entity���Ǘ�����N���X�B
	class Manager {
		std::vector<DefaultChunk> entities;
		SmartPtr<Entity> SpawnCreature(const String&);
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