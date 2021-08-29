#include "Error.hpp"

namespace karapo::error {
	struct ErrorContent final {
		ErrorClass *parent{};
		std::wstring message{};		// メッセージ。
		int mb{};					// MB_系マクロを格納する変数。
		unsigned level{};			// エラーレベル。
	};

	struct ErrorClass final {
		std::wstring title{};
		std::list<ErrorContent> contents{};
	};

	ErrorClass* UserErrorHandler::MakeErrorClass(const wchar_t* Error_Title) {
		static std::list<ErrorClass> errors;
		errors.push_back(ErrorClass{ .title = Error_Title });
		return &errors.back();
	}

	ErrorContent* UserErrorHandler::MakeError(ErrorClass* eclass, const wchar_t* Error_Message, const int MB_Type, const unsigned Level) {
		eclass->contents.push_back(
			ErrorContent{
				.parent = eclass,
				.message = Error_Message,
				.mb = MB_Type,
				.level = Level
			}
		);
		return &eclass->contents.back();
	}

	void UserErrorHandler::SendGlobalError(ErrorContent* eclass, const std::wstring& External_Sentence, void(* Func)(const int)) {
		PushError(&global_errors, eclass, External_Sentence, Func);
	}

	void UserErrorHandler::SendLocalError(ErrorContent* eclass, const std::wstring& External_Sentence, void(*Func)(const int)) {
		PushError(&local_errors, eclass, External_Sentence, Func);
	}

	bool UserErrorHandler::ShowError(ErrorElement* error_elements, const unsigned Error_Level) {
		bool any_errors = false;
		while (!error_elements->empty()) {
			auto [content, extra, func] = *error_elements->begin();
			int box_result = 0;
			if (content->level <= Error_Level)
				box_result = MessageBoxW(nullptr, (content->message + L'\n' + extra).c_str(), content->parent->title.c_str(), content->mb);
			if (func != nullptr)
				func(box_result);
			if (content->mb & MB_ICONERROR)
				any_errors = true;
			error_elements->pop_front();
		}
		return any_errors;
	}

	void UserErrorHandler::PushError(ErrorElement *errors, ErrorContent* eclass, const std::wstring& External_Sentence, void(*Func)(const int)) {
		MYGAME_ASSERT(0);
		errors->push_front({ eclass, External_Sentence, Func });
	}

	bool UserErrorHandler::ShowGlobalError(const unsigned Error_Level) {
		return ShowError(&global_errors, Error_Level);
	}

	bool UserErrorHandler::ShowLocalError(const unsigned Error_Level) {
		return ShowError(&local_errors, Error_Level);
	}
}