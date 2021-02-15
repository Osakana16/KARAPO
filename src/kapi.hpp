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

		// - Entity系 -

		std::function<void(SmartPtr<Entity>)> RegisterEntity;
		std::function<void(const String&)> KillEntity;
		std::function<SmartPtr<Entity>(const String&)> GetEntity;

		// - Event系 -

		std::function<void(const String&)> ExecuteEventByName;
		std::function<void(const WorldVector)> ExecuteEventByOrigin;
	};
}