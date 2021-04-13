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
	protected:
		Layer();
		const TargetRender& Screen = screen;
		std::unique_ptr<Filter> filter;
	public:
		// ���\�[�X��o�^����B
		virtual void Register(std::shared_ptr<Entity>) = 0;
		virtual void Execute() = 0;
		void SetFilter(std::unique_ptr<Filter>);
	};

	class ImageLayer : public Layer {
		std::wstring name{};	// ���C���[��
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

	// �L�����o�X
	class Canvas final : private Singleton {
		// ���C���[�̃|�C���^
		using LayerPtr = std::unique_ptr<ImageLayer>;
		// ���C���[
		using Layers = std::vector<LayerPtr>;
		// �Ǘ������C���[
		Layers layers;
		// ����Ώۂ̃��C���[
		LayerPtr* selecting_layer{};
	public:
		void Update() noexcept;
		void Register(std::shared_ptr<Entity>&),
			Register(std::shared_ptr<Entity>&, const int), 
			Register(std::shared_ptr<Entity>&, const std::wstring&);

		// ���C���[���쐬���A�ǉ�����B
		bool CreateRelativeLayer(const std::wstring&), CreateAbsoluteLayer(const std::wstring&);
		bool CreateRelativeLayer(const std::wstring&, const int), CreateAbsoluteLayer(const std::wstring&, const int);
		bool CreateLayer(std::unique_ptr<ImageLayer>, const int);
		void DeleteLayer(const std::wstring&) noexcept, DeleteLayer(const int) noexcept;
		// ����Ώۂ̃��C���[��I������B
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