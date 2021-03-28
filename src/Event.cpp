#include "Event.hpp"

#include "Canvas.hpp"

#include "Engine.hpp"

#include <queue>
#include <chrono>
#include <forward_list>

namespace karapo::event {
	using Context = std::queue<std::wstring, std::list<std::wstring>>;

	namespace {
		std::pair<std::wstring, std::wstring> GetParamInfo(const std::wstring& Param) {
			// 引数の情報。
			const auto Index = Param.find(L':');
			const auto Var = (Index == Param.npos ? Param : Param.substr(0, Index));
			const auto Type = (Index == Param.npos ? L"" : Param.substr(Index + 1));
			return { Var, Type };
		}

		bool IsStringType(const std::wstring& Param_Type) noexcept {
			return Param_Type == innertype::String;
		}

		bool IsNumberType(const std::wstring& Param_Type) noexcept {
			return Param_Type == innertype::Number;
		}

		bool IsNoType(const std::wstring& Param_Type) noexcept {
			return Param_Type == innertype::None;
		}
	}

	namespace command {
		// Entity操作系コマンド
		class EntityCommand : public Command {
			std::wstring entity_name;
		protected:
			std::shared_ptr<karapo::Entity> GetEntity() const noexcept {
				return Program::Instance().entity_manager.GetEntity(entity_name.c_str());
			}
		};

		// 普通のコマンド
		// executed変数により、実行云々の情報を管理する。
		class StandardCommand : public Command {
			bool executed = false;	// 実行したか否か。
		public:
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

		// 変数
		class Variable : public StandardCommand {
			std::wstring varname;
			std::any value;
		public:
			Variable(const std::wstring& VName, const std::wstring& Any_Value) noexcept {
				varname = VName;
				auto [iv, ir] = ToInt<int>(Any_Value.c_str());
				auto [fv, fr] = ToDec<Dec>(Any_Value.c_str());

				if (wcslen(ir) <= 0) {
					value = iv;
				} else if (wcslen(fr) <= 0) {
					value = fv;
				} else {
					try {
						value = Program::Instance().var_manager.Get<true>(VName);
					} catch (std::out_of_range&) {
						value = Any_Value;
					}
				}
			}

			~Variable() noexcept final {}

			void Execute() override {
				Program::Instance().var_manager.MakeNew(varname) = value;
				StandardCommand::Execute();
			}
		};

		// 条件対象設定
		class Case : public StandardCommand {
			std::any value;
		public:
			Case(const std::wstring& Param) noexcept {
				const auto [Value, Type] = GetParamInfo(Param);
				if (IsStringType(Type)) {
					value = Value;
				} else if (IsNumberType(Type)) {
					value = std::stoi(Value);
				}
			}

			~Case() noexcept final {}

			void Execute() override {
				Program::Instance().event_manager.SetCondTarget(value);
				StandardCommand::Execute();
			}
		};

		// 条件式
		class Of : public StandardCommand {
			std::wstring condition_sentence;
		public:
			Of(const std::wstring& Condition_Sentence) noexcept {
				condition_sentence = Condition_Sentence;
			}

			~Of() noexcept final {}

			void Execute() override {
				Program::Instance().event_manager.Evalute(condition_sentence);
				StandardCommand::Execute();
			}
		};

		// 条件終了
		class EndCase final : public StandardCommand {
		public:
			EndCase() noexcept {}
			~EndCase() final {}

			void Execute() final {
				Program::Instance().event_manager.FreeCase();
				StandardCommand::Execute();
			}
		};

		// 画像を読み込み、表示させる。
		class Image : public StandardCommand {
			std::shared_ptr<karapo::entity::Image> image;
			const std::wstring Path;
		public:
			Image(const std::wstring& P, const WorldVector WV) : Path(P) {
				image = std::make_shared<karapo::entity::Image>(WV);
			}

			~Image() override {}

			void Execute() override {
				image->Load(Path.c_str());
				Program::Instance().entity_manager.Register(image);
				StandardCommand::Execute();
			}
		};

		// BGM
		class Music final : public StandardCommand {
			std::shared_ptr<karapo::entity::Sound> music;
			const std::wstring Path;
		public:
			Music(const std::wstring& P) : Path(P) {
				music = std::make_shared<karapo::entity::Sound>(WorldVector{ 0, 0 });
			}

			~Music() override {}

			void Execute() override {
				music->Load(Path);
				Program::Instance().entity_manager.Register(music);
				StandardCommand::Execute();
			}
		};

		// 効果音
		class Sound final : public StandardCommand {
			std::shared_ptr<karapo::entity::Sound> sound;
			const std::wstring Path;
		public:
			Sound(const std::wstring& P, const WorldVector& WV) : Path(P) {
				sound = std::make_shared<karapo::entity::Sound>(WV);
			}

			~Sound() noexcept override {}

			void Execute() override {
				sound->Load(Path);
				Program::Instance().entity_manager.Register(sound);
				StandardCommand::Execute();
			}
		};

		namespace entity {
			// Entityの移動
			class Teleport final : public StandardCommand {
				std::wstring entity_name;
				WorldVector move;
			public:
				Teleport(const std::wstring& ename, const WorldVector& MV) noexcept {
					entity_name = ename;
					move = MV;
				}

				~Teleport() override {}

				void Execute() override {
					auto ent = Program::Instance().entity_manager.GetEntity(entity_name);
					ent->Teleport(move);
					StandardCommand::Execute();
				}
			};

			// Entityの削除。
			class Kill final : public StandardCommand {
				std::wstring entity_name;
			public:
				Kill(const std::wstring& ename) noexcept {
					entity_name = ename;
				}

				~Kill() override {}

				void Execute() override {
					Program::Instance().entity_manager.Kill(entity_name);
					StandardCommand::Execute();
				}
			};
		}

		// コマンドの別名
		class Alias : public StandardCommand {
			std::wstring newone, original;
		public:
			Alias(std::wstring s1, std::wstring s2) noexcept {
				original = s1;
				newone = s2;
			}

			~Alias() noexcept final {}

			void Execute() override;
		};

		class Filter final : public StandardCommand {
			int index, potency;
			std::wstring kind_name;
		public:
			Filter(const int I, const std::wstring& KN, const int P) noexcept {
				index = I;
				kind_name = KN;
				potency = P % 256;
			}

			~Filter() final {}

			void Execute() final {
				Program::Instance().canvas.ApplyFilter(index, kind_name, potency);
				StandardCommand::Execute();
			}
		};

		class DLLCommand : public StandardCommand {
		protected:
			std::wstring dll_name;
		public:
			DLLCommand(const std::wstring& dname) noexcept {
				dll_name = dname;
			}

			~DLLCommand() override {}
		};

		// DLLアタッチ
		class Attach final : public DLLCommand {
		public:
			inline Attach(const std::wstring& dname) : DLLCommand(dname) {}
			
			void Execute() final {
				Program::Instance().dll_manager.Load(dll_name);
				StandardCommand::Execute();
			}
		};

		class Detach final : public DLLCommand {
		public:
			inline Detach(const std::wstring& dname) : DLLCommand(dname) {}

			void Execute() final {
				Program::Instance().dll_manager.Detach(dll_name);
				StandardCommand::Execute();
			}
		};

		// イベント呼出
		class Call : public StandardCommand {
			std::wstring event_name;
		public:
			Call(const std::wstring& ename) noexcept {
				event_name = ename;
			}

			~Call() noexcept final {}

			void Execute() override {
				Program::Instance().event_manager.Call(event_name);
				StandardCommand::Execute();
			}
		};

		namespace hidden {
			class EndOf final : public StandardCommand {
				bool executed = false;
			public:
				EndOf() noexcept {}
				~EndOf() noexcept final {}

				void Execute() final {
					Program::Instance().event_manager.FreeOf();
					StandardCommand::Execute();
				}

				bool IgnoreCondition() const noexcept final {
					return true;
				}
			};
		}
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

			if (!cmd->IsUnnecessary() && (cmd->IgnoreCondition() || Program::Instance().event_manager.condition_manager.Can_Execute)) {
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
	class EventGenerator final : private Singleton {
	public:
		// 解析クラス
		class Parser final {
			// 一文字ずつの解析器。
			// 空白を基準とする単語単位の解析結果をcontextとして排出。
			class LexicalParser final {
				Context context;
			public:
				LexicalParser(const std::wstring Sentence) noexcept {
					auto PushWord = [](Context  *const context, std::wstring *const text) noexcept {
						wprintf_s(L"Pushed:%s\n", text->c_str());
						context->push(*text);
						text->clear();
					};

					// 読み込もうとした単語が、文字列として書かれているかを判定する為の変数
					// 値がtrueの間、解析器はスペースを区切り文字として認識しなくなる。
					bool is_string = false;
					auto text = std::wstring(L"");

					for (auto c : Sentence) {
						if (!is_string) {
							// スペース判定
							if (IsSpace(c) || c == L'\0' || c == L'\n') {
								// 貯めこんだ文字を単語として格納
								if (!text.empty()) {
									PushWord(&context, &text);
									if (c == L'\n') {
										text = L"\n";
										PushWord(&context, &text);
									}
								}
								continue;
							} else if (c == L'\r') {
								// 復帰コードは無視。
								continue;
							}

							// 演算子判定
							switch (c) {
								case L'\'':
									is_string = true;
									break;
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
									text += c;
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

			class TypeDeterminer final {
				Context compiled{};
			public:
				TypeDeterminer(Context& context) {
					while (!context.empty()) {
						auto&& word = std::move(context.front());
						if (iswdigit(word[0])) {
							word += std::wstring(L":") + innertype::Number;
						} else if (word[0] == L'\'') {
							word += std::wstring(L":") + innertype::String;
							for (size_t pos = word.find(L'\''); pos != std::wstring::npos; pos = word.find(L'\'')) {
								word.erase(pos, 1);
							}
						}
						compiled.push(std::move(word));
						context.pop();
					}
				}

				auto Result() noexcept {
					return std::move(compiled);
				}
			};

			// SCからECまでの範囲の文章を解析する解析器。
			// それ以外には、解析中かどうかを表現するための関数群を持つ。
			template<wchar_t SC, wchar_t EC>
			struct SubParser {
				static constexpr bool IsValidToStart(const wchar_t Ch) noexcept { return SC == Ch; }
				static constexpr bool IsValidToEnd(const wchar_t Ch) noexcept { return EC == Ch; }

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
					std::wstring name;
				public:
					[[nodiscard]] auto Interpret(const std::wstring& Sentence) noexcept {
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
					std::unordered_map<std::wstring, TriggerType> trigger_map;
				public:
					TriggerParser() {
						trigger_map[L"t"] = TriggerType::Trigger;
						trigger_map[L"b"] = TriggerType::Button;
						trigger_map[L"l"] = TriggerType::Load;
						trigger_map[L"n"] = TriggerType::None;
					}

					[[nodiscard]] auto Interpret(const std::wstring& Sentence) noexcept {
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
					[[nodiscard]] auto Interpret(const std::wstring& Sentence) noexcept {
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

				std::wstring event_name;
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

				std::tuple<std::wstring, TriggerType, WorldVector, WorldVector> Result()const noexcept {
					return { event_name, trigger, min_origin, max_origin };
				}
			};

			std::unordered_map<std::wstring, Event> events;
			std::wstring ename;
		public:
			// コマンド解析クラス
			// 単語に基づくコマンドの生成と、それの結果を排出する。
			class CommandParser final : public SubParser<L'{', L'}'> {
				// 予約語
				std::unordered_map<std::wstring, GenerateFunc> words;

				std::vector<std::wstring> parameters;	// 引数
				Event::Commands commands;	// 
				GenerateFunc liketoRun;				// 生成関数を実行するための関数ポインタ。
				std::wstring generating_command_name = L"";
				bool request_abort = false;

				void CheckCommandWord(const std::wstring& text) noexcept(false) {
					// コマンド生成関数を取得。
					auto gen = words.at(text);
					if (liketoRun == nullptr) {
						liketoRun = gen;
						generating_command_name = text;
					} else {
						// 既に生成関数が設定されている時:
						throw std::runtime_error("一行にコマンドが2つ以上書かれています。");
					}
				}

				void CheckArgs(const std::wstring& text) noexcept(false) {
					// 引数の型をチェックする。

					// 引数の情報。
					const auto Index = text.find(L':');
					const auto Type = (Index == text.npos ? L"" : text.substr(Index));

					// 引数が(変数ではない)定数値かどうか。
					const bool Is_Const = !Type.empty();
					if (Is_Const) {
						parameters.push_back(text);
					} else {
						// 変数探し
						auto& value = Program::Instance().var_manager.Get<true>(text);
						if (value.type() == typeid(int))
							parameters.push_back(std::to_wstring(std::any_cast<int>(value)) + std::wstring(L":") + innertype::Number);
						else if (value.type() == typeid(Dec))
							parameters.push_back(std::to_wstring(std::any_cast<Dec>(value)) + std::wstring(L":") + innertype::Number);
						else
							parameters.push_back(std::any_cast<std::wstring>(value) + std::wstring(L":") + innertype::String);
					}
				}

				bool CompileCommand() {
					auto&& f = liketoRun(parameters);
					if (f.isEnough()) {
						auto&& result = f.Result();
						if (result != nullptr) {
							// 引数が十分に積まれている時:
							if (f.is_dynamic) {
								if (generating_command_name == L"of") {
									auto endof = words.at(L"__endof")({}).Result();
									commands.push_back(std::move(endof));
								}

								// 動的コマンドはイベントのコマンドに追加。
								commands.push_back(std::move(result));

							} else if (f.is_static) {
								// 静的コマンドは即実行。
								result->Execute();
							}
						}
						liketoRun = nullptr;
						parameters.clear();
						return true;
					}
					return false;
				}

				void Interpret(Context *context) noexcept {
					auto text = context->front();
					bool compiled = false;
					if (IsParsing()) {
						// HACK: CheckCommandWordとCheckArgsを含め、「'」等の引数に含まれない文字に対する処理を改善する。

						if (!IsValidToEnd(text[0])) {
							bool is_command = false;

							// ここで、一行内のコマンドとその為の引数を一つずつ確認していく。
							// コマンド確認
							try {
								CheckCommandWord(text);
								is_command = true;
							} catch (std::runtime_error& e) {
								// 主にCheckCommandWordからの例外をここで捕捉。
								if (liketoRun != nullptr) {
									MessageBoxA(nullptr, e.what(), "エラー", MB_OK | MB_ICONERROR);
									request_abort = true;
								}
							} catch (std::out_of_range& e) {}

							if (!is_command && !request_abort) {
								try {
									CheckArgs(text);			// 引数確認。
								} catch (std::out_of_range& e) {
									const bool Is_Variable = (generating_command_name == L"var" || generating_command_name == L"変数");
									// 主にCheckArgsからの例外をここで捕捉。
									if (Is_Variable) {
										// 変数コマンドの引数だった場合は変数名として引数に積む。
										parameters.push_back(text);
									} else {
										MessageBoxA(nullptr, "未定義の変数が使用されています。", "エラー", MB_OK | MB_ICONERROR);
										request_abort = true;
									}
								}
							}

							if (!request_abort) {
								compiled = CompileCommand();
							}
						}
					}

					if (request_abort) {
						EndParsing();
						return;
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
					if (compiled) {
						while (context->front() == L"\n") {
							context->pop();
						}
					}
					Interpret(context);
				}
			public:
				// コマンドの別名を作成する。
				void MakeAlias(const std::wstring& S1, const std::wstring& S2) noexcept {
					words[S1] = words.at(S2);
				}

				bool NeedAbortion() const noexcept {
					return request_abort;
				}

				CommandParser(Context *context) noexcept {
					EventGenerator::Instance().cmdparser = this;
					
					// ヒットさせる単語を登録する。

					Program::Instance().dll_manager.RegisterExternalCommand(&words);

					words[L"music"] =
						words[L"音楽"] =
						words[L"BGM"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
					{
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Var, Type] = GetParamInfo(params[0]);
								return (IsStringType(Type) ? std::make_unique<command::Music>(Var) : nullptr);
							},
							.isEnough = [params]() -> bool { return params.size() == 1; },
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"sound"] =
						words[L"効果音"] =
						words[L"音"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
					{
						printf("There is a sound!\n");
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [File_Path, Path_Type] = GetParamInfo(params[0]);
								const auto [Vec_X, X_Type] = GetParamInfo(params[1]);
								const auto [Vec_Y, Y_Type] = GetParamInfo(params[2]);
								if (IsStringType(Path_Type) && IsNumberType(X_Type) && IsNumberType(Y_Type)) {
									return std::make_unique<command::Sound>(File_Path, WorldVector{ ToDec<Dec>(Vec_X.c_str(), nullptr), ToDec<Dec>(Vec_Y.c_str(), nullptr) });
								} else {
									return nullptr;
								}
							},
							.isEnough = [params]() -> bool { return params.size() == 1; },
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"image"] =
						words[L"画像"] =
						words[L"絵"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
					{
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [File_Path, Path_Type] = GetParamInfo(params[0]);
								const auto [Vec_X, X_Type] = GetParamInfo(params[1]);
								const auto [Vec_Y, Y_Type] = GetParamInfo(params[2]);
								if (IsStringType(Path_Type) && IsNumberType(X_Type) && IsNumberType(Y_Type)) {
									return std::make_unique<command::Image>(File_Path, WorldVector{ ToDec<Dec>(Vec_X.c_str(), nullptr), ToDec<Dec>(Vec_Y.c_str(), nullptr) });
								} else {
									return nullptr;
								}
							},
							.isEnough = [params]() -> bool { return params.size() == 3; },
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"teleport"] =
						words[L"瞬間移動"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Target, Target_Type] = GetParamInfo(params[0]);
								const auto [X, X_Type] = GetParamInfo(params[1]);
								const auto [Y, Y_Type] = GetParamInfo(params[2]);
								if (IsStringType(Target_Type) && IsNumberType(X_Type) && IsNumberType(Y_Type)) {
									return std::make_unique<command::entity::Teleport>(
										Target,
										WorldVector{ ToDec<Dec>(X.c_str(), nullptr), ToDec<Dec>(Y.c_str(), nullptr)
									});
								} else {
									return nullptr;
								}
							},
							.isEnough = [params]() -> bool { return params.size() == 3; },
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"kill"] =
						words[L"殺害"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Var, Type] = GetParamInfo(params[0]);
								return (IsStringType(Type) ? std::make_unique<command::entity::Kill>(Var) : nullptr);
							},
							.isEnough = [params]() -> bool { return params.size() == 1; },
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"alias"] =
						words[L"別名"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								const auto [Base, Base_Type] = GetParamInfo(params[0]);
								const auto [New_One, New_Type] = GetParamInfo(params[1]);
								if (IsNoType(Base_Type) && IsNoType(New_Type))
									return std::make_unique<command::Alias>(Base, New_One);
								else
									return nullptr;
							},
							.isEnough = [params]() -> bool { return params.size() == 2; },
							.is_static = true,
							.is_dynamic = false
						};
					};

					// DLLアタッチ
					words[L"attach"] =
						words[L"接続"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								const auto [Var, Type] = GetParamInfo(params[0]);
								return (IsStringType(Type) ? std::make_unique<command::Attach>(Var) : nullptr);
							},
							.isEnough = [params]() -> bool { return params.size() == 1; },
							.is_static = true,
							.is_dynamic = false
						};
					};

					// DLLデタッチ
					words[L"detach"] =
						words[L"切断"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								const auto [Var, Type] = GetParamInfo(params[0]);
								return (IsStringType(Type) ? std::make_unique<command::Detach>(Var) : nullptr);
							},
							.isEnough = [params]() -> bool { return params.size() == 1; },
							.is_static = true,
							.is_dynamic = false
						};
					};

					words[L"call"] =
						words[L"呼出"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								const auto [Event_Name, Name_Type] = GetParamInfo(params[0]);
								if (IsStringType(Name_Type))
									return std::make_unique<command::Call>(Event_Name);
								else
									return nullptr;
							},
							.isEnough = [params]() -> bool { return params.size() == 1; },
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"var"] =
						words[L"変数"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								const auto [Var, Var_Type] = GetParamInfo(params[0]);
								const auto [Value, Value_Type] = GetParamInfo(params[1]);
								if (Var_Type == L"")
									return std::make_unique<command::Variable>(Var, Value);
								else
									return nullptr;
							},
							.isEnough = [params]() -> bool { return params.size() == 2; },
							.is_static = true,
							.is_dynamic = false
						};
					};

					words[L"filter"] =
						words[L"フィルター"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								const auto Index = std::stoi(params[0]);
								const auto Potency = std::stoi(params[2]);
								return std::make_unique<command::Filter>(Index, params[1], Potency);
							},
							.isEnough = [params]() -> bool { return params.size() == 3; },
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"case"] =
						words[L"条件"] = [&](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								return std::make_unique<command::Case>(params[0]);
							},
							.isEnough = [params]() -> bool { return params.size() == 1; },
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"of"] =
						words[L"分岐"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								const auto [Var, Type] = GetParamInfo(params[0]);
								if (!IsNoType(Type))
									return std::make_unique<command::Of>(Var);
								else
									return nullptr;
							},
							.isEnough = [params]() -> bool { return params.size() == 1; },
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"__endof"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr { return std::make_unique<command::hidden::EndOf>(); },
							.isEnough = [params]() -> bool { return params.size() == 0; },
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"endcase"] =
						words[L"分岐終了"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								return std::make_unique<command::EndCase>();
							},
							.isEnough = [params]() -> bool { return params.size() == 0; },
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

			// 字句解析し、その結果を返す。
			Context ParseLexical(const std::wstring& Sentence) const noexcept {
				LexicalParser lexparser(Sentence);
				return lexparser.Result();
			}

			// 型を決定し、その結果を返す。
			Context DetermineType(Context context) const noexcept {
				TypeDeterminer type_determiner(context);
				return type_determiner.Result();
			}

			// 字句解析と型決定を行い、その結果を返す。
			Context ParseBasic(const std::wstring& Sentence) const noexcept {
				return DetermineType(ParseLexical(Sentence));
			}

			// イベント情報を解析し、その結果を返す。
			auto ParseInformation(Context* context) const noexcept {
				InformationParser infoparser(context);
				return infoparser.Result();
			}

			// コマンドを解析し、その結果を返す。
			Event::Commands ParseCommand(Context* context, bool* const abort) const noexcept {
				CommandParser cmdparser(context);
				if (abort != nullptr) *abort = cmdparser.NeedAbortion();
				return std::move(cmdparser.Result());
			}

			Parser() = default;

			Parser(const std::wstring& Sentence) noexcept {
				auto context = ParseBasic(Sentence);
				bool aborted = false;

				while (!context.empty() && !aborted) {
					auto [name, trigger, min, max] = ParseInformation(&context);
					Event::Commands commands = ParseCommand(&context, &aborted);

					Event event{ .commands = std::move(commands), .trigger_type = trigger, .origin{ min, max } };
					events[name] = std::move(event);
				}

				if (aborted) {
					MessageBoxW(nullptr, L"イベント解析を強制終了しました。", L"エラー", MB_OK | MB_ICONERROR);
					events.clear();
				}
			}

			[[nodiscard]] auto Result() noexcept {
				return std::move(events);
			}
		};

		static EventGenerator& Instance() noexcept {
			static EventGenerator evg;
			return evg;
		}

		void Generate(const std::wstring& Path) noexcept {
			CharCode cc;
			char* plain_text;
			ReadWTextFile((Path + L".ges").c_str(), &plain_text, &cc);
			std::wstring sentence;
			switch (cc) {
				case CharCode::CP932:
				{
					std::string str = plain_text;
					wchar_t *converted = new wchar_t[str.size()];
					CP932ToWide(converted, str.c_str(), str.size());
					sentence = converted;
					delete[] plain_text;
					delete[] converted;
					break;
				}
				case CharCode::UTF8:
				{
					std::u8string str = reinterpret_cast<char8_t*>(plain_text);
					wchar_t *converted = new wchar_t[str.size()];
					UTF8ToWide(converted, str.c_str(), str.size());
					sentence = converted;
					delete[] plain_text;
					delete[] converted;
					break;
				}
				case CharCode::UTF16BE:
					Reverse16Endian(reinterpret_cast<char16_t*>(plain_text));
					[[fallthrough]];
				case CharCode::UTF16LE:
				{
					std::u16string str = reinterpret_cast<char16_t*>(plain_text);
					wchar_t *converted = new wchar_t[str.size()];
					UTF16ToWide(converted, str.c_str(), str.size());
					sentence = converted;
					delete[] plain_text;
					delete[] converted;
					break;
				}
				case CharCode::UTF32BE:
					Reverse32Endian(reinterpret_cast<char32_t*>(plain_text));
					[[fallthrough]];
				case CharCode::UTF32LE:
				{
					std::u32string str = reinterpret_cast<char32_t*>(plain_text);
					wchar_t *converted = new wchar_t[str.size()];
					UTF32ToWide(converted, str.c_str(), str.size());
					sentence = converted;
					delete[] plain_text;
					delete[] converted;
					break;
				}
			}
			Parser parser(sentence);
			events = std::move(parser.Result());
		}
	
		void MakeAliasCommand(const std::wstring& O, const std::wstring& N) {
			cmdparser->MakeAlias(O, N);
		}

		[[nodiscard]] auto Result() noexcept {
			return std::move(events);
		}
	private:
		Parser::CommandParser *cmdparser = nullptr;
		std::unordered_map<std::wstring, Event> events;
	};

	// 
	void Manager::ConditionManager::SetTarget(std::any& tv) {
		target_value = tv;
	}

	// 条件式を評価する
	void Manager::ConditionManager::Evalute(const std::wstring& Sentence) noexcept {
		can_execute = true;
		auto& type = target_value.type();
		if (type == typeid(int)) {
			can_execute = std::any_cast<int>(target_value) == ToInt(Sentence.c_str(), nullptr);
		} else if (type == typeid(Dec)) {
			can_execute = std::any_cast<Dec>(target_value) == ToDec<Dec>(Sentence.c_str(), nullptr);
		} else {
			can_execute = std::any_cast<std::wstring>(target_value) == Sentence;
		}
	}

	void Manager::ConditionManager::FreeCase() {
		target_value.reset();
		FreeOf();
	}

	void Manager::ConditionManager::FreeOf() {
		can_execute = true;
	}

	void Manager::LoadEvent(const std::wstring path) noexcept {
		EventGenerator::Instance().Generate(path);
		events = std::move(EventGenerator::Instance().Result());
	}

	void Manager::Update() noexcept {
		std::queue<std::wstring> dead;
		for (auto& e : events) {
			auto& event = e.second;
			if (event.trigger_type == TriggerType::Load) {
				Call(e.first);
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
				Call(e.first);
				break;
			}
		}
	}

	void Manager::Call(const std::wstring& EName) noexcept {
		CommandExecuter cmd_executer(std::move(events.at(EName).commands));
		cmd_executer.Execute();
		Event event;
		try {
			auto&& e = events.at(EName);
			event = std::move(e);
		} catch (std::out_of_range& e) {
			std::wstring message = L"イベント名「" + EName + L"」が見つからなかったので実行できません。";
			MessageBoxW(nullptr, message.c_str(), L"イベントエラー", MB_OK | MB_ICONERROR);
			return;
		}
		event.commands = std::move(cmd_executer.Result());
		event.trigger_type = TriggerType::None;
	}

	void Manager::SetCondTarget(std::any tv) {
		condition_manager.SetTarget(tv);
	}

	void Manager::Evalute(const std::wstring& Sentence) {
		condition_manager.Evalute(Sentence);
	}

	void Manager::FreeCase() {
		condition_manager.FreeCase();
	}

	void Manager::FreeOf() {
		condition_manager.FreeOf();
	}

	void Manager::MakeEmptyEvent(const std::wstring& Event_Name) {
		events[Event_Name] = Event();
	}

	Event* Manager::GetEvent(const std::wstring& Event_Name) noexcept {
		auto e = events.find(Event_Name);
		return (e != events.end() ? &e->second : nullptr);
	}

	bool EventEditor::IsEditing() const noexcept {
		return (targeting != nullptr);
	}

	bool EventEditor::IsEditing(const std::wstring& Event_Name) const noexcept {
		return (IsEditing() && Program::Instance().event_manager.GetEvent(Event_Name) == targeting);
	}

	void EventEditor::MakeNewEvent(const std::wstring& Event_Name) {
		Program::Instance().event_manager.MakeEmptyEvent(Event_Name);
		SetTarget(Event_Name);
	}

	void EventEditor::AddCommand(const std::wstring& Command_Sentence, const int Index) {
		EventGenerator::Parser parser;
		auto context = parser.ParseBasic(Command_Sentence);
		auto commands = parser.ParseCommand(&context, nullptr);

		for (auto& cmd : commands) {
			if (Index < 0 && !targeting->commands.empty())
				targeting->commands.insert(targeting->commands.end() + Index + 1, std::move(cmd));
			else {
				if (targeting->commands.empty())
					targeting->commands.insert(targeting->commands.begin(), std::move(cmd));
				else
					targeting->commands.insert(targeting->commands.begin() + Index, std::move(cmd));
			}
		}
	}

	void EventEditor::SetTarget(const std::wstring& Event_Name) {
		targeting = Program::Instance().event_manager.GetEvent(Event_Name);
	}

	void EventEditor::ChangeRange(const WorldVector& Min, const WorldVector& Max) {
		targeting->origin[0] = Min;
		targeting->origin[1] = Max;
	}

	void EventEditor::ChangeTriggerType(const std::wstring& TSentence) {
		if (TSentence == L"l")
			targeting->trigger_type = TriggerType::Load;
		else if (TSentence == L"n")
			targeting->trigger_type = TriggerType::None;
		else if (TSentence == L"b")
			targeting->trigger_type = TriggerType::Button;
		else if (TSentence == L"t")
			targeting->trigger_type = TriggerType::Trigger;
	}

	void command::Alias::Execute() {
		EventGenerator::Instance().MakeAliasCommand(original, newone);
		StandardCommand::Execute();
	}
}