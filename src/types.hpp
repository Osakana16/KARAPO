#pragma once

// �����^�����Ƃ����V�����^���`���邽�߂̃}�N��
#define KARAPO_NEWTYPE(newone,base_interger_type) enum class newone : base_interger_type{}

namespace karapo {
	namespace raw {
		using TargetRender = int;
		using Resource = int;
	}
	KARAPO_NEWTYPE(TargetRender, raw::TargetRender);
	enum class PlayType { Normal = 1, Loop = 1 | 2 };
}