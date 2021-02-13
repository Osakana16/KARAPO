#pragma once
#include <queue>

namespace karapo::memory {
	template<typename T>
	concept Managenable =
		requires(T m) { m.Main(); m.CanDelete(); m.Delete(); } ||
		requires(T m) { m.Draw(); };

	// ゲーム内からEntityを削除するクラス。
	template<typename T>
	class Execusion final {
		// 削除リスト
		std::queue<T*> deadmen{};
	public:
		// 削除対象に登録する。
		void Register(T* se) noexcept {
			deadmen.push(se);
		}

		// listsからの削除を実行する。
		template<typename Array>
		void Execute(Array& lists) noexcept {
			for (auto& l : lists) {
				for (; !deadmen.empty(); deadmen.pop()) {
					if (&l == deadmen.front())
						l = nullptr;
				}
			}
		}
	};

	template<Managenable T>
	class Chunk {
		std::vector<std::shared_ptr<T>> objects;
		std::unordered_map<String, std::weak_ptr<T>> refs;
	public:
		void Update() noexcept {
		}

		std::shared_ptr<T> Get(const String& Name) const noexcept {
			try {
				return std::shared_ptr<T>(refs.at(Name));
			} catch (...) {
				return nullptr;
			}
		}

		std::shared_ptr<T> Get(std::function<bool(std::shared_ptr<T>)> Condition) const noexcept {
			for (auto& o : objects) {
				if (Condition(o)) {
					return objects;
				}
			}
			return nullptr;
		}
	};

	template<Managenable T, uint N>
	class FixedChunk {
		std::shared_ptr<T> objects[N]{ nullptr };
		std::unordered_map<String, std::weak_ptr<T>> refs;
	public:
		FixedChunk() noexcept {
			refs.clear();
		}

		auto Update() noexcept {
			Execusion<std::shared_ptr<T>> deadmen;
			for (auto& object : objects) {
				if (object == nullptr)
					continue;

				if (!object->CanDelete())
					object->Main();
				else
					deadmen.Register(&object);
			}
			deadmen.Execute<std::shared_ptr<T>[N]>(objects);
		}

		auto Size() const noexcept {
			return refs.size();
		}

		bool IsFull() const noexcept {
			for (auto& object : objects) {
				if (object == nullptr)
					return false;
			}
			return true;
		}

		auto Register(std::shared_ptr<T> object) {
			Register(object, object->Name());
		}

		auto Register(std::shared_ptr<T> object, const String& Name) {
			if (object == nullptr)
				return;

			refs[object->Name()] = object;
			for (int i = 0; i < N; i++) {
				if (objects[i] == nullptr) {
					objects[i] = object;
					break;
				}
			}
		}

		auto Kill(const String& name) noexcept {
			try {
				auto weak = refs.at(name);
				auto shared = weak.lock();
				shared->Delete();
			} catch (...) {}
		}

		std::shared_ptr<T> Get(const String& Name) const noexcept {
			try {
				std::weak_ptr<T> weak = refs.at(Name);
				if (!weak.expired())
					return weak.lock();
			} catch (...) {}
			return nullptr;
		}

		std::shared_ptr<T> Get(std::function<bool(std::shared_ptr<T>)> Condition) const noexcept {
			for (auto& o : objects) {
				if (Condition(o)) {
					return o;
				}
			}
			return nullptr;
		}
	};
}