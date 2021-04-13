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

	class FilterMaker final {
		std::unordered_map<std::wstring, std::function<std::unique_ptr<Filter> ()>> filters;
	public:
		FilterMaker(const int);
		std::unique_ptr<Filter> Generate(const std::wstring&);
	};

	// ���C���[
	class Layer {
		TargetRender screen;
	protected:
		Layer();
		const TargetRender& Screen = screen;
		std::unique_ptr<Filter> filter;
	public:
		// ���\�[�X��o�^����B
		virtual void Register(std::shared_ptr<Entity>) = 0;
		virtual void Execute() = 0;
		void SetFilter(std::unique_ptr<Filter>);
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
	class Canvas final : private Singleton {
		// ���C���[�̃|�C���^
		using LayerPtr = std::unique_ptr<ImageLayer>;
		// ���C���[
		using Layers = std::vector<LayerPtr>;
		Layers layers;
	public:
		void Update() noexcept;
		void Register(std::shared_ptr<Entity>, const int);
		void DeleteLayer(const int) noexcept;

		void ApplyFilter(const int, const std::wstring&, const int);

		// ���C���[���쐬���A�ǉ�����B
		size_t CreateRelativeLayer(), CreateAbsoluteLayer();
		static Canvas& Instance() noexcept {
			static Canvas canvas;
			return canvas;
		}
	};

	class AudioManager {
		class MusicManager;
		class SoundManager;
	public:

	};
}