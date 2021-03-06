#include "Canvas.hpp"
#include "Engine.hpp"
#include <deque>
#include <thread>

namespace karapo {
	Filter::~Filter() {}
	namespace filter {
		// フィルター無し
		class None final : public Filter {
		public:
			~None() final {}

			void Draw(const TargetRender Screen) noexcept final {
				auto [w, h] = GetProgram()->WindowSize();
				GetProgram()->engine.DrawRect(Rect{ 0, 0, w, h }, Screen);
			}
		};

		// 色反転フィルター
		class ReversedColor final : public Filter {
			const int Potency;
		public:
			ReversedColor(const int P) noexcept : Potency(P % 256) {}
			~ReversedColor() final {}
		};

		// X軸反転フィルター
		class XReversed final : public Filter {
		public:
			XReversed() noexcept {}
			~XReversed() final {}
		};

		// Y軸反転フィルター
		class YReversed final : public Filter {
		public:
			YReversed() noexcept {}
			~YReversed() final {}
		};

		// モノクロフィルター
		class Monochrome : public Filter {
			const Color Base_Color;
		public:
			Monochrome(const Color C) noexcept : Base_Color(C) {}
			~Monochrome() override {}
		};
	}

	Layer::Layer() {
		screen = GetProgram()->engine.MakeScreen();
	}

	void ImageLayer::Execute() {
		auto p = GetProgram();

		p->engine.ClearScreen();
		Draw();
		p->engine.ChangeTargetScreen(p->engine.GetBackScreen());
		p->engine.DrawRect(Rect{ 0, 0, 0, 0 }, Screen);
	}

	void ImageLayer::Register(std::shared_ptr<Entity> d) {
		if (IsRegistered(d))
			return;

		drawing.push_back(d);
	}

	bool ImageLayer::IsRegistered(std::shared_ptr<Entity> d) const noexcept {
		return std::find(drawing.begin(), drawing.end(), d) != drawing.end();
	}

	/**
	* 相対位置型レイヤー
	* 座標base_originを中心とした相対位置による描写を行うレイヤー。
	*/
	class RelativeLayer : public ImageLayer {
	private:
		std::shared_ptr<Entity> base;
	public:
		inline RelativeLayer(std::shared_ptr<Entity> b) : ImageLayer() {
			base = b;
		}

		/**
		* 描写
		* (0,0) <= base_origin <= (W, H)を満たすように画面描写を行う。
		*/
		void Draw() override {
			auto p = GetProgram();
			const ScreenVector Screen_Size { p->WindowSize().first, p->WindowSize().second };
			const auto Draw_Origin = Screen_Size / 2;
			auto base_origin = base->Origin();
			std::vector<std::shared_ptr<Entity>> deadmen;
			p->engine.DrawRect(Rect{ 0, 0, Screen_Size[0], Screen_Size[1] }, Screen);
			p->engine.ChangeTargetScreen(Screen);
			p->engine.ClearScreen();
			for (auto drawer : drawing) {
				if (drawer->CanDelete()) {
					deadmen.push_back(drawer);
				} else {
					drawer->Draw(base_origin);
				}
			}
			p->engine.ChangeTargetScreen(p->engine.GetBackScreen());
			for (auto& dead : deadmen) {
				drawing.erase(std::find(drawing.begin(), drawing.end(), dead));
			}
		}
	};

	/**
	* 絶対位置型レイヤー
	*/
	class AbsoluteLayer : public ImageLayer {
	public:
		inline AbsoluteLayer() : ImageLayer() {}

		void Draw() override {
			auto p = GetProgram();
			const ScreenVector Screen_Size{ p->WindowSize().first, p->WindowSize().second };
			std::vector<std::shared_ptr<Entity>> deadmen;
			p->engine.DrawRect(Rect{ 0, 0, Screen_Size[0], Screen_Size[1] }, Screen);
			p->engine.ChangeTargetScreen(Screen);
			p->engine.ClearScreen();
			for (auto drawer : drawing) {
				if (drawer->CanDelete()) {
					deadmen.push_back(drawer);
				} else {
					drawer->Draw(WorldVector{ 0.0, 0.0 });
				}
			}
			p->engine.ChangeTargetScreen(p->engine.GetBackScreen());
			for (auto& dead : deadmen) {
				drawing.erase(std::find(drawing.begin(), drawing.end(), dead));
			}
		}
	};
}

namespace karapo {
	void Canvas::Update() noexcept {
		for (auto& layer : layers) {
			layer->Execute();
		}
	}

	void Canvas::Register(std::shared_ptr<Entity> d, const int Index) {
		layers[Index]->Register(d);
	}

	void Canvas::DeleteLayer(const int Index) noexcept {
		layers.erase(std::find(layers.begin(), layers.end(), layers[Index]));
	}

	size_t Canvas::CreateRelativeLayer(std::shared_ptr<Entity> e) {
		layers.push_back(std::make_unique<RelativeLayer>(e));
		return layers.size();
	}

	size_t Canvas::CreateAbsoluteLayer() {
		layers.push_back(std::make_unique<AbsoluteLayer>());
		return layers.size();
	}
}