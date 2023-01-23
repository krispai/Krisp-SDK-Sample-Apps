#ifndef KRISP_TEST_UTILS_ARGUMENT_PARSER
#define KRISP_TEST_UTILS_ARGUMENT_PARSER

// Defined macros
#define IGNORE_OTHERS true
#define USE_OTHERS false

// Headers from standard library
#include <string>
#include <vector>
#include <map>
#include <cassert>
#include <codecvt>
#include <locale>

namespace KRISP {
    namespace TEST_UTILS {
        class ArgumentParser;
    }
};

enum ArgType {
    OPTIONAL,
    IMPORTANT,
    DEFAULT
};

class ArgValue
{
    private:
        typedef std::string Str;
    public:
        Str     value;
        Str     pair;
        ArgType type;
        bool    opt;
    public:
        ArgValue(const Str& v, const Str& p, const ArgType t, const bool o)
        { 
            value = v;
            pair = p;
            type = t;
            opt = o;
        }
};

class KRISP::TEST_UTILS::
ArgumentParser
{
    private:
        typedef std::string Str;
        typedef std::map<Str, ArgValue> Map;
        typedef std::vector<Str> OtherType;
        typedef std::pair<Map::iterator, bool> Ret; 
        Map arguments;
        OtherType others;
        bool ignore_others;
        char** argv;
        int argc;
        Str error;
		Str empty_;
    public:
        void addArgument(const Str& l, const Str& s, const ArgType t=DEFAULT);
        const bool parse();
        const bool getOptionalArgument(const Str& k) const;
        const Str& getArgument(const Str& k) const;
        const Str& tryGetArgument(const Str& k, const Str& d) const;
        const Str& getError() const;
        const OtherType& getOthers() const;
    private:
        ArgumentParser();
    public:
        ArgumentParser(int argc, char** argv, bool o=IGNORE_OTHERS);
        ~ArgumentParser();
};

inline std::wstring convertMBString2WString(const std::string& str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    std::wstring wstr = myconv.from_bytes(str);
    return wstr;
}

#endif
