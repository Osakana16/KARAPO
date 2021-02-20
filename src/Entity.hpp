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

	// Entity����̉�(�z��)�ŊǗ�����N���X
	class Chunk {
		std::vector<SmartPtr<Entity>> entities;
		std::unordered_map<String, std::weak_ptr<Entity>> refs;
	public:
		Chunk();
		// Entity���X�V����B
		void Update() noexcept;
		// �Y�����閼�O��Entity���擾����B
		SmartPtr<Entity> Get(const String& Name) const noexcept;
		// �Y�����������Entity���擾����B
		SmartPtr<Entity> Get(std::function<bool(SmartPtr<Entity>)> Condition) const noexcept;
		// �Ǘ����̐���Ԃ��B
		size_t Size() const noexcept;
		// Entity���E���B
		void Kill(const String& name) noexcept;
	};

	// Entity����̉�(�z��)�ŊǗ�����N���X(�Œ蒷��)
	template<uint N>
	class FixedChunk {
		SmartPtr<Entity> entities[N]{ nullptr };
		std::unordered_map<String, std::weak_ptr<Entity>> refs;
	public:
		FixedChunk() noexcept {
			refs.clear();
		}

		// Entity���X�V����B
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

		// �Ǘ����̐���Ԃ��B
		size_t Size() const noexcept {
			return refs.size();
		}

		// ���̉򂪖��^������Ԃ��B
		bool IsFull() const noexcept {
			for (auto& ent : entities) {
				if (ent == nullptr)
					return false;
			}
			return true;
		}

		// Entity��o�^����B
		auto Register(SmartPtr<Entity> ent) {
			Register(ent, ent->Name());
		}

		// Entity��o�^����B
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

		// Entity���E���B
		auto Kill(const String& name) noexcept {
			try {
				auto weak = refs.at(name);
				auto shared = weak.lock();
				if (shared) shared->Delete();
				else refs.erase(name);
			} catch (std::out_of_range& e) {
				// ��������Ȃ���Ή������Ȃ��B
			}
		}

		// �Y�����閼�O��Entity���擾����B
		SmartPtr<Entity> Get(const String& Name) const noexcept {
			SmartPtr<Entity> ent = nullptr;
			try {
				std::weak_ptr<Entity> weak = refs.at(Name);
				ent = weak.lock();
			} catch (...) {}
			return ent;
		}

		// �Y�����������Entity���擾����B
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
		void Register(SmartPtr<Entity>) noexcept, Register(SmartPtr<Entity>, const size_t) noexcept;
	};
}