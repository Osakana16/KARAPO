/**
* Canvas.hpp - �Q�[����ł̃L�����o�X�E���C���[�@�\�̒�`�Q�B
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

	// ���C���[
	class Layer {
		TargetRender screen;
		std::wstring name{};	// ���C���[��
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
		// ���\�[�X��o�^����B
		virtual void Register(std::shared_ptr<Entity>&) noexcept;
		bool IsRegistered(const std::shared_ptr<Entity>&) const noexcept;
		inline auto Name() const noexcept { return name; }
		void Show() noexcept { hide = true; }
		void Hide() noexcept { hide = false; }

		virtual void Draw() noexcept = 0;
		virtual const wchar_t* KindName() const noexcept = 0;
	};

	// �L�����o�X
	class Canvas final : private Singleton {
		// ���C���[�̃|�C���^
		using LayerPtr = std::unique_ptr<Layer>;
		// ���C���[
		using Layers = std::vector<LayerPtr>;
		// �Ǘ������C���[
		Layers layers;
		std::vector<std::pair<int, LayerPtr>> hiding{};

		// ����Ώۂ̃��C���[
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

		// ���C���[���쐬���A�ǉ�����B
		bool CreateRelativeLayer(const std::wstring&), CreateAbsoluteLayer(const std::wstring&);
		bool CreateRelativeLayer(const std::wstring&, const int), CreateAbsoluteLayer(const std::wstring&, const int);
		bool CreateLayer(std::unique_ptr<Layer>, const int);
		bool DeleteLayer(const std::wstring&) noexcept, DeleteLayer(const int) noexcept;
		// ����Ώۂ̃��C���[��I������B
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