/**
* Error.hpp - �Q�[�����̃G���[���������߂̒�`�Q�B
*/
#pragma once
namespace karapo::error {
	// �G���[�̃O���[�v
	struct ErrorClass;
	// �G���[�̓��e
	struct ErrorContent;

	// ���[�U�̃G���[�������N���X�B
	class UserErrorHandler {
		using ErrorElement = std::list<std::tuple<ErrorContent*, std::wstring, void(*)(const int)>>;
		// �Q�[���S�̂Ƃ��ẴG���[�Q�B
		inline static ErrorElement global_errors{};
		// �Q�[���̓���̃G���[�Q�B
		ErrorElement local_errors{};

		static void ShowError(ErrorElement*, const unsigned Error_Level);
	public:
		static void SendGlobalError(ErrorContent*, const std::wstring& = L"", void(*)(const int) = nullptr), ShowGlobalError(const unsigned Error_Level);
		void SendLocalError(ErrorContent*, const std::wstring& = L"", void(*)(const int) = nullptr), ShowLocalError(const unsigned Error_Level);
		
		// �V���ȃG���[�̃O���[�v���쐬����B
		static ErrorClass* MakeErrorClass(const wchar_t* Error_Title);
		// �V���ȃG���[���쐬����B
		static ErrorContent* MakeError(ErrorClass*, const wchar_t* Error_Message, const int MB_Type, const unsigned Level);
	};
}