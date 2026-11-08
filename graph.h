#pragma once
#include<vector>
#include<cstdint>
#include<capstone/capstone.h>
#include"Hashmap.h" //Rs hashmap

struct Basic_Block{ //nodes, BRICK BY BRICK
    uint64_t st_address; //start addresses of the blocks im making
    std::vector<cs_insn> instruction; //the assembly code we got after disasm
    std::vector<uint64_t> edges; //the je instuctions (jump)
};

//the actual glue, connecting the blocks TT)
class CFG{
    public: 
    HashMap<Basic_Block> blocks; //the whole graph will be stored here, search keys will be addresses
    CFG(): blocks(1000){
    }

    void build_from_instruct(const std::vector<cs_insn>& instructions);
    std::vector<std::vector<int>>translator(std::vector<uint64_t>& id_to_addr);
};
