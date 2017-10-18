/** \file hexReader.cpp
  * \brief A program to view binary data files as hexadecimal numbers.
  *
  * \author C. R. Thornsberry
  * \date Jul. 2nd, 2015
  */

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <bitset>
#include <stdlib.h>
#include <string.h>

#include "optionHandler.hpp"

#define HEAD 1145128264 // Run begin buffer
#define DATA 1096040772 // Physics data buffer
#define SCAL 1279345491 // Scaler type buffer
#define DEAD 1145128260 // Deadtime buffer
#define DIR 542263620   // "DIR "
#define PAC 541278544   // "PAC "
#define ENDFILE 541478725 // End of file buffer
#define ENDBUFF -1 // End of buffer marker

unsigned int buffer_select = 0;
unsigned int search_int = 0;
bool show_raw = false;
bool convert = false;
bool show_zero = true;
bool do_search = false;

/* Extract a string from a character array. */
std::string csubstr(char *str_, unsigned int start_index_=0){
	std::string output = "";
	unsigned int index = start_index_;
	while(str_[index] != '\0' && str_[index] != ' '){
		output += str_[index];
		index++;
	}
	return output;
}

bool is_alpha_numeric(const char &input_){
	if((input_ >= 0x30 && input_ <= 0x39) || (input_ >= 0x41 && input_ <= 0x5a) || (input_ >= 0x61 && input_ <= 0x7a)) return true;
	return false;
}

template <typename T>
std::string convert_to_ascii(T input_){
	char *output = new char[sizeof(T)+1];

	memcpy(output, (char*)&input_, sizeof(T));
	for(size_t i = 0; i < sizeof(T); i++){
		if(!is_alpha_numeric(output[i])) output[i] = 0x20;	
	}
	output[sizeof(T)] = '\0';
	
	return std::string(output);
}

template <typename T>
std::string convert_to_hex(T input_){
	std::bitset<sizeof(T)*8> set(input_);  
	std::stringstream stream;
	stream << std::hex << std::uppercase << set.to_ulong();
	std::string output = stream.str();
	if(output.size() < sizeof(T)*2){
		std::string temp = "0x";
		for(unsigned int i = output.size(); i < sizeof(T)*2; i++){
			temp += '0';
		}
		return temp + output;
	}

	return "0x" + output;
}

template <typename T>
void go(std::ifstream *input_, unsigned int &buff_count, unsigned int &good_buff_count, unsigned int &total_count){
	bool good_buffer;
	int show_next = 0;
	unsigned int count = 0;
	unsigned int word_count = 0;
	unsigned int words_per_line = 10;
	if(convert) words_per_line = 5;

	T word;

	if(buffer_select != 0){ good_buffer = false; }
	else{ good_buffer = true; }

	while(true){
		input_->read((char*)&word, sizeof(T));
		if(input_->eof()){ 
			if(buff_count > 1){
				std::cout << " Buffer Size: " << word_count << " words\n";
				std::cout << "============================================================================================================================\n";
			}
			break; 
		}

		if(do_search){
			if(show_next > 0){
				std::cout << convert_to_hex(word) << "  ";
				if(convert){ std::cout << convert_to_ascii(word) << "  "; }
				show_next = show_next - 1;
				if(show_next <= 0){ std::cout << std::endl; }
			}		
			else if(word == search_int){
				std::cout << convert_to_hex(word) << "  ";
				if(convert){ std::cout << convert_to_ascii(word) << "  "; }
				show_next = 4;
			}
			total_count++;
			word_count++;
			continue;
		}

		// Check for end of buffer
		/*if(word == ENDBUFF || (!show_zero && word == 0)){
			total_count++;
			continue;
		}*/
		if((!show_zero && word == 0)){
			total_count++;
			word_count++;
			continue;
		}
				
		// Check for a new buffer
		if(word == HEAD || word == DATA || word == SCAL || word == DEAD || word == DIR || word == PAC || word == ENDFILE){ // new buffer
			buff_count++;
			if(buffer_select != 0){
				if(word == buffer_select){ good_buffer = true; }
				else{ good_buffer = false; }
			}
			
			if(good_buffer){
				good_buff_count++;
				if(buff_count > 1){
					if(show_raw){ std::cout << "\n"; }
					std::cout << "\n Buffer Size: " << word_count << " words\n";
					std::cout << "============================================================================================================================\n";
				}
				std::cout << "\n============================================================================================================================\n";
				std::cout << " Buffer Number: " << buff_count << std::endl;
				std::cout << " Buffer Type: " << convert_to_hex(word);
				if(word == HEAD){ std::cout << " \"HEAD\"\n"; }
				else if(word == DATA){ std::cout << " \"DATA\"\n"; }
				else if(word == ENDFILE){ std::cout << " \"EOF \"\n"; }
				else if(word == SCAL){ std::cout << " \"SCAL\"\n"; }
				else if(word == DEAD){ std::cout << " \"DEAD\"\n"; }
				else if(word == DIR){ std::cout << " \"DIR\"\n"; }
				else if(word == PAC){ std::cout << " \"PAC\"\n"; }
				std::cout << " Total Count: " << total_count << " words\n";
				count = 0;
			}
			word_count = 0; // New buffer. Reset the buffer word count
		}
		
		if(!good_buffer){ // We don't care about this buffer
			total_count++;
			word_count++;
			continue; 
		} 

		total_count++;
		word_count++;
		if(show_raw){
			if(count == 0){ std::cout << "\n0000  "; }
			else if(count % words_per_line == 0){ 
				std::stringstream stream; stream << count;
				std::string temp_string = stream.str();
				std::string padding = "";
				if(temp_string.size() < 4){
					for(unsigned int i = 0; i < (4 - temp_string.size()); i++){ 
						padding += "0";
					}
				}
				std::cout << "\n" << padding << temp_string << "  "; 
			}
			std::cout << convert_to_hex(word) << "  "; count++;
			if(convert){ std::cout << convert_to_ascii(word) << "  "; }
		}
	}
}

// Display a list of commonly used ldf buffer headers.
void list(){
	std::cout << "  Typical ldf buffer types:\n";
	std::cout << "   \"HEAD\" 0x" << std::hex << HEAD << std::dec << " (" << HEAD << ")\n"; // ldf header buffer.
	std::cout << "   \"DATA\" 0x" << std::hex << DATA << std::dec << " (" << DATA << ")\n"; // Physics data buffer
	std::cout << "   \"SCAL\" 0x" << std::hex << SCAL << std::dec << " (" << SCAL << ")\n"; // Scaler type buffer
	std::cout << "   \"DEAD\" 0x" << std::hex << DEAD << std::dec << " (" << DEAD << ")\n"; // Deadtime buffer
	std::cout << "   \"DIR \" 0x" << std::hex << DIR << std::dec << " (" << DIR << ")\n";   // "DIR "
	std::cout << "   \"PAC \" 0x" << std::hex << PAC << std::dec << " (" << PAC << ")\n";   // "PAC "
	std::cout << "   \"EOF \" 0x" << std::hex << EOF << std::dec << " (" << EOF << ")\n";   // End of buffer marker
}

int main(int argc, char *argv[]){
	optionHandler handler;
	handler.add(optionExt("input", required_argument, NULL, 'i', "<filename>", "Specify the filename of the input data file"));
	handler.add(optionExt("type", required_argument, NULL, 't', "<int>", "Only display buffers of a specified type"));
	handler.add(optionExt("raw", no_argument, NULL, 'r', "", "Display raw buffer words (hidden by default)"));
	handler.add(optionExt("convert", no_argument, NULL, 'c', "", "Attempt to convert words to Ascii characters"));
	handler.add(optionExt("search", required_argument, NULL, 's', "<int>", "Search for an integer in the data stream"));
	handler.add(optionExt("list", no_argument, NULL, 'l', "", "Display a list of commonly used ldf buffer headers"));
	handler.add(optionExt("zero", no_argument, NULL, 'z', "", "Suppress zero output"));
	handler.add(optionExt("word", required_argument, NULL, 'w', "<int>", "Specify the file word size"));
	handler.add(optionExt("offset", required_argument, NULL, 'o', "<long long>", "Specify the start word of the file"));

	if(!handler.setup(argc, argv)){
		return 1;
	}

	// Display a list of buffer types.
	if(handler.getOption(5)->active){
		list();
		return 0;
	}

	std::string ifname;
	if(!handler.getOption(0)->active){
		std::cout << " ERROR: No input filename specified!\n";
		return 1;
	}
	else{
		ifname = handler.getOption(0)->argument;
	}
	
	int word_size = 4;
	std::streampos foffset = 0;
	
	if(handler.getOption(1)->active){
		buffer_select = strtoul(handler.getOption(1)->argument.c_str(), NULL, 0);
		std::cout << " Displaying only buffer type " << convert_to_hex(buffer_select) << "\n";		
	}
	if(handler.getOption(2)->active){
		show_raw = true;
	}
	if(handler.getOption(3)->active){
		convert = true;
	}
	if(handler.getOption(4)->active){
		do_search = true; 
		search_int = strtoul(handler.getOption(4)->argument.c_str(), NULL, 0);
		std::cout << " Searching for " << search_int << " (" << convert_to_hex(search_int) << ")\n";
	}
	if(handler.getOption(6)->active){
		show_zero = false;
	}
	if(handler.getOption(7)->active){
		word_size = strtol(handler.getOption(7)->argument.c_str(), NULL, 0);
		if(word_size != 1 && word_size != 2 && word_size != 4 && word_size != 8){
			std::cout << " Error: Invalid word size (" << word_size << " bytes)!\n";
			return 1;
		}
		std::cout << " Using word size of " << word_size << " bytes\n";
	}
	if(handler.getOption(8)->active){
		foffset = strtoll(handler.getOption(8)->argument.c_str(), NULL, 0);
		std::cout << " Starting at word no. " << foffset << " in file.\n";
	}
	
	std::ifstream input(ifname.c_str(), std::ios::binary);
	if(!input.good()){
		std::cout << " ERROR: Failed to open input file \"" << ifname << "\"!\n";
		return 1;
	}

	input.seekg(foffset*word_size);

	unsigned int good_buff_count = 0;
	unsigned int total_count = 0;
	unsigned int buff_count = 0;

	if(word_size == 1){ go<unsigned char>(&input, buff_count, good_buff_count, total_count); }
	else if(word_size == 2){ go<unsigned short>(&input, buff_count, good_buff_count, total_count); }
	else if(word_size == 4){ go<unsigned int>(&input, buff_count, good_buff_count, total_count); }
	else{ go<unsigned long long>(&input, buff_count, good_buff_count, total_count); }

	input.close();
	
	if(!do_search){
		std::cout << "\n\n Read " << total_count << " " << word_size << " byte words (";
		total_count *= word_size;
		std::cout << total_count << " bytes)\n";
		std::cout << "  Found " << buff_count << " total buffers\n";
		if(buffer_select != 0){ std::cout << "  Found " << good_buff_count << " " << convert_to_hex(buffer_select) << " buffers\n"; }
	}
	
	return 0;
}
