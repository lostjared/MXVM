/**
 * @file html_gen.hpp
 * @brief HTML documentation generator from MXVM source code
 * @author Jared Bruni
 */
#ifndef __HTML__GEN___H_
#define __HTML__GEN___H_

#include "mxvm/instruct.hpp"

namespace mxvm {

    /** @brief Generates syntax-highlighted HTML output from MXVM source code */
    class HTMLGen {
      public:
        /**
         * @brief Construct an HTML generator from MXVM source text
         * @param src MXVM source code string
         */
        HTMLGen(const std::string &src);

        /**
         * @brief Write the generated HTML to an output stream
         * @param out Output stream to write HTML to
         * @param filename Source filename used in the HTML title
         */
        void output(std::ostream &out, std::string filename);

      private:
        std::string source;  ///< the MXVM source text to convert
    };
} // namespace mxvm

#endif