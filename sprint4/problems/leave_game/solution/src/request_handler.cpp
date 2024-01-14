#include "request_handler.h"

#include <string>
#include <string_view>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace http_handler {

  
std::string UrlDecode(std::string_view sv)
{
    std::string value(sv);
    std::ostringstream oss;
    
    for (std::size_t i = 0; i < value.size(); ++i)
    {
        auto ch = value[i];
        if (ch == '+')
        {
            oss << " "s;
        }
        else if (ch == '%' && (i + 2) < value.size())
        {
            auto hex = value.substr(i + 1, 2);
            auto dec = static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
            oss << dec;
            i += 2;
        }
        else
        {
            oss << ch;
        }
    }
    
    return oss.str();
}

// Возвращает true, если каталог p содержится внутри base_path.
bool IsSubPath(fs::path path, fs::path base) {
    // Приводим оба пути к каноничному виду (без . и ..)
    path = fs::weakly_canonical(path);
    base = fs::weakly_canonical(base);

    // Проверяем, что все компоненты base содержатся внутри path
    for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
        if (p == path.end() || *p != *b) {
            return false;
        }
    }
    return true;
}

bool PathIsValid(const std::filesystem::path& path) {
    return std::filesystem::exists(path) && !std::filesystem::is_directory(path);
}


std::string_view GetFileExtension(std::string_view path) {
    auto pos = path.rfind('.');
    if (pos == std::string_view::npos) {
        return {};
    }
    return path.substr(pos);
}

// Создаёт StringResponse с заданными параметрами
StringResponse MakeStringResponse(http::status status, std::string_view body, unsigned http_version,
                                  bool keep_alive,
                                  std::string_view content_type) {
    StringResponse response(status, http_version);
    response.set(http::field::content_type, content_type);
    response.body() = body;
    response.content_length(body.size());
    response.keep_alive(keep_alive);
    return response;
}

FileResponse MakeFileResponse(http::status status, http::file_body::value_type& body, unsigned http_version,
                                  bool keep_alive,
                                  std::string_view content_type) {
    FileResponse response(status, http_version);
    response.body() = std::move(body);
    response.set(http::field::content_type, content_type);   
    response.keep_alive(keep_alive);
    response.prepare_payload();
    return response;
}

bool IsApiEndpoint(std::string_view target) {
    return target.substr(0, 5) == "/api/"sv;
}

void str_to_lower(std::string& str) {
    std::transform(str.begin(), str.end(), str.begin(),
        [](unsigned char c){ return std::tolower(c); });
}



std::string_view GetContentType(std::string extension) {    
    if (extension == ".html"s || extension == ".htm"s) {
        return ContentType::TEXT_HTML;
    } else if (extension == ".css"s) {
        return ContentType::CSS;
    } else if (extension == ".txt"s) {
        return ContentType::TXT;
    } else if (extension == ".js"s) {
        return ContentType::JS;
    } else if (extension == ".json"s) {
        return ContentType::JSON;
    } else if (extension == ".xml"s) {
        return ContentType::XML;
    } else if (extension == ".png"s) {
        return ContentType::PNG;
    } else if (extension == ".jpg"s || extension == ".jpe"s || extension == ".jpeg"s) {
        return ContentType::JPG_JPE_JPEG;
    } else if (extension == ".gif"s) {
        return ContentType::GIF;
    } else if (extension == ".bmp"s) {
        return ContentType::BMP;
    } else if (extension == ".ico"s) {
        return ContentType::ICO;
    } else if (extension == ".tiff"s || extension == ".tif"s) {
        return ContentType::TIFF_TIF;
    } else if (extension == ".svg"s || extension == ".svgz"s) {
        return ContentType::SVG_SVGZ;
    } else if (extension == ".mp3"s) {
        return ContentType::MP3;
    } else {
        return ContentType::EMPTY_UNKNOWN;
    }
}

}  // namespace http_handler
