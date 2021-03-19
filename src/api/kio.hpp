#pragma once
enum class CharCode {
	Unknown,	// 対応外の文字コード
	CP932,
	UTF8,		// BOMつきUTF-8
	UTF16LE,
	UTF16BE,
	UTF32LE,
	UTF32BE
};

void ReadTextFile(const char* File_Name, char** allocatable_text, CharCode* const copied_code);
void ReadWTextFile(const wchar_t* File_Name, char** allocatable_text, CharCode* const copied_code);
size_t CP932ToWide(wchar_t* replaced_text, const char* Source_Text, const size_t Source_Length);
size_t UTF8ToWide(wchar_t* replaced_text, const char8_t* Source_Text, const size_t Source_Length);
size_t UTF16ToWide(wchar_t* replaced_text, const char16_t* Source_Text, const size_t Source_Length);
size_t UTF32ToWide(wchar_t* replaced_text, const char32_t* Source_Text, const size_t Source_Length);
size_t WideToCP932(char* replaced_text, const wchar_t* Source_Text, const size_t Source_Length);