#include <emscripten/bind.h>
#include <string>
#include<mxvm/mxvm.hpp>
#include<scanner/exception.hpp>



void collectAndRegisterAllExterns(std::unique_ptr<mxvm::Program>& program) {
    for (const auto& obj : program->objects) {
        for(auto &lbl : obj->labels) {
            if(lbl.second.second)
                mxvm::Program::base->add_extern(obj->name, lbl.first, false);
        }
    }
}


class MXVM_Debug {
public:
    MXVM_Debug () {}

    std::string parseToHTML(const std::string& code) {
        std::ostringstream html;
        std::ostringstream html_err;
        std::streambuf* old_cerr = std::cerr.rdbuf();
        std::cerr.rdbuf(html_err.rdbuf());

        try {
            mxvm::Parser parser(code);
            parser.include_path = "include";
            parser.scan();
            std::unique_ptr<mxvm::Program> program(new mxvm::Program());
            mxvm::html_mode = true;
            if(parser.generateProgramCode(mxvm::Mode::MODE_COMPILE, program)) {
                std::ostringstream code_v;
                program->generateCode(program->object, code_v);
                program->assembly_code = code_v.str();
                collectAndRegisterAllExterns(program);
                if(program->root_name == program->name)
                    program->object = false;
                else
                    program->object = true;
                program->assembly_code = code_v.str();
                parser.generateDebugHTML(html, program);
            } else {
                html << "Parse Error";
            }
        } catch(mx::Exception &e) {
            html << "Error: " << e.what() << "<br>\n";
        } catch(std::exception  &e) {
            html << "Exception: " << e.what() << "<br>\n";
        } catch(scan::ScanExcept &e) {
            html << e.why() << "\n";
        }
        catch(...) {
            html << "Unknown Error: <br>";
        } 
        std::cerr.rdbuf(old_cerr);

        if(!html_err.str().empty()) {
            if(!html_err.str().empty()) {
                return R"(
                    <div class="section">
                        <div class="section-header" style="background:#b71c1c;color:#fff;padding:20px 30px;font-size:1.4rem;font-weight:600;">
                            <span style="font-size:1.5em;">❌</span> Error
                        </div>
                        <div class="section-content" style="padding:30px;">
                            <div class="error" style="background:#2c1810;border:1px solid #d32f2f;color:#ff8a80;padding:20px;margin:20px;border-radius:8px;font-family:'Courier New',monospace;">
                                )" + html_err.str() + R"(
                            </div>
                        </div>
                    </div>
                )";
            }
        }

        return html.str();
    }
};

EMSCRIPTEN_BINDINGS(MXVM_Debug_bindings) {
    emscripten::class_<MXVM_Debug>("MXVM_Debug")
        .constructor<>()
        .function("parseToHTML", &MXVM_Debug::parseToHTML); 
}