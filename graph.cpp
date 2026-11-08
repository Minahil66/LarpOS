#include "graph.h"
#include<vector>
#include<unordered_map>


void CFG::build_from_instruct(const std::vector<cs_insn>& instructions){
    if(instructions.empty()) return;

    //the first block
    Basic_Block CB;
    CB.st_address=instructions[0].address; //address predefined in capstone

    for(size_t i=0;i<instructions.size(); i++){
        CB.instruction.push_back(instructions[i]); //instruction and instuctions are TWO DISTINCT THINGS
        std::string mnemonic = instructions[i].mnemonic; //mnemonics is predefined char array from capstone
        
        if (!mnemonic.empty() && mnemonic == "jmp" || mnemonic == "ret" || mnemonic[0] == 'j'){
           
            if(mnemonic != "ret" && instructions[i].op_str[0] == '0'){
                uint64_t target= std::stoull(instructions[i].op_str, nullptr, 16); //base 16 for hex, sring TO unsigned long long
                CB.edges.push_back(target); 
            }

            if (mnemonic != "jmp" && mnemonic != "ret" && i + 1 < instructions.size()) {
                CB.edges.push_back(instructions[i + 1].address);
            }

            blocks.insert(CB.st_address, CB);
            if (i+1 < instructions.size()){
                CB=Basic_Block();

            CB.st_address=instructions[i+1].address;
        }
    } 
} 

if (!CB.instruction.empty()){
    blocks.insert(CB.st_address, CB);
}
}
std::vector<vector<int>>CFG::translator(std::vector<uint64_t>& id_to_addr){
    std::vector<unsigned long long> keys;
    blocks.get_all_keys(keys);

    std::unordered_map<uint64_t, int> addr_to_id;
    std::vector<std::vector<int>>adj_list;

    for(size_t i=0;i<keys.size(); i++){
        addr_to_id[keys[i]] = i;
        id_to_addr.push_back(keys[i]);
        adj_list.push_back({});
    }

    for (size_t i=0;i<keys.size(); i++){
        uint64_t addr = keys[i];
        Basic_Block block;
        blocks.get(addr,block);

        int curr_id = addr_to_id[addr];

        for(uint64_t target : block.edges){
            if(addr_to_id.find(target) != addr_to_id.end()){
                adj_list[curr_id].push_back(addr_to_id[target]);

            }
        }
    }
    return adj_list;

}