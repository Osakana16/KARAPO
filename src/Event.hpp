/**
* Event.hpp - ユーザが逐次的にゲームを制御するための定義群。
*/
#pragma once

namespace karapo::event {
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
			std::any* target_variable;
			std::vector<bool> results;
		public:
			void SetTarget(std::any* tv);
			// 条件式を評価する
			bool Evalute(const String& Sentence) const noexcept;
		} condition_manager;

		void *cmdparser;

		std::unordered_map<String, Event> events;

		void SetCMDParser(void*);
	public:
		void AliasCommand(const String&, const String&);

		// イベントを読み込み、生成する。
		void LoadEvent(const String Path) noexcept;
		// 座標からイベントを実行する。
		void ExecuteEvent(const WorldVector) noexcept;
		// イベント名からイベントを実行する。
		void ExecuteEvent(const String&) noexcept;
		//
		void Update() noexcept;

		void SetCondTarget(std::any*);
		bool Evalute(const String& Sentence);
	};
}