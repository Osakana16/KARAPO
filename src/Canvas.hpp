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

	// レイヤー
	// 画像描写担当
	class Layer {
		TargetRender screen;
	protected:
		Layer();
		const TargetRender& Screen = screen;
	public:
		// リソースを登録する。
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

	// キャンバス
	class Canvas {
		// レイヤーのポインタ
		using LayerPtr = std::unique_ptr<ImageLayer>;
		// レイヤー
		using Layers = std::vector<LayerPtr>;
		Layers layers;
	public:
		void Update() noexcept;
		void Register(std::shared_ptr<Entity>, const int);
		void DeleteLayer(const int) noexcept;

		// レイヤーを作成し、追加する。
		size_t CreateRelativeLayer(std::shared_ptr<Entity>), CreateAbsoluteLayer();
	};

	class AudioManager {
		class MusicManager;
		class SoundManager;
	public:

	};
}