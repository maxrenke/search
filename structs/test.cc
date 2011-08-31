#include "../incl/utils.hpp"

const static Test tests[] = {
};

enum { Ntests = sizeof(tests) / sizeof(tests[0]) };

const static Benchmark benches[] = {
};

enum { Nbenches = sizeof(benches) / sizeof(benches[0]) };

int main(int argc, const char *argv[]) {
	const char *regexp = ".*";
	if (argc == 2)
		regexp = argv[1];

	bool ok = runtests(tests, Ntests, regexp);
	runbenches(benches, Nbenches, regexp);

	return ok ? 0 : 1;
}