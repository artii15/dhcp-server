#ifndef OPTIONS_H
#define OPTIONS_H

#include <map>
#include "option.h"

class Options {
	public:
		Options(uint8_t* rawOptions, unsigned maxOptionsLength);
		Option& get(uint8_t code);

	private:
		void read(uint8_t* rawOptions, unsigned maxOptionsLength);

		std::map<uint8_t, Option> options;
};

#endif
