/**
* Entity.hpp - �Q�[�����̃L�����N�^�[��I�u�W�F�N�g��\���ׂ̒�`�Q�B
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

	// �摜Entity�N���X
	class Image : public Object {
		resource::Image image = resource::Image(&Default_ProgramInterface);
		bool can_delete = false;
	protected:
		WorldVector length{};
	public:
		Image(const WorldVector&, const WorldVector&);
		inline int Main() override { return 0; }
		const wchar_t *Name() const noexcept override;
		const wchar_t *KindName() const noexcept override;
		bool CanDelete() const noexcept override;
		void Delete() override;
		void Draw(WorldVector) override;
		void Load(const std::wstring&);
		WorldVector Length() const noexcept;
	};

	// ��Entity�N���X
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
	};

	// �{�^��
	class Button : public Image {
		std::wstring name{};
		bool collided_enough{};

		// �Փˎ��̏���
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

	// Entity����̉�(�z��)�ŊǗ�����N���X
	class Chunk {
		std::unordered_map<std::wstring, std::shared_ptr<Entity>> entities{};
	public:
		// Entity���X�V����B
		void Update() noexcept;
		// Entity��o�^����B
		void Register(std::shared_ptr<Entity>&) noexcept;
		// �Y�����閼�O��Entity���擾����B
		std::shared_ptr<Entity> Get(const std::wstring& Name) const noexcept;
		// �Y�����������Entity���擾����B
		std::shared_ptr<Entity> Get(std::function<bool(std::shared_ptr<Entity>)> Condition) const noexcept;
		// �Ǘ����̐���Ԃ��B
		size_t Size() const noexcept;
		// Entity���E���B
		void Kill(const std::wstring& name) noexcept;
	};

	// Entity���Ǘ�����N���X�B
	class Manager final : private Singleton {
		std::vector<Chunk> chunks;

		Manager() = default;
		~Manager() = default;
	public:
		// Entity���X�V����B
		void Update() noexcept;
		// �Y�����閼�O��Entity����肷��B
		std::shared_ptr<Entity> GetEntity(const std::wstring& Name) noexcept;
		// �֐����̏����ɓ��Ă͂܂�Entity����肷��B
		std::shared_ptr<Entity> GetEntity(std::function<bool(std::shared_ptr<Entity>)> Condition) noexcept;

		// �Y�����閼�O��Entity���E���B
		void Kill(const std::wstring&) noexcept;

		// �Ǘ����ɂ���Entity�̐���Ԃ��B
		size_t Amount() const noexcept;

		// Entity���Ǘ����ɒu���B
		void Register(std::shared_ptr<Entity>) noexcept, Register(std::shared_ptr<Entity>, const std::wstring&) noexcept;

		static Manager& Instance() noexcept {
			static Manager manager;
			return manager;
		}
	};
}