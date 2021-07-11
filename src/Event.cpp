#include "Event.hpp"

#include "Canvas.hpp"

#include "Engine.hpp"

#include <queue>
#include <chrono>
#include <forward_list>
#include <optional>

#define DYNAMIC_COMMAND(NAME) class NAME : public DynamicCommand
#define DYNAMIC_COMMAND_CONSTRUCTOR(NAME) NAME(const std::vector<std::wstring>& Param) : DynamicCommand(Param)

namespace karapo::event {
	namespace {
		// �����񒆂�{}�Ŏw�肳�ꂽ�����ɕϐ��̒l�Œu������B
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
					Program::Instance().var_manager.Get(variable::Executing_Event_Name)
					);
				executing_name.pop_back();
				if (auto pos = executing_name.rfind(L'\n'); pos != executing_name.npos)
					executing_name = executing_name.substr(pos);
				if (auto pos = executing_name.find(L'\n'); pos != executing_name.npos)
					executing_name.erase(executing_name.begin());
				auto var = Program::Instance().var_manager.Get(executing_name + L'.' + str);
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
		// Entity����n�R�}���h
		class EntityCommand : public Command {
			std::wstring entity_name;
		protected:
			std::shared_ptr<karapo::Entity> GetEntity() const noexcept {
				return Program::Instance().entity_manager.GetEntity(entity_name.c_str());
			}
		};

		class DynamicCommand : public Command {
			using ReferenceAny = std::reference_wrapper<std::any>;

			// �ǂݍ��ޗ\��̈������B
			std::vector<std::wstring> param_names{};
		protected:
			// �R�}���h�G���[�N���X�B
			inline static error::ErrorClass *command_error_class{};
			// �G���[�Q
			inline static error::ErrorContent *incorrect_type_error{},		// �^�s��v�G���[
				*lack_of_parameters_error{},										// �����s���G���[
				*empty_name_error{},													// �󖼃G���[
				*entity_not_found_error{};											// Entity��������Ȃ��G���[

			DynamicCommand() noexcept {
				if (command_error_class == nullptr) [[unlikely]]
					command_error_class = error::UserErrorHandler::MakeErrorClass(L"�R�}���h�G���[");
				if (incorrect_type_error == nullptr) [[unlikely]]
					incorrect_type_error = error::UserErrorHandler::MakeError(command_error_class, L"�����̌^���s�K�؂ł��B", MB_OK | MB_ICONERROR, 1);
				if (lack_of_parameters_error == nullptr) [[unlikely]]
					lack_of_parameters_error = error::UserErrorHandler::MakeError(command_error_class, L"����������Ȃ��ׁA�R�}���h�����s�ł��܂���B", MB_OK | MB_ICONERROR, 1);
				if (empty_name_error == nullptr) [[unlikely]]
					empty_name_error = error::UserErrorHandler::MakeError(command_error_class, L"���O����ɂ��邱�Ƃ͂ł��܂���B", MB_OK | MB_ICONERROR, 2);
				if (entity_not_found_error == nullptr) [[unlikely]]
					entity_not_found_error = error::UserErrorHandler::MakeError(command_error_class, L"Entity��������܂���B", MB_OK | MB_ICONERROR, 2);
			}

			DynamicCommand(const decltype(param_names)& Param) noexcept : DynamicCommand() {
				param_names = Param;
			}

			// �������R�}���h���s���ɓǂݍ��ޕK�v�����邩�ۂ��B
			bool MustSearch() const noexcept {
				return !param_names.empty();
			}

			// �������擾����B
			template<const bool Get_Param_Name = false>
			std::any GetParam(const int Index) const noexcept {
				if (Index < 0 || Index >= param_names.size())
					return nullptr;

				auto [var, type] = Default_ProgramInterface.GetParamInfo(param_names[Index]);
				if constexpr (!Get_Param_Name) {
					// ������K�؂Ȓl�ɕϊ�������ŕԂ������B
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
						auto event_name = std::any_cast<std::wstring>(Program::Instance().var_manager.Get(variable::Executing_Event_Name));
						event_name = event_name.substr(0, event_name.size() - 1);
						if (auto pos = event_name.rfind(L'\n'); pos != std::wstring::npos)
							event_name = event_name.substr(pos);
						if (auto pos = event_name.find(L'\n'); pos != std::wstring::npos)
							event_name.erase(event_name.begin());

						return Program::Instance().var_manager.Get(param_names[Index]);
					}
				} else {
					// ���������擾���鏈���B
					if (Default_ProgramInterface.IsStringType(type) || Default_ProgramInterface.IsNumberType(type))
						return var;
					else
						return param_names[Index];
				}
			}

			// ������z��ɒ��ڐݒ肷��B
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

			// Value��T�̎Q�ƌ^�̒l��ێ����邩��Ԃ��B
			template<typename T>
			static bool IsReferenceType(const std::any& Value) noexcept {
				return (Value.type() == typeid(ReferenceAny) && std::any_cast<const ReferenceAny&>(Value).get().type() == typeid(T));
			}

			// Value��T�܂���T�̎Q�ƌ^�̒l��ێ����邩��Ԃ��B
			template<typename T>
			static bool IsSameType(const std::any& Value) noexcept {
				return (Value.type() == typeid(T) || IsReferenceType<T>(Value));
			}

			template<typename T>
			static T& GetReferencedValue(const std::any& Value) noexcept {
				return std::any_cast<T&>(std::any_cast<ReferenceAny>(Value).get());
			}
		};

		// �ϐ�
		DYNAMIC_COMMAND(Variable) {
		protected:
			std::wstring varname;
			std::any value;
		public:
			Variable(const std::wstring & VName, const std::wstring & Any_Value) noexcept : Variable(std::vector<std::wstring>{}) {
				varname = VName;
				if (!Any_Value.empty()) {
					auto [iv, ir] = ToInt<int>(Any_Value.c_str());
					auto [fv, fr] = ToDec<Dec>(Any_Value.c_str());

					if (wcslen(ir) <= 0) {
						value = iv;
					} else if (wcslen(fr) <= 0) {
						value = fv;
					} else {
						goto string_value;
					}
				} else {
				string_value:
					value = Program::Instance().var_manager.Get(VName);
					if (value.type() == typeid(std::nullptr_t)) {
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
					} else {
						if (varname[0] == L'&') {
							value = std::ref(Program::Instance().var_manager.Get(std::any_cast<std::wstring>(GetParam<true>(1))));
						}
					}
				}
				return true;

				// �ϐ�����������Ă��Ȃ��ꍇ�̃G���[(=������0��)�B
			noname_error:
				// �����l���w�肳��Ă��Ȃ��ꍇ�̃G���[(=������1��)�B 
			novalue_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: var/�ϐ�");
				goto return_failed;
			return_failed:
				return false;
			}

			void Execute(const std::wstring & Var_Name) noexcept {
				if (Var_Name[0] == L'&') {
					Program::Instance().var_manager.MakeNew(Var_Name.substr(1)) = value;
				} else
					Program::Instance().var_manager.MakeNew(Var_Name) = value;
			}
			void Execute() override {
				if (CheckParams()) {
					auto event_name = std::any_cast<std::wstring>(Program::Instance().var_manager.Get(variable::Executing_Event_Name));
					event_name.pop_back();
					auto pos = event_name.rfind(L'\n');
					if (pos != std::wstring::npos)
						event_name = event_name.substr(pos + 1);

					if (varname[0] == L'&')
						Execute(L'&' + event_name + std::wstring(L"@") + varname.substr(1));
					else
						Execute(event_name + std::wstring(L"@") + varname);
				}
			}
		};

		// �O���[�o���ϐ�
		class Global final : public Variable {
		public:
			using Variable::Variable;

			void Execute() override {
				if (CheckParams()) {
					Variable::Execute(varname);
				}
			}
		};

		// ������̓��e����ϐ����擾�B
		DYNAMIC_COMMAND(AsVar final) {
			std::wstring variable_name{}, string_content{};
		public:
			AsVar(const std::wstring & Variable_Name, const std::wstring & String_Content) noexcept : AsVar({}) {
				variable_name = Variable_Name;
				string_content = String_Content;
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(AsVar) {}

			void Execute() final {
				if (MustSearch()) {
					auto variable_name_param = GetParam<true>(0),
						string_content_param = GetParam(1);

					if (variable_name_param.type() == typeid(std::nullptr_t) || string_content_param.type() == typeid(std::nullptr_t)) {
						goto lack_error;
					} else if (!IsSameType<std::wstring>(variable_name_param) || !IsSameType<std::wstring>(string_content_param)) {
						goto type_error;
					}
					variable_name = (!IsReferenceType<std::wstring>(variable_name_param) ? std::any_cast<std::wstring&>(variable_name_param) : GetReferencedValue<std::wstring>(variable_name_param));
					string_content = (!IsReferenceType<std::wstring>(string_content_param) ? std::any_cast<std::wstring&>(string_content_param) : GetReferencedValue<std::wstring>(string_content_param));
				}
				
				if (variable_name.front() == L'&') {
					Program::Instance().var_manager.MakeNew(variable_name.substr(1)) = std::ref(Program::Instance().var_manager.Get(string_content));
				} else {
					Program::Instance().var_manager.MakeNew(variable_name) = Program::Instance().var_manager.Get(string_content);
				}
				goto end_of_function;
			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: asvar/�ϐ��擾");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: asvar/�ϐ��擾");
				goto end_of_function;
			end_of_function:
				return;
			}
		};

		// �ϐ�/�֐�/Entity�̑��݊m�F
		DYNAMIC_COMMAND(Exist final) {
			inline static error::ErrorContent *assign_error{};
		public:
			DYNAMIC_COMMAND_CONSTRUCTOR(Exist) {
				if (assign_error == nullptr) [[unlikely]]
					assign_error = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"�����̕ϐ������݂��܂���B\n�V�������̕ϐ����쐬���܂���?", MB_YESNO | MB_ICONERROR, 2);
			}

			~Exist() noexcept final {}

			void Execute() final {
				std::wstring name[2]{ {}, {} };
				for (int i = 0; i < 2; i++)
					name[i] = std::any_cast<std::wstring>(GetParam<true>(i));

				int result = 0;
				if (Program::Instance().var_manager.Get(name[0]).type() != typeid(std::nullptr_t)) {
					result |= std::any_cast<int>(Program::Instance().var_manager.Get(L"__�ϐ�����"));
				}
				if (Program::Instance().event_manager.GetEvent(name[0]) != nullptr) {
					result |= std::any_cast<int>(Program::Instance().var_manager.Get(L"__�C�x���g����"));
				}
				if (Program::Instance().entity_manager.GetEntity(name[0]) != nullptr) {
					result |= std::any_cast<int>(Program::Instance().var_manager.Get(L"__�L��������"));
				}

				auto *v = &Program::Instance().var_manager.Get(name[1]);
				if (v->type() != typeid(std::nullptr_t)) {
					*v = static_cast<int>(result);
				} else {
					event::Manager::Instance().error_handler.SendLocalError(assign_error, L"�ϐ�: " + name[1], [](const int Result) {
						switch (Result) {
							case IDYES:
							{
								auto& var = Program::Instance().var_manager.Get(L"__assignable");
								auto& value = Program::Instance().var_manager.Get(L"__calculated");
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

		// �����Ώېݒ�
		DYNAMIC_COMMAND(Case final) {
			std::any value;
		public:
			Case(const std::wstring & Param) noexcept : Case(std::vector<std::wstring>{}) {
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

		// ������
		DYNAMIC_COMMAND(Of final) {
			std::wstring mode;
			std::any value;
		public:
			Of(const std::wstring & Condition_Sentence, const std::any & V) noexcept : Of(std::vector<std::wstring>{}) {
				mode = Condition_Sentence;
				value = V;
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Of) {}

			~Of() noexcept final {}

			void Execute() override {
				if (MustSearch()) {
					mode = std::any_cast<std::wstring>(GetParam<true>(0));
					auto vname = std::any_cast<std::wstring>(GetParam<true>(1));
					value = Program::Instance().var_manager.Get(vname);
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
					} else if (IsReferenceType<int>(value)) {
						value = GetReferencedValue<int>(value);
					} else if (IsReferenceType<Dec>(value)) {
						value = GetReferencedValue<Dec>(value);
					} else if (IsReferenceType<std::wstring>(value)) {
						value = GetReferencedValue<std::wstring>(value);
					}
				}
				Program::Instance().var_manager.Get(L"of_state") = (int)Program::Instance().event_manager.Evalute(mode, value);
			}
		};

		// ������(�S�ĊY���Ȃ��̏ꍇ)
		DYNAMIC_COMMAND(Else final) {
		public:
			Else() noexcept : Else(std::vector<std::wstring>{}) {}

			DYNAMIC_COMMAND_CONSTRUCTOR(Else) {}

			~Else() noexcept final {}

			void Execute() override {
				Program::Instance().var_manager.Get(L"of_state") = (int)!std::any_cast<int>(Program::Instance().var_manager.Get(L"of_state"));
			}
		};

		// �����I��
		DYNAMIC_COMMAND(EndCase final) {
		public:
			EndCase() noexcept : EndCase(std::vector<std::wstring>{}) {}
			~EndCase() final {}

			DYNAMIC_COMMAND_CONSTRUCTOR(EndCase) {}

			void Execute() final {
				Program::Instance().event_manager.FreeCase();
			}
		};

		// �摜��ǂݍ��݁A�\��������B
		DYNAMIC_COMMAND(Image) {
			std::shared_ptr<karapo::entity::Image> image;
			std::wstring path;
		public:
			Image(const std::wstring & P, const WorldVector WV) : Image(P, WV, WorldVector{ 0, 0 }) {}

			Image(const std::wstring & P, const WorldVector WV, const WorldVector Len) : Image(std::vector<std::wstring>{}) {
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
					} else if (!IsSameType<std::wstring>(path_param) ||
						(!IsSameType<int>(x_param) && !IsSameType<Dec>(x_param)) ||
						(!IsSameType<int>(y_param) && !IsSameType<Dec>(y_param)) ||
						(!IsSameType<int>(w_param) && !IsSameType<Dec>(w_param)) ||
						(!IsSameType<int>(h_param) && !IsSameType<Dec>(h_param))) [[unlikely]]
					{
						goto type_error;
					}


					path = std::any_cast<std::wstring>(path_param);
					Dec x{}, y{}, w{}, h{};
					if (x_param.type() == typeid(int) || x_param.type() == typeid(Dec))
						x = (x_param.type() == typeid(Dec) ? std::any_cast<Dec>(x_param) : std::any_cast<int>(x_param));
					else if (IsReferenceType<int>(x_param) || IsReferenceType<Dec>(y_param))
						x = (IsReferenceType<Dec>(x_param) ? GetReferencedValue<Dec>(x_param) : GetReferencedValue<int>(x_param));

					if (y_param.type() == typeid(int) || y_param.type() == typeid(Dec))
						y = (x_param.type() == typeid(Dec) ? std::any_cast<Dec>(y_param) : std::any_cast<int>(y_param));
					else if (IsReferenceType<int>(y_param) || IsReferenceType<Dec>(y_param))
						y = (IsReferenceType<Dec>(y_param) ? GetReferencedValue<Dec>(y_param) : GetReferencedValue<int>(y_param));

					if (w_param.type() == typeid(int) || w_param.type() == typeid(Dec))
						w = (x_param.type() == typeid(Dec) ? std::any_cast<Dec>(w_param) : std::any_cast<int>(w_param));
					else if (IsReferenceType<int>(w_param) || IsReferenceType<Dec>(w_param))
						w = (IsReferenceType<Dec>(w_param) ? GetReferencedValue<Dec>(w_param) : GetReferencedValue<int>(w_param));

					if (h_param.type() == typeid(int) || h_param.type() == typeid(Dec))
						h = (h_param.type() == typeid(Dec) ? std::any_cast<Dec>(h_param) : std::any_cast<int>(h_param));
					else if (IsReferenceType<int>(h_param) || IsReferenceType<Dec>(h_param))
						h = (IsReferenceType<Dec>(h_param) ? GetReferencedValue<Dec>(h_param) : GetReferencedValue<int>(h_param));

					image = std::make_shared<karapo::entity::Image>(WorldVector{ x, y }, WorldVector{ w, h });
				}
				image->Load(path.c_str());
				Program::Instance().entity_manager.Register(image);
				return;

			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: image/�摜");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: image/�摜");
				goto end_of_function;
			end_of_function:
				return;
			}
		};

		// �A�j���[�V�����p�ϐ����쐬����B
		DYNAMIC_COMMAND(Animation final) {
			std::wstring var_name{};
		public:
			Animation(const std::wstring & Var_Name) noexcept : Animation(std::vector<std::wstring>{}) {
				var_name = Var_Name;
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Animation) {}

			~Animation() noexcept final {}

			void Execute() final {
				if (var_name.empty())
					goto name_error;

				Program::Instance().var_manager.MakeNew(var_name + L".animation") = animation::Animation();
				Program::Instance().var_manager.MakeNew(var_name + L".frame") = animation::FrameRef();
				return;
			name_error:
				event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"�R�}���h��: animation/�A�j��");
			}
		};

		// �A�j���[�V�����ϐ��Ƀt���[����ǉ�����B
		DYNAMIC_COMMAND(AddFrame final) {
			std::wstring var_name{}, image_path{};
		public:
			AddFrame(const std::wstring & Var_Name, const std::wstring & Image_Path) noexcept : AddFrame(std::vector<std::wstring>{}) {
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
					else if (!IsSameType<std::wstring>(var_name_param) || !IsSameType<std::wstring>(path_param)) [[unlikely]]
						goto type_error;

					if (!IsReferenceType<std::wstring>(var_name_param))
						var_name = std::any_cast<std::wstring>(var_name_param);
					else
						var_name = GetReferencedValue<std::wstring>(var_name_param);
					if (!IsReferenceType<std::wstring>(path_param))
						image_path = std::any_cast<std::wstring>(path_param);
					else
						image_path = GetReferencedValue<std::wstring>(path_param);
				}

				if (var_name.empty() || image_path.empty())
					goto name_error;
				else {
					auto& anime_var = Program::Instance().var_manager.Get(var_name + L".animation");
					if (anime_var.type() != typeid(animation::Animation))
						goto type_error;
					else {
						auto& anime = std::any_cast<animation::Animation&>(anime_var);
						auto& frame_var = Program::Instance().var_manager.Get(var_name + L".frame");
						auto& frame = std::any_cast<animation::FrameRef&>(frame_var);

						resource::Image image;
						image = Program::Instance().engine.LoadImage(image_path);
						anime.PushBack(image);
						frame.InitFrame(anime.Begin(), anime.End());
					}
				}
				return;
			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: addframe/�t���[���ǉ�");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: addframe/�t���[���ǉ�");
				goto end_of_function;
			name_error:
				event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"�R�}���h��: addframe/�t���[���ǉ�");
				goto end_of_function;
			end_of_function:
				return;
			}
		};

		// �t���[�������ɐi�߂�B
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
					auto& frame_var = Program::Instance().var_manager.Get(var_name + L".frame");
					if (frame_var.type() != typeid(animation::FrameRef))
						goto type_error;
					else {
						std::any_cast<animation::FrameRef&>(frame_var)++;
						auto frame = std::any_cast<animation::FrameRef&>(frame_var);
					}
				}
				return;
			name_error:
				event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"�R�}���h��: nextframe/���t���[��");
				return;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: nextframe/���t���[��");
				return;
			}
		};

		// �t���[����O�ɖ߂��B
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
					auto& frame_var = Program::Instance().var_manager.Get(var_name + L".frame");
					if (frame_var.type() != typeid(animation::FrameRef))
						goto type_error;
					else {
						std::any_cast<animation::FrameRef&>(frame_var)--;
					}
				}
				return;
			name_error:
				event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"�R�}���h��: backframe/�O�t���[��");
				return;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: backframe/�O�t���[��");
				return;
			}
		};

		// �摜����ꕔ���R�s�[����B
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

					if (var_name.type() == typeid(std::nullptr_t) ||
						path_param.type() == typeid(std::nullptr_t) ||
						x_param.type() == typeid(std::nullptr_t) ||
						y_param.type() == typeid(std::nullptr_t) ||
						w_param.type() == typeid(std::nullptr_t) ||
						h_param.type() == typeid(std::nullptr_t)) [[unlikely]]
					{
						goto lack_error;
					} else if (!IsSameType<std::wstring>(var_name) || !IsSameType<std::wstring>(path_param) ||
						!IsSameType<int>(x_param) || !IsSameType<int>(y_param) ||
						!IsSameType<int>(w_param) || !IsSameType<int>(h_param)) [[unlikely]]
					{
						goto type_error;
					}
					variable_name = (!IsReferenceType<std::wstring>(var_name) ? std::any_cast<std::wstring>(var_name) : GetReferencedValue<std::wstring>(var_name));
					path = (!IsReferenceType<std::wstring>(path_param) ? std::any_cast<std::wstring>(path_param) : GetReferencedValue<std::wstring>(path_param));
					position[0] = (!IsReferenceType<int>(x_param) ? std::any_cast<int>(x_param) : GetReferencedValue<int>(x_param));
					position[1] = (!IsReferenceType<int>(y_param) ? std::any_cast<int>(y_param) : GetReferencedValue<int>(y_param));
					length[0] = (!IsReferenceType<int>(w_param) ? std::any_cast<int>(w_param) : GetReferencedValue<int>(w_param));
					length[1] = (!IsReferenceType<int>(h_param) ? std::any_cast<int>(h_param) : GetReferencedValue<int>(h_param));
				}
				Program::Instance().engine.CopyImage(&path, position, length);
				Program::Instance().var_manager.Get(variable_name) = path;
				return;

			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: capture/�摜�؎�");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: capture/�摜�؎�");
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
				music = std::make_shared<karapo::entity::Sound>(PlayType::Loop, WorldVector{ 0, 0 });
			}

			~Music() override {}

			void Execute() override {
				if (MustSearch()) {
					auto path_param = GetParam(0);
					if (path_param.type() == typeid(std::nullptr_t)) [[unlikely]]
						goto lack_error;
					else if (!IsSameType<std::wstring>(path_param)) [[unlikely]]
						goto type_error;

					path = (!IsReferenceType<std::wstring>(path_param) ? std::any_cast<std::wstring>(path_param) : GetReferencedValue<std::wstring>(path_param));
				}
				ReplaceFormat(&path);
				music->Load(path);
				Program::Instance().entity_manager.Register(music);
				return;

			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: music/BGM/���y");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: music/BGM/���y");
				goto end_of_function;
			end_of_function:
				return;
			}
		};

		// ���ʉ�
		DYNAMIC_COMMAND(Sound) {
			std::shared_ptr<karapo::entity::Sound> sound;
			std::wstring path;
		public:
			Sound(const std::wstring & P, const WorldVector & WV) : Sound(std::vector<std::wstring>{}) {
				path = P;
				sound = std::make_shared<karapo::entity::Sound>(PlayType::Normal, WV);
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
					} else if (!IsSameType<std::wstring>(path_param) ||
						(!IsSameType<Dec>(x_param) && !IsSameType<int>(x_param)) ||
						(!IsSameType<Dec>(y_param) && !IsSameType<int>(y_param))) [[unlikely]]
					{
						goto type_error;
					}

					path = (!IsReferenceType<std::wstring>(path_param) ? std::any_cast<std::wstring>(path_param) : GetReferencedValue<std::wstring>(path_param));
					Dec x{}, y{};
					if (IsReferenceType<int>(x_param) || IsReferenceType<Dec>(x_param))
						x = (IsReferenceType<Dec>(x_param) ? GetReferencedValue<Dec>(x_param) : GetReferencedValue<int>(x_param));
					else
						x = (x_param.type() == typeid(Dec) ? std::any_cast<Dec>(x_param) : std::any_cast<int>(x_param));

					if (IsReferenceType<int>(y_param) || IsReferenceType<Dec>(y_param))
						y = (IsReferenceType<Dec>(y_param) ? GetReferencedValue<Dec>(y_param) : GetReferencedValue<int>(y_param));
					else
						y = (y_param.type() == typeid(Dec) ? std::any_cast<Dec>(y_param) : std::any_cast<int>(y_param));

					sound = std::make_shared<karapo::entity::Sound>(PlayType::Normal, WorldVector{ x, y });
				}
				ReplaceFormat(&path);
				sound->Load(path);
				Program::Instance().entity_manager.Register(sound);
				return;
			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: Sound/���ʉ�");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: Sound/���ʉ�");
				goto end_of_function;
			end_of_function:
				return;
			}
		};

		DYNAMIC_COMMAND(Font final) {
			std::wstring variable_name{}, source_font_name{};
			size_t length{}, thick{};
			FontKind font_kind{};
		public:
			Font(const std::wstring& Variable_Name, const std::wstring& Font_Name, const size_t Length, const size_t Thick, const int Font_Kind) : Font(std::vector<std::wstring>{}) {
				variable_name = Variable_Name;
				source_font_name = Font_Name;
				length = Length;
				thick = Thick;
				font_kind = (Font_Kind == 1 ? FontKind::Anti_Aliasing : FontKind::Normal);
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Font) {}

			void Execute() {
				if (MustSearch()) {
					auto variable_name_param = GetParam<true>(0),
						source_font_name_param = GetParam(1),
						length_param = GetParam(2),
						thick_param = GetParam(3),
						font_kind_param = GetParam(4);

					if (variable_name_param.type() == typeid(std::nullptr_t) ||
						source_font_name_param.type() == typeid(std::nullptr_t) ||
						length_param.type() == typeid(std::nullptr_t) ||
						thick_param.type() == typeid(std::nullptr_t) ||
						font_kind_param.type() == typeid(std::nullptr_t)) [[unlikely]]
					{
						goto lack_error;
					}
					else if (!IsSameType<std::wstring>(variable_name_param) || !IsSameType<std::wstring>(source_font_name_param) ||
						!IsSameType<int>(length_param) || !IsSameType<int>(thick_param) || !IsSameType<int>(font_kind_param)) [[unlikely]]
					{
						goto type_error;
					}
					variable_name = (!IsReferenceType<std::wstring>(variable_name_param) ? std::any_cast<std::wstring>(variable_name_param) : GetReferencedValue<std::wstring>(variable_name_param));
					source_font_name = (!IsReferenceType<std::wstring>(source_font_name_param) ? std::any_cast<std::wstring>(source_font_name_param) : GetReferencedValue<std::wstring>(source_font_name_param));
					length = (!IsReferenceType<int>(length_param) ? std::any_cast<int>(length_param) : GetReferencedValue<int>(length_param));
					thick = (!IsReferenceType<int>(thick_param) ? std::any_cast<int>(thick_param) : GetReferencedValue<int>(thick_param));
					font_kind = ((!IsReferenceType<int>(font_kind_param) ? std::any_cast<int>(font_kind_param) : GetReferencedValue<int>(font_kind_param)) == 1 ? FontKind::Anti_Aliasing : FontKind::Normal);
				}

				if (variable_name.empty() || source_font_name.empty()) {
					goto name_error;
				}
				Program::Instance().var_manager.MakeNew(variable_name) =
					Program::Instance().engine.MakeFont(source_font_name + L':' + std::to_wstring(length) + L':' + std::to_wstring(thick) + L':' + std::to_wstring((int)font_kind), source_font_name, length, thick, font_kind);
				goto end_of_function;
			name_error:
				event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"�R�}���h��: font/�t�H���g");
				goto end_of_function;
			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: font/�t�H���g");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: font/�t�H���g");
				goto end_of_function;
			end_of_function:
				return;
			}
		};

		// �����Đ�
		DYNAMIC_COMMAND(Play final) {
			std::wstring file_path{};
			error::ErrorContent *entity_kind_error{};
		public:
			Play(const std::wstring & File_Name) noexcept : Play(std::vector<std::wstring>{}) {
				file_path = File_Name;
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Play) {
				if (entity_kind_error == nullptr) {
					entity_kind_error = error::UserErrorHandler::MakeError(
						command_error_class,
						L"�w�肵��Entity�̎�ނ��s�K�؂ł��B",
						MB_OK | MB_ICONERROR, 
						1
					);
				}
			}

			~Play() noexcept final {}

			void Execute() final {
				if (MustSearch()) {
					auto file_path_param = GetParam(0);
					if (file_path_param.type() == typeid(std::nullptr_t))
						goto lack_error;
					else if (!IsSameType<std::wstring>(file_path_param))
						goto type_error;
					
					file_path = (!IsReferenceType<std::wstring>(file_path_param) ? std::any_cast<std::wstring>(file_path_param) : GetReferencedValue<std::wstring>(file_path_param));
				}
				if (file_path.empty())
					goto name_error;
				else {
					auto ent = Program::Instance().entity_manager.GetEntity(file_path);
					if (ent == nullptr)
						goto not_found_error;
					else if (ent->KindName() != std::wstring(L"���ʉ�"))
						goto kind_error;
					
					auto& position_var = Program::Instance().var_manager.Get(file_path.substr(0, file_path.find(L'.')) + L"_position");
					const int Position = (position_var.type() == typeid(int) ? std::any_cast<int>(position_var) : 0);
					std::static_pointer_cast<karapo::entity::Sound>(ent)->Play(Position);
				}
				goto end_of_function;
			not_found_error:
				event::Manager::Instance().error_handler.SendLocalError(entity_not_found_error, L"�R�}���h��: play/�Đ�");
				goto end_of_function;
			kind_error:
				event::Manager::Instance().error_handler.SendLocalError(entity_kind_error, L"�R�}���h��: play/�Đ�");
				goto end_of_function;
			name_error:
				event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"�R�}���h��: play/�Đ�");
				goto end_of_function;
			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: play/�Đ�");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: play/�Đ�");
				goto end_of_function;
			end_of_function:
				return;
			}
		};

		// �����ꎞ��~
		DYNAMIC_COMMAND(Pause final) {
			std::wstring file_path{};
			error::ErrorContent *entity_kind_error{};
		public:
			Pause(const std::wstring & File_Name) noexcept : Pause(std::vector<std::wstring>{}) {
				file_path = File_Name;
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Pause) {
				if (entity_kind_error == nullptr) {
					entity_kind_error = error::UserErrorHandler::MakeError(
						command_error_class,
						L"�w�肵��Entity�̎�ނ��s�K�؂ł��B",
						MB_OK | MB_ICONERROR,
						1
					);
				}
			}

			~Pause() noexcept final {}

			void Execute() final {
				if (MustSearch()) {
					auto file_path_param = GetParam(0);
					if (file_path_param.type() == typeid(std::nullptr_t))
						goto lack_error;
					else if (!IsSameType<std::wstring>(file_path_param))
						goto type_error;

					file_path = (!IsReferenceType<std::wstring>(file_path_param) ? std::any_cast<std::wstring>(file_path_param) : GetReferencedValue<std::wstring>(file_path_param));
				}

				if (file_path.empty())
					goto name_error;
				else {
					auto ent = Program::Instance().entity_manager.GetEntity(file_path);
					if (ent == nullptr)
						goto not_found_error;
					else if (ent->KindName() != std::wstring(L"���ʉ�"))
						goto kind_error;

					Program::Instance().var_manager.MakeNew(file_path.substr(0, file_path.find(L'.')) + L"_position") = std::static_pointer_cast<karapo::entity::Sound>(ent)->Stop();
				}
				goto end_of_function;
			not_found_error:
				event::Manager::Instance().error_handler.SendLocalError(entity_not_found_error, L"�R�}���h��: pause/�ꎞ��~");
				goto end_of_function;
			kind_error:
				event::Manager::Instance().error_handler.SendLocalError(entity_kind_error, L"�R�}���h��: pause/�ꎞ��~");
				goto end_of_function;
			name_error:
				event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"�R�}���h��: pause/�ꎞ��~");
				goto end_of_function;
			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: pause/�ꎞ��~");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: pause/�ꎞ��~");
				goto end_of_function;
			end_of_function:
				return;
			}
		};

		// ������~
		DYNAMIC_COMMAND(Stop final) {
			std::wstring file_path{};
			error::ErrorContent *entity_kind_error{};
		public:
			Stop(const std::wstring & File_Name) noexcept : Stop(std::vector<std::wstring>{}) {
				file_path = File_Name;
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Stop) {
				if (entity_kind_error == nullptr) {
					entity_kind_error = error::UserErrorHandler::MakeError(
						command_error_class,
						L"�w�肵��Entity�̎�ނ��s�K�؂ł��B",
						MB_OK | MB_ICONERROR,
						1
					);
				}
			}

			~Stop() noexcept final {}

			void Execute() final {
				if (MustSearch()) {
					auto file_path_param = GetParam(0);
					if (file_path_param.type() == typeid(std::nullptr_t))
						goto lack_error;
					else if (!IsSameType<std::wstring>(file_path_param))
						goto type_error;

					file_path = (!IsReferenceType<std::wstring>(file_path_param) ? std::any_cast<std::wstring>(file_path_param) : GetReferencedValue<std::wstring>(file_path_param));
				}

				if (file_path.empty())
					goto name_error;
				else {
					auto ent = Program::Instance().entity_manager.GetEntity(file_path);
					if (ent == nullptr)
						goto not_found_error;
					else if (ent->KindName() != std::wstring(L"���ʉ�"))
						goto kind_error;

					std::static_pointer_cast<karapo::entity::Sound>(ent)->Stop();
				}
				goto end_of_function;
			not_found_error:
				event::Manager::Instance().error_handler.SendLocalError(entity_not_found_error, L"�R�}���h��: stop/��~");
				goto end_of_function;
			kind_error:
				event::Manager::Instance().error_handler.SendLocalError(entity_kind_error, L"�R�}���h��: stop/��~");
				goto end_of_function;
			name_error:
				event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"�R�}���h��: stop/��~");
				goto end_of_function;
			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: stop/��~");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: stop/��~");
				goto end_of_function;
			end_of_function:
				return;
			}
		};

		// �{�^��
		DYNAMIC_COMMAND(Button final) {
			std::shared_ptr<karapo::entity::Button> button;
			std::any path{};
		public:
			Button(const std::wstring & Name,
				const WorldVector & WV,
				const std::wstring & Image_Path,
				const WorldVector & Size) noexcept : Button(std::vector<std::wstring>{})
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
					} else if (!IsSameType<std::wstring>(name_param) ||
						(!IsSameType<Dec>(x_param) && !IsSameType<int>(x_param)) ||
						(!IsSameType<Dec>(y_param) && !IsSameType<int>(y_param))) [[unlikely]]
					{
						goto type_error;
					}

					auto name = (!IsReferenceType<std::wstring>(name_param) ? std::any_cast<std::wstring>(name_param) : GetReferencedValue<std::wstring>(name_param));
					Dec x{}, y{};
					if (IsReferenceType<int>(x_param) || IsReferenceType<Dec>(x_param))
						x = (IsReferenceType<Dec>(x_param) ? GetReferencedValue<Dec>(x_param) : GetReferencedValue<int>(x_param));
					else
						x = (x_param.type() == typeid(Dec) ? std::any_cast<Dec>(x_param) : std::any_cast<int>(x_param));
					if (IsReferenceType<int>(y_param) || IsReferenceType<Dec>(y_param))
						y = (IsReferenceType<Dec>(y_param) ? GetReferencedValue<Dec>(y_param) : GetReferencedValue<int>(y_param));
					else
						y = (y_param.type() == typeid(Dec) ? std::any_cast<Dec>(y_param) : std::any_cast<int>(y_param));

					auto w_param = GetParam(3),
						h_param = GetParam(4),
						path_param = GetParam(5);
					Dec w{}, h{};
					if (w_param.type() != typeid(std::nullptr_t) && h_param.type() != typeid(std::nullptr_t) && path_param.type() != typeid(std::nullptr_t)) {
						if (IsReferenceType<int>(w_param) || IsReferenceType<Dec>(w_param))
							w = (IsReferenceType<Dec>(w_param) ? GetReferencedValue<Dec>(w_param) : GetReferencedValue<int>(w_param));
						else
							w = (w_param.type() == typeid(Dec) ? std::any_cast<Dec>(w_param) : std::any_cast<int>(w_param));
						if (IsReferenceType<int>(h_param) || IsReferenceType<Dec>(h_param))
							h = (IsReferenceType<Dec>(h_param) ? GetReferencedValue<Dec>(h_param) : GetReferencedValue<int>(h_param));
						else
							h = (h_param.type() == typeid(Dec) ? std::any_cast<Dec>(h_param) : std::any_cast<int>(h_param));
						path = (!IsReferenceType<std::wstring>(path_param) ? std::any_cast<std::wstring>(path_param) : GetReferencedValue<std::wstring>(path_param));
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
							Program::Instance().var_manager.Get(
								std::any_cast<std::wstring>(GetParam<true>(5))
							)
						)
					);
				}

				Program::Instance().entity_manager.Register(button);
				return;
			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: Button/�{�^��");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: Button/�{�^��");
				goto end_of_function;
			end_of_function:
				return;
			}
		};

		// �����o��
		DYNAMIC_COMMAND(Print final) {
			std::shared_ptr<karapo::entity::Text> text{};
			std::wstring name{};
		public:
			Print(const std::wstring & Name, const WorldVector & Pos) noexcept : Print(std::vector<std::wstring>{}) {
				name = Name;
				auto pos = Pos;
				ReplaceFormat(&name);
				text = std::make_shared<karapo::entity::Text>(name, pos);
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
					} else if (!IsSameType<std::wstring>(name_param) ||
						(!IsSameType<Dec>(x_param) && !IsSameType<int>(x_param)) ||
						(!IsSameType<Dec>(y_param) && !IsSameType<int>(y_param))) [[unlikely]]
					{
						goto type_error;
					}

					name = (!IsReferenceType<std::wstring>(name_param) ? std::any_cast<std::wstring>(name_param) : GetReferencedValue<std::wstring>(name_param));
					Dec x{}, y{};
					if (IsReferenceType<int>(x_param) || IsReferenceType<Dec>(x_param))
						x = (IsReferenceType<Dec>(x_param) ? GetReferencedValue<Dec>(x_param) : GetReferencedValue<int>(x_param));
					else
						x = (x_param.type() == typeid(Dec) ? std::any_cast<Dec>(x_param) : std::any_cast<int>(x_param));
					if (IsReferenceType<int>(y_param) || IsReferenceType<Dec>(y_param))
						y = (IsReferenceType<Dec>(y_param) ? GetReferencedValue<Dec>(y_param) : GetReferencedValue<int>(y_param));
					else
						y = (y_param.type() == typeid(Dec) ? std::any_cast<Dec>(y_param) : std::any_cast<int>(y_param));

					ReplaceFormat(&name);
					text = std::make_shared<karapo::entity::Text>(name, WorldVector{ x, y });
				}
				Program::Instance().var_manager.MakeNew(name + L".text") = std::wstring(L"");
				Program::Instance().var_manager.MakeNew(name + L".rgb") = std::wstring(L"");
				Program::Instance().var_manager.MakeNew(name + L".font") = 0;
				Program::Instance().entity_manager.Register(text);
				return;
			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: text/����");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: text/����");
				goto end_of_function;
			end_of_function:
				return;
			}
		};

		// ��������
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
					} else if (!IsSameType<std::wstring>(name_param) ||
						!IsSameType<int>(x_param) ||
						!IsSameType<int>(y_param) ||
						!IsSameType<int>(len_param)) [[unlikely]]
					{
						goto type_error;
					}
					var = &Program::Instance().var_manager.Get(std::any_cast<std::wstring>(name_param));
					pos[0] = (!IsReferenceType<int>(x_param) ? std::any_cast<int>(x_param) : GetReferencedValue<int>(x_param));
					pos[1] = (!IsReferenceType<int>(y_param) ? std::any_cast<int>(y_param) : GetReferencedValue<int>(y_param));
					length = (!IsReferenceType<int>(len_param) ? std::any_cast<int>(len_param) : GetReferencedValue<int>(len_param));
				}
				wchar_t str[10000];
				Program::Instance().engine.GetString(pos, str, length);
				*var = std::wstring(str);
				return;
			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: input/����");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: input/����");
				goto end_of_function;
			end_of_function:
				return;
			}
		};

		namespace entity {
			// Entity�̈ړ�
			DYNAMIC_COMMAND(Teleport final) {
				std::wstring entity_name;
				WorldVector move;
			public:
				Teleport(const std::wstring & ename, const WorldVector & MV) noexcept : Teleport(std::vector<std::wstring>{}) {
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
						} else if (!IsSameType<std::wstring>(name_param)) [[unlikely]]
							goto type_error;

						entity_name = (!IsReferenceType<std::wstring>(name_param) ? std::any_cast<std::wstring>(name_param) : GetReferencedValue<std::wstring>(name_param));

						Dec x{}, y{};
						if (IsReferenceType<Dec>(x_param) || IsReferenceType<int>(x_param))
							x = (IsReferenceType<Dec>(x_param) ? GetReferencedValue<Dec>(x_param) : GetReferencedValue<int>(x_param));
						else if (x_param.type() == typeid(int) || x_param.type() == typeid(Dec))
							x = (x_param.type() == typeid(Dec) ? std::any_cast<Dec>(x_param) : std::any_cast<int>(x_param));
						else [[unlikely]]
							goto type_error;

						if (IsReferenceType<Dec>(y_param) || IsReferenceType<int>(y_param))
							y = (IsReferenceType<Dec>(y_param) ? GetReferencedValue<Dec>(y_param) : GetReferencedValue<int>(y_param));
						else if (y_param.type() == typeid(int) || y_param.type() == typeid(Dec))
							y = (y_param.type() == typeid(Dec) ? std::any_cast<Dec>(y_param) : std::any_cast<int>(y_param));
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
					event::Manager::Instance().error_handler.SendLocalError(entity_not_found_error, L"�R�}���h��: teleport/�u�Ԉړ�");
					goto end_of_function;
				lack_error:
					event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: teleport/�u�Ԉړ�");
					goto end_of_function;
				type_error:
					event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: teleport/�u�Ԉړ�");
					goto end_of_function;
				end_of_function:
					return;
				}
			};

			// Entity�̍폜�B
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
						else if (!IsSameType<std::wstring>(name_param)) [[unlikely]]
							goto type_error;

						entity_name = (!IsReferenceType<std::wstring>(name_param) ? std::any_cast<std::wstring>(name_param) : GetReferencedValue<std::wstring>(name_param));
					}
					if (entity_name.empty())
						goto name_error;

					ReplaceFormat(&entity_name);
					if (entity_name == L"__all" || entity_name == L"__�S��") {
						std::vector<std::wstring> names{};
						auto sen = std::any_cast<std::wstring>(Program::Instance().var_manager.Get(L"entity::Manager.__�Ǘ���"));
						{
							std::wstring temp{};
							for (auto ch : sen) {
								if (ch != L'\\') {
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
					event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"�R�}���h��: kill/�E�Q");
					goto end_of_function;
				lack_error:
					event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: kill/�E�Q");
					goto end_of_function;
				type_error:
					event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: kill/�E�Q");
					goto end_of_function;
				end_of_function:
					return;
				}
			};

			// Entity�܂���Entity�̎�ނ̍X�V�����B
			DYNAMIC_COMMAND(Freeze final) {
				std::wstring candidate_name{};
			public:
				Freeze(const std::wstring & ename) noexcept : Freeze(std::vector<std::wstring>{}) {
					candidate_name = ename;
				}

				DYNAMIC_COMMAND_CONSTRUCTOR(Freeze) {}

				~Freeze() final {}

				void Execute() override {
					if (MustSearch()) {
						auto candidate_name_param = GetParam(0);
						if (!IsSameType<std::wstring>(candidate_name_param)) {
							goto type_error;
						}
						candidate_name = (!IsReferenceType<std::wstring>(candidate_name_param) ? std::any_cast<std::wstring>(candidate_name_param) : GetReferencedValue<std::wstring>(candidate_name_param));
					}

					if (candidate_name.empty()) {
						goto name_error;
					} else {
						auto ent = karapo::entity::Manager::Instance().GetEntity(candidate_name);
						if (ent != nullptr) {
							karapo::entity::Manager::Instance().Deactivate(ent);
						} else {
							karapo::entity::Manager::Instance().Deactivate(candidate_name);
						}
					}
					goto end_of_function;
				name_error:
					event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"�R�}���h��: freeze/����");
					goto end_of_function;
				lack_error:
					event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: freeze/����");
					goto end_of_function;
				type_error:
					event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: freeze/����");
					goto end_of_function;
				end_of_function:
					return;
				}
			};

			// Entity�܂���Entity�̎�ނ̍X�V�ĊJ�B
			DYNAMIC_COMMAND(Defrost final) {
				std::wstring candidate_name{};
			public:
				Defrost(const std::wstring & ename) noexcept : Defrost(std::vector<std::wstring>{}) {
					candidate_name = ename;
				}

				DYNAMIC_COMMAND_CONSTRUCTOR(Defrost) {}

				~Defrost() final {}

				void Execute() override {
					if (MustSearch()) {
						auto candidate_name_param = GetParam(0);
						if (!IsSameType<std::wstring>(candidate_name_param)) {
							goto type_error;
						}
						candidate_name = (!IsReferenceType<std::wstring>(candidate_name_param) ? std::any_cast<std::wstring>(candidate_name_param) : GetReferencedValue<std::wstring>(candidate_name_param));
					}
					if (candidate_name.empty()) {
						goto name_error;
					} else {
						if (candidate_name.empty()) {
							goto name_error;
						} else {
							auto ent = karapo::entity::Manager::Instance().GetEntity(candidate_name);
							if (ent != nullptr) {
								karapo::entity::Manager::Instance().Activate(ent);
							} else {
								karapo::entity::Manager::Instance().Activate(candidate_name);
							}
						}
					}
					goto end_of_function;
				name_error:
					event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"�R�}���h��: defrost/��");
					goto end_of_function;
				lack_error:
					event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: defrost/��");
					goto end_of_function;
				type_error:
					event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: defrost/��");
					goto end_of_function;
				end_of_function:
					return;
				}
			};
		}

		namespace array {
			DYNAMIC_COMMAND(Front final) {
				std::wstring assignable_name{}, array_name{};
			public:
				Front(const std::wstring & Assignable_Name, const std::wstring & Array_Name) noexcept : Front(std::vector<std::wstring>{}) {
					assignable_name = Assignable_Name;
					array_name = Array_Name;
				}

				~Front() final {}

				DYNAMIC_COMMAND_CONSTRUCTOR(Front) {}

				void Execute() final {
					auto& assignable = Program::Instance().var_manager.MakeNew(assignable_name);
					auto& source_array = Program::Instance().var_manager.Get(array_name);
					if (source_array.type() == typeid(std::wstring)) {
						std::wstring result = (!IsReferenceType<std::wstring>(source_array) ? std::any_cast<std::wstring&>(source_array) : GetReferencedValue<std::wstring>(source_array));

						if (!result.empty())
							assignable = result.front() + std::wstring();
					} else {
						goto type_error;
					}
					goto end_of_function;
				name_error:
					event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"�R�}���h��: front");
					goto end_of_function;
				lack_error:
					event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: front");
					goto end_of_function;
				type_error:
					event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: front");
					goto end_of_function;
				end_of_function:
					return;
				}
			};

			DYNAMIC_COMMAND(Back final) {
				std::wstring assignable_name{}, array_name{};
			public:
				Back(const std::wstring& Assignable_Name, const std::wstring& Array_Name) noexcept : Back(std::vector<std::wstring>{}) {
					assignable_name = Assignable_Name;
					array_name = Array_Name;
				}

				~Back() final {}

				DYNAMIC_COMMAND_CONSTRUCTOR(Back) {}

				void Execute() final {
					auto& assignable = Program::Instance().var_manager.MakeNew(assignable_name);
					auto& source_array = Program::Instance().var_manager.Get(array_name);
					if (source_array.type() == typeid(std::wstring)) {
						std::wstring result = (!IsReferenceType<std::wstring>(source_array) ? std::any_cast<std::wstring&>(source_array) : GetReferencedValue<std::wstring>(source_array));

						if (!result.empty())
							assignable = result.back() + std::wstring();
					} else {
						goto type_error;
					}
					goto end_of_function;
				name_error:
					event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"�R�}���h��: back");
					goto end_of_function;
				lack_error:
					event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: back");
					goto end_of_function;
				type_error:
					event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: back");
					goto end_of_function;
				end_of_function:
					return;
				}
			};

			DYNAMIC_COMMAND(Pop final) {
				std::wstring array_name{};
				std::optional<int> index = std::nullopt;

				template<typename T>
				bool PopArray(T& source_array) noexcept {
					if (!index.has_value())
						source_array.pop_back();
					else if (index.value() >= 0 && index.value() < source_array.size())
						source_array.erase(source_array.begin() + index.value());
					else if (index.value() < 0 && index.value() >= -source_array.size())
						source_array.erase(source_array.end() + index.value());
					else
						return false;

					return true;
				}
			public:
				Pop(const std::wstring& Array_Name, const std::optional<int> Index) noexcept : Pop(std::vector<std::wstring>{}) {
					array_name = Array_Name;
					index = Index;
				}

				~Pop() final {}

				DYNAMIC_COMMAND_CONSTRUCTOR(Pop) {}

				void Execute() final {
					if (MustSearch()) {
						auto array_name_param = GetParam<true>(0),
							index_param = GetParam(1);
						if (array_name_param.type() == typeid(std::nullptr_t))
							goto lack_error;
						else if (index_param.type() == typeid(int))
							index = std::any_cast<int>(index_param);

						array_name = std::any_cast<std::wstring>(array_name_param);
					}

					if (!array_name.empty()) {
						auto& source_array = Program::Instance().var_manager.Get(array_name);
						if (IsSameType<std::wstring>(source_array)) {
							PopArray((!IsReferenceType<std::wstring>(source_array) ? std::any_cast<std::wstring&>(source_array) : GetReferencedValue<std::wstring>(source_array)));
						} else {
							goto type_error;
						}
					} else {
						goto name_error;
					}
					goto end_of_function;
				name_error:
					event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"�R�}���h��: pop");
					goto end_of_function;
				lack_error:
					event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: pop");
					goto end_of_function;
				type_error:
					event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: pop");
					goto end_of_function;
				end_of_function:
					return;
				}
			};
		}

		// �L�[���͖��̃R�}���h����
		class Bind final : public DynamicCommand {
			std::wstring key_name{}, command_sentence{};
		public:
			Bind(const std::wstring& Key_Name, const std::wstring& Command_Sentence) noexcept {
				key_name = Key_Name;
				command_sentence = Command_Sentence;
			}

			void Execute() override;
		};

		// �R�}���h�̕ʖ�
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
			Filter(const std::wstring & N, const std::wstring & KN, const int P) noexcept : Filter(std::vector<std::wstring>{}) {
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
					} else if (!IsSameType<std::wstring>(layer_name_param) ||
						!IsSameType<std::wstring>(kind_name_param) ||
						!IsSameType<int>(potecy_param)) [[unlikely]]
					{
						goto type_error;
					}
					layer_name = (!IsReferenceType<std::wstring>(layer_name_param) ? std::any_cast<std::wstring>(layer_name_param) : GetReferencedValue<std::wstring>(layer_name_param));
					kind_name = (!IsReferenceType<std::wstring>(kind_name_param) ? std::any_cast<std::wstring>(kind_name_param) : GetReferencedValue<std::wstring>(kind_name_param));
					potency = (!IsReferenceType<int>(potecy_param) ? std::any_cast<int>(potecy_param) : GetReferencedValue<int>(potecy_param));
				}

				if (layer_name.empty() || kind_name.empty()) [[unlikely]] {
					goto name_error;
				}
				ReplaceFormat(&layer_name);
				ReplaceFormat(&kind_name);
				Program::Instance().canvas.ApplyFilter(layer_name, kind_name, potency);

				return;
			name_error:
				event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"�R�}���h��: filter/�t�B���^�[");
				goto end_of_function;
			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: filter/�t�B���^�[");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: filter/�t�B���^�[");
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

		// DLL�A�^�b�`
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

		// �C�x���g�ďo
		DYNAMIC_COMMAND(Call final) {
			std::wstring event_name{};
			inline static error::ErrorContent *event_not_found_error{};
		public:
			DYNAMIC_COMMAND_CONSTRUCTOR(Call) {
				if (event_not_found_error == nullptr)
					event_not_found_error = error::UserErrorHandler::MakeError(
						event::Manager::Instance().error_class,
						L"�w�肳�ꂽ�C�x���g��������܂���B",
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
					else if (!IsSameType<std::wstring>(params[0])) [[unlikely]]
						goto type_error;

					event_name = (!IsReferenceType<std::wstring>(params[0]) ? std::any_cast<std::wstring>(params[0]) : GetReferencedValue<std::wstring>(params[0]));
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
							auto& value = params[i + 1];
							auto event_param_name = e->param_names[i];
							if (event_param_name[0] == L'&') {
								auto& newvar = Program::Instance().var_manager.MakeNew(event_name + std::wstring(L"@") + event_param_name.substr(1));
								auto&& referrencable_name = std::any_cast<std::wstring>(GetParam<true>(i + 1));
								auto& referrencable = Program::Instance().var_manager.Get(referrencable_name);
								if (referrencable.type() != typeid(std::reference_wrapper<std::any>))
									newvar = std::ref(referrencable);
								else
									newvar = referrencable;
							} else {
								auto& newvar = Program::Instance().var_manager.MakeNew(event_name + std::wstring(L"@") + event_param_name);
								if (value.type() == typeid(int))
									newvar = std::any_cast<int>(value);
								else if (value.type() == typeid(Dec))
									newvar = std::any_cast<Dec>(value);
								else if (value.type() == typeid(std::wstring))
									newvar = std::any_cast<std::wstring>(value);
								else if (value.type() == typeid(variable::Record))
									newvar = std::any_cast<variable::Record>(value);
							}
						}
					}
					if (!Program::Instance().event_manager.Call(event_name)) {
						goto event_error;
					}
				}
				return;
			event_error:
				event::Manager::Instance().error_handler.SendLocalError(event_not_found_error, L"�R�}���h��: call/�ďo");
				goto end_of_function;
			name_error:
				event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"�R�}���h��: call/�ďo");
				goto end_of_function;
			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: call/�ďo");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: call/�ďo");
				goto end_of_function;
			end_of_function:
				return;
			}
		};

		// �C�x���g�ǉ��Ǎ�
		DYNAMIC_COMMAND(Import final) {
			std::wstring file_name;
		public:
			Import(const std::wstring & File_Name) noexcept : Import(std::vector<std::wstring>{}) {
				file_name = File_Name;
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Import) {}

			~Import() noexcept final {}

			void Execute() override {
				if (MustSearch()) {
					auto name_param = GetParam(0);
					if (name_param.type() == typeid(std::nullptr_t)) [[unlikely]]
						goto lack_error;
					else if (!IsSameType<std::wstring>(name_param)) [[unlikely]]
						goto type_error;

					file_name = (!IsReferenceType<std::wstring>(name_param) ? std::any_cast<std::wstring>(name_param) : GetReferencedValue<std::wstring>(name_param));
				}
				if (file_name.empty()) [[unlikely]]
					goto name_error;

				ReplaceFormat(&file_name);
				Program::Instance().event_manager.ImportEvent(file_name);

				return;
			name_error:
				event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"�R�}���h��: import/�捞");
				goto end_of_function;
			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: import/�捞");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: import/�捞");
				goto end_of_function;
			end_of_function:
				return;
			}
		};

		// �C�x���g�Ǎ�
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
					else if (!IsSameType<std::wstring>(name_param)) [[unlikely]]
						goto type_error;

					file_name = (!IsReferenceType<std::wstring>(name_param) ? std::any_cast<std::wstring>(name_param) : GetReferencedValue<std::wstring>(name_param));
				}
				if (file_name.empty()) [[unlikely]]
					goto name_error;

				ReplaceFormat(&file_name);
				Program::Instance().event_manager.RequestEvent(file_name);

				return;
			name_error:
				event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"�R�}���h��: load/�Ǎ�");
				goto end_of_function;
			lack_error:
				event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: load/�Ǎ�");
				goto end_of_function;
			type_error:
				event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: load/�Ǎ�");
				goto end_of_function;
			end_of_function:
				return;
			}
		};

		namespace layer {
			// ���C���[����(�w��ʒu)
			DYNAMIC_COMMAND(Make final) {
				std::wstring kind_name{}, layer_name{};
				int index = 0;
				inline static const std::unordered_map<std::wstring, bool (Canvas::*)(const std::wstring&, const int)> Create{
					// ���Έʒu���C���[
					{ L"scroll",  &Canvas::CreateRelativeLayer },
					{ L"�X�N���[��",  &Canvas::CreateRelativeLayer },
					{ L"relative",  &Canvas::CreateRelativeLayer },
					{ L"���Έʒu",  &Canvas::CreateRelativeLayer },
					// ��Έʒu���C���[
					{ L"fixed",  &Canvas::CreateAbsoluteLayer },
					{ L"�Œ�",  &Canvas::CreateAbsoluteLayer },
					{ L"absolute",  &Canvas::CreateAbsoluteLayer },
					{ L"��Έʒu",  &Canvas::CreateAbsoluteLayer }
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
						layer_kind_not_found_error = error::UserErrorHandler::MakeError(command_error_class, L"���C���[�̎�ނ�������܂���B", MB_OK | MB_ICONERROR, 2);
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
						} else if (!IsSameType<int>(index_param) ||
							!IsSameType<std::wstring>(kind_param) ||
							!IsSameType<std::wstring>(layer_param)) [[unlikely]]
						{
							goto type_error;
						}

						index = (!IsReferenceType<int>(index_param) ? std::any_cast<int>(index_param) : GetReferencedValue<int>(index_param));
						kind_name = (!IsReferenceType<std::wstring>(kind_param) ? std::any_cast<std::wstring>(kind_param) : GetReferencedValue<std::wstring>(kind_param));
						layer_name = (!IsReferenceType<std::wstring>(layer_param) ? std::any_cast<std::wstring>(layer_param) : GetReferencedValue<std::wstring>(layer_param));
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
					event::Manager::Instance().error_handler.SendLocalError(layer_kind_not_found_error, L"�R�}���h��: makelayer/���C���[����");
					goto end_of_function;
				name_error:
					event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"�R�}���h��: makelayer/���C���[����");
					goto end_of_function;
				lack_error:
					event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: makelayer/���C���[����");
					goto end_of_function;
				type_error:
					event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: makelayer/���C���[����");
					goto end_of_function;
				end_of_function:
					return;
				}
			};

			// ���C���[�ύX
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
						else if (!IsSameType<std::wstring>(layer_name_param)) [[unlikely]]
							goto type_error;

						layer_name = (!IsReferenceType<std::wstring>(layer_name_param) ? std::any_cast<std::wstring>(layer_name_param) : GetReferencedValue<std::wstring>(layer_name_param));
					}

					if (layer_name.empty()) [[unlikely]]
						goto name_error;

					ReplaceFormat(&layer_name);
					Program::Instance().canvas.SelectLayer(layer_name);

					return;
				name_error:
					event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"�R�}���h��: selectlayer/���C���[�I��");
					goto end_of_function;
				lack_error:
					event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: selectlayer/���C���[�I��");
					goto end_of_function;
				type_error:
					event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: selectlayer/���C���[�I��");
					goto end_of_function;
				end_of_function:
					return;
				}
			};

			// ���Έʒu���C���[�̊�ݒ�
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
						else if (!IsSameType<std::wstring>(entity_name_param) || !IsSameType<std::wstring>(layer_name_param)) [[unlikely]]
							goto type_error;

						entity_name = (!IsReferenceType<std::wstring>(entity_name_param) ? std::any_cast<std::wstring>(entity_name_param) : GetReferencedValue<std::wstring>(entity_name_param));
						layer_name = (!IsReferenceType<std::wstring>(layer_name_param) ? std::any_cast<std::wstring>(layer_name_param) : GetReferencedValue<std::wstring>(layer_name_param));
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
					event::Manager::Instance().error_handler.SendLocalError(entity_not_found_error, L"�R�}���h��: setbasis/���C���[�");
					goto end_of_function;
				name_error:
					event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"�R�}���h��: setbasis/���C���[�");
					goto end_of_function;
				lack_error:
					event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: setbasis/���C���[�");
					goto end_of_function;
				type_error:
					event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: setbasis/���C���[�");
					goto end_of_function;
				end_of_function:
					return;
				}
			};

			// ���C���[�폜
			DYNAMIC_COMMAND(Delete final) {
				std::wstring name{};
			public:
				Delete(const std::wstring & N) noexcept : Delete(std::vector<std::wstring>{}) {
					name = N;
				}

				DYNAMIC_COMMAND_CONSTRUCTOR(Delete) {}

				~Delete() final {}

				void Execute() final {
					if (MustSearch()) {
						auto name_param = GetParam(0);
						if (name_param.type() == typeid(std::nullptr_t)) [[unlikely]]
							goto lack_error;
						else if (!IsSameType<std::wstring>(name_param)) [[unlikely]]
							goto type_error;

						name = (!IsReferenceType<std::wstring>(name) ? std::any_cast<std::wstring>(name) : GetReferencedValue<std::wstring>(name));
					}
					if (name.empty()) [[unlikely]]
						goto name_error;

					ReplaceFormat(&name);
					if (name == L"__all" || name == L"__�S��") {
						for (int i = 0; Program::Instance().canvas.DeleteLayer(i););
						Program::Instance().canvas.CreateAbsoluteLayer(L"�f�t�H���g���C���[");
						Program::Instance().canvas.SelectLayer(L"�f�t�H���g���C���[");
					} else {
						Program::Instance().canvas.DeleteLayer(name);
					}

					return;
				name_error:
					event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"�R�}���h��: deletelayer/���C���[�폜");
					goto end_of_function;
				lack_error:
					event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: deletelayer/���C���[�폜");
					goto end_of_function;
				type_error:
					event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: deletelayer/���C���[�폜");
					goto end_of_function;
				end_of_function:
					return;
				}
			};

			DYNAMIC_COMMAND(Show final) {
				std::wstring name{};
			public:
				Show(const std::wstring & N) noexcept : Show(std::vector<std::wstring>{}) {
					name = N;
				}

				DYNAMIC_COMMAND_CONSTRUCTOR(Show) {}

				~Show() final {}

				void Execute() final {
					if (MustSearch()) {
						auto name_param = GetParam(0);
						if (name_param.type() == typeid(std::nullptr_t)) [[unlikely]]
							goto lack_error;
						else if (!IsSameType<std::wstring>(name_param)) [[unlikely]]
							goto type_error;

						name = (!IsReferenceType<std::wstring>(name) ? std::any_cast<std::wstring>(name) : GetReferencedValue<std::wstring>(name));
					}
					if (name.empty()) [[unlikely]]
						goto name_error;
					ReplaceFormat(&name);
					Program::Instance().canvas.Show(name);

					return;
				name_error:
					event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"�R�}���h��: showlayer/���C���[�\��");
					goto end_of_function;
				lack_error:
					event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: showlayer/���C���[�\��");
					goto end_of_function;
				type_error:
					event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: showlayer/���C���[�\��");
					goto end_of_function;
				end_of_function:
					return;
				}
			};

			DYNAMIC_COMMAND(Hide final) {
				std::wstring name{};
			public:
				Hide(const std::wstring & N) noexcept : Hide(std::vector<std::wstring>{}) {
					name = N;
				}

				DYNAMIC_COMMAND_CONSTRUCTOR(Hide) {}

				~Hide() final {}

				void Execute() final {
					if (MustSearch()) {
						auto name_param = GetParam(0);
						if (name_param.type() == typeid(std::nullptr_t)) [[unlikely]]
							goto lack_error;
						else if (!IsSameType<std::wstring>(name_param)) [[unlikely]]
							goto type_error;

						name = (!IsReferenceType<std::wstring>(name) ? std::any_cast<std::wstring>(name) : GetReferencedValue<std::wstring>(name));
					}
					if (name.empty()) [[unlikely]]
						goto name_error;
					ReplaceFormat(&name);
					Program::Instance().canvas.Hide(name);

					return;
				name_error:
					event::Manager::Instance().error_handler.SendLocalError(empty_name_error, L"�R�}���h��: hidelayer/���C���[��\��");
					goto end_of_function;
				lack_error:
					event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: hidelayer/���C���[��\��");
					goto end_of_function;
				type_error:
					event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: hidelayer/���C���[��\��");
					goto end_of_function;
				end_of_function:
					return;
				}
			};
		}

		namespace math {
#define MATH_COMMAND_CALCULATE(OPERATION) \
	if (Is_Only_Int) { \
		cal.i = (!IsReferenceType<int>(value[0]) ? std::any_cast<int>(value[0]) : GetReferencedValue<int>(value[0])) OPERATION (!IsReferenceType<int>(value[1]) ? std::any_cast<int>(value[1]) : GetReferencedValue<int>(value[1])); \
	} else { \
		cal.d = 0.0; \
		if (IsSameType<int>(value[0])) { \
			cal.d = (!IsReferenceType<int>(value[0]) ? std::any_cast<int>(value[0]) : GetReferencedValue<int>(value[0])); \
		} else { \
			cal.d = (!IsReferenceType<Dec>(value[0]) ? std::any_cast<Dec>(value[0]) : GetReferencedValue<Dec>(value[0])); \
		} \
 \
		if (IsSameType<int>(value[1])) { \
			cal.d OPERATION= (!IsReferenceType<int>(value[1]) ? std::any_cast<int>(value[1]) : GetReferencedValue<int>(value[1])); \
		} else { \
			cal.d OPERATION= (!IsReferenceType<Dec>(value[1]) ? std::any_cast<Dec>(value[1]) : GetReferencedValue<Dec>(value[1])); \
		} \
	}

			DYNAMIC_COMMAND(MathCommand) {
				std::wstring self_class_name{};
			protected:
				inline static error::ErrorClass* operation_error_class{};
				inline static error::ErrorContent* assign_error{};

				static void Reassign(const int Result) {
					switch (Result) {
						case IDYES:
						{
							auto& var = Program::Instance().var_manager.Get(L"__assignable");
							auto& value = Program::Instance().var_manager.Get(L"__calculated");
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

				void SendAssignError(const std::any & Value) {
					event::Manager::Instance().error_handler.SendLocalError(assign_error, L"�R�}���h��: " + GetOperatorName() + L'\n' + (L"�ϐ�: " + var_name), &MathCommand::Reassign);
					Program::Instance().var_manager.MakeNew(L"__assignable") = var_name;
					if (Value.type() == typeid(int))
						Program::Instance().var_manager.MakeNew(L"__calculated") = std::any_cast<int>(Value);
					else if (Value.type() == typeid(Dec))
						Program::Instance().var_manager.MakeNew(L"__calculated") = std::any_cast<Dec>(Value);
					else if (Value.type() == typeid(std::wstring))
						Program::Instance().var_manager.MakeNew(L"__calculated") = std::any_cast<std::wstring>(Value);
				}

				void SendAssignError(const bool Is_Only_Int, const CalculateValue Cal_Value) {
					event::Manager::Instance().error_handler.SendLocalError(assign_error, L"�R�}���h��: " + GetOperatorName() + L'\n' + (L"�ϐ�: " + var_name).c_str(), &MathCommand::Reassign);
					Program::Instance().var_manager.MakeNew(L"__assignable") = var_name;
					Program::Instance().var_manager.MakeNew(L"__calculated") = (Is_Only_Int ? Cal_Value.i : Cal_Value.d);
				}

				void SendAssignError(const std::wstring& Sentence) {
					event::Manager::Instance().error_handler.SendLocalError(assign_error, L"�R�}���h��: " + GetOperatorName() + L'\n' + (L"�ϐ�: " + var_name).c_str(), &MathCommand::Reassign);
					Program::Instance().var_manager.MakeNew(L"__assignable") = var_name;
					Program::Instance().var_manager.MakeNew(L"__calculated") = Sentence;
				}

				const std::wstring& GetOperatorName() const noexcept {
					return self_class_name;
				}
			public:
				MathCommand(const std::vector<std::wstring>&Params, const std::wstring& Self_Class_Name) : DynamicCommand(Params) {
					if (operation_error_class == nullptr) [[unlikely]]
						operation_error_class = error::UserErrorHandler::MakeErrorClass(L"���Z�G���[");
					if (assign_error == nullptr) [[unlikely]]
						assign_error = error::UserErrorHandler::MakeError(operation_error_class,  L"�����̕ϐ������݂��܂���B\n�V�������̕ϐ����쐬���܂���?", MB_YESNO | MB_ICONERROR, 2);
					self_class_name = Self_Class_Name;
				}

				std::wstring var_name{};
				std::any value[2]{};

				// �v�Z�ɕK�v�Ȓl��W�J����B
				// �����Ȃ�true�A���s�Ȃ�false��Ԃ��B
				[[nodiscard]] bool Extract(const int Length) noexcept {
					if (MustSearch()) {
						var_name = std::any_cast<std::wstring>(GetParam<true>(0));
						for (int i = 0; i < Length; i++) {
							value[i] = GetParam(i + 1);
							auto s = std::any_cast<std::wstring>(GetParam<true>(i + 1));
							// �^�`�F�b�N
							if (value[i].type() == typeid(std::wstring) || value[i].type() == typeid(int) || value[i].type() == typeid(Dec) || value[i].type() == typeid(resource::Resource)) {
								continue;
							} else if (value[i].type() == typeid(std::reference_wrapper<std::any>)) {
								continue;
							} else if (value[i].type() == typeid(animation::FrameRef)) {
								value[i] = std::ref(
									std::any_cast<animation::FrameRef&>(
									Program::Instance().var_manager.Get(std::any_cast<std::wstring>(GetParam<true>(i + 1)))
								)
								);
							} else if (value[i].type() == typeid(std::nullptr_t)) {
								goto lack_error;
							} else
								goto type_error;
						}
					}
					return true;
				lack_error:
					event::Manager::Instance().error_handler.SendLocalError(lack_of_parameters_error, L"�R�}���h��: " + GetOperatorName());
					goto failed_exit;
				type_error:
					event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: " + GetOperatorName());
					goto failed_exit;
				failed_exit:
					return false;
				}
			};

			class Assign final : public MathCommand {
			public:
				Assign(const std::vector<std::wstring>&Params) noexcept : MathCommand(Params, L"assign/���") {}
				~Assign() final {}

				void Execute() final {
					if (Extract(1)) {
						auto& v = Program::Instance().var_manager.Get(var_name);
						if (v.type() != typeid(std::nullptr_t)) [[likely]] {
							if (v.type() == typeid(std::reference_wrapper<std::any>)) {
								auto& r = std::any_cast<std::reference_wrapper<std::any>&>(v);
								auto& ref = std::any_cast<std::reference_wrapper<std::any>&>(v).get();
								if (IsSameType<int>(value[0]))
									ref = (!IsReferenceType<int>(value[0]) ? std::any_cast<int>(value[0]) : GetReferencedValue<int>(value[0]));
								else if (IsSameType<Dec>(value[0]))
									ref = (!IsReferenceType<Dec>(value[0]) ? std::any_cast<Dec>(value[0]) : GetReferencedValue<Dec>(value[0]));
								else if (IsSameType<std::wstring>(value[0])) {
									auto txt = (!IsReferenceType<std::wstring>(value[0]) ? std::any_cast<std::wstring>(value[0]) : GetReferencedValue<std::wstring>(value[0]));
									ReplaceFormat(&txt);
									ref = txt;
								} else if (IsSameType<resource::Resource>(value[0])) {
									ref = (!IsReferenceType<resource::Resource>(value[0]) ? std::any_cast<resource::Resource>(value[0]) : GetReferencedValue<resource::Resource>(value[0]));
								} else if (value[0].type() == typeid(std::reference_wrapper<animation::FrameRef>)) {
									ref = std::any_cast<std::reference_wrapper<animation::FrameRef>&>(value[0]);
								}
							} else {
								if (IsSameType<int>(value[0]))
									v = (!IsReferenceType<int>(value[0]) ? std::any_cast<int>(value[0]) : GetReferencedValue<int>(value[0]));
								else if (IsSameType<Dec>(value[0]))
									v = (!IsReferenceType<Dec>(value[0]) ? std::any_cast<Dec>(value[0]) : GetReferencedValue<Dec>(value[0]));
								else if (IsSameType<std::wstring>(value[0])) {
									auto txt = (!IsReferenceType<std::wstring>(value[0]) ? std::any_cast<std::wstring>(value[0]) : GetReferencedValue<std::wstring>(value[0]));
									ReplaceFormat(&txt);
									v = txt;
								} else if (value[0].type() == typeid(std::reference_wrapper<animation::FrameRef>)) {
									int i = 0;
									v = std::any_cast<std::reference_wrapper<animation::FrameRef>&>(value[0]);
								} else if (IsSameType<resource::Resource>(value[0])) {
									v = (!IsReferenceType<resource::Resource>(value[0]) ? std::any_cast<resource::Resource>(value[0]) : GetReferencedValue<resource::Resource>(value[0]));
								}
							}
						} else {
							SendAssignError(value[0]);
						}
					}

				}
			};

			// ���Z
			class Sum final : public MathCommand {
			public:
				Sum(const std::vector<std::wstring>&Params) noexcept : MathCommand(Params, L"sum/���Z") {}
				~Sum() final {}

				void Execute() final {
					if (Extract(2)) {
						if (!IsSameType<std::wstring>(value[0]) && !IsSameType<std::wstring>(value[1])) {
							const bool Is_Only_Int = (IsSameType<int>(value[0]) && IsSameType<int>(value[1]));

							CalculateValue cal;
							MATH_COMMAND_CALCULATE(+);

							auto* v = &Program::Instance().var_manager.Get(var_name);
							if (v->type() != typeid(std::nullptr_t)) [[likely]] {
								if (v->type() == typeid(std::reference_wrapper<std::any>)) {
									auto& ref = std::any_cast<std::reference_wrapper<std::any>&>(*v).get();
									if (Is_Only_Int)
										ref = cal.i;
									else
										ref = cal.d;
								} else {
									if (Is_Only_Int)
										*v = cal.i;
									else
										*v = cal.d;
								}
							} else {
								SendAssignError(Is_Only_Int, cal);
							}
						} else {
							std::wstring result{};
							for (int i = 0; i < 2; i++) {
								if (IsSameType<std::wstring>(value[i])) {
									result += (!IsReferenceType<std::wstring>(value[i]) ? std::any_cast<std::wstring&>(value[i]) : GetReferencedValue<std::wstring>(value[i]));
								} else {
									goto type_error;
								}
							}
							{
								auto* v = &Program::Instance().var_manager.Get(var_name);
								if (v->type() != typeid(std::nullptr_t)) [[likely]] {
									*v = result;
								} else {
									SendAssignError(result);
								}
							}
							return;
						type_error:
							event::Manager::Instance().error_handler.SendLocalError(incorrect_type_error, L"�R�}���h��: " + GetOperatorName());
						}
					}
				}
			};

			// ���Z
			class Sub final : public MathCommand {
			public:
				Sub(const std::vector<std::wstring>&Params) noexcept : MathCommand(Params, L"sub/���Z") {}
				~Sub() final {}

				void Execute() final {
					if (Extract(2)) {
						const bool Is_Only_Int = (IsSameType<int>(value[0]) && IsSameType<int>(value[1]));

						CalculateValue cal;
						MATH_COMMAND_CALCULATE(-);

						auto* v = &Program::Instance().var_manager.Get(var_name);
						if (v->type() != typeid(std::nullptr_t)) [[likely]] {
							if (v->type() == typeid(std::reference_wrapper<std::any>)) {
								auto& ref = std::any_cast<std::reference_wrapper<std::any>&>(*v).get();
								if (Is_Only_Int)
									ref = cal.i;
								else
									ref = cal.d;
							} else {
								if (Is_Only_Int)
									*v = cal.i;
								else
									*v = cal.d;
							}
						} else {
							SendAssignError(Is_Only_Int, cal);
						}
					}

				}
			};

			// ��Z
			class Mul final : public MathCommand {
			public:
				Mul(const std::vector<std::wstring>&Params) noexcept : MathCommand(Params, L"mul/��Z") {}
				~Mul() final {}

				void Execute() final {
					if (Extract(2)) {
						const bool Is_Only_Int = (IsSameType<int>(value[0]) && IsSameType<int>(value[1]));

						CalculateValue cal;
						MATH_COMMAND_CALCULATE(*);

						auto *v = &Program::Instance().var_manager.Get(var_name);
						if (v->type() != typeid(std::nullptr_t)) [[likely]] {
							if (v->type() == typeid(std::reference_wrapper<std::any>)) {
								auto& ref = std::any_cast<std::reference_wrapper<std::any>&>(*v).get();
								if (Is_Only_Int)
									ref = cal.i;
								else
									ref = cal.d;
							} else {
								if (Is_Only_Int)
									*v = cal.i;
								else
									*v = cal.d;
							}
						} else {
							SendAssignError(Is_Only_Int, cal);
						}
					}

				}
			};

			// ���Z
			class Div final : public MathCommand {
			public:
				Div(const std::vector<std::wstring>&Params) noexcept : MathCommand(Params, L"div/���Z") {}
				~Div() final {}

				void Execute() final {
					if (Extract(2)) {
						const bool Is_Only_Int = (IsSameType<int>(value[0]) && IsSameType<int>(value[1]));

						CalculateValue cal;
						MATH_COMMAND_CALCULATE(/ );

						auto* v = &Program::Instance().var_manager.Get(var_name);
						if (v->type() != typeid(std::nullptr_t)) [[likely]] {
							if (v->type() == typeid(std::reference_wrapper<std::any>)) {
								auto& ref = std::any_cast<std::reference_wrapper<std::any>&>(*v).get();
								if (Is_Only_Int)
									ref = cal.i;
								else
									ref = cal.d;
							} else {
								if (Is_Only_Int)
									*v = cal.i;
								else
									*v = cal.d;
							}
						} else {
							SendAssignError(Is_Only_Int, cal);
						}
					}

				}
			};

			class Mod final : public MathCommand {
			public:
				Mod(const std::vector<std::wstring>&Params) noexcept : MathCommand(Params, L"mod/��]") {}
				~Mod() final {}

				void Execute() final {
					if (Extract(2)) {
						const bool Is_Only_Int = (IsSameType<int>(value[0]) && IsSameType<int>(value[1]));

						CalculateValue cal;
						if (Is_Only_Int) {
							cal.i = (!IsReferenceType<int>(value[0]) ? std::any_cast<int>(value[0]) : GetReferencedValue<int>(value[0])) % (!IsReferenceType<int>(value[1]) ? std::any_cast<int>(value[1]) : GetReferencedValue<int>(value[1])); \
						} else {
							cal.d = 0.0;
							if (IsSameType<int>(value[0])) {
								cal.d = (!IsReferenceType<int>(value[0]) ? std::any_cast<int>(value[0]) : GetReferencedValue<int>(value[0]));
							} else {
								cal.d = (!IsReferenceType<Dec>(value[0]) ? std::any_cast<Dec>(value[0]) : GetReferencedValue<Dec>(value[0]));
							}

							if (value[1].type() == typeid(int)) {
								cal.d = fmod(cal.d, (!IsReferenceType<int>(value[1]) ? std::any_cast<int>(value[1]) : GetReferencedValue<int>(value[1])));
							} else {
								cal.d = fmod(cal.d, (!IsReferenceType<Dec>(value[1]) ? std::any_cast<Dec>(value[1]) : GetReferencedValue<Dec>(value[1])));
							}
						}

						auto* v = &Program::Instance().var_manager.Get(var_name);
						if (v->type() != typeid(std::nullptr_t)) [[likely]] {
							if (v->type() == typeid(std::reference_wrapper<std::any>)) {
								auto& ref = std::any_cast<std::reference_wrapper<std::any>&>(*v).get();
								if (Is_Only_Int)
									ref = cal.i;
								else
									ref = cal.d;
							} else {
								if (Is_Only_Int)
									*v = cal.i;
								else
									*v = cal.d;
							}
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
					const bool Is_First_Int = IsSameType<int>(value[0]);
					const bool Is_Second_Int = IsSameType<int>(value[1]);
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
				BitCommand(const std::vector<std::wstring>& Params, const std::wstring& Self_Class_Name) : MathCommand(Params, Self_Class_Name) {
					if (logic_operation_error_class == nullptr) [[unlikely]]
						logic_operation_error_class = error::UserErrorHandler::MakeErrorClass(L"�_�����Z�G���[");
					if (not_integer_error == nullptr) [[unlikely]]
						not_integer_error = error::UserErrorHandler::MakeError(logic_operation_error_class, L"�r�b�g���Z�ɗp����ϐ��̒l�������l�ł͂���܂���B", MB_OK | MB_ICONERROR, 2);
				}

				~BitCommand() override {}
			};

			// �r�b�g�_���a
			class Or final : public BitCommand {
			public:
				Or(const std::vector<std::wstring>&Params) noexcept : BitCommand(Params, L"or/�_���a") {}
				~Or() final {}

				void Execute() final {
					if (Extract(2)) [[likely]] {
						const auto Is_Int = CheckValueType();

						if (Is_Int.first && Is_Int.second) [[likely]] {
							CalculateValue cal;
							cal.i = ((!IsReferenceType<int>(value[0]) ? std::any_cast<int>(value[0]) : GetReferencedValue<int>(value[0])) | (!IsReferenceType<int>(value[1]) ? std::any_cast<int>(value[1]) : GetReferencedValue<int>(value[1])));
							auto* v = &Program::Instance().var_manager.Get(var_name);
							if (v->type() != typeid(std::nullptr_t)) {
								if (v->type() == typeid(std::reference_wrapper<std::any>)) {
									std::any_cast<std::reference_wrapper<std::any>&>(*v).get() = cal.i;
								} else {
									*v = cal.i;
								}
							} else {
								SendAssignError(true, cal);
							}
						} else {
							SendBitLogicError(L"���Z: �r�b�g�_���a\n", Is_Int);
						}
					}

				}
			};

			// �r�b�g�_����
			class And final : public BitCommand {
			public:
				And(const std::vector<std::wstring>&Params) noexcept : BitCommand(Params, L"and/�_����") {}
				~And() final {}

				void Execute() final {
					if (Extract(2)) [[likely]] {
						const auto Is_Int = CheckValueType();
						if (Is_Int.first && Is_Int.second) [[likely]] {
							CalculateValue cal;
							cal.i = ((!IsReferenceType<int>(value[0]) ? std::any_cast<int>(value[0]) : GetReferencedValue<int>(value[0])) & (!IsReferenceType<int>(value[1]) ? std::any_cast<int>(value[1]) : GetReferencedValue<int>(value[1])));
							auto* v = &Program::Instance().var_manager.Get(var_name);
							if (v->type() != typeid(std::nullptr_t)) {
								if (v->type() == typeid(std::reference_wrapper<std::any>)) {
									std::any_cast<std::reference_wrapper<std::any>&>(*v).get() = cal.i;
								} else {
									*v = cal.i;
								}
							} else {
								SendAssignError(true, cal);
							}
						} else {
							SendBitLogicError(L"���Z: �r�b�g�_����\n", Is_Int);
						}
					}

				}
			};

			// �r�b�g�r���I�_���a
			class Xor final : public BitCommand {
			public:
				Xor(const std::vector<std::wstring>&Params) noexcept : BitCommand(Params, L"xor/�r���I�_���a") {}
				~Xor() final {}

				void Execute() final {
					if (Extract(2)) [[likely]] {
						const auto Is_Int = CheckValueType();

						if (Is_Int.first && Is_Int.second) [[likely]] {
							CalculateValue cal;
							cal.i = ((!IsReferenceType<int>(value[0]) ? std::any_cast<int>(value[0]) : GetReferencedValue<int>(value[0])) ^ (!IsReferenceType<int>(value[1]) ? std::any_cast<int>(value[1]) : GetReferencedValue<int>(value[1])));
							auto* v = &Program::Instance().var_manager.Get(var_name);
							if (v->type() != typeid(std::nullptr_t)) {
								if (v->type() == typeid(std::reference_wrapper<std::any>)) {
									std::any_cast<std::reference_wrapper<std::any>&>(*v).get() = cal.i;
								} else {
									*v = cal.i;
								}
							} else {
								SendAssignError(true, cal);
							}
						} else {
							SendBitLogicError(L"���Z: �r�b�g�r���I�_���a\n", Is_Int);
						}
					}

				}
			};

			// �r�b�g�_���ے�
			class Not final : public BitCommand {
			public:
				Not(const std::vector<std::wstring>&Params) noexcept : BitCommand(Params, L"not/�_���ے�") {}
				~Not() final {}

				void Execute() final {
					if (Extract(1)) [[likely]] {
						std::wstring var_name{};
						var_name = std::any_cast<std::wstring>(GetParam<true>(0));

						if (value[0].type() == typeid(int)) [[likely]] {
							auto * v = &Program::Instance().var_manager.Get(var_name);
							CalculateValue cal;
							cal.i = ~(!IsReferenceType<int>(value[0]) ? std::any_cast<int>(value[0]) : GetReferencedValue<int>(value[0]));
							if (v->type() != typeid(std::nullptr_t)) {
								if (v->type() == typeid(std::reference_wrapper<std::any>)) {
									std::any_cast<std::reference_wrapper<std::any>&>(*v).get() = cal.i;
								} else {
									*v = cal.i;
								}
							} else {
								SendAssignError(true, cal);
							}
						} else {
							SendBitLogicError(L"���Z: �r�b�g�_���ے�\n", { false, true });
						}
					}

				}
			};
		}

		namespace hidden {
			// Entity���ւ��S�Ă�Manager���X�V
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

	void EventExecuter::PushEvent(const std::wstring& Event_Name) noexcept {
		event_queue.push_back(Event_Name);
	}

	void EventExecuter::Update() {
		while (!event_queue.empty()) {
			Manager::Instance().Call(event_queue.front());
			event_queue.pop_front();
		}
	}

	void EventExecuter::Execute(const std::list<CommandGraph>& Commands) {
		if (Commands.empty())
			return;

		Program::Instance().var_manager.MakeNew(L"of_state") = 1;
		const int& Go_Parent = std::any_cast<int&>(Program::Instance().var_manager.Get(L"of_state"));

		// ��������A�R�}���h���s�B
		{
			const CommandGraph *Executing = &Commands.front();
			while (Executing != nullptr) {
				Executing->command->Execute();
				Executing = (Go_Parent ? Executing->parent : Executing->neighbor);
			}
		}
	}

	// �C�x���g�����N���X
	// �C�x���g�t�@�C���̉�́A�R�}���h�̐����A�C�x���g�̐ݒ�E�������s���B
	class EventGenerator final : private Singleton {
	public:
		// ��̓N���X
		class Parser final {
			// �\����
			struct Syntax final {
				size_t line{};
				std::wstring text{};
				Syntax* parent{};
			};

			// �ꕶ�����̉�͊�B
			// �󔒂���Ƃ���P��P�ʂ̉�͌��ʂ�context�Ƃ��Ĕr�o�B
			class LexicalParser final {
				Context context;

				auto MakeStringContext(std::wstring::const_iterator current_char, std::wstring::const_iterator end) noexcept {
					if (current_char == end) {
						return current_char;
					} else if (*current_char == L'\'') {
						context.back() += *current_char;
						return current_char;
					}
					context.back() += *current_char;
					return MakeStringContext(current_char + 1, end);
				}

				auto MakeContext(std::wstring::const_iterator current_char, std::wstring::const_iterator end) noexcept {
					if (current_char == end) {
						return current_char;
					} else if (*current_char == L'\n') {
						context.push_back({});
						context.back() += *current_char;
						context.push_back({});
						current_char++;
						return current_char;
					}

					switch (*current_char) {
						case L'\'':
							context.back() += *current_char;
							current_char = MakeStringContext(current_char + 1, end);
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
							context.push_back({});
							context.back() += *current_char;
							context.push_back({});
							break;
						case L'\r':
							break;
						case L' ':
						case L'\t':
							context.push_back({});
							break;
						default:
							context.back() += *current_char;
							break;
					}
					return MakeContext(current_char + 1, end);
				}

				auto JoinContext(Context::iterator current_context, const std::wstring& Pattern) noexcept {
					if (current_context == context.end()) {
						return;
					}
					if (Pattern.find(*current_context) != std::wstring::npos) {
						auto next = std::next(current_context, 1);
						auto op = (*current_context + *next);
						if (Pattern == op) {
							current_context = context.erase(current_context);
							*current_context = op;
							JoinContext(std::find_if(std::next(current_context, 1), context.end(), [](const std::wstring& Word) { return Word == L"\n"; }), Pattern);
						} else
							goto next_char;
					} else {
					next_char:
						JoinContext(std::next(current_context, 1), Pattern);
					}
				}
			public:
				LexicalParser(const std::wstring Sentence) noexcept {
					{
						auto sentence_iterator = Sentence.begin();
						context.push_back({});
						while (sentence_iterator != Sentence.end()) {
							sentence_iterator = MakeContext(sentence_iterator, Sentence.end());
						}
					}
					// �󕶎��̍폜�B
					{
						auto context_iterator = context.begin();
						while (context_iterator != context.end()) {
							if (context_iterator->empty())
								context_iterator = context.erase(context_iterator);
							else
								context_iterator++;
						}
					}

					{
						std::list<std::wstring> operators{ L"//", L"==", L"<=", L">=" };
						std::wstring tmp{};
						while (!operators.empty()) {
							JoinContext(context.begin(), operators.front());
							operators.pop_front();
						}
					}
				}

				auto Result() const noexcept {
					return context;
				}
			};

			// �\���؉��
			class SyntaxParser final {
				std::list<Syntax> tree{};
				inline static error::ErrorContent *invalid_operator_error{};
			public:
				SyntaxParser(Context* lexical_context) noexcept {
					if (invalid_operator_error == nullptr)
						invalid_operator_error = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"�\���Ɍ�肪����܂��B", MB_OK | MB_ICONERROR, 1);

					error::ErrorContent *error_occurred{};

					decltype(Syntax::line) line = 1;
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
									event::Manager::Instance().error_handler.SendLocalError(error_occurred, L"��: " + tree_iterator->text);
								}
								tree_iterator = old;
								break;
							}
							case L'\n':
							{
								line++;
								if (operator_iterator != tree.end()) {
									if (operator_iterator->text != L"{") {
										operator_iterator = tree.end();
										tree_iterator->parent = nullptr;
										tree.push_front(Syntax{ .line = line, .parent = &(*tree_iterator) });
									} else {
										// �R�}���h���͕����s�̕��ō\�������B
										// ({}��e�Ƃ���\���؂��������݂���B)
										// 
										// ���ׁ̈A���{}��e�Ƃ��Đݒ�B
										tree_iterator->parent = &(*operator_iterator);
									}
									tree_iterator = tree.begin();
								}
								break;
							}
							case L',':
								// �J���}�͖����B
								break;
							default:
							not_operator:
								if (str != L"//") {
									tree_iterator->text = str;
									tree.push_front(Syntax{ .line = line, .parent = &(*tree_iterator) });
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

			// �Ӗ����
			class SemanticParser final {
				std::unordered_map<std::wstring, Event> parsing_events{};

				WorldVector origin[2]{ { -1, -1 }, { -1, -1 } };
				TriggerType trigger_type = TriggerType::Invalid;
				std::wstring event_name{};
				std::vector<std::wstring> event_params{}, command_parameters{};
				std::list<CommandGraph> commands{};
				std::unordered_map<std::wstring, GenerateFunc> words{};

				// �R�}���h���𔭌��������ۂ��B
				bool found_command_sentence = false;

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
						empty_name_error = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"�C�x���g������ɂ��邱�Ƃ͂ł��܂���B", MB_OK | MB_ICONERROR, 1);
					if (invalid_trigger_type_warning == nullptr)
						invalid_trigger_type_warning = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"�s���ȃC�x���g�����^�C�v���w�肳�ꂽ���߁A����������ݒ肵�܂����B", MB_OK | MB_ICONWARNING, 4);
					if (command_not_found_error == nullptr)
						command_not_found_error = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"�w�肳�ꂽ�R�}���h��������܂���B", MB_OK | MB_ICONERROR, 2);
					if (lack_of_parameters_error == nullptr)
						lack_of_parameters_error = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"�R�}���h�̈���������Ȃ��ׁA�����ł��܂���B", MB_OK | MB_ICONERROR, 2);
					if (too_many_parameters_warning == nullptr)
						too_many_parameters_warning = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"�R�}���h�̈�������������ׁA�]���Ȃ��͔̂p�����܂����B", MB_OK | MB_ICONWARNING, 3);
					if (already_new_event_name_defined_error == nullptr)
						already_new_event_name_defined_error = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"���ɃC�x���g�����ݒ肳��Ă��܂��B", MB_OK | MB_ICONERROR, 1);
					if (already_new_trigger_type_defined_error == nullptr)
						already_new_trigger_type_defined_error = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"���ɔ����^�C�v���w�肳��Ă��܂��B", MB_OK | MB_ICONERROR, 1);

					Program::Instance().dll_manager.RegisterExternalCommand(&words);

					words[L"text"] =
						words[L"����"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
					{
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Name, Name_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								const auto [X, X_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
								const auto [Y, Y_Type] = Default_ProgramInterface.GetParamInfo(params[2]);
								if (Default_ProgramInterface.IsStringType(Name_Type) &&
									Default_ProgramInterface.IsNumberType(X_Type) &&
									Default_ProgramInterface.IsNumberType(Y_Type))
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
						words[L"���y"] =
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
						words[L"���ʉ�"] =
						words[L"��"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
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
						words[L"�摜"] =
						words[L"�G"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
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

					words[L"font"] =
						words[L"�t�H���g"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Var_Name, Var_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								const auto [Font_Name, Font_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
								const auto [Length, Length_Type] = Default_ProgramInterface.GetParamInfo(params[2]);
								const auto [Thick, Thick_Type] = Default_ProgramInterface.GetParamInfo(params[3]);
								const auto [Aliasing, Aliasing_Type] = Default_ProgramInterface.GetParamInfo(params[4]);

								if (Default_ProgramInterface.IsNoType(Var_Type) &&
									Default_ProgramInterface.IsStringType(Font_Type) &&
									Default_ProgramInterface.IsNumberType(Length_Type) &&
									Default_ProgramInterface.IsNumberType(Thick_Type) &&
									Default_ProgramInterface.IsNumberType(Aliasing_Type))
								{
									return std::make_unique<command::Font>(Var_Name, 
										Font_Name,
										ToInt(Length.c_str(), nullptr), 
										ToInt(Thick.c_str(), nullptr),
										ToInt(Aliasing.c_str(), nullptr)
									);
								} else {
									return std::make_unique<command::Font>(params);
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
						words[L"����"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
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
						words[L"��"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
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

					words[L"play"] =
						words[L"�Đ�"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
					{
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Path, Path_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								if (Default_ProgramInterface.IsStringType(Path_Type)) {
									return std::make_unique<command::Play>(Path);
								} else {
									return std::make_unique<command::Play>(params);
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

					words[L"pause"] =
						words[L"�ꎞ��~"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
					{
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Path, Path_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								if (Default_ProgramInterface.IsStringType(Path_Type)) {
									return std::make_unique<command::Pause>(Path);
								} else {
									return std::make_unique<command::Pause>(params);
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

					words[L"stop"] =
						words[L"��~"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
					{
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Path, Path_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								if (Default_ProgramInterface.IsStringType(Path_Type)) {
									return std::make_unique<command::Stop>(Path);
								} else {
									return std::make_unique<command::Stop>(params);
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
						words[L"�A�j��"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
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
						words[L"�t���[���ǉ�"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
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
						words[L"���t���[��"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
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
						words[L"�O�t���[��"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
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
						words[L"�摜�؎�"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
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
						words[L"�{�^��"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"����"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"�u�Ԉړ�"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"�E�Q"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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

					words[L"front"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Assignable, Assignable_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								const auto [Array, Array_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
								if (Default_ProgramInterface.IsNoType(Assignable_Type) && Default_ProgramInterface.IsNoType(Array_Type))
									return std::make_unique<command::array::Front>(Assignable, Array);
								else
									return std::make_unique<command::array::Front>(params);
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

					words[L"back"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Assignable, Assignable_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								const auto [Array, Array_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
								if (Default_ProgramInterface.IsNoType(Assignable_Type) && Default_ProgramInterface.IsNoType(Array_Type))
									return std::make_unique<command::array::Back>(Assignable, Array);
								else
									return std::make_unique<command::array::Back>(params);
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

					words[L"pop"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() noexcept -> CommandPtr {
								if (params.size() == 1) {
									const auto [Array_Var, Array_Var_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
									if (Default_ProgramInterface.IsNoType(Array_Var_Type))
										return std::make_unique<command::array::Pop>(Array_Var, std::nullopt);
									else
										return std::make_unique<command::array::Pop>(params);
								} else if (params.size() == 2) {
									const auto [Array_Var, Array_Var_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
									const auto [Index, Index_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
									if (Default_ProgramInterface.IsNoType(Array_Var_Type) && Default_ProgramInterface.IsNumberType(Index_Type))
										return std::make_unique<command::array::Pop>(Array_Var, ToInt(Index.c_str(), nullptr));
									else
										return std::make_unique<command::array::Pop>(params);
								} else {
									return std::make_unique<command::array::Pop>(params);
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

					words[L"makelayer"] =
						words[L"���C���[����"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"���C���[�"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"���C���[�I��"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"���C���[�폜"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"���C���[�\��"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"���C���[��\��"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"�L�["] = [this](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"�ʖ�"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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

					// DLL�A�^�b�`
					words[L"attach"] =
						words[L"�ڑ�"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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

					// DLL�f�^�b�`
					words[L"detach"] =
						words[L"�ؒf"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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

					// �O���C�x���g�t�@�C���ǂݍ���
					words[L"load"] =
						words[L"�Ǎ�"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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

					// �O���C�x���g�t�@�C���ǂݍ���
					words[L"import"] =
						words[L"�捞"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"�ďo"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"�ϐ�"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								const auto [Var, Var_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								const auto [Value, Value_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
								if (Value_Type != L"")
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
						words[L"���"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								const auto [Var, Var_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								const auto [Value, Value_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
								if (Value_Type != L"")
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

					words[L"asvar"] =
						words[L"�ϐ��擾"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								const auto [Var, Var_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								const auto [Value, Value_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
								if (Default_ProgramInterface.IsStringType(Value_Type))
									return std::make_unique<command::AsVar>(Var, Value);
								else
									return std::make_unique<command::AsVar>(params);
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
						words[L"���݊m�F"] = [](const std::vector<std::wstring>& params)->KeywordInfo {
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
						words[L"�t�B���^�["] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"����"] = [&](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"����"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"�ȊO"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"����I��"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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

					words[L"__entity�����X�V"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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

					words[L"assign"] = words[L"���"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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

					words[L"sum"] = words[L"���Z"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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

					words[L"sub"] = words[L"���Z"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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

					words[L"mul"] = words[L"��Z"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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

					words[L"div"] = words[L"���Z"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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

					words[L"mod"] = words[L"��]"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"�_���a"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"�_����"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"�r���I�_���a"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"�_���ے�"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
					std::list<Syntax*> stack{};
					// ��: ���݂�case�̃C�e���[�^
				// �E: else�����݂��邩�ۂ��B
					std::list<std::pair<decltype(commands)::iterator, bool>> case_stack{};

					std::unordered_map<std::wstring, std::function<void(std::list<Syntax*>* stack)>> operator_funcs{
						{ 
							L"[]", [this](std::list<Syntax*>* stack) {
								stack->pop_front();
								if (event_name.empty()) {
									switch (stack->size()) {
										case 0:
											error_occurred = empty_name_error;
											event::Manager::Instance().error_handler.SendLocalError(error_occurred);
											break;
										case 1:
											event_name = stack->front()->text;
											stack->pop_front();
											break;
										default:
											std::wstring candidate_name{};
											while (!stack->empty()) {
												candidate_name += stack->front()->text + L' ';
												stack->pop_front();
											}
											candidate_name.pop_back();
											event_name = candidate_name;
											break;
									}
								} else {
									error_occurred = already_new_event_name_defined_error;
									event::Manager::Instance().error_handler.SendLocalError(error_occurred, (L"���d��`�Ƃ��Ĕ��肳�ꂽ�C�x���g��: " + stack->front()->text));
								}
							} 
						},
						{
							L"<>", [this](std::list<Syntax*>* stack) {
								stack->pop_front();
								if (trigger_type == TriggerType::Invalid) {
									auto word = stack->front()->text;
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
									stack->pop_front();

									for (int i = 0; !stack->empty(); ) {
										if (stack->front()->text == L"~")
											i = 1;
										else {
											origin[(origin[0][1] > -1)][i] = ToDec<Dec>(stack->front()->text.c_str(), nullptr);
											i = 0;
										}
										stack->pop_front();
									}
								} else {
									error_occurred = already_new_trigger_type_defined_error;
									event::Manager::Instance().error_handler.SendLocalError(error_occurred, (L"���d��`�Ƃ��Ĕ��肳�ꂽ�����^�C�v��: " + stack->front()->text));
								}
							}
						},
						{
							L"()", [this](std::list<Syntax*>* stack) {
								stack->pop_front();
								while (!stack->empty()) {
									event_params.push_back(stack->front()->text);
									stack->pop_front();
								}
							}
						},
						{
							L"{}", [this,&case_stack](std::list<Syntax*>* stack) {
								stack->pop_front();
								found_command_sentence = true;

								std::wstring command_name{};
								GenerateFunc generator_candidate{};
								while (!stack->empty()) {
									auto it = words.find(stack->front()->text);
									if (it != words.end()) {
										command_name = std::move(stack->front()->text);
										generator_candidate = it->second;
									} else {
										auto param = std::move(stack->front()->text);
										if (iswdigit(param[0]) || param[0] == L'-') {
											param += std::wstring(L":") + innertype::Number;
										} else if (param[0] == L'\'') {
											param += std::wstring(L":") + innertype::String;
											for (size_t pos = param.find(L'\''); pos != std::wstring::npos; pos = param.find(L'\'')) {
												param.erase(pos, 1);
											}
										}
										command_parameters.push_back(param);
									}
									stack->pop_front();
								}

								if (generator_candidate != nullptr) {
									auto generator = generator_candidate(command_parameters);
									switch (generator.checkParamState()) {
										case KeywordInfo::ParamResult::Lack:
											error_occurred = lack_of_parameters_error;
											event::Manager::Instance().error_handler.SendLocalError(
												error_occurred,
												(L"�R�}���h��: " + command_name).c_str()
											);
											break;
										case KeywordInfo::ParamResult::Medium:
										case KeywordInfo::ParamResult::Maximum:
											if (command_name == L"case") {
												if (!case_stack.front().second) {
													// else�������ׁA�ÖٓI�ɋ��else��}���B
													commands.insert(
														std::prev(case_stack.front().first, 1),
														CommandGraph{
															.command = words[L"else"]({}).Result(),
															.word = L"else"
														}
													);
												}
												case_stack.pop_front();
											} else if (command_name == L"endcase") {
												// else�����J�n�B
												case_stack.push_front({ commands.begin(), false });
											} else if (command_name == L"else") {
												// else�����݂���̂�true�B
												case_stack.front().second = true;
											}

											commands.push_front(CommandGraph{
												.command = generator.Result(),
												.word = command_name,
											});
											break;
										case KeywordInfo::ParamResult::Excess:
											event::Manager::Instance().error_handler.SendLocalError(
												too_many_parameters_warning,
												(L"�R�}���h��: " + command_name).c_str()
											);
											break;
									}
								} else {
									if (!command_name.empty()) {
										error_occurred = command_not_found_error;
										event::Manager::Instance().error_handler.SendLocalError(error_occurred, L"�R�}���h��: " + command_name);
									}
								}
								command_parameters.clear();
							}
						}
					};

					const auto Command_Sentence_Iterator = operator_funcs.find(L"{}");
					for (auto it = syntax->begin(); it != syntax->end(); it++) {
						Syntax *syntax_content = &(*it);
						while (syntax_content != nullptr) {
							stack.push_front(syntax_content);
							if (auto operator_iterator = operator_funcs.find(stack.front()->text); operator_iterator != operator_funcs.end()) {
								if (operator_iterator == Command_Sentence_Iterator) found_command_sentence = true;
								if (stack.size() > 1) {
									it--;
									operator_funcs[std::move(stack.front()->text)](&stack);
								}
								stack.clear();
							} else {
								it++;
							}
							syntax_content = syntax_content->parent;
						}

						if (event::Manager::Instance().error_handler.ShowLocalError(4)) {
							break;
						}

						if (!event_name.empty() && found_command_sentence) {
							// �R�}���h�̐e��ݒ�B
							for (auto command = commands.begin(); command != commands.end(); command++) {
								auto next = std::next(command, 1);
								if (next != commands.end()) {
									command->parent = &(*next);
								}
							}

							auto& target_event = parsing_events[std::move(event_name)];
							target_event.trigger_type = trigger_type;
							target_event.param_names = std::move(event_params);
							for (int i = 0; i < 2; i++)
								target_event.origin[i] = origin[i];

							target_event.commands = std::move(commands);
							trigger_type = TriggerType::Invalid;
							found_command_sentence = false;
						}
					}
				}

				auto& Result() {
					return parsing_events;
				}
			};

			// �œK���@�\
			class Optimizer final {
			public:
				Optimizer() = default;

				// �o�������̍œK���B
				Optimizer(std::list<CommandGraph> *commands) noexcept {
					// �K�؂ȓ��̌`��
					SortOfElse(commands);
				}

				// of/else�R�}���h��std::list����case�̒���ɕ��ג����B
				void SortOfElse(std::list<CommandGraph> *commands) noexcept {
					std::list<std::list<CommandGraph>::iterator> case_iterators{}, endcase_iterators{}, neighbor_caididate_iterators{};
					{						
						// �ŏ��ɁA�S�Ă�case��encase��T���B
						for (auto it = commands->begin(); it != commands->end(); it++) {
							if (it->word == L"case") {
								case_iterators.push_front(it);
							} else if (it->word == L"endcase") {
								endcase_iterators.push_front(it);
							}
						}

						// case��endcase������v���Ȃ��ꍇ�A�\���I�ɂ��������̂ŏI���B
						if (case_iterators.size() != endcase_iterators.size())
							return;
						else if (case_iterators.empty() || endcase_iterators.empty())
							return;

						decltype(neighbor_caididate_iterators) candidate_iterators{};
						int lock_count = 0;
						int iterator_count = 0;
						// 
						for (auto it = case_iterators.front(); it != commands->end(); it++) {
							if (it->word == L"case" && it != *std::next(case_iterators.begin(), iterator_count)) {
								lock_count++;
							} else if (lock_count <= 0 && (it->word == L"of" || it->word == L"else")) {
								candidate_iterators.push_back(it);
							} else if (it->word == L"endcase") {
								if (lock_count > 0) {
									lock_count--;
								} else {
									candidate_iterators.pop_front();
									while (!candidate_iterators.empty()) {
										neighbor_caididate_iterators.push_back(candidate_iterators.front());
										candidate_iterators.pop_front();
									}
									if (++iterator_count < case_iterators.size()) {
										it = *std::next(case_iterators.begin(), iterator_count);
									}
								}
							}
						}
					}
					
 					std::list<std::list<CommandGraph>::iterator> fixed_iterators{};
					auto it = case_iterators.front();
					auto reversed_endcase_iterators = endcase_iterators;
					std::reverse(reversed_endcase_iterators.begin(), reversed_endcase_iterators.end());
					decltype(it) last_endcase_iterator{};

					// 
					while (it != commands->end()) {
						if (std::find(fixed_iterators.begin(), fixed_iterators.end(), it) == fixed_iterators.end()) {
							if (it->word == L"of" || it->word == L"else") {
								fixed_iterators.push_back(it);
								if (!neighbor_caididate_iterators.empty() && std::distance(commands->begin(), it) < std::distance(commands->begin(), neighbor_caididate_iterators.front())) {
									if (it->word != L"else") {
										it->neighbor = &(*neighbor_caididate_iterators.front());
										neighbor_caididate_iterators.pop_front();
									}
								}

								it--;
								if (it->word != L"case" && it->word != L"of" && it->word != L"else") {
									if (!endcase_iterators.empty())
										it->parent = &(*reversed_endcase_iterators.front());
									else
										it->parent = &(*last_endcase_iterator);
								}
								it++;
							} else if (it->word == L"endcase") {
								fixed_iterators.push_back(case_iterators.front());
								fixed_iterators.push_back(endcase_iterators.front());

								it = case_iterators.back();
								last_endcase_iterator = reversed_endcase_iterators.front();

								case_iterators.pop_front();
								endcase_iterators.pop_front();
								reversed_endcase_iterators.pop_front();
								continue;
							}
						}
						it++;
					}
					it = it;
				}
			};

			std::unordered_map<std::wstring, Event> events;
			std::wstring ename;

			error::ErrorContent *parser_abortion_error{};
		public:
			// �����͂��A���̌��ʂ�Ԃ��B
			Context ParseLexical(const std::wstring& Sentence) const noexcept {
				LexicalParser lexparser(Sentence);
				return lexparser.Result();
			}

			// �����͂ƌ^������s���A���̌��ʂ�Ԃ��B
			Context ParseBasic(const std::wstring& Sentence) const noexcept {
				return ParseLexical(Sentence);
			}

			Parser() {
				if (parser_abortion_error == nullptr)
					parser_abortion_error = error::UserErrorHandler::MakeError(event::Manager::Instance().error_class, L"�C�x���g��͂������I�����܂����B", MB_OK | MB_ICONERROR, 1);
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

	namespace {
		template<typename T>
		bool EvaluteValues(const std::any& Left_Value, const std::any& Right_Value, bool(*cmp)(const T&, const T&)) noexcept {
			T left = (Left_Value.type() == typeid(T) ? std::any_cast<T>(Left_Value) : std::any_cast<std::reference_wrapper<T>>(Left_Value).get());
			T right = (Right_Value.type() == typeid(T) ? std::any_cast<T>(Right_Value) : std::any_cast<std::reference_wrapper<T>>(Right_Value).get());
			
			return cmp(left, right);
		}

		template<typename T>
		bool EqualValues(const T& Left_Value, const T& Right_Value) noexcept {
			return Left_Value == Right_Value;
		}

		template<typename T>
		bool NotEqualValues(const T& Left_Value, const T& Right_Value) noexcept {
			return Left_Value != Right_Value;
		}

		template<typename T>
		bool LessEqualValues(const T& Left_Value, const T& Right_Value) noexcept {
			return Left_Value <= Right_Value;
		}

		template<typename T>
		bool HighEqualValues(const T& Left_Value, const T& Right_Value) noexcept {
			return Left_Value >= Right_Value;
		}

		template<typename T>
		bool LessValues(const T& Left_Value, const T& Right_Value) noexcept {
			return Left_Value < Right_Value;
		}

		template<typename T>
		bool HighValues(const T& Left_Value, const T& Right_Value) noexcept {
			return Left_Value > Right_Value;
		}

		template<typename T>
		std::unordered_map<std::wstring, bool(*)(const T&, const T&)> compares{
			{ L"==", &EqualValues<T> },
			{ L"!", &NotEqualValues<T> },
			{ L"<", &LessValues<T> },
			{ L">", &HighValues<T> },
			{ L"<=", &LessEqualValues<T> },
			{ L">=", &HighEqualValues<T> }
		};
	}

	// ��������]������
	bool Manager::ConditionManager::Evalute(const std::wstring& Mode, const std::any& Right_Value) noexcept {
		can_execute = true;
		auto& target_type = target_value.type();

		// �����^�݂̂��r����B
		if (target_type == Right_Value.type()) {
			if (target_type == typeid(int) || target_type == typeid(std::reference_wrapper<int>)) {
				can_execute = EvaluteValues<int>(target_value, Right_Value, compares<int>[Mode]);
			} else if (target_type == typeid(Dec)) {
				can_execute = EvaluteValues<Dec>(target_value, Right_Value, compares<Dec>[Mode]);
			} else if (target_type == typeid(std::wstring)) {
				can_execute = EvaluteValues<std::wstring>(target_value, Right_Value, compares<std::wstring>[Mode]);
			}
		}
		return can_execute;
	}

	void Manager::ConditionManager::FreeCase() {
		target_value.reset();
	}

	Manager::Manager() {
		error_class = error_handler.MakeErrorClass(L"�C�x���g�G���[");
		call_error = error_handler.MakeError(error_class, L"�w�肳�ꂽ�C�x���g��������܂���B", MB_OK | MB_ICONERROR, 2);
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

	bool Manager::Push(const std::wstring& EName) noexcept {
		auto candidate = events.find(EName);
		if (candidate != events.end()) {
			Program::Instance().EventExecuterInstance().PushEvent(candidate->first);
			return true;
		}
		return false;
	}

	bool Manager::Call(const std::wstring& EName) noexcept {
		auto candidate = events.find(EName);
		if (candidate != events.end()) {
			auto& event_name = std::any_cast<std::wstring&>(Program::Instance().var_manager.Get(variable::Executing_Event_Name));
			Program::Instance().var_manager.Get(variable::Executing_Event_Name) = (event_name += std::wstring(EName) + L"\n");
			Program::Instance().EventExecuterInstance().Execute(candidate->second.commands);
			event_name.erase(event_name.find(EName + L"\n"));
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
			L"[�_�~�[]\n<n>\n()\n" + Command_Sentence
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
			std::wstring(L"[�_�~�[]\n") +
			L'<' + TSentence + L'>' +
			std::wstring(L"\n()\n{}")
		).Result().begin()->second.trigger_type;
	}

	void command::Alias::Execute() {

	}

	void command::Bind::Execute() {
		static uint64_t count = 0;
		EventGenerator::Parser parser;
		const auto Event_Name = L"__�L�[�C�x���g_" + std::to_wstring(count++);
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