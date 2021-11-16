/* csv-parser | MIT Liscence | https://github.com/kirin123kirin/csv-parser/blob/master/LICENSE
 * Changes.
 *  2021.11.15  Change to class template, and Japanize Custom kirin123kirin
 *  csv-parser | MIT Liscence | https://github.com/AriaFallah/csv-parser/blob/master/LICENSE
 */
#ifndef PARSER_H
#define PARSER_H

#include <iostream>
#include <istream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace csv {
template <typename CharT>
struct choose_literal;  // not defined

template <>
struct choose_literal<char> {
    static constexpr const char choose(const char s, const wchar_t) { return s; }
};

template <>
struct choose_literal<wchar_t> {
    static constexpr const wchar_t choose(const char, const wchar_t s) { return s; }
};

template <>
struct choose_literal<char*> {
    static constexpr const char* choose(const char* s, const wchar_t*) { return s; }
};

template <>
struct choose_literal<wchar_t*> {
    static constexpr const wchar_t* choose(const char*, const wchar_t* s) { return s; }
};

#define TYPED_LITERAL(CharT, Literal) (choose_literal<CharT>::choose(Literal, L##Literal))

enum class Term { CRLF = -2 };
enum class FieldType { DATA, ROW_END, CSV_END };

// Checking for '\n', '\r', and '\r\n' by default
inline bool operator==(const char c, const Term t) {
    switch(t) {
        case Term::CRLF:
            return c == '\r' || c == '\n';
        default:
            return static_cast<char>(t) == c;
    }
}

inline bool operator==(const wchar_t c, const Term t) {
    switch(t) {
        case Term::CRLF:
            return c == L'\r' || c == L'\n';
        default:
            return static_cast<char>(t) == c;
    }
}

inline bool operator!=(const char c, const Term t) {
    return !(c == t);
}
inline bool operator!=(const wchar_t c, const Term t) {
    return !(c == t);
}

// Wraps returned fields so we can also indicate
// that we hit row endings or the end of the csv itself
template <typename CharT>
struct Field {
    explicit Field(FieldType t) : type(t), data(nullptr) {}
    explicit Field(const std::basic_string<CharT>& str) : type(FieldType::DATA), data(&str) {}

    FieldType type;
    const std::basic_string<CharT>* data;
};

// Reads and parses lines from a csv file
template <typename CharT>
class CsvParser {
   public:
    using Stream = std::basic_istream<CharT>;
    using ROWS = std::vector<std::vector<std::basic_string<CharT>>>;

   private:
    // CSV state for state machine
    enum class State { START_OF_FIELD, IN_FIELD, IN_QUOTED_FIELD, IN_ESCAPED_QUOTE, END_OF_ROW, EMPTY };
    State m_state = State::START_OF_FIELD;

    // Configurable attributes
    CharT m_quote = TYPED_LITERAL(CharT, '"');
    CharT m_delimiter = TYPED_LITERAL(CharT, ',');
    Term m_terminator = Term::CRLF;
    Stream& m_input;

    // Buffer capacities
    static constexpr int FIELDBUF_CAP = 1024;
    static constexpr int INPUTBUF_CAP = 1024 * 128;

    // Buffers
    std::basic_string<CharT> m_fieldbuf;
    std::unique_ptr<CharT[]> m_inputbuf = std::unique_ptr<CharT[]>(new CharT[INPUTBUF_CAP]{});

    // Misc
    bool m_eof = false;
    size_t m_cursor = INPUTBUF_CAP;
    size_t m_inputbuf_size = INPUTBUF_CAP;
    std::streamoff m_scanposition = -INPUTBUF_CAP;

   public:
    // Creates the CSV parser which by default, splits on commas,
    // uses quotes to escape, and handles CSV files that end in either
    // '\r', '\n', or '\r\n'.
    CsvParser(Stream& input) : m_input(input) {
        // Reserve space upfront to improve performance
        m_fieldbuf.reserve(FIELDBUF_CAP);
        if(!m_input.good()) {
            throw std::runtime_error("Something is wrong with input stream");
        }
    }

    /* move constructor */
    // CsvParser(CsvParser<Stream>&& other) noexcept : m_input({}) {}
    // CsvParser<Stream>& operator=(CsvParser<Stream>&& other) {
    //     if(this == &other)
    //         return *this;
    //     this->m_state = other.m_state;

    //     this->m_quote = other.m_quote;
    //     this->m_delimiter = other.m_delimiter;
    //     this->m_terminator = other.m_terminator;
    //     if(this->m_input.is_open())
    //         this->m_input.close();
    //     this->m_input = std::move(other.m_input);

    //     other.m_fieldbuf.swap(this->m_fieldbuf);
    //     other.m_inputbuf.swap(this->m_inputbuf);

    //     this->m_eof = other.m_eof;
    //     this->m_cursor = other.m_cursor;
    //     this->m_inputbuf_size = other.m_inputbuf_size;
    //     this->m_scanposition = other.m_scanposition;

    //     return *this;
    // }

    // Change the quote character
    CsvParser&& quote(CharT c) noexcept {
        m_quote = c;
        return std::move(*this);
    }

    // Change the delimiter character
    CsvParser&& delimiter(CharT c) noexcept {
        m_delimiter = c;
        return std::move(*this);
    }

    // Change the terminator character
    CsvParser&& terminator(CharT c) noexcept {
        m_terminator = static_cast<Term>(c);
        return std::move(*this);
    }

    // The parser is in the empty state when there are
    // no more tokens left to read from the input buffer
    bool empty() { return m_state == State::EMPTY; }

    // Not the actual position in the stream (its buffered) just the
    // position up to last availiable token
    std::streamoff position() const { return m_scanposition + static_cast<std::streamoff>(m_cursor); }

    // Reads a single field from the CSV
    Field<CharT> next_field() {
        if(empty()) {
            return Field<CharT>(FieldType::CSV_END);
        }
        m_fieldbuf.clear();

        // This loop runs until either the parser has
        // read a full field or until there's no tokens left to read
        for(;;) {
            CharT* maybe_token = top_token();

            // If we're out of tokens to read return whatever's left in the
            // field and row buffers. If there's nothing left, return null.
            if(!maybe_token) {
                m_state = State::EMPTY;
                return !m_fieldbuf.empty() ? Field<CharT>(m_fieldbuf) : Field<CharT>(FieldType::CSV_END);
            }

            // Parsing the CSV is done using a finite state machine
            CharT c = *maybe_token;
            switch(m_state) {
                case State::START_OF_FIELD:
                    m_cursor++;
                    if(c == m_terminator) {
                        handle_crlf(c);
                        m_state = State::END_OF_ROW;
                        return Field<CharT>(m_fieldbuf);
                    }

                    if(c == m_quote) {
                        m_state = State::IN_QUOTED_FIELD;
                    } else if(c == m_delimiter) {
                        return Field<CharT>(m_fieldbuf);
                    } else {
                        m_state = State::IN_FIELD;
                        m_fieldbuf += c;
                    }

                    break;

                case State::IN_FIELD:
                    m_cursor++;
                    if(c == m_terminator) {
                        handle_crlf(c);
                        m_state = State::END_OF_ROW;
                        return Field<CharT>(m_fieldbuf);
                    }

                    if(c == m_delimiter) {
                        m_state = State::START_OF_FIELD;
                        return Field<CharT>(m_fieldbuf);
                    } else {
                        m_fieldbuf += c;
                    }

                    break;

                case State::IN_QUOTED_FIELD:
                    m_cursor++;
                    if(c == m_quote) {
                        m_state = State::IN_ESCAPED_QUOTE;
                    } else {
                        m_fieldbuf += c;
                    }

                    break;

                case State::IN_ESCAPED_QUOTE:
                    m_cursor++;
                    if(c == m_terminator) {
                        handle_crlf(c);
                        m_state = State::END_OF_ROW;
                        return Field<CharT>(m_fieldbuf);
                    }

                    if(c == m_quote) {
                        m_state = State::IN_QUOTED_FIELD;
                        m_fieldbuf += c;
                    } else if(c == m_delimiter) {
                        m_state = State::START_OF_FIELD;
                        return Field<CharT>(m_fieldbuf);
                    } else {
                        m_state = State::IN_FIELD;
                        m_fieldbuf += c;
                    }

                    break;

                case State::END_OF_ROW:
                    m_state = State::START_OF_FIELD;
                    return Field<CharT>(FieldType::ROW_END);

                case State::EMPTY:
                    throw std::logic_error("You goofed");
            }
        }
    }

   private:
    // When the parser hits the end of a line it needs
    // to check the special case of '\r\n' as a terminator.
    // If it finds that the previous token was a '\r', and
    // the next token will be a '\n', it skips the '\n'.
    void handle_crlf(const CharT c) {
        if(m_terminator != Term::CRLF || c != TYPED_LITERAL(CharT, '\r')) {
            return;
        }

        CharT* token = top_token();
        if(token && *token == TYPED_LITERAL(CharT, '\n')) {
            m_cursor++;
        }
    }

    // Pulls the next token from the input buffer, but does not move
    // the cursor forward. If the stream is empty and the input buffer
    // is also empty return a nullptr.
    CharT* top_token() {
        // Return null if there's nothing left to read
        if(m_eof && m_cursor == m_inputbuf_size) {
            return nullptr;
        }

        // Refill the input buffer if it's been fully read
        if(m_cursor == m_inputbuf_size) {
            m_scanposition += static_cast<std::streamoff>(m_cursor);
            m_cursor = 0;
            m_input.read(m_inputbuf.get(), INPUTBUF_CAP);

            // Indicate we hit end of file, and resize
            // input buffer to show that it's not at full capacity
            if(m_input.eof()) {
                m_eof = true;
                m_inputbuf_size = (size_t)m_input.gcount();

                // Return null if there's nothing left to read
                if(m_inputbuf_size == 0) {
                    return nullptr;
                }
            }
        }

        return &m_inputbuf[m_cursor];
    }

   public:
    // Iterator implementation for the CSV parser, which reads
    // from the CSV row by row in the form of a vector of strings
    class iterator {
       public:
        using difference_type = std::ptrdiff_t;
        using value_type = std::vector<std::basic_string<CharT>>;
        using pointer = const std::vector<std::basic_string<CharT>>*;
        using reference = const std::vector<std::basic_string<CharT>>&;
        using iterator_category = std::input_iterator_tag;

        explicit iterator(CsvParser* p, bool end = false) : m_parser(p) {
            if(!end) {
                m_row.reserve(50);
                m_current_row = 0;
                next();
            }
        }

        iterator& operator++() {
            next();
            return *this;
        }

        iterator operator++(int) {
            iterator i = (*this);
            ++(*this);
            return i;
        }

        bool operator==(const iterator& other) const {
            return m_current_row == other.m_current_row && m_row.size() == other.m_row.size();
        }

        bool operator!=(const iterator& other) const { return !(*this == other); }

        reference operator*() const { return m_row; }

        pointer operator->() const { return &m_row; }

       private:
        value_type m_row{};
        CsvParser* m_parser;
        int m_current_row = -1;

        void next() {
            value_type::size_type num_fields = 0;
            for(;;) {
                auto field = m_parser->next_field();
                switch(field.type) {
                    case FieldType::CSV_END:
                        if(num_fields < m_row.size()) {
                            m_row.resize(num_fields);
                        }
                        m_current_row = -1;
                        return;
                    case FieldType::ROW_END:
                        if(num_fields < m_row.size()) {
                            m_row.resize(num_fields);
                        }
                        m_current_row++;
                        return;
                    case FieldType::DATA:
                        if(num_fields < m_row.size()) {
                            m_row[num_fields] = std::move(*field.data);
                        } else {
                            m_row.push_back(std::move(*field.data));
                        }
                        num_fields++;
                }
            }
        }
    };

    iterator begin() { return iterator(this); };
    iterator end() { return iterator(this, true); };
};

// template <typename Stream, typename CharT = typename Stream::_Mysb::char_type>
template <typename CharT>
std::vector<std::vector<std::basic_string<CharT>>> CsvVec(std::basic_istream<CharT>& stream,
                                                          CharT delimiter = TYPED_LITERAL(CharT, ','),
                                                          CharT quote = TYPED_LITERAL(CharT, '"')) {
    csv::CsvParser<CharT> parser(stream);
    parser.delimiter(delimiter);
    parser.quote(quote);

    std::vector<std::vector<std::basic_string<CharT>>> ret{};
    for(auto&& row : parser) {
        ret.emplace_back(row);
    }
    return ret;
}

template <typename CharT>
std::vector<std::vector<std::basic_string<CharT>>> CsvVec(const CharT* buf,
                                                          CharT delimiter = TYPED_LITERAL(CharT, ','),
                                                          CharT quote = TYPED_LITERAL(CharT, '"')) {
    std::basic_istringstream<CharT> stream(buf);
    return CsvVec<CharT>(stream, delimiter, quote);
}

template <typename CharT>
std::vector<std::vector<std::basic_string<CharT>>> CsvVec(const std::basic_string<CharT>& str,
                                                          CharT delimiter = TYPED_LITERAL(CharT, ','),
                                                          CharT quote = TYPED_LITERAL(CharT, '"')) {
    std::basic_istringstream<CharT> stream(str);
    return CsvVec<CharT>(stream, delimiter, quote);
}

std::vector<std::vector<std::string>> CsvstdinVec(char delimiter = ',',
                                                  char quote = '"'
#if _WIN32
                                                  char* codepage = "Japanese_Japan.932") {
#else
                                                  char* codepage = "Japanese_Japan.65001") {
#endif
    return CsvVec<char>(std::cin, delimiter, quote);
}

std::vector<std::vector<std::wstring>> CsvstdinVec(wchar_t delimiter = L',',
                                                   wchar_t quote = L'"',
#if _WIN32
                                                   char* codepage = "Japanese_Japan.932") {
#else
                                                   char* codepage = "Japanese_Japan.65001") {
#endif
    std::ios_base::sync_with_stdio(false);
    std::locale default_loc(codepage);
    std::locale::global(default_loc);
    std::locale ctype_default(std::locale::classic(), default_loc, std::locale::ctype);
    std::wcout.imbue(ctype_default);
    std::wcin.imbue(ctype_default);

    return CsvVec<wchar_t>(std::wcin, delimiter, quote);
}

template <typename CharT, typename U>
std::vector<std::vector<std::basic_string<CharT>>> CsvfileVec(U filename,
                                                              CharT delimiter = TYPED_LITERAL(CharT, ','),
                                                              CharT quote = TYPED_LITERAL(CharT, '"'),
                                                              char* codepage = "Japanese_Japan.65001") {
    std::ios_base::sync_with_stdio(false);
    std::locale default_loc(codepage);
    std::locale::global(default_loc);
    std::locale ctype_default(std::locale::classic(), default_loc, std::locale::ctype);

    std::basic_ifstream<CharT> stream(filename);
    if(!stream.good())
        throw std::runtime_error("File Not Found.");
    return CsvVec<CharT>(stream, delimiter, quote);
}

}  // namespace csv
#endif
