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

	Layer::Layer(const std::wstring& Layer_Name) noexcept {
		screen = Program::Instance().engine.MakeScreen();
		name = Layer_Name;
		Program::Instance().var_manager.MakeNew(Name() + L".__管理中") = std::wstring(L"");
	}

	void Layer::SetFilter(std::unique_ptr<Filter> new_filter) noexcept {
		filter = std::move(new_filter);
	}

	void Layer::Execute() noexcept {
		if (IsShowing())
			Draw();
	}

	void Layer::Register(std::shared_ptr<Entity>& drawable_entity) noexcept {
		if (IsRegistered(drawable_entity))
			return;

		auto& var = Program::Instance().var_manager.Get<false>(Name() + L".__管理中");
		std::any_cast<std::wstring&>(var) += std::wstring(drawable_entity->Name()) + L'\\';
		drawing.push_back(drawable_entity);
	}

	bool Layer::IsRegistered(const std::shared_ptr<Entity>& Drawable_Entity) const noexcept {
		return std::find(drawing.begin(), drawing.end(), Drawable_Entity) != drawing.end();
	}

	void Layer::Remove(const std::shared_ptr<Entity>& Target_Entity) noexcept {
		auto iterator = std::find(drawing.begin(), drawing.end(), Target_Entity);
		if (iterator != drawing.end()) {
			drawing.erase(iterator);
			auto& var = Program::Instance().var_manager.Get<false>(Name() + L".__管理中");
			auto& str = std::any_cast<std::wstring&>(var);
			const auto Position = str.find(Target_Entity->Name());
			str.erase(Position, Position + wcslen(Target_Entity->Name()) + 1);
		}
	}

	/**
	* 相対位置型レイヤー
	* baseを中心として周囲の描写を行うレイヤー。
	*/
	class RelativeLayer : public Layer {
		std::shared_ptr<Entity> base = nullptr;
	public:
		inline RelativeLayer(const std::wstring& lname) noexcept : Layer(lname) {
			SetFilter(std::make_unique<filter::None>());
			Program::Instance().var_manager.MakeNew(lname + L".ベース") = std::wstring(L"");
		}

		~RelativeLayer() noexcept {
			Program::Instance().var_manager.Delete(Name() + L".ベース");
		}

		void Execute() noexcept override {
			Layer::Execute();
			if (base != nullptr && base->CanDelete()) {
				Program::Instance().var_manager.Get<false>(Name() + L".ベース") = std::wstring(L"");
				base = nullptr;
			}
		}

		// 中心となるEntityを設定する。
		void SetBase(std::shared_ptr<Entity>& ent) noexcept {
			base = ent;
			Program::Instance().var_manager.Get<false>(Name() + L".ベース") = std::wstring(base->Name());
		}

		/**
		* 描写
		* (0,0) <= base_origin <= (W, H)を満たすように画面描写を行う。
		*/
		void Draw() noexcept override {
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
	class AbsoluteLayer : public Layer {
	public:
		inline AbsoluteLayer(const std::wstring& lname) : Layer(lname) {
			SetFilter(std::make_unique<filter::None>());
		}

		void Draw() noexcept override {
			auto& p = Program::Instance();
			p.engine.ChangeTargetScreen(Screen);
			p.engine.ClearScreen();
			for (auto drawer : drawing) {
				if (drawer->Origin()[0] + drawer->Length()[0] < 0 || drawer->Origin()[0] > Program::Instance().WindowSize().first ||
					drawer->Origin()[1] + drawer->Length()[1] < 0 || drawer->Origin()[1] > Program::Instance().WindowSize().second)
					continue;

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
			layer->Execute();
		}
	}

	void Canvas::Register(std::shared_ptr<Entity>& d) {
		if (selecting_layer == layers.end())
			return;

		(*selecting_layer)->Register(d);
	}

	void Canvas::Register(std::shared_ptr<Entity>& d, const int Index) {
		if (!IsIndexValid(Index))
			return;

		layers[Index]->Register(d);
	}

	void Canvas::Register(std::shared_ptr<Entity>& d, const std::wstring& Name) {
		auto it = std::find_if(layers.begin(), layers.end(), FindLayerByName(Name));
		if (it == layers.end())
			return;

		(*it)->Register(d);
	}

	void Canvas::SelectLayer(const int Index) noexcept {
		if (!IsIndexValid(Index))
			return;

		selecting_layer = layers.begin() + Index;
	}

	void Canvas::SelectLayer(const std::wstring& Name) noexcept {
		auto it = std::find_if(layers.begin(), layers.end(), FindLayerByName(Name));
		if (it != layers.end()) {
			selecting_layer = it;
		}
	}

	bool Canvas::DeleteLayer(const std::wstring& Name) noexcept {
		auto layer = std::find_if(layers.begin(), layers.end(), FindLayerByName(Name));
		if (layer == layers.end())
			return false;

		if (selecting_layer != layers.end() && (*selecting_layer)->Name() == Name)
			selecting_layer = layers.end();

		Program::Instance().var_manager.Delete((*layer)->Name() + L".__管理中");
		const std::wstring&& Selecting_Name = (selecting_layer != layers.end() ? (*selecting_layer)->Name() : L"");
		layers.erase(layer);
		if (!Selecting_Name.empty())
			selecting_layer = std::find_if(layers.begin(), layers.end(), FindLayerByName(Selecting_Name));
		return true;
	}

	bool Canvas::DeleteLayer(const int Index) noexcept {
		if (!IsIndexValid(Index))
			return false;

		if (selecting_layer != layers.end() && (*selecting_layer)->Name() == (*(layers.begin() + Index))->Name())
			selecting_layer = layers.end();

		Program::Instance().var_manager.Delete(layers[Index]->Name() + L".__管理中");

		const std::wstring&& Selecting_Name = (selecting_layer != layers.end() ? (*selecting_layer)->Name() : L"");
		layers.erase(layers.begin() + Index);
		if (!Selecting_Name.empty())
			selecting_layer = std::find_if(layers.begin(), layers.end(), FindLayerByName(Selecting_Name));
		return true;
	}

	void Canvas::SetBasis(std::shared_ptr<Entity>& base, const std::wstring& Layer_Name) {
		auto it = std::find_if(layers.begin(), layers.end(), FindLayerByName(Layer_Name));
		if (it == layers.end())
			return;
		static_cast<RelativeLayer*>(it->get())->SetBase(base);
	}

	void Canvas::Show(const int Index) noexcept {
		if (!IsIndexValid(Index))
			return;

		layers[Index]->Show();
	}

	void Canvas::Hide(const int Index) noexcept {
		if (!IsIndexValid(Index))
			return;

		layers[Index]->Hide();
	}

	void Canvas::Show(const std::wstring& Name) noexcept {
		auto it = std::find_if(layers.begin(), layers.end(), FindLayerByName(Name));
		if (it != layers.end())
			(*it)->Show();
	}

	void Canvas::Hide(const std::wstring& Name) noexcept {
		auto it = std::find_if(layers.begin(), layers.end(), FindLayerByName(Name));
		if (it != layers.end())
			(*it)->Hide();
	}

	void Canvas::ApplyFilter(const std::wstring& Name, const std::wstring& Filter_Name, const int Potency) {
		FilterMaker filter_maker(Potency);
		auto it = std::find_if(layers.begin(), layers.end(), FindLayerByName(Name));
		if (it != layers.end())
			(*it)->SetFilter(filter_maker.Generate(Filter_Name));
	}

	bool Canvas::CreateLayer(std::unique_ptr<Layer> layer, const int Index) {
		if (layer == nullptr ||
			std::find_if(layers.begin(), layers.end(), FindLayerByName(layer)) != layers.end())
		{
			if (!layers.empty() && (Index < 0 || Index >= layers.size()))
				return false;
		}
		const std::wstring&& Selecting_Name = (!layers.empty() && selecting_layer != layers.end() ? (*selecting_layer)->Name() : L"");
		layers.insert(layers.begin() + Index, std::move(layer));
		if (!Selecting_Name.empty())
			selecting_layer = std::find_if(layers.begin(), layers.end(), FindLayerByName(Selecting_Name));
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
		if (!IsIndexValid(Index)) {
			return L"";
		}

		std::wstring sen{};
		sen += layers[Index]->Name() + L'=';						// 名前
		sen += std::wstring(layers[Index]->KindName()) + L':';		// 種類名
		sen += std::to_wstring(Index);								// 要素位置
		return sen;
	}

	bool Canvas::IsIndexValid(const int Index) const noexcept {
		return (!layers.empty() && Index >= 0 && Index < layers.size());
	}

	void Canvas::Remove(const std::shared_ptr<Entity>& Target_Entity) noexcept {
		for (auto& layer : layers) {
			layer->Remove(Target_Entity);
		}
	}
}