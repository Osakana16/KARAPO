#include "Phase.hpp"

namespace karapo::phase {
	void Phase::PullRequest(std::shared_ptr<Phase>& p) {
		request_phase = p;
	}

	std::shared_ptr<Phase> Phase::Request() const {
		return request_phase;
	}
}