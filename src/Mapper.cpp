#include "Mapper.hpp"

namespace karapo::mapper {
	PlainCharMapper::PlainCharMapper(const String Path) {
		map.push_back({});
		FILE *fp = nullptr;
		if (_wfopen_s(&fp, Path.c_str(), L"r,ccs=UNICODE")) {
			Char c;
			while ((c = fgetwc(fp)) != WEOF) {
				if (c == L'\n') {
					map.push_back({});
				} else {
					map.back().push_back(c);
				}
			}
			fclose(fp);
		}
	}
}