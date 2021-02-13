#include "Engine.hpp"

static auto program = karapo::Program();

namespace karapo {
	Program* GetProgram() {
		return &program;
	}
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	return program.Main();
}