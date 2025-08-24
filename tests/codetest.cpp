#include<iostream>
#include<string>
#include<sstream>
#include<cstdlib>



enum class SrcType { PAS, MXVM };

class TestSource {
public:
    explicit TestSource(const std::string &filename_) : filename(filename_) {
        if(caseTest(filename_, ".pas")) {
            type = SrcType::PAS;
        } else if(caseTest(filename_, ".mxvm")) {
            type = SrcType::MXVM;
        } else {
            throw std::runtime_error("Error source must be pas or MXVM.\n");
        }
    }
    
    int run(bool echo);

    static std::string toLower(const std::string &s) {
        std::string text;
        for(size_t i = 0; i < s.length(); ++i) {
            text += tolower(s[i]);
        }
        return text;
    }

    static bool caseTest(const std::string &s1, const std::string &s2) {
        std::string l1 = toLower(s1);
        std::string l2 = toLower(s2);
        if(l1.find(l2) != std::string::npos)
            return true;
        return false;
    }

private:
    std::string filename;
    SrcType type;
};

int TestSource::run(bool echo) {
    std::ostringstream stream;
    if(type == SrcType::PAS) {
        stream << "mxx " << filename << " output.mxvm";
    } else if(type == SrcType::MXVM) {
        stream << "mxvmc --path /usr/local/lib " << filename << " --dry-run";
    }
    std::cout << stream.str() << "\n";
    FILE *fptr = popen(stream.str().c_str(), "r");
    while(!feof(fptr)) {
        char buffer[256];
        if(fgets(buffer, 256, fptr) && echo)
            std::cout << buffer;
    }
    return pclose(fptr);
}


int main(int argc, char **argv) {
    bool echo = false;
    if(argc != 3) {
        std::cerr << "mxvm-test: Error requires two arguments\n";
        return EXIT_FAILURE;
    }

    if(TestSource::caseTest(argv[2], "echo"))
        echo = true;

    TestSource testsrc(argv[1]);
    return testsrc.run(echo);
}