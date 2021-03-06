/**
* Canvas.hpp - �Q�[����ł̃L�����o�X�E���C���[�@�\�̒�`�Q�B
*/
#pragma once

namespace karapo {
	class Filter {
	public:
		virtual ~Filter() = 0;
		virtual void Draw(const TargetRender) noexcept = 0;
	};

	// ���C���[
	// �摜�`�ʒS��
	class Layer {
		TargetRender screen;
	protected:
		Layer();
		const TargetRender& Screen = screen;
	public:
		// ���\�[�X��o�^����B
		virtual void Register(std::shared_ptr<Entity>) = 0;
		virtual void Execute() = 0;
	};

	class ImageLayer : public Layer {
	protected:
		std::vector<std::shared_ptr<Entity>> drawing;
	public:
		ImageLayer() : Layer() {}
		virtual void Execute() override;
		void Register(std::shared_ptr<Entity>);
		bool IsRegistered(std::shared_ptr<Entity>) const noexcept;

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
		void Register(std::shared_ptr<Entity>, const int);
		void DeleteLayer(const int) noexcept;

		// ���C���[���쐬���A�ǉ�����B
		size_t CreateRelativeLayer(std::shared_ptr<Entity>), CreateAbsoluteLayer();
	};

	class AudioManager {
		class MusicManager;
		class SoundManager;
	public:

	};
}