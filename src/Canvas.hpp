/**
* Canvas.hpp - �Q�[����ł̃L�����o�X�E���C���[�@�\�̒�`�Q�B
*/
#pragma once

namespace karapo {
	// ���C���[
	// �摜�`�ʒS��
	class Layer {
		TargetRender screen;
	protected:
		Layer();
		const TargetRender& Screen = screen;
	public:
		// ���\�[�X��o�^����B
		virtual void Register(SmartPtr<Entity>) = 0;
		virtual void Execute() = 0;
	};

	// - �O���錾 -

	class ImageLayer;		// �摜���C���[
	class RelativeLayer;	// ���Έʒu�^���C���[
	class AbsoluteLayer;	// ��Έʒu�^���C���[

	// �L�����o�X
	class Canvas {
		using Layers = std::vector<ImageLayer*>;
		Layers layers;

		RelativeLayer *MakeLayer(SmartPtr<Entity>);
		AbsoluteLayer *MakeLayer();

		void AddLayer(ImageLayer*), 
			AddLayer(RelativeLayer*),
			AddLayer(AbsoluteLayer *);
	public:
		void Update() noexcept;
		void Register(SmartPtr<Entity>, const int);
		void DeleteLayer(const int) noexcept;

		template<class C>
		void CreateLayer(SmartPtr<Entity> e = nullptr) {
			if constexpr (std::is_same_v<C, RelativeLayer*>) {
				AddLayer(MakeLayer(e));
			} else if constexpr (std::is_same_v<C, AbsoluteLayer*>) {
				AddLayer(MakeLayer());
			} else {
				static_assert(0);
			}
		}
	};

	class AudioManager {
		class MusicManager;
		class SoundManager;
	public:

	};
}