/* 
    coded by Jared Bruni (jaredbruni@protonmail.com)
    https://lostsidedead.biz
*/
#ifndef _STR_BUF_X
#define _STR_BUF_X

#include<string>
#include<sstream>
#include<cstdint>
#include<optional>
#include<unordered_map>
#include"types.hpp"


namespace scan {
    namespace buffer {
        template<typename Ch = char, typename String = std::basic_string<Ch, std::char_traits<Ch>>>
        class StringBuffer {
        public:
            using ch_type    = Ch;
            using string_type = String;
            StringBuffer(const string_type &buf, const std::string &filename = "unknown");
            std::optional<ch_type> getch();
            std::optional<ch_type> curch();
            std::optional<ch_type> peekch(int num = 1);
            std::optional<ch_type> prevch();
            std::optional<ch_type> forward_step(int num = 1);
            std::optional<ch_type> backward_step(int num = 1);
            std::pair<uint64_t, uint64_t> cur_line();
            std::string cur_file() const;
            void process_line_directive(uint64_t newLine, const std::string &newFile);
            bool eof(uint64_t pos = 0);
            void reset(uint64_t pos = 0);

        private:
            string_type buffer_;
            uint64_t    index;
            uint64_t    currentLine;
            uint64_t    currentColumn;
            std::string currentFile;
        };

        template<typename Ch, typename String>
        StringBuffer<Ch, String>::StringBuffer(const string_type &buf,
                                                const std::string &filename)
            : buffer_{buf + " \n"}, index{0},
              currentLine{1}, currentColumn{1},
              currentFile{filename}
        {}

        template<typename Ch, typename String>
        std::optional<Ch> StringBuffer<Ch, String>::getch() {
            if (index < buffer_.length()) {
                Ch ch = buffer_[index++];
                if (ch == '\n') {
                    currentLine++;
                    currentColumn = 1;
                } else {
                    currentColumn++;
                }
                return ch;
            }
            return std::nullopt;
        }

        template<typename Ch, typename String>
        std::optional<Ch> StringBuffer<Ch, String>::curch() {
            if (index < buffer_.length())
                return buffer_[index];
            return std::nullopt;
        }

        template<typename Ch, typename String>
        std::optional<Ch> StringBuffer<Ch, String>::peekch(int num) {
            if (index + num < buffer_.length())
                return buffer_[index + num];
            return std::nullopt;
        }

        template<typename Ch, typename String>
        std::optional<Ch> StringBuffer<Ch, String>::prevch() {
            if (index > 0)
                return buffer_[index - 1];
            return std::nullopt;
        }

        template<typename Ch, typename String>
        std::optional<Ch> StringBuffer<Ch, String>::forward_step(int num) {
            if (index + num < buffer_.length()) {
                index += num;
                return buffer_[index];
            }
            return std::nullopt;
        }

        template<typename Ch, typename String>
        std::optional<Ch> StringBuffer<Ch, String>::backward_step(int num) {
            if (static_cast<int64_t>(index) >= num) {
                index -= num;
                if(buffer_[index] == '\n') currentLine --;
                return buffer_[index];
            }
            return std::nullopt;
        }

        template<typename Ch, typename String>
        std::pair<uint64_t, uint64_t> StringBuffer<Ch, String>::cur_line() {
            uint64_t line = (currentColumn == 1 && index > 0) ? currentLine - 1 : currentLine;
            uint64_t col  = (currentColumn > 1 ? currentColumn - 1 : 1);
            return std::make_pair(line, col);
        }

        template<typename Ch, typename String>
        std::string StringBuffer<Ch, String>::cur_file() const {
            return currentFile;
        }

        template<typename Ch, typename String>
        void StringBuffer<Ch, String>::process_line_directive(uint64_t newLine,
                                                                const std::string &newFile) {
            currentLine = newLine;
            currentColumn = 1;
            currentFile = newFile;
        }

        template<typename Ch, typename String>
        bool StringBuffer<Ch, String>::eof(uint64_t pos) {
            return (pos + index >= buffer_.size());
        }

        template<typename Ch, typename String>
        void StringBuffer<Ch, String>::reset(uint64_t pos) {
            index = pos;
            currentLine = 1;
            currentColumn = 1;
        }
    }   
 
    namespace token {
      
        using types::CharType;
   
        template<typename Ch, int MAX_SIZE=256>
        class TokenMap {
        public:
                    
            TokenMap();
            std::optional<types::CharType> lookup_int8(int8_t c);
        private:
            std::unordered_map<Ch, types::CharType> token_map;
        };

        template<typename Ch, int MAX_CHARS>
        TokenMap<Ch, MAX_CHARS>::TokenMap() {
            std::size_t i = 0;
            for(i = 0;  i < MAX_CHARS; ++i) {
                token_map[i] = CharType::TT_NULL;
            }
            for(i = 'a'; i <= 'z'; ++i) {
                token_map[i] = CharType::TT_CHAR;
            }
            for(i = 'A'; i <= 'Z'; ++i) {
                token_map[i] = CharType::TT_CHAR;
            }
            for(i = '0'; i <= '9'; ++i) {
                token_map[i] = CharType::TT_DIGIT;
            }
            token_map[' '] = CharType::TT_SPACE;
            token_map['+'] = CharType::TT_SYMBOL;
            token_map['-'] = CharType::TT_SYMBOL;
            token_map['*'] = CharType::TT_SYMBOL;
            token_map['/'] = CharType::TT_SYMBOL;
            token_map['%'] = CharType::TT_SYMBOL;
            token_map['&'] = CharType::TT_SYMBOL;
            token_map['|'] = CharType::TT_SYMBOL;
            token_map['^'] = CharType::TT_SYMBOL;
            token_map['~'] = CharType::TT_SYMBOL;
            token_map['!'] = CharType::TT_SYMBOL;
            token_map['='] = CharType::TT_SYMBOL;
            token_map['<'] = CharType::TT_SYMBOL;
            token_map['>'] = CharType::TT_SYMBOL;
            token_map['('] = CharType::TT_SYMBOL;
            token_map[')'] = CharType::TT_SYMBOL;
            token_map['['] = CharType::TT_SYMBOL;
            token_map[']'] = CharType::TT_SYMBOL;
            token_map['{'] = CharType::TT_SYMBOL;
            token_map['}'] = CharType::TT_SYMBOL;
            token_map[';'] = CharType::TT_SYMBOL;
            token_map[':'] = CharType::TT_SYMBOL;
            token_map[','] = CharType::TT_SYMBOL;
            token_map['.'] = CharType::TT_SYMBOL;
            token_map['?'] = CharType::TT_SYMBOL;
            token_map['#'] = CharType::TT_SYMBOL;
            token_map['_'] = CharType::TT_CHAR;
            token_map['\\'] = CharType::TT_SYMBOL;
            token_map['\''] = CharType::TT_SINGLE;
            token_map['\"'] = CharType::TT_STRING;  
            token_map['"'] = CharType::TT_STRING;  
            token_map['@'] = CharType::TT_SYMBOL;   
            token_map['$'] = CharType::TT_SYMBOL;
        }
        template<typename Ch, int MAX_CHARS>
        std::optional<types::CharType> TokenMap<Ch, MAX_CHARS>::lookup_int8(int8_t c) {
            if(c >= 0 && c < MAX_CHARS) {
                auto f = token_map.find(c);
                if(f != token_map.end())
                    return f->second;
            }
            return std::nullopt;
        }
        
        template<typename Ch = char, typename String = std::basic_string<Ch, std::char_traits<Ch>>>
        class Token {
        public:
            using ch_type = Ch;
            using string_type = String;
            Token() : line{1}, col{1} {}
            Token(const types::TokenType &t) : type{t}, line{1}, col{1} {}
            Token(const Token<Ch,String> &t) : type{t.type}, value{t.value}, line{t.line}, col{t.col}, filename{t.filename} {}
            Token(Token<Ch,String>  &&t) : type{t.type}, value{std::move(t.value)}, line{t.line}, col{t.col}, filename{t.filename} {}
            Token<Ch,String> &operator=(const types::TokenType &type);
            Token<Ch,String> &operator=(const Token<Ch,String> &t);
            Token<Ch,String> &operator=(Token<Ch,String> &&t);
            void setToken(const types::TokenType &type, const String &value);
            string_type getTokenValue() const { return value; }
            types::TokenType getTokenType() const { return type; }
            void print(std::ostream &out);
            const std::pair<uint64_t, uint64_t> get_pos() const { return std::make_pair(line, col); }
            void set_pos(const std::pair<uint64_t, uint64_t> &p);
            void set_filename(const std::string &filename) { this->filename = filename; }
            std::string get_filename() const { return this->filename; }
            uint64_t getLine() const { return line; }
            uint64_t getCol() const { return col; }
        private:
            types::TokenType type;
            string_type value;
            uint64_t line = 1, col = 1;
            std::string filename;
            
        };
        
        template<typename Ch, typename String>
        void Token<Ch, String>::setToken(const types::TokenType &type, const String &value) {
            this->type = type;
            this->value = value;
        }

        template<typename Ch, typename String>
        Token<Ch,String> &Token<Ch,String>::operator=(const types::TokenType &type) {
            setToken(type, value);
            return *this;
        }
        template<typename Ch, typename String>
        Token<Ch,String> &Token<Ch,String>::operator=(const Token<Ch,String> &t) {
            setToken(t.type, t.value);
            filename = t.filename;
            return *this;
        }
        template<typename Ch, typename String>
        Token<Ch,String> &Token<Ch,String>::operator=(Token<Ch,String> &&t) {
            setToken(t.type, t.value);
            return *this;
        }

        template<typename Ch, typename String>
        void Token<Ch, String>::print(std::ostream &out) {
            out << value << " -> \t";
            types::print_type_TokenType(out, this->type);
            out << "\n"; 
        }
        template<typename Ch, typename String>
        void Token<Ch, String>::set_pos(const std::pair<uint64_t, uint64_t> &p) {
            line = p.first;
            col = p.second;
        }
    }
       
  }

#endif