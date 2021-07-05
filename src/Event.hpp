/**
* Event.hpp - ユーザが逐次的にゲームを制御するための定義群。
*/
#pragma once

namespace karapo::event {
	namespace innertype {
		constexpr const wchar_t *const Number = L"number";			// 数値型(整数または浮動小数点数)
		constexpr const wchar_t *const String = L"string";			// 文字列型(文字または文字列)
		constexpr const wchar_t *const Undecided = L"";				// 未決定型
		constexpr const wchar_t *const None = L"";					// 型無し(変数や特定の記号)
	}

	// イベント発生タイプ
	enum class TriggerType {
		Invalid,					// 不正
		None,					// 何もしない(call用)
		Load,					// ロード後実行
		Auto,					// 自動実行
		Trigger				// 触れている間に実行
	};

	struct CommandGraph final {
		std::unique_ptr<Command> command{};	// コマンド本体。
		std::wstring word{};											// コマンド名。
		CommandGraph* parent{};							// 次に実行するコマンド。
		CommandGraph* neighbor{};						// 条件分岐でfalseになった場合に実行するコマンド。
	};

	// イベント
	struct Event {
		using Commands = std::list<CommandGraph>;
		
		Commands commands;						// コマンド
		TriggerType trigger_type;				// イベント発生タイプ
		WorldVector origin[2];					// イベント
		std::vector<std::wstring> param_names{};	// 引数名
	};

	// イベント管理クラス
	// ワールド毎のイベント内容管理、文章解析、コマンド実行等を行う。
	class Manager final : private Singleton {
		// イベントを読み込む必要があるか否か。
		std::wstring requesting_path{};
	
		class ConditionManager final {
			std::any target_value;
			bool can_execute = true;
		public:
			ConditionManager() = default;
			ConditionManager(std::any& tv) { SetTarget(tv); }
			void SetTarget(std::any& tv);
			// 条件式を評価する
			bool Evalute(const std::wstring&, const std::any&) noexcept;
			void FreeCase();
			bool CanExecute() const noexcept { return can_execute; }
		};
		
		std::deque<ConditionManager> condition_manager;
		decltype(condition_manager)::iterator condition_current;

		std::unordered_map<std::wstring, Event> events;
		// イベントを生成する。
		std::unordered_map<std::wstring, Event> GenerateEvent(const std::wstring&) noexcept;
		void OnLoad() noexcept;

		error::ErrorContent *call_error{};

		Manager();
		~Manager() = default;
	public:
		class CommandExecuter;
		// イベントを読み込み、新しく設定し直す。
		void LoadEvent(const std::wstring Path) noexcept;
		// イベントの遅延読み込み。
		void RequestEvent(const std::wstring&) noexcept;
		// イベントを読み込み、追加で設定する。
		void ImportEvent(const std::wstring&) noexcept;
		// 座標からイベントを実行する。
		void ExecuteEvent(const WorldVector) noexcept;
		//
		bool Push(const std::wstring&) noexcept;
		// イベント名からイベントを実行する。
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

	// イベント編集クラス
	class EventEditor final {
		Event* targeting = nullptr;
	public:
		// 現在、編集中のイベントを持つか否か。
		bool IsEditing() const noexcept;
		// 現在、該当する名前のイベントを編集中か否か。
		bool IsEditing(const std::wstring&) const noexcept;

		// 新しい空のイベントを作成、編集対象にする。
		void MakeNewEvent(const std::wstring&);
		// 指定した名前のイベントを編集対象として設定する。
		void SetTarget(const std::wstring&);
		// イベント発生の種類を設定する。
		void ChangeTriggerType(const std::wstring&);
		// イベント発生の範囲を設定する。
		void ChangeRange(const WorldVector&, const WorldVector&);
		// コマンドを追加する。
		void AddCommand(const std::wstring&, const int);
	};
}