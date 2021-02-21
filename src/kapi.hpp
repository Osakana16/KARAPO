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
			ProgramInterface* pi;
		public:
			const String& Path = path;
			std::function<void()> Reload;

			ResourceType(ProgramInterface*);
			void Load(const String&);
		};

		class Image : public ResourceType {
		public:
			using Length = int;
			using ResourceType::ResourceType;
		};

		class Sound : public ResourceType {
		public:
			using ResourceType::ResourceType;
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

	namespace value {
		KARAPO_NEWTYPE(Key, int);
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

	namespace event {
		// �C�x���g�p�R�}���h
		class Command {
		public:
			virtual ~Command() = 0;

			// �R�}���h�����s����B
			virtual void Execute() = 0;

			// ���s���I�������ǂ����B
			virtual bool Executed() const noexcept = 0;

			// �R�}���h���s�v�ɂȂ������ǂ�����\���B
			// true��Ԃ��ꍇ�A�Ď��s���Ă͂����Ȃ������Ӗ�����B
			virtual bool IsUnnecessary() const noexcept = 0;
		};

		using CommandPtr = std::unique_ptr<Command>;

		// �R�}���h�̏��
		struct KeywordInfo final {
			// ���������R�}���h��Ԃ��B
			std::function<CommandPtr()> Result = []() -> CommandPtr { return nullptr; };

			// �����̐����\���ł��邩�ۂ���Ԃ��B
			std::function<bool()> isEnough = []() -> bool { return false; };

			// �R�}���h����͒��Ɏ��s����邩�ۂ��ǂ����B
			bool is_static = false;

			// �R�}���h���C�x���g���s���Ɏ��s����邩�ۂ��ǂ����B
			bool is_dynamic = false;
		};

		using GenerateFunc = std::function<KeywordInfo(const Array<String>&)>;
	}

	enum class PlayType {
		Normal = 1,
		Loop = 3
	};

	struct ProgramInterface {
		std::function<HWND()> GetHandler;
		std::function<std::pair<int, int>()> GetWindowLength;

		std::function<void(int, int, int, int, Color)> DrawLine;
		std::function<void(const Rect, const Color, const bool)> DrawRect;
		std::function<void(const Rect, const resource::Image&)> DrawRectImage;
		std::function<void(const Rect, const karapo::TargetRender)> DrawRectScreen;
		std::function<bool(const resource::Resource)> IsPlayingSound;
		std::function<resource::Resource(const String&)> LoadImage;
		std::function<resource::Resource(const String&)> LoadSound;

		// - Canvas�n -

		std::function<size_t()> CreateAbsoluteLayer;
		std::function<size_t(SmartPtr<Entity>)> CreateRelativeLayer;

		// - Entity�n -

		std::function<void(SmartPtr<Entity>, const size_t)> RegisterEntity;
		std::function<void(const String&)> KillEntity;
		std::function<SmartPtr<Entity>(const String&)> GetEntityByName;
		std::function<SmartPtr<Entity>(std::function<bool(SmartPtr<Entity>)>)> GetEntityByFunc;

		// - Event�n -

		std::function<void(const String&)> LoadEvent;
		std::function<void(const String&)> ExecuteEventByName;
		std::function<void(const WorldVector)> ExecuteEventByOrigin;

		// - �L�[�n -

		std::function<bool(const value::Key)> IsPressingKey, IsPressedKey;

		struct {
			value::Key
				F1,
				F2,
				F3,
				F4,
				F5,
				F6,
				F7,
				F8,
				F9,
				F10,
				F11,
				F12,
				Up,
				Left,
				Right,
				Down,
				App,
				Escape,
				Space,
				Backspace,
				Enter,
				Delete,
				Home,
				End,
				Insert,
				Page_Up,
				Page_Down,
				Tab,
				Caps,
				Pause,
				At,
				Colon,
				SemiColon,
				Plus,
				Minus,
				LBracket,
				RBracket,
				LCtrl,
				RCtrl,
				LShift,
				RShift,
				LAlt,
				RAlt,
				A,
				B,
				C,
				D,
				E,
				F,
				G,
				H,
				I,
				J,
				K,
				L,
				M,
				N,
				O,
				P,
				Q,
				R,
				S,
				T,
				U,
				V,
				W,
				X,
				Y,
				Z,
				N0,
				N1,
				N2,
				N3,
				N4,
				N5,
				N6,
				N7,
				N8,
				N9;
		} keys;
	};
}