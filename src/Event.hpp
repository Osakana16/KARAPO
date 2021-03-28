/**
* Event.hpp - ���[�U�������I�ɃQ�[���𐧌䂷�邽�߂̒�`�Q�B
*/
#pragma once

namespace karapo::event {
	namespace innertype {
		constexpr const wchar_t *const Number = L"number";	// ���l�^(�����܂��͕��������_��)
		constexpr const wchar_t *const String = L"string";	// ������^(�����܂��͕�����)
		constexpr const wchar_t *const None = L"";			// �^����(�ϐ������̋L��)
		constexpr const wchar_t *const Block = L"scope";	// �X�R�[�v�^({ �܂��� })
	}

	// �C�x���g�����^�C�v
	enum class TriggerType {
		None,					// �������Ȃ�(call�p)
		Load,					// ���[�h����s
		Auto,					// ������s
		Trigger,				// �G��Ă���ԂɎ��s
		Button					// ����L�[
	};

	// �C�x���g
	struct Event {
		using Commands = std::deque<CommandPtr>;
		Commands commands;			// �R�}���h
		TriggerType trigger_type;	// �C�x���g�����^�C�v
		WorldVector origin[2];		// �C�x���g
	};

	// �C�x���g�Ǘ��N���X
	// ���[���h���̃C�x���g���e�Ǘ��A���͉�́A�R�}���h���s�����s���B
	class Manager final : private Singleton {
		class CommandExecuter;
	
		class ConditionManager final {
			std::any target_value;
			bool can_execute = true;
		public:
			void SetTarget(std::any& tv);
			// ��������]������
			void Evalute(const std::wstring& Sentence) noexcept;
			void FreeCase(), FreeOf();
			const bool& Can_Execute = can_execute;
		} condition_manager;

		std::unordered_map<std::wstring, Event> events;

		Manager() = default;
		~Manager() = default;
	public:
		// �C�x���g��ǂݍ��݁A��������B
		void LoadEvent(const std::wstring Path) noexcept;
		// ���W����C�x���g�����s����B
		void ExecuteEvent(const WorldVector) noexcept;
		// �C�x���g������C�x���g�����s����B
		void Call(const std::wstring&) noexcept;
		//
		void Update() noexcept;

		Event* GetEvent(const std::wstring&) noexcept;

		void MakeEmptyEvent(const std::wstring&);

		void SetCondTarget(std::any);
		void Evalute(const std::wstring& Sentence);
		void FreeCase(), FreeOf();

		static Manager& Instance() noexcept {
			static Manager manager;
			return manager;
		}
	};

	// �C�x���g�ҏW�N���X
	class EventEditor final {
		Event* targeting = nullptr;
	public:
		// ���݁A�ҏW���̃C�x���g�������ۂ��B
		bool IsEditing() const noexcept;
		// ���݁A�Y�����閼�O�̃C�x���g��ҏW�����ۂ��B
		bool IsEditing(const std::wstring&) const noexcept;

		// �V������̃C�x���g���쐬�A�ҏW�Ώۂɂ���B
		void MakeNewEvent(const std::wstring&);
		// �w�肵�����O�̃C�x���g��ҏW�ΏۂƂ��Đݒ肷��B
		void SetTarget(const std::wstring&);
		// �C�x���g�����̎�ނ�ݒ肷��B
		void ChangeTriggerType(const std::wstring&);
		// �C�x���g�����͈̔͂�ݒ肷��B
		void ChangeRange(const WorldVector&, const WorldVector&);
		// �R�}���h��ǉ�����B
		void AddCommand(const std::wstring&, const int);
	};
}