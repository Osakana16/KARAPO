/**
* Mapper.hpp - �e�L�X�g�t�@�C����
*/
#pragma once
namespace karapo::mapper {
	struct PlainCharMapper {
		std::vector<std::vector<Char>> map;
		PlainCharMapper(const String Path);
	};
}