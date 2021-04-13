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
				auto [w, h] = Program::Instance().WindowSize();
				Program::Instance().engine.DrawRect(Rect{ 0, 0, w, h }, Screen);
			}
		};

		// 色反転フィルター
		class ReversedColor final : public Filter {
			const int Potency;
		public:
			ReversedColor(const int P) noexcept : Potency(P % 256) {}
			~ReversedColor() final {}

			void Draw(const TargetRender Screen) noexcept final {
				auto [w, h] = Program::Instance().WindowSize();
				Program::Instance().engine.DrawRect(Rect{ 0, 0, w, h }, Screen);
				Program::Instance().engine.SetBlend(BlendMode::Reverse, Potency);
				Program::Instance().engine.DrawRect(Rect{ 0, 0, w, h }, Screen);
				Program::Instance().engine.SetBlend(BlendMode::None, Potency);
			}
		};

		// X軸反転フィルター
		class XReversed final : public Filter {
		public:
			XReversed() noexcept {}
			~XReversed() final {}

			void Draw(const TargetRender Screen) noexcept final {
				const auto [W, H] = Program::Instance().WindowSize();
				Program::Instance().engine.DrawRect(Rect{ 0, H, W, 0 }, Screen);
			}
		};

		// Y軸反転フィルター
		class YReversed final : public Filter {
		public:
			YReversed() noexcept {}
			~YReversed() final {}
			void Draw(const TargetRender Screen) noexcept final {
				const auto [W, H] = Program::Instance().WindowSize();
				Program::Instance().engine.DrawRect(Rect{ W, 0, 0, H }, Screen);
			}
		};

		// モノクロフィルター
		class Monochrome : public Filter {
			const Color Base_Color;
		public:
			Monochrome(const Color C) noexcept : Base_Color(C) {}
			~Monochrome() override {}
			void Draw(const TargetRender Screen) noexcept final {}
		};
	}

	FilterMaker::FilterMaker(const int P) {
		filters[L"none"] = []() { return std::make_unique<filter::None>(); };

		filters[L"reversed_color"] = 
			filters[L"色反転"] = [P]() { return std::make_unique<filter::ReversedColor>(P); };

		filters[L"mirrorx"] = 
			filters[L"縦反転"] = []() { return std::make_unique<filter::XReversed>(); };
		filters[L"mirrory"] = 
			filters[L"横反転"] = []() { return std::make_unique<filter::YReversed>(); };
		filters[L"monochrome"] = filters[L"モノクロ"] = []() { return std::make_unique<filter::Monochrome>(Color{ 0, 0, 0 }); };
	}

	std::unique_ptr<Filter> FilterMaker::Generate(const std::wstring& Filter_Name) {
		return filters.at(Filter_Name)();
	}

	Layer::Layer() {
		screen = Program::Instance().engine.MakeScreen();
	}

	void Layer::SetFilter(std::unique_ptr<Filter> new_filter) {
		filter = std::move(new_filter);
	}

	void ImageLayer::Execute() {
		auto& p = Program::Instance();
		{
			std::vector<std::shared_ptr<Entity>> dels{};
			for (auto& ent : drawing) {
				if (ent->CanDelete()) {
					dels.push_back(ent);
				}
			}

			for (auto ent : dels) {
				drawing.erase(std::find(drawing.begin(), drawing.end(), ent));
			}
		}
		p.engine.ClearScreen();
		Draw();
		p.engine.ChangeTargetScreen(p.engine.GetBackScreen());
		p.engine.DrawRect(Rect{ 0, 0, 0, 0 }, Screen);
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
		std::shared_ptr<Entity> base = nullptr;
	public:
		inline RelativeLayer() : ImageLayer() {
			SetFilter(std::make_unique<filter::None>());
		}

		void Execute() override {
			ImageLayer::Execute();
			if (base != nullptr && base->CanDelete())
				base = nullptr;
		}

		void SetBase(std::shared_ptr<Entity>& ent) noexcept {
			base = ent;
		}

		/**
		* 描写
		* (0,0) <= base_origin <= (W, H)を満たすように画面描写を行う。
		*/
		void Draw() override {
			static WorldVector old_origin = { 0.0, 0.0 };
			auto& p = Program::Instance();
			const ScreenVector Screen_Size { p.WindowSize().first, p.WindowSize().second };
			const auto Draw_Origin = Screen_Size / 2;

			if (base != nullptr) old_origin = base->Origin();
			auto base_origin = old_origin;
			filter->Draw(Screen);
			p.engine.ChangeTargetScreen(Screen);
			p.engine.ClearScreen();
			for (auto drawer : drawing) {
				drawer->Draw(base_origin);
			}
			p.engine.ChangeTargetScreen(p.engine.GetBackScreen());
		}
	};

	/**
	* 絶対位置型レイヤー
	*/
	class AbsoluteLayer : public ImageLayer {
	public:
		inline AbsoluteLayer() : ImageLayer() {
			SetFilter(std::make_unique<filter::None>());
		}

		void Draw() override {
			auto& p = Program::Instance();
			std::vector<std::shared_ptr<Entity>> deadmen;
			filter->Draw(Screen);
			p.engine.ChangeTargetScreen(Screen);
			p.engine.ClearScreen();
			for (auto drawer : drawing) {
				drawer->Draw(WorldVector{ 0.0, 0.0 });
			}
			p.engine.ChangeTargetScreen(p.engine.GetBackScreen());
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

	void Canvas::ApplyFilter(const int Index, const std::wstring& Filter_Name, const int Potency) {
		FilterMaker filter_maker(Potency);
		layers[Index]->SetFilter(filter_maker.Generate(Filter_Name));
	}

	size_t Canvas::CreateRelativeLayer() {
		layers.push_back(std::make_unique<RelativeLayer>());
		return layers.size();
	}

	size_t Canvas::CreateAbsoluteLayer() {
		layers.push_back(std::make_unique<AbsoluteLayer>());
		return layers.size();
	}
}