#pragma once
#include "Resource.hpp"

namespace karapo {
	using SImage = std::shared_ptr<resource::Image>;

	/// <summary>
	/// �摜�̏W����
	/// </summary>
	using Sprite = std::deque<SImage>;

	namespace animation {
		class BaseAnimation {
		protected:
			Sprite sprite;
			void RemoveInvalidImage()noexcept;
		public:
			BaseAnimation();
			BaseAnimation(std::initializer_list<SImage>&);

			// �t���[���𖖒[�ɒǉ�����B
			void PushBack(SImage) noexcept;
			// �t���[�����[�ɒǉ�����B
			void PushFront(SImage) noexcept;
			// �t���[����C�ӂ̈ʒu�ɒǉ�����B
			void PushTo(SImage, signed) noexcept;
			// �t���[����C�ӂ̈ʒu�ɒǉ�����B
			void PushTo(SImage, unsigned) noexcept;


			// �t���[������Ԃ��B
			size_t Size() const noexcept;
		};

		// �C���f�b�N�X�ɂ��A�j���[�V����
		class Animation : public virtual BaseAnimation {
		public:
			using BaseAnimation::BaseAnimation;
			// �t���[���Q��
			// 
			SImage& operator[](signed) noexcept;

			// �t���[���Q��(���������œK����)
			SImage& operator[](unsigned) noexcept;

			Sprite::iterator Begin(), End();
		};

		class FrameRef {
			Sprite::iterator it, begin, end;
			void Fix() noexcept;
		public:
			FrameRef() = default;
			FrameRef(Sprite::iterator b, Sprite::iterator e);

			void InitFrame(Sprite::iterator b, Sprite::iterator e);

			// ���݂̃t���[�����Q�Ƃ���
			SImage& operator*() noexcept;

			// ���݂̃t���[����N�����̃t���[���ɂ���
			SImage& operator<<(const int N) noexcept;
			// ���݂̃t���[����N���O�̃t���[���ɂ���
			SImage& operator>>(const int N) noexcept;

			// ���݂̃t���[�������̃t���[���ɂ���
			SImage& operator++(int) noexcept;
			// ���݂̃t���[����O�̃t���[���ɂ���
			SImage& operator--(int) noexcept;
			// ���݂̃t���[����O�̃t���[���ɂ���
			SImage& operator++() noexcept;
			// ���݂̃t���[�������̃t���[���ɂ���
			SImage& operator--() noexcept;
		};
	}
}