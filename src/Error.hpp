/**
* Error.hpp - ゲーム内のエラーを扱うための定義群。
*/
#pragma once
namespace karapo::error {
	// エラーのグループ
	struct ErrorClass;
	// エラーの内容
	struct ErrorContent;

	// ユーザのエラーを扱うクラス。
	class UserErrorHandler {
		using ErrorElement = std::list<std::tuple<ErrorContent*, std::wstring, void(*)(const int)>>;
		// ゲーム全体としてのエラー群。
		inline static ErrorElement global_errors{};
		// ゲームの特定のエラー群。
		ErrorElement local_errors{};

		static void ShowError(ErrorElement*, const unsigned Error_Level);
	public:
		static void SendGlobalError(ErrorContent*, const std::wstring& = L"", void(*)(const int) = nullptr), ShowGlobalError(const unsigned Error_Level);
		void SendLocalError(ErrorContent*, const std::wstring& = L"", void(*)(const int) = nullptr), ShowLocalError(const unsigned Error_Level);
		
		// 新たなエラーのグループを作成する。
		static ErrorClass* MakeErrorClass(const wchar_t* Error_Title);
		// 新たなエラーを作成する。
		static ErrorContent* MakeError(ErrorClass*, const wchar_t* Error_Message, const int MB_Type, const unsigned Level);
	};
}