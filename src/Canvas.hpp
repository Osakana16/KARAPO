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

	// - 前方宣言 -

	class ImageLayer;		// 画像レイヤー
	class RelativeLayer;	// 相対位置型レイヤー
	class AbsoluteLayer;	// 絶対位置型レイヤー

	// キャンバス
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