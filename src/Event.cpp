#include "Event.hpp"

#include "Canvas.hpp"

#include "Engine.hpp"

#include <chrono>
#include <forward_list>

namespace karapo::event {
	namespace command {
		Command::~Command() {}

		// Entity����n�R�}���h
		class EntityCommand : public Command {
			String entity_name;
		protected:
			SmartPtr<karapo::entity::Entity> GetEntity() const noexcept {
				return GetProgram()->entity_manager.GetEntity(entity_name.c_str());
			}
		};

		// �ϐ�
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

		// �摜��ǂݍ��݁A�\��������B
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

		// ���ʉ�
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

			// Entity�̈ړ�
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

			// Entity�̍폜�B
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

		// �ʃ��[���h�ւ̈ړ�
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

		// �R�}���h�̕ʖ�
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

		// DLL�A�^�b�`
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

		// �C�x���g�ďo
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

	// �C�x���g�̃R�}���h���s�N���X
	class Manager::CommandExecuter final {
		bool called_result = false;

		Event::Commands commands;	// ���s���̃R�}���h
		Event::Commands ended;		// ���s���I�����R�}���h

		// ��̃R�}���h�����s����B
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
		std::unordered_map<String, Event> events;
	public:
		// ��̓N���X
		class Parser final {
			using Context = std::queue<String, std::list<String>>;

			// �ꕶ�����̉�͊�B
			// �󔒂���Ƃ���P��P�ʂ̉�͌��ʂ�context�Ƃ��Ĕr�o�B
			class LexicalParser final {
				Context context;
			public:
				LexicalParser(const String Sentence) noexcept {
					auto PushWord = [](Context  *const context, karapo::String *const text) noexcept {
						wprintf_s(L"Pushed:%s\n", text->c_str());
						context->push(*text);
						text->clear();
					};

					// �ǂݍ������Ƃ����P�ꂪ�A������Ƃ��ď�����Ă��邩�𔻒肷��ׂ̕ϐ�
					// �l��true�̊ԁA��͊�̓X�y�[�X����؂蕶���Ƃ��ĔF�����Ȃ��Ȃ�B
					bool is_string = false;
					auto text = String(L"");

					for (auto c : Sentence) {
						if (!is_string) {
							// �X�y�[�X����
							if (IsSpace(c) || c == L'\0' || c == L'\n') {
								// ���߂��񂾕�����P��Ƃ��Ċi�[
								if (!text.empty())
									PushWord(&context, &text);

								continue;
							}

							// ���Z�q����
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
									// ���߂��񂾕�����P��Ƃ��Ċi�[
									if (!text.empty()) {
										PushWord(&context, &text);
									}
									// �L������
									text = c;
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

			// SC����EC�܂ł͈̔͂̕��͂���͂����͊�B
			// ����ȊO�ɂ́A��͒����ǂ�����\�����邽�߂̊֐��Q�����B
			template<Char SC, Char EC>
			struct SubParser {
				static constexpr bool IsValidToStart(const Char Ch) noexcept { return SC == Ch; }
				static constexpr bool IsValidToEnd(const Char Ch) noexcept { return EC == Ch; }

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

				// �����^�C�v�̉�͂ƕϊ����s���N���X
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

				// �C�x���g�ʒu�̉�͂ƕϊ����s���N���X
				class ConditionParser final : public SubParser<L'<', L'>'> {
					static constexpr auto DecMin = std::numeric_limits<Dec>::min();

					Dec minx, maxx;		// minx�̎���maxx������O��Ȃ̂ŁA�錾�ʒu��ύX���Ȃ����B
					Dec miny, maxy;		// miny�̎���maxy������O��Ȃ̂ŁA�錾�ʒu��ύX���Ȃ����B
					Dec *current = &minx;	// current��minx����n�܂�B
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

				std::tuple<String, TriggerType, WorldVector, WorldVector> Result()const noexcept {
					return { event_name, trigger, min_origin, max_origin };
				}
			};

			std::unordered_map<String, Event> events;
			String ename;
		public:
			// �R�}���h��̓N���X
			// �P��Ɋ�Â��R�}���h�̐����ƁA����̌��ʂ�r�o����B
			class CommandParser final : public SubParser<L'{', L'}'> {
				// �R�}���h�̏��
				struct KeywordInfo final {
					// ���������R�}���h��Ԃ��B
					std::function<CommandPtr()> Result = []() -> CommandPtr { MYGAME_ASSERT(0); return nullptr; };

					// �����̐����\���ł��邩�ۂ���Ԃ��B
					std::function<bool()> isEnough = []() -> bool { MYGAME_ASSERT(0); return false; };

					// �R�}���h����͒��Ɏ��s����邩�ۂ��ǂ����B
					bool is_static = false;

					// �R�}���h���C�x���g���s���Ɏ��s����邩�ۂ��ǂ����B
					bool is_dynamic = false;
				};
				
				using FUNC = std::function<KeywordInfo(const Array<String>&)>;
				
				// �\���
				std::unordered_map<String, FUNC> words;

				Array<String> parameters;	// ����
				Event::Commands commands;	// 
				FUNC liketoRun;				// �����֐������s���邽�߂̊֐��|�C���^�B
				bool is_string = false;
				bool is_newvar = false;

				void Interpret(Context *context) noexcept {
				restart:
					auto text = context->front();

					if (IsParsing()) {
						if (liketoRun == nullptr) {
							if (text != L"\'") {
								liketoRun = words.at(text);
								is_newvar = (text == L"var" || text == L"�ϐ�");
							} else
								is_string = false;
						} else {
							auto&& f = liketoRun(parameters);
							if (f.isEnough()) {
								MYGAME_ASSERT(f.is_dynamic ^ f.is_static);

								if (f.is_dynamic) {
									commands.push_back(std::move(f.Result()));
								}

								// �R�}���h�𑦎��s
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
							// '}' ���폜�B
							context->pop();
							return;
						}
					}
					context->pop();
					Interpret(context);
				}
			public:
				// �R�}���h�̕ʖ����쐬����B
				void MakeAlias(const String& S1, const String& S2) noexcept {
					words[S1] = words.at(S2);
				}

				CommandParser(Context *context) noexcept {
					GetProgram()->event_manager.SetCMDParser(this);

					// �q�b�g������P���o�^����B
	
					words[L"music"] =
						words[L"���y"] =
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
						words[L"���ʉ�"] =
						words[L"��"] = [](const Array<String>& params) -> KeywordInfo
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
						words[L"�摜"] = 
						words[L"�G"] = [](const Array<String>& params) -> KeywordInfo
					{
						return {
							.Result = [&]() noexcept -> CommandPtr { return std::make_unique<command::Image>(params[0], WorldVector{ ToDec<Dec>(params[1].c_str(), nullptr), ToDec<Dec>(params[2].c_str(), nullptr) }); },
							.isEnough = [params]() -> bool { return params.size() == 3; },
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"teleport"] = 
						words[L"�u�Ԉړ�"] = [](const Array<String>& params) -> KeywordInfo {
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
						words[L"�E�Q"] = [](const Array<String>& params) -> KeywordInfo {
						return {
							.Result = [&]() noexcept -> CommandPtr { return std::make_unique<command::entity::Kill>(params[0]); },
							.isEnough = [params]() -> bool { return params.size() == 1; },
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"alias"] =
						words[L"�ʖ�"] = [](const Array<String>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr { return std::make_unique<command::Alias>(params[0], params[1]); },
							.isEnough = [params]() -> bool { return params.size() == 2; },
							.is_static = true,
							.is_dynamic = false
						};
					};

					// DLL�A�^�b�`
					words[L"attach"] = 
						words[L"�ڑ�"] = [](const Array<String>& params) -> KeywordInfo {
						return {
							.Result = [&]() -> CommandPtr {
								return std::make_unique<command::Attach>(params[0]);
							},
							.isEnough = [params]() -> bool { return params.size() == 1; },
							.is_static = true,
							.is_dynamic = false
						};
					};

					// DLL�f�^�b�`
					words[L"detach"] =
						words[L"�ؒf"] = [](const Array<String>& params) -> KeywordInfo {
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
						words[L"�ďo"] = [](const Array<String>& params) -> KeywordInfo {
						return {
							.isEnough = [params]() -> bool { return params.size() == 1; },
							.is_static = false,
							.is_dynamic = true
						};
					};

					words[L"var"] = 
						words[L"�ϐ�"] = [](const Array<String>& params) -> KeywordInfo {
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
						words[L"����"] = [](const Array<String>& params) -> KeywordInfo {
						return {
							.is_static = true,
							.is_dynamic = false
						};
					};

					words[L"of"] =
						words[L"����"] = [](const Array<String>& params) -> KeywordInfo {
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

	// ��������]������
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