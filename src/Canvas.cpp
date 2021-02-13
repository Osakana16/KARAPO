#include "Canvas.hpp"
#include "Engine.hpp"
#include <deque>
#include <thread>

namespace karapo {
	Layer::Layer() {
		screen = GetProgram()->engine.MakeScreen();
	}

	class ImageLayer : public Layer {
	protected:
		Array<SmartPtr<entity::Entity>> drawing;
	public:
		ImageLayer() : Layer() {}
		virtual void Execute() override {
			auto p = GetProgram();
			
			p->engine.ClearScreen();
			Draw();
			p->engine.ChangeTargetScreen(p->engine.GetBackScreen());
			p->engine.DrawRect(Rect{ 0, 0, 0, 0 }, Screen);
		}

		void Register(SmartPtr<entity::Entity> d) {
			if (IsRegistered(d))
				return;

			drawing.push_back(d);
		}

		bool IsRegistered(SmartPtr<entity::Entity> d) const noexcept {
			return std::find(drawing.begin(), drawing.end(), d) != drawing.end();
		}

		virtual void Draw() = 0;
	};
}

namespace karapo {
	/**
	* 相対位置型レイヤー
	* 座標base_originを中心とした相対位置による描写を行うレイヤー。
	*/
	class RelativeLayer : public ImageLayer {
	private:
		SmartPtr<entity::Entity> base;
	public:
		inline RelativeLayer(SmartPtr<entity::Entity> b) : ImageLayer() {
			base = b;
		}

		/**
		* 描写
		* (0,0) <= base_origin <= (W, H)を満たすように画面描写を行う。
		*/
		void Draw() override {
			auto p = GetProgram();
			const ScreenVector Screen_Size { p->Width, p->Height };
			const auto Draw_Origin = Screen_Size / 2;
			auto base_origin = base->Origin();
			std::queue<SmartPtr<entity::Entity>> dead;
			for (auto drawer : drawing) {
				if (drawer->CanDelete()) {
					dead.push(drawer);
				} else {
					drawer->Draw(base_origin, Screen);
				}
			}

			for (; !dead.empty(); dead.pop()) {
				drawing.erase(std::find(drawing.begin(), drawing.end(), dead.front()));
			}
		}
	};
}

namespace karapo {
	/**
	* 絶対位置型レイヤー
	*/
	class AbsoluteLayer : public ImageLayer {
	public:
		inline AbsoluteLayer() : ImageLayer() {}

		void Draw() override {
			auto p = GetProgram();
	
			std::queue<SmartPtr<entity::Entity>> dead;
			for (auto drawer : drawing) {
				if (drawer->CanDelete()) {
					dead.push(drawer);
				} else {
					drawer->Draw({ 0, 0 }, Screen);
				}
			}

			for (; !dead.empty(); dead.pop()) {
				drawing.erase(std::find(drawing.begin(), drawing.end(), dead.front()));
			}
		}
	};
}

namespace karapo {
	namespace {
		karapo::RelativeLayer* MakeRelativeLayer(karapo::SmartPtr<entity::Entity> e) {
			return new karapo::RelativeLayer(e);
		}

		karapo::AbsoluteLayer* MakeAbsoluteLayer() {
			return new karapo::AbsoluteLayer();
		}
	}

	void Canvas::Update() noexcept {
		for (auto& layer : layers) {
			layer->Execute();
		}
	}

	void Canvas::AddLayer(ImageLayer* layer) {
		layers.push_back(layer);
	}

	void Canvas::AddLayer(RelativeLayer* layer) {
		AddLayer(static_cast<ImageLayer*>(layer));
	}

	void Canvas::AddLayer(AbsoluteLayer* layer) {
		AddLayer(static_cast<ImageLayer*>(layer));
	}

	void Canvas::Register(SmartPtr<entity::Entity> d, const int Index) {
		layers[Index]->Register(d);
	}

	void Canvas::DeleteLayer(const int Index) noexcept {
		delete layers[Index];
		layers.erase(std::find(layers.begin(), layers.end(), layers[Index]));
	}

	RelativeLayer *Canvas::MakeLayer(SmartPtr<entity::Entity> e) {
		return MakeRelativeLayer(e);
	}

	AbsoluteLayer *Canvas::MakeLayer() {
		return MakeAbsoluteLayer();
	}
}