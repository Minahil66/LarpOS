#include<iostream>
#include<fstream>
#include<libelf.h>
#include<gelf.h>
#include<fcntl.h>
#include<unistd.h>
#include<vector>
#include"disasm.h" //user-defined library

namespace Disasm{
    std::vector<cs_insn>get_instruct(const std::string& filepath){
        std::vector<cs_insn>instruct_list; //to store instructs we go find in the assembly
        std::vector<uint8_t> bin_code; //machine code bytes
        uint64_t base_address;
        
        //1:
        int id=open(filepath.c_str(), O_RDONLY); //opens the file (cpp), changes into c, in read only
        if (id<0)
            return instruct_list; //return empty instruciton list if file open fails
    
            elf_version(EV_CURRENT); //EV_CURRECT = 1
            Elf* elf = elf_begin(id, ELF_C_READ,NULL); //Elf is an struct, ELF_C_READ tells libelf lib what you intent to do w the file (read in dis case) //check for the 4 bytes in the beginning 0x7f, E, L, F
            if(elf == NULL){
                close(id); //close the file if its not a valid elf file (null)
                return instruct_list; //empty instruction list;
            }

            //2:
            GElf_Ehdr ehdr; //predefined struct to hold elf header;
            cs_arch arch = CS_ARCH_X86; //default value
            cs_mode mode = CS_MODE_64; // Default to 64-bit

            //actually fill in the header;
            if(gelf_getehdr(elf, &ehdr) != NULL){
                if(ehdr.e_machine == EM_X86_64) arch = CS_ARCH_X86;
                else if( ehdr.e_machine == EM_386) mode=CS_MODE_32;
                else if( ehdr.e_machine == EM_ARM) { arch=CS_ARCH_ARM; mode=CS_MODE_ARM; }
                else if(ehdr.e_machine == EM_AARCH64) { arch=CS_ARCH_ARM64; mode=CS_MODE_ARM; }
                else if(ehdr.e_machine == EM_MIPS) { arch=CS_ARCH_MIPS; mode=CS_MODE_MIPS32; }
            }

            //3:
            size_t shstrndx; //index of text section
            elf_getshstrndx(elf, &shstrndx); //get the string table index

            Elf_Scn* scn=NULL; //pointer at beginning 
            while((scn = elf_nextscn(elf, scn)) !=NULL){ //loop through all sections
                GElf_Shdr shdr; //section header details for current section
                gelf_getshdr(scn, &shdr); //fill shdr- section header w current section
                char* name=elf_strptr(elf, shstrndx, shdr.sh_name); //get the name of the section we loopin throught
                if(name!=NULL && std::string(name) == ".text"){ // if that matches text we copy it into scn
                    Elf_Data* data = elf_getdata(scn, NULL);
                    if(data != NULL){
                        bin_code.assign((uint8_t*)data->d_buf, (uint8_t*)data->d_buf+data->d_size); //type cast data into raw byte format, instruct_list.assign(start_pointer, End pointer), d_buff tells you the beginning of the bytes and d_size tells u the end memory point
                    }
                    base_address = shdr.sh_addr; 
                    break;
                }
            }
            //4:
            elf_end(elf); //free mem
            close(id); //close linux system call

            //5:
            if(!bin_code.empty()){
                csh handle; //activate capstone
            
            if(cs_open(arch, mode, &handle)==CS_ERR_OK){
                cs_insn* insn = nullptr; //detailed info of disassembled intructs

                size_t count=cs_disasm(handle, bin_code.data(), bin_code.size(), base_address, 0, &insn); //translate the binary code to assembly, and returns number of instructions found

                if(count>0){
                    for(size_t i=0; i<count;i++){
                        instruct_list.push_back(insn[i]); //copy intruxtion into final list
                    }
                    cs_free(insn, count); //free memory
                }
                cs_close(&handle); //close capstone
            }
        }
        return instruct_list;
    }
}
