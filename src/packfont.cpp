#include <iostream>
#include <vector>
#include <array>
#include <string>
#include <string_view>
#include <optional>
#include <fstream>
#include <sstream>
#include <charconv>
#include <algorithm>
#include <cstdint>
#include <filesystem>

#define STB_RECT_PACK_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_rect_pack.h>
#include <stb_truetype.h>
#include <stb_image_write.h>

#define JLIB_IMPLEMENTATION
#include <jlib/log.h>
#include <jlib/defer.h>
#include <jlib/text_file.h>
#include <jlib/binary_file.h>

using namespace std::string_literals;
void print(auto... args)
{
    log<true, false>(args...);
}
void error(auto... args)
{
    print("ERROR: ", args...);
    throw std::exception();
}

auto find_sysfont()
{
    auto sysfont = std::array{
        "C://Windows//Fonts//"s,
        "/mnt/c/Windows/Fonts/"s,
        "/System/Library/Fonts/Supplemental/"s,
    };
    for (auto &path : sysfont)
    {
        print("checking for system font dir:", path);
        if (std::filesystem::exists(path))
        {
            return path;
        }
    }
    error("could not determine system font path");
    return ""s;
}

auto load_font_data(const std::string &sysfont, const std::string &fname)
{
    print("loading font data");
    auto data = std::vector<uint8_t>{};
    if (!read_binary_file(sysfont + fname + ".ttf", data))
    {
        if (!read_binary_file(sysfont + fname + ".ttc", data))
        {
            error("could not load font ttf or ttc");
        }
    }
    return data;
}

auto get_font_offset(std::vector<uint8_t> &font_data, uint32_t font_index)
{
    auto offset = stbtt_GetFontOffsetForIndex(font_data.data(), font_index);
    if (-1 == offset)
    {
        error("font index", font_index, "is not valid, possibly an issue with the font file");
    }
    return offset;
}

int packfont(int argc, char *argv[])
{
    print("Packfont");
    if (argc < 3)
    {
        print("usage: packfont <fname> <size> <output> [padding] [font_index] [oversampling]");
        return 0;
    }

    auto sysfont = find_sysfont();
    const auto fname = std::string{argv[1]};
    const auto font_size = STBTT_POINT_SIZE((float)atof(argv[2]));
    const auto output = std::string{argv[3]};
    const auto padding = (argc > 4) ? atoi(argv[4]) : 1;
    const auto font_index = (argc > 5) ? atoi(argv[5]) : 0;
    const auto oversample = (argc > 6) ? atoi(argv[6]) : 1;

    print("settings: ");
    print(" - input:     ", fname);
    print(" - size:      ", font_size);
    print(" - output:    ", output);
    print(" - padding:   ", padding);
    print(" - font idx:  ", font_index);
    print(" - sampling:  ", oversample);

    const auto start_char = 32;
    const auto end_char = 128;
    const auto num_chars = end_char - start_char;

    // load font data
    auto font_data = load_font_data(sysfont, fname);

    // check font index
    auto offset = get_font_offset(font_data, font_index);
    auto font_info = stbtt_fontinfo{};
    stbtt_InitFont(&font_info, font_data.data(), offset);

    auto pack_success = false;
    auto pixels = std::vector<uint8_t>{};
    auto char_data = std::array<stbtt_packedchar, num_chars>{};
    auto texture_width = 64;
    auto texture_height = 64;
    while (texture_width < 2048)
    {
        print("attempting to pack ", texture_width, "*", texture_height);
        pixels.clear();
        pixels.resize(texture_width * texture_height);
        auto context = stbtt_pack_context{};
        defer { stbtt_PackEnd(&context); };

        if (0 == stbtt_PackBegin(&context, pixels.data(), texture_width, texture_height, 0, padding, nullptr))
        {
            error("pack-begin failed, unrecoverable");
        }

        stbtt_PackSetOversampling(&context, oversample, oversample);

        if (0 == stbtt_PackFontRange(&context, font_data.data(), font_index, font_size, start_char, num_chars, char_data.data()))
        {
            print("packing failed");
        }
        else
        {
            print("packing succeeded");
            pack_success = true;
            break;
        }

        if (texture_width == texture_height)
        {
            texture_width *= 2;
        }
        else
        {
            texture_height *= 2;
        }
    }

    if (pack_success)
    {
        print("writing png");
        auto pixels32 = std::vector<uint32_t>{};
        // for (auto p: pixels) {
        //     auto p32 = (uint32_t)p;

        //     if (p32) {
        //         p32 = 0xff;
        //     }

        //     pixels32.push_back(
        //         0xff000000|(p32<<16)|(p32<<8)|p32
        //         // 0x00ffffff|(p32<<24)
        //     );
        // }

        for (auto i = 0; i < texture_width; i++)
        {
            for (auto j = 0; j < texture_height; j++)
            {
                // auto &p32 = pixels[i * texture_height + j];
                // if (p32)
                // {
                //     p32 = 1;
                // }
            }
        }
        for (auto i = 0; i < texture_width; i++)
        {
            for (auto j = 0; j < texture_height; j++)
            {
                auto p32 = pixels[i * texture_height + j];
                auto ncount =
                    ((i > 0) ? (pixels[(i - 1) * texture_height + j]) : 0) +
                    ((i < texture_width - 1) ? (pixels[(i + 1) * texture_height + j]) : 0) +
                    ((j > 0) ? (pixels[i * texture_height + j - 1]) : 0) +
                    ((j < texture_height - 1) ? (pixels[i * texture_height + j + 1]) : 0);

                if (p32)
                {
                    pixels32.push_back(0xff000000);
                }
                else
                {
                    pixels32.push_back(0);
                }
            }
        }

        if (0 == stbi_write_png((output + ".png").c_str(), texture_width, texture_height, 4, pixels32.data(), texture_width * 4))
        {
            error("write png failed");
        }

        print("writing text file");
        auto data = std::stringstream{};
        data << "CHAR\tx0\ty0\tx1\ty1\tx0ff\ty0ff\txoff2\tyoff2\txadvance\n";
        data << start_char << ' ' << end_char << '\n';
        for (auto i = 0; i < num_chars; i++)
        {
            auto width = 0;
            auto bearing = 0;
            stbtt_GetCodepointHMetrics(&font_info, start_char + i, &width, &bearing);
            data << start_char + i << '\t'
                 << (int)char_data[i].x0 << '\t'
                 << (int)char_data[i].y0 << '\t'
                 << (int)char_data[i].x1 << '\t'
                 << (int)char_data[i].y1 << '\t'
                 << (int)char_data[i].xoff << '\t'
                 << (int)char_data[i].yoff << '\t'
                 << (int)char_data[i].xoff2 << '\t'
                 << (int)char_data[i].yoff2 << '\t'
                 << (int)char_data[i].xadvance
                 << '\n';
        }

        write_text_file(output + ".spritefont", data.str());
        print("write text file succeeded");
    }
    else
    {
        error("could not pack into texture up to maximum size");
    }

    return 0;
}

int main(int argc, char *argv[])
try
{
    return packfont(argc, argv);
}
catch (...)
{
    print("packfont failed");
    return 1;
}
