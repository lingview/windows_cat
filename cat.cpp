#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <windows.h>
#include <uchardet/uchardet.h>
#include <iconv.h>
#include <io.h>
#include <fcntl.h>

std::unordered_map<std::string, UINT> encoding_to_codepage = {
    {"UTF-8", CP_UTF8},
    {"GBK", CP_ACP},
    {"GB2312", 936},
    {"BIG5", 950},
    {"Shift_JIS", 932},
    {"EUC-KR", 949},
    {"ISO-8859-1", 28591}
};

std::string detect_encoding(const std::vector<char>& buffer)
{
    uchardet_t ucd = uchardet_new();
    uchardet_handle_data(ucd, buffer.data(), buffer.size());
    uchardet_data_end(ucd);

    const char* detected_charset = uchardet_get_charset(ucd);
    if (detected_charset && *detected_charset != '\0')
    {
        std::string result(detected_charset);
        uchardet_delete(ucd);
        return result;
    }
    else
    {
        uchardet_delete(ucd);
        return "UTF-8";
    }
}

std::string convert_encoding(const std::string& input, const std::string& from_encoding, const std::string& to_encoding)
{
    iconv_t cd = iconv_open(to_encoding.c_str(), from_encoding.c_str());
    if (cd == (iconv_t)-1)
    {
        throw std::runtime_error("初始化iconv失败");
    }

    size_t inbytesleft = input.size();
    char* inbuf = const_cast<char*>(input.c_str());

    std::string output;
    size_t outbuffer_size = input.size() * 4;
    output.resize(outbuffer_size);
    char* outbuf = &output[0];
    size_t outbytesleft = outbuffer_size;

    if (iconv(cd, NULL, &inbytesleft, NULL, &outbytesleft) == (size_t)-1)
    {
        iconv_close(cd);
        throw std::runtime_error("Iconv复位失败");
    }

    if (iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft) == (size_t)-1)
    {
        iconv_close(cd);
        throw std::runtime_error("编码转换失败");
    }

    iconv_close(cd);

    output.resize(outbuffer_size - outbytesleft);
    return output;
}

std::wstring convert_to_wide_string(const std::string& input, UINT from_codepage)
{
    int len = MultiByteToWideChar(from_codepage, 0, input.c_str(), -1, NULL, 0);
    if (len == 0)
        return L"";

    std::wstring wide(len, L'\0');
    MultiByteToWideChar(from_codepage, 0, input.c_str(), -1, &wide[0], len);

    if (!wide.empty())
        wide.pop_back();
    return wide;
}

std::string convert_from_wide_string(const std::wstring& input, UINT to_codepage = CP_UTF8)
{
    int len = WideCharToMultiByte(to_codepage, 0, input.c_str(), -1, NULL, 0, NULL, NULL);
    if (len == 0)
        return "";

    std::string narrow(len, '\0');
    WideCharToMultiByte(to_codepage, 0, input.c_str(), -1, &narrow[0], len, NULL, NULL);

    if (!narrow.empty())
        narrow.pop_back();

    return narrow;
}



void cat_file(const std::string& filename, const std::string& specified_encoding, bool number_lines, bool show_ends, bool show_tabs, bool show_nonprinting, bool squeeze_blank, bool number_nonblank) {
    _setmode(_fileno(stdout), _O_WTEXT);

    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "文件打开失败：" << filename << std::endl;
        return;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (file.read(buffer.data(), size)) {
        std::string detected_encoding;
        if (specified_encoding.empty()) {
            detected_encoding = detect_encoding(buffer);
            /*std::wcout << L"发现编码：" << convert_to_wide_string(detected_encoding, CP_UTF8) << std::endl;*/
        }
        else {
            detected_encoding = specified_encoding;
            std::wcout << L"使用指定的编码：" << convert_to_wide_string(specified_encoding, CP_UTF8) << std::endl;
        }

        std::wstring content;
        if (encoding_to_codepage.find(detected_encoding) != encoding_to_codepage.end()) {
            try {
                content = convert_to_wide_string(std::string(buffer.begin(), buffer.end()), encoding_to_codepage[detected_encoding]);
            }
            catch (const std::exception& e) {
                std::wcerr << L"编码转换错误：" << convert_to_wide_string(e.what(), CP_UTF8) << std::endl;
                return;
            }
        }
        else {
            try {
                content = convert_to_wide_string(convert_encoding(std::string(buffer.begin(), buffer.end()), detected_encoding, "UTF-8"), CP_UTF8);
            }
            catch (const std::exception& e) {
                std::wcerr << L"编码转换错误：" << convert_to_wide_string(e.what(), CP_UTF8) << std::endl;
                return;
            }
        }

        std::wistringstream stream(content);
        std::wstring line;
        unsigned long lineNumber = 0;
        bool previous_line_blank = false;

        while (std::getline(stream, line)) {
            if (squeeze_blank && line.empty() && previous_line_blank) continue;
            previous_line_blank = line.empty();
            std::wstringstream processed_line;
            for (wchar_t ch : line) {
                if (show_tabs && ch == L'\t') {
                    processed_line << L"^I";
                }
                else if (show_nonprinting && !iswprint(ch)) {
                    if (ch == L'\r') processed_line << L"^M";
                    else if (ch == L'\b') processed_line << L"^H";
                    else if (ch >= 1 && ch <= 31) processed_line << L'^' << static_cast<wchar_t>('A' + ch - 1);
                    else processed_line << ch;
                }
                else {
                    processed_line << ch;
                }
            }

            if (show_ends) processed_line << L"$";

            if ((number_lines || number_nonblank) && (!line.empty() || number_lines)) {
                lineNumber++;
                std::wcout << lineNumber << L"\t";
            }

            std::wcout << processed_line.str() << std::endl;
        }
    }
    else {
        std::wcerr << L"读取文件失败：" << convert_to_wide_string(filename, CP_UTF8) << std::endl;
    }
}



void print_help(const char* progname)
{
    std::cout << "用法: " << progname << " [选项] 文件..." << std::endl;
    std::cout << "选项:" << std::endl;
    std::cout << "  --encoding=<编码>      指定文件编码" << std::endl;
    std::cout << "  -n, --number           编号所有输出行" << std::endl;
    std::cout << "  -T                     将制表符显示为^I" << std::endl;
    std::cout << "  -v                     显示非打印字符" << std::endl;
    std::cout << "  -s                     压缩多个相邻的空白" << std::endl;
    std::cout << "  -b                     仅对非空输出行编号" << std::endl;
    std::cout << "  --help                 显示此帮助信息并退出" << std::endl;
    std::cout << "  --version              输出版本信息并退出" << std::endl;
}

void print_version()
{
    std::cout << "版本 1.0" << std::endl;
}

int main(int argc, char* argv[])
{
    //SetConsoleOutputCP(CP_UTF8);

    bool number_lines = false;
    bool show_ends = false;
    bool show_tabs = false;
    bool show_nonprinting = false;
    bool squeeze_blank = false;
    bool number_nonblank = false;
    std::string specified_encoding;
    std::vector<std::string> files;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--help")
        {
            print_help(argv[0]);
            return 0;
        }
        else if (arg == "--version")
        {
            print_version();
            return 0;
        }
        else if (arg.substr(0, 10) == "--encoding=")
        {
            specified_encoding = arg.substr(10);
        }
        else if (arg[0] == '-')
        {
            for (size_t j = 1; j < arg.length(); ++j)
            {
                switch (arg[j])
                {
                case 'n':
                    number_lines = true;
                    break;
                    //case 'E':
                    //    show_ends = true;
                    //    break;
                case 'T':
                    show_tabs = true;
                    break;
                case 'v':
                    show_nonprinting = true;
                    break;
                case 's':
                    squeeze_blank = true;
                    break;
                case 'b':
                    number_nonblank = true;
                    break;
                default:
                    std::cerr << "未知命令： -" << arg[j] << std::endl;
                    print_help(argv[0]);
                    return 1;
                }
            }
        }
        else
        {
            files.push_back(arg);
        }
    }
    if (files.empty())
    {
        std::cerr << "没有提供输入文件：" << std::endl;
        print_help(argv[0]);
        return 1;
    }

    for (const auto& file : files)
    {
        cat_file(file, specified_encoding, number_lines, show_ends, show_tabs, show_nonprinting, squeeze_blank, number_nonblank);
    }

    return 0;
}