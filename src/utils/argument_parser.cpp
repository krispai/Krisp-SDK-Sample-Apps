#include "argument_parser.hpp"

#include <cassert>
#include <iostream>


ArgumentParser::ArgumentParser() {
}

ArgumentParser::ArgumentParser(int argc, char** argv, bool o) {
	this->argc = argc;
	this->argv = argv;
	ignore_others = o;
	assert(0 < argc);
	assert(0 != argv);
}

ArgumentParser::~ArgumentParser() {
}

void ArgumentParser::addArgument(const std::string& l, const std::string& s,
		const ArgType t) {
	assert(l.size());
	assert(s.size());
	if (!(arguments.insert(std::make_pair(l, ArgValue("", s, t, false))).second &&
		arguments.insert(std::make_pair(s, ArgValue("", l, t, false))).second)) {
		std::cerr << "Dublicate argument: "
			<< l << " " << s << "!" << std::endl;
		return;
	}
}

const bool ArgumentParser::parse() {
	assert(argc > 0);
	Map::iterator r = arguments.end();
	for (int i = 1; i < argc; ++i) {
		r = arguments.find(argv[i]);
		if (arguments.end() == r) {
			if (ignore_others) {
				error = std::string("Invalid argument: ") + argv[i] + "!";
				return false;
			} else {
				others.push_back(argv[i]);
			}
		} else {
			if (r->second.type == OPTIONAL) {
				r->second.opt = true;
				Map::iterator p_it = arguments.find(r->second.pair);
				p_it->second.opt = true;
			} else if (i + 1 < argc) {
				++i;
				r->second.value = argv[i];
				Map::iterator p_it = arguments.find(r->second.pair);
				p_it->second.value = argv[i];
			} else {
				error = std::string("Non value for: ") + argv[i] + "!";
				return false;
			}
		}
	}
	for (auto i = arguments.begin(); i != arguments.end(); ++i) {
		if (i->second.type == IMPORTANT &&
				i->second.value.empty()) {
			error = std::string("argument ") + i->first + " is important!";
			return false;
		}
	}
	return true;
}

const bool ArgumentParser::getOptionalArgument(const std::string& k) const {
	Map::const_iterator r = arguments.find(k);
	if (arguments.end() != r) {
		return r->second.opt;
	}
	return false;
}

const std::string& ArgumentParser::getArgument(const std::string& k) const {
	Map::const_iterator r = arguments.find(k);
	if (arguments.end() != r) {
		return r->second.value;
	}
	return empty_;
}

const std::string& ArgumentParser::tryGetArgument(const std::string& k,
		const std::string& def) const {
	Map::const_iterator r = arguments.find(k);
	if (arguments.end() != r) 
		return r->second.value;
	return def;
}

const std::string& ArgumentParser::getError() const {
	return error;
}

const std::vector<std::string>& ArgumentParser::getOthers() const {
	return others;
}
