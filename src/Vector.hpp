#pragma once
#define DIMENSION 2

#if DIMENSION < 2
#error "次元マクロDIMENSIONの値が低すぎます！\n2か3の値に設定してください。"
#elif DIMENSION > 3
#error "次元マクロDIMENSIONの値が高すぎます！\n2か3の値に設定してください。"
#endif

namespace karapo {
	// ベクトル
	template<typename T>
	using Vector = std::valarray<T>;
	using WorldVector = Vector<Dec>;	// 実数型ベクトル
	using ScreenVector = Vector<int>;	// 整数型ベクトル
	using BinaryVector = Vector<bool>;

	template<typename T>
	constexpr T Length(const Vector<T>& v) {
#if DIMENSION == 2
		return std::sqrt(std::pow(v[0], 2) + std::pow(v[1], 2));
#elif DIMENSION == 3
		return std::sqrt(std::pow(v[0], 2) + std::pow(v[1], 2) + std::pow(v[2], 2));
#endif
	}

	template<typename T>
	constexpr T Distance(const Vector<T>& v1, const Vector<T>& v2) noexcept {
		return Length(v1 - v2);
	}
}