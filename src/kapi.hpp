#pragma once
#undef PlaySound
#undef LoadImage

#include <memory>
#include <valarray>
#include <string>
#include <functional>

// 整数型を元とした新しい型を定義するためのマクロ
#define KARAPO_NEWTYPE(newone,base_interger_type) enum class newone : base_interger_type{}

namespace karapo {
	struct ProgramInterface;

	namespace raw {
		using TargetRender = int;
		using Resource = int;
	}

	// 浮動小数点数
	using Dec = double;
	using Rect = RECT;
	using Point = POINT;

	using Char = wchar_t;				// 文字および文字列のポインタ
	using String = std::wstring;	// 文字列

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
	// 頻繁に使用するSTL系配列
	template<typename T>
	using Array = std::vector<T>;

	struct Color {
		int r, g, b;
	};

	KARAPO_NEWTYPE(TargetRender, raw::TargetRender);

	// ベクトル
	template<typename T>
	using Vector = std::valarray<T>;
	using WorldVector = Vector<Dec>;	// 実数型ベクトル
	using ScreenVector = Vector<int>;	// 整数型ベクトル
	using BinaryVector = Vector<bool>;

	template<typename T>
	constexpr T Length(const Vector<T>& v) {
		return std::sqrt(std::pow(v[0], 2) + std::pow(v[1], 2));
	}

	template<typename T>
	constexpr T Distance(const Vector<T>& v1, const Vector<T>& v2) noexcept {
		return Length(v1 - v2);
	}

	// ゲームのキャラクターやオブジェクトの元となるクラス。
	class Entity {
	public:
		virtual ~Entity() = 0;

		// Entity実行関数
		virtual int Main() = 0;
		// 自身の位置
		virtual WorldVector Origin() const noexcept = 0;
		// 自身の名前
		virtual const Char* Name() const noexcept = 0;

		// 自身が削除できる状態にあるかを返す。
		virtual bool CanDelete() const noexcept = 0;
		// 自身を削除できる状態にする。
		virtual void Delete() = 0;
		// 描写する。
		virtual void Draw(WorldVector, TargetRender) = 0;
		// 指定した座標へテレポートする。
		virtual void Teleport(WorldVector) = 0;
	};

	namespace event {
		// イベント用コマンド
		class Command {
		public:
			virtual ~Command() = 0;

			// コマンドを実行する。
			virtual void Execute() = 0;

			// 実行し終えたかどうか。
			virtual bool Executed() const noexcept = 0;

			// コマンドが不要になったかどうかを表す。
			// trueを返す場合、再実行してはいけない事を意味する。
			virtual bool IsUnnecessary() const noexcept = 0;
		};

		using CommandPtr = std::unique_ptr<Command>;

		// コマンドの情報
		struct KeywordInfo final {
			// 生成したコマンドを返す。
			std::function<CommandPtr()> Result = []() -> CommandPtr { return nullptr; };

			// 引数の数が十分であるか否かを返す。
			std::function<bool()> isEnough = []() -> bool { return false; };

			// コマンドが解析中に実行されるか否かどうか。
			bool is_static = false;

			// コマンドがイベント実行中に実行されるか否かどうか。
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
		std::function<void(const Rect, const resource::Image&)> DrawRectImage;
		std::function<void(const Rect, const karapo::TargetRender)> DrawRectScreen;
		std::function<bool(const resource::Resource)> IsPlayingSound;
		std::function<resource::Resource(const String&)> LoadImage;
		std::function<resource::Resource(const String&)> LoadSound;

		// - Entity系 -

		std::function<void(SmartPtr<Entity>)> RegisterEntity;
		std::function<void(const String&)> KillEntity;
		std::function<SmartPtr<Entity>(const String&)> GetEntity;

		// - Event系 -

		std::function<void(const String&)> ExecuteEventByName;
		std::function<void(const WorldVector)> ExecuteEventByOrigin;
	};
}