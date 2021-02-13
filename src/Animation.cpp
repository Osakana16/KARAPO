#include "Animation.hpp"

namespace karapo::animation {
	BaseAnimation::BaseAnimation(){}

	BaseAnimation::BaseAnimation(std::initializer_list<SImage>& img_list) {
		for (auto img : img_list) {
			sprite.push_back(img);
		}
		RemoveInvalidImage();
	}

	size_t BaseAnimation::Size() const noexcept {
		return sprite.size();
	}

	void BaseAnimation::PushBack(SImage img) noexcept {
		sprite.push_back(img);
	}

	void BaseAnimation::PushFront(SImage img) noexcept {
		sprite.push_front(img);
	}

	void BaseAnimation::PushTo(SImage img, signed n) noexcept {
		sprite.insert(sprite.begin() + (n % sprite.size()), img);
	}

	void BaseAnimation::PushTo(SImage img, unsigned n) noexcept {
		sprite.insert(sprite.begin() + (n % sprite.size()), img);
	}

	void BaseAnimation::RemoveInvalidImage() noexcept {
		for (auto l = sprite.begin(); l != sprite.end(); l++) {
			if (*l != nullptr)
				sprite.erase(l);
		}
	}

	SImage& Animation::operator[](signed n) noexcept {
		if (n >= 0)	return sprite[n % Size()];
		else		return sprite[(Size() - 1) + (n % Size())];
	}

	SImage& Animation::operator[](unsigned n) noexcept {
		return sprite[n % Size()];
	}

	Sprite::iterator Animation::Begin() {
		return sprite.begin();
	}

	Sprite::iterator Animation::End() {
		return sprite.end();
	}

	FrameRef::FrameRef(Sprite::iterator b, Sprite::iterator e) {
		InitFrame(b, e);
	}

	void FrameRef::InitFrame(Sprite::iterator b, Sprite::iterator e) {
		begin = b;
		end = e;

		it = begin;
	}

	SImage& FrameRef::operator*() noexcept {
		return (*it);
	}

	SImage& FrameRef::operator<<(const int N) noexcept {
		if (N <= 0)
			return (*it);

		(*this)++;
		operator<<(N - 1);
	}

	SImage& FrameRef::operator>>(const int N) noexcept {
		if (N <= 0)
			return (*it);

		++(*this);
		operator>>(N - 1);
	}

	SImage& FrameRef::operator++(int) noexcept {
		it++;
		Fix();
		return (*it);
	}

	SImage& FrameRef::operator--(int) noexcept {
		it--;
		Fix();
		return (*it);
	}

	SImage& FrameRef::operator++() noexcept {
		it--;
		Fix();
		return (*it);
	}

	SImage& FrameRef::operator--() noexcept {
		it++;
		Fix();
		return (*it);
	}

	void FrameRef::Fix() noexcept {
		if (it > end)
			it = begin;
		else if (it < begin)
			it = end;
	}
}