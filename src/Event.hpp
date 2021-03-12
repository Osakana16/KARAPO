/**
* Event.hpp - ユーザが逐次的にゲームを制御するための定義群。
*/
#pragma once

namespace karapo::event {
	namespace innertype {
		constexpr const wchar_t *const Number = L"number";	// 数値型(整数または浮動小数点数)
		constexpr const wchar_t *const String = L"string";	// 文字列型(文字または文字列)
		constexpr const wchar_t *const None = L"";			// 型無し(変数や特定の記号)
		constexpr const wchar_t *const Block = L"scope";	// スコープ型({ または })
	}

	// イベント発生タイプ
	enum class TriggerType {
		None,					// 何もしない(call用)
		Load,					// ロード後実行
		Auto,					// 並列実行
		Trigger,				// 触れている間に実行
		Button					// 決定キー
	};

	// イベント
	struct Event {
		using Commands = std::deque<CommandPtr>;
		Commands commands;			// コマンド
		TriggerType trigger_type;	// イベント発生タイプ
		WorldVector origin[2];		// イベント
	};

	// イベント管理クラス
	// ワールド毎のイベント内容管理、文章解析、コマンド実行等を行う。
	class Manager {
		class EventGenerator;
		class CommandExecuter;
		class VariableManager;

		class ConditionManager final {
			std::any target_value;
			bool can_execute = true;
		public:
			void SetTarget(std::any& tv);
			// 条件式を評価する
			void Evalute(const std::wstring& Sentence) noexcept;
			void Free();
			const bool& Can_Execute = can_execute;
		} condition_manager;

		void *cmdparser;

		std::unordered_map<std::wstring, Event> events;

		void SetCMDParser(void*);
	public:
		void AliasCommand(const std::wstring&, const std::wstring&);

		// イベントを読み込み、生成する。
		void LoadEvent(const std::wstring Path) noexcept;
		// 座標からイベントを実行する。
		void ExecuteEvent(const WorldVector) noexcept;
		// イベント名からイベントを実行する。
		void ExecuteEvent(const std::wstring&) noexcept;
		//
		void Update() noexcept;

		void SetCondTarget(std::any);
		void Evalute(const std::wstring& Sentence);
		void FreeCase();
	};
}