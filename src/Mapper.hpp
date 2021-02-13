/**
* Mapper.hpp - テキストファイルを
*/
#pragma once
namespace karapo::mapper {
	struct PlainCharMapper {
		std::vector<std::vector<Char>> map;
		PlainCharMapper(const String Path);
	};
}