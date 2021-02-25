#pragma once
namespace karapo::phase {
	class Phase {
		std::shared_ptr<Phase> request_phase;
	public:
		void PullRequest(std::shared_ptr<Phase>&);
		std::shared_ptr<Phase> Request() const;

		virtual void Update() = 0;
	};
}