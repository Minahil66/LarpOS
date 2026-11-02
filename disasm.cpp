#include<iostream>
#include<fstream>
#include<libelf.h>
#include<gelf.h>
#include<fcntl.h>
#include<unistd.h>
#include<vector>
#include<disasm.h> //user-defined library

namespace Disarm{
    std::vector<cs_insn>get_instruct(const std::string& filepath){
        std::vector<cs_insn>instruct_list; //to store instructs we go find in the assembly
        std::vector<u_int8_t> bin_code; //machien code bytes
        
        //1:
        int id=open(filepath.c_str(), O_RDONLY); //opens the file (cpp), changes into c, in read only
        if (id<0){
            return instruct_list; //return empty instruciton list if file open fails
    
            elf_version(EV_CURRENT); //EV_CURRECT = 1
            Elf* elf = elf_begin(id, ELF_C_READ,NULL); //Elf is an struct, ELF_C_READ tells libelf lib what you intent to do w the file (read in dis case) //check for the 4 bytes in the beginning 0x7f, E, L, F
            if(elf == NULL){
                close(id); //close the file if its not a valid elf file (null)
                return instruct_list; //empty instruction list;
            }

            //2:
            GElf_Ehdr ehdr; //predefined struct to hold elf header;
            cs_arch archit = CS_ARCH_X86; //default value;
        }
    }

}