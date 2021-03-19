#include <locale>
#include <fstream>
#include <cstdint>
#include <cwchar>
#include <cuchar>

namespace {
	union CharBytes {
		char32_t c32;
		char16_t c16[2];
		wchar_t w;
		char8_t c8[4];
		char c[4];
	};

	class TextFileReader final {
		CharCode char_code = CharCode::Unknown;

		static size_t GetFileSize(std::ifstream& ifs) noexcept {
			ifs.seekg(0, std::ios_base::end);
			const size_t File_Size = ifs.tellg();
			ifs.seekg(0, std::ios_base::beg);
			return File_Size;
		}

		static size_t GetFileSize(std::wifstream& ifs) noexcept {
			ifs.seekg(0, std::ios_base::end);
			const size_t File_Size = ifs.tellg();
			ifs.seekg(0, std::ios_base::beg);
			return File_Size;
		}

		template<typename T, size_t byte_length>
		union Bom {
			T value;
			uint8_t byte[byte_length];
		};

		static CharCode GetCodeType(CharBytes chbyte) noexcept {
			const Bom<uint32_t, 3> UTF8_Bom = { .byte = { 0xEF, 0xBB, 0xBF } };
			const Bom<uint16_t, 2> UTF16le_Bom = { .byte = { 0xFF, 0xFE } };
			const Bom<uint16_t, 2> UTF16be_Bom = { .byte = { 0xFE, 0xFF } };
			const Bom<uint32_t, 4> UTF32le_Bom = { .byte = { 0xFE, 0xFF, 0x00, 0x00 } };
			const Bom<uint32_t, 4> UTF32be_Bom = { .byte = { 0x00, 0x00, 0xFF, 0xFE } };

			Bom<uint32_t, 3> u8_checker;
			Bom<uint16_t, 2> u16_checker;
			Bom<uint32_t, 4> u32_checker;

			for (int i = 0; i < 3; i++) {
				u8_checker.byte[i] = chbyte.c8[i];
			}

			for (int i = 0; i < 2; i++) {
				u16_checker.byte[i] = chbyte.c8[i];
			}

			for (int i = 0; i < 4; i++) {
				u32_checker.byte[i] = chbyte.c8[i];
			}

			if (u8_checker.value == UTF8_Bom.value) {
				return CharCode::UTF8;
			} else if (u32_checker.value == UTF32le_Bom.value) {
				return CharCode::UTF32LE;
			} else if (u32_checker.value == UTF32be_Bom.value) {
				return CharCode::UTF32BE;
			} else if (u16_checker.value == UTF16le_Bom.value) {
				return CharCode::UTF16LE;
			} else if (u16_checker.value == UTF16be_Bom.value) {
				return CharCode::UTF16BE;
			} else {
				return CharCode::CP932;
			}
		}

		void CheckBOM() {
			CharBytes chbyte;
			chbyte.c32 = reinterpret_cast<char32_t*>(plain_text)[0];
			char_code = GetCodeType(chbyte);
			switch (char_code) {
				case CharCode::CP932:
					pos = &plain_text[0];
					break;
				case CharCode::UTF8:
					pos = &plain_text[3];
					break;
				case CharCode::UTF16LE:
				case CharCode::UTF16BE:
					pos = &plain_text[2];
					break;
				case CharCode::UTF32LE:
				case CharCode::UTF32BE:
					pos = &plain_text[4];
					break;
			}
		}

		char *pos = nullptr;			// 
		char *plain_text = nullptr;		// 適切な型に変換していない状態の生の文字列。
		size_t plain_length = 0;		// 

		bool is_nobom_utf8 = false;	// BOM無しファイルをUTF-8として読み込むか否か。
	public:
		TextFileReader(const char* File_Name) noexcept {
			std::ifstream ifs(File_Name, std::ios_base::in | std::ios_base::binary);
			if (!ifs.fail()) {
				plain_length = GetFileSize(ifs) + 1;
				plain_text = new char[plain_length] { '\0' };
				for (int i = 0;; i++) {
					char ch = '\0';
					if (ifs.eof()) {
						plain_text[i] = ch;
						break;
					} else {
						ifs.read(&ch, sizeof(char));
						plain_text[i] = ch;
					}
				}
				CheckBOM();
			}
		}

		TextFileReader(const wchar_t* File_Name) {
			std::wifstream ifs(File_Name, std::ios_base::in | std::ios_base::binary);
			if (!ifs.fail()) {
				plain_length = GetFileSize(ifs) + 1;
				plain_text = new char[plain_length] { '\0' };
				for (int i = 0;; i++) {
					wchar_t ch = '\0';
					if (ifs.eof()) {
						plain_text[i] = ch;
						break;
					} else {
						ifs.read(&ch, sizeof(char));
						plain_text[i] = ch;
					}
				}
				CheckBOM();
			}
		}

		CharCode GetCharCode() const noexcept {
			return char_code;
		}

		char* GetCopied() {
			size_t convert_length = plain_length + 1;
			char *target_text = new char[convert_length] { 0 };
			memmove(target_text, plain_text, plain_length);
			return std::move(target_text);
		}

		~TextFileReader() {
			delete[] plain_text;
		}
	};
}

void ReadTextFile(const char* File_Name, char** allocatable_text, CharCode* const copied_code) {
	TextFileReader txt_reader(File_Name);
	*allocatable_text = txt_reader.GetCopied();
	*copied_code = txt_reader.GetCharCode();
}

void ReadWTextFile(const wchar_t* File_Name, char** allocatable_text, CharCode* const copied_code) {
	TextFileReader txt_reader(File_Name);
	*allocatable_text = txt_reader.GetCopied();
	*copied_code = txt_reader.GetCharCode();
}

namespace {
	size_t ToWide(wchar_t* replaced_text, const char* Source_Text, const size_t Source_Length, const char* Locale) {
		setlocale(LC_CTYPE, Locale);
		return mbstowcs(replaced_text, Source_Text, Source_Length);
	}
}

size_t CP932ToWide(wchar_t* replaced_text, const char* Source_Text, const size_t Source_Length) {
	return ToWide(replaced_text, Source_Text, Source_Length, "ja_JP");
}

size_t UTF8ToWide(wchar_t* replaced_text, const char8_t* Source_Text, const size_t Source_Length) {
	return ToWide(replaced_text, reinterpret_cast<const char*>(Source_Text), Source_Length, "ja_JP.UTF-8");
}

size_t UTF16ToWide(wchar_t* replaced_text, const char16_t* Source_Text, const size_t Source_Length) {
	setlocale(LC_CTYPE, "ja_JP.UTF-8");
	char* convert_text = new char[Source_Length * 4]{ '\0' };
	for (int i = 1;; i++) {
		if (Source_Text[i] == u'\0') {
			strcat_s(convert_text, Source_Length * 4, "\0");
			break;
		}
		char ch[4]{ '\0' };
		c16rtomb(ch, Source_Text[i], nullptr);
		strcat_s(convert_text, Source_Length * 4, ch);
	}
	auto len = ToWide(replaced_text, convert_text, Source_Length, "ja_JP.UTF-8");
	delete[] convert_text;
	return len;
}

size_t UTF32ToWide(wchar_t* replaced_text, const char32_t* Source_Text, const size_t Source_Length) {
	setlocale(LC_CTYPE, "ja_JP.UTF-8");
	char* convert_text = new char[Source_Length * 4]{ '\0' };
	for (int i = 0;; i++) {
		if (Source_Text[i] == u'\0') {
			strcat_s(convert_text, Source_Length * 4, "\0");
			break;
		}
		char ch[4]{ '\0' };
		c32rtomb(ch, Source_Text[i], nullptr);
		strcat_s(convert_text, Source_Length * 4, ch);
	}
	auto len = ToWide(replaced_text, convert_text, Source_Length, "ja_JP.UTF-8");
	delete[] convert_text;
	return len;
}

size_t WideToCP932(char* replaced_text, const wchar_t* Source_Text, const size_t Source_Length) {
	setlocale(LC_CTYPE, "ja_JP");
	return wcstombs(replaced_text, Source_Text, Source_Length);
}