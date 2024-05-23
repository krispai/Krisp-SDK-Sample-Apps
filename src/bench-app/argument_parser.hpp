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

#if defined _WIN32 || defined __CYGWIN__
    #define TEST_UTILS_EXPORT   __declspec( dllexport )
#else
    #if __GNUC__ >= 4
        #define TEST_UTILS_EXPORT __attribute__ ((visibility ("default")))
    #else
        #define TEST_UTILS_EXPORT
    #endif
#endif

namespace KRISP {
    namespace TEST_UTILS {
        class ArgumentParser;
    }
};

enum ArgType {
    ARG_OPTIONAL = 1,
    ARG_IMPORTANT,
    ARG_DEFAULT
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
        TEST_UTILS_EXPORT void addArgument(const Str& l, const Str& s, const ArgType t = ARG_DEFAULT);
        TEST_UTILS_EXPORT const bool parse();
        TEST_UTILS_EXPORT const bool getOptionalArgument(const Str& k) const;
        TEST_UTILS_EXPORT const Str& getArgument(const Str& k) const;
        TEST_UTILS_EXPORT const Str& tryGetArgument(const Str& k, const Str& d) const;
        TEST_UTILS_EXPORT const Str& getError() const;
        TEST_UTILS_EXPORT const OtherType& getOthers() const;
    private:
        ArgumentParser();
    public:
        TEST_UTILS_EXPORT ArgumentParser(int argc, char** argv, bool o=IGNORE_OTHERS);
        TEST_UTILS_EXPORT ~ArgumentParser();
};

TEST_UTILS_EXPORT inline std::wstring convertMBString2WString(const std::string& str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    std::wstring wstr = myconv.from_bytes(str);
    return wstr;
    // std::vector<wchar_t>  wstr;
    // std::vector<char> ssss(str.begin(),str.end());
    // wstr.resize(str.size()+1);
    // size_t size=std::mbstowcs(wstr.data(),str.c_str(),wstr.size());
    // assert(size!=SIZE_MAX);
    // wstr.resize(size+1);
    // return std::wstring(wstr.data());
}

#endif // KRISP_TEST_UTILS_ARGUMENT_PARSER
