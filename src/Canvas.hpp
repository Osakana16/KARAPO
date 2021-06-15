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

	class FilterMaker final {
		std::unordered_map<std::wstring, std::function<std::unique_ptr<Filter> ()>> filters;
	public:
		FilterMaker(const int);
		std::unique_ptr<Filter> Generate(const std::wstring&);
	};

	// レイヤー
	class Layer {
		TargetRender screen;
		std::wstring name{};	// レイヤー名
		bool hide = false;
		bool IsShowing() const noexcept { return !hide; }
	protected:
		std::vector<std::shared_ptr<Entity>> drawing{};
		Layer(const std::wstring& Layer_Name) noexcept;
		const TargetRender& Screen = screen;
		std::unique_ptr<Filter> filter;
	public:
		void SetFilter(std::unique_ptr<Filter>) noexcept;

		virtual void Execute() noexcept;
		// リソースを登録する。
		virtual void Register(std::shared_ptr<Entity>&) noexcept;
		bool IsRegistered(const std::shared_ptr<Entity>&) const noexcept;
		inline auto Name() const noexcept { return name; }
		void Show() noexcept { hide = true; }
		void Hide() noexcept { hide = false; }

		virtual void Draw() noexcept = 0;
		virtual const wchar_t* KindName() const noexcept = 0;
	};

	// キャンバス
	class Canvas final : private Singleton {
		// レイヤーのポインタ
		using LayerPtr = std::unique_ptr<Layer>;
		// レイヤー
		using Layers = std::vector<LayerPtr>;
		// 管理中レイヤー
		Layers layers;
		std::vector<std::pair<int, LayerPtr>> hiding{};

		// 操作対象のレイヤー
		Layers::iterator selecting_layer{};

		template<typename T>
		static std::function<bool(const std::unique_ptr<Layer>&)> FindLayerByName(const T& Findable) noexcept {
			if constexpr (std::is_same_v<std::wstring, T>) {
				return [&Findable](const std::unique_ptr<Layer>& layer) noexcept -> bool {
					return layer->Name() == Findable;
				};
			} else if constexpr (std::is_same_v<std::unique_ptr<Layer>, T>) {
				return [&Findable](const std::unique_ptr<Layer>& layer) noexcept -> bool {
					return layer->Name() == Findable->Name();
				};
			} else
				static_assert(0);
		}

		bool IsIndexValid(const int) const noexcept;
	public:
		void Update() noexcept;
		void Register(std::shared_ptr<Entity>&),
			Register(std::shared_ptr<Entity>&, const int), 
			Register(std::shared_ptr<Entity>&, const std::wstring&);

		// レイヤーを作成し、追加する。
		bool CreateRelativeLayer(const std::wstring&), CreateAbsoluteLayer(const std::wstring&);
		bool CreateRelativeLayer(const std::wstring&, const int), CreateAbsoluteLayer(const std::wstring&, const int);
		bool CreateLayer(std::unique_ptr<Layer>, const int);
		bool DeleteLayer(const std::wstring&) noexcept, DeleteLayer(const int) noexcept;
		// 操作対象のレイヤーを選択する。
		void SelectLayer(const int) noexcept, SelectLayer(const std::wstring&) noexcept;
		void SetBasis(std::shared_ptr<Entity>&, const std::wstring&);

		void ApplyFilter(const std::wstring&, const std::wstring&, const int);

		void Show(const int) noexcept, Hide(const int) noexcept,
			Show(const std::wstring&) noexcept, Hide(const std::wstring&) noexcept;

		std::wstring GetLayerInfo(const int);

		static Canvas& Instance() noexcept {
			static Canvas canvas;
			return canvas;
		}
	};

	class AudioManager {
		class MusicManager;
		class SoundManager;
	public:

	};
}