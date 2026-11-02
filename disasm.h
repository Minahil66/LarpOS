#pragma once
#include<capstone/capstone.h>
#include<vector>
#include<string>
#include<fstream>
#include<libelf.h>
#include<gelf.h>
#include<fcntl.h>
#include<unistd.h>

namespace Disasm{
    std::vector<cs_insn>get_instruct(const std::string& filepath); // path of the file that is to be converted;
}
