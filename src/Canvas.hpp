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

	class ImageLayer : public Layer {
	protected:
		Array<SmartPtr<Entity>> drawing;
	public:
		ImageLayer() : Layer() {}
		virtual void Execute() override;
		void Register(SmartPtr<Entity>);
		bool IsRegistered(SmartPtr<Entity>) const noexcept;

		virtual void Draw() = 0;
	};

	// �L�����o�X
	class Canvas {
		// ���C���[�̃|�C���^
		using LayerPtr = std::unique_ptr<ImageLayer>;
		// ���C���[
		using Layers = std::vector<LayerPtr>;
		Layers layers;
	public:
		void Update() noexcept;
		void Register(SmartPtr<Entity>, const int);
		void DeleteLayer(const int) noexcept;

		// ���C���[���쐬���A�ǉ�����B
		void CreateRelativeLayer(SmartPtr<Entity>), CreateAbsoluteLayer();
	};

	class AudioManager {
		class MusicManager;
		class SoundManager;
	public:

	};
}