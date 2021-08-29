#include "Engine.hpp"

#include <fstream>
#include <filesystem>

namespace karapo {
	void Program::OnInit() {
		engine.OnInit(this);
		canvas.CreateAbsoluteLayer(L"�f�t�H���g���C���[");
		canvas.SelectLayer(L"�f�t�H���g���C���[");
		entity_manager.Register(std::make_shared<entity::Mouse>());
	}

	int Program::Main() {
#if 0
		try {
			// DLL�t�H���_����DLL��S�ēǂݍ��ށB
			for (const auto& dll_dir : std::filesystem::directory_iterator(L"DLL")) {
				auto& path = dll_dir.path();
				if (In(path.c_str(), L".dll"))
					dll_manager.Load(path);
			}
		} catch (std::filesystem::filesystem_error& error) {
			//MessageBoxA(nullptr, error.what(), "�G���[", MB_OK | MB_ICONERROR);
		}
#endif
		//dll_manager.LoadedInit();
		var_manager.MakeNew(L"window_width") = WindowSize().first;
		var_manager.MakeNew(L"window_height") = WindowSize().second;
		event::Manager::Instance().LoadEvent(L"main.ks");
		while (UpdateMessage() == 0) {
			engine.UpdateKeys();
			engine.UpdateBindedKeys();
			engine.ClearScreen();
			Frame();
			//dll_manager.Update();
			canvas.Update();
			event_manager.Update();
			entity_manager.Update();
			engine.FlipScreen();
			error::UserErrorHandler::ShowGlobalError(4);
			EventExecuterInstance().Update();
		}
		return 0;
	}

	event::EventExecuter& Program::EventExecuterInstance() {
		static event::EventExecuter event_executer {};
		return event_executer;
	}

	HWND Program::MainHandler() const noexcept {
		return handler;
	}

	void Program::Frame() {
		static std::chrono::system_clock::time_point frame_start_time;
		frame_start_time = GetTime();
		while ((GetTime() - frame_start_time).count() < 1000 / 120) {}
		frame_start_time = GetTime();
	}

	event::EventEditor* Program::MakeEventEditor() {
		editors.push_back({});
		return &editors.back();
	}

	void Program::FreeEventEditor(event::EventEditor* editor) {
		auto it = std::find_if(editors.begin(), editors.end(), [editor](event::EventEditor ee) { return &ee == editor; });
		if (it != editors.end())
			editors.erase(it);
	}

	std::chrono::system_clock::time_point Program::GetTime() {
		return std::chrono::system_clock::now();
	}

	namespace variable {
		Manager::Manager() {
			vars[L"null"] = nullptr;
			vars[Managing_Var_Name] = std::wstring(L"");		// �Ǘ����̕ϐ��̖��O
			vars[Executing_Event_Name] = std::wstring(L"");

			vars[L"__�������C�x���g"] = std::wstring(L"");

			vars[L"__�ϐ�����"] = 1 << 0;					// 
			vars[L"__�C�x���g����"] = 1 << 1;				// 
			vars[L"__�L��������"] = 1 << 2;					// 
		}

		// Name���画�ʂ��ēK�؂Ȍ^�̕ϐ����쐬����B
		std::any& Manager::MakeNew(const std::wstring& Name) noexcept {
			std::any_cast<std::wstring&>(vars[Managing_Var_Name]) += Name + L"\n";
			// �쐬���ꂽ�ϐ���Ԃ��ׂ̕ϐ��B
			std::any *returnable_candidate{};
			auto at_pos = Name.find(L'@');

			// Record�^������ȊO�̏ꍇ�̏����B
			if (const size_t Pos = Name.find(L'.', (at_pos != std::wstring::npos ? at_pos : 0)); Pos != std::wstring::npos) {
				// �ϐ����Ɂu.�v���܂܂�Ă�����ARecord�^�Ƃ��ĔF������B
				returnable_candidate = &MakeStruct(Name.substr(0, Pos), Name.substr(Pos + 1));
			} else
				returnable_candidate = &vars[Name];	// �����o�������Ȃ��A�P�̂̕ϐ����쐬����B

			return *returnable_candidate;
		}

		// Record�^�ϐ��̃����o�ϐ����쐬����B
		std::any& Manager::MakeStruct(const std::wstring& Struct_Name, const std::wstring& Member_Name) noexcept {
			// Record�̕ϐ�����T���B
			auto record_candidate = vars.find(Struct_Name);
			Record *record{};
			if (record_candidate == vars.end()) {
				vars[Struct_Name] = Record();	// ���݂��Ȃ��ꍇ�A�V�����\���̕ϐ����쐬�B
				record_candidate = vars.find(Struct_Name);
				record = &std::any_cast<Record&>(record_candidate->second);
				goto get_from_map_pair;
			} else if (IsReference(record_candidate->second)) {
				record = &std::any_cast<Record&>(std::any_cast<std::reference_wrapper<std::any>&>(record_candidate->second).get());
			} else if (record_candidate->second.type() != typeid(Record)) {
				record_candidate->second = Record();		// �^���Ⴄ�ꍇ�A�l���㏑���B
				goto get_from_map_pair;
			} else {
			get_from_map_pair:
				record = &std::any_cast<Record&>(record_candidate->second);
			}
			// �\���̕ϐ��ɑ΂��āA�����o�ϐ����쐬���ĕԂ��B
			return record->members[Member_Name];
		}

		std::any& Manager::Get(const std::wstring& Name) noexcept {
			if (IsRecordName(Name)) {
				return GetStruct(Name.substr(0, Name.find(L'.')), Name.substr(Name.find(L'.') + 1));
			} else {
				auto var = vars.find(Name);
				if (var == vars.end()) {
					return GetLocal(Name);
				}
				return var->second;
			}
		no_member:
			return vars[L"null"];
		}

		std::any& Manager::GetLocal(const std::wstring& Name) noexcept {
			auto event_name = std::any_cast<std::wstring>(Get(variable::Executing_Event_Name));
			if (!event_name.empty()) {
				event_name.pop_back();
				auto pos = event_name.rfind(L'\n');
				if (pos != std::wstring::npos)
					event_name = event_name.substr(pos + 1);

				// �ϐ���������Ȃ������ꍇ�A
				// ���[�J���ϐ���T���B
				auto var = vars.find(event_name + L'@' + Name);
				if (var != vars.end())
					return var->second;
			}
			return vars[L"null"];
		}

		std::any& Manager::GetStruct(const std::wstring& Struct_Name, const std::wstring& Member_Name) noexcept {
			auto *record_candidate = &Get(Struct_Name);
			if (!IsRecord(*record_candidate)) {
				record_candidate = &GetLocal(Struct_Name);
			}
			if (IsRecord(*record_candidate)) {
				Record *record{};
				if (!IsReference(*record_candidate))
					record = &std::any_cast<Record&>(*record_candidate);
				else
					record = &std::any_cast<Record&>(std::any_cast<std::reference_wrapper<std::any>&>(*record_candidate).get());

				auto member = record->members.find(Member_Name);
				if (member != record->members.end()) {
					return member->second;
				}
			}
			return vars.at(L"null");
		}

		bool Manager::IsRecord(const std::any& Var) const noexcept {
			return (Var.type() == typeid(Record) || (IsReference(Var) && std::any_cast<std::reference_wrapper<std::any>>(Var).get().type() == typeid(Record)));
		}

		bool Manager::IsReference(const std::any& Var) const noexcept {
			return Var.type() == typeid(std::reference_wrapper<std::any>);
		}

		void Manager::Delete(const std::wstring& Name) noexcept {
			const auto Pos = std::any_cast<std::wstring>(vars[Managing_Var_Name]).find(Name);
			if (Pos != std::wstring::npos) {
				auto var = std::any_cast<std::wstring>(vars[Managing_Var_Name]);
				var.erase(Pos, Name.size());
				vars[Managing_Var_Name] = var;
				vars.erase(Name);
			}
		}

		bool Manager::IsRecordName(const std::wstring& Var_Name) const noexcept {
			return (Var_Name.find(L'.') != Var_Name.npos);
		}
	}

	namespace dll {
		void Manager::Attach(const std::wstring& Path) {
			dlls[Path].mod = LoadLibraryW(Path.c_str());
		}

		void Manager::Load(const std::wstring& Path) {
			Attach(Path);
			auto& dll = dlls.at(Path);
			dll.Init = reinterpret_cast<DLL::Initializer>(GetProcAddress(dll.mod, "KarapoDLLInit"));
			dll.Update = reinterpret_cast<DLL::LoopableMain>(GetProcAddress(dll.mod, "KarapoUpdate"));
			dll.RegisterExternalCommand = reinterpret_cast<DLL::EventRegister>(GetProcAddress(dll.mod, "RegisterCommand"));
		}

		void Manager::LoadedInit() {
			for (auto& dll : dlls) {
				dll.second.Init(Default_ProgramInterface);
			}
		}

		void Manager::Update() {
			std::vector<std::wstring> detach_lists;
			for (auto& dll : dlls) {
				if (!dll.second.Update()) {
					detach_lists.push_back(dll.first);
				}
			}

			while (!detach_lists.empty()) {
				auto path = detach_lists.back();
				Detach(path);
				detach_lists.pop_back();
			}
		}

		void Manager::RegisterExternalCommand(std::unordered_map<std::wstring, event::GenerateFunc>* words) {
#if 0
			for (auto& dll : dlls) {
				dll.second.RegisterExternalCommand(words);
			}
#endif
		}

		void Manager::Detach(const std::wstring& Path) {
			FreeLibrary(dlls[Path].mod);
			dlls.erase(Path);
		}

		HMODULE Manager::Get(const std::wstring& Name) noexcept {
#if 0
			auto dll = dlls.find(Name);
			if (dll != dlls.end())
				dll->second.mod;
			else
				return nullptr;
#else
			return nullptr;
#endif
		}
	}
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	if (!karapo::Program::Instance().engine.Failed()) {
		karapo::Program::Instance().OnInit();
		return karapo::Program::Instance().Main();
	} else
		return 1;
}