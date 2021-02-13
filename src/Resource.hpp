#pragma once

namespace karapo {
	namespace resource {
		enum class Resource : int { Invalid = -1 };

		struct ResourceType {
			operator Resource() const noexcept;
		protected:
			Resource resource = Resource::Invalid;
		};

		class Image : public ResourceType {
			String path;
		public:
			using Length = int;

			const String& Path = path;
			void Load(const String&);
			void Reload();
		};

		class Video : public Image {
		public:
			using Image::Image;
		};

		class Sound : public ResourceType {
			String path;
		public:
			const String& Path = path;
			void Load(const String&);
			void Reload();
			using ResourceType::ResourceType;
		};
	}
}