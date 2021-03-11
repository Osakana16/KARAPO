#include "Event.hpp"

#include "Canvas.hpp"

#include "Engine.hpp"

#include <queue>
#include <chrono>
#include <forward_list>

namespace karapo::event {
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
				return GetProgram()->entity_manager.GetEntity(entity_name.c_str());
			}
		};

		// 変数
		class Variable : public Command {
			std::wstring varname;
			std::any value;
			bool executed = false;
		public:
			Variable(const std::wstring& VName, const std::wstring& Any_Value) noexcept {
				varname = VName;
				auto [iv, ir] = ToInt<int>(VName.c_str());
				auto [fv, fr] = ToDec<Dec>(VName.c_str());

				if (ir == nullptr) {
					value = iv;
				} else if (fr == nullptr) {
					value = fv;
				} else {
					try {
						value = GetProgram()->var_manager.Get<true>(VName);
					} catch (std::out_of_range&) {
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

		// 条件対象設定
		class Case : public Command {
			std::wstring varname;
			bool executed = false;
		public:
			Case(const std::wstring& Name) noexcept {
				varname = Name;
			}

			void Execute() override {
				GetProgram()->event_manager.SetCondTarget(&GetProgram()->var_manager.Get<true>(varname));
				executed = true;
			}

			bool Executed() const noexcept override {
				return executed;
			}

			bool IsUnnecessary() const noexcept override {
				return Executed();
			}
		};

		// 条件式
		class Of : public Command {
			std::wstring condition_sentence;
			bool executed = false;
		public:
			Of(const std::wstring& Condition_Sentence) noexcept {
				condition_sentence = Condition_Sentence;
			}

			void Execute() override {
				GetProgram()->event_manager.Evalute(condition_sentence);
				executed = true;
			}

			bool Executed() const noexcept override {
				return executed;
			}

			bool IsUnnecessary() const noexcept override {
				return Executed();
			}
		};

		// 条件終了
		class EndCase final : public Command {
			bool executed = false;
		public:
			EndCase() noexcept {}
			~EndCase() final {}

			void Execute() final {
				GetProgram()->event_manager.FreeCase();
				executed = true;
			}

			bool Executed() const noexcept final {
				return executed;
			}

			bool IsUnnecessary() const noexcept final {
				return Executed();
			}
		};

		// 画像を読み込み、表示させる。
		class Image : public Command {
			std::shared_ptr<karapo::entity::Image> image;
			const std::wstring Path;
			bool executed = false;
		public:
			Image(const std::wstring& P, const WorldVector WV) : Path(P) {
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
			std::shared_ptr<karapo::entity::Sound> music;
			const std::wstring Path;
			bool executed = false;
		public:
			Music(const std::wstring& P) : Path(P) {
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
			std::shared_ptr<karapo::entity::Sound> sound;
			const std::wstring Path;
			bool executed = false;
		public:
			Sound(const std::wstring& P, const WorldVector& WV) : Path(P) {
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
			// Entityの移動
			class Teleport final : public Command {
				std::wstring entity_name;
				WorldVector move;
				bool executed;
			public:
				Teleport(const std::wstring& ename, const WorldVector& MV) noexcept {
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
				std::wstring entity_name;
				bool executed = false;
			public:
				Kill(const std::wstring& ename) noexcept {
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

		// コマンドの別名
		class Alias : public Command {
			std::wstring newone, original;
			bool executed = false;
		public:
			Alias(std::wstring s1, std::wstring s2) noexcept {
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

		class Filter final : public Command {
			int index, potency;
			std::wstring kind_name;
			bool executed = false;
		public:
			Filter(const int I, const std::wstring& KN, const int P) noexcept {
				index = I;
				kind_name = KN;
				potency = P % 256;
			}

			~Filter() final {}

			void Execute() final {
				GetProgram()->canvas.ApplyFilter(index, kind_name, potency);
				executed = true;
			}

			bool Executed() const noexcept final {
				return executed;
			}

			bool IsUnnecessary() const noexcept final {
				return Executed();
			}
		};

		class DLL : public Command {
		protected:
			std::wstring dll_name;
			bool executed;
		public:
			DLL(const std::wstring& dname) noexcept {
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
			inline Attach(const std::wstring& dname) : DLL(dname) {}
			
			void Execute() final {
				GetProgram()->dll_manager.Load(dll_name);
			}
		};

		class Detach final : public DLL {
		public:
			inline Detach(const std::wstring& dname) : DLL(dname) {}

			void Execute() final {
				GetProgram()->dll_manager.Detach(dll_name);
			}
		};

		// イベント呼出
		class Call : public Command {
			std::wstring event_name;
			bool executed = false;
		public:
			Call(std::wstring& ename) noexcept {
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

			if (!cmd->IsUnnecessary() && GetProgram()->event_manager.condition_manager.Can_Execute) {
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
		std::unordered_map<std::wstring, Event> events;
	public:
		// 解析クラス
		class Parser final {
			using Context = std::queue<std::wstring, std::list<std::wstring>>;

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
				bool is_variable = false;
				bool request_abort = false;

				void CheckCommandWord(const std::wstring& text) noexcept(false) {
					// コマンド生成関数を取得。
					auto gen = words.at(text);
					if (liketoRun == nullptr) {
						liketoRun = gen;
						is_variable = (text == L"var" || text == L"変数");
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
						auto& value = GetProgram()->var_manager.Get<true>(text);
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
									// 主にCheckArgsからの例外をここで捕捉。
									if (is_variable) {
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
					GetProgram()->event_manager.SetCMDParser(this);

					// ヒットさせる単語を登録する。
	
					GetProgram()->dll_manager.RegisterExternalCommand(&words);

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
								return (IsStringType(Type) ?std::make_unique<command::Attach>(Var) : nullptr);
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
								const auto [Var, Type] = GetParamInfo(params[0]);
								if (IsNoType(Type))
									return std::make_unique<command::Case>(Var);
								else
									return nullptr;
							},
							.isEnough = [params]() -> bool { return params.size() == 1; },
							.is_static = true,
							.is_dynamic = false
						};
					};

					words[L"of"] =
						words[L"分岐"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								const auto [Var, Type] = GetParamInfo(params[0]);
								if (IsNoType(Type))
									return std::make_unique<command::Of>(Var);
								else
									return nullptr;
							},
							.isEnough = [params]() -> bool { return params.size() == 1; },
							.is_static = true,
							.is_dynamic = false
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

			Parser(const std::wstring& Sentence) noexcept {
				LexicalParser lexparser(Sentence);
				auto context = lexparser.Result();
				TypeDeterminer type_determiner(context);
				context = type_determiner.Result();
				bool aborted = false;

				while (!context.empty() && !aborted) {
					InformationParser infoparser(&context);
					auto [name, trigger, min, max] = infoparser.Result();
					CommandParser cmdparser(&context);
					aborted = cmdparser.NeedAbortion();
					Event::Commands commands = std::move(cmdparser.Result());

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

		EventGenerator(const std::wstring& Path) noexcept {
			FILE *fp = nullptr;
			if (_wfopen_s(&fp, (Path + L".ges").c_str(), L"r,ccs=UNICODE") == 0) {
				std::wstring sentence = L"";
				for (wchar_t c; (c = fgetwc(fp)) != WEOF;)
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
	void Manager::ConditionManager::Evalute(const std::wstring& Sentence) noexcept {
		can_execute = true;
		auto& type = target_variable->type();
		if (type == typeid(int)) {
			can_execute = std::any_cast<int>(*target_variable) == ToInt(Sentence.c_str(), nullptr);
		} else if (type == typeid(Dec)) {
			can_execute = std::any_cast<Dec>(*target_variable) == ToDec<Dec>(Sentence.c_str(), nullptr);
		} else {
			can_execute = std::any_cast<std::wstring>(*target_variable) == Sentence;
		}
	}

	void Manager::ConditionManager::Free() {
		target_variable = nullptr;
		can_execute = true;
	}

	void Manager::LoadEvent(const std::wstring path) noexcept {
		EventGenerator generator(path);
		events = std::move(generator.Result());
	}

	void Manager::Update() noexcept {
		std::queue<std::wstring> dead;
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

	void Manager::ExecuteEvent(const std::wstring& EName) noexcept {
		CommandExecuter cmd_executer(std::move(events.at(EName).commands));
		cmd_executer.Execute();
		auto& event = events.at(EName);
		event.commands = std::move(cmd_executer.Result());
		event.commands.clear();
	}

	void Manager::SetCMDParser(void* a) {
		cmdparser = a;
	}

	void Manager::AliasCommand(const std::wstring& O, const std::wstring& N) {
		static_cast<EventGenerator::Parser::CommandParser*>(cmdparser)->MakeAlias(O, N);
	}

	void Manager::SetCondTarget(std::any* tv) {
		condition_manager.SetTarget(tv);
	}

	void Manager::Evalute(const std::wstring& Sentence) {
		condition_manager.Evalute(Sentence);
	}

	void Manager::FreeCase() {
		condition_manager.Free();
	}

	void command::Alias::Execute() {
		GetProgram()->event_manager.AliasCommand(original, newone);
		executed = true;
	}
}