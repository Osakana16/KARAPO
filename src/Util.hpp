#pragma once


#define MYGAME_STRING(T) karapo::String(LT))

#ifndef NDEBUG
#define MYGAME_ASSERT(X) assert(X)
#else
#define MYGAME_ASSERT(X) __assume(X)
#endif

// 受け取ったポインタをdeleteし、アドレスをnullptrにする。
#define MYGAME_DELETE(PTR) delete PTR; PTR = nullptr

namespace karapo {
	using wchar = wchar_t;
	using char8 = char8_t;
	using char16 = char16_t;
	using char32 = char32_t;
	using wint = wint_t;
	using int8 = std::int8_t;
	using int16 = std::int16_t;
	using int32 = std::int32_t;
	using int64 = std::int64_t;
	using uint = std::uint_fast32_t;
	using uint8 = std::uint8_t;
	using uint16 = std::uint16_t;
	using uint32 = std::uint32_t;
	using uint64 = std::uint64_t;

	using Char = wchar;				// 文字および文字列のポインタ
	using String = std::wstring;	// 文字列

	template<typename T>
	using SmartPtr = std::shared_ptr<T>;

	using Rect = RECT;
	using Point = POINT;

	struct Color {
		int r, g, b;
	};

	// 浮動小数点数
	using Dec = double;
	// 頻繁に使用するSTL系配列
	template<typename T>
	using Array = std::vector<T>;

	// 乱数関数
	template<typename T>
	T Random(const T Min, const T Max) noexcept {
		std::random_device rand;
#if defined(_WIN64)
		std::mt19937_64 mt(rand());
#else
		std::mt19937 mt(rand());
#endif
		if constexpr (std::is_integral<T>::value) {
			std::uniform_int_distribution<> random(Min, Max);
			return random(mt);
		} else if constexpr (std::is_floating_point<T>::value) {
			std::uniform_real_distribution<> random(Min, Max);
			return random(mt);
		}
	}

#define CASE_SPACE case L' ': case L'　': case L'	'): case L' ': case L' ': case L' ': case L' '

	constexpr bool IsSpace(const char C) noexcept {
		return (C == ' ' || C == '　' || C == '	' || C == '	');
	}

	constexpr bool IsSpace(const wchar_t WC) noexcept {
		return (WC == L' ' || WC == L'　' || WC == L'	' || WC == L' ' || WC == L' ' || WC == L'　	' || WC == L' ' || WC == L' ');
	}

	constexpr bool IsSpace(const char16_t C) noexcept {
		return (C == u' ' || C == u'　' || C == u'	' || C == u' ' || C == u' ' || C == u'　' || C == u' ' || C == u' ');
	}
	
	constexpr bool IsSpace(const char32_t C) noexcept {
		return (C == U' ' || C == U'　' || C == U'	' || C == U' ' || C == U' ' || C == U'　' || C == U' ' || C == U' ');
	}

	template<typename T = int>
	T ToInt(const char* C, char**EP, const int Radix = 0) noexcept {
		if constexpr (std::is_same_v<T, int8_t> || std::is_same_v<T, int16_t> || std::is_same_v<T, int32_t>) {
			return strtol(C, EP, Radix);
		} else if constexpr (std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t>) {
			return strtoul(C, EP, Radix);
		} else if constexpr (std::is_same_v<T, int64_t>) {
			return strtoll(C, EP, Radix);
		} else if constexpr (std::is_same_v<T, uint64_t>) {
			return strtoull(C, EP, Radix);
		} else {
			static_assert(0);
		}
	}

	template<typename T = int>
	T ToInt(const wchar_t* C, wchar_t**EP, const int Radix = 0) noexcept {
		if constexpr (std::is_same_v<T, int8_t> || std::is_same_v<T, int16_t> || std::is_same_v<T, int32_t>) {
			return wcstol(C, EP, Radix);
		} else if constexpr (std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t>) {
			return wcstoul(C, EP, Radix);
		} else if constexpr (std::is_same_v<T, int64_t>) {
			return wcstoll(C, EP, Radix);
		} else if constexpr (std::is_same_v<T, uint64_t>) {
			return wcstoull(C, EP, Radix);
		} else {
			static_assert(0);
		}
	}

	template<typename T = int>
	std::pair<T, char*> ToInt(const char* C, const int Radix = 0) noexcept {
		char *ep;
		T i = ToInt<T>(C, &ep, Radix);
		return { i, ep };
	}

	template<typename T = int>
	std::pair<T, wchar_t*> ToInt(const wchar_t* C, const int Radix = 0) noexcept {
		wchar_t *ep;
		T i = ToInt<T>(C, &ep, Radix);
		return { i, ep };
	}

	template<typename T>
	T ToDec(const char* C, char**EP) noexcept {
		if constexpr (std::is_same_v<T, float>) {
			return strtof(C, EP);
		} else if constexpr (std::is_same_v<T, double>) {
			return strtod(C, EP);
		} else if constexpr (std::is_same_v<T, long double>) {
			return strtold(C, EP);
		} else {
			static_assert(0);
		}
	}

	template<typename T>
	T ToDec(const wchar_t* C, wchar_t**EP) noexcept {
		if constexpr (std::is_same_v<T, float>) {
			return wcstof(C, EP);
		} else if constexpr (std::is_same_v<T, double>) {
			return wcstod(C, EP);
		} else if constexpr (std::is_same_v<T, long double>) {
			return wcstold(C, EP);
		} else {
			static_assert(0);
		}
	}

	template<typename T>
	std::pair<T, char*> ToDec(const char* C) noexcept {
		char *ep;
		T d = ToDec<T>(C, &ep);
		return { d, ep };
	}

	template<typename T>
	std::pair<T, wchar_t*> ToDec(const wchar_t* C) noexcept {
		wchar_t *ep;
		T d = ToDec<T>(C, &ep);
		return { d, ep };
	}

	// 二つの文字列が等しいかを調べる
	bool Compare(const char *C1, const char *C2) noexcept,
		Compare(const wchar_t *C1, const wchar_t *C2) noexcept,
		Compare(const char16_t *C1, const char16_t *C2) noexcept,
		Compare(const char32_t *C1, const char32_t *C2) noexcept;

	/// <summary>
	/// 文字・文字列が対象の文字列に含まれているかを調べる
	/// </summary>
	/// <param name="C1">調べる文字列</param>
	/// <param name="C2">対象の文字列</param>
	/// <returns>含まれていればtrue、そうでないならfalse</returns>
	bool In(const char *C1, const char C2) noexcept,
		In(const wchar_t *C1, const wchar_t C2) noexcept,
		In(const char16_t *C1, const char16_t C2) noexcept,
		In(const char32_t *C1, const char32_t C2) noexcept,
		In(const char *C1, const char *C2) noexcept,
		In(const wchar_t *C1, const wchar_t *C2) noexcept,
		In(const char16_t *C1, const char16_t *C2) noexcept,
		In(const char32_t *C1, const char32_t *C2) noexcept;

	size_t StrLen(const char *ch) noexcept,
		StrLen(const wchar_t* ch)noexcept;
}