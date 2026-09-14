// CACHE_HEADER is selected by the CMake executable target.
#include CACHE_HEADER

#include <cassert>
#include <iostream>

int slow_get_page(const int key) {
	return key;
}

int main() {
	std::size_t m, n;
	int hits {0};
	std::cin >> m >> n;
	cache_t<int> c{m};
	for (std::size_t i = 0; i < n; ++i) {
		int p;
		std::cin >> p;
		assert(std::cin.good());
		if (c.lookup_update(p, slow_get_page)) hits += 1;
	}
	std::cout << hits << '\n';

}
