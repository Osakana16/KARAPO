/**
* Event.hpp - ���[�U�������I�ɃQ�[���𐧌䂷�邽�߂̒�`�Q�B
*/
#pragma once

namespace karapo::event {
	namespace innertype {
		constexpr const wchar_t *const Number = L"number";			// ���l�^(�����܂��͕��������_��)
		constexpr const wchar_t *const String = L"string";			// ������^(�����܂��͕�����)
		constexpr const wchar_t *const Undecided = L"";				// ������^
		constexpr const wchar_t *const None = L"";					// �^����(�ϐ������̋L��)
	}

	// �C�x���g�����^�C�v
	enum class TriggerType {
		Invalid,					// �s��
		None,					// �������Ȃ�(call�p)
		Load,					// ���[�h����s
		Auto,					// �������s
		Trigger				// �G��Ă���ԂɎ��s
	};

	struct CommandGraph final {
		std::unique_ptr<Command> command{};	// �R�}���h�{�́B
		std::wstring word{};											// �R�}���h���B
		CommandGraph* parent{};							// ���Ɏ��s����R�}���h�B
		CommandGraph* neighbor{};						// ���������false�ɂȂ����ꍇ�Ɏ��s����R�}���h�B
	};

	// �C�x���g
	struct Event {
		using Commands = std::list<CommandGraph>;
		
		Commands commands;						// �R�}���h
		TriggerType trigger_type;				// �C�x���g�����^�C�v
		WorldVector origin[2];					// �C�x���g
		std::vector<std::wstring> param_names{};	// ������
	};

	// �C�x���g�Ǘ��N���X
	// ���[���h���̃C�x���g���e�Ǘ��A���͉�́A�R�}���h���s�����s���B
	class Manager final : private Singleton {
		// �C�x���g��ǂݍ��ޕK�v�����邩�ۂ��B
		std::wstring requesting_path{};
	
		class ConditionManager final {
			std::any target_value;
			bool can_execute = true;
		public:
			ConditionManager() = default;
			ConditionManager(std::any& tv) { SetTarget(tv); }
			void SetTarget(std::any& tv);
			// ��������]������
			bool Evalute(const std::wstring&, const std::any&) noexcept;
			void FreeCase();
			bool CanExecute() const noexcept { return can_execute; }
		};
		
		std::deque<ConditionManager> condition_manager;
		decltype(condition_manager)::iterator condition_current;

		std::unordered_map<std::wstring, Event> events;
		// �C�x���g�𐶐�����B
		std::unordered_map<std::wstring, Event> GenerateEvent(const std::wstring&) noexcept;
		void OnLoad() noexcept;

		error::ErrorContent *call_error{};

		Manager();
		~Manager() = default;
	public:
		class CommandExecuter;
		// �C�x���g��ǂݍ��݁A�V�����ݒ肵�����B
		void LoadEvent(const std::wstring Path) noexcept;
		// �C�x���g�̒x���ǂݍ��݁B
		void RequestEvent(const std::wstring&) noexcept;
		// �C�x���g��ǂݍ��݁A�ǉ��Őݒ肷��B
		void ImportEvent(const std::wstring&) noexcept;
		// ���W����C�x���g�����s����B
		void ExecuteEvent(const WorldVector) noexcept;
		//
		bool Push(const std::wstring&) noexcept;
		// �C�x���g������C�x���g�����s����B
		bool Call(const std::wstring&) noexcept;
		//
		void Update() noexcept;

		Event* GetEvent(const std::wstring&) noexcept;

		void MakeEmptyEvent(const std::wstring&);

		void NewCaseTarget(std::any);
		bool Evalute(const std::wstring&, const std::any&);
		void FreeCase();
		bool CanOfExecute() const noexcept;

		static Manager& Instance() noexcept {
			static Manager manager;
			return manager;
		}

		error::UserErrorHandler error_handler{};
		error::ErrorClass *error_class{};
	};

	class EventExecuter final {
		std::list<std::wstring> event_queue{};
	public:
		void PushEvent(const std::wstring&) noexcept;
		void Update();
		void Execute(const std::list<CommandGraph>&);
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