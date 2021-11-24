# CSV Parser

Fast, header-only, C++14 CSV parser.

## Custom Usage

Creating 2D vectors from CSV data.
Several methods are provided.

using namespace std;

## Pattern1. From char, wchar_t pointer.
```cpp
const char* data = "\"a\"\t\"あ\tい\"\t\"b\"\t\"c\"\r\n\"e\"\t\"f\"\t\"g\"\r\n";
const wchar_t* wdata = L"\"a\"\t\"あ\tい\"\t\"b\"\t\"c\"\r\n\"e\"\t\"f\"\t\"g\"\r\n";
vector<vector<string>> rows = csv::CsvVec(data, '\t');
vector<vector<wstring>> rows = csv::CsvVec(wdata, L'\t');
```

## Pattern2. From std::string, std::wstring.
```cpp
string data = "\"a\"\t\"あ\tい\"\t\"b\"\t\"c\"\r\n\"e\"\t\"f\"\t\"g\"\r\n";
wstring wdata = L"\"a\"\t\"あ\tい\"\t\"b\"\t\"c\"\r\n\"e\"\t\"f\"\t\"g\"\r\n";
vector<vector<string>> rows = csv::CsvVec(data, '\t');
vector<vector<wstring>> rows = csv::CsvVec(wdata, L'\t');
```

## Pattern3. From Any Stream Object.
```cpp
ifstream fstream("hoge.csv");
vector<vector<string>> rows = csv::CsvVec(fstream, '\t');
wifstream wfstream("hoge.csv");
vector<vector<wstring>> rows = csv::CsvVec(wfstream, L'\t');
```

## Pattern4. From OS File Path.
```cpp
vector<vector<string>> rows = csv::CsvfileVec("hoge.csv", '\t');
vector<vector<wstring>> rows = csv::CsvfileVec("hoge.csv", L'\t');
```

## Pattern5. From Std input.
```cpp
vector<vector<string>> csv::CsvstdinVec<char>('\t');
vector<vector<wstring>> csv::CsvstdinVec<wchar_t>(L'\t');
```

## Below Original ReadMe

---

#### Configuration

You initialize the parser by passing it any input stream of characters. For
example, you can read from a file

```cpp
std::ifstream f("some_file.csv");
CsvParser parser(f);
```

or you can read from `stdin`

```cpp
CsvParser parser(std::cin);
```

Moreover, you can configure the parser by chaining configuration methods like

```cpp
CsvParser parser = CsvParser(std::cin)
  .delimiter(';')    // delimited by ; instead of ,
  .quote('\'')       // quoted fields use ' instead of "
  .terminator('\0'); // terminated by \0 instead of by \r\n, \n, or \r
```

#### Parsing

You can read from the CSV using a range based for loop. Each row of the CSV is
represented as a `std::vector<std::string>`.

```cpp
#include <iostream>
#include "../parser.hpp"

using namespace aria::csv;

int main() {
  std::ifstream f("some_file.csv");
  CsvParser parser(f);

  for (auto& row : parser) {
    for (auto& field : row) {
      std::cout << field << " | ";
    }
    std::cout << std::endl;
  }
}
```

Behind the scenes, when using the range based for, the parser only ever
allocates as much memory as needed to represent a single row of your CSV. If
that's too much, you can step down to a lower level, where you read from the CSV
a field at a time, which only allocates the amount of memory needed for a single
field.

```cpp
#include <iostream>
#include "./parser.hpp"

using namespace aria::csv;

int main() {
  CsvParser parser(std::cin);

  for (;;) {
    auto field = parser.next_field();
    switch (field.type) {
      case FieldType::DATA:
        std::cout << *field.data << " | ";
        break;
      case FieldType::ROW_END:
        std::cout << std::endl;
        break;
      case FieldType::CSV_END:
        std::cout << std::endl;
        return 0;
    }
  }
}
```

It is possible to inspect the current cursor position using `parser.position()`.
This will return the position of the last parsed token. This is useful when
reporting things like progress through a file. You can use
`file.seekg(0, std::ios::end);` to get a file size.
