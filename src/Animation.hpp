#pragma once

namespace karapo {
	// スプライト
	using Sprite = std::deque<resource::Image>;

	namespace animation {
		class BaseAnimation {
		protected:
			Sprite sprite;
			void RemoveInvalidImage()noexcept;
		public:
			BaseAnimation();
			BaseAnimation(std::initializer_list<resource::Image>&);

			// フレームを末端に追加する。
			void PushBack(resource::Image) noexcept;
			// フレームを先端に追加する。
			void PushFront(resource::Image) noexcept;
			// フレームを任意の位置に追加する。
			void PushTo(resource::Image, signed) noexcept;
			// フレームを任意の位置に追加する。
			void PushTo(resource::Image, unsigned) noexcept;


			// フレーム数を返す。
			size_t Size() const noexcept;
		};

		// インデックスによるアニメーション
		class Animation : public virtual BaseAnimation {
		public:
			using BaseAnimation::BaseAnimation;
			// フレーム参照
			// 
			resource::Image& operator[](signed) noexcept;

			// フレーム参照(負数無し最適化版)
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

			// 現在のフレームを参照する
			resource::Image& operator*() noexcept;

			// 現在のフレームをN個分次のフレームにする
			resource::Image& operator<<(const int N) noexcept;
			// 現在のフレームをN個分前のフレームにする
			resource::Image& operator>>(const int N) noexcept;

			// 現在のフレームを次のフレームにする
			resource::Image& operator++(int) noexcept;
			// 現在のフレームを前のフレームにする
			resource::Image& operator--(int) noexcept;
			// 現在のフレームを前のフレームにする
			resource::Image& operator++() noexcept;
			// 現在のフレームを次のフレームにする
			resource::Image& operator--() noexcept;
		};
	}
}