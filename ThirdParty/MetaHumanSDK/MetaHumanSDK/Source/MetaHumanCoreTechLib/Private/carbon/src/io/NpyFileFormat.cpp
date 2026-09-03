// Copyright Epic Games, Inc. All Rights Reserved.

#include <carbon/io/NpyFileFormat.h>
#include <carbon/io/Utils.h>

#include <fstream>

CARBON_NAMESPACE_BEGIN(CARBON_NPY_NAMESPACE)

template <> std::string NpyTypeName<int>() { return "<i4"; }
template <> std::string NpyTypeName<float>() { return "<f4"; }
template <> std::string NpyTypeName<double>() { return "<f8"; }
template <> std::string NpyTypeName<uint8_t>() { return "<u1"; }
template <> std::string NpyTypeName<uint16_t>() { return "<u2"; }

int NPYHeader::DataTypeSize() const
{
    if (m_dataType == "<i4") // int
    {
        return 4;
    }
    else if (m_dataType == "<f4") // float
    {
        return 4;
    }
    else if (m_dataType == "<f8") // double
    {
        return 8;
    }
    else if (m_dataType == "<u1" || m_dataType == "|b1" || m_dataType == "|u1") // uint8_t
    {
        return 1;
    }
    else if (m_dataType == "<u2") // uint16_t
    {
        return 2;
    }
    else { CARBON_CRITICAL("Unsupported dtype"); }
}

inline const char* magicString = "\x93NUMPY";
inline const char* boolString[2] = { "False", "True" };

inline std::vector<char> readStringInQuotes(const std::vector<char>& htxt, int& cnt, int headerLen)
{
    std::vector<char> result;
    const char openingQuote = htxt[cnt++];
    while (cnt < headerLen && htxt[cnt] != openingQuote)
    {
        result.push_back(htxt[cnt++]);
    }
    result.push_back(0);
    cnt++;
    return result;
}

void SaveNPYRaw(std::ostream& out, const NPYHeader& header, const uint8_t* data, size_t dataSize)
{
    std::string htxt = std::string(magicString) + "    {\'descr\': \'" + header.m_dataType + "\', 'fortran_order': " + boolString[header.m_fortranOrder] +
        ", \'shape\': (";
    for (size_t i = 0; i < header.m_shape.size(); i++)
    {
        htxt += std::to_string(header.m_shape[i]);
        if (i != header.m_shape.size() - 1) { htxt += ", "; }
    }
    htxt += "), }";
    const size_t htxtLen = htxt.size();
    size_t hbufLen = (htxtLen + 63) & (~63);
    htxt[6] = '\x01';
    htxt[7] = '\x00';
    htxt[8] = (hbufLen - 10) & 255; // the -10 is for the 10 bytes of preHeader
    htxt[9] = ((hbufLen - 10) >> 8) & 255;

    std::string hbuf;
    hbuf.insert(0, hbufLen, ' ');
    hbuf.replace(0, htxtLen, htxt);

    out.write(&hbuf[0], hbufLen);
    out.write((const char*)data, dataSize);
}

void LoadNPYRawHeader(const std::vector<char>& htxt, NPYHeader& header)
{
    header = {};

    const int headerLen = (int)htxt.size();

    int cnt = 1;
    unsigned char checks = 0;
    for (;;)
    {
        if (cnt >= headerLen) { break; }
        if (htxt[cnt] == '}') { break; }
        if ((htxt[cnt] == ' ') || (htxt[cnt] == ','))
        {
            cnt++;
            continue;
        }
        if ((htxt[cnt] == '\'') || (htxt[cnt] == '\"') || (htxt[cnt] == '`'))
        {
            const std::vector<char> str = readStringInQuotes(htxt, cnt, headerLen);

            while (cnt < headerLen && (htxt[cnt] == ':' || htxt[cnt] == ' '))
            {
                cnt++;
            }
            if (std::strcmp(str.data(), "descr") == 0)
            {
                const std::vector<char> str2 = readStringInQuotes(htxt, cnt, headerLen);
                header.m_dataType = std::string(&str2[0]);
                checks |= 1;
            }
            else if (std::strcmp(str.data(), "fortran_order") == 0)
            {
                std::vector<char> str3;
                while (cnt < headerLen && htxt[cnt] != ' ' && htxt[cnt] != ',')
                {
                    str3.push_back(htxt[cnt++]);
                }
                str3.push_back(0);
                cnt++;
                if (std::strcmp(str3.data(), "True") == 0)
                {
                    header.m_fortranOrder = true;
                    checks |= 2;
                }
                else if (std::strcmp(str3.data(), "False") == 0)
                {
                    header.m_fortranOrder = false;
                    checks |= 2;
                }
                else { CARBON_CRITICAL("Unrecognized value for fortran_order"); }
            }
            else if (std::strcmp(str.data(), "shape") == 0)
            {
                if ((cnt >= headerLen) || (htxt[cnt++] != '(')) { CARBON_CRITICAL("Header parsing error"); }
                bool numberStarted = false;
                int number = 0;
                for (;;)
                {
                    if (cnt >= headerLen) { break; }
                    if (!numberStarted)
                    {
                        if (std::isdigit(static_cast<unsigned char>(htxt[cnt])))
                        {
                            numberStarted = true;
                            number = htxt[cnt] - '0';
                            cnt++;
                        }
                        else if (htxt[cnt] == ')') { break; }
                        else if (htxt[cnt] == ' ') { cnt++; }
                        else { CARBON_CRITICAL("Header parsing error"); }
                    }
                    else
                    {
                        if (std::isdigit(static_cast<unsigned char>(htxt[cnt])))
                        {
                            number = 10 * number + (htxt[cnt] - '0');
                            cnt++;
                        }
                        else if ((htxt[cnt] == ',') || (htxt[cnt] == ' ') || (htxt[cnt] == ')'))
                        {
                            header.m_shape.push_back(number);
                            numberStarted = false;
                            checks |= 4;
                            if (htxt[cnt++] == ')') { break; }
                            if (cnt < headerLen && htxt[cnt] == ')') { cnt++; break; }
                        }
                        else if (htxt[cnt] == 'L')
                        {
                            cnt++;
                        }
                        else { CARBON_CRITICAL("Header parsing error"); }
                    }
                }
            }
            else { CARBON_CRITICAL("Unrecognized key in header"); }
        }
        else { CARBON_CRITICAL("Header parsing error"); }
    }
    if (checks != 7) { CARBON_CRITICAL("Header parsing error"); }
}

void LoadNPYRawHeader(std::istream& in, NPYHeader& header)
{
    header = {};

    char preHeader[10];
    in.read(preHeader, 10);
    if (!in) { CARBON_CRITICAL("Failed to read NumPy header"); }
    preHeader[9] = '\0';
    if (std::strncmp(preHeader, magicString, 6) != 0) { CARBON_CRITICAL("Not a NumPy file"); }
    if ((preHeader[6] != '\x01') || (preHeader[7] != '\x00')) { CARBON_CRITICAL("Unsupported NPY version"); }

    int headerLen = (unsigned char)preHeader[8] + ((unsigned char)preHeader[9] << 8);
    if ((headerLen + 10) % 64 != 0)
    {
        // old numpy version save using an unaligned header
        // LOG_WARNING("Unaligned NPY header: {}", headerLen);
    }

    std::vector<char> htxt(headerLen);
    in.read(htxt.data(), headerLen);
    if (!in) { CARBON_CRITICAL("Failed loading header"); }

    LoadNPYRawHeader(htxt, header);

}

void LoadNPYRawData(std::istream& in, const NPYHeader& header, std::vector<uint8_t>& data)
{
    size_t scalarsCount = 1;
    for (auto x : header.m_shape)
    {
        scalarsCount *= x;
    }

    data.resize(scalarsCount * header.DataTypeSize());
    in.read((char*)data.data(), header.DataTypeSize() * scalarsCount);
    if (!in) { CARBON_CRITICAL("Data read error"); }
}

void LoadNPYRaw(std::istream& in, NPYHeader& header, std::vector<uint8_t>& data)
{
    LoadNPYRawHeader(in, header);
    LoadNPYRawData(in, header, data);
}

void LoadNPYRawHeader(FILE* pFile, NPYHeader& header)
{
    char preHeader[10];
    if (fread(preHeader, 1, 10, pFile) != 10)
    {
        CARBON_CRITICAL("Failed to read NumPy preheader");
    }
    preHeader[9] = '\0';
    if (std::strncmp(preHeader, magicString, 6) != 0) { CARBON_CRITICAL("Not a NumPy file"); }
    if ((preHeader[6] != '\x01') || (preHeader[7] != '\x00')) { CARBON_CRITICAL("Unsupported NPY version"); }

    int headerLen = (unsigned char)preHeader[8] + ((unsigned char)preHeader[9] << 8);
    if ((headerLen + 10) % 64 != 0)
    {
        // old numpy version save using an unaligned header
        // LOG_WARNING("Unaligned NPY header: {}", headerLen);
    }

    std::vector<char> htxt(headerLen);
    if (fread(htxt.data(), 1, headerLen, pFile) != (size_t)headerLen)
    {
        CARBON_CRITICAL("Failed to read NumPy header");
    }

    LoadNPYRawHeader(htxt, header);
}

void LoadNPYRawData(FILE* pFile, const NPYHeader& header, std::vector<uint8_t>& data)
{
    size_t scalarsCount = 1;
    for (auto x : header.m_shape)
    {
        scalarsCount *= x;
    }

    data.resize(scalarsCount * header.DataTypeSize());
    if (fread(data.data(), 1, data.size(), pFile) != data.size()) {
        CARBON_CRITICAL("failed to read data");
    }
}

CARBON_NAMESPACE_END(CARBON_NPY_NAMESPACE)
