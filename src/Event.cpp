#include "Event.hpp"

#include "Canvas.hpp"

#include "Engine.hpp"

#include <queue>
#include <chrono>
#include <forward_list>

#define DYNAMIC_COMMAND(NAME) class NAME : public DynamicCommand
#define DYNAMIC_COMMAND_CONSTRUCTOR(NAME) NAME(const std::vector<std::wstring>& Param) : DynamicCommand(Param)

namespace karapo::event {
	namespace {
		// 文字列中の{}で指定された部分に変数の値で置換する。
		void ReplaceFormat(std::wstring* const sentence) noexcept {
			size_t pos = 0;
			for (;;) {
				pos = sentence->find(L'{', pos);
				if (pos == sentence->npos)
					break;
				auto epos = sentence->find('}', pos);

				auto begin = sentence->substr(0, pos);
				auto str = sentence->substr(pos + 1, epos - pos - 1);
				auto end = sentence->substr(epos + 1);
				if (begin == L"{")
					begin.clear();
				if (end == L"}")
					end.clear();

				auto executing_name = std::any_cast<std::wstring>(
					Program::Instance().var_manager.Get<false>(variable::Executing_Event_Name)
				);
				executing_name.pop_back();
				if (auto pos = executing_name.rfind(L'\n'); pos != executing_name.npos)
					executing_name = executing_name.substr(pos);
				auto var = Program::Instance().var_manager.Get<false>(executing_name + L'.' + str);
				if (var.type() == typeid(std::wstring)) {
					*sentence = begin + std::any_cast<std::wstring>(var) + end;
				} else if (var.type() == typeid(int)) {
					*sentence = begin + std::to_wstring(std::any_cast<int>(var)) + end;
				} else if (var.type() == typeid(Dec)) {
					*sentence = begin + std::to_wstring(std::any_cast<Dec>(var)) + end;
				} else {
					*sentence = begin + end;
				}
				pos++;
			}
		}
	}
	using Context = std::list<std::wstring>;

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

			void Reset() noexcept override {
				executed = false;
			}

			bool Executed() const noexcept override {
				return executed;
			}

			bool IsUnnecessary() const noexcept override {
				return Executed();
			}
		};

		// 動的コマンド向け抽象クラス。
		// 引数をコマンド実行時に読み込む為のメンバ関数を持つ。
		class DynamicCommand : public StandardCommand {
			// 読み込む予定の引数名。
			std::vector<std::wstring> param_names{};
		protected:
			DynamicCommand() = default;

			DynamicCommand(const decltype(param_names)& Param) noexcept {
				param_names = Param;
			}

			// 引数をコマンド実行時に読み込む必要があるか否か。
			bool MustSearch() const noexcept {
				return !param_names.empty();
			}

			// 指定した番号から引数名を読み込む。
			template<typename T, const bool Get_Param_Name = false>
			T GetParam(const int Index) const {
				if constexpr (!Get_Param_Name) {
					auto [vname, vtype] = Default_ProgramInterface.GetParamInfo(param_names[Index]);
					if constexpr (!std::is_same_v<T, std::any>) {
						// 型が判明する場合、定数なのでその値を取得。
						if (Default_ProgramInterface.IsStringType(vtype))
							return std::any_cast<T>(vname);
						else if (Default_ProgramInterface.IsNumberType(vtype)) {
							if constexpr (std::is_same_v<T, Dec>) {
								auto [fv, fp] = ToDec<Dec>(vname.c_str());
								return fv;
							} else if constexpr (std::is_same_v<T, int>) {
								auto [iv, ip] = ToInt(vname.c_str());
								return iv;
							}
						}
						else {
							// 型がない場合は変数管理オブジェクトから取得。
							return std::any_cast<T>(Program::Instance().var_manager.Get<false>(vname));
						}
					} else {
						// std::anyが指定された場合、変数から直接取得。
						return Program::Instance().var_manager.Get<false>(vname);
					}
				} else {
					// 引数名を返す。
					return Default_ProgramInterface.GetParamInfo(param_names[Index]).first;
				}
			}

			void SetAllParams(std::vector<std::any>* to) {
				for (int i = 0; i < param_names.size(); i++) {
					auto value = GetParam<std::any>(i);
					if (value.type() == typeid(std::nullptr_t)) {
						auto value_name = GetParam<std::wstring, true>(i);
						auto [fv, fp] = ToDec<Dec>(value_name.c_str());
						auto [iv, ip] = ToInt(value_name.c_str());

						if (wcslen(ip) <= 0)
							value = iv;
						else if (wcslen(fp) <= 0)
							value = fv;
						else
							value = value_name;
					}
					to->push_back(value);
				}
			}
		};

		// 変数
		DYNAMIC_COMMAND(Variable) {
		protected:
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
						auto str = Any_Value;
						ReplaceFormat(&str);
						value = str;
					}
				}
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Variable) {}

			~Variable() noexcept {}

			void CheckParams() noexcept {
				if (MustSearch()) {
					varname = GetParam<std::wstring, true>(0);
					value = GetParam<std::any>(1);
					if (value.type() == typeid(std::nullptr_t)) {
						auto value_name = GetParam<std::wstring, true>(1);
						auto [fv, fp] = ToDec<Dec>(value_name.c_str());
						auto [iv, ip] = ToInt(value_name.c_str());
						if (wcslen(ip) <= 0)
							value = iv;
						else if (wcslen(fp) <= 0)
							value = fv;
						else {
							ReplaceFormat(&value_name);
							value = value_name;
						}
					}
				}
			}

			void Execute(const std::wstring& Var_Name) noexcept {
				Program::Instance().var_manager.MakeNew(varname) = value;
				StandardCommand::Execute();
			}

			void Execute() override {
				CheckParams();
				Program::Instance().var_manager.MakeNew(varname) = value;
				StandardCommand::Execute();
			}
		};

		// グローバル変数
		class Global final : public Variable {
		public:
			using Variable::Variable;

			void Execute() override {
				CheckParams();
				varname = varname.substr(varname.find(L'.') + 1);
				Variable::Execute(varname);
			}
		};
		
		// 変数/関数/Entityの存在確認
		DYNAMIC_COMMAND(Exist final) {
			inline static error::ErrorContent *assign_error{};
		public:
			DYNAMIC_COMMAND_CONSTRUCTOR(Exist) {
				if (assign_error == nullptr)
					assign_error = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"代入先の変数が存在しません。\n新しく個の変数を作成しますか?", MB_YESNO | MB_ICONERROR, 2);
			}

			~Exist() noexcept final {}

			void Execute() final {
				std::wstring name[2]{ {}, {} };
				for (int i = 0; i < 2; i++)
					name[i] = GetParam<std::wstring, true>(i);

				int result = 0;
				if (Program::Instance().var_manager.Get<false>(name[0]).type() != typeid(std::nullptr_t)) {
					result |= std::any_cast<int>(Program::Instance().var_manager.Get<false>(L"__変数存在"));
				}
				if (Program::Instance().event_manager.GetEvent(name[0]) != nullptr) {
					result |= std::any_cast<int>(Program::Instance().var_manager.Get<false>(L"__イベント存在"));
				}
				if (Program::Instance().entity_manager.GetEntity(name[0]) != nullptr) {
					result |= std::any_cast<int>(Program::Instance().var_manager.Get<false>(L"__キャラ存在"));
				}

				auto *v = &Program::Instance().var_manager.Get<false>(name[1]);
				if (v->type() != typeid(std::nullptr_t)) {
					*v = static_cast<int>(result);
				} else {
					event::Manager::Instance().error_handler.SendLocalError(assign_error, L"変数: " + name[1], [](const int Result) {
						switch (Result) {
							case IDYES:
							{
								auto& var = Program::Instance().var_manager.Get<false>(L"__assignable");
								auto& value = Program::Instance().var_manager.Get<false>(L"__calculated");
								Program::Instance().var_manager.MakeNew(std::any_cast<std::wstring>(var)) = std::any_cast<int>(value);
								break;
							}
							case IDNO:
								break;
							default:
								MYGAME_ASSERT(0);
						}
						Program::Instance().var_manager.Delete(L"__assignable");
						Program::Instance().var_manager.Delete(L"__calculated");
					});
					Program::Instance().var_manager.MakeNew(L"__assignable") = name[1];
					Program::Instance().var_manager.MakeNew(L"__calculated") = result;
				}
				StandardCommand::Execute();
			}
		};

		// 条件対象設定
		DYNAMIC_COMMAND(Case final) {
			std::any value;
		public:
			Case(const std::wstring& Param) noexcept {
				const auto [Value, Type] = Default_ProgramInterface.GetParamInfo(Param);
				if (Default_ProgramInterface.IsStringType(Type)) {
					value = Value;
				} else if (Default_ProgramInterface.IsNumberType(Type)) {
					value = std::stoi(Value);
				}
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Case) {}

			~Case() noexcept final {}

			void Execute() override {
				if (MustSearch()) {
					value = GetParam<std::any>(0);
				}
				Program::Instance().event_manager.NewCaseTarget(value);
				StandardCommand::Execute();
			}
		};

		// 条件式
		DYNAMIC_COMMAND(Of final) {
			std::wstring mode;
			std::any value;
		public:
			Of(const std::wstring& Condition_Sentence, const std::any& V) noexcept {
				mode = Condition_Sentence;
				value = V;
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Of) {}

			~Of() noexcept final {}

			void Execute() override {
				if (MustSearch()) {
					mode = GetParam<std::wstring, true>(0);
					auto vname = GetParam<std::wstring, true>(1);
					value = Program::Instance().var_manager.Get<false>(vname);
					if (value.type() == typeid(std::nullptr_t)) {
						auto [iv, ip] = ToInt(vname.c_str());
						auto [fv, fp] = ToDec<Dec>(vname.c_str());
						if (wcslen(ip) <= 0)
							value = iv;
						else if (wcslen(fp) <= 0)
							value = fv;
						else {
							ReplaceFormat(&vname);
							value = vname;
						}
					}
				}
				Program::Instance().event_manager.Evalute(mode, value);
				StandardCommand::Execute();
			}
		};

		// 条件終了
		DYNAMIC_COMMAND(EndCase final) {
		public:
			EndCase() noexcept {}
			~EndCase() final {}

			DYNAMIC_COMMAND_CONSTRUCTOR(EndCase) {}

			void Execute() final {
				Program::Instance().event_manager.FreeCase();
				StandardCommand::Execute();
			}
		};

		// 画像を読み込み、表示させる。
		DYNAMIC_COMMAND(Image) {
			std::shared_ptr<karapo::entity::Image> image;
			std::wstring path;
		public:
			Image(const std::wstring& P, const WorldVector WV) : Image(P, WV, WorldVector{ 0, 0 }) {}

			Image(const std::wstring& P, const WorldVector WV, const WorldVector Len) : path(P) {
				image = std::make_shared<karapo::entity::Image>(WV, Len);
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Image) {}

			~Image() override {}

			void Execute() override {
				if (MustSearch()) {
					path = GetParam<std::wstring>(0);
					auto x = GetParam<Dec>(1);
					auto y = GetParam<Dec>(2);
					auto w = GetParam<Dec>(3);
					auto h = GetParam<Dec>(4);
					image = std::make_shared<karapo::entity::Image>(WorldVector{ x, y }, WorldVector{ w, h });
				}
				image->Load(path.c_str());
				Program::Instance().entity_manager.Register(image);
				StandardCommand::Execute();
			}
		};

		// BGM
		DYNAMIC_COMMAND(Music) {
			std::shared_ptr<karapo::entity::Sound> music;
			std::wstring path;
		public:
			Music(const std::wstring & P) : Music(std::vector<std::wstring>{}) {
				path = P;
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Music) {
				music = std::make_shared<karapo::entity::Sound>(WorldVector{ 0, 0 });
			}

			~Music() override {}

			void Execute() override {
				if (MustSearch()) {
					path = GetParam<std::wstring>(0);
				}
				ReplaceFormat(&path);
				music->Load(path);
				Program::Instance().entity_manager.Register(music);
				StandardCommand::Execute();
			}
		};

		// 効果音
		DYNAMIC_COMMAND(Sound) {
			std::shared_ptr<karapo::entity::Sound> sound;
			std::wstring path;
		public:
			Sound(const std::wstring& P, const WorldVector& WV) : path(P) {
				sound = std::make_shared<karapo::entity::Sound>(WV);
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Sound) {}

			~Sound() noexcept override {}

			void Execute() override {
				if (MustSearch()) {
					path = GetParam<std::wstring>(0);
					auto x = GetParam<Dec>(1);
					auto y = GetParam<Dec>(2);
					sound = std::make_shared<karapo::entity::Sound>(WorldVector{ x, y });
				}
				ReplaceFormat(&path);
				sound->Load(path);
				Program::Instance().entity_manager.Register(sound);
				StandardCommand::Execute();
			}
		};

		// ボタン
		DYNAMIC_COMMAND(Button final) {
			std::shared_ptr<karapo::entity::Button> button;
			std::wstring path{};
		public:
			Button(const std::wstring& Name,
				const WorldVector& WV,
				const std::wstring& Image_Path,
				const WorldVector& Size) noexcept
			{
				auto name = Name;
				ReplaceFormat(&name);
				button = std::make_shared<karapo::entity::Button>(name, WV, Size);
				path = Image_Path;
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Button) {}

			~Button() noexcept final {}

			void Execute() final {
				if (MustSearch()) {
					auto name = GetParam<std::wstring>(0);
					auto x = GetParam<Dec>(1);
					auto y = GetParam<Dec>(2);
					Dec w{}, h{};
					if (GetParam<std::any>(3).type() != typeid(std::nullptr_t)) {
						w = GetParam<Dec>(3);
						h = GetParam<Dec>(4);
						path = GetParam<std::wstring>(5);
					}
					ReplaceFormat(&name);
					button = std::make_shared<karapo::entity::Button>(name, WorldVector{ x, y }, WorldVector{ w, h });
				}
				ReplaceFormat(&path);
				button->Load(path);
				Program::Instance().entity_manager.Register(button);
				StandardCommand::Execute();
			}
		};

		// 文字出力
		DYNAMIC_COMMAND(Print final) {
			std::shared_ptr<karapo::entity::Text> text;
		public:
			Print(const std::wstring& Name, const WorldVector& Pos) noexcept {
				auto name = Name;
				auto pos = Pos;
				ReplaceFormat(&name);
				text = std::make_shared<karapo::entity::Text>(name, pos);
				Program::Instance().var_manager.MakeNew(name + L".text") = std::wstring(L"");
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Print) {}

			~Print() noexcept final {}

			void Execute() final {
				if (MustSearch()) {
					auto name = GetParam<std::wstring>(0);
					auto x = GetParam<Dec>(1);
					auto y = GetParam<Dec>(2);
					ReplaceFormat(&name);

					Program::Instance().var_manager.MakeNew(name + L".text") = std::wstring(L"");
					text = std::make_shared<karapo::entity::Text>(name, WorldVector{ x, y });
				}
				Program::Instance().entity_manager.Register(text);
				StandardCommand::Execute();
			}
		};

		// 文字入力
		DYNAMIC_COMMAND(Input final) {
			size_t length = 10000;
			ScreenVector pos{ 0, 0 };
			std::any *var{};
		public:
			DYNAMIC_COMMAND_CONSTRUCTOR(Input) {}

			~Input() noexcept final {}

			void Execute()final {
				if (MustSearch()) {
					var = &Program::Instance().var_manager.Get<false>(GetParam<std::wstring, true>(0));
					pos[0] = GetParam<int>(1);
					pos[1] = GetParam<int>(2);
					length = GetParam<int>(3);
				}
				wchar_t str[10000];
				Program::Instance().engine.GetString(pos, str, length);
				*var = std::wstring(str);
				StandardCommand::Execute();
			}
		};

		namespace entity {
			// Entityの移動
			DYNAMIC_COMMAND(Teleport final) {
				std::wstring entity_name;
				WorldVector move;
			public:
				Teleport(const std::wstring& ename, const WorldVector& MV) noexcept {
					entity_name = ename;
					move = MV;
				}

				DYNAMIC_COMMAND_CONSTRUCTOR(Teleport) {}

				~Teleport() override {}

				void Execute() override {
					if (MustSearch()) {
						entity_name = GetParam<std::wstring>(0);
						auto x = GetParam<Dec>(1);
						auto y = GetParam<Dec>(2);
						move = { x, y };
					}
					ReplaceFormat(&entity_name);
					auto ent = Program::Instance().entity_manager.GetEntity(entity_name);
					ent->Teleport(move);
					StandardCommand::Execute();
				}
			};

			// Entityの削除。
			DYNAMIC_COMMAND(Kill final) {
				std::wstring entity_name;
			public:
				Kill(const std::wstring& ename) noexcept {
					entity_name = ename;
				}

				DYNAMIC_COMMAND_CONSTRUCTOR(Kill) {}

				~Kill() override {}

				void Execute() override {
					if (MustSearch()) {
						entity_name = GetParam<std::wstring>(0);
					}

					ReplaceFormat(&entity_name);
					if (entity_name == L"__all" || entity_name == L"__全員") {
						std::vector<std::wstring> names{};
						auto sen = std::any_cast<std::wstring>(Program::Instance().var_manager.Get<false>(variable::Managing_Entity_Name));
						{
							std::wstring temp{};
							for (auto ch : sen) {
								if (ch != L'\n') {
									temp += ch;
								} else {
									names.push_back(temp);
									temp.clear();
								}
							}
						}

						for (const auto& name : names) {
							Program::Instance().entity_manager.Kill(name.substr(0, name.find(L'=')));
						}
						Program::Instance().entity_manager.Register(std::make_shared<karapo::entity::Mouse>());
					} else {
						Program::Instance().entity_manager.Kill(entity_name);
					}
					StandardCommand::Execute();
				}
			};
		}

		// キー入力毎のコマンド割当
		class Bind final : public StandardCommand {
			std::wstring key_name{}, command_sentence{};
		public:
			Bind(const std::wstring& Key_Name, const std::wstring& Command_Sentence) noexcept {
				key_name = Key_Name;
				command_sentence = Command_Sentence;
			}

			void Execute() override;
		};

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

		DYNAMIC_COMMAND(Filter final) {
			int potency{};
			std::wstring layer_name{}, kind_name{};
		public:
			Filter(const std::wstring& N, const std::wstring& KN, const int P) noexcept {
				layer_name = N;
				kind_name = KN;
				potency = P % 256;
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Filter) {}

			~Filter() final {}

			void Execute() final {
				if (MustSearch()) {
					layer_name = GetParam<std::wstring>(0);
					kind_name = GetParam<std::wstring>(1);
					potency = GetParam<int>(2);
				}
				ReplaceFormat(&layer_name);
				ReplaceFormat(&kind_name);
				Program::Instance().canvas.ApplyFilter(layer_name, kind_name, potency);
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
		DYNAMIC_COMMAND(Call final) {
			std::wstring event_name;
		public:
			DYNAMIC_COMMAND_CONSTRUCTOR(Call) {}

			~Call() noexcept final {}

			void Execute() override {
				std::vector<std::any> params{};
				SetAllParams(&params);
				if (MustSearch()) {
					event_name = std::any_cast<std::wstring>(params[0]);
				}
				ReplaceFormat(&event_name);
				Event* e = Program::Instance().event_manager.GetEvent(event_name);
				if (!params.empty()) {
					for (int i = 0; i < e->param_names.size(); i++) {
						auto value = params[i + 1];
						auto& newvar = Program::Instance().var_manager.MakeNew(event_name + std::wstring(L".") + e->param_names[i]);
						if (value.type() == typeid(int))
							newvar = std::any_cast<int>(value);
						else if (value.type() == typeid(Dec))
							newvar = std::any_cast<Dec>(value);
						else if (value.type() == typeid(std::wstring))
							newvar = std::any_cast<std::wstring>(value);
					}
				}
				Program::Instance().event_manager.Call(event_name);
				StandardCommand::Execute();
			}
		};

		// イベント追加読込
		DYNAMIC_COMMAND(Import final) {
			std::wstring file_name;
		public:
			Import(const std::wstring& File_Name) noexcept {
				file_name = File_Name;
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Import) {}

			~Import() noexcept final {}

			void Execute() override {
				if (MustSearch()) {
					file_name = GetParam<std::wstring>(0);
				}
				ReplaceFormat(&file_name);
				Program::Instance().event_manager.ImportEvent(file_name);
				StandardCommand::Execute();
			}
		};

		// イベント読込
		DYNAMIC_COMMAND(Load final) {
			std::wstring file_name;
		public:
			Load(const std::wstring & File_Name) noexcept {
				file_name = File_Name;
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Load) {}

			~Load() noexcept final {}

			void Execute() override {
				if (MustSearch()) {
					file_name = GetParam<std::wstring>(0);
				}
				ReplaceFormat(&file_name);
				Program::Instance().event_manager.RequestEvent(file_name);
				StandardCommand::Execute();
			}
		};

		namespace layer {
			// レイヤー生成(指定位置)
			DYNAMIC_COMMAND(Make final) {
				std::wstring kind_name{}, layer_name{};
				int index = 0;
				inline static const std::unordered_map<std::wstring, bool (Canvas::*)(const std::wstring&, const int)> Create{
					// 相対位置レイヤー
					{ L"scroll",  &Canvas::CreateRelativeLayer },
					{ L"スクロール",  &Canvas::CreateRelativeLayer },
					{ L"relative",  &Canvas::CreateRelativeLayer },
					{ L"相対位置",  &Canvas::CreateRelativeLayer },
					// 絶対位置レイヤー
					{ L"fixed",  &Canvas::CreateAbsoluteLayer },
					{ L"固定",  &Canvas::CreateAbsoluteLayer },
					{ L"absolute",  &Canvas::CreateAbsoluteLayer },
					{ L"絶対位置",  &Canvas::CreateAbsoluteLayer }
				};
			public:
				Make(const int Index, const std::wstring & KN, const std::wstring & LN) noexcept : Make(std::vector<std::wstring>{}) {
					index = Index;
					kind_name = KN;
					layer_name = LN;
				}

				DYNAMIC_COMMAND_CONSTRUCTOR(Make) {}

				~Make() final {}

				void Execute() final {
					if (MustSearch()) {
						index = GetParam<int>(0);
						kind_name = GetParam<std::wstring>(1);
						layer_name = GetParam<std::wstring>(2);
					}
					ReplaceFormat(&kind_name);
					ReplaceFormat(&layer_name);
					auto it = Create.find(kind_name);
					if (it != Create.end()) {
						(Program::Instance().canvas.*it->second)(layer_name, index);
					}
					StandardCommand::Execute();
				}
			};

			// レイヤー変更
			DYNAMIC_COMMAND(Select final) {
				std::wstring layer_name{};
			public:
				Select(const std::wstring& LN) noexcept {
					layer_name = LN;
				}

				DYNAMIC_COMMAND_CONSTRUCTOR(Select) {}

				~Select() final {}

				void Execute() final {
					if (MustSearch()) {
						layer_name = GetParam<std::wstring>(0);
					}
					ReplaceFormat(&layer_name);
					Program::Instance().canvas.SelectLayer(layer_name);
					StandardCommand::Execute();
				}
			};

			// 相対位置レイヤーの基準設定
			DYNAMIC_COMMAND(SetBasis final) {
				std::wstring entity_name{}, layer_name{};
			public:
				SetBasis(const std::wstring& EN, const std::wstring& LN) noexcept {
					entity_name = EN;
					layer_name = LN;
				}

				DYNAMIC_COMMAND_CONSTRUCTOR(SetBasis) {}

				~SetBasis() final {}

				void Execute() final {
					if (MustSearch()) {
						entity_name = GetParam<std::wstring>(0);
						layer_name = GetParam<std::wstring>(1);
					}
					ReplaceFormat(&entity_name);
					ReplaceFormat(&layer_name);
					auto ent = Program::Instance().entity_manager.GetEntity(entity_name);
					Program::Instance().canvas.SetBasis(ent, layer_name);
					StandardCommand::Execute();
				}
			};

			// レイヤー削除
			DYNAMIC_COMMAND(Delete final) {
				std::wstring name{};
			public:
				Delete(const std::wstring& N) noexcept {
					name = N;
				}

				DYNAMIC_COMMAND_CONSTRUCTOR(Delete) {}

				~Delete() final {}

				void Execute() final {
					if (MustSearch()) {
						name = GetParam<std::wstring>(0);
					}
					ReplaceFormat(&name);
					if (name == L"__all" || name == L"__全部") {
						for (int i = 0; Program::Instance().canvas.DeleteLayer(i););
						Program::Instance().canvas.CreateAbsoluteLayer(L"デフォルトレイヤー");
						Program::Instance().canvas.SelectLayer(L"デフォルトレイヤー");
					} else {
						Program::Instance().canvas.DeleteLayer(name);
					}
					StandardCommand::Execute();
				}
			};

			DYNAMIC_COMMAND(Show final) {
				std::wstring name{};
			public:
				Show(const std::wstring &N) noexcept {
					name = N;
				}

				DYNAMIC_COMMAND_CONSTRUCTOR(Show) {}

				~Show() final {}

				void Execute() final {
					if (MustSearch()) {
						name = GetParam<std::wstring>(0);
					}
					ReplaceFormat(&name);
					Program::Instance().canvas.Show(name);
					StandardCommand::Execute();
				}
			};

			DYNAMIC_COMMAND(Hide final) {
				std::wstring name{};
			public:
				Hide(const std::wstring &N) noexcept {
					name = N;
				}

				DYNAMIC_COMMAND_CONSTRUCTOR(Hide) {}

				~Hide() final {}

				void Execute() final {
					if (MustSearch()) {
						name = GetParam<std::wstring>(0);
					}
					ReplaceFormat(&name);
					Program::Instance().canvas.Hide(name);
					StandardCommand::Execute();
				}
			};
		}

		namespace math {
#define MATH_COMMAND_CALCULATE(OPERATION) \
	if (Is_Only_Int) { \
		cal.i = std::any_cast<int>(value[0]) OPERATION std::any_cast<int>(value[1]); \
	} else { \
		cal.d = 0.0; \
		if (value[0].type() == typeid(int)) { \
			cal.d = std::any_cast<int>(value[0]); \
		} else { \
			cal.d = std::any_cast<Dec>(value[0]); \
		} \
 \
		if (value[1].type() == typeid(int)) { \
			\
			cal.d OPERATION= std::any_cast<int>(value[1]); \
		} else { \
			cal.d OPERATION= std::any_cast<Dec>(value[1]); \
		} \
	}

			DYNAMIC_COMMAND(MathCommand) {
			protected:
				inline static error::ErrorClass* operation_error_class{};
				inline static error::ErrorContent* assign_error{};

				static void Reassign(const int Result) {
					switch (Result) {
						case IDYES:
						{
							auto& var = Program::Instance().var_manager.Get<false>(L"__assignable");
							auto& value = Program::Instance().var_manager.Get<false>(L"__calculated");
							if (value.type() == typeid(Dec))
								Program::Instance().var_manager.MakeNew(std::any_cast<std::wstring>(var)) = std::any_cast<Dec>(value);
							else if (value.type() == typeid(int))
								Program::Instance().var_manager.MakeNew(std::any_cast<std::wstring>(var)) = std::any_cast<int>(value);
							else if (value.type() == typeid(std::wstring))
								Program::Instance().var_manager.MakeNew(std::any_cast<std::wstring>(var)) = std::any_cast<std::wstring>(value);
							break;
						}
						case IDNO:
							break;
						default:
							MYGAME_ASSERT(0);
					}
					Program::Instance().var_manager.Delete(L"__assignable");
					Program::Instance().var_manager.Delete(L"__calculated");
				}

				union CalculateValue {
					int i;
					Dec d;
				};

				void SendAssignError(const std::any& Value) {
					event::Manager::Instance().error_handler.SendLocalError(assign_error, (L"変数: " + var_name).c_str(), &MathCommand::Reassign);
					Program::Instance().var_manager.MakeNew(L"__assignable") = var_name;
					if (Value.type() == typeid(int))
						Program::Instance().var_manager.MakeNew(L"__calculated") = std::any_cast<int>(Value);
					else if (Value.type() == typeid(Dec))
						Program::Instance().var_manager.MakeNew(L"__calculated") = std::any_cast<Dec>(Value);
					else if (Value.type() == typeid(std::wstring))
						Program::Instance().var_manager.MakeNew(L"__calculated") = std::any_cast<std::wstring>(Value);

				}

				void SendAssignError(const bool Is_Only_Int, const CalculateValue Cal_Value) {
					event::Manager::Instance().error_handler.SendLocalError(assign_error, (L"変数: " + var_name).c_str(), &MathCommand::Reassign);
					Program::Instance().var_manager.MakeNew(L"__assignable") = var_name;
					Program::Instance().var_manager.MakeNew(L"__calculated") = (Is_Only_Int ? Cal_Value.i : Cal_Value.d);
				}
			public:
				MathCommand(const std::vector<std::wstring>& Params) : DynamicCommand(Params) {
					if (operation_error_class == nullptr)
						operation_error_class = error::UserErrorHandler::MakeErrorClass(L"演算エラー");
					if (assign_error == nullptr)
						assign_error = error::UserErrorHandler::MakeError(operation_error_class, L"代入先の変数が存在しません。\n新しくこの変数を作成しますか?", MB_YESNO | MB_ICONERROR, 2);
				}

				std::wstring var_name{};
				std::any value[2]{};
				void Extract(const int Length) {
					if (MustSearch()) {
						var_name = GetParam<std::wstring, true>(0);
						for (int i = 0; i < Length; i++) {
							value[i] = GetParam<std::any>(i + 1);
							if (value[i].type() == typeid(int))
								value[i] = std::any_cast<int>(value[i]);
							else if (value[i].type() == typeid(Dec))
								value[i] = std::any_cast<Dec>(value[i]);
							else {
								value[i] = GetParam<std::wstring, true>(i + 1);
								auto tmp = std::any_cast<std::wstring>(value[i]);
								auto [iv, ip] = ToInt(tmp.c_str());
								auto [fv, fp] = ToDec<Dec>(tmp.c_str());
								if (wcslen(ip) <= 0)
									value[i] = iv;
								else
									value[i] = fv;
							}
						}
					}
				}
			};

			class Assign final : public MathCommand {
			public:
				using MathCommand::MathCommand;
				~Assign() final {}

				void Execute() final {
					Extract(1);
					auto var_name = GetParam<std::wstring, true>(0);

					auto* v = &Program::Instance().var_manager.Get<false>(var_name);
					if (v->type() != typeid(std::nullptr_t)) {
						if (value[0].type() == typeid(int))
							*v = std::any_cast<int>(value[0]);
						else if (value[0].type() == typeid(Dec))
							*v = std::any_cast<Dec>(value[0]);
						else if (value[0].type() == typeid(std::wstring))
							*v = std::any_cast<std::wstring>(value[0]);
					} else {
						SendAssignError(value[0]);
					}
					StandardCommand::Execute();
				}
			};

			// 加算
			class Sum final : public MathCommand {
			public:
				using MathCommand::MathCommand;

				~Sum() final {}

				void Execute() final {
					Extract(2);
					const bool Is_Only_Int = (value[0].type() == typeid(int) && value[1].type() == typeid(int));
					
					CalculateValue cal;
					MATH_COMMAND_CALCULATE(+);

					auto* v = &Program::Instance().var_manager.Get<false>(var_name);
					if (v->type() != typeid(std::nullptr_t)) {
						*v = (Is_Only_Int ? cal.i : cal.d);
					} else {
						SendAssignError(Is_Only_Int, cal);
					}
					StandardCommand::Execute();
				}
			};

			// 減算
			class Sub final : public MathCommand {
			public:
				using MathCommand::MathCommand;
				~Sub() final {}

				void Execute() final {
					Extract(2);
					const bool Is_Only_Int = (value[0].type() == typeid(int) && value[1].type() == typeid(int));

					CalculateValue cal;
					MATH_COMMAND_CALCULATE(-);

					auto* v = &Program::Instance().var_manager.Get<false>(var_name);
					if (v->type() != typeid(std::nullptr_t)) {
						*v = (Is_Only_Int ? cal.i : cal.d);
					} else {
						SendAssignError(Is_Only_Int, cal);
					}
					StandardCommand::Execute();
				}
			};

			// 乗算
			class Mul final : public MathCommand {
			public:
				using MathCommand::MathCommand;
				~Mul() final {}

				void Execute() final {
					Extract(2);
					const bool Is_Only_Int = (value[0].type() == typeid(int) && value[1].type() == typeid(int));

					CalculateValue cal;
					MATH_COMMAND_CALCULATE(*);

					auto *v = &Program::Instance().var_manager.Get<false>(var_name);
					if (v->type() != typeid(std::nullptr_t)) {
						*v = (Is_Only_Int ? cal.i : cal.d);
					} else {
						SendAssignError(Is_Only_Int, cal);
					}
					StandardCommand::Execute();
				}
			};

			// 徐算
			class Div final : public MathCommand {
			public:
				using MathCommand::MathCommand;
				~Div() final {}

				void Execute() final {
					Extract(2);
					const bool Is_Only_Int = (value[0].type() == typeid(int) && value[1].type() == typeid(int));

					CalculateValue cal;
					MATH_COMMAND_CALCULATE(/);

					auto* v = &Program::Instance().var_manager.Get<false>(var_name);
					if (v->type() != typeid(std::nullptr_t)) {
						*v = (Is_Only_Int ? cal.i : cal.d);
					} else {
						SendAssignError(Is_Only_Int, cal);
					}
					StandardCommand::Execute();
				}
			};

			class BitCommand : public MathCommand {
			protected:
				inline static error::ErrorClass* logic_operation_error_class{};
				inline static error::ErrorContent* not_integer_error{};

				auto AddTypeName(std::wstring* extra_message, const int Index) noexcept {
					auto source_name = value[Index].type().name();
					auto converted_name = new(std::nothrow) wchar_t[strlen(source_name) + 1]{};
					if (converted_name != nullptr) {
						mbstowcs(converted_name, source_name, strlen(source_name) + 1);
						*extra_message += GetParam<std::wstring, true>(Index) + std::wstring(L": ") + converted_name + L'\n';
						delete[] converted_name;
					}
				}

				std::pair<bool, bool> CheckValueType() const noexcept {
					const bool Is_First_Int = (value[0].type() == typeid(int));
					const bool Is_Second_Int = (value[1].type() == typeid(int));
					return { Is_First_Int, Is_Second_Int };
				}

				void SendBitLogicError(std::wstring extra_message, const std::pair<bool, bool>& Is_Int) {
					if (!Is_Int.first)
						AddTypeName(&extra_message, 0);
					if (!Is_Int.second)
						AddTypeName(&extra_message, 1);
					event::Manager::Instance().error_handler.SendLocalError(not_integer_error, extra_message);
				}
			public:
				BitCommand(const std::vector<std::wstring>& Params) : MathCommand(Params) {
					if (logic_operation_error_class == nullptr)
						logic_operation_error_class = error::UserErrorHandler::MakeErrorClass(L"論理演算エラー");
					if (not_integer_error == nullptr)
						not_integer_error = error::UserErrorHandler::MakeError(logic_operation_error_class, L"ビット演算に用いる変数の値が整数値ではありません。", MB_OK | MB_ICONERROR, 2);
				}

				~BitCommand() override {}
			};

			// ビット論理和
			class Or final : public BitCommand {
			public:
				using BitCommand::BitCommand;
				~Or() final {}

				void Execute() final {
					Extract(2);
					const auto Is_Int = CheckValueType();

					if (Is_Int.first && Is_Int.second) {
						CalculateValue cal;
						cal.i = std::any_cast<int>(value[0]) | std::any_cast<int>(value[1]);
						auto* v = &Program::Instance().var_manager.Get<false>(var_name);
						if (v->type() != typeid(std::nullptr_t)) {
							*v = cal.i;
						} else {
							SendAssignError(true, cal);
						}
					} else {
						SendBitLogicError(L"演算: ビット論理和\n", Is_Int);
					}
					StandardCommand::Execute();
				}
			};

			// ビット論理積
			class And final : public BitCommand {
			public:
				using BitCommand::BitCommand;
				~And() final {}

				void Execute() final {
					Extract(2);
					const auto Is_Int = CheckValueType();
					if (Is_Int.first && Is_Int.second) {
						CalculateValue cal;
						cal.i = std::any_cast<int>(value[0]) & std::any_cast<int>(value[1]);
						auto* v = &Program::Instance().var_manager.Get<false>(var_name);
						if (v->type() != typeid(std::nullptr_t)) {
							*v = cal.i;
						} else {
							SendAssignError(true, cal);
						}
					} else {
						SendBitLogicError(L"演算: ビット論理積\n", Is_Int);
					}
					StandardCommand::Execute();
				}
			};

			// ビット排他的論理和
			class Xor final : public BitCommand {
			public:
				using BitCommand::BitCommand;
				~Xor() final {}

				void Execute() final {
					Extract(2);
					const auto Is_Int = CheckValueType();

					if (Is_Int.first && Is_Int.second) {
						CalculateValue cal;
						cal.i = std::any_cast<int>(value[0]) ^ std::any_cast<int>(value[1]);
						auto* v = &Program::Instance().var_manager.Get<false>(var_name);
						if (v->type() != typeid(std::nullptr_t)) {
							*v = cal.i;
						} else {
							SendAssignError(true, cal);
						}
					} else {
						SendBitLogicError(L"演算: ビット排他的論理和\n", Is_Int);
					}
					StandardCommand::Execute();
				}
			};

			// ビット論理否定
			class Not final : public BitCommand {
			public:
				using BitCommand::BitCommand;
				~Not() final {}

				void Execute() final {
					Extract(1);
					std::wstring var_name{};
					var_name = GetParam<std::wstring, true>(0);
				
					if (value[0].type() == typeid(int)) {
						auto* v = &Program::Instance().var_manager.Get<false>(var_name);
						CalculateValue cal;
						cal.i = ~std::any_cast<int>(value[0]);
						if (v->type() != typeid(std::nullptr_t)) {
							*v = cal.i;
						} else {
							SendAssignError(true, cal);
						}
					} else {
						SendBitLogicError(L"演算: ビット論理否定\n", { false, true });
					}
					StandardCommand::Execute();
				}
			};
		}

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

			// Entityが関わる全てのManagerを更新
			class UpdateEntity final : public StandardCommand {
			public:
				UpdateEntity() noexcept {}
				~UpdateEntity() noexcept final {}

				void Execute() final {
					Program::Instance().entity_manager.Update();
					Program::Instance().canvas.Update();

					StandardCommand::Execute();
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

			if (!cmd->IsUnnecessary() && (cmd->IgnoreCondition() || Program::Instance().event_manager.CanOfExecute())) {
				cmd->Execute();
				return std::move(cmd);
			} else {
				cmd->Reset();
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
				else {
					// 再格納する。
					commands.push_front(std::move(recycled));
				}
				event::Manager::Instance().error_handler.ShowLocalError(4);
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
						context->push_back(*text);
						text->clear();
					};

					{
						// 読み込もうとした単語が、文字列として書かれているかを判定する為の変数
						// 値がtrueの間、解析器はスペースを区切り文字として認識しなくなる。
						bool is_string = false;
						bool need_skip = false;
						auto text = std::wstring(L"");

						for (auto c : Sentence) {
							if (!is_string) {
								if (auto it = context.cend(); context.size() >= 2 && *(--it) == L"/" && *(--it) == L"/") {
									for (int i = 0; i < 2; i++)
										context.pop_back();
									need_skip = true;
								}

								if (!need_skip) {
									// スペース判定
									if (IsSpace(c) || c == L'\0' || c == L'\n') {
										// 貯めこんだ文字を単語として格納
										if (!text.empty()) {
											PushWord(&context, &text);
										}

										if (c == L'\n') {
											text = L'\n';
											PushWord(&context, &text);
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
										case '/':
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
									if (c == L'\n') {
										need_skip = false;
									}
									continue;
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

					{
						// 文字の結合部
						// >、=、<をそれぞれ結合する。

						std::wstring tmp{};
						Context::iterator it = context.begin();
						while (it != context.end()) {
							it = std::find_if(it, context.end(), [](std::wstring text) {
								return text == L"<" || text == L"=" || text == L">";
							});

							if (it != context.end()) {
								tmp += *it;
								if (tmp.size() == 2) {
									if (tmp == L"<=" || tmp == L"==" || tmp == L">=") {
										// 結合
										context.erase(it--);
										context.insert(it, tmp);
										context.erase(it++);
									} else if (tmp == L"<>" || tmp == L"=>" || tmp == L"=<") {
										// 扱う演算子ではないので一時保存用を末梢。
										tmp.clear();
									}
								}
								it++;
							}
						}
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
						if (iswdigit(word[0]) || word[0] == L'-') {
							word += std::wstring(L":") + innertype::Number;
						} else if (word[0] == L'\'') {
							word += std::wstring(L":") + innertype::String;
							for (size_t pos = word.find(L'\''); pos != std::wstring::npos; pos = word.find(L'\'')) {
								word.erase(pos, 1);
							}
						}
						compiled.push_back(std::move(word));
						context.pop_front();
					}
				}

				auto Result() noexcept {
					return std::move(compiled);
				}
			};

			// SCからECまでの範囲の文章を解析する解析器。
			// それ以外には、解析中かどうかを表現するための関数群を持つ。
			template<wchar_t SC, wchar_t EC>
			class RangeParser {
				bool parsing = false;
				error::ErrorContent* abort_error{};
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

				void AbortParsing(error::ErrorContent* error) noexcept {
					abort_error = error;
				}
			public:
				static constexpr bool IsValidToStart(const wchar_t Ch) noexcept { return SC == Ch; }
				static constexpr bool IsValidToEnd(const wchar_t Ch) noexcept { return EC == Ch; }

				~RangeParser() noexcept {
					MYGAME_ASSERT(!parsing);
				}

				error::ErrorContent* AbortedReason() const noexcept {
					return abort_error;
				}
			};

			// イベント情報を解析器。
			// 特にイベント名、発生タイプ、イベント位置を解析し、
			// 結果を適切な型に変換して排出する。
			class InformationParser final {
				// イベント名の解析と変換を行うクラス 
				class NameParser final : public RangeParser<L'[', L']'> {
					std::wstring name;
					inline static error::ErrorContent *empty_name_error{};
				public:
					NameParser() noexcept {
						if (empty_name_error == nullptr)
							empty_name_error = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"イベント名を空にすることはできません。", MB_OK | MB_ICONERROR, 1);
					}

					[[nodiscard]] auto Interpret(const std::wstring& Sentence) noexcept {
						if (Sentence.size() == 1) {
							if (IsValidToStart(Sentence[0])) {
								StartParsing();
								return false;
							} else if (IsValidToEnd(Sentence[0])) {
								if (name.empty()) {
									AbortParsing(empty_name_error);
									EndParsing();
									return false;
								} else {
									EndParsing();
									return true;
								}
							}
						}

						if (IsParsing()) {
							name = Sentence;
						}
						return false;
					}

					auto Result() const noexcept {
						return name;
					}
				} nparser;

				// イベント発生タイプ及び位置の解析と変換を行うクラス
				class ConditionParser final : public RangeParser<L'<', L'>'> {
					const std::unordered_map<std::wstring, TriggerType> Trigger_Map{
						{ L"n", TriggerType::None },
						{ L"t", TriggerType::Trigger },
						{ L"a", TriggerType::Auto },
						{ L"b", TriggerType::Button },
						{ L"l", TriggerType::Load }
					};

					// 発生タイプ
					TriggerType tt = TriggerType::Invalid;

					// 最小・最大X座標
					Dec minx{}, maxx{};
					// 最小・最大Y座標
					Dec miny{}, maxy{};
					// 解析結果格納用ポインタ
					Dec *current = &minx;

					inline static error::ErrorContent* not_condition_warning{};
					inline static error::ErrorContent* too_many_positions_error{};
				public:
					ConditionParser() noexcept {
						if (not_condition_warning == nullptr) {
							not_condition_warning = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"指定されたイベント発生タイプが存在しなかった為、\n発生タイプ無し(n)として設定しました。", MB_OK | MB_ICONWARNING, 2);
						}

						if (too_many_positions_error == nullptr) {
							too_many_positions_error = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"指定座標が多すぎます。", MB_OK | MB_ICONERROR, 1);
						}
					}

					[[nodiscard]] auto Interpret(const std::wstring& Sentence) noexcept {
						if (AbortedReason() != nullptr) {
							if (IsParsing())
								EndParsing();
							return false;
						}

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
							if (current < &minx || current > &maxy) {
								AbortParsing(too_many_positions_error);
							} else {
								if (tt == TriggerType::Invalid) {
									// 発生タイプ解析
									auto it = Trigger_Map.find(Sentence);
									if (it != Trigger_Map.end()) {
										tt = it->second;
									} else {
										const auto Event_Name = std::any_cast<std::wstring>(Program::Instance().var_manager.Get<false>(L"__生成中イベント"));

										auto sub_message = L"該当イベント: " + Event_Name + L'\n';
										sub_message += L"指定された発生タイプ: " + Sentence;
										event::Manager::Instance().error_handler.SendLocalError(not_condition_warning, sub_message);
										tt = TriggerType::None;
									}
								} else if (tt != TriggerType::None && tt != TriggerType::Auto && tt != TriggerType::Load) {
									if (Sentence == L"~") {
										if (current + 1 <= &maxy)
											current++;
									} else if (Sentence == L",") {
										current = &miny;
									}
									*current = ToDec<Dec>(Sentence.c_str(), nullptr);
								}
							}
						}
						return false;
					}

					auto Result() const noexcept {
						return std::tuple<TriggerType, WorldVector, WorldVector>{ tt, WorldVector{ minx, miny }, WorldVector{ maxx, maxy }};
					}
				} pparser;

				std::wstring event_name;
				TriggerType trigger;
				WorldVector min_origin, max_origin;
				std::valarray<bool> enough_result{ false, false };

				auto Interpret(Context *context) noexcept {
					if (std::count(std::begin(enough_result), std::end(enough_result), true) == enough_result.size())
						return;

					// ここからcontextの解析が開始される。
					// 基本的に、全ての解析器が一つのcontextを解析する。
					// 

					auto& Sentence = context->front();
					std::valarray<bool> result{ false, false };	// 各内容の解析結果

					if (!enough_result[0] && nparser.Interpret(Sentence)) {
						event_name = nparser.Result();
						result[0] = true;
					}

					if (!enough_result[1] && pparser.Interpret(Sentence)) {
						auto [trigger_type, min, max] = pparser.Result();
						trigger = trigger_type;
						if (min[0] > max[0])
							std::swap(min[0], max[0]);
						if (min[1] > max[1])
							std::swap(min[1], max[1]);

						min_origin = min;
						max_origin = max;
						result[1] = true;
					}

					if (std::count(std::begin(result), std::end(result), true) > 1) {
						// 一行の解析で2つ以上の結果が得られた場合は不正とする。
						MYGAME_ASSERT(0);
					}

					enough_result |= result;

					context->pop_front();
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

			// イベントの引数を解析するクラス
			class ParamParser final : public RangeParser<L'(', L')'> {
				std::vector<std::wstring> param_names{};
				error::ErrorContent* empty_param_name_error{};
			public:
				ParamParser() {
					if (empty_param_name_error == nullptr)
						empty_param_name_error = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"引数名を空にすることはできません。", MB_OK | MB_ICONERROR, 1);
				}

				auto Interpret(Context *const context) noexcept {
					if (AbortedReason() != nullptr) {
						if (IsParsing()) {
							while (!IsValidToEnd(context->front()[0])) {
								context->pop_front();
							}
							EndParsing();
						}
						return;
					}

					const auto Sentence = context->front();
					if (IsParsing()) {
						if (IsValidToEnd(Sentence[0])) {
							if (param_names[0] == L"") {
								param_names.clear();
							}
							EndParsing();
							return;
						} else {
							if (Sentence != L",") {
								param_names.back() = Sentence;
							} else {
								if (!param_names.empty() && param_names.back().empty()) {
									AbortParsing(empty_param_name_error);
								} else
									param_names.push_back({});
							}
						}
					} else {
						if (IsValidToStart(Sentence[0])) {
							StartParsing();
							param_names.push_back({});
						}
					}
					context->pop_front();
					Interpret(context);
				}

				ParamParser(Context *context) {
					Interpret(context);
				}

				auto Result() const noexcept {
					return param_names;
				}
			};

			std::unordered_map<std::wstring, Event> events;
			std::wstring ename;

			error::ErrorContent *parser_abortion_error{};
		public:
			// コマンド解析クラス
			// 単語に基づくコマンドの生成と、それの結果を排出する。
			class CommandParser final : public RangeParser<L'{', L'}'> {
				// 予約語
				std::unordered_map<std::wstring, GenerateFunc> words;

				std::vector<std::wstring> parameters;	// 引数
				Event::Commands commands;	// 
				GenerateFunc liketoRun;				// 生成関数を実行するための関数ポインタ。
				std::wstring generating_command_name = L"";
				bool request_abort = false;
				bool finish_sentence = false;

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
#if 0
						// 変数探し
						auto& value = Program::Instance().var_manager.Get<true>(text);
						if (value.type() == typeid(int))
							parameters.push_back(std::to_wstring(std::any_cast<int>(value)) + std::wstring(L":") + innertype::Number);
						else if (value.type() == typeid(Dec))
							parameters.push_back(std::to_wstring(std::any_cast<Dec>(value)) + std::wstring(L":") + innertype::Number);
						else
							parameters.push_back(std::any_cast<std::wstring>(value) + std::wstring(L":") + innertype::String);
#else
						const auto Event_Name = std::any_cast<std::wstring>(Program::Instance().var_manager.Get<false>(L"__生成中イベント"));
						parameters.push_back(Event_Name + L'.' + text + std::wstring(L":") + innertype::Undecided);
#endif
					}
				}

				bool CompileCommand() {
					if (liketoRun) {
						auto&& f = liketoRun(parameters);
						auto Param_Enough = f.checkParamState();
						switch (Param_Enough) {
							case KeywordInfo::ParamResult::Lack:
							{
								return false;
							}
							case KeywordInfo::ParamResult::Medium:
							{
								if (!finish_sentence)
									return false;
							}
							case KeywordInfo::ParamResult::Maximum:
							case KeywordInfo::ParamResult::Excess:
							{
								auto&& result = f.Result();
								if (result != nullptr) {
									// 引数が十分に積まれている時:
									if (f.is_dynamic) {
										// 動的コマンドはイベントのコマンドに追加。
										commands.push_back(std::move(result));
										if (generating_command_name == L"of") {
											auto endof = words.at(L"__endof")({}).Result();
											commands.insert(commands.end() - 1, std::move(endof));
										} else if (generating_command_name == L"kill") {
											auto force = words.at(L"__entity強制更新")({}).Result();
											commands.push_back(std::move(force));
										}
									} else if (f.is_static) {
										// 静的コマンドは即実行。
										result->Execute();
									}
								}
								liketoRun = nullptr;
								parameters.clear();
								return true;
							}
						}
					}
					return false;
				}

				void Interpret(Context *context) noexcept {
					auto text = context->front();
					bool compiled = false;
					if (IsParsing()) {
						// HACK: CheckCommandWordとCheckArgsを含め、「'」等の引数に含まれない文字に対する処理を改善する。
						finish_sentence = (text == L"\n");
						if (!finish_sentence) {
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
										const bool Is_Variable = (generating_command_name == L"var" || 
											generating_command_name == L"変数" || 
											generating_command_name == L"global" || 
											generating_command_name == L"大域");
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
							}
						}

						if (!request_abort) {
							compiled = CompileCommand();
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
							context->pop_front();
							return;
						} else if (finish_sentence) {
							liketoRun = nullptr;
							parameters.clear();
						}
					}
					context->pop_front();
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

					words[L"text"] =
						words[L"文章"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
					{
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Name, Name_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								const auto [X, X_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
								const auto [Y, Y_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
								if (Default_ProgramInterface.IsStringType(Name_Type) &&
									Default_ProgramInterface.IsStringType(X_Type) &&
									Default_ProgramInterface.IsStringType(Y_Type)) 
								{
									auto [xv, xp] = ToDec<Dec>(X.c_str());
									auto [yv, yp] = ToDec<Dec>(Y.c_str());
									return std::make_unique<command::Print>(Name, WorldVector{ xv, yv });
								}
								else
									return std::make_unique<command::Print>(params);
							},
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
									case 1:
									case 2:
										return KeywordInfo::ParamResult::Lack;
									case 3:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"music"] =
						words[L"音楽"] =
						words[L"BGM"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
					{
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Var, Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								if (Default_ProgramInterface.IsStringType(Type))
									return std::make_unique<command::Music>(Var);
								else 
									return std::make_unique<command::Music>(params);
							},
							.checkParamState  = [params]() -> KeywordInfo::ParamResult { 
								switch (params.size()) {
									case 1:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Lack;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"sound"] =
						words[L"効果音"] =
						words[L"音"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
					{
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [File_Path, Path_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								const auto [Vec_X, X_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
								const auto [Vec_Y, Y_Type] = Default_ProgramInterface.GetParamInfo(params[2]);
								if (Default_ProgramInterface.IsStringType(Path_Type) && Default_ProgramInterface.IsNumberType(X_Type) && Default_ProgramInterface.IsNumberType(Y_Type)) {
									return std::make_unique<command::Sound>(File_Path, WorldVector{ ToDec<Dec>(Vec_X.c_str(), nullptr), ToDec<Dec>(Vec_Y.c_str(), nullptr) });
								} else {
									return std::make_unique<command::Sound>(params);
								}
							},
							.checkParamState  = [params]() -> KeywordInfo::ParamResult { 
								switch (params.size()) {
									case 0:
									case 1:
									case 2:
										return KeywordInfo::ParamResult::Lack;
									case 3:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
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
								const auto [File_Path, Path_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								const auto [Vec_X, VX_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
								const auto [Vec_Y, VY_Type] = Default_ProgramInterface.GetParamInfo(params[2]);
								const auto [Len_X, LX_Type] = Default_ProgramInterface.GetParamInfo(params[3]);
								const auto [Len_Y, LY_Type] = Default_ProgramInterface.GetParamInfo(params[4]);
	
								if (Default_ProgramInterface.IsStringType(Path_Type) && 
									Default_ProgramInterface.IsNumberType(VX_Type) && 
									Default_ProgramInterface.IsNumberType(VY_Type) &&
									Default_ProgramInterface.IsNumberType(LX_Type) &&
									Default_ProgramInterface.IsNumberType(LY_Type))
								{
									return std::make_unique<command::Image>(File_Path, 
										WorldVector{ ToDec<Dec>(Vec_X.c_str(), nullptr), ToDec<Dec>(Vec_Y.c_str(), nullptr) }, 
										WorldVector{ ToDec<Dec>(Len_X.c_str(), nullptr), ToDec<Dec>(Len_Y.c_str(), nullptr) });
								} else {
									return std::make_unique<command::Image>(params);
								}
							},
							.checkParamState  = [params]() -> KeywordInfo::ParamResult { 
								switch (params.size()) {
									case 0:
									case 1:
									case 2:
									case 3:
									case 4:
										return KeywordInfo::ParamResult::Lack;
									case 5:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"button"] =
						words[L"ボタン"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() noexcept -> CommandPtr {
								switch (params.size()) {
									case 3:
									{
										const auto [Name, Name_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
										const auto [VX, VX_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
										const auto [VY, VY_Type] = Default_ProgramInterface.GetParamInfo(params[2]);
										if (Default_ProgramInterface.IsStringType(Name_Type) &&
											Default_ProgramInterface.IsNumberType(VX_Type) &&
											Default_ProgramInterface.IsNumberType(VY_Type))
										{
											return std::make_unique<command::Button>(
												Name,
												WorldVector{ ToDec<Dec>(VX.c_str(), nullptr), ToDec<Dec>(VY.c_str(), nullptr) },
												L"",
												WorldVector{ 0.0, 0.0 }
											);
										} else {
											return std::make_unique<command::Button>(params);
										}
										break;
									}
									case 6:
									{
										const auto [Name, Name_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
										const auto [VX, VX_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
										const auto [VY, VY_Type] = Default_ProgramInterface.GetParamInfo(params[2]);
										const auto [SX, SX_Type] = Default_ProgramInterface.GetParamInfo(params[3]);
										const auto [SY, SY_Type] = Default_ProgramInterface.GetParamInfo(params[4]);
										const auto [Img, Img_Type] = Default_ProgramInterface.GetParamInfo(params[5]);
										if (Default_ProgramInterface.IsStringType(Name_Type) &&
											Default_ProgramInterface.IsNumberType(VX_Type) &&
											Default_ProgramInterface.IsNumberType(VY_Type) &&
											Default_ProgramInterface.IsStringType(Img_Type) &&
											Default_ProgramInterface.IsNumberType(SX_Type) &&
											Default_ProgramInterface.IsNumberType(SY_Type))
										{
											return std::make_unique<command::Button>(
												Name,
												WorldVector{ ToDec<Dec>(VX.c_str(), nullptr), ToDec<Dec>(VY.c_str(), nullptr) },
												Img,
												WorldVector{ ToDec<Dec>(SX.c_str(), nullptr), ToDec<Dec>(SY.c_str(), nullptr) }
											);
										} else {
											return std::make_unique<command::Button>(params);
										}
										break;
									}
								}
							},
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
									case 1:
									case 2:
										return KeywordInfo::ParamResult::Lack;
									case 3:
										return KeywordInfo::ParamResult::Medium;
									case 4:
									case 5:
										return KeywordInfo::ParamResult::Lack;
									case 6:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"input"] =
						words[L"入力"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
								.Result = [&]() noexcept -> CommandPtr {
									return std::make_unique<command::Input>(params);
								},
								.checkParamState = [params]() -> KeywordInfo::ParamResult {
									switch (params.size()) {
										case 0:
										case 1:
										case 2:
										case 3:
											return KeywordInfo::ParamResult::Lack;
										case 4:
											return KeywordInfo::ParamResult::Maximum;
										default:
											return KeywordInfo::ParamResult::Excess;
									}
								},
								.is_static = false,
								.is_dynamic = true
						};
					};

					words[L"teleport"] =
						words[L"瞬間移動"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Target, Target_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								const auto [X, X_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
								const auto [Y, Y_Type] = Default_ProgramInterface.GetParamInfo(params[2]);
								if (Default_ProgramInterface.IsStringType(Target_Type) && Default_ProgramInterface.IsNumberType(X_Type) && Default_ProgramInterface.IsNumberType(Y_Type)) {
									return std::make_unique<command::entity::Teleport>(
										Target,
										WorldVector{ ToDec<Dec>(X.c_str(), nullptr), ToDec<Dec>(Y.c_str(), nullptr)
									});
								} else {
									return std::make_unique<command::entity::Teleport>(params);
								}
							},
							.checkParamState  = [params]() -> KeywordInfo::ParamResult { 
								switch (params.size()) {
									case 0:
									case 1:
									case 2:
										return KeywordInfo::ParamResult::Lack;
									case 3:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"kill"] =
						words[L"殺害"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Var, Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								if (Default_ProgramInterface.IsStringType(Type))
									return std::make_unique<command::entity::Kill>(Var);
								else
									return std::make_unique<command::entity::Kill>(params);
							},
							.checkParamState  = [params]() -> KeywordInfo::ParamResult { 
								switch (params.size()) {
									case 0:
										return KeywordInfo::ParamResult::Lack;
									case 1:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"makelayer"] =
						words[L"レイヤー生成"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Index, Index_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								const auto [Kind, Kind_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
								const auto [Name, Name_Type] = Default_ProgramInterface.GetParamInfo(params[2]);
								if (Default_ProgramInterface.IsNumberType(Index_Type) &&
									Default_ProgramInterface.IsStringType(Kind_Type) &&
									Default_ProgramInterface.IsStringType(Name_Type))
								{
									auto i = ToInt<int>(Index.c_str(), nullptr);
									return std::make_unique<command::layer::Make>(i, Kind, Name);
								}
								else
									return std::make_unique<command::layer::Make>(params);
							},
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
									case 1:
									case 2:
										return KeywordInfo::ParamResult::Lack;
									case 3:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"setbasis"] =
						words[L"レイヤー基準"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Entity_Name, Entity_Name_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								const auto [Layer_Name, Layer_Name_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
								if (Default_ProgramInterface.IsStringType(Entity_Name_Type) && Default_ProgramInterface.IsStringType(Layer_Name_Type))
									return std::make_unique<command::layer::SetBasis>(Entity_Name, Layer_Name);
								else
									return std::make_unique<command::layer::SetBasis>(params);
							},
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
									case 1:
										return KeywordInfo::ParamResult::Lack;
									case 2:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"selectlayer"] =
						words[L"レイヤー選択"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Name, Name_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								if (Default_ProgramInterface.IsStringType(Name_Type))
									return std::make_unique<command::layer::Select>(Name);
								else
									return std::make_unique<command::layer::Select>(params);
							},
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
										return KeywordInfo::ParamResult::Lack;
									case 1:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"deletelayer"] =
						words[L"レイヤー削除"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Name, Name_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								if (Default_ProgramInterface.IsStringType(Name_Type))
									return std::make_unique<command::layer::Delete>(Name);
								else
									return std::make_unique<command::layer::Delete>(params);
							},
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
										return KeywordInfo::ParamResult::Lack;
									case 1:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"showlayer"] =
						words[L"レイヤー表示"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Name, Name_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								return (Default_ProgramInterface.IsStringType(Name_Type) ? 
									std::make_unique<command::layer::Show>(Name) : std::make_unique<command::layer::Show>(params));
							},
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
										return KeywordInfo::ParamResult::Lack;
									case 1:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"hidelayer"] =
						words[L"レイヤー非表示"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Name, Name_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								return (Default_ProgramInterface.IsStringType(Name_Type) ?
									std::make_unique<command::layer::Hide>(Name) : std::make_unique<command::layer::Hide>(params));
							},
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
										return KeywordInfo::ParamResult::Lack;
									case 1:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"bind"] =
						words[L"キー"] = [this](const std::vector<std::wstring>& params) -> KeywordInfo {
							return {
							.Result = [&]() -> CommandPtr {
								const auto [Base, Base_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								const auto [Cmd, Cmd_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
								if (Default_ProgramInterface.IsStringType(Base_Type)) {
									std::wstring sub_param = Cmd + L' ';
									for (auto it = params.begin() + 2; it != params.end(); it++) {
										auto [word, word_type] = Default_ProgramInterface.GetParamInfo(*it);
										if (Default_ProgramInterface.IsStringType(word_type)) {
											sub_param += L'\'' + word + L'\'' + std::wstring(L" ");
										} else {
											sub_param += word + std::wstring(L" ");
										}
									}
									return std::make_unique<command::Bind>(Base, sub_param);
								} else
									return nullptr;
							},
							.checkParamState = [&]() -> KeywordInfo::ParamResult {
								if (params.size() <= 1) {
									return KeywordInfo::ParamResult::Lack;
								}

								const auto [Cmd, Cmd_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
								auto it = words.find(Cmd);
								if (it == words.end())
									return KeywordInfo::ParamResult::Lack;

								std::vector<std::wstring> exparams{};
								for (auto it = params.begin() + 2; it < params.end(); it++) {
									exparams.push_back(*it);
								}
								return it->second(exparams).checkParamState();
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"alias"] =
						words[L"別名"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								const auto [Base, Base_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								const auto [New_One, New_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
								if (Default_ProgramInterface.IsNoType(Base_Type) && Default_ProgramInterface.IsNoType(New_Type))
									return std::make_unique<command::Alias>(Base, New_One);
								else
									return nullptr;
							},
							.checkParamState  = [params]() -> KeywordInfo::ParamResult { 
								switch (params.size()) {
									case 0:
									case 1:
										return KeywordInfo::ParamResult::Lack;
									case 2:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = true,
							.is_dynamic = false
						};
					};

					// DLLアタッチ
					words[L"attach"] =
						words[L"接続"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								const auto [Var, Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								return (Default_ProgramInterface.IsStringType(Type) ?
									std::make_unique<command::Attach>(Var) : nullptr);
							},
							.checkParamState  = [params]() -> KeywordInfo::ParamResult { 
								switch (params.size()) {
									case 0:
										return KeywordInfo::ParamResult::Lack;
									case 1:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = true,
							.is_dynamic = false
						};
					};

					// DLLデタッチ
					words[L"detach"] =
						words[L"切断"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								const auto [Var, Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								return (Default_ProgramInterface.IsStringType(Type) ?
									std::make_unique<command::Detach>(Var) : nullptr);
							},
							.checkParamState  = [params]() -> KeywordInfo::ParamResult { 
								switch (params.size()) {
									case 0:
										return KeywordInfo::ParamResult::Lack;
									case 1:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = true,
							.is_dynamic = false
						};
					};

					// 外部イベントファイル読み込み
					words[L"load"] =
						words[L"読込"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								const auto [Path, Path_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								return (Default_ProgramInterface.IsStringType(Path_Type) ?
									std::make_unique<command::Load>(Path) : std::make_unique<command::Load>(params));
							},
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
										return KeywordInfo::ParamResult::Lack;
									case 1:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					// 外部イベントファイル読み込み
					words[L"import"] =
						words[L"取込"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								const auto [Path, Path_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								return (Default_ProgramInterface.IsStringType(Path_Type) ? 
									std::make_unique<command::Import>(Path) : std::make_unique<command::Import>(params));
							},
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
										return KeywordInfo::ParamResult::Lack;
									case 1:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"call"] =
						words[L"呼出"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								return std::make_unique<command::Call>(params);
							},
							.checkParamState  = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
										return KeywordInfo::ParamResult::Lack;
									case 1:
										return KeywordInfo::ParamResult::Medium;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"var"] =
						words[L"変数"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								const auto [Var, Var_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								const auto [Value, Value_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
								if (Var_Type == L"")
									return std::make_unique<command::Variable>(Var, Value);
								else
									return std::make_unique<command::Variable>(params);
							},
							.checkParamState  = [params]() -> KeywordInfo::ParamResult { 
								switch (params.size()) {
									case 0:
									case 1:
										return KeywordInfo::ParamResult::Lack;
									case 2:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}  
							},
							.is_static = true,
							.is_dynamic = false
						};
					};

					words[L"global"] =
						words[L"大域"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								const auto [Var, Var_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								const auto [Value, Value_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
								if (Var_Type == L"")
									return std::make_unique<command::Global>(Var, Value);
								else
									return std::make_unique<command::Global>(params);
							},
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
									case 1:
										return KeywordInfo::ParamResult::Lack;
									case 2:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = true,
							.is_dynamic = false
						};
					};

					words[L"exist"] = 
						words[L"存在確認"] = [](const std::vector<std::wstring>& params)->KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								return std::make_unique<command::Exist>(params);
							},
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
									case 1:
										return KeywordInfo::ParamResult::Lack;
									case 2:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"filter"] =
						words[L"フィルター"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								const auto [Layer_Name, Layer_Name_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								const auto [Filter_Name, Filter_Name_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
								const auto [Potency_Value, Potency_Value_Type] = Default_ProgramInterface.GetParamInfo(params[2]);
								if (Default_ProgramInterface.IsStringType(Layer_Name_Type) &&
								    Default_ProgramInterface.IsStringType(Filter_Name_Type) &&
								    Default_ProgramInterface.IsNumberType(Potency_Value_Type))
								{
									const auto Potency = std::stoi(Potency_Value);
									return std::make_unique<command::Filter>(Layer_Name, Filter_Name, Potency);
								} else {
									return std::make_unique<command::Filter>(params);
								}
							},
							.checkParamState  = [params]() -> KeywordInfo::ParamResult { 
								switch (params.size()) {
									case 0:
									case 1:
									case 2:
										return KeywordInfo::ParamResult::Lack;
									case 3:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"case"] =
						words[L"条件"] = [&](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								const auto [Var, Var_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								if (!Default_ProgramInterface.IsUndecidedType(Var_Type))
									return std::make_unique<command::Case>(Var);
								else
									return std::make_unique<command::Case>(params);
							},
							.checkParamState  = [params]() -> KeywordInfo::ParamResult { 
								switch (params.size()) {
									case 0:
										return KeywordInfo::ParamResult::Lack;
									case 1:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								} 
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"of"] =
						words[L"分岐"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								if (params.size() == 1) {
									const auto [Var, Type] = Default_ProgramInterface.GetParamInfo(params[0]);
									if (Default_ProgramInterface.IsNumberType(Type)) {
										auto [iv, ip] = ToInt(Var.c_str());
										auto [fv, fp] = ToDec<Dec>(Var.c_str());

										if (wcslen(ip) <= 0)
											return std::make_unique<command::Of>(L"==", iv);
										else
											return std::make_unique<command::Of>(L"==", fv);
									} else if (Default_ProgramInterface.IsStringType(Type)) {
										return std::make_unique<command::Of>(L"==", Var);
									} else {
										return std::make_unique<command::Of>(std::vector<std::wstring>{ L"==", params[0] });
									}
								} else {
									const auto [Mode, Mode_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
									const auto [Var, Type] = Default_ProgramInterface.GetParamInfo(params[1]);
									if (Default_ProgramInterface.IsNumberType(Type)) {
										auto [iv, ip] = ToInt(Var.c_str());
										auto [fv, fp] = ToDec<Dec>(Var.c_str());

										if (wcslen(ip) <= 0)
											return std::make_unique<command::Of>(Mode, iv);
										else
											return std::make_unique<command::Of>(Mode, fv);
									} else if (Default_ProgramInterface.IsStringType(Type)) {
										return std::make_unique<command::Of>(Mode, Var);
									} else {
										return std::make_unique<command::Of>(params);
									}
								}
							},
							.checkParamState  = [params]() -> KeywordInfo::ParamResult { 
								switch (params.size()) {
									case 0:
										return KeywordInfo::ParamResult::Lack;
									case 1:
										return KeywordInfo::ParamResult::Medium;
									case 2:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"__endof"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr { return std::make_unique<command::hidden::EndOf>(); },
							.checkParamState  = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
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
							.checkParamState  = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"__entity強制更新"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								return std::make_unique<command::hidden::UpdateEntity>();
							},
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"assign"] = words[L"代入"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								return std::make_unique<command::math::Assign>(params);
							},
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
									case 1:
										return KeywordInfo::ParamResult::Lack;
									case 2:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"sum"] = words[L"加算"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								return std::make_unique<command::math::Sum>(params);
							},
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
									case 1:
									case 2:
										return KeywordInfo::ParamResult::Lack;
									case 3:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"sub"] = words[L"減算"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								return std::make_unique<command::math::Sub>(params);
							},
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
									case 1:
									case 2:
										return KeywordInfo::ParamResult::Lack;
									case 3:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"mul"] = words[L"乗算"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								return std::make_unique<command::math::Mul>(params);
							},
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
									case 1:
									case 2:
										return KeywordInfo::ParamResult::Lack;
									case 3:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"div"] = words[L"徐算"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								return std::make_unique<command::math::Div>(params);
							},
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
									case 1:
									case 2:
										return KeywordInfo::ParamResult::Lack;
									case 3:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"or"] =
						words[L"論理和"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								return std::make_unique<command::math::Or>(params);
							},
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
									case 1:
									case 2:
										return KeywordInfo::ParamResult::Lack;
									case 3:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"and"] =
						words[L"論理積"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								return std::make_unique<command::math::And>(params);
							},
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
									case 1:
									case 2:
										return KeywordInfo::ParamResult::Lack;
									case 3:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"xor"] =
						words[L"排他的論理和"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								return std::make_unique<command::math::Xor>(params);
							},
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
									case 1:
									case 2:
										return KeywordInfo::ParamResult::Lack;
									case 3:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"not"] =
						words[L"論理否定"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								return std::make_unique<command::math::Not>(params);
							},
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
									case 1:
										return KeywordInfo::ParamResult::Lack;
									case 2:
										return KeywordInfo::ParamResult::Maximum;
									default:
										return KeywordInfo::ParamResult::Excess;
								}
							},
							.is_static = false,
							.is_dynamic = true
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

			// イベント引数を解析し、その結果を返す。
			auto ParseArgs(Context* context) const noexcept {
				ParamParser pparser(context);
				return pparser.Result();
			}

			// コマンドを解析し、その結果を返す。
			Event::Commands ParseCommand(Context* context, bool* const abort) const noexcept {
				CommandParser cmdparser(context);
				if (abort != nullptr) *abort = cmdparser.NeedAbortion();
				return std::move(cmdparser.Result());
			}

			Parser() {
				if (parser_abortion_error == nullptr)
					parser_abortion_error = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"イベント解析を強制終了しました。", MB_OK | MB_ICONERROR, 1);
			}

			Parser(const std::wstring& Sentence) noexcept : Parser() {
				auto context = ParseBasic(Sentence);
				bool aborted = false;

				while (!context.empty() && !aborted) {
					auto [name, trigger, min, max] = ParseInformation(&context);
					Program::Instance().var_manager.Get<false>(L"__生成中イベント") = name;

					auto params = ParseArgs(&context);
					Event::Commands commands = ParseCommand(&context, &aborted);

					if (event::Manager::Instance().error_handler.ShowLocalError(4))
						break;

					Event event{ .commands = std::move(commands), .trigger_type = trigger, .origin{ min, max }, .param_names=std::move(params) };
					events[name] = std::move(event);
				}

				if (aborted) {
					error::UserErrorHandler::SendGlobalError(parser_abortion_error);
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
	void Manager::ConditionManager::Evalute(const std::wstring& Mode, const std::any& Right_Value) noexcept {
		can_execute = true;
		auto& target_type = target_value.type();
		// 同じ型のみを比較する。
		if (target_type == Right_Value.type()) {
			if (Mode == L"==") {
				if (target_type == typeid(int)) {
					can_execute = (std::any_cast<int>(target_value) == std::any_cast<int>(Right_Value));
				} else if (target_type == typeid(Dec)) {
					can_execute = (std::any_cast<Dec>(target_value) == std::any_cast<Dec>(Right_Value));
				} else if (target_type == typeid(std::wstring)) {
					can_execute = (std::any_cast<std::wstring>(target_value) == std::any_cast<std::wstring>(Right_Value));
				}
			} else if (Mode == L"!") {
				if (target_type == typeid(int)) {
					can_execute = (std::any_cast<int>(target_value) != std::any_cast<int>(Right_Value));
				} else if (target_type == typeid(Dec)) {
					can_execute = (std::any_cast<Dec>(target_value) != std::any_cast<Dec>(Right_Value));
				} else if (target_type == typeid(std::wstring)) {
					can_execute = (std::any_cast<std::wstring>(target_value) != std::any_cast<std::wstring>(Right_Value));
				}
			} else if (Mode == L"<=") {
				if (target_type == typeid(int)) {
					can_execute = (std::any_cast<int>(target_value) <= std::any_cast<int>(Right_Value));
				} else if (target_type == typeid(Dec)) {
					can_execute = (std::any_cast<Dec>(target_value) <= std::any_cast<Dec>(Right_Value));
				}
			} else if (Mode == L">=") {
				if (target_type == typeid(int)) {
					can_execute = (std::any_cast<int>(target_value) >= std::any_cast<int>(Right_Value));
				} else if (target_type == typeid(Dec)) {
					can_execute = (std::any_cast<Dec>(target_value) >= std::any_cast<Dec>(Right_Value));
				}
			} else if (Mode == L"<") {
				if (target_type == typeid(int)) {
					can_execute = (std::any_cast<int>(target_value) < std::any_cast<int>(Right_Value));
				} else if (target_type == typeid(Dec)) {
					can_execute = (std::any_cast<Dec>(target_value) < std::any_cast<Dec>(Right_Value));
				}
			} else if (Mode == L">") {
				if (target_type == typeid(int)) {
					can_execute = (std::any_cast<int>(target_value) > std::any_cast<int>(Right_Value));
				} else if (target_type == typeid(Dec)) {
					can_execute = (std::any_cast<Dec>(target_value) > std::any_cast<Dec>(Right_Value));
				}
			}
		}
	}

	void Manager::ConditionManager::FreeCase() {
		target_value.reset();
		FreeOf();
	}

	void Manager::ConditionManager::FreeOf() {
		can_execute = true;
	}

	Manager::Manager() {
		error_class = error_handler.MakeErrorClass(L"イベントエラー");
		call_error = error_handler.MakeError(error_class, L"指定されたイベントが見つかりません。", MB_OK | MB_ICONERROR, 2);
	}

	std::unordered_map<std::wstring, Event> Manager::GenerateEvent(const std::wstring& Path) noexcept {
		EventGenerator::Instance().Generate(Path);
		return EventGenerator::Instance().Result();
	}

	void Manager::LoadEvent(const std::wstring path) noexcept {
		events = std::move(GenerateEvent(path));
	}

	void Manager::RequestEvent(const std::wstring& Path) noexcept {
		requesting_path = Path;
	}

	void Manager::ImportEvent(const std::wstring& Path) noexcept {
		auto additional = std::move(GenerateEvent(Path));
		for (auto& e : additional) {
			events[e.first] = std::move(e.second);
		}
	}

	void Manager::Update() noexcept {
		for (auto& e : events) {
			if (e.second.trigger_type == TriggerType::Load) {
				Call(e.first);
			}
		}

		std::queue<std::wstring> dead;
		for (auto& e : events) {
			auto& event = e.second;
			if (event.trigger_type == TriggerType::Auto) {
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

		if (!requesting_path.empty()) {
			LoadEvent(requesting_path);
			requesting_path.clear();
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
		auto candidate = events.find(EName);
		if (candidate != events.end()) {
			auto event_name = std::any_cast<std::wstring>(Program::Instance().var_manager.Get<false>(variable::Executing_Event_Name));
			CommandExecuter cmd_executer(std::move(candidate->second.commands));
			Program::Instance().var_manager.Get<false>(variable::Executing_Event_Name) = (event_name += std::wstring(EName) + L"\n");
			cmd_executer.Execute();

			candidate->second.commands = std::move(cmd_executer.Result());
			if (candidate->second.trigger_type == TriggerType::Load)
				candidate->second.trigger_type = TriggerType::None;
			event_name.erase(event_name.find(EName + L"\n"));

			Program::Instance().var_manager.Get<false>(variable::Executing_Event_Name) = event_name;
		} else {
			error_handler.SendLocalError(call_error, L"イベント名: " + EName);
			return;
		}
	}

	void Manager::NewCaseTarget(std::any tv) {
		condition_manager.push_back(ConditionManager(tv));
		condition_current = condition_manager.end() - 1;
	}

	void Manager::Evalute(const std::wstring& Mode, const std::any& Right_Value) {
		condition_current->Evalute(Mode, Right_Value);
	}

	void Manager::FreeCase() {
		condition_current->FreeCase();
		condition_manager.pop_back();
		if (!condition_manager.empty())
			condition_current = condition_manager.end() - 1;
	}

	void Manager::FreeOf() {
		condition_current->FreeOf();
	}

	bool Manager::CanOfExecute() const noexcept {
		if (!condition_manager.empty() && condition_current != condition_manager.end()) {
			return (condition_current->CanExecute());
		} else
			return true;
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
		else if (TSentence == L"a")
			targeting->trigger_type = TriggerType::Auto;
		else if (TSentence == L"t")
			targeting->trigger_type = TriggerType::Trigger;
	}

	void command::Alias::Execute() {
		EventGenerator::Instance().MakeAliasCommand(original, newone);
		StandardCommand::Execute();
	}

	void command::Bind::Execute() {
		static uint64_t count = 0;
		EventGenerator::Parser parser;
		const auto Event_Name = L"__キーイベント_" + std::to_wstring(count++);
		auto editor = Program::Instance().MakeEventEditor();
		editor->MakeNewEvent(Event_Name);
		editor->ChangeTriggerType(L"n");
		editor->ChangeRange({ 0, 0 }, { 0, 0 });
		editor->AddCommand(L'{' + command_sentence + L'}', 0);
		Program::Instance().FreeEventEditor(editor);


		Program::Instance().engine.BindKey(key_name, [Event_Name]() {
			Program::Instance().event_manager.Call(Event_Name);
		});
		StandardCommand::Execute();
	}
}