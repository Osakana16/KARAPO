#include "Phase.hpp"

namespace karapo::phase {
	void Phase::PullRequest(SmartPtr<Phase>& p) {
		request_phase = p;
	}

	SmartPtr<Phase> Phase::Request() const {
		return request_phase;
	}
}