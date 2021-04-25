#include "Event.hpp"

#include "Canvas.hpp"

#include "Engine.hpp"

#include <queue>
#include <chrono>
#include <forward_list>

#define DYNAMIC_COMMAND(NAME) class NAME : public DynamicCommand
#define DYNAMIC_COMMAND_CONSTRUCTOR(NAME) NAME(const std::vector<std::wstring>& Param) : DynamicCommand(Param)

namespace karapo::event {
	using Context = std::queue<std::wstring, std::list<std::wstring>>;

	namespace command {
		// Entity����n�R�}���h
		class EntityCommand : public Command {
			std::wstring entity_name;
		protected:
			std::shared_ptr<karapo::Entity> GetEntity() const noexcept {
				return Program::Instance().entity_manager.GetEntity(entity_name.c_str());
			}
		};

		// ���ʂ̃R�}���h
		// executed�ϐ��ɂ��A���s�]�X�̏����Ǘ�����B
		class StandardCommand : public Command {
			bool executed = false;	// ���s�������ۂ��B
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

		// ���I�R�}���h�������ۃN���X�B
		// �������R�}���h���s���ɓǂݍ��ވׂ̃����o�֐������B
		class DynamicCommand : public StandardCommand {
			// �ǂݍ��ޗ\��̈������B
			std::vector<std::wstring> param_names{};
		protected:
			DynamicCommand() = default;

			DynamicCommand(const decltype(param_names)& Param) noexcept {
				param_names = Param;
			}

			// �������R�}���h���s���ɓǂݍ��ޕK�v�����邩�ۂ��B
			bool MustSearch() const noexcept {
				return !param_names.empty();
			}

			// �w�肵���ԍ������������ǂݍ��ށB
			template<typename T, const bool Get_Param_Name = false>
			T GetParam(const int Index) const {
				if constexpr (!Get_Param_Name) {
					auto [vname, vtype] = Default_ProgramInterface.GetParamInfo(param_names[Index]);
					if constexpr (!std::is_same_v<T, std::any>) {
						// �^����������ꍇ�A�萔�Ȃ̂ł��̒l���擾�B
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
							// �^���Ȃ��ꍇ�͕ϐ��Ǘ��I�u�W�F�N�g����擾�B
							return std::any_cast<T>(Program::Instance().var_manager.Get<false>(vname));
						}
					} else {
						// std::any���w�肳�ꂽ�ꍇ�A�ϐ����璼�ڎ擾�B
						return Program::Instance().var_manager.Get<false>(vname);
					}
				} else {
					// ��������Ԃ��B
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
						if (wcslen(fp) <= 0)
							value = fv;
						else if (wcslen(ip) <= 0)
							value = iv;
						else
							value = value_name;
					}
					to->push_back(value);
				}
			}
		};

		// �ϐ�
		DYNAMIC_COMMAND(Variable final) {
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

			DYNAMIC_COMMAND_CONSTRUCTOR(Variable) {}

			~Variable() noexcept final {}

			void Execute() override {
				if (MustSearch()) {
					varname = GetParam<std::wstring, true>(0);
					value = GetParam<std::any>(1);
					if (value.type() == typeid(std::nullptr_t)) {
						auto value_name = GetParam<std::wstring, true>(1);
						auto [fv, fp] = ToDec<Dec>(value_name.c_str());
						auto [iv, ip] = ToInt(value_name.c_str());
						if (wcslen(fp) <= 0)
							value = fv;
						else if (wcslen(ip) <= 0)
							value = iv;
						else
							value = value_name;
					}
				}
				Program::Instance().var_manager.MakeNew(varname) = value;
				StandardCommand::Execute();
			}
		};
		
		// �����Ώېݒ�
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
				Program::Instance().event_manager.SetCondTarget(value);
				StandardCommand::Execute();
			}
		};

		// ������
		DYNAMIC_COMMAND(Of final) {
			std::wstring condition_sentence;
		public:
			Of(const std::wstring& Condition_Sentence) noexcept {
				condition_sentence = Condition_Sentence;
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Of) {}

			~Of() noexcept final {}

			void Execute() override {
				if (MustSearch()) {
					condition_sentence = GetParam<std::wstring>(0);
				}
				Program::Instance().event_manager.Evalute(condition_sentence);
				StandardCommand::Execute();
			}
		};

		// �����I��
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

		// �摜��ǂݍ��݁A�\��������B
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
				music->Load(path);
				Program::Instance().entity_manager.Register(music);
				StandardCommand::Execute();
			}
		};

		// ���ʉ�
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
				sound->Load(path);
				Program::Instance().entity_manager.Register(sound);
				StandardCommand::Execute();
			}
		};

		// �{�^��
		DYNAMIC_COMMAND(Button final) {
			std::shared_ptr<karapo::entity::Button> button;
			std::wstring path{};
		public:
			Button(const std::wstring& Name,
				const WorldVector& WV,
				const std::wstring& Image_Path,
				const WorldVector& Size) noexcept
			{
				button = std::make_shared<karapo::entity::Button>(Name, WV, Size);
				path = Image_Path;
			}

			DYNAMIC_COMMAND_CONSTRUCTOR(Button) {}

			~Button() noexcept final {}

			void Execute() final {
				if (MustSearch()) {
					auto name = GetParam<std::wstring>(0);
					auto x = GetParam<Dec>(1);
					auto y = GetParam<Dec>(2);
					auto w = GetParam<Dec>(3);
					auto h = GetParam<Dec>(4);
					path = GetParam<std::wstring>(5);
					button = std::make_shared<karapo::entity::Button>(name, WorldVector{ x, y }, WorldVector{ w, h });
				}
				button->Load(path);
				Program::Instance().entity_manager.Register(button);
				StandardCommand::Execute();
			}
		};

		namespace entity {
			// Entity�̈ړ�
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
					auto ent = Program::Instance().entity_manager.GetEntity(entity_name);
					ent->Teleport(move);
					StandardCommand::Execute();
				}
			};

			// Entity�̍폜�B
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
					if (entity_name == L"__all" || entity_name == L"__�S��") {
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
					} else {
						Program::Instance().entity_manager.Kill(entity_name);
					}
					StandardCommand::Execute();
				}
			};
		}

		// �L�[���͖��̃R�}���h����
		class Bind final : public StandardCommand {
			std::wstring key_name{}, command_sentence{};
		public:
			Bind(const std::wstring& Key_Name, const std::wstring& Command_Sentence) noexcept {
				key_name = Key_Name;
				if (key_name[0] == L'+')
					key_name.erase(key_name.begin());
				command_sentence = Command_Sentence;
			}

			void Execute() override;
		};

		// �R�}���h�̕ʖ�
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
					kind_name = GetParam<std::wstring>(0);
					potency = GetParam<int>(2);
				}
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

		// DLL�A�^�b�`
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

		// �C�x���g�ďo
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

		namespace layer {
			// ���C���[����(�w��ʒu)
			DYNAMIC_COMMAND(Make final) {
				std::wstring kind_name{}, layer_name{};
				int index = 0;
				std::unordered_map<std::wstring, bool (Canvas::*)(const std::wstring&, const int)> create{};
			public:
				Make(const int Index, const std::wstring & KN, const std::wstring & LN) noexcept : Make(std::vector<std::wstring>{}) {
					index = Index;
					kind_name = KN;
					layer_name = LN;
				}

				DYNAMIC_COMMAND_CONSTRUCTOR(Make) {
					create[L"scroll"] =
						create[L"�X�N���[��"] =
						create[L"relative"] =
						create[L"���Έʒu"] = &Canvas::CreateRelativeLayer;

					create[L"fixed"] =
						create[L"�Œ�"] =
						create[L"absolute"] =
						create[L"��Έʒu"] = &Canvas::CreateAbsoluteLayer;
				}

				~Make() final {}

				void Execute() final {
					if (MustSearch()) {
						index = GetParam<int>(0);
						kind_name = GetParam<std::wstring>(1);
						layer_name = GetParam<std::wstring>(2);
					}
					auto it = create.find(kind_name);
					if (it != create.end()) {
						(Program::Instance().canvas.*it->second)(layer_name, index);
					}
					StandardCommand::Execute();
				}
			};

			// ���C���[�ύX
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
					Program::Instance().canvas.SelectLayer(layer_name);
					StandardCommand::Execute();
				}
			};

			// ���Έʒu���C���[�̊�ݒ�
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
					auto ent = Program::Instance().entity_manager.GetEntity(entity_name);
					Program::Instance().canvas.SetBasis(ent, layer_name);
					StandardCommand::Execute();
				}
			};

			// ���C���[�폜
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
					Program::Instance().canvas.DeleteLayer(name);
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

			// Entity���ւ��S�Ă�Manager���X�V
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

	// �C�x���g�̃R�}���h���s�N���X
	class Manager::CommandExecuter final {
		bool called_result = false;

		Event::Commands commands;	// ���s���̃R�}���h
		Event::Commands ended;		// ���s���I�����R�}���h

		// ��̃R�}���h�����s����B
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

		// �R�}���h�S�̂����s����B
		void Execute() noexcept {
			while (!commands.empty()) {
				// �Ď��s�R�}���h
				CommandPtr recycled = Execute(std::move(commands.front()));
				if (recycled == nullptr)
					commands.pop_front();
				else // �Ċi�[����B
					commands.push_front(std::move(recycled));
			}
		}

		[[nodiscard]] auto&& Result() noexcept {
			called_result = true;
			return std::move(ended);
		}

		~CommandExecuter() noexcept {}
	};

	// �C�x���g�����N���X
	// �C�x���g�t�@�C���̉�́A�R�}���h�̐����A�C�x���g�̐ݒ�E�������s���B
	class EventGenerator final : private Singleton {
	public:
		// ��̓N���X
		class Parser final {
			// �ꕶ�����̉�͊�B
			// �󔒂���Ƃ���P��P�ʂ̉�͌��ʂ�context�Ƃ��Ĕr�o�B
			class LexicalParser final {
				Context context;
			public:
				LexicalParser(const std::wstring Sentence) noexcept {
					auto PushWord = [](Context  *const context, std::wstring *const text) noexcept {
						wprintf_s(L"Pushed:%s\n", text->c_str());
						context->push(*text);
						text->clear();
					};

					// �ǂݍ������Ƃ����P�ꂪ�A������Ƃ��ď�����Ă��邩�𔻒肷��ׂ̕ϐ�
					// �l��true�̊ԁA��͊�̓X�y�[�X����؂蕶���Ƃ��ĔF�����Ȃ��Ȃ�B
					bool is_string = false;
					auto text = std::wstring(L"");

					for (auto c : Sentence) {
						if (!is_string) {
							// �X�y�[�X����
							if (IsSpace(c) || c == L'\0' || c == L'\n') {
								// ���߂��񂾕�����P��Ƃ��Ċi�[
								if (!text.empty()) {
									PushWord(&context, &text);
								}

								if (c == L'\n') {
									text = L'\n';
									PushWord(&context, &text);
								}
								continue;
							} else if (c == L'\r') {
								// ���A�R�[�h�͖����B
								continue;
							}

							// ���Z�q����
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
									// ���߂��񂾕�����P��Ƃ��Ċi�[
									if (!text.empty()) {
										PushWord(&context, &text);
									}
									// �L������
									text = c;
									PushWord(&context, &text);
									continue;
								}
								break;
							}
						} else {
							// ���Z�q����
							switch (c) {
								case L'\'':
									is_string = false;
									text += c;
									PushWord(&context, &text);
									continue;
							}
						}
						// �ǂݍ��񂾕�����t��������
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

			// SC����EC�܂ł͈̔͂̕��͂���͂����͊�B
			// ����ȊO�ɂ́A��͒����ǂ�����\�����邽�߂̊֐��Q�����B
			template<wchar_t SC, wchar_t EC>
			struct SubParser {
				static constexpr bool IsValidToStart(const wchar_t Ch) noexcept { return SC == Ch; }
				static constexpr bool IsValidToEnd(const wchar_t Ch) noexcept { return EC == Ch; }

				~SubParser() noexcept {
					MYGAME_ASSERT(!parsing);
				}
			protected:
				// ��͒����ǂ����B
				bool IsParsing() const noexcept { return parsing; }

				// ��͊J�n��錾����B
				void StartParsing() noexcept {
					MYGAME_ASSERT(!parsing);
					parsing = true;
				}

				// ��͒�~��錾����B
				void EndParsing() noexcept {
					MYGAME_ASSERT(parsing);
					parsing = false;
				}
			private:
				bool parsing = false;
			};

			// �C�x���g������͊�B
			// ���ɃC�x���g���A�����^�C�v�A�C�x���g�ʒu����͂��A
			// ���ʂ�K�؂Ȍ^�ɕϊ����Ĕr�o����B
			class InformationParser final {
				// �C�x���g���̉�͂ƕϊ����s���N���X 
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

				// �C�x���g�����^�C�v�y�шʒu�̉�͂ƕϊ����s���N���X
				class ConditionParser final : public SubParser<L'<', L'>'> {
					const std::unordered_map<std::wstring, TriggerType> Trigger_Map{
						{ L"n", TriggerType::None },
						{ L"t", TriggerType::Trigger },
						{ L"b", TriggerType::Button },
						{ L"l", TriggerType::Load }
					};

					// �����^�C�v
					TriggerType tt = TriggerType::Invalid;

					// �ŏ��E�ő�X���W
					Dec minx{}, maxx{};
					// �ŏ��E�ő�Y���W
					Dec miny{}, maxy{};
					// ��͌��ʊi�[�p�|�C���^
					Dec *current = &minx;
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
							if (tt == TriggerType::Invalid) {
								// �����^�C�v���
								auto it = Trigger_Map.find(Sentence);
								if (it != Trigger_Map.end()) {
									tt = it->second;
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

					// ��������context�̉�͂��J�n�����B
					// ��{�I�ɁA�S�Ẳ�͊킪���context����͂���B
					// 

					auto& Sentence = context->front();
					std::valarray<bool> result{ false, false };	// �e���e�̉�͌���

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
						// ��s�̉�͂�2�ȏ�̌��ʂ�����ꂽ�ꍇ�͕s���Ƃ���B
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

			// �C�x���g�̈�������͂���N���X
			class ParamParser final : public SubParser<L'(', L')'> {
				std::vector<std::wstring> param_names{};
			public:
				auto Interpret(Context *const context) noexcept {
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
								*(param_names.end() - 1) = Sentence;
							} else {
								param_names.push_back({});
							}
						}
					} else {
						if (IsValidToStart(Sentence[0])) {
							StartParsing();
							param_names.push_back({});
						}
					}
					context->pop();
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
		public:
			// �R�}���h��̓N���X
			// �P��Ɋ�Â��R�}���h�̐����ƁA����̌��ʂ�r�o����B
			class CommandParser final : public SubParser<L'{', L'}'> {
				// �\���
				std::unordered_map<std::wstring, GenerateFunc> words;

				std::vector<std::wstring> parameters;	// ����
				Event::Commands commands;	// 
				GenerateFunc liketoRun;				// �����֐������s���邽�߂̊֐��|�C���^�B
				std::wstring generating_command_name = L"";
				bool request_abort = false;
				bool finish_sentence = false;

				void CheckCommandWord(const std::wstring& text) noexcept(false) {
					// �R�}���h�����֐����擾�B
					auto gen = words.at(text);
					if (liketoRun == nullptr) {
						liketoRun = gen;
						generating_command_name = text;
					} else {
						// ���ɐ����֐����ݒ肳��Ă��鎞:
						throw std::runtime_error("��s�ɃR�}���h��2�ȏ㏑����Ă��܂��B");
					}
				}

				void CheckArgs(const std::wstring& text) noexcept(false) {
					// �����̌^���`�F�b�N����B

					// �����̏��B
					const auto Index = text.find(L':');
					const auto Type = (Index == text.npos ? L"" : text.substr(Index));

					// ������(�ϐ��ł͂Ȃ�)�萔�l���ǂ����B
					const bool Is_Const = !Type.empty();
					if (Is_Const) {
						parameters.push_back(text);
					} else {
#if 0
						// �ϐ��T��
						auto& value = Program::Instance().var_manager.Get<true>(text);
						if (value.type() == typeid(int))
							parameters.push_back(std::to_wstring(std::any_cast<int>(value)) + std::wstring(L":") + innertype::Number);
						else if (value.type() == typeid(Dec))
							parameters.push_back(std::to_wstring(std::any_cast<Dec>(value)) + std::wstring(L":") + innertype::Number);
						else
							parameters.push_back(std::any_cast<std::wstring>(value) + std::wstring(L":") + innertype::String);
#else
						const auto Event_Name = std::any_cast<std::wstring>(Program::Instance().var_manager.Get<false>(L"__�������C�x���g"));
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
									// �������\���ɐς܂�Ă��鎞:
									if (f.is_dynamic) {
										// ���I�R�}���h�̓C�x���g�̃R�}���h�ɒǉ��B
										commands.push_back(std::move(result));
										if (generating_command_name == L"of") {
											auto endof = words.at(L"__endof")({}).Result();
											commands.insert(commands.end() - 1, std::move(endof));
										} else if (generating_command_name == L"kill") {
											auto force = words.at(L"__entity�����X�V")({}).Result();
											commands.push_back(std::move(force));
										}
									} else if (f.is_static) {
										// �ÓI�R�}���h�͑����s�B
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
						// HACK: CheckCommandWord��CheckArgs���܂߁A�u'�v���̈����Ɋ܂܂�Ȃ������ɑ΂��鏈�������P����B
						finish_sentence = (text == L"\n");
						if (!finish_sentence) {
							if (!IsValidToEnd(text[0])) {
								bool is_command = false;

								// �����ŁA��s���̃R�}���h�Ƃ��ׂ̈̈���������m�F���Ă����B
								// �R�}���h�m�F
								try {
									CheckCommandWord(text);
									is_command = true;
								} catch (std::runtime_error& e) {
									// ���CheckCommandWord����̗�O�������ŕߑ��B
									if (liketoRun != nullptr) {
										MessageBoxA(nullptr, e.what(), "�G���[", MB_OK | MB_ICONERROR);
										request_abort = true;
									}
								} catch (std::out_of_range& e) {}

								if (!is_command && !request_abort) {
									try {
										CheckArgs(text);			// �����m�F�B
									} catch (std::out_of_range& e) {
										const bool Is_Variable = (generating_command_name == L"var" || generating_command_name == L"�ϐ�");
										// ���CheckArgs����̗�O�������ŕߑ��B
										if (Is_Variable) {
											// �ϐ��R�}���h�̈����������ꍇ�͕ϐ����Ƃ��Ĉ����ɐςށB
											parameters.push_back(text);
										} else {
											MessageBoxA(nullptr, "����`�̕ϐ����g�p����Ă��܂��B", "�G���[", MB_OK | MB_ICONERROR);
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
							// '}' ���폜�B
							context->pop();
							return;
						} else if (finish_sentence) {
							liketoRun = nullptr;
							parameters.clear();
						}
					}
					context->pop();
					Interpret(context);
				}
			public:
				// �R�}���h�̕ʖ����쐬����B
				void MakeAlias(const std::wstring& S1, const std::wstring& S2) noexcept {
					words[S1] = words.at(S2);
				}

				bool NeedAbortion() const noexcept {
					return request_abort;
				}

				CommandParser(Context *context) noexcept {
					EventGenerator::Instance().cmdparser = this;
					
					// �q�b�g������P���o�^����B

					Program::Instance().dll_manager.RegisterExternalCommand(&words);

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
						words[L"�{�^��"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() noexcept -> CommandPtr {
								const auto [Name, Name_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								const auto [VX, VX_Type] = Default_ProgramInterface.GetParamInfo(params[1]);
								const auto [VY, VY_Type] = Default_ProgramInterface.GetParamInfo(params[2]);
								const auto [Img, Img_Type] = Default_ProgramInterface.GetParamInfo(params[3]);
								const auto [SX, SX_Type] = Default_ProgramInterface.GetParamInfo(params[4]);
								const auto [SY, SY_Type] = Default_ProgramInterface.GetParamInfo(params[5]);
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
						words[L"�E�Q"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
									std::make_unique<command::layer::Select>(Name);
								else
									return std::make_unique<command::layer::Select>(params);
								return (Default_ProgramInterface.IsStringType(Name_Type) ? std::make_unique<command::layer::Select>(Name) : nullptr);
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
								return (Default_ProgramInterface.IsStringType(Name_Type) ? std::make_unique<command::layer::Delete>(Name) : nullptr);
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

					// DLL�A�^�b�`
					words[L"attach"] =
						words[L"�ڑ�"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								const auto [Var, Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								return (Default_ProgramInterface.IsStringType(Type) ? std::make_unique<command::Attach>(Var) : nullptr);
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

					// DLL�f�^�b�`
					words[L"detach"] =
						words[L"�ؒf"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								const auto [Var, Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								return (Default_ProgramInterface.IsStringType(Type) ? std::make_unique<command::Detach>(Var) : nullptr);
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

					words[L"call"] =
						words[L"�ďo"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"�ϐ�"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"����"] = [&](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								const auto [Var, Var_Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								if (Default_ProgramInterface.IsUndecidedType(Var_Type))
									return std::make_unique<command::Case>(Var);
								else
									return std::make_unique<command::Case>(params[0]);
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
						words[L"����"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								const auto [Var, Type] = Default_ProgramInterface.GetParamInfo(params[0]);
								if (!Default_ProgramInterface.IsNoType(Type))
									return std::make_unique<command::Of>(Var);
								else
									return std::make_unique<command::Of>(params);
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
						words[L"����I��"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
							.is_static = true,
							.is_dynamic = false
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

					Interpret(context);
				}

				[[nodiscard]] auto Result() noexcept {
					return std::move(commands);
				}
			};

			// �����͂��A���̌��ʂ�Ԃ��B
			Context ParseLexical(const std::wstring& Sentence) const noexcept {
				LexicalParser lexparser(Sentence);
				return lexparser.Result();
			}

			// �^�����肵�A���̌��ʂ�Ԃ��B
			Context DetermineType(Context context) const noexcept {
				TypeDeterminer type_determiner(context);
				return type_determiner.Result();
			}

			// �����͂ƌ^������s���A���̌��ʂ�Ԃ��B
			Context ParseBasic(const std::wstring& Sentence) const noexcept {
				return DetermineType(ParseLexical(Sentence));
			}

			// �C�x���g������͂��A���̌��ʂ�Ԃ��B
			auto ParseInformation(Context* context) const noexcept {
				InformationParser infoparser(context);
				return infoparser.Result();
			}

			// �C�x���g��������͂��A���̌��ʂ�Ԃ��B
			auto ParseArgs(Context* context) const noexcept {
				ParamParser pparser(context);
				return pparser.Result();
			}

			// �R�}���h����͂��A���̌��ʂ�Ԃ��B
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
					Program::Instance().var_manager.MakeNew(L"__�������C�x���g") = name;

					auto params = ParseArgs(&context);
					Event::Commands commands = ParseCommand(&context, &aborted);

					Event event{ .commands = std::move(commands), .trigger_type = trigger, .origin{ min, max }, .param_names=std::move(params) };
					events[name] = std::move(event);
				}

				if (aborted) {
					MessageBoxW(nullptr, L"�C�x���g��͂������I�����܂����B", L"�G���[", MB_OK | MB_ICONERROR);
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

	// ��������]������
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
		auto candidate = events.find(EName);
		if (candidate != events.end()) {
			auto event_name = std::any_cast<std::wstring>(Program::Instance().var_manager.Get<false>(variable::Executing_Event_Name));
			CommandExecuter cmd_executer(std::move(candidate->second.commands));
			Program::Instance().var_manager.Get<false>(variable::Executing_Event_Name) = (event_name += std::wstring(EName) + L"\n");
			cmd_executer.Execute();

			candidate->second.commands = std::move(cmd_executer.Result());
			candidate->second.trigger_type = TriggerType::None;
			event_name.erase(event_name.find(EName + L"\n"));

			Program::Instance().var_manager.Get<false>(variable::Executing_Event_Name) = event_name;
		} else {
			std::wstring message = L"�C�x���g���u" + EName + L"�v��������Ȃ������̂Ŏ��s�ł��܂���B";
			MessageBoxW(nullptr, message.c_str(), L"�C�x���g�G���[", MB_OK | MB_ICONERROR);
			return;
		}
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

	void command::Bind::Execute() {
		static uint64_t count = 0;
		EventGenerator::Parser parser;
		const auto Event_Name = L"__�L�[�C�x���g_" + std::to_wstring(count++);
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