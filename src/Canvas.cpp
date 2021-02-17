#include "Canvas.hpp"
#include "Engine.hpp"
#include <deque>
#include <thread>

namespace karapo {
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

	void ImageLayer::Register(SmartPtr<Entity> d) {
		if (IsRegistered(d))
			return;

		drawing.push_back(d);
	}

	bool ImageLayer::IsRegistered(SmartPtr<Entity> d) const noexcept {
		return std::find(drawing.begin(), drawing.end(), d) != drawing.end();
	}

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
			const ScreenVector Screen_Size { p->WindowSize().first, p->WindowSize().second };
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
	void Canvas::Update() noexcept {
		for (auto& layer : layers) {
			layer->Execute();
		}
	}

	void Canvas::Register(SmartPtr<Entity> d, const int Index) {
		layers[Index]->Register(d);
	}

	void Canvas::DeleteLayer(const int Index) noexcept {
		layers.erase(std::find(layers.begin(), layers.end(), layers[Index]));
	}

	void Canvas::CreateRelativeLayer(SmartPtr<Entity> e) {
		layers.push_back(std::make_unique<RelativeLayer>(e));
	}

	void Canvas::CreateAbsoluteLayer() {
		layers.push_back(std::make_unique<AbsoluteLayer>());
	}
}