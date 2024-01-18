#include <string>
#include <iostream>

int main() {
	std::string wow;
	size_t position = 0;

	// Need to escape quote with a double of ".
	while (position < wow.size()) {
		if (wow.at(position) == '\"') {
			wow.insert(position, "\"");
			position++;
		}
		position++;
	}

	std::cout << wow << std::endl;

	return 0;
}
