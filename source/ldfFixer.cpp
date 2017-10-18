/** \file ldfFixer.cpp
 * \brief A program to repair invalid ldf buffers written by poll2.
 *
 * This program is intended to be used in order to diagnose and repair
 * ldf files which contain buffers which are of the incorrect length.
 * CRT
 *
 * \author C. R. Thornsberry
 * \date Feb. 8th, 2017
 */
 
#include <iostream>
#include <fstream>
#include <vector>

#include "optionHandler.hpp"

#define HEAD 1145128264 // Run begin buffer
#define DATA 1096040772 // Physics data buffer
#define SCAL 1279345491 // Scaler type buffer
#define DEAD 1145128260 // Deadtime buffer
#define DIR 542263620   // "DIR "
#define PAC 541278544   // "PAC "
#define ENDFILE 541478725 // End of file buffer

const unsigned int delimiter = -1;

const int buffLength = 8194;
const int buffLengthB = 32776;

class buffer{
  public:
	std::streampos startpos;
	int length;
	bool valid;
	
	buffer() : startpos(0), length(0), valid(false) { }
	
	buffer(const std::streampos &start_, const int &length_) : startpos(start_), length(length_) { 
		if(length == buffLength) valid = true;
		else valid = false;
	}
};

bool validBuffer(const unsigned int &head_){
	if(head_==HEAD || head_==DATA || head_==SCAL || head_==DEAD || head_==DIR || head_==PAC || head_==ENDFILE)
		return true;
		
	return false;
}

int main(int argc, char *argv[]){
	optionHandler handler;
	handler.add(optionExt("input", required_argument, NULL, 'i', "<filename>", "Specify the filename of the input ldf file"));
	handler.add(optionExt("output", required_argument, NULL, 'o', "<filename>", "Specify the filename of the output ldf file"));
	handler.add(optionExt("force", no_argument, NULL, 'f', "", "Force overwrite of the output ldf file"));
	handler.add(optionExt("debug", no_argument, NULL, 'd', "", "Toggle debug mode"));

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
	
	std::string ofname = "ldfFixer.ldf";
	if(handler.getOption(1)->active){
		ofname = handler.getOption(1)->argument;
	}

	bool forceOverwrite = false;
	if(handler.getOption(2)->active){
		forceOverwrite = true;
	}
	
	bool debug = false;
	if(handler.getOption(3)->active){
		debug = true;
	}

	// Open the input file.
	std::ifstream fin(ifname.c_str(), std::ios::binary);

	if(!fin.good()){
		std::cout << " ERROR: Failed to open input file \"" << ifname << "\"!\n";
		return 1;
	}
	
	// Check that the output file doesn't exist.
	if(!forceOverwrite){
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
	}

	unsigned int buffHeader;

	std::streampos lastValidHeader = 0;
	std::streampos prevBufferStart = 0;

	std::vector<buffer> fileBuffers;

	fin.seekg(0, std::ios::end);
	std::streampos fileLength = fin.tellg();
	fin.seekg(0, std::ios::beg);
	
	std::cout << " Greetings gentlemen. I'm the fixer. I make buffer problems go away.\n";
	
	// Report the file size and the number of buffers.
	std::cout << " Input file length is " << fileLength << " B (" << fileLength/4 << " words, " << fileLength/buffLengthB << 
	             " ldf buffers w/ rem=" << (fileLength%buffLengthB)/4 << " words [delta=" << ((fileLength%buffLengthB)/4)-buffLength << "])\n\n";

	int errorCount = 1;
	int numBuffers = 0;
	int numBadBuffers = 0;

	// Scan the input file and search for buffer errors.	
	while(true){
		fin.read((char*)&buffHeader, 4);
		
		if(fin.eof()) break;
		
		if(validBuffer(buffHeader)){ // Skip the remaning words in the buffer.
			lastValidHeader = fin.tellg();
			fin.seekg(buffLengthB-4, std::ios::cur);
			if(numBuffers > 0)
				fileBuffers.push_back(buffer(prevBufferStart, buffLength));
			prevBufferStart = lastValidHeader;
			prevBufferStart += -4;
			numBuffers++;
			continue;
		}
		
		// Rewind to just after the most recent good header.
		fin.seekg(lastValidHeader, std::ios::beg);
		int bufferLength = 1;
		
		// Search for the next valid buffer.
		while(true){
			fin.read((char*)&buffHeader, 4);
			
			if(fin.eof()) break;
			
			if(!validBuffer(buffHeader)){
				bufferLength++;
				continue;
			}
			
			// We found the next good buffer.
			std::cout << " " << errorCount++ << ") INVALID BUFFER no. " << fileBuffers.size()+1 << " at position " << lastValidHeader/4 << " in file. Buffer contains " << bufferLength << " words [delta=" << bufferLength-buffLength << "] ";
			if(bufferLength < buffLength ) std::cout << "(UNDERFLOW)\n";
			else std::cout << "(OVERFLOW)\n";
			
			fileBuffers.push_back(buffer(lastValidHeader, bufferLength));
			lastValidHeader = fin.tellg();
			fin.seekg(buffLengthB-4, std::ios::cur);
			numBuffers++;
			numBadBuffers++;
			break;
		}
	}
	
	// Add the final buffer to the list.
	fileBuffers.push_back(buffer(lastValidHeader, (int)(fileLength-lastValidHeader)/4+1));
	
	if(numBadBuffers == 0){
		std::cout << " Found no ldf buffer errors! Nothing to repair :-)\n";
	}
	else{
		std::cout << "\n I found " << numBadBuffers << " invalid buffers of " << numBuffers << " total.\n";
		std::cout << "  WARNING: This file will not be scannable by UTKscan! Repair is recommended.\n\n";
		
		std::string userInput;
		while(true){
			std::cout << " Time is of the essence. Shall we proceed with buffer repair? (y,n) ";
			std::cin >> userInput;
			if(userInput == "y" || userInput == "Y" || userInput == "n" || userInput == "N") break;
			std::cout << "  Type (y) or (n).\n";
		}
		
		std::cout << std::endl;
		
		if(userInput == "n" || userInput == "N"){
			std::cout << " Aborting!\n";
			fin.close();
			fout.close();
			return 0;
		}

		// Clear the eof state of the ifstream and seek to the beginning.
		fin.clear();
		fin.seekg(0, std::ios::beg);
	
		unsigned int data[buffLength];
	
		errorCount = 1;
		for(std::vector<buffer>::iterator iter = fileBuffers.begin(); iter != fileBuffers.end(); ++iter){
			if(iter->valid){
				fin.read((char*)data, buffLength*4);
				if(debug)
					std::cout << "  DEBUG: Copying buffer at position " << iter->startpos/4 << " [start=0x" << std::hex << data[0] << ", stop=0x" << data[buffLength-2] << std::dec << "]\n";
				fout.write((char*)data, buffLength*4);
			}
			else{
				std::cout << " Reparing buffer error no. " << errorCount++ << ")\n";
				fin.read((char*)data, iter->length*4);
				std::cout << " -Copying " << iter->length << " words at position " << iter->startpos/4 << "\n";
				fout.write((char*)data, iter->length*4);				
				if(buffLength > iter->length){ // Underfilled buffer.
					std::cout << " -Appending " << buffLength-iter->length << " words to end of buffer\n\n";
					for(int i = 0; i < buffLength-iter->length; i++){
						fout.write((char*)&delimiter, 4);
					}
				}
				else{ // Overfilled buffer.
					std::cout << " -WARNING: Repair of overfilled buffers is not currently implemented!\n";
				}
			}
		}
		
		// Report on what we did.
		fileLength = fout.tellp();
		std::cout << " DONE! Successfully repaired " << numBadBuffers << " invalid buffers!\n";
		std::cout << " Output file length is " << fileLength << " B (" << fileLength/4 << " words, " << fileLength/buffLengthB << 
			         " ldf buffers w/ rem=" << (fileLength%buffLengthB)/4 << " words [delta=" << ((fileLength%buffLengthB)/4)-buffLength << "])\n";
	}
	
	fin.close();
	fout.close();
	
	return 0;
}
