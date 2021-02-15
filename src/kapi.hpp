#pragma once
#undef PlaySound
#undef LoadImage

#include <memory>
#include <valarray>
#include <string>
#include <functional>

// �����^�����Ƃ����V�����^���`���邽�߂̃}�N��
#define KARAPO_NEWTYPE(newone,base_interger_type) enum class newone : base_interger_type{}

namespace karapo {
	struct ProgramInterface;

	namespace raw {
		using TargetRender = int;
		using Resource = int;
	}

	// ���������_��
	using Dec = double;
	using Rect = RECT;
	using Point = POINT;

	using Char = wchar_t;				// ��������ѕ�����̃|�C���^
	using String = std::wstring;	// ������

	namespace resource {
		enum class Resource : int { Invalid = -1 };

		struct ResourceType {
			inline operator Resource() const noexcept { return resource; }
		protected:
			Resource resource = Resource::Invalid;
			String path;
		public:
			const String& Path = path;
			std::function<void()> Reload;
			inline auto Load(const String& P) { path = P; Reload(); }
		};

		class Image : public ResourceType {
		public:
			using Length = int;
			Image(ProgramInterface*);
		};

		class Sound : public ResourceType {
		public:
			Sound(ProgramInterface*);
		};

		class Video : public Image {
		public:
			using Image::Image;
		};
	}

	template<typename T>
	using SmartPtr = std::shared_ptr<T>;
	// �p�ɂɎg�p����STL�n�z��
	template<typename T>
	using Array = std::vector<T>;

	struct Color {
		int r, g, b;
	};

	KARAPO_NEWTYPE(TargetRender, raw::TargetRender);

	// �x�N�g��
	template<typename T>
	using Vector = std::valarray<T>;
	using WorldVector = Vector<Dec>;	// �����^�x�N�g��
	using ScreenVector = Vector<int>;	// �����^�x�N�g��
	using BinaryVector = Vector<bool>;

	template<typename T>
	constexpr T Length(const Vector<T>& v) {
		return std::sqrt(std::pow(v[0], 2) + std::pow(v[1], 2));
	}

	template<typename T>
	constexpr T Distance(const Vector<T>& v1, const Vector<T>& v2) noexcept {
		return Length(v1 - v2);
	}

	// �Q�[���̃L�����N�^�[��I�u�W�F�N�g�̌��ƂȂ�N���X�B
	class Entity {
	public:
		virtual ~Entity() = 0;

		// Entity���s�֐�
		virtual int Main() = 0;
		// ���g�̈ʒu
		virtual WorldVector Origin() const noexcept = 0;
		// ���g�̖��O
		virtual const Char* Name() const noexcept = 0;

		// ���g���폜�ł����Ԃɂ��邩��Ԃ��B
		virtual bool CanDelete() const noexcept = 0;
		// ���g���폜�ł����Ԃɂ���B
		virtual void Delete() = 0;
		// �`�ʂ���B
		virtual void Draw(WorldVector, TargetRender) = 0;
		// �w�肵�����W�փe���|�[�g����B
		virtual void Teleport(WorldVector) = 0;
	};

	enum class PlayType {
		Normal = 1,
		Loop = 3
	};

	struct ProgramInterface {
		std::function<void(int, int, int, int, Color)> DrawLine;
		std::function<void(const Rect, const resource::Image&)> DrawRectImage;
		std::function<void(const Rect, const karapo::TargetRender)> DrawRectScreen;
		std::function<bool(const resource::Resource)> IsPlayingSound;
		std::function<resource::Resource(const String&)> LoadImage;
		std::function<resource::Resource(const String&)> LoadSound;

		// - Entity�n -

		std::function<void(SmartPtr<Entity>)> RegisterEntity;
		std::function<void(const String&)> KillEntity;
		std::function<SmartPtr<Entity>(const String&)> GetEntity;

		// - Event�n -

		std::function<void(const String&)> ExecuteEventByName;
		std::function<void(const WorldVector)> ExecuteEventByOrigin;
	};
}