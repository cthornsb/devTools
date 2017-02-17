/** \file headitor.cpp
 * \brief A program to repair HEAD and DIR buffers written by poll2.
 *
 * This program is intended to be used in order to diagnose and repair
 * ldf files which contain buffers which are of the incorrect length.
 * CRT
 *
 * \author C. R. Thornsberry
 * \date Feb. 17th, 2017
 */
 
#include <iostream>
#include <fstream>
#include <sstream>
#include <bitset>

#include "optionHandler.hpp"

#define HEAD 1145128264 // Run begin buffer
#define DIR 542263620   // "DIR "

template <typename T>
std::string convert_to_hex(T input_, bool to_text_=false){
    std::bitset<sizeof(T)*8> set(input_);  
    std::stringstream stream;
    if(!to_text_){ stream << std::hex << std::uppercase << set.to_ulong(); }
    else{ stream << std::uppercase << set.to_ulong(); }
    std::string output = stream.str();
    if(!to_text_ && output.size() < sizeof(T)*2){
    	std::string temp = "0x";
    	for(unsigned int i = output.size(); i < sizeof(T)*2; i++){
    		temp += '0';
    	}
    	return temp + output;
    }
    else if(to_text_ && output.size() < (sizeof(T)+1)*2){
    	std::string temp = "";
    	for(unsigned int i = output.size(); i < (sizeof(T)+1)*2; i++){
    		temp += ' ';
    	}
    	return temp + output;
    }
    
    if(!to_text_){ return "0x" + output; }
    return output;
}

bool validBuffer(const unsigned int &head_){
	if(head_==HEAD || head_==DIR)
		return true;
		
	return false;
}

int main(int argc, char *argv[]){
	optionHandler handler;
	handler.addOption(optionExt("input", required_argument, NULL, 'i', "<filename>", "Specify the filename of the input ldf file"));
	handler.addOption(optionExt("force", no_argument, NULL, 'f', "", "Force overwrite of file header and never ask first"));
	handler.addOption(optionExt("debug", no_argument, NULL, 'd', "", "Toggle debug mode"));

	if(!handler.setup(argc, argv)){
		return 1;
	}

	std::string ifname;
	if(!handler.getOption(0)->active){
		std::cout << " ERROR: No input filename specified!\n";
		return 1;
	}
	else{
		ifname = handler.getOption(0)->argument;
	}

	bool forceOverwrite = false;
	if(handler.getOption(1)->active){
		forceOverwrite = true;
	}
	
	bool debug = false;
	if(handler.getOption(2)->active){
		debug = true;
	}

	// Open the input file.
	std::ifstream fin(ifname.c_str(), std::ios::binary);

	if(!fin.good()){
		std::cout << " ERROR: Failed to open input file \"" << ifname << "\"!\n";
		return 1;
	}
	
	// Check that the output file doesn't exist.
	/*if(!forceOverwrite){
		std::ifstream fouttest(ofname.c_str(), std::ios::binary);
		if(fouttest.good()){
			std::cout << " ERROR: Output file \"" << ofname << "\" already exists!\n";
			fin.close();
			return 1;
		}
	}

	// Open the output file.
	std::ofstream fout(ofname.c_str(), std::ios::binary);

	if(!fout.good()){
		std::cout << " ERROR: Failed to open output file \"" << ofname << "\"!\n";
		fin.close();
		return 1;
	}*/

	unsigned int buffCount = 0;
	unsigned int buffHeader;
	unsigned int buffLength;

	// Scan the input file and search for the end of the file header.	
	while(true){
		fin.read((char*)&buffHeader, 4);
		
		if(fin.eof()) break;
		
		if(validBuffer(buffHeader)){ // Skip the remaning words in the buffer.
			buffCount++;
			fin.read((char*)&buffLength, 4);
			fin.seekg(buffLength*4, std::ios::cur);
		}
		else break;
	}
	
	size_t endOfHeader = fin.tellg();
	endOfHeader -= 4;

	unsigned int *data = new unsigned int[endOfHeader/4];

	std::cout << " Found " << buffCount << " valid file header buffers.\n";
	std::cout << " Discovered end of file header at word " << endOfHeader/4 << " in input file.\n";
	
	if(debug) std::cout << " Copying " << endOfHeader << " B from input file.\n";

	fin.seekg(0, std::ios::beg);
	fin.read((char*)data, endOfHeader);
	fin.close();

	unsigned int count = 0;
	for(size_t index = 0; index < endOfHeader/4; index++){
		if(count == 0){ std::cout << "\n00000  "; }
		else if(count % 10 == 0){ 
			std::stringstream stream; stream << count;
			std::string temp_string = stream.str();
			std::string padding = "";
			if(temp_string.size() < 5){
				for(unsigned int i = 0; i < (5 - temp_string.size()); i++){ 
					padding += "0";
				}
			}
			std::cout << "\n" << padding << temp_string << "  "; 
		}
		std::cout << convert_to_hex(data[index]) << "  "; count++;
	}

	delete[] data;
	
	return 0;
}
