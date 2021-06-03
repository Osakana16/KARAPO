#pragma once

namespace karapo {
	// �X�v���C�g
	using Sprite = std::deque<resource::Image>;

	namespace animation {
		class BaseAnimation {
		protected:
			Sprite sprite;
			void RemoveInvalidImage()noexcept;
		public:
			BaseAnimation();
			BaseAnimation(std::initializer_list<resource::Image>&);

			// �t���[���𖖒[�ɒǉ�����B
			void PushBack(resource::Image) noexcept;
			// �t���[�����[�ɒǉ�����B
			void PushFront(resource::Image) noexcept;
			// �t���[����C�ӂ̈ʒu�ɒǉ�����B
			void PushTo(resource::Image, signed) noexcept;
			// �t���[����C�ӂ̈ʒu�ɒǉ�����B
			void PushTo(resource::Image, unsigned) noexcept;


			// �t���[������Ԃ��B
			size_t Size() const noexcept;
		};

		// �C���f�b�N�X�ɂ��A�j���[�V����
		class Animation : public virtual BaseAnimation {
		public:
			using BaseAnimation::BaseAnimation;
			// �t���[���Q��
			// 
			resource::Image& operator[](signed) noexcept;

			// �t���[���Q��(���������œK����)
			resource::Image& operator[](unsigned) noexcept;

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
			resource::Image& operator*() noexcept;

			// ���݂̃t���[����N�����̃t���[���ɂ���
			resource::Image& operator<<(const int N) noexcept;
			// ���݂̃t���[����N���O�̃t���[���ɂ���
			resource::Image& operator>>(const int N) noexcept;

			// ���݂̃t���[�������̃t���[���ɂ���
			resource::Image& operator++(int) noexcept;
			// ���݂̃t���[����O�̃t���[���ɂ���
			resource::Image& operator--(int) noexcept;
			// ���݂̃t���[����O�̃t���[���ɂ���
			resource::Image& operator++() noexcept;
			// ���݂̃t���[�������̃t���[���ɂ���
			resource::Image& operator--() noexcept;
		};
	}
}