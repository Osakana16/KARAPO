#include "Event.hpp"

#include "Canvas.hpp"

#include "Engine.hpp"

#include <chrono>
#include <forward_list>

namespace karapo::event {
	namespace command {
		Command::~Command() {}

		// Entity操作系コマンド
		class EntityCommand : public Command {
			String entity_name;
		protected:
			SmartPtr<karapo::entity::Entity> GetEntity() const noexcept {
				return GetProgram()->entity_manager.GetEntity(entity_name.c_str());
			}
		};

		// 変数
		class Variable : public Command {
			String varname;
			std::any value;
			bool executed = false;
		public:
			Variable(const String& VName, const String& Any_Value) noexcept {
				varname = VName;
				auto [iv, ir] = ToInt<int>(VName.c_str());
				auto [fv, fr] = ToDec<Dec>(VName.c_str());
				if (ir == nullptr) {
					value = iv;
				} else if (fr == nullptr) {
					value = fv;
				} else {
					try {
						value = GetProgram()->var_manager[VName];
					} catch (...) {
						value = Any_Value;
					}
				}
			}

			void Execute() override {
				GetProgram()->var_manager.MakeNew(varname) = value;
			}

			bool Executed() const noexcept override {
				return executed;
			}

			bool IsUnnecessary() const noexcept override {
				return Executed();
			}
		};

		class Case : public Command {
			String varname;
			bool executed = false;
		public:
			Case(const String& Name) noexcept {
				varname = Name;
			}

			void Execute() override {
				GetProgram()->event_manager.SetCondTarget(&GetProgram()->var_manager[varname]);
			}

			bool Executed() const noexcept override {
				return executed;
			}

			bool IsUnnecessary() const noexcept override {
				return Executed();
			}
		};

		class Of : public Command {
			String condition_sentence;
			bool executed = false;
		public:
			Of(const String& Condition_Sentence) noexcept {
				condition_sentence = Condition_Sentence;
			}

			void Execute() override {
				GetProgram()->event_manager.Evalute(condition_sentence);
			}

			bool Executed() const noexcept override {
				return executed;
			}

			bool IsUnnecessary() const noexcept override {
				return Executed();
			}
		};

		// 画像を読み込み、表示させる。
		class Image : public Command {
			SmartPtr<karapo::entity::Image> image;
			const String Path;
			bool executed = false;
		public:
			Image(const String& P, const WorldVector WV) : Path(P) {
				image = std::make_shared<karapo::entity::Image>(WV);
			}

			~Image() override {}

			void Execute() override {
				image->Load(Path.c_str());
				GetProgram()->entity_manager.Register(image);
				executed = true;
			}

			bool Executed() const noexcept override {
				return executed;
			}

			bool IsUnnecessary() const noexcept override {
				return Executed();
			}
		};

		// BGM
		class Music final : public Command {
			SmartPtr<karapo::entity::Sound> music;
			const String Path;
			bool executed = false;
		public:
			Music(const String& P) : Path(P) {
				music = std::make_shared<karapo::entity::Sound>(WorldVector{ 0, 0 });
			}

			~Music() override {}

			void Execute() override {
				executed = true;
				music->Load(Path);
				GetProgram()->entity_manager.Register(music);
			}

			bool Executed() const noexcept override {
				return executed;
			}

			bool IsUnnecessary() const noexcept override {
				return Executed();
			}
		};

		// 効果音
		class Sound final : public Command {
			SmartPtr<karapo::entity::Sound> sound;
			const String Path;
			bool executed = false;
		public:
			Sound(const String& P, const WorldVector& WV) : Path(P) {
				sound = std::make_shared<karapo::entity::Sound>(WV);
			}

			~Sound() noexcept override {}

			void Execute() override {
				sound->Load(Path);
				GetProgram()->entity_manager.Register(sound);
				executed = true;
			}

			bool Executed() const noexcept override {
				return executed;
			}

			bool IsUnnecessary() const noexcept override {
				return Executed();
			}
		};

		namespace entity {
			class Spawn final : public Command {
				bool executed = false;
				String name;
				WorldVector origin;
			public:
				Spawn(const String& Name, const WorldVector O) {
					name = Name;
					origin = O;
				}

				~Spawn() noexcept override {}

				void Execute() override {
					executed = true;
				}

				bool Executed() const noexcept override {
					return executed;
				}

				bool IsUnnecessary() const noexcept override {
					return Executed();
				}
			};

			// Entityの移動
			class Teleport final : public Command {
				String entity_name;
				WorldVector move;
				bool executed;
			public:
				Teleport(const String& ename, const WorldVector& MV) noexcept {
					entity_name = ename;
					move = MV;
				}

				~Teleport() override {}

				void Execute() override {
					auto ent = GetProgram()->entity_manager.GetEntity(entity_name);
					ent->Teleport(move);
					executed = true;
				}

				bool Executed() const noexcept override {
					return executed;
				}

				bool IsUnnecessary() const noexcept override {
					return Executed();
				}
			};

			// Entityの削除。
			class Kill final : public Command {
				String entity_name;
				bool executed = false;
			public:
				Kill(const String& ename) noexcept {
					entity_name = ename;
				}

				~Kill() override {}

				void Execute() override {
					GetProgram()->entity_manager.Kill(entity_name);
					executed = true;
				}

				bool Executed() const noexcept override {
					return executed;
				}

				bool IsUnnecessary() const noexcept override {
					return Executed();
				}
			};
		}		

		// 別ワールドへの移動
		class Transfer : public Command {
			String world_name;
			bool executed = false;
		public:
			Transfer(String& wname) noexcept {
				world_name = wname;
			}

			void Execute() override {
				executed = true;
			}

			bool Executed() const noexcept override {
				return executed;
			}

			bool IsUnnecessary() const noexcept override {
				return Executed();
			}
		};

		// コマンドの別名
		class Alias : public Command {
			String newone, original;
			bool executed = false;
		public:
			Alias(String s1, String s2) noexcept {
				original = s1;
				newone = s2;
			}

			void Execute() override;

			bool Executed() const noexcept override {
				return executed;
			}

			bool IsUnnecessary() const noexcept override {
				return Executed();
			}
		};

		class DLL : public Command {
		protected:
			String dll_name;
			bool executed;
		public:
			DLL(const String& dname) noexcept {
				dll_name = dname;
			}

			~DLL() override {}

			bool Executed() const noexcept final {
				return executed;
			}

			bool IsUnnecessary() const noexcept final {
				return Executed();
			}
		};

		// DLLアタッチ
		class Attach final : public DLL {
		public:
			inline Attach(const String& dname) : DLL(dname) {}
			
			void Execute() final {
				GetProgram()->dll_manager.Attach(dll_name);
			}
		};

		class Detach final : public DLL {
		public:
			inline Detach(const String& dname) : DLL(dname) {}

			void Execute() final {
				GetProgram()->dll_manager.Detach(dll_name);
			}
		};

		// イベント呼出
		class Call : public Command {
			String event_name;
			bool executed = false;
		public:
			Call(String& ename) noexcept {
				event_name = ename;
			}

			void Execute() override {
				GetProgram()->event_manager.ExecuteEvent(event_name);
				executed = true;

			}

			bool Executed() const noexcept override {
				return executed;
			}

			bool IsUnnecessary() const noexcept override {
				return Executed();
			}
		};
	}

	// イベントのコマンド実行クラス
	class Manager::CommandExecuter final {
		bool called_result = false;

		Event::Commands commands;	// 実行中のコマンド
		Event::Commands ended;		// 実行を終えたコマンド

		// 一つのコマンドを実行する。
		CommandPtr Execute(CommandPtr cmd) noexcept {
			if (cmd == nullptr)
				return nullptr;

			if (!cmd->IsUnnecessary()) {
				cmd->Execute();
				return std::move(cmd);
			} else {
				ended.push_back(std::move(cmd));
				return nullptr;
			}
		}
	public:
		CommandExecuter(Event::Commands&& wantto_exec) noexcept {
			commands = std::move(wantto_exec);
		}

		// コマンド全体を実行する。
		void Execute() noexcept {
			while (!commands.empty()) {
				// 再実行コマンド
				CommandPtr recycled = Execute(std::move(commands.front()));
				if (recycled == nullptr)
					commands.pop_front();
				else // 再格納する。
					commands.push_front(std::move(recycled));
			}
		}

		[[nodiscard]] auto&& Result() noexcept {
			called_result = true;
			return std::move(ended);
		}

		~CommandExecuter() noexcept {}
	};

	// イベント生成クラス
	// イベントファイルの解析、コマンドの生成、イベントの設定・生成を行う。
	class Manager::EventGenerator final {
		std::unordered_map<String, Event> events;
	public:
		// 解析クラス
		class Parser final {
			using Context = std::queue<String, std::list<String>>;

			// 一文字ずつの解析器。
			// 空白を基準とする単語単位の解析結果をcontextとして排出。
			class LexicalParser final {
				Context context;
			public:
				LexicalParser(const String Sentence) noexcept {
					auto PushWord = [](Context  *const context, karapo::String *const text) noexcept {
						wprintf_s(L"Pushed:%s\n", text->c_str());
						context->push(*text);
						text->clear();
					};

					// 読み込もうとした単語が、文字列として書かれているかを判定する為の変数
					// 値がtrueの間、解析器はスペースを区切り文字として認識しなくなる。
					bool is_string = false;
					auto text = String(L"");

					for (auto c : Sentence) {
						if (!is_string) {
							// スペース判定
							if (IsSpace(c) || c == L'\0' || c == L'\n') {
								// 貯めこんだ文字を単語として格納
								if (!text.empty())
									PushWord(&context, &text);

								continue;
							}

							// 演算子判定
							switch (c) {
								case L'\'':
									is_string = true;
									[[fallthrough]];
								case L'~':
								case L']':
								case L'[':
								case L'>':
								case L'<':
								case L'(':
								case L')':
								case L',':
								case L'{':
								case L'}':
								{
									// 貯めこんだ文字を単語として格納
									if (!text.empty()) {
										PushWord(&context, &text);
									}
									// 記号を代入
									text = c;
									PushWord(&context, &text);
									continue;
								}
								break;
							}
						} else {
							// 演算子判定
							switch (c) {
								case L'\'':
									is_string = false;
									// 貯めこんだ文字を単語として格納
									if (!text.empty()) {
										PushWord(&context, &text);
									}
									// 記号を代入
									text = c;
									PushWord(&context, &text);
									continue;
							}
						}
						// 読み込んだ文字を付け加える
						text += c;
					}
				}
				auto Result() const noexcept {
					return context;
				}
			};

			// SCからECまでの範囲の文章を解析する解析器。
			// それ以外には、解析中かどうかを表現するための関数群を持つ。
			template<Char SC, Char EC>
			struct SubParser {
				static constexpr bool IsValidToStart(const Char Ch) noexcept { return SC == Ch; }
				static constexpr bool IsValidToEnd(const Char Ch) noexcept { return EC == Ch; }

				~SubParser() noexcept {
					MYGAME_ASSERT(!parsing);
				}
			protected:
				// 解析中かどうか。
				bool IsParsing() const noexcept { return parsing; }

				// 解析開始を宣言する。
				void StartParsing() noexcept {
					MYGAME_ASSERT(!parsing);
					parsing = true;
				}

				// 解析停止を宣言する。
				void EndParsing() noexcept { 
					MYGAME_ASSERT(parsing);
					parsing = false; 
				}
			private:
				bool parsing = false;
			};

			// イベント情報を解析器。
			// 特にイベント名、発生タイプ、イベント位置を解析し、
			// 結果を適切な型に変換して排出する。
			class InformationParser final {
				// イベント名の解析と変換を行うクラス 
				class NameParser final : public SubParser<L'[', L']'> {
					String name;
				public:
					[[nodiscard]] auto Interpret(const String& Sentence) noexcept {
						if (Sentence.size() == 1) {
							if (IsValidToStart(Sentence[0])) {
								StartParsing();
								return false;
							} else if (IsValidToEnd(Sentence[0])) {
								EndParsing();
								return true;
							}
						}
						if (IsParsing())
							name = Sentence;
						return false;
					}

					auto Result() const noexcept {
						return name;
					}
				} nparser;

				// 発生タイプの解析と変換を行うクラス
				class TriggerParser final : public SubParser<L'(', L')'> {
					TriggerType tt;
					std::unordered_map<String, TriggerType> trigger_map;
				public:
					TriggerParser() {
						trigger_map[L"t"] = TriggerType::Trigger;
						trigger_map[L"b"] = TriggerType::Button;
						trigger_map[L"l"] = TriggerType::Load;
						trigger_map[L"n"] = TriggerType::None;
					}

					[[nodiscard]] auto Interpret(const String& Sentence) noexcept {
						if (Sentence.size() == 1) {
							if (IsValidToStart(Sentence[0])) {
								StartParsing();
								return false;
							} else if (IsValidToEnd(Sentence[0])) {
								EndParsing();
								return true;
							}
						}

						if (IsParsing()) {
							tt = trigger_map.at(Sentence);
						}
						return false;
					}

					auto Result() const noexcept {
						return tt;
					}
				} tparser;

				// イベント位置の解析と変換を行うクラス
				class ConditionParser final : public SubParser<L'<', L'>'> {
					static constexpr auto DecMin = std::numeric_limits<Dec>::min();

					Dec minx, maxx;		// minxの次にmaxxが来る前提なので、宣言位置を変更しない事。
					Dec miny, maxy;		// minyの次にmaxyが来る前提なので、宣言位置を変更しない事。
					Dec *current = &minx;	// currentはminxから始まる。
				public:
					[[nodiscard]] auto Interpret(const String& Sentence) noexcept {
						if (Sentence.size() == 1) {
							if (IsValidToStart(Sentence[0])) {
								StartParsing();
								return false;
							} else if (IsValidToEnd(Sentence[0])) {
								EndParsing();
								return true;
							}
						}

						if (IsParsing()) {
							if (Sentence == L"~") {
								if (current + 1 <= &maxy)
									current++;
							} else if (Sentence == L",") {
								current = &miny;
							}
							*current = ToDec<Dec>(Sentence.c_str(), nullptr);
						}
						return false;
					}

					auto Result() const noexcept {
						return std::pair<WorldVector, WorldVector>{ WorldVector{ minx, miny }, WorldVector{ maxx, maxy }};
					}
				} pparser;

				String event_name;
				TriggerType trigger;
				WorldVector min_origin, max_origin;
				std::valarray<bool> enough_result{ false, false, false };

				auto Interpret(Context *context) noexcept {
					if (std::count(std::begin(enough_result), std::end(enough_result), true) == enough_result.size())
						return;

					// ここからcontextの解析が開始される。
					// 基本的に、全ての解析器が一つのcontextを解析する。
					// 

					auto& Sentence = context->front();
					std::valarray<bool> result{ false, false, false };	// 各内容の解析結果

					if (!enough_result[0] && nparser.Interpret(Sentence)) {
						event_name = nparser.Result();
						result[0] = true;
					}

					if (!enough_result[1] && tparser.Interpret(Sentence)) {
						trigger = tparser.Result();
						result[1] = true;
					}

					if (!enough_result[2] && pparser.Interpret(Sentence)) {
						auto [min, max] = pparser.Result();
						if (min[0] > max[0])
							std::swap(min[0], max[0]);
						if (min[1] > max[1])
							std::swap(min[1], max[1]);

						min_origin = min;
						max_origin = max;
						result[2] = true;
					}

					if (std::count(std::begin(result), std::end(result), true) > 1) {
						// 一行の解析で2つ以上の結果が得られた場合は不正とする。
						MYGAME_ASSERT(0);
					}

					enough_result |= result;

					context->pop();
					Interpret(context);
				}
			public:
				InformationParser(Context *context) noexcept {
					Interpret(context);
				}

				std::tuple<String, TriggerType, WorldVector, WorldVector> Result()const noexcept {
					return { event_name, trigger, min_origin, max_origin };
				}
			};

			std::unordered_map<String, Event> events;
			String ename;
		public:
			// コマンド解析クラス
			// 単語に基づくコマンドの生成と、それの結果を排出する。
			class CommandParser final : public SubParser<L'{', L'}'> {
				// コマンドの情報
				struct KeywordInfo final {
					// 生成したコマンドを返す。
					std::function<CommandPtr()> Result = []() -> CommandPtr { MYGAME_ASSERT(0); return nullptr; };

					// 引数の数が十分であるか否かを返す。
					std::function<bool()> isEnough = []() -> bool { MYGAME_ASSERT(0); return false; };

					// コマンドが解析中に実行されるか否かどうか。
					bool is_static = false;

					// コマンドがイベント実行中に実行されるか否かどうか。
					bool is_dynamic = false;
				};
				
				using FUNC = std::function<KeywordInfo(const Array<String>&)>;
				
				// 予約語
				std::unordered_map<String, FUNC> words;

				Array<String> parameters;	// 引数
				Event::Commands commands;	// 
				FUNC liketoRun;				// 生成関数を実行するための関数ポインタ。
				bool is_string = false;
				bool is_newvar = false;

				void Interpret(Context *context) noexcept {
				restart:
					auto text = context->front();

					if (IsParsing()) {
						if (liketoRun == nullptr) {
							if (text != L"\'") {
								liketoRun = words.at(text);
								is_newvar = (text == L"var" || text == L"変数");
							} else
								is_string = false;
						} else {
							auto&& f = liketoRun(parameters);
							if (f.isEnough()) {
								MYGAME_ASSERT(f.is_dynamic ^ f.is_static);

								if (f.is_dynamic) {
									commands.push_back(std::move(f.Result()));
								}

								// コマンドを即実行
								if (f.is_static) {
									f.Result()->Execute();
								}
								liketoRun = nullptr;
								is_newvar = false;
								parameters.clear();
								if (!IsValidToEnd(text[0]))
									goto restart;
							} else {
								if (text == L"\'") {
									is_string = !is_string;
								} else {
									if (is_string || isdigit(text[0]) || is_newvar) {
										parameters.push_back(text);
									} else {
										auto& value = GetProgram()->var_manager[text];
										if (value.type() == typeid(int)) {
											parameters.push_back(std::to_wstring(std::any_cast<int>(value)));
										} else if (value.type() == typeid(Dec)) {
											parameters.push_back(std::to_wstring(std::any_cast<Dec>(value)));
										} else {
											parameters.push_back(std::any_cast<String>(value));
										}
									}
								}
							}
						}
					}

					if (text.size() == 1) {
						if (IsValidToStart(text[0])) {
							assert(!IsParsing());
							StartParsing();
						} else if (IsValidToEnd(text[0])) {
							assert(IsParsing());
							EndParsing();
							// '}' を削除。
							context->pop();
							return;
						}
					}
					context->pop();
					Interpret(context);
				}
			public:
				// コマンドの別名を作成する。
				void MakeAlias(const String& S1, const String& S2) noexcept {
					words[S1] = words.at(S2);
				}

				CommandParser(Context *context) noexcept {
					GetProgram()->event_manager.SetCMDParser(this);

					// ヒットさせる単語を登録する。
	
					words[L"music"] =
						words[L"音楽"] =
						words[L"BGM"] = [](const Array<String>& params) -> KeywordInfo
					{
						return {
							.Result = [&]() noexcept -> CommandPtr { return std::make_unique<command::Music>(params[0]); },
							.isEnough = [params]() -> bool { return params.size() == 1; },
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"sound"] =
						words[L"効果音"] =
						words[L"音"] = [](const Array<String>& params) -> KeywordInfo
					{
						printf("There is a sound!\n");
						return {
							.Result = [&]() noexcept -> CommandPtr { return std::make_unique<command::Sound>(params[0], WorldVector{ ToDec<Dec>(params[1].c_str(), nullptr), ToDec<Dec>(params[2].c_str(), nullptr) }); },
							.isEnough = [params]() -> bool { return params.size() == 1; },
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"image"] =
						words[L"画像"] = 
						words[L"絵"] = [](const Array<String>& params) -> KeywordInfo
					{
						return {
							.Result = [&]() noexcept -> CommandPtr { return std::make_unique<command::Image>(params[0], WorldVector{ ToDec<Dec>(params[1].c_str(), nullptr), ToDec<Dec>(params[2].c_str(), nullptr) }); },
							.isEnough = [params]() -> bool { return params.size() == 3; },
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"teleport"] = 
						words[L"瞬間移動"] = [](const Array<String>& params) -> KeywordInfo {
						return {
							.Result = [&]() noexcept -> CommandPtr {
								return std::make_unique<command::entity::Teleport>(
									params[0],
									WorldVector{ ToDec<Dec>(params[1].c_str(), nullptr), ToDec<Dec>(params[2].c_str(),
									nullptr)
									});
							},
							.isEnough = [params]() -> bool { return params.size() == 3; },
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"kill"] = 
						words[L"殺害"] = [](const Array<String>& params) -> KeywordInfo {
						return {
							.Result = [&]() noexcept -> CommandPtr { return std::make_unique<command::entity::Kill>(params[0]); },
							.isEnough = [params]() -> bool { return params.size() == 1; },
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"alias"] =
						words[L"別名"] = [](const Array<String>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr { return std::make_unique<command::Alias>(params[0], params[1]); },
							.isEnough = [params]() -> bool { return params.size() == 2; },
							.is_static = true,
							.is_dynamic = false
						};
					};

					// DLLアタッチ
					words[L"attach"] = 
						words[L"接続"] = [](const Array<String>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								return std::make_unique<command::Attach>(params[0]);
							},
							.isEnough = [params]() -> bool { return params.size() == 1; },
							.is_static = true,
							.is_dynamic = false
						};
					};

					// DLLデタッチ
					words[L"detach"] =
						words[L"切断"] = [](const Array<String>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								return std::make_unique<command::Detach>(params[0]);
							},
							.isEnough = [params]() -> bool { return params.size() == 1; },
							.is_static = true,
							.is_dynamic = false
						};
					};

					words[L"call"] = 
						words[L"呼出"] = [](const Array<String>& params) -> KeywordInfo {
						return {
							.isEnough = [params]() -> bool { return params.size() == 1; },
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"var"] = 
						words[L"変数"] = [](const Array<String>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								return std::make_unique<command::Variable>(params[0], params[1]); 
							},
							.isEnough = [params]() -> bool { return params.size() == 2; },
							.is_static = true,
							.is_dynamic = false
						};
					};

					words[L"case"] =
						words[L"条件"] = [](const Array<String>& params) -> KeywordInfo {
						return {
							.is_static = true,
							.is_dynamic = false
						};
					};

					words[L"of"] =
						words[L"分岐"] = [](const Array<String>& params) -> KeywordInfo {
						return {
							.is_static = true,
							.is_dynamic = false
						};
					};
					Interpret(context);
				}

				[[nodiscard]] auto Result() noexcept {
					return std::move(commands);
				}
			};


			Parser(const String& Sentence) noexcept {
				LexicalParser lexparser(Sentence);
				auto context = lexparser.Result();

				while (!context.empty()) {
					InformationParser infoparser(&context);
					auto [name, trigger, min, max] = infoparser.Result();
					CommandParser cmdparser(&context);
					Event::Commands commands = std::move(cmdparser.Result());

					Event event{ .commands = std::move(commands), .trigger_type = trigger, .origin{ min, max } };
					events[name] = std::move(event);
				}
			}

			[[nodiscard]] auto Result() noexcept {
				return std::move(events);
			}
		};

		EventGenerator(const String& Path) noexcept {
			FILE *fp = nullptr;
			if (_wfopen_s(&fp, (Path + L".ges").c_str(), L"r,ccs=UNICODE") == 0) {
				String sentence = L"";
				for (Char c; (c = fgetwc(fp)) != WEOF;)
					sentence += c;

				Parser parser(sentence);
				events = std::move(parser.Result());
				fclose(fp);
			}
		}

		[[nodiscard]] auto Result() noexcept {
			return std::move(events);
		}
	};

	// 
	void Manager::ConditionManager::SetTarget(std::any* tv) {
		target_variable = tv;
	}

	// 条件式を評価する
	bool Manager::ConditionManager::Evalute(const String& Sentence) const noexcept {
		auto& type = target_variable->type();
		if (type == typeid(int)) {
			return std::any_cast<int>(*target_variable) == ToInt(Sentence.c_str(), nullptr);
		} else if (type == typeid(Dec)) {
			return std::any_cast<Dec>(*target_variable) == ToDec<Dec>(Sentence.c_str(), nullptr);
		} else {
			return std::any_cast<String>(*target_variable) == Sentence;
		}
	}

	void Manager::LoadEvent(const String path) noexcept {
		EventGenerator generator(path);
		events = std::move(generator.Result());
	}

	void Manager::Update() noexcept {
		std::queue<String> dead;
		for (auto& e : events) {
			auto& event = e.second;
			if (event.trigger_type == TriggerType::Load) {
				ExecuteEvent(e.first);
				if (event.commands.empty()) {
					dead.push(e.first);
				}
			}
		}

		while (!dead.empty()) {
			events.erase(dead.front());
			dead.pop();
		}
	}

	void Manager::ExecuteEvent(const WorldVector origin) noexcept {
		for (auto& e : events) {
			auto& event = e.second;
			if (event.origin[0][0] < origin[0] && origin[0] < event.origin[1][0] &&
				event.origin[0][1] < origin[1] && origin[1] < event.origin[1][1])
			{
				ExecuteEvent(e.first);
				break;
			}
		}
	}

	void Manager::ExecuteEvent(const String& EName) noexcept {
		CommandExecuter cmd_executer(std::move(events.at(EName).commands));
		cmd_executer.Execute();
		auto& event = events.at(EName);
		event.commands = std::move(cmd_executer.Result());
		event.commands.clear();
	}

	void Manager::SetCMDParser(void* a) {
		cmdparser = a;
	}

	void Manager::AliasCommand(const String& O, const String& N) {
		static_cast<EventGenerator::Parser::CommandParser*>(cmdparser)->MakeAlias(O, N);
	}

	void Manager::SetCondTarget(std::any* tv) {
		condition_manager.SetTarget(tv);
	}

	bool Manager::Evalute(const String& Sentence) {
		return condition_manager.Evalute(Sentence);
	}

	void command::Alias::Execute() {
		GetProgram()->event_manager.AliasCommand(original, newone);
		executed = true;
	}
}