/**
* Event.hpp - ���[�U�������I�ɃQ�[���𐧌䂷�邽�߂̒�`�Q�B
*/
#pragma once

namespace karapo::event {
	namespace command {
		class Command {
		public:
			virtual ~Command() = 0;

			// �R�}���h�����s����B
			virtual void Execute() = 0;

			// ���s���I�������ǂ����B
			virtual bool Executed() const noexcept = 0;

			// �R�}���h���s�v�ɂȂ������ǂ�����\���B
			// true��Ԃ��ꍇ�A�Ď��s���Ă͂����Ȃ������Ӗ�����B
			virtual bool IsUnnecessary() const noexcept = 0;
		};
	}

	// �C�x���g�����^�C�v
	enum class TriggerType {
		None,					// �������Ȃ�(call�p)
		Load,					// ���[�h����s
		Auto,					// ������s
		Trigger,				// �G��Ă���ԂɎ��s
		Button					// ����L�[
	};

	using CommandPtr = std::unique_ptr<command::Command>;

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
			bool Evalute(const String& Sentence) const noexcept;
		} condition_manager;

		void *cmdparser;

		std::unordered_map<String, Event> events;

		void SetCMDParser(void*);
	public:
		void AliasCommand(const String&, const String&);

		// �C�x���g��ǂݍ��݁A��������B
		void LoadEvent(const String Path) noexcept;
		// ���W����C�x���g�����s����B
		void ExecuteEvent(const WorldVector) noexcept;
		// �C�x���g������C�x���g�����s����B
		void ExecuteEvent(const String&) noexcept;
		//
		void Update() noexcept;

		void SetCondTarget(std::any*);
		bool Evalute(const String& Sentence);
	};
}