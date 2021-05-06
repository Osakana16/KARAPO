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

	ImageLayer::ImageLayer(const std::wstring& lname) : Layer() { 
		name = lname;
		Program::Instance().var_manager.MakeNew(Name() + L".__管理中") = std::wstring(L"");
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
				auto& var = Program::Instance().var_manager.Get<false>(Name() + L".__管理中");
				auto str = std::any_cast<std::wstring>(var);
				auto it = str.find(ent->Name());
				str.erase(it, it + wcslen(ent->Name()) + 1);
				var = str;
			}
		}
		Draw();
	}

	void ImageLayer::Register(std::shared_ptr<Entity> d) {
		if (IsRegistered(d))
			return;

		auto& var = Program::Instance().var_manager.Get<false>(Name() + L".__管理中");
		auto str = std::any_cast<std::wstring>(var);
		str += std::wstring(d->Name()) + L'\n';
		var = str;
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
		inline RelativeLayer(const std::wstring& lname) : ImageLayer(lname) {
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
			
			p.engine.ChangeTargetScreen(Screen);
			p.engine.ClearScreen();
			for (auto drawer : drawing) {
				drawer->Draw(base_origin);
			}
			p.engine.ChangeTargetScreen(p.engine.GetBackScreen());
			filter->Draw(Screen);
		}

		const wchar_t* KindName() const noexcept {
			return L"スクロール";
		}
	};

	/**
	* 絶対位置型レイヤー
	*/
	class AbsoluteLayer : public ImageLayer {
	public:
		inline AbsoluteLayer(const std::wstring& lname) : ImageLayer(lname) {
			SetFilter(std::make_unique<filter::None>());
		}

		void Draw() override {
			auto& p = Program::Instance();
			std::vector<std::shared_ptr<Entity>> deadmen;
			p.engine.ChangeTargetScreen(Screen);
			p.engine.ClearScreen();
			for (auto drawer : drawing) {
				drawer->Draw(WorldVector{ 0.0, 0.0 });
			}
			p.engine.ChangeTargetScreen(p.engine.GetBackScreen());
			filter->Draw(Screen);
		}

		const wchar_t* KindName() const noexcept {
			return L"固定";
		}
	};
}

namespace karapo {
	void Canvas::Update() noexcept {
		for (auto& layer : layers) {
			if (layer->IsShowing()) layer->Execute();
		}
	}

	void Canvas::Register(std::shared_ptr<Entity>& d) {
		if (selecting_layer == nullptr)
			return;

		(*selecting_layer)->Register(d);
	}

	void Canvas::Register(std::shared_ptr<Entity>& d, const int Index) {
		if (Index < 0 || Index >= layers.size())
			return;

		layers[Index]->Register(d);
	}

	void Canvas::Register(std::shared_ptr<Entity>& d, const std::wstring& Name) {
		auto it = std::find_if(layers.begin(), layers.end(), [Name](std::unique_ptr<ImageLayer>& layer) { return layer->Name() == Name; });
		if (it == layers.end())
			return;

		(*it)->Register(d);
	}

	void Canvas::SelectLayer(const int Index) noexcept {
		selecting_layer = &layers[Index];
	}

	void Canvas::SelectLayer(const std::wstring& Name) noexcept {
		auto it = std::find_if(layers.begin(), layers.end(), [Name](std::unique_ptr<ImageLayer>& layer) { return layer->Name() == Name; });
		if (it != layers.end()) {
			selecting_layer = &*it;
		}
	}

	void Canvas::DeleteLayer(const std::wstring& Name) noexcept {
		auto layer = std::find_if(layers.begin(), layers.end(), [Name](std::unique_ptr<ImageLayer>& layer) { return layer->Name() == Name; });
		Program::Instance().var_manager.Delete((*layer)->Name() + L".__管理中");
		layers.erase(layer);
	}

	void Canvas::DeleteLayer(const int Index) noexcept {
		Program::Instance().var_manager.Delete(layers[Index]->Name() + L".__管理中");
		layers.erase(layers.begin() + Index);
	}

	void Canvas::SetBasis(std::shared_ptr<Entity>& base, const std::wstring& Layer_Name) {
		auto it = std::find_if(layers.begin(), layers.end(), [Layer_Name](std::unique_ptr<ImageLayer>& layer) { return layer->Name() == Layer_Name; });
		if (it == layers.end())
			return;
		static_cast<RelativeLayer*>(it->get())->SetBase(base);
	}

	void Canvas::Show(const int Index) noexcept {
		layers[Index]->Show();
	}

	void Canvas::Hide(const int Index) noexcept {
		layers[Index]->Hide();
	}

	void Canvas::Show(const std::wstring& Name) noexcept {
		auto it = std::find_if(layers.begin(), layers.end(), [Name](std::unique_ptr<ImageLayer>& layer) { return layer->Name() == Name; });
		if (it != layers.end())
			(*it)->Show();
	}

	void Canvas::Hide(const std::wstring& Name) noexcept {
		auto it = std::find_if(layers.begin(), layers.end(), [Name](std::unique_ptr<ImageLayer>& layer) { return layer->Name() == Name; });
		if (it != layers.end())
			(*it)->Hide();
	}

	void Canvas::ApplyFilter(const std::wstring& Name, const std::wstring& Filter_Name, const int Potency) {
		FilterMaker filter_maker(Potency);
		auto it = std::find_if(layers.begin(), layers.end(), [Name](std::unique_ptr<ImageLayer>& layer) { return layer->Name() == Name; });
		if (it != layers.end())
			(*it)->SetFilter(filter_maker.Generate(Filter_Name));
	}

	bool Canvas::CreateLayer(std::unique_ptr<ImageLayer> layer, const int Index) {
		if (layer == nullptr ||
			std::find_if(layers.begin(), layers.end(), [&layer](std::unique_ptr<ImageLayer>& candidate) { return layer->Name() == candidate->Name(); }) != layers.end())
		{
			if (!layers.empty() && (Index < 0 || Index >= layers.size()))
				return false;
		}
		layers.insert(layers.begin() + Index, std::move(layer));
		return true;
	}

	bool Canvas::CreateRelativeLayer(const std::wstring& Name) {
		return CreateLayer(std::make_unique<RelativeLayer>(Name), 0);
	}

	bool Canvas::CreateAbsoluteLayer(const std::wstring& Name) {
		return CreateLayer(std::make_unique<AbsoluteLayer>(Name), 0);
	}

	bool Canvas::CreateRelativeLayer(const std::wstring& Name, const int Index) {
		return CreateLayer(std::make_unique<RelativeLayer>(Name), Index);
	}

	bool Canvas::CreateAbsoluteLayer(const std::wstring& Name, const int Index) {
		return CreateLayer(std::make_unique<AbsoluteLayer>(Name), Index);
	}

	std::wstring Canvas::GetLayerInfo(const int Index) {
		if (Index >= layers.size()) {
			return L"";
		}

		std::wstring sen{};
		sen += layers[Index]->Name() + L'=';						// 名前
		sen += std::wstring(layers[Index]->KindName()) + L':';		// 種類名
		sen += std::to_wstring(Index);								// 要素位置
		return sen;
	}
}