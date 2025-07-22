#pragma once

#include "XCAFDocProcessor.hxx"

class XCAFIGESProcessor : public XCAFDocProcessor
{
public:
    aiScene* processFile(const std::string& path) override;

private:
    aiScene* processIGESFile(const std::string& path);
};