#include "Event.hpp"

#include "Canvas.hpp"

#include "Engine.hpp"

#include <queue>
#include <chrono>
#include <forward_list>

namespace karapo::event {
	namespace {
		std::pair<std::wstring, std::wstring> GetParamInfo(const std::wstring& Param) {
			// �����̏��B
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
		// Entity����n�R�}���h
		class EntityCommand : public Command {
			std::wstring entity_name;
		protected:
			std::shared_ptr<karapo::Entity> GetEntity() const noexcept {
				return GetProgram()->entity_manager.GetEntity(entity_name.c_str());
			}
		};

		// �ϐ�
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

		// �����Ώېݒ�
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

		// ������
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

		// �����I��
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

		// �摜��ǂݍ��݁A�\��������B
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

		// ���ʉ�
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
			// Entity�̈ړ�
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

			// Entity�̍폜�B
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

		// �R�}���h�̕ʖ�
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

		// DLL�A�^�b�`
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

		// �C�x���g�ďo
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

	// �C�x���g�̃R�}���h���s�N���X
	class Manager::CommandExecuter final {
		bool called_result = false;

		Event::Commands commands;	// ���s���̃R�}���h
		Event::Commands ended;		// ���s���I�����R�}���h

		// ��̃R�}���h�����s����B
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
	class Manager::EventGenerator final {
		std::unordered_map<std::wstring, Event> events;
	public:
		// ��̓N���X
		class Parser final {
			using Context = std::queue<std::wstring, std::list<std::wstring>>;

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
									if (c == L'\n') {
										text = L"\n";
										PushWord(&context, &text);
									}
								}
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

				// �����^�C�v�̉�͂ƕϊ����s���N���X
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

				// �C�x���g�ʒu�̉�͂ƕϊ����s���N���X
				class ConditionParser final : public SubParser<L'<', L'>'> {
					static constexpr auto DecMin = std::numeric_limits<Dec>::min();

					Dec minx, maxx;		// minx�̎���maxx������O��Ȃ̂ŁA�錾�ʒu��ύX���Ȃ����B
					Dec miny, maxy;		// miny�̎���maxy������O��Ȃ̂ŁA�錾�ʒu��ύX���Ȃ����B
					Dec *current = &minx;	// current��minx����n�܂�B
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

					// ��������context�̉�͂��J�n�����B
					// ��{�I�ɁA�S�Ẳ�͊킪���context����͂���B
					// 

					auto& Sentence = context->front();
					std::valarray<bool> result{ false, false, false };	// �e���e�̉�͌���

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
				bool is_variable = false;
				bool request_abort = false;

				void CheckCommandWord(const std::wstring& text) noexcept(false) {
					// �R�}���h�����֐����擾�B
					auto gen = words.at(text);
					if (liketoRun == nullptr) {
						liketoRun = gen;
						is_variable = (text == L"var" || text == L"�ϐ�");
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
						// �ϐ��T��
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
							// �������\���ɐς܂�Ă��鎞:
							if (f.is_dynamic) {
								// ���I�R�}���h�̓C�x���g�̃R�}���h�ɒǉ��B
								commands.push_back(std::move(result));
							} else if (f.is_static) {
								// �ÓI�R�}���h�͑����s�B
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
						// HACK: CheckCommandWord��CheckArgs���܂߁A�u'�v���̈����Ɋ܂܂�Ȃ������ɑ΂��鏈�������P����B

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
									// ���CheckArgs����̗�O�������ŕߑ��B
									if (is_variable) {
										// �ϐ��R�}���h�̈����������ꍇ�͕ϐ����Ƃ��Ĉ����ɐςށB
										parameters.push_back(text);
									} else {
										MessageBoxA(nullptr, "����`�̕ϐ����g�p����Ă��܂��B", "�G���[", MB_OK | MB_ICONERROR);
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
							// '}' ���폜�B
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
				// �R�}���h�̕ʖ����쐬����B
				void MakeAlias(const std::wstring& S1, const std::wstring& S2) noexcept {
					words[S1] = words.at(S2);
				}

				bool NeedAbortion() const noexcept {
					return request_abort;
				}

				CommandParser(Context *context) noexcept {
					GetProgram()->event_manager.SetCMDParser(this);

					// �q�b�g������P���o�^����B
	
					GetProgram()->dll_manager.RegisterExternalCommand(&words);

					words[L"music"] =
						words[L"���y"] =
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
						words[L"���ʉ�"] =
						words[L"��"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
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
						words[L"�摜"] = 
						words[L"�G"] = [](const std::vector<std::wstring>& params) -> KeywordInfo
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
						words[L"�u�Ԉړ�"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"�E�Q"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"�ʖ�"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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

					// DLL�A�^�b�`
					words[L"attach"] = 
						words[L"�ڑ�"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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

					// DLL�f�^�b�`
					words[L"detach"] =
						words[L"�ؒf"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"�ďo"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
						return {
							.isEnough = [params]() -> bool { return params.size() == 1; },
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"var"] = 
						words[L"�ϐ�"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"�t�B���^�["] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"����"] = [&](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"����"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
						words[L"����I��"] = [](const std::vector<std::wstring>& params) -> KeywordInfo {
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
					MessageBoxW(nullptr, L"�C�x���g��͂������I�����܂����B", L"�G���[", MB_OK | MB_ICONERROR);
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

	// ��������]������
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