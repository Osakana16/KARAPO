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
		Array<SmartPtr<Entity>> drawing;
	public:
		ImageLayer() : Layer() {}
		virtual void Execute() override {
			auto p = GetProgram();
			
			p->engine.ClearScreen();
			Draw();
			p->engine.ChangeTargetScreen(p->engine.GetBackScreen());
			p->engine.DrawRect(Rect{ 0, 0, 0, 0 }, Screen);
		}

		void Register(SmartPtr<Entity> d) {
			if (IsRegistered(d))
				return;

			drawing.push_back(d);
		}

		bool IsRegistered(SmartPtr<Entity> d) const noexcept {
			return std::find(drawing.begin(), drawing.end(), d) != drawing.end();
		}

		virtual void Draw() = 0;
	};
}

namespace karapo {
	/**
	* ���Έʒu�^���C���[
	* ���Wbase_origin�𒆐S�Ƃ������Έʒu�ɂ��`�ʂ��s�����C���[�B
	*/
	class RelativeLayer : public ImageLayer {
	private:
		SmartPtr<Entity> base;
	public:
		inline RelativeLayer(SmartPtr<Entity> b) : ImageLayer() {
			base = b;
		}

		/**
		* �`��
		* (0,0) <= base_origin <= (W, H)�𖞂����悤�ɉ�ʕ`�ʂ��s���B
		*/
		void Draw() override {
			auto p = GetProgram();
			const ScreenVector Screen_Size { p->Width, p->Height };
			const auto Draw_Origin = Screen_Size / 2;
			auto base_origin = base->Origin();
			std::queue<SmartPtr<Entity>> dead;
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
	* ��Έʒu�^���C���[
	*/
	class AbsoluteLayer : public ImageLayer {
	public:
		inline AbsoluteLayer() : ImageLayer() {}

		void Draw() override {
			auto p = GetProgram();
	
			std::queue<SmartPtr<Entity>> dead;
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
		karapo::RelativeLayer* MakeRelativeLayer(karapo::SmartPtr<Entity> e) {
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

	void Canvas::Register(SmartPtr<Entity> d, const int Index) {
		layers[Index]->Register(d);
	}

	void Canvas::DeleteLayer(const int Index) noexcept {
		delete layers[Index];
		layers.erase(std::find(layers.begin(), layers.end(), layers[Index]));
	}

	RelativeLayer *Canvas::MakeLayer(SmartPtr<Entity> e) {
		return MakeRelativeLayer(e);
	}

	AbsoluteLayer *Canvas::MakeLayer() {
		return MakeAbsoluteLayer();
	}
}