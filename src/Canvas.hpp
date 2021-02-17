/**
* Canvas.hpp - ゲーム上でのキャンバス・レイヤー機能の定義群。
*/
#pragma once

namespace karapo {
	// レイヤー
	// 画像描写担当
	class Layer {
		TargetRender screen;
	protected:
		Layer();
		const TargetRender& Screen = screen;
	public:
		// リソースを登録する。
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

	// キャンバス
	class Canvas {
		// レイヤーのポインタ
		using LayerPtr = std::unique_ptr<ImageLayer>;
		// レイヤー
		using Layers = std::vector<LayerPtr>;
		Layers layers;
	public:
		void Update() noexcept;
		void Register(SmartPtr<Entity>, const int);
		void DeleteLayer(const int) noexcept;

		// レイヤーを作成し、追加する。
		void CreateRelativeLayer(SmartPtr<Entity>), CreateAbsoluteLayer();
	};

	class AudioManager {
		class MusicManager;
		class SoundManager;
	public:

	};
}