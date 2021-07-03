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

	// ��Entity�N���X
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

	// �{�^��
	class Button : public Image {
		std::wstring name{};
		bool collided_enough{};

		void Update();
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

	class StringNameManager final {
		const std::wstring Manager_Name{};
		std::wstring *managing_variable{};
	public:
		StringNameManager(const std::wstring&) noexcept;
		void Add(const std::shared_ptr<Entity>&), Remove(const std::shared_ptr<Entity>&);
		bool IsRegistered(const std::shared_ptr<Entity>&) const noexcept;
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
		void Kill(const std::wstring& Entity_Name) noexcept;
		// �Y�����閼�O��Entity��Chunk���珜�O����B
		void Remove(const std::shared_ptr<Entity>& Target) noexcept;
	};

	// Entity���Ǘ�����N���X�B
	class Manager final : private Singleton {
		std::unordered_set<std::wstring> freezable_entity_kind{},
			killable_entities{};
		std::vector<Chunk> chunks{};	// Entity���X�V����`�����N�B
		Chunk idle_chunk{};				// Entity���X�V���Ȃ��`�����N�B
		Chunk glacial_chunk{};			// Entity���X�V�����A�����Ȃ鑀��̑Ώۂɂ����Ȃ��`�����N�B

		error::UserErrorHandler error_handler{};
		error::ErrorClass *entity_error_class{};
		error::ErrorContent *entity_already_registered_warning{};

		Manager();
		~Manager() = default;
	public:
		StringNameManager name_manager;

		// Entity���X�V����B
		void Update() noexcept;
		// �Y�����閼�O��Entity����肷��B
		std::shared_ptr<Entity> GetEntity(const std::wstring& Name) const noexcept;
		// �֐����̏����ɓ��Ă͂܂�Entity����肷��B
		std::shared_ptr<Entity> GetEntity(std::function<bool(std::shared_ptr<Entity>)> Condition) const noexcept;

		// �Y�����閼�O��Entity���E���B
		void Kill(const std::wstring&) noexcept;

		// �Ǘ����ɂ���Entity�̐���Ԃ��B
		size_t Amount() const noexcept;

		// Entity���Ǘ����ɒu���B
		void Register(std::shared_ptr<Entity>) noexcept, Register(std::shared_ptr<Entity>, const std::wstring&) noexcept;

		// �Y�����閼�O��Entity���X�V�Ώۂ���O���B
		bool Deactivate(std::shared_ptr<Entity>& target) noexcept;
		// �Y������Entity�̎�ނ��X�V�Ώۂ���O���B
		bool Deactivate(const std::wstring& Entity_Name) noexcept;
		// �Y�����閼�O��Entity���X�V�Ώۂɂ���B
		bool Activate(std::shared_ptr<Entity>& target) noexcept;
		// �Y������Entity�̎�ނ��X�V�Ώۂɂ���B
		bool Activate(const std::wstring& Entity_Name) noexcept;

		// �Y�����閼�O��Entity���X�V�Ώۂ���O���B
		bool Freeze(std::shared_ptr<Entity>& target) noexcept;
		// �Y������Entity�̎�ނ��X�V�Ώۂ���O���B
		bool Freeze(const std::wstring& Entity_Name) noexcept;
		// �Y�����閼�O��Entity���X�V�Ώۂɂ���B
		bool Defrost(std::shared_ptr<Entity>& target) noexcept;
		// �Y������Entity�̎�ނ��X�V�Ώۂɂ���B
		bool Defrost(const std::wstring& Entity_Name) noexcept;

		static Manager& Instance() noexcept {
			static Manager manager;
			return manager;
		}
	};
}