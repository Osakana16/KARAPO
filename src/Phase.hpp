#pragma once
namespace karapo::phase {
	class Phase {
		SmartPtr<Phase> request_phase;
	public:
		void PullRequest(SmartPtr<Phase>&);
		SmartPtr<Phase> Request() const;

		virtual void Update() = 0;
	};
}