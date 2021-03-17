/**
* Canvas.hpp - ゲーム上でのキャンバス・レイヤー機能の定義群。
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

	// レイヤー
	class Layer {
		TargetRender screen;
	protected:
		Layer();
		const TargetRender& Screen = screen;
		std::unique_ptr<Filter> filter;
	public:
		// リソースを登録する。
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

	// キャンバス
	class Canvas final : private Singleton {
		// レイヤーのポインタ
		using LayerPtr = std::unique_ptr<ImageLayer>;
		// レイヤー
		using Layers = std::vector<LayerPtr>;
		Layers layers;
	public:
		void Update() noexcept;
		void Register(std::shared_ptr<Entity>, const int);
		void DeleteLayer(const int) noexcept;

		void ApplyFilter(const int, const std::wstring&, const int);

		// レイヤーを作成し、追加する。
		size_t CreateRelativeLayer(std::shared_ptr<Entity>), CreateAbsoluteLayer();
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