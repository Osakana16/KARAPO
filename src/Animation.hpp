#pragma once
#include "Resource.hpp"

namespace karapo {
	using SImage = std::shared_ptr<resource::Image>;

	/// <summary>
	/// 画像の集合体
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

			// フレームを末端に追加する。
			void PushBack(SImage) noexcept;
			// フレームを先端に追加する。
			void PushFront(SImage) noexcept;
			// フレームを任意の位置に追加する。
			void PushTo(SImage, signed) noexcept;
			// フレームを任意の位置に追加する。
			void PushTo(SImage, unsigned) noexcept;


			// フレーム数を返す。
			size_t Size() const noexcept;
		};

		// インデックスによるアニメーション
		class Animation : public virtual BaseAnimation {
		public:
			using BaseAnimation::BaseAnimation;
			// フレーム参照
			// 
			SImage& operator[](signed) noexcept;

			// フレーム参照(負数無し最適化版)
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

			// 現在のフレームを参照する
			SImage& operator*() noexcept;

			// 現在のフレームをN個分次のフレームにする
			SImage& operator<<(const int N) noexcept;
			// 現在のフレームをN個分前のフレームにする
			SImage& operator>>(const int N) noexcept;

			// 現在のフレームを次のフレームにする
			SImage& operator++(int) noexcept;
			// 現在のフレームを前のフレームにする
			SImage& operator--(int) noexcept;
			// 現在のフレームを前のフレームにする
			SImage& operator++() noexcept;
			// 現在のフレームを次のフレームにする
			SImage& operator--() noexcept;
		};
	}
}