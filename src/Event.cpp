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
				if (auto pos = executing_name.find(L'\n'); pos != executing_name.npos)
					executing_name.erase(executing_name.begin());
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

		// 動的コマンド向け抽象クラス。
		// 引数をコマンド実行時に読み込む為のメンバ関数を持つ。
		class DynamicCommand : public Command {
			// 読み込む予定の引数名。
			std::vector<std::wstring> param_names{};
		protected:
			inline static error::ErrorClass *command_error_class{};
			inline static error::ErrorContent *incorrect_type_error{},
				*lack_of_parameters_error{},
				*empty_name_error{},
				*entity_not_found_error{};

			DynamicCommand() noexcept {
				if (command_error_class == nullptr) [[unlikely]]
					command_error_class = error::UserErrorHandler::MakeErrorClass(L"コマンドエラー");
				if (incorrect_type_error == nullptr) [[unlikely]]
					incorrect_type_error = error::UserErrorHandler::MakeError(command_error_class, L"引数の型が不適切です。", MB_OK | MB_ICONERROR, 1);
				if (lack_of_parameters_error == nullptr) [[unlikely]]
					lack_of_parameters_error = error::UserErrorHandler::MakeError(command_error_class, L"引数が足りない為、コマンドを実行できません。", MB_OK | MB_ICONERROR, 1);
				if (empty_name_error == nullptr) [[unlikely]]
					empty_name_error = error::UserErrorHandler::MakeError(command_error_class, L"名前を空にすることはできません。", MB_OK | MB_ICONERROR, 2);
				if (entity_not_found_error == nullptr) [[unlikely]]
					entity_not_found_error = error::UserErrorHandler::MakeError(command_error_class, L"Entityが見つかりません。", MB_OK | MB_ICONERROR, 2);
			}

			DynamicCommand(const decltype(param_names)& Param) noexcept : DynamicCommand() {
				param_names = Param;
			}

			// 引数をコマンド実行時に読み込む必要があるか否か。
			bool MustSearch() const noexcept {
				return !param_names.empty();
			}

			template<const bool Get_Param_Name = false>
			std::any GetParam(const int Index) const noexcept {
				if (Index < 0 || Index >= param_names.size())
					return nullptr;

				auto [var, type] = Default_ProgramInterface.GetParamInfo(param_names[Index]);
				if constexpr (!Get_Param_Name) {
					if (Default_ProgramInterface.IsNumberType(type)) {
						auto [iv, ip] = ToInt(var.c_str());
						auto [fv, fp] = ToDec<Dec>(var.c_str());
						if (wcslen(ip) <= 0)
							return iv;
						else
							return fv;
					} else if (Default_ProgramInterface.IsStringType(type)) {
						return var;
					} else {
						auto event_name = std::any_cast<std::wstring>(Program::Instance().var_manager.Get<false>(variable::Executing_Event_Name));
						event_name = event_name.substr(0, event_name.size() - 1);
						if (auto pos = event_name.rfind(L'\n'); pos != std::wstring::npos)
							event_name = event_name.substr(pos);
						if (auto pos = event_name.find(L'\n'); pos != std::wstring::npos)
							event_name.erase(event_name.begin());

						auto& global_var = Program::Instance().var_manager.Get<false>(param_names[Index]);
						auto& local_var = Program::Instance().var_manager.Get<false>(event_name + L'.' + param_names[Index]);
						if (local_var.type() != typeid(std::nullptr_t))
							return local_var;
						else
							return global_var;	// 
					}
				} else {
					if (Default_ProgramInterface.IsStringType(type) || Default_ProgramInterface.IsNumberType(type))
						return var;
					else
						return param_names[Index];
				}
			}

			void SetAllParams(std::vector<std::any>* to) {
				for (int i = 0; i < param_names.size(); i++) {
					auto value = GetParam(i);
					if (value.type() == typeid(std::nullptr_t)) {
						auto value_name = std::any_cast<std::wstring>(GetParam<true>(i));
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
			Variable(const std::wstring& VName, const std::wstring& Any_Value) noexcept : Variable(std::vector<std::wstring>{}) {
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

			bool CheckParams() noexcept {
				if (MustSearch()) {
					auto name = GetParam<true>(0);
					auto any_value = GetParam<true>(1);
					if (name.type() == typeid(std::nullptr_t))
						goto noname_error;
					else if (any_value.type() == typeid(std::nullptr_t))
						goto novalue_error;

					varname = std::any_cast<std::wstring>(name);
					value = GetParam(1);
					if (value.type() == typeid(std::nullptr_t)) {
						auto value_name = std::any_cast<std::wstring>(any_value);
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
				return true;

			// 変数名が書かれていない場合のエラー(=引数が0個)。
			noname_error:
			// 初期値が指定されていない場合のエラー(=引数が1個)。 
			novalue_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"コマンド名: var/変数");
				goto return_failed;
			return_failed:
				return false;
			}

			void Execute(const std::wstring& Var_Name) noexcept {
				Program::Instance().var_manager.MakeNew(varname) = value;
			}

			void Execute() override {
				if (CheckParams()) {
					Program::Instance().var_manager.MakeNew(varname) = value;
				}
			}
		};

		// グローバル変数
		class Global final : public Variable {
		public:
			using Variable::Variable;

			void Execute() override {
				if (CheckParams()) {
					varname = varname.substr(varname.find(L'.') + 1);
					Variable::Execute(varname);
				}
			}
		};
		
		// 変数/関数/Entityの存在確認
		DYNAMIC_COMMAND(Exist final) {
			inline static error::ErrorContent *assign_error{};
		public:
			DYNAMIC_COMMAND_CONSTRUCTOR(Exist) {
				if (assign_error == nullptr) [[unlikely]]
					assign_error = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"代入先の変数が存在しません。\n新しくこの変数を作成しますか?", MB_YESNO | MB_ICONERROR, 2);
			}

			~Exist() noexcept final {}

			void Execute() final {
				std::wstring name[2]{ {}, {} };
				for (int i = 0; i < 2; i++)
					name[i] = std::any_cast<std::wstring>(GetParam<true>(i));

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
			}
		};

		// 条件対象設定
		DYNAMIC_COMMAND(Case final) {
			std::any value;
		public:
			Case(const std::wstring& Param) noexcept : Case(std::vector<std::wstring>{}) {
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
					value = GetParam(0);
				}
				Program::Instance().event_manager.NewCaseTarget(value);
			}
		};

		// 条件式
		DYNAMIC_COMMAND(Of final) {
			std::wstring mode;
			std::any value;
		public:
			Of(const std::wstring& Condition_Sentence, const std::any& V) noexcept : Of(std::vector<std::wstring>{}) {
				mode = Condition_Sentence;
				value = V;
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Of) {}

			~Of() noexcept final {}

			void Execute() override {
				if (MustSearch()) {
					mode = std::any_cast<std::wstring>(GetParam<true>(0));
					auto vname = std::any_cast<std::wstring>(GetParam<true>(1));
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
				Program::Instance().var_manager.Get<false>(L"of_state") = (int)Program::Instance().event_manager.Evalute(mode, value);
			}
		};

		// 条件式(全て該当なしの場合)
		DYNAMIC_COMMAND(Else final) {
		public:
			Else() noexcept : Else(std::vector<std::wstring>{}) {}

			DYNAMIC_COMMAND_CONSTRUCTOR(Else) {}

			~Else() noexcept final {}

			void Execute() override {
				Program::Instance().var_manager.Get<false>(L"of_state") = (int)!std::any_cast<int>(Program::Instance().var_manager.Get<false>(L"of_state"));
			}
		};

		// 条件終了
		DYNAMIC_COMMAND(EndCase final) {
		public:
			EndCase() noexcept : EndCase(std::vector<std::wstring>{}) {}
			~EndCase() final {}

			DYNAMIC_COMMAND_CONSTRUCTOR(EndCase) {}

			void Execute() final {
				Program::Instance().event_manager.FreeCase();
			}
		};

		// 画像を読み込み、表示させる。
		DYNAMIC_COMMAND(Image) {
			std::shared_ptr<karapo::entity::Image> image;
			std::wstring path;
		public:
			Image(const std::wstring& P, const WorldVector WV) : Image(P, WV, WorldVector{ 0, 0 }) {}

			Image(const std::wstring& P, const WorldVector WV, const WorldVector Len) : Image(std::vector<std::wstring>{}) {
				path = P;
				image = std::make_shared<karapo::entity::Image>(WV, Len);
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Image) {}

			~Image() override {}

			void Execute() override {
				if (MustSearch()) {
					auto path_param = GetParam(0),
						x_param = GetParam(1),
						y_param = GetParam(2),
						w_param = GetParam(3),
						h_param = GetParam(4);

					if (path_param.type() == typeid(std::nullptr_t) ||
						x_param.type() == typeid(std::nullptr_t) ||
						y_param.type() == typeid(std::nullptr_t) ||
						w_param.type() == typeid(std::nullptr_t) ||
						h_param.type() == typeid(std::nullptr_t)) [[unlikely]]
					{
						goto lack_error;
					} 
					else if (path_param.type() != typeid(std::wstring) ||
						(x_param.type() != typeid(int) && x_param.type() != typeid(Dec)) ||
						(y_param.type() != typeid(int) && y_param.type() != typeid(Dec)) ||
						(w_param.type() != typeid(int) && w_param.type() != typeid(Dec)) ||
						(h_param.type() != typeid(int) && h_param.type() != typeid(Dec))) [[unlikely]]
					{
						goto type_error;
					}


					path = std::any_cast<std::wstring>(path_param);
					Dec x = (x_param.type() == typeid(Dec) ? std::any_cast<Dec>(x_param) : std::any_cast<int>(x_param)),
						y = (y_param.type() == typeid(Dec) ? std::any_cast<Dec>(y_param) : std::any_cast<int>(y_param)),
						w = (w_param.type() == typeid(Dec) ? std::any_cast<Dec>(w_param) : std::any_cast<int>(w_param)),
						h = (h_param.type() == typeid(Dec) ? std::any_cast<Dec>(h_param) : std::any_cast<int>(h_param));
					image = std::make_shared<karapo::entity::Image>(WorldVector{ x, y }, WorldVector{ w, h });
				}
				image->Load(path.c_str());
				Program::Instance().entity_manager.Register(image);
				return;

			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"コマンド名: image/画像");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"コマンド名: image/画像");
				goto end_of_function;
			end_of_function:
				return;
			}
		};

		// アニメーション用変数を作成する。
		DYNAMIC_COMMAND(Animation final) {
			std::wstring var_name{};
		public:
			Animation(const std::wstring& Var_Name) noexcept : DynamicCommand(std::vector<std::wstring>{}) {
				var_name = Var_Name;
			}

			~Animation() noexcept final {}

			void Execute() final {
				if (var_name.empty())
					goto name_error;
				Program::Instance().var_manager.MakeNew(var_name) = animation::Animation();
				Program::Instance().var_manager.MakeNew(var_name + L".frame") = animation::FrameRef();
				return;
			name_error:
				event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"コマンド名: animation/アニメ");
			}
		};

		// アニメーション変数にフレームを追加する。
		DYNAMIC_COMMAND(AddFrame final) {
			std::wstring var_name{}, image_path{};
		public:
			AddFrame(const std::wstring& Var_Name, const std::wstring& Image_Path) noexcept : AddFrame(std::vector<std::wstring>{}) {
				var_name = Var_Name;
				image_path = Image_Path;
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(AddFrame) {}

			~AddFrame() noexcept final {}

			void Execute() final {
				if (MustSearch()) {
					auto var_name_param = GetParam<true>(0),
						path_param = GetParam(1);
					if (var_name_param.type() == typeid(std::nullptr_t) || path_param.type() == typeid(std::nullptr_t)) [[unlikely]]
						goto lack_error;
					else if (var_name_param.type() != typeid(std::wstring) || path_param.type() != typeid(std::wstring)) [[unlikely]]
						goto type_error;

					var_name = std::any_cast<std::wstring>(var_name_param);
					image_path = std::any_cast<std::wstring>(path_param);
				}

				if (var_name.empty() || image_path.empty())
					goto name_error;
				else {
					auto& anime_var = Program::Instance().var_manager.Get<false>(var_name);
					if (anime_var.type() != typeid(animation::Animation))
						goto type_error;
					else {
						auto& anime = std::any_cast<animation::Animation&>(anime_var);
						auto& frame_var = Program::Instance().var_manager.Get<false>(var_name + L".frame");
						auto& frame = std::any_cast<animation::FrameRef&>(frame_var);

						resource::Image image;
						image = Program::Instance().engine.LoadImage(image_path);
						anime.PushBack(image);
						frame.InitFrame(anime.Begin(), anime.End());
					}
				}
				return;
			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"コマンド名: addframe/フレーム追加");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"コマンド名: addframe/フレーム追加");
				goto end_of_function;
			name_error:
				event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"コマンド名: addframe/フレーム追加");
				goto end_of_function;
			end_of_function:
				return;
			}
		};

		// フレームを次に進める。
		DYNAMIC_COMMAND(NextFrame final) {
			std::wstring var_name{};
		public:
			NextFrame(const std::wstring & Var_Name) noexcept : DynamicCommand(std::vector<std::wstring>{}) {
				var_name = Var_Name;
			}

			~NextFrame() noexcept final {}

			void Execute() final {
				if (var_name.empty())
					goto name_error;
				else {
					auto& frame_var = Program::Instance().var_manager.Get<false>(var_name + L".frame");
					if (frame_var.type() != typeid(animation::FrameRef))
						goto type_error;
					else {
						std::any_cast<animation::FrameRef&>(frame_var)++;
						auto frame = std::any_cast<animation::FrameRef&>(frame_var);
					}
				}
				return;
			name_error:
				event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"コマンド名: nextframe/次フレーム");
				return;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"コマンド名: nextframe/次フレーム");
				return;
			}
		};

		// フレームを前に戻す。
		DYNAMIC_COMMAND(BackFrame final) {
			std::wstring var_name{};
		public:
			BackFrame(const std::wstring & Var_Name) noexcept : DynamicCommand(std::vector<std::wstring>{}) {
				var_name = Var_Name;
			}

			~BackFrame() noexcept final {}

			void Execute() final {
				if (var_name.empty())
					goto name_error;
				else {
					auto& frame_var = Program::Instance().var_manager.Get<false>(var_name + L".frame");
					if (frame_var.type() != typeid(animation::FrameRef))
						goto type_error;
					else {
						std::any_cast<animation::FrameRef&>(frame_var)--;
					}
				}
				return;
			name_error:
				event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"コマンド名: backframe/前フレーム");
				return;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"コマンド名: backframe/前フレーム");
				return;
			}
		};

		// 画像から一部をコピーする。
		DYNAMIC_COMMAND(Capture) {
			std::wstring variable_name{}, path{};
			ScreenVector position{}, length{};
		public:
			Capture(const std::wstring & V, const std::wstring & P, const ScreenVector Pos, const ScreenVector Len) : Capture(std::vector<std::wstring>{}) {
				variable_name = V;
				path = P;
				position = Pos;
				length = Len;
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Capture) {}

			~Capture() override {}

			void Execute() override {
				int x{}, y{}, w{}, h{};
				if (MustSearch()) {
					auto var_name = GetParam<true>(0),
						path_param = GetParam(1),
						x_param = GetParam(2),
						y_param = GetParam(3),
						w_param = GetParam(4),
						h_param = GetParam(5);

					path_param.has_value();
					if (var_name.type() == typeid(std::nullptr_t) ||
						path_param.type() == typeid(std::nullptr_t) ||
						x_param.type() == typeid(std::nullptr_t) ||
						y_param.type() == typeid(std::nullptr_t) ||
						w_param.type() == typeid(std::nullptr_t) ||
						h_param.type() == typeid(std::nullptr_t)) [[unlikely]]
					{
						goto lack_error;
					}
					else if (var_name.type() != typeid(std::wstring) ||
						path_param.type() != typeid(std::wstring) ||
						x_param.type() != typeid(int) ||
						y_param.type() != typeid(int) ||
						w_param.type() != typeid(int) ||
						h_param.type() != typeid(int)) [[unlikely]]
					{
						goto type_error;
					}

					variable_name = std::any_cast<std::wstring>(var_name);
					path = std::any_cast<std::wstring>(path_param);
					position[0] = std::any_cast<int>(x_param);
					position[1] = std::any_cast<int>(y_param);
					length[0] = std::any_cast<int>(w_param);
					length[1] = std::any_cast<int>(h_param);
				}
				Program::Instance().engine.CopyImage(&path, position, length);
				Program::Instance().var_manager.Get<false>(variable_name) = path;
				return;

			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"コマンド名: capture/画像切取");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"コマンド名: capture/画像切取");
				goto end_of_function;
			end_of_function:
				return;
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
					auto path_param = GetParam(0);
					if (path_param.type() == typeid(std::nullptr_t)) [[unlikely]]
						goto lack_error;
					else if (path_param.type() != typeid(std::wstring)) [[unlikely]]
						goto type_error;
					
					path = std::any_cast<std::wstring>(GetParam(0));
				}
				ReplaceFormat(&path);
				music->Load(path);
				Program::Instance().entity_manager.Register(music);
				return;

			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"コマンド名: music/BGM/音楽");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"コマンド名: music/BGM/音楽");
				goto end_of_function;
			end_of_function:
				return;
			}
		};

		// 効果音
		DYNAMIC_COMMAND(Sound) {
			std::shared_ptr<karapo::entity::Sound> sound;
			std::wstring path;
		public:
			Sound(const std::wstring& P, const WorldVector& WV) : Sound(std::vector<std::wstring>{}) {
				path = P;
				sound = std::make_shared<karapo::entity::Sound>(WV);
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Sound) {}

			~Sound() noexcept override {}

			void Execute() override {
				if (MustSearch()) {
					auto path_param = GetParam(0),
						x_param = GetParam(1),
						y_param = GetParam(2);
					if (path_param.type() == typeid(std::nullptr_t) ||
						x_param.type() == typeid(std::nullptr_t) ||
						y_param.type() == typeid(std::nullptr_t)) [[unlikely]]
					{
						goto lack_error;
					}
					else if (path_param.type() != typeid(std::wstring) ||
						(x_param.type() != typeid(int) && x_param.type() != typeid(Dec)) ||
						(y_param.type() != typeid(int) && y_param.type() != typeid(Dec))) [[unlikely]]
					{
						goto type_error;
					}

					path = std::any_cast<std::wstring>(GetParam(0));
					Dec x = (GetParam(1).type() == typeid(Dec) ? std::any_cast<Dec>(GetParam(1)) : std::any_cast<int>(GetParam(1))),
						y = (GetParam(2).type() == typeid(Dec) ? std::any_cast<Dec>(GetParam(2)) : std::any_cast<int>(GetParam(2)));
					sound = std::make_shared<karapo::entity::Sound>(WorldVector{ x, y });
				}
				ReplaceFormat(&path);
				sound->Load(path);
				Program::Instance().entity_manager.Register(sound);
				return;
			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"コマンド名: Sound/効果音");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"コマンド名: Sound/効果音");
				goto end_of_function;
			end_of_function:
				return;
			}
		};

		// ボタン
		DYNAMIC_COMMAND(Button final) {
			std::shared_ptr<karapo::entity::Button> button;
			std::any path{};
		public:
			Button(const std::wstring& Name,
				const WorldVector& WV,
				const std::wstring& Image_Path,
				const WorldVector& Size) noexcept : Button(std::vector<std::wstring>{})
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
					auto name_param = GetParam(0),
						x_param = GetParam(1),
						y_param = GetParam(2);
					if (name_param.type() == typeid(std::nullptr_t) ||
						x_param.type() == typeid(std::nullptr_t) ||
						y_param.type() == typeid(std::nullptr_t)) [[unlikely]]
					{
						goto lack_error;
					} 
					else if (name_param.type() != typeid(std::wstring) ||
						(x_param.type() != typeid(Dec) && x_param.type() != typeid(int)) ||
						(y_param.type() != typeid(Dec) && y_param.type() != typeid(int))) [[unlikely]]
					{
						goto type_error;
					}

					auto name = std::any_cast<std::wstring>(name_param);
					Dec x = (GetParam(1).type() == typeid(Dec) ? std::any_cast<Dec>(x_param) : std::any_cast<int>(x_param)),
						y = (GetParam(2).type() == typeid(Dec) ? std::any_cast<Dec>(y_param) : std::any_cast<int>(y_param));

					auto w_param = GetParam(3),
						h_param = GetParam(4),
						path_param = GetParam(5);
					Dec w{}, h{};
					if (w_param.type() != typeid(std::nullptr_t) && h_param.type() != typeid(std::nullptr_t) && path_param.type() != typeid(std::nullptr_t)) {
						w = (GetParam(3).type() == typeid(Dec) ? std::any_cast<Dec>(w_param) : std::any_cast<int>(w_param));
						h = (GetParam(4).type() == typeid(Dec) ? std::any_cast<Dec>(h_param) : std::any_cast<int>(h_param));
						path = std::move(path_param);
					}
					ReplaceFormat(&name);
					button = std::make_shared<karapo::entity::Button>(name, WorldVector{ x, y }, WorldVector{ w, h });
				}

				if (path.type() == typeid(std::wstring)) {
					auto path_string = std::any_cast<std::wstring>(path);
					ReplaceFormat(&path_string);
					button->Load(path_string);
				} else if (path.type() == typeid(animation::FrameRef)) {
					button->Load(&std::any_cast<animation::FrameRef&>(
							Program::Instance().var_manager.Get<false>(
								std::any_cast<std::wstring>(GetParam<true>(5))
							)
						)
					);
				}

				Program::Instance().entity_manager.Register(button);
				return;
			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"コマンド名: Button/ボタン");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"コマンド名: Button/ボタン");
				goto end_of_function;
			end_of_function:
				return;
			}
		};

		// 文字出力
		DYNAMIC_COMMAND(Print final) {
			std::shared_ptr<karapo::entity::Text> text;
		public:
			Print(const std::wstring& Name, const WorldVector& Pos) noexcept : Print(std::vector<std::wstring>{}) {
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
					auto name_param = GetParam(0),
						x_param = GetParam(1),
						y_param = GetParam(2);
					if (name_param.type() == typeid(std::nullptr_t) ||
						x_param.type() == typeid(std::nullptr_t) ||
						y_param.type() == typeid(std::nullptr_t)) [[unlikely]]
					{
						goto lack_error;
					}
					else if (name_param.type() != typeid(std::wstring) ||
						(x_param.type() != typeid(Dec) && x_param.type() != typeid(int)) ||
						(y_param.type() != typeid(Dec) && y_param.type() != typeid(int))) [[unlikely]]
					{
						goto type_error;
					}
					auto name = std::any_cast<std::wstring>(name_param);
					Dec x = (x_param.type() == typeid(Dec) ? std::any_cast<Dec>(x_param) : std::any_cast<int>(x_param)),
						y = (y_param.type() == typeid(Dec) ? std::any_cast<Dec>(y_param) : std::any_cast<int>(y_param));
					ReplaceFormat(&name);

					Program::Instance().var_manager.MakeNew(name + L".text") = std::wstring(L"");
					text = std::make_shared<karapo::entity::Text>(name, WorldVector{ x, y });
				}
				Program::Instance().entity_manager.Register(text);
				return;
			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"コマンド名: text/文章");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"コマンド名: text/文章");
				goto end_of_function;
			end_of_function:
				return;
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
					auto name_param = GetParam<true>(0),
						x_param = GetParam(1),
						y_param = GetParam(2),
						len_param = GetParam(3);
					if (name_param.type() == typeid(std::nullptr_t) ||
						x_param.type() == typeid(std::nullptr_t) ||
						y_param.type() == typeid(std::nullptr_t) ||
						len_param.type() == typeid(std::nullptr_t)) [[unlikely]]
					{
						goto lack_error;
					} 
					else if (name_param.type() != typeid(std::wstring) ||
						x_param.type() != typeid(int) ||
						y_param.type() != typeid(int) ||
						len_param.type() != typeid(int)) [[unlikely]]
					{
						goto type_error;
					}
					var = &Program::Instance().var_manager.Get<false>(std::any_cast<std::wstring>(name_param));
					pos[0] = std::any_cast<int>(x_param);
					pos[1] = std::any_cast<int>(y_param);
					length = std::any_cast<int>(len_param);
				}
				wchar_t str[10000];
				Program::Instance().engine.GetString(pos, str, length);
				*var = std::wstring(str);
				return;
			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"コマンド名: input/入力");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"コマンド名: input/入力");
				goto end_of_function;
			end_of_function:
				return;
			}
		};

		namespace entity {
			// Entityの移動
			DYNAMIC_COMMAND(Teleport final) {
				std::wstring entity_name;
				WorldVector move;
			public:
				Teleport(const std::wstring& ename, const WorldVector& MV) noexcept : Teleport(std::vector<std::wstring>{}) {
					entity_name = ename;
					move = MV;
				}

				DYNAMIC_COMMAND_CONSTRUCTOR(Teleport) {}

				~Teleport() override {}

				void Execute() override {
					if (MustSearch()) {
						auto name_param = GetParam(0),
							x_param = GetParam(1),
							y_param = GetParam(2);
						if (name_param.type() == typeid(std::nullptr_t) ||
							x_param.type() == typeid(std::nullptr_t) ||
							y_param.type() == typeid(std::nullptr_t)) [[unlikely]]
						{
							goto lack_error;
						} 
						else if (name_param.type() != typeid(std::wstring)) [[unlikely]]
						{
							goto type_error;
						}
						entity_name = std::any_cast<std::wstring>(name_param);
						Dec x{}, y{};
						if (x_param.type() == typeid(Dec))
							x = std::any_cast<Dec>(x_param);
						else if (x_param.type() == typeid(int))
							x = std::any_cast<int>(x_param);
						else [[unlikely]]
							goto type_error;

						if (y_param.type() == typeid(Dec))
							y = std::any_cast<Dec>(y_param);
						else if (y_param.type() == typeid(int))
							y = std::any_cast<int>(y_param);
						else [[unlikely]]
							goto type_error;
						move = { x, y };
					}

					{
						ReplaceFormat(&entity_name);
						auto ent = Program::Instance().entity_manager.GetEntity(entity_name);
						if (ent != nullptr)
							ent->Teleport(move);
						else
							goto entity_error;

					}
					return;
				entity_error:
					event::Manager::Instance().error_handler.SendLocalError(entity_not_found_error, L"コマンド名: teleport/瞬間移動");
					goto end_of_function;
				lack_error:
					event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"コマンド名: teleport/瞬間移動");
					goto end_of_function;
				type_error:
					event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"コマンド名: teleport/瞬間移動");
					goto end_of_function;
				end_of_function:
					return;
				}
			};

			// Entityの削除。
			DYNAMIC_COMMAND(Kill final) {
				std::wstring entity_name{};
			public:
				Kill(const std::wstring & ename) noexcept : Kill(std::vector<std::wstring>{}) {
					entity_name = ename;
				}

				DYNAMIC_COMMAND_CONSTRUCTOR(Kill) {}

				~Kill() override {}

				void Execute() override {
					if (MustSearch()) {
						auto name_param = GetParam(0);
						if (name_param.type() == typeid(std::nullptr_t)) [[unlikely]]
							goto lack_error;
						else if (name_param.type() != typeid(std::wstring)) [[unlikely]]
							goto type_error;

						entity_name = std::any_cast<std::wstring>(name_param);
					}
					if (entity_name.empty())
						goto name_error;

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
					
					return;
				name_error:
					event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"コマンド名: kill/殺害");
					goto end_of_function;
				lack_error:
					event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"コマンド名: kill/殺害");
					goto end_of_function;
				type_error:
					event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"コマンド名: kill/殺害");
					goto end_of_function;
				end_of_function:
					return;
				}
			};

			// EntityまたはEntityの種類の更新凍結。
			DYNAMIC_COMMAND(Freeze final) {
				std::wstring candidate_name{};
			public:
				Freeze(const std::wstring& ename) noexcept : Freeze(std::vector<std::wstring>{}) {
					candidate_name = ename;
				}

				DYNAMIC_COMMAND_CONSTRUCTOR(Freeze) {}

				~Freeze() final {}

				void Execute() override {
					if (MustSearch()) {
						auto candidate_name_param = GetParam(0);
						if (candidate_name_param.type() != typeid(std::wstring)) {
							goto type_error;
						}
						candidate_name = std::any_cast<std::wstring>(candidate_name_param);
					}

					if (candidate_name.empty()) {
						goto name_error;
					} else {
						auto ent = karapo::entity::Manager::Instance().GetEntity(candidate_name);
						if (ent != nullptr) {
							karapo::entity::Manager::Instance().Freeze(ent);
						} else {
							karapo::entity::Manager::Instance().Freeze(candidate_name);
						}
					}
					goto end_of_function;
				name_error:
					event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"コマンド名: freeze/凍結");
					goto end_of_function;
				lack_error:
					event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"コマンド名: freeze/凍結");
					goto end_of_function;
				type_error:
					event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"コマンド名: freeze/凍結");
					goto end_of_function;
				end_of_function:
					return;
				}
			};

			// EntityまたはEntityの種類の更新再開。
			DYNAMIC_COMMAND(Defrost final) {
				std::wstring candidate_name{};
			public:
				Defrost(const std::wstring& ename) noexcept : Defrost(std::vector<std::wstring>{}) {
					candidate_name = ename;
				}

				DYNAMIC_COMMAND_CONSTRUCTOR(Defrost) {}

				~Defrost() final {}

				void Execute() override {
					if (MustSearch()) {
						auto candidate_name_param = GetParam(0);
						if (candidate_name_param.type() != typeid(std::wstring)) {
							goto type_error;
						}
						candidate_name = std::any_cast<std::wstring>(candidate_name_param);
					}
					if (candidate_name.empty()) {
						goto name_error;
					} else {
						if (candidate_name.empty()) {
							goto name_error;
						} else {
							auto ent = karapo::entity::Manager::Instance().GetEntity(candidate_name);
							if (ent != nullptr) {
								karapo::entity::Manager::Instance().Defrost(ent);
							} else {
								karapo::entity::Manager::Instance().Defrost(candidate_name);
							}
						}
					}
					goto end_of_function;
				name_error:
					event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"コマンド名: defrost/解凍");
					goto end_of_function;
				lack_error:
					event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"コマンド名: defrost/解凍");
					goto end_of_function;
				type_error:
					event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"コマンド名: defrost/解凍");
					goto end_of_function;
				end_of_function:
					return;
				}
			};
		}

		// キー入力毎のコマンド割当
		class Bind final : public DynamicCommand {
			std::wstring key_name{}, command_sentence{};
		public:
			Bind(const std::wstring& Key_Name, const std::wstring& Command_Sentence) noexcept {
				key_name = Key_Name;
				command_sentence = Command_Sentence;
			}

			void Execute() override;
		};

		// コマンドの別名
		class Alias : public DynamicCommand {
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
			Filter(const std::wstring& N, const std::wstring& KN, const int P) noexcept : Filter(std::vector<std::wstring>{}) {
				layer_name = N;
				kind_name = KN;
				potency = P % 256;
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Filter) {}

			~Filter() final {}

			void Execute() final {
				if (MustSearch()) {
					auto layer_name_param = GetParam(0),
						kind_name_param = GetParam(1),
						potecy_param = GetParam(2);
					if (layer_name_param.type() == typeid(std::nullptr_t) ||
						kind_name_param.type() == typeid(std::nullptr_t) ||
						potecy_param.type() == typeid(std::nullptr_t)) [[unlikely]]
					{
						goto lack_error;
					} 
					else if (layer_name_param.type() != typeid(std::wstring) ||
						kind_name_param.type() != typeid(std::wstring) ||
						potecy_param.type() != typeid(int)) [[unlikely]]
					{
						goto type_error;
					}
					layer_name = std::any_cast<std::wstring>(layer_name_param);
					kind_name = std::any_cast<std::wstring>(kind_name_param);
					potency = std::any_cast<int>(potecy_param);
				}

				if (layer_name.empty() || kind_name.empty()) [[unlikely]] {
					goto name_error;
				}
				ReplaceFormat(&layer_name);
				ReplaceFormat(&kind_name);
				Program::Instance().canvas.ApplyFilter(layer_name, kind_name, potency);
				
				return;
			name_error:
				event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"コマンド名: filter/フィルター");
				goto end_of_function;
			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"コマンド名: filter/フィルター");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"コマンド名: filter/フィルター");
				goto end_of_function;
			end_of_function:
				return;
			}
		};

		class DLLCommand : public DynamicCommand {
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
				
			}
		};

		class Detach final : public DLLCommand {
		public:
			inline Detach(const std::wstring& dname) : DLLCommand(dname) {}

			void Execute() final {
				Program::Instance().dll_manager.Detach(dll_name);
				
			}
		};

		// イベント呼出
		DYNAMIC_COMMAND(Call final) {
			std::wstring event_name{};
			inline static error::ErrorContent *event_not_found_error{};
		public:
			DYNAMIC_COMMAND_CONSTRUCTOR(Call) {
				if (event_not_found_error == nullptr)
					event_not_found_error = error::UserErrorHandler::MakeError(
						event::Manager::Instance().error_class,
						L"指定されたイベントが見つかりません。",
						MB_OK | MB_ICONERROR,
						1
					);
			}

			~Call() noexcept final {}

			void Execute() override {
				std::vector<std::any> params{};
				SetAllParams(&params);
				if (MustSearch()) {
					if (params[0].type() == typeid(std::nullptr_t)) [[unlikely]]
						goto lack_error;
					else if (params[0].type() != typeid(std::wstring)) [[unlikely]]
						goto type_error;

					event_name = std::any_cast<std::wstring>(params[0]);
				}
				if (event_name.empty()) [[unlikely]]
					goto name_error;
				else {
					ReplaceFormat(&event_name);
					Event* e = Program::Instance().event_manager.GetEvent(event_name);
					if (e == nullptr)
						goto event_error;
					else if (!params.empty()) {
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
					if (!Program::Instance().event_manager.Call(event_name)) {
						goto event_error;
					}
				}
				return;
			event_error:
				event::Manager::Instance().error_handler.SendLocalError(event_not_found_error, L"コマンド名: call/呼出");
				goto end_of_function;
			name_error:
				event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"コマンド名: call/呼出");
				goto end_of_function;
			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"コマンド名: call/呼出");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"コマンド名: call/呼出");
				goto end_of_function;
			end_of_function:
				return;
			}
		};

		// イベント追加読込
		DYNAMIC_COMMAND(Import final) {
			std::wstring file_name;
		public:
			Import(const std::wstring& File_Name) noexcept : Import(std::vector<std::wstring>{}) {
				file_name = File_Name;
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Import) {}

			~Import() noexcept final {}

			void Execute() override {
				if (MustSearch()) {
					auto name_param = GetParam(0);
					if (name_param.type() == typeid(std::nullptr_t)) [[unlikely]]
						goto lack_error;
					else if (name_param.type() != typeid(std::wstring)) [[unlikely]]
						goto type_error;
					file_name = std::any_cast<std::wstring>(name_param);
				}
				if (file_name.empty()) [[unlikely]]
					goto name_error;

				ReplaceFormat(&file_name);
				Program::Instance().event_manager.ImportEvent(file_name);
				
				return;
			name_error:
				event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"コマンド名: import/取込");
				goto end_of_function;
			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"コマンド名: import/取込");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"コマンド名: import/取込");
				goto end_of_function;
			end_of_function:
				return;
			}
		};

		// イベント読込
		DYNAMIC_COMMAND(Load final) {
			std::wstring file_name;
		public:
			Load(const std::wstring & File_Name) noexcept : Load(std::vector<std::wstring>{}) {
				file_name = File_Name;
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Load) {}

			~Load() noexcept final {}

			void Execute() override {
				if (MustSearch()) {
					auto name_param = GetParam(0);
					if (name_param.type() == typeid(std::nullptr_t)) [[unlikely]]
						goto lack_error;
					else if (name_param.type() != typeid(std::wstring)) [[unlikely]]
						goto type_error;
					file_name = std::any_cast<std::wstring>(name_param);
				}
				if (file_name.empty()) [[unlikely]]
					goto name_error;

				ReplaceFormat(&file_name);
				Program::Instance().event_manager.RequestEvent(file_name);
				
				return;
			name_error:
				event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"コマンド名: load/読込");
				goto end_of_function;
			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"コマンド名: load/読込");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"コマンド名: load/読込");
				goto end_of_function;
			end_of_function:
				return;
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

				inline static error::ErrorContent *layer_kind_not_found_error{};
			public:
				Make(const int Index, const std::wstring & KN, const std::wstring & LN) noexcept : Make(std::vector<std::wstring>{}) {
					index = Index;
					kind_name = KN;
					layer_name = LN;
				}

				DYNAMIC_COMMAND_CONSTRUCTOR(Make) {
					if (layer_kind_not_found_error == nullptr) [[unlikely]]
						layer_kind_not_found_error = error::UserErrorHandler::MakeError(command_error_class, L"レイヤーの種類が見つかりません。", MB_OK | MB_ICONERROR, 2);
				}

				~Make() final {}

				void Execute() final {
					if (MustSearch()) {
						auto index_param = GetParam(0),
							kind_param = GetParam(1),
							layer_param = GetParam(2);
						if (index_param.type() == typeid(std::nullptr_t) ||
							kind_param.type() == typeid(std::nullptr_t) ||
							layer_param.type() == typeid(std::nullptr_t)) [[unlikely]]
						{
							goto lack_error;
						}
						else if (index_param.type() != typeid(int) ||
							kind_param.type() != typeid(std::wstring) ||
							layer_param.type() != typeid(std::wstring)) [[unlikely]]
						{
							goto type_error;
						}
						index = std::any_cast<int>(index_param);
						kind_name = std::any_cast<std::wstring>(kind_param);
						layer_name = std::any_cast<std::wstring>(layer_param);
					}
					if (kind_name.empty() || layer_name.empty()) [[unlikely]]
						goto name_error;
					{
						ReplaceFormat(&kind_name);
						ReplaceFormat(&layer_name);
						auto it = Create.find(kind_name);
						if (it != Create.end()) [[likely]] {
							(Program::Instance().canvas.*it->second)(layer_name, index);
						} else
							goto kind_not_found_error;


					}
					return;
				kind_not_found_error:
					event::Manager::Instance().error_handler.SendLocalError(layer_kind_not_found_error, L"コマンド名: makelayer/レイヤー生成");
					goto end_of_function;
				name_error:
					event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"コマンド名: makelayer/レイヤー生成");
					goto end_of_function;
				lack_error:
					event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"コマンド名: makelayer/レイヤー生成");
					goto end_of_function;
				type_error:
					event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"コマンド名: makelayer/レイヤー生成");
					goto end_of_function;
				end_of_function:
					return;
				}
			};

			// レイヤー変更
			DYNAMIC_COMMAND(Select final) {
				std::wstring layer_name{};
			public:
				Select(const std::wstring & LN) noexcept : Select(std::vector<std::wstring>{}) {
					layer_name = LN;
				}

				DYNAMIC_COMMAND_CONSTRUCTOR(Select) {}

				~Select() final {}

				void Execute() final {
					if (MustSearch()) {
						auto layer_name_param = GetParam(0);
						if (layer_name_param.type() == typeid(std::nullptr_t)) [[unlikely]]
							goto lack_error;
						else if (layer_name_param.type() != typeid(std::wstring)) [[unlikely]]
							goto type_error;

						layer_name = std::any_cast<std::wstring>(layer_name_param);
					}

					if (layer_name.empty()) [[unlikely]]
						goto name_error;

					ReplaceFormat(&layer_name);
					Program::Instance().canvas.SelectLayer(layer_name);
					
					return;
				name_error:
					event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"コマンド名: selectlayer/レイヤー選択");
					goto end_of_function;
				lack_error:
					event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"コマンド名: selectlayer/レイヤー選択");
					goto end_of_function;
				type_error:
					event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"コマンド名: selectlayer/レイヤー選択");
					goto end_of_function;
				end_of_function:
					return;
				}
			};

			// 相対位置レイヤーの基準設定
			DYNAMIC_COMMAND(SetBasis final) {
				std::wstring entity_name{}, layer_name{};
			public:
				SetBasis(const std::wstring & EN, const std::wstring & LN) noexcept : SetBasis(std::vector<std::wstring>{}) {
					entity_name = EN;
					layer_name = LN;
				}

				DYNAMIC_COMMAND_CONSTRUCTOR(SetBasis) {}

				~SetBasis() final {}

				void Execute() final {
					if (MustSearch()) {
						auto entity_name_param = GetParam(0),
							layer_name_param = GetParam(1);

						if (entity_name_param.type() == typeid(std::nullptr_t) || layer_name_param.type() == typeid(std::nullptr_t)) [[unlikely]]
							goto lack_error;
						else if (entity_name_param.type() != typeid(std::wstring) || layer_name_param.type() != typeid(std::wstring)) [[unlikely]]
							goto type_error;

						entity_name = std::any_cast<std::wstring>(GetParam(0));
						layer_name = std::any_cast<std::wstring>(GetParam(1));
					}

					if (entity_name.empty() || layer_name.empty())
						goto name_error;

					{
						ReplaceFormat(&entity_name);
						ReplaceFormat(&layer_name);
						auto ent = Program::Instance().entity_manager.GetEntity(entity_name);
						if (ent == nullptr) [[unlikely]]
							goto entity_error;

						Program::Instance().canvas.SetBasis(ent, layer_name);
					}
					return;
				entity_error:
					event::Manager::Instance().error_handler.SendLocalError(entity_not_found_error, L"コマンド名: setbasis/レイヤー基準");
					goto end_of_function;
				name_error:
					event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"コマンド名: setbasis/レイヤー基準");
					goto end_of_function;
				lack_error:
					event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"コマンド名: setbasis/レイヤー基準");
					goto end_of_function;
				type_error:
					event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"コマンド名: setbasis/レイヤー基準");
					goto end_of_function;
				end_of_function:
					return;
				}
			};

			// レイヤー削除
			DYNAMIC_COMMAND(Delete final) {
				std::wstring name{};
			public:
				Delete(const std::wstring& N) noexcept : Delete(std::vector<std::wstring>{}) {
					name = N;
				}

				DYNAMIC_COMMAND_CONSTRUCTOR(Delete) {}

				~Delete() final {}

				void Execute() final {
					if (MustSearch()) {
						auto name_param = GetParam(0);
						if (name_param.type() == typeid(std::nullptr_t)) [[unlikely]]
							goto lack_error;
						else if (name_param.type() != typeid(std::wstring)) [[unlikely]]
							goto type_error;

						name = std::any_cast<std::wstring>(name_param);
					}
					if (name.empty()) [[unlikely]]
						goto name_error;

					ReplaceFormat(&name);
					if (name == L"__all" || name == L"__全部") {
						for (int i = 0; Program::Instance().canvas.DeleteLayer(i););
						Program::Instance().canvas.CreateAbsoluteLayer(L"デフォルトレイヤー");
						Program::Instance().canvas.SelectLayer(L"デフォルトレイヤー");
					} else {
						Program::Instance().canvas.DeleteLayer(name);
					}
					
					return;
				name_error:
					event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"コマンド名: deletelayer/レイヤー削除");
					goto end_of_function;
				lack_error:
					event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"コマンド名: deletelayer/レイヤー削除");
					goto end_of_function;
				type_error:
					event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"コマンド名: deletelayer/レイヤー削除");
					goto end_of_function;
				end_of_function:
					return;
				}
			};

			DYNAMIC_COMMAND(Show final) {
				std::wstring name{};
			public:
				Show(const std::wstring &N) noexcept : Show(std::vector<std::wstring>{}) {
					name = N;
				}

				DYNAMIC_COMMAND_CONSTRUCTOR(Show) {}

				~Show() final {}

				void Execute() final {
					if (MustSearch()) {
						auto name_param = GetParam(0);
						if (name_param.type() == typeid(std::nullptr_t)) [[unlikely]]
							goto lack_error;
						else if (name_param.type() != typeid(std::wstring)) [[unlikely]]
							goto type_error;

						name = std::any_cast<std::wstring>(name_param);
					}
					if (name.empty()) [[unlikely]]
						goto name_error;
					ReplaceFormat(&name);
					Program::Instance().canvas.Show(name);
					
					return;
				name_error:
					event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"コマンド名: showlayer/レイヤー表示");
					goto end_of_function;
				lack_error:
					event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"コマンド名: showlayer/レイヤー表示");
					goto end_of_function;
				type_error:
					event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"コマンド名: showlayer/レイヤー表示");
					goto end_of_function;
				end_of_function:
					return;
				}
			};

			DYNAMIC_COMMAND(Hide final) {
				std::wstring name{};
			public:
				Hide(const std::wstring &N) noexcept : Hide(std::vector<std::wstring>{}) {
					name = N;
				}

				DYNAMIC_COMMAND_CONSTRUCTOR(Hide) {}

				~Hide() final {}

				void Execute() final {
					if (MustSearch()) {
						auto name_param = GetParam(0);
						if (name_param.type() == typeid(std::nullptr_t)) [[unlikely]]
							goto lack_error;
						else if (name_param.type() != typeid(std::wstring)) [[unlikely]]
							goto type_error;

						name = std::any_cast<std::wstring>(name_param);
					}
					if (name.empty()) [[unlikely]]
						goto name_error;
					ReplaceFormat(&name);
					Program::Instance().canvas.Hide(name);
					
					return;
				name_error:
					event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"コマンド名: hidelayer/レイヤー非表示");
					goto end_of_function;
				lack_error:
					event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"コマンド名: hidelayer/レイヤー非表示");
					goto end_of_function;
				type_error:
					event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"コマンド名: hidelayer/レイヤー非表示");
					goto end_of_function;
				end_of_function:
					return;
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
					if (operation_error_class == nullptr) [[unlikely]]
						operation_error_class = error::UserErrorHandler::MakeErrorClass(L"演算エラー");
					if (assign_error == nullptr) [[unlikely]]
						assign_error = error::UserErrorHandler::MakeError(operation_error_class, L"代入先の変数が存在しません。\n新しくこの変数を作成しますか?", MB_YESNO | MB_ICONERROR, 2);
				}

				std::wstring var_name{};
				std::any value[2]{};

				// 計算に必要な値を展開する。
				// 成功ならtrue、失敗ならfalseを返す。
				[[nodiscard]] bool Extract(const int Length) noexcept {
					if (MustSearch()) {
						var_name = std::any_cast<std::wstring>(GetParam<true>(0));
						for (int i = 0; i < Length; i++) {
							value[i] = GetParam(i + 1);
							// 型チェック
							if (value[i].type() == typeid(std::wstring)) {
								value[i] = std::any_cast<std::wstring>(GetParam<true>(i + 1));
								auto tmp = std::any_cast<std::wstring>(value[i]);
								auto [iv, ip] = ToInt(tmp.c_str());
								auto [fv, fp] = ToDec<Dec>(tmp.c_str());
								if (wcslen(ip) <= 0)
									value[i] = iv;
								else if (wcslen(fp) <= 0)
									value[i] = fv;
							} else if (value[i].type() == typeid(int) || value[i].type() == typeid(Dec)) {
								continue;
							} else if (value[i].type() == typeid(animation::FrameRef)) {
								value[i] = std::ref(
									std::any_cast<animation::FrameRef&>(
										Program::Instance().var_manager.Get<false>(std::any_cast<std::wstring>(GetParam<true>(i + 1)))
									)
								);
							} else if (value[i].type() == typeid(std::nullptr_t)) [[unlikely]]
								goto lack_error;
							else
								goto type_error;
						}
					}
					return true;
				lack_error:
					event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error);
					goto failed_exit;
				type_error:
					event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error);
					goto failed_exit;
				failed_exit:
					return false;
				}
			};

			class Assign final : public MathCommand {
			public:
				using MathCommand::MathCommand;
				~Assign() final {}

				void Execute() final {
					if (Extract(1)) {
						auto var_name = std::any_cast<std::wstring>(GetParam<true>(0));
						auto& v = Program::Instance().var_manager.Get<false>(var_name);
						if (v.type() != typeid(std::nullptr_t)) [[likely]] {
							if (value[0].type() == typeid(int))
								v = std::any_cast<int>(value[0]);
							else if (value[0].type() == typeid(Dec))
								v = std::any_cast<Dec>(value[0]);
							else if (value[0].type() == typeid(std::wstring)) {
								auto txt = std::any_cast<std::wstring>(value[0]);
								ReplaceFormat(&txt);
								v = txt;
							} else if (value[0].type() == typeid(std::reference_wrapper<animation::FrameRef>)) {
								int i = 0;
								v = std::any_cast<std::reference_wrapper<animation::FrameRef>&>(value[0]);
							}
						} else {
							SendAssignError(value[0]);
						}
					}

				}
			};

			// 加算
			class Sum final : public MathCommand {
			public:
				using MathCommand::MathCommand;

				~Sum() final {}

				void Execute() final {
					if (Extract(2)) {
						const bool Is_Only_Int = (value[0].type() == typeid(int) && value[1].type() == typeid(int));

						CalculateValue cal;
						MATH_COMMAND_CALCULATE(+);

						auto* v = &Program::Instance().var_manager.Get<false>(var_name);
						if (v->type() != typeid(std::nullptr_t)) [[likely]] {
							if (Is_Only_Int)
								*v = cal.i;
							else
								*v = cal.d;
						} else {
							SendAssignError(Is_Only_Int, cal);
						}
					}
					
				}
			};

			// 減算
			class Sub final : public MathCommand {
			public:
				using MathCommand::MathCommand;
				~Sub() final {}

				void Execute() final {
					if (Extract(2)) {
						const bool Is_Only_Int = (value[0].type() == typeid(int) && value[1].type() == typeid(int));

						CalculateValue cal;
						MATH_COMMAND_CALCULATE(-);

						auto* v = &Program::Instance().var_manager.Get<false>(var_name);
						if (v->type() != typeid(std::nullptr_t)) [[likely]] {
							if (Is_Only_Int)
								*v = cal.i;
							else
								*v = cal.d;
						} else {
							SendAssignError(Is_Only_Int, cal);
						}
					}
					
				}
			};

			// 乗算
			class Mul final : public MathCommand {
			public:
				using MathCommand::MathCommand;
				~Mul() final {}

				void Execute() final {
					if (Extract(2)) {
						const bool Is_Only_Int = (value[0].type() == typeid(int) && value[1].type() == typeid(int));

						CalculateValue cal;
						MATH_COMMAND_CALCULATE(*);

						auto *v = &Program::Instance().var_manager.Get<false>(var_name);
						if (v->type() != typeid(std::nullptr_t)) [[likely]] {
							if (Is_Only_Int)
								*v = cal.i;
							else
								*v = cal.d;
						} else {
							SendAssignError(Is_Only_Int, cal);
						}
					}
					
				}
			};

			// 徐算
			class Div final : public MathCommand {
			public:
				using MathCommand::MathCommand;
				~Div() final {}

				void Execute() final {
					if (Extract(2)) {
						const bool Is_Only_Int = (value[0].type() == typeid(int) && value[1].type() == typeid(int));

						CalculateValue cal;
						MATH_COMMAND_CALCULATE(/ );

						auto* v = &Program::Instance().var_manager.Get<false>(var_name);
						if (v->type() != typeid(std::nullptr_t)) [[likely]] {
							if (Is_Only_Int)
								*v = cal.i;
							else
								*v = cal.d;
						} else {
							SendAssignError(Is_Only_Int, cal);
						}
					}
					
				}
			};

			class Mod final : public MathCommand {
			public:
				using MathCommand::MathCommand;
				~Mod() final {}

				void Execute() final {
					if (Extract(2)) {
						const bool Is_Only_Int = (value[0].type() == typeid(int) && value[1].type() == typeid(int));

						CalculateValue cal;
						if (Is_Only_Int) {
							cal.i = std::any_cast<int>(value[0]) % std::any_cast<int>(value[1]);
						} else {
							cal.d = 0.0;
							if (value[0].type() == typeid(int)) {
								cal.d = std::any_cast<int>(value[0]);
							} else {
								cal.d = std::any_cast<Dec>(value[0]);
							}

							if (value[1].type() == typeid(int)) {
								cal.d = fmod(cal.d, std::any_cast<int>(value[1]));
							} else {
								cal.d = fmod(cal.d, std::any_cast<int>(value[1]));
							}
						}

						auto* v = &Program::Instance().var_manager.Get<false>(var_name);
						if (v->type() != typeid(std::nullptr_t)) [[likely]] {
							if (Is_Only_Int)
								*v = cal.i;
							else
								*v = cal.d;
						} else {
							SendAssignError(Is_Only_Int, cal);
						}
					}
					
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
						*extra_message += std::any_cast<std::wstring>(GetParam<true>(Index)) + std::wstring(L": ") + converted_name + L'\n';
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
					if (logic_operation_error_class == nullptr) [[unlikely]]
						logic_operation_error_class = error::UserErrorHandler::MakeErrorClass(L"論理演算エラー");
					if (not_integer_error == nullptr) [[unlikely]]
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
					if (Extract(2)) [[likely]] {
						const auto Is_Int = CheckValueType();

						if (Is_Int.first && Is_Int.second) [[likely]] {
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
					}
					
				}
			};

			// ビット論理積
			class And final : public BitCommand {
			public:
				using BitCommand::BitCommand;
				~And() final {}

				void Execute() final {
					if (Extract(2)) [[likely]] {
						const auto Is_Int = CheckValueType();
						if (Is_Int.first && Is_Int.second) [[likely]] {
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
					}
					
				}
			};

			// ビット排他的論理和
			class Xor final : public BitCommand {
			public:
				using BitCommand::BitCommand;
				~Xor() final {}

				void Execute() final {
					if (Extract(2)) [[likely]] {
						const auto Is_Int = CheckValueType();

						if (Is_Int.first && Is_Int.second) [[likely]] {
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
					}
					
				}
			};

			// ビット論理否定
			class Not final : public BitCommand {
			public:
				using BitCommand::BitCommand;
				~Not() final {}

				void Execute() final {
					if (Extract(1)) [[likely]] {
						std::wstring var_name{};
						var_name = std::any_cast<std::wstring>(GetParam<true>(0));

						if (value[0].type() == typeid(int)) [[likely]] {
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
					}
					
				}
			};
		}

		namespace hidden {
			// Entityが関わる全てのManagerを更新
			class UpdateEntity final : public DynamicCommand {
			public:
				UpdateEntity() noexcept {}
				~UpdateEntity() noexcept final {}

				void Execute() final {
					Program::Instance().entity_manager.Update();
					Program::Instance().canvas.Update();

					
				}
			};
		}
	}

	// イベントのコマンド実行クラス
	class Manager::CommandExecuter final {
	public:
		CommandExecuter(const std::list<CommandTree>* commands) {
			if (commands->empty())
				return;

			Program::Instance().var_manager.MakeNew(L"of_state") = 1;
			// ここから、コマンド実行。
			{
				const CommandTree *executing = &commands->front();
				const CommandTree *goal = &commands->back();
				while (executing != nullptr) {
					executing->command->Execute();
					
					// 親が存在せず、自身がゴールでない場合:
					// 自身がcaseコマンドなので、他のofコマンド候補を探す。
					if (executing->parent == nullptr && executing != goal) {
						auto command_iterator = std::find_if(commands->begin(), commands->end(), [executing](const CommandTree& tree) {
							return executing == &tree;
						});

						command_iterator++;
						while (command_iterator != commands->end()) {
							if (command_iterator->word == L"of" || command_iterator->word == L"else") {
								command_iterator->command->Execute();
								auto can_execute = std::any_cast<int>(Program::Instance().var_manager.Get<false>(L"of_state"));
								if (can_execute) {
									// 実行可能なコマンドなので、ここからコマンド実行。
									executing = command_iterator->parent;
									break;
								}
							}
							command_iterator++;
						}
					} else
						executing = executing->parent;
				}
			}
		}
	};

	// イベント生成クラス
	// イベントファイルの解析、コマンドの生成、イベントの設定・生成を行う。
	class EventGenerator final : private Singleton {
	public:
		// 解析クラス
		class Parser final {
			// 構文木
			struct Syntax final {
				std::wstring text{};
				Syntax* parent{};
			};

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
						auto text = std::wstring(L"");

						for (auto c : Sentence) {
							if (!is_string) {
								

								// スペース判定
								if (IsSpace(c) || c == L'\0' || c == L'\n') {
									// 貯めこんだ文字を単語として格納
									if (!text.empty()) {
										PushWord(&context, &text);
									}

									if (c == L'\n' && context.back() != L"\n") {
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
									case L'/':
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

					{
						// 文字の結合部
						// >、=、<、/をそれぞれ結合する。

						std::wstring tmp{};
						Context::iterator it = context.begin();
						while (it != context.end()) {
							it = std::find_if(it, context.end(), [](std::wstring text) {
								return text == L"<" || text == L"=" || text == L">" || text == L"/" || text == L"\n";
							});

							if (it != context.end()) {
								if (*it == L"\n") {
									tmp.clear();
									it++;
									continue;
								}
								tmp += *it;
								if (tmp.size() == 2) {
									if (tmp == L"<=" || tmp == L"==" || tmp == L">=" || tmp == L"//") {
										// 結合
										context.erase(it--);
										context.insert(it, tmp);
										context.erase(it++);
										tmp.clear();
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

			// 構文木解析
			class SyntaxParser final {
				std::list<Syntax> tree{};
				inline static error::ErrorContent *invalid_operator_error{};
			public:
				SyntaxParser(Context* lexical_context) noexcept {
					if (invalid_operator_error == nullptr)
						invalid_operator_error = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"構文に誤りがあります。", MB_OK | MB_ICONERROR, 1);
					
					error::ErrorContent *error_occurred{};

					tree.push_front({});
					tree.push_front(Syntax{ .parent = &tree.front() });
					auto operator_iterator = tree.end();
					auto tree_iterator = tree.begin();

					while (!lexical_context->empty() && error_occurred == nullptr) {
						auto str = lexical_context->front();
						switch (str[0]) {
							case L'{':
							case L'}':
							case L'[':
							case L']':
							case L'<':
							case L'>':
							case L'(':
							case L')':
							{
								if (str.size() > 1)
									goto not_operator;
								auto old = tree_iterator;
								if (operator_iterator == tree.end()) {
									tree_iterator++;
									operator_iterator = tree_iterator;
								} else
									tree_iterator = operator_iterator;
								
								tree_iterator->text += str[0];
								// 
								if (tree_iterator->text.size() == 2 && tree_iterator->text != L"()" && tree_iterator->text != L"<>" && tree_iterator->text != L"[]" && tree_iterator->text != L"{}") {
									error_occurred = invalid_operator_error;
									event::Manager::Instance().error_handler.SendLocalError(error_occurred, L"文: " + tree_iterator->text);
								}
								tree_iterator = old;
								break;
							}
							case L'\n':
							{
								if (operator_iterator != tree.end()) {
									if (operator_iterator->text != L"{") {
										operator_iterator = tree.end();
										tree_iterator->parent = nullptr;
										tree.push_front(Syntax{ .parent = &(*tree_iterator) });
									} else {
										// コマンド文は複数行の文で構成される。
										// ({}を親とする構文木が複数存在する。)
										// 
										// その為、常に{}を親として設定。
										tree_iterator->parent = &(*operator_iterator);
									}
									tree_iterator = tree.begin();
								}
								break;
							}
							case L',':
								// カンマは無視。
								break;
							default:
							not_operator:
								if (str != L"//") {
									tree_iterator->text = str;
									tree.push_front(Syntax{ .parent = &(*tree_iterator) });
									tree_iterator = tree.begin();
									break;
								} else {
									while (lexical_context->front() != L"\n") {
										lexical_context->pop_front();
									}
									continue;
								}
						}
						lexical_context->pop_front();
					}
					tree.pop_front();
				}

				auto& Result() noexcept {
					return tree;
				}
			};

			// 意味解析
			class SemanticParser final {
				std::list<Syntax*> visited{}, queue{};
				std::list<std::wstring> stack{};

				std::unordered_map<std::wstring, Event> parsing_events{};

				CommandTree* parent = nullptr;
				WorldVector origin[2]{ { -1, -1 }, { -1, -1 } };
				TriggerType trigger_type = TriggerType::Invalid;
				std::wstring event_name;
				std::vector<std::wstring> params{};
				std::list<CommandTree> commands{};
				std::vector<std::wstring> command_parameters{};
				std::unordered_map<std::wstring, GenerateFunc> words{};

				error::ErrorContent *error_occurred{};
				inline static error::ErrorContent 
					*empty_name_error{}, 
					*invalid_trigger_type_warning{},
					*command_not_found_error{},
					*lack_of_parameters_error{},
					*too_many_parameters_warning{},
					*already_new_event_name_defined_error{},
					*already_new_trigger_type_defined_error{};
			public:
				SemanticParser() noexcept {
					if (empty_name_error == nullptr)
						empty_name_error = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"イベント名を空にすることはできません。", MB_OK | MB_ICONERROR, 1);
					if (invalid_trigger_type_warning == nullptr)
						invalid_trigger_type_warning = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"不明なイベント発生タイプが指定されたため、発生無しを設定しました。", MB_OK | MB_ICONWARNING, 4);
					if (command_not_found_error == nullptr)
						command_not_found_error = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"指定されたコマンドが見つかりません。", MB_OK | MB_ICONERROR, 2);
					if (lack_of_parameters_error == nullptr)
						lack_of_parameters_error = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"コマンドの引数が足りない為、生成できません。", MB_OK | MB_ICONERROR, 2);
					if (too_many_parameters_warning == nullptr)
						too_many_parameters_warning = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"コマンドの引数が多すぎる為、余分なものは廃棄しました。", MB_OK | MB_ICONWARNING, 3);
					if (already_new_event_name_defined_error == nullptr)
						already_new_event_name_defined_error = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"既にイベント名が設定されています。", MB_OK | MB_ICONERROR, 1);
					if (already_new_trigger_type_defined_error == nullptr)
						already_new_trigger_type_defined_error = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"既に発生タイプが指定されています。", MB_OK | MB_ICONERROR, 1);
			
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
								} else
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
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
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
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
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

					words[L"freeze"] =
						words[L"凍結"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
					{
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Name, Name_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								if (Default_ProgramInterface.IsStringType(Name_Type)) {
									return std::make_unique<command::entity::Freeze>(Name);
								} else {
									return std::make_unique<command::entity::Freeze>(params);
								}
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

					words[L"defrost"] =
						words[L"解凍"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
					{
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Name, Name_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								if (Default_ProgramInterface.IsStringType(Name_Type)) {
									return std::make_unique<command::entity::Defrost>(Name);
								} else {
									return std::make_unique<command::entity::Defrost>(params);
								}
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

					words[L"animation"] =
						words[L"アニメ"] = [](const std::vector<std::wstring>& params) -> KeywordInfo 
					{
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Var_Name, Var_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								if (Default_ProgramInterface.IsNoType(Var_Type))
									return std::make_unique<command::Animation>(Var_Name);
								else
									return nullptr;
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

					words[L"addframe"] =
						words[L"フレーム追加"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
					{
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Var_Name, Var_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								const auto [Path, Path_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
								if (Default_ProgramInterface.IsNoType(Var_Type) && Default_ProgramInterface.IsStringType(Path_Type))
									return std::make_unique<command::AddFrame>(Var_Name, Path);
								else
									return std::make_unique<command::AddFrame>(params);
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

					words[L"nextframe"] =
						words[L"次フレーム"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
					{
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Var_Name, Var_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								if (Default_ProgramInterface.IsNoType(Var_Type))
									return std::make_unique<command::NextFrame>(Var_Name);
								else
									return nullptr;
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

					words[L"backframe"] =
						words[L"前フレーム"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
					{
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Var_Name, Var_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								if (Default_ProgramInterface.IsNoType(Var_Type))
									return std::make_unique<command::BackFrame>(Var_Name);
								else
									return nullptr;
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

					words[L"capture"] =
						words[L"画像切取"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
					{
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Var_Name, Var_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								const auto [File_Path, Path_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
								const auto [Vec_X, VX_Type] = Default_ProgramInterface.GetParamInfo(params[2]);
								const auto [Vec_Y, VY_Type] = Default_ProgramInterface.GetParamInfo(params[3]);
								const auto [Len_X, LX_Type] = Default_ProgramInterface.GetParamInfo(params[4]);
								const auto [Len_Y, LY_Type] = Default_ProgramInterface.GetParamInfo(params[5]);

								if (Default_ProgramInterface.IsNoType(Var_Type) &&
									Default_ProgramInterface.IsStringType(Path_Type) &&
									Default_ProgramInterface.IsNumberType(VX_Type) &&
									Default_ProgramInterface.IsNumberType(VY_Type) &&
									Default_ProgramInterface.IsNumberType(LX_Type) &&
									Default_ProgramInterface.IsNumberType(LY_Type))
								{
									return std::make_unique<command::Capture>(Var_Name,
										File_Path,
										ScreenVector{ ToInt(Vec_X.c_str(), nullptr), ToInt(Vec_Y.c_str(), nullptr) },
										ScreenVector{ ToInt(Len_X.c_str(), nullptr), ToInt(Len_Y.c_str(), nullptr) });
								} else {
									return std::make_unique<command::Capture>(params);
								}
							},
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
									case 1:
									case 2:
									case 3:
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
								} else
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

					// DLLアタッチ
					words[L"attach"] =
						words[L"接続"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								const auto [Var, Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								return (Default_ProgramInterface.IsStringType(Type) ?
									std::make_unique<command::Attach>(Var) : nullptr);
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
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
								switch (params.size()) {
									case 0:
										return KeywordInfo::ParamResult::Lack;
									default:
										return KeywordInfo::ParamResult::Medium;
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
									auto [mode, mode_type] = Default_ProgramInterface.GetParamInfo(params[0]);
									const auto [Var, Type] = Default_ProgramInterface.GetParamInfo(params[1]);

									mode = mode.substr(mode.find(L'.') + 1);
									if (Default_ProgramInterface.IsNumberType(Type)) {
										auto [iv, ip] = ToInt(Var.c_str());
										auto [fv, fp] = ToDec<Dec>(Var.c_str());
										if (wcslen(ip) <= 0)
											return std::make_unique<command::Of>(mode, iv);
										else
											return std::make_unique<command::Of>(mode, fv);
									} else if (Default_ProgramInterface.IsStringType(Type)) {
										return std::make_unique<command::Of>(mode, Var);
									} else {
										return std::make_unique<command::Of>(params);
									}
								}
							},
							.checkParamState = [params]() -> KeywordInfo::ParamResult {
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

					words[L"else"] =
						words[L"以外"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								return std::make_unique<command::Else>();
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

					words[L"endcase"] =
						words[L"分岐終了"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								return std::make_unique<command::EndCase>();
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

					words[L"mod"] = words[L"剰余"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								return std::make_unique<command::math::Mod>(params);
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
				}

				SemanticParser(std::list<Syntax> *syntax) noexcept : SemanticParser() {
					// 左: 現在のcaseのイテレータ
					// 右: elseが存在するか否か。
					std::list<std::pair<decltype(commands)::iterator, bool>> case_stack{};
					// コマンド文を発見したか否か。
					bool found_command_sentence = false;

					for (auto it = syntax->begin(); it != syntax->end(); it++) {
						if (std::find(visited.begin(), visited.end(), &(*it)) != visited.end()) {
							continue;
						}
						auto route = &(*it);
						while (route != nullptr) {
							visited.push_back(route);
							queue.push_back(route);
							route = route->parent;
						}

						while (!queue.empty() && error_occurred == nullptr) {
							stack.push_front(queue.front()->text);
							auto& op = stack.front();
							if (op == L"[]") {
								stack.pop_front();
								if (event_name.empty()) {
									switch (stack.size()) {
										case 0:
											error_occurred = empty_name_error;
											event::Manager::Instance().error_handler.SendLocalError(error_occurred);
											break;
										case 1:
											event_name = stack.front();
											stack.pop_front();
											break;
										default:
											std::wstring candidate_name{};
											while (!stack.empty()) {
												candidate_name += stack.front() + L' ';
												stack.pop_front();
											}
											candidate_name.pop_back();
											event_name = candidate_name;
											break;
									}
								} else {
									error_occurred = already_new_event_name_defined_error;
									event::Manager::Instance().error_handler.SendLocalError(error_occurred, (L"多重定義として判定されたイベント名: " + stack.front()));
								}
							} else if (op == L"<>") {
								stack.pop_front();
								if (trigger_type == TriggerType::Invalid) {
									auto word = stack.front();
									if (word == L"a") {
										trigger_type = TriggerType::Auto;
									} else if (word == L"t") {
										trigger_type = TriggerType::Trigger;
									} else if (word == L"n") {
										trigger_type = TriggerType::None;
									} else if (word == L"l") {
										trigger_type = TriggerType::Load;
									} else {
										trigger_type = TriggerType::None;
										event::Manager::Instance().error_handler.SendLocalError(invalid_trigger_type_warning);
									}
									stack.pop_front();

									for (int i = 0; !stack.empty(); ) {
										if (stack.front() == L"~")
											i = 1;
										else {
											origin[(origin[0][1] > -1)][i] = ToDec<Dec>(stack.front().c_str(), nullptr);
											i = 0;
										}
										stack.pop_front();
									}
								} else {
									error_occurred = already_new_trigger_type_defined_error;
									event::Manager::Instance().error_handler.SendLocalError(error_occurred, (L"多重定義として判定された発生タイプ名: " + stack.front()));
								}
							} else if (op == L"()") {
								stack.pop_front();
								while (!stack.empty()) {
									params.push_back(stack.front());
									stack.pop_front();
								}
							} else if (op == L"{}") {
								found_command_sentence = true;
								stack.pop_front();
								std::wstring command_name{};
								GenerateFunc generator_candidate{};
								while (!stack.empty()) {
									auto it = words.find(stack.front());
									if (it != words.end()) {
										command_name = stack.front();
										generator_candidate = it->second;
									} else {
										command_parameters.push_back(stack.front());
									}
									stack.pop_front();
								}

								if (generator_candidate != nullptr) {
									auto generator = generator_candidate(command_parameters);
									switch (generator.checkParamState()) {
										case KeywordInfo::ParamResult::Lack:
											error_occurred = lack_of_parameters_error;
											event::Manager::Instance().error_handler.SendLocalError(
												error_occurred,
												(L"コマンド名: " + command_name).c_str()
											);
											break;
										case KeywordInfo::ParamResult::Medium:
										case KeywordInfo::ParamResult::Maximum:
											if (command_name == L"case") {
												if (!case_stack.front().second) {
													// elseが無い為、暗黙的に空のelseを挿入。
													commands.push_front(CommandTree{
														.command = words[L"else"]({}).Result(),
														.word = L"else",
														.parent = parent
													});
													parent = &commands.front();
												}
												case_stack.pop_front();
											} else if (command_name == L"endcase") {
												// else検査開始。
												case_stack.push_front({ commands.begin(), false });
											} else if (command_name == L"else") {
												// elseが存在するのでtrue。
												case_stack.front().second = true;
											}

											commands.push_front(CommandTree{
												.command = generator.Result(),
												.word = command_name,
												.parent = parent
											});
											parent = &commands.front();
											break;
										case KeywordInfo::ParamResult::Excess:
											event::Manager::Instance().error_handler.SendLocalError(
												too_many_parameters_warning,
												(L"コマンド名: " + command_name).c_str()
											);
											break;
									}
								} else {
									if (!command_name.empty()) {
										error_occurred = command_not_found_error;
										event::Manager::Instance().error_handler.SendLocalError(error_occurred, L"コマンド名: " + command_name);
									}
								}
								command_parameters.clear();
							}
							queue.pop_front();
						}
						stack.clear();

						if (event::Manager::Instance().error_handler.ShowLocalError(4)) {
							break;
						}

						if (!event_name.empty() && found_command_sentence) {
							parsing_events[event_name].trigger_type = trigger_type;
							parsing_events[event_name].param_names = std::move(params);
							for (int i = 0; i < 2; i++)
								parsing_events[event_name].origin[i] = origin[i];

							parsing_events[event_name].commands = std::move(commands);
							trigger_type = TriggerType::Invalid;
							commands.clear();
							params.clear();
							event_name.clear();
							parent = nullptr;
							found_command_sentence = false;
						}
					}


				}

				auto& Result() {
					return parsing_events;
				}
			};

			// 最適化機構
			class Optimizer final {
			public:
				Optimizer() = default;

				// 出来る限りの最適化。
				Optimizer(std::list<CommandTree> *commands) noexcept {
					// 適切な道の形成
					SortOfElse(commands);
				}

				// of/elseコマンドをstd::list内のcaseの直後に並べ直す。
				void SortOfElse(std::list<CommandTree> *commands) noexcept {
					auto it = commands->begin();
					std::list<decltype(it)> case_pos{};
					// 各caseに対して、ofコマンドを挿入しなおした回数を保存するリスト。
					std::list<int> inserted_counts{};
					while (it != commands->end()) {
						if (it->word == L"case") {
							case_pos.push_front(it);
							case_pos.front()->parent = nullptr;
							inserted_counts.push_front(0);
						} else if (it->word == L"of" || it->word == L"else") {
							// 親の修正
							{
								// 多重分岐避け
								std::list<CommandTree*> ignore_case{};
								CommandTree *of_parent{};
								auto parent = &(*it);
								while (parent->parent != nullptr) {
									if (parent->parent->word == L"case") {
										ignore_case.push_back(parent->parent);
									} else if ((parent->parent->word == L"of" || parent->parent->word == L"else") && of_parent == nullptr && ignore_case.empty()) {
										// 次のコマンドがofコマンドまたはelseコマンド:
										// 分岐終了なので変更対象に設定。
										of_parent = parent;
									} else if (parent->parent->word == L"endcase" && of_parent != nullptr) {
										if (ignore_case.empty()) {
											// 変更対象をendcaseコマンドに繋げる。
											of_parent->parent = parent->parent;
											break;
										} else {
											ignore_case.pop_back();
										}
									}
									parent = parent->parent;
								}
							}
							inserted_counts.front()++;
							// ofコマンドの位置をcaseの直後に設定。
							commands->insert(std::next(case_pos.front(), inserted_counts.front()), std::move(*it));
							it = commands->erase(it);
							continue;
						} else if (it->word == L"endcase") {
							case_pos.pop_front();
							inserted_counts.pop_front();
						}
						it++;
					}
				}
			};

			std::unordered_map<std::wstring, Event> events;
			std::wstring ename;

			error::ErrorContent *parser_abortion_error{};
		public:
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

			Parser() {
				if (parser_abortion_error == nullptr)
					parser_abortion_error = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"イベント解析を強制終了しました。", MB_OK | MB_ICONERROR, 1);
			}

			Parser(const std::wstring& Sentence) noexcept : Parser() {
				auto context = ParseBasic(Sentence);
				bool aborted = false;
				auto tree = std::move(SyntaxParser(&context).Result());
				events = std::move(SemanticParser(&tree).Result());
				for (auto& e : events) {
					Optimizer(&e.second.commands);
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
			ReadWTextFile(Path.c_str(), &plain_text, &cc);
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

		[[nodiscard]] auto Result() noexcept {
			return std::move(events);
		}
	private:
		std::unordered_map<std::wstring, Event> events;
	};

	// 
	void Manager::ConditionManager::SetTarget(std::any& tv) {
		target_value = tv;
	}

	// 条件式を評価する
	bool Manager::ConditionManager::Evalute(const std::wstring& Mode, const std::any& Right_Value) noexcept {
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
		return can_execute;
	}

	void Manager::ConditionManager::FreeCase() {
		target_value.reset();
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
		OnLoad();
	}

	void Manager::RequestEvent(const std::wstring& Path) noexcept {
		requesting_path = Path;
	}

	void Manager::ImportEvent(const std::wstring& Path) noexcept {
		auto additional = std::move(GenerateEvent(Path));
		for (auto& e : additional) {
			events[e.first] = std::move(e.second);
		}
		OnLoad();
	}

	void Manager::OnLoad() noexcept {
		for (auto& e : events) {
			if (e.second.trigger_type == TriggerType::Load) {
				e.second.trigger_type = TriggerType::None;
				Call(e.first);
			}
		}
	}

	void Manager::Update() noexcept {
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

		error_handler.ShowLocalError(4);

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

	bool Manager::Call(const std::wstring& EName) noexcept {
		auto candidate = events.find(EName);
		if (candidate != events.end()) {
			auto event_name = std::any_cast<std::wstring>(Program::Instance().var_manager.Get<false>(variable::Executing_Event_Name));	
			Program::Instance().var_manager.Get<false>(variable::Executing_Event_Name) = (event_name += std::wstring(EName) + L"\n");
			CommandExecuter cmd_executer(&candidate->second.commands);
			event_name.erase(event_name.find(EName + L"\n"));
			Program::Instance().var_manager.Get<false>(variable::Executing_Event_Name) = event_name;
			return true;
		}
		return false;
	}

	void Manager::NewCaseTarget(std::any tv) {
		condition_manager.push_back(ConditionManager(tv));
		condition_current = condition_manager.end() - 1;
	}

	bool Manager::Evalute(const std::wstring& Mode, const std::any& Right_Value) {
		if (!condition_manager.empty() && condition_current != condition_manager.end())
			return condition_current->Evalute(Mode, Right_Value);
		else
			return false;
	}

	void Manager::FreeCase() {
		condition_current->FreeCase();
		condition_manager.pop_back();
		if (!condition_manager.empty())
			condition_current = condition_manager.end() - 1;
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

	void EventEditor::MakeNewEvent(const std::wstring& Event_Name) {
		Program::Instance().event_manager.MakeEmptyEvent(Event_Name);
		SetTarget(Event_Name);
	}

	bool EventEditor::IsEditing() const noexcept {
		return (targeting != nullptr);
	}

	bool EventEditor::IsEditing(const std::wstring& Event_Name) const noexcept {
		return (IsEditing() && Program::Instance().event_manager.GetEvent(Event_Name) == targeting);
	}

	void EventEditor::AddCommand(const std::wstring& Command_Sentence, const int Index) {
		auto commands = std::move(EventGenerator::Parser(
			L"[ダミー]\n<n>\n()\n" + Command_Sentence
		).Result().begin()->second.commands);

		for (auto& cmd : commands) {
			if (Index < 0 && !targeting->commands.empty())
				targeting->commands.insert(std::next(targeting->commands.end(), Index + 1), std::move(cmd));
			else {
				if (targeting->commands.empty())
					targeting->commands.insert(targeting->commands.begin(), std::move(cmd));
				else
					targeting->commands.insert(std::next(targeting->commands.begin(), Index), std::move(cmd));
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
		targeting->trigger_type = EventGenerator::Parser(
			std::wstring(L"[ダミー]\n") +
			L'<' + TSentence + L'>' +
			std::wstring(L"\n()\n{}")
		).Result().begin()->second.trigger_type;
	}

	void command::Alias::Execute() {
		
	}

	void command::Bind::Execute() {
		static uint64_t count = 0;
		EventGenerator::Parser parser;
		const auto Event_Name = L"__キーイベント_" + std::to_wstring(count++);
		auto editor = Program::Instance().MakeEventEditor();
		editor->MakeNewEvent(Event_Name);
		editor->ChangeTriggerType(L"n");
		editor->ChangeRange({ 0, 0 }, { 0, 0 });
		editor->AddCommand(L"{\n" + command_sentence + L"\n}", 0);
		Program::Instance().FreeEventEditor(editor);


		Program::Instance().engine.BindKey(key_name, [Event_Name]() {
			Program::Instance().event_manager.Call(Event_Name);
		});
		
	}
}