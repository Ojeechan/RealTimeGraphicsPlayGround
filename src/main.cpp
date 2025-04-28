#include "rt_graphics_app.hpp"
#include <exception>
#include <iostream>

int main() {
	RTGraphicsApp app;
	try {
		app.run();
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}