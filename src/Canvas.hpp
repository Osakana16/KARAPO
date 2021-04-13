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
	protected:
		Layer();
		const TargetRender& Screen = screen;
		std::unique_ptr<Filter> filter;
	public:
		// リソースを登録する。
		virtual void Register(std::shared_ptr<Entity>) = 0;
		virtual void Execute() = 0;
		void SetFilter(std::unique_ptr<Filter>);
	};

	class ImageLayer : public Layer {
		std::wstring name{};	// レイヤー名
	protected:
		std::vector<std::shared_ptr<Entity>> drawing;
	public:
		ImageLayer(const std::wstring& lname) : Layer() { name = lname; }
		virtual void Execute() override;
		void Register(std::shared_ptr<Entity>);
		bool IsRegistered(std::shared_ptr<Entity>) const noexcept;
		inline auto Name() const noexcept { return name; }

		virtual void Draw() = 0;
	};

	// キャンバス
	class Canvas final : private Singleton {
		// レイヤーのポインタ
		using LayerPtr = std::unique_ptr<ImageLayer>;
		// レイヤー
		using Layers = std::vector<LayerPtr>;
		// 管理中レイヤー
		Layers layers;
		// 操作対象のレイヤー
		LayerPtr* selecting_layer{};
	public:
		void Update() noexcept;
		void Register(std::shared_ptr<Entity>&),
			Register(std::shared_ptr<Entity>&, const int), 
			Register(std::shared_ptr<Entity>&, const std::wstring&);

		// レイヤーを作成し、追加する。
		bool CreateRelativeLayer(const std::wstring&), CreateAbsoluteLayer(const std::wstring&);
		bool CreateRelativeLayer(const std::wstring&, const int), CreateAbsoluteLayer(const std::wstring&, const int);
		bool CreateLayer(std::unique_ptr<ImageLayer>, const int);
		void DeleteLayer(const std::wstring&) noexcept, DeleteLayer(const int) noexcept;
		// 操作対象のレイヤーを選択する。
		void SelectLayer(const int) noexcept, SelectLayer(const std::wstring&) noexcept;

		void ApplyFilter(const std::wstring&, const std::wstring&, const int);

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