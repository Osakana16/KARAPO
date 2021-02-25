/**
* Event.hpp - ���[�U�������I�ɃQ�[���𐧌䂷�邽�߂̒�`�Q�B
*/
#pragma once

namespace karapo::event {
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
	class Manager {
		class EventGenerator;
		class CommandExecuter;
		class VariableManager;

		class ConditionManager final {
			std::any* target_variable;
			std::vector<bool> results;
		public:
			void SetTarget(std::any* tv);
			// ��������]������
			bool Evalute(const std::wstring& Sentence) const noexcept;
		} condition_manager;

		void *cmdparser;

		std::unordered_map<std::wstring, Event> events;

		void SetCMDParser(void*);
	public:
		void AliasCommand(const std::wstring&, const std::wstring&);

		// �C�x���g��ǂݍ��݁A��������B
		void LoadEvent(const std::wstring Path) noexcept;
		// ���W����C�x���g�����s����B
		void ExecuteEvent(const WorldVector) noexcept;
		// �C�x���g������C�x���g�����s����B
		void ExecuteEvent(const std::wstring&) noexcept;
		//
		void Update() noexcept;

		void SetCondTarget(std::any*);
		bool Evalute(const std::wstring& Sentence);
	};
}