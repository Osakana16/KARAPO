#pragma once
#undef PlaySound
#undef LoadImage

#include "kio.hpp"

#include <any>
#include <memory>
#include <valarray>
#include <string>
#include <functional>

// 整数型を元とした新しい型を定義するためのマクロ
#define KARAPO_NEWTYPE(newone,base_interger_type) enum class newone : base_interger_type{}

namespace karapo {
	class Singleton {
		Singleton(const Singleton&) = delete;
		Singleton(Singleton&&) = delete;
		Singleton& operator=(const Singleton&) = delete;
		Singleton& operator=(Singleton&&) = delete;
	protected:
		Singleton() = default;
		~Singleton() = default;
	};

	struct ProgramInterface;

	namespace raw {
		using TargetRender = int;
		using Resource = int;
	}

	// 浮動小数点数
	using Dec = double;
	using Rect = RECT;
	using Point = POINT;

	namespace resource {
		enum class Resource : int { Invalid = -1 };

		struct ResourceType {
			inline operator Resource() const noexcept { return resource; }
		protected:
			Resource resource = Resource::Invalid;
			std::wstring path;
			ProgramInterface* pi;
		public:
			const std::wstring& Path = path;
			std::function<void()> Reload;

			ResourceType(ProgramInterface*);
			void Load(const std::wstring&);
			bool IsValid() const noexcept;
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

	struct Color {
		int r, g, b;
	};

	enum class TargetRender : raw::TargetRender { Invalid = -1 };

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

	namespace value {
		KARAPO_NEWTYPE(Key, int);
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
		virtual const wchar_t* Name() const noexcept = 0;
		// 種類名
		virtual const wchar_t* KindName() const noexcept = 0;

		// 自身が削除できる状態にあるかを返す。
		virtual bool CanDelete() const noexcept = 0;
		// 自身を削除できる状態にする。
		virtual void Delete() = 0;
		// 描写する。
		virtual void Draw(WorldVector) = 0;
		// 指定した座標へテレポートする。
		virtual void Teleport(WorldVector) = 0;
	};

	namespace variable {
		static constexpr const wchar_t* const Managing_Var_Name = L"__管理中変数";
		static constexpr const wchar_t* const Managing_Entity_Name = L"__管理中キャラ";
		static constexpr const wchar_t* const Executing_Event_Name = L"__実行中イベント";
	}

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

			// 条件分岐の結果にかかわらず、強制的に実行すべきコマンドかを返す。
			virtual bool IgnoreCondition() const noexcept { return false; }
		};

		using CommandPtr = std::unique_ptr<Command>;

		// コマンドの情報
		struct KeywordInfo final {
			enum class ParamResult {
				Lack,		// 引数不足
				Medium,		// 引数十分
				Maximum,	// 引数十分(これ以上、引数必要なし)
				Excess		// 引数余分
			};
			// 生成したコマンドを返す。
			std::function<CommandPtr()> Result = []() -> CommandPtr { return nullptr; };

			// 引数の数が十分であるか否かを返す。
			std::function<ParamResult()> checkParamState = []() -> ParamResult { return ParamResult::Lack; };

			// コマンドが解析中に実行されるか否かどうか。
			bool is_static = false;

			// コマンドがイベント実行中に実行されるか否かどうか。
			bool is_dynamic = false;
		};

		class EventEditor;

		using GenerateFunc = std::function<KeywordInfo(const std::vector<std::wstring>&)>;
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
		std::function<void(const std::wstring& Mes, const ScreenVector O, const int Font_Size, const Color C)> DrawSentence;
		std::function<bool(const resource::Resource)> IsPlayingSound;
		std::function<resource::Resource(const std::wstring&)> LoadImage;
		std::function<resource::Resource(const std::wstring&)> LoadSound;

		// - Canvas系 -

		std::function<bool(const std::wstring&)> CreateAbsoluteLayer,
												 CreateRelativeLayer;
		std::function<bool(const std::wstring&, const int)> InsertAbsoluteLayer,
															InsertRelativeLayer;
		std::function<void(const int)> DeleteLayerByIndex;
		std::function<void(const std::wstring&)> DeleteLayerByName;


		// - Entity系 -

		std::function<void(std::shared_ptr<Entity>)> RegisterEntity;
		std::function<void(std::shared_ptr<Entity>, const std::wstring&)> RegisterEntityByLayerName;
		std::function<void(const std::wstring&)> KillEntity;
		std::function<std::shared_ptr<Entity>(const std::wstring&)> GetEntityByName;
		std::function<std::shared_ptr<Entity>(std::function<bool(std::shared_ptr<Entity>)>)> GetEntityByFunc;

		// - Event系 -

		std::function<void(const std::wstring&)> LoadEvent;
		std::function<void(const std::wstring&)> ExecuteEventByName;
		std::function<void(const WorldVector)> ExecuteEventByOrigin;
		std::function<std::any&(const std::wstring&)> MakeVar, GetVar;

		std::function<event::EventEditor*()> GetEventEditor;
		std::function<void(event::EventEditor*, const std::wstring&)>
			MakeNewEvent, SetTargetEvent, ChangeEventTriggerType;
		std::function<void(event::EventEditor*, const std::wstring&, const int)>
			AddCommand;
		std::function<void(event::EventEditor*, const WorldVector, const WorldVector)>
			ChangeEventRange;
		std::function<void(event::EventEditor*)> FreeEventEditor;
		std::function<std::pair<std::wstring, std::wstring>(const std::wstring&)> GetParamInfo;
		std::function<bool(const std::wstring&)> IsStringType, IsNumberType, IsNoType;

		// - キー系 -

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