#pragma once
#define DIMENSION 2

#if DIMENSION < 2
#error "�����}�N��DIMENSION�̒l���Ⴗ���܂��I\n2��3�̒l�ɐݒ肵�Ă��������B"
#elif DIMENSION > 3
#error "�����}�N��DIMENSION�̒l���������܂��I\n2��3�̒l�ɐݒ肵�Ă��������B"
#endif

namespace karapo {
	// �x�N�g��
	template<typename T>
	using Vector = std::valarray<T>;
	using WorldVector = Vector<Dec>;	// �����^�x�N�g��
	using ScreenVector = Vector<int>;	// �����^�x�N�g��
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