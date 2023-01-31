#ifndef ARGUMENT_PARSER
#define ARGUMENT_PARSER

#define IGNORE_OTHERS true

#include <string>
#include <vector>
#include <map>
#include <cassert>
#include <locale>


enum ArgType {
	OPTIONAL,
	IMPORTANT,
	DEFAULT
};

class ArgValue {
public:
	std::string value;
	std::string pair;
	ArgType type;
	bool opt;
public:
	ArgValue(const std::string& v, const std::string& p, const ArgType t,
		const bool o)
	{ 
		value = v;
		pair = p;
		type = t;
		opt = o;
	}
};

class ArgumentParser {
private:
	typedef std::map<std::string, ArgValue> Map;
	typedef std::vector<std::string> OtherType;
	typedef std::pair<Map::iterator, bool> Ret; 
	Map arguments;
	OtherType others;
	bool ignore_others;
	char** argv;
	int argc;
	std::string error;
	std::string empty_;
public:
	void addArgument(const std::string& l, const std::string& s,
		const ArgType t=DEFAULT);
	const bool parse();
	const bool getOptionalArgument(const std::string& k) const;
	const std::string& getArgument(const std::string& k) const;
	const std::string& tryGetArgument(const std::string& k,
		const std::string& d) const;
	const std::string& getError() const;
	const OtherType& getOthers() const;
private:
	ArgumentParser();
public:
	ArgumentParser(int argc, char** argv, bool o=IGNORE_OTHERS);
	~ArgumentParser();
};

#endif
