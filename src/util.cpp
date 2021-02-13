#include <algorithm>
#include <utility>

namespace karapo {
	bool Compare(const char *C1, const char *C2) noexcept {
		return (strcmp(C1, C2) == 0);
	}

	bool Compare(const wchar_t *C1, const wchar_t *C2) noexcept {
		return (wcscmp(C1, C2) == 0);
	}

	bool Compare(const char16_t *C1, const char16_t *C2) noexcept {
		return (std::u16string(C1).compare(C2));
	}

	bool Compare(const char32_t *C1, const char32_t *C2) noexcept {
		return (std::u32string(C1).compare(C2));
	}

	bool In(const char *C1, const char C2) noexcept {
		return (strchr(C1, C2) != nullptr);
	}

	bool In(const wchar_t *C1, const wchar_t C2) noexcept {
		return (wcschr(C1, C2) != nullptr);
	}

	bool In(const char16_t *C1, const char16_t C2) noexcept {
		return (std::u16string(C1).find(C2) != std::u16string::npos);
	}

	bool In(const char32_t *C1, const char32_t C2) noexcept {
		return (std::u32string(C1).find(C2) != std::u32string::npos);
	}

	bool In(const char *C1, const char *C2) noexcept {
		return (strstr(C1, C2) != nullptr);
	}

	bool In(const wchar_t *C1, const wchar_t *C2) noexcept {
		return (wcsstr(C1, C2) != nullptr);
	}

	bool In(const char16_t *C1, const char16_t* C2) noexcept {
		return (std::u16string(C1).find(C2) != std::u16string::npos);
	}

	bool In(const char32_t *C1, const char32_t *C2) noexcept {
		return (std::u32string(C1).find(C2) != std::u32string::npos);
	}

	size_t StrLen(const char *Ch) noexcept {
		return strlen(Ch);
	}

	size_t StrLen(const wchar_t *Ch) noexcept {
		return wcslen(Ch);
	}
}