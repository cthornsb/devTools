#include <iostream>
#include <fstream>

#include <unistd.h>
#include <getopt.h>
#include <cstring>
#include <cmath>
#include <chrono>

#include "XiaData.hpp"

// Local files
#include "tqdcAnalyzer.hpp"

// Define the name of the program.
#ifndef PROG_NAME
#define PROG_NAME "tqdcAnalyzer"
#endif

const double fwhmCoeff = 2*std::sqrt(2*std::log(2));

///////////////////////////////////////////////////////////////////////////////
// class tqdcUnpacker
///////////////////////////////////////////////////////////////////////////////

/** Process all events in the event list.
  * \param[in]  addr_ Pointer to a location in memory. 
  * \return Nothing.
  */
void tqdcUnpacker::ProcessRawEvent(ScanInterface *addr_/*=NULL*/){
	if(!addr_){ return; }
	
	XiaData *current_event = NULL;
	
	// Fill the processor event deques with events
	while(!rawEvent.empty()){
		if(!running) break;
	
		current_event = rawEvent.front();
		rawEvent.pop_front(); // Remove this event from the raw event deque.

		// Check that this channel event exists.
		if(!current_event){ continue; }

		// Send the event to the scan interface object for processing.
		if(addr_->AddEvent(current_event))
			addr_->ProcessEvents();
	}
	
	// Finish up with this raw event.
	addr_->ProcessEvents();
}

///////////////////////////////////////////////////////////////////////////////
// class tqdcScanner
///////////////////////////////////////////////////////////////////////////////

/// Default constructor.
tqdcScanner::tqdcScanner() : ScanInterface(), minimumEvents(10000), chanid(0), integrationRangeLow(5), integrationRangeHigh(10), shortIntegralRangeLow(5), shortIntegralRangeHigh(10) {
}

/// Destructor.
tqdcScanner::~tqdcScanner(){
	ClearAll();
}

/** ExtraCommands is used to send command strings to classes derived
  * from ScanInterface. If ScanInterface receives an unrecognized
  * command from the user, it will pass it on to the derived class.
  * \param[in]  cmd_ The command to interpret.
  * \param[out] arg_ Vector or arguments to the user command.
  * \return True if the command was recognized and false otherwise.
  */
bool tqdcScanner::ExtraCommands(const std::string &cmd_, std::vector<std::string> &args_){
	if(cmd_ == "tqdc"){
		tqdcs.clear();
		for(std::deque<ChannelEvent*>::iterator iter = unsorted.begin(); iter != unsorted.end(); ++iter){
			(*iter)->ComputeBaseline();
			tqdcs.push_back((*iter)->IntegratePulse((*iter)->max_index-integrationRangeLow, (*iter)->max_index+integrationRangeHigh));
		}
		ProcessTQDC();
	}
	else if(cmd_ == "psd"){
		tqdcs.clear();
		tqdcs2.clear();
		for(std::deque<ChannelEvent*>::iterator iter = unsorted.begin(); iter != unsorted.end(); ++iter){
			(*iter)->ComputeBaseline();
			tqdcs.push_back((*iter)->IntegratePulse((*iter)->max_index-integrationRangeLow, (*iter)->max_index+integrationRangeHigh));
			tqdcs2.push_back((*iter)->IntegratePulse2((*iter)->max_index-shortIntegralRangeLow, (*iter)->max_index+shortIntegralRangeHigh));
		}
		ProcessTQDC();
	}
	else if(cmd_ == "set"){
		if(args_.size() >= 1){ // Set the channel id.
			chanid = strtoul(args_.at(0).c_str(), NULL, 0);
			std::cout << msgHeader << "Set signal ID to (" << chanid << ").\n";
		}
		else std::cout << msgHeader << "Current chanid=" << chanid << std::endl;
	}
	else if(cmd_ == "clear"){
		std::cout << msgHeader << "Clearing events\n";
		ClearAll();
	}
	else if(cmd_ == "size"){
		std::cout << msgHeader << "Currently " << unsorted.size() << " events in deque\n";
	}
	else if(cmd_ == "length"){
		if(!unsorted.empty()) std::cout << msgHeader << "Trace length = " << unsorted.at(0)->traceLength << " ticks (" << unsorted.at(0)->traceLength*4 << " ns)\n";
		else std::cout << msgHeader << "Event list is empty!\n";
	}
	else if(cmd_ == "num"){
		if(args_.size() >= 1){
			minimumEvents = strtoul(args_.at(0).c_str(), NULL, 0);
			std::cout << msgHeader << "Set minimum number of events to " << minimumEvents << "\n";
		}
		else std::cout << msgHeader << "Minimum number of events is " << minimumEvents << "\n";
	}
	else if(cmd_ == "write"){
		std::string ofname = "tqdc.dat";
		if(args_.size() >= 1) ofname = args_.at(0);
		if(Write(ofname.c_str())) // Write the output file.
			std::cout << msgHeader << "Wrote time differences to file \"" << ofname << "\".\n";
		else
			std::cout << msgHeader << "Error! Failed to open file \"" << ofname << "\" for writing.\n";
	}
	else if(cmd_ == "range"){
		if(args_.size() >= 2){
			if(args_.size() >= 4){
				shortIntegralRangeLow = strtol(args_.at(2).c_str(), NULL, 0);
				shortIntegralRangeHigh = strtol(args_.at(3).c_str(), NULL, 0);
			}
			integrationRangeLow = strtol(args_.at(0).c_str(), NULL, 0);
			integrationRangeHigh = strtol(args_.at(1).c_str(), NULL, 0);
		}
		std::cout << " " << integrationRangeLow << ", " << integrationRangeHigh << ", " << shortIntegralRangeLow << ", " << shortIntegralRangeHigh << std::endl;
	}
	else{ return false; } // Unrecognized command.

	return true;
}

/** ExtraArguments is used to send command line arguments to classes derived
  * from ScanInterface. This method should loop over the optionExt elements
  * in the vector userOpts and check for those options which have been flagged
  * as active by ::Setup(). This should be overloaded in the derived class.
  * \return Nothing.
  */
void tqdcScanner::ExtraArguments(){
	if(userOpts.at(0).active){
		chanid = strtoul(userOpts.at(0).argument.c_str(), NULL, 0);
		std::cout << msgHeader << "Set signal ID to " << chanid << ".\n";
	}
	if(userOpts.at(1).active){
		minimumEvents = strtoul(userOpts.at(1).argument.c_str(), NULL, 0);
		std::cout << msgHeader << "Set minimum number of events to " << minimumEvents << ".\n";
	}
}

/** CmdHelp is used to allow a derived class to print a help statement about
  * its own commands. This method is called whenever the user enters 'help'
  * or 'h' into the interactive terminal (if available).
  * \param[in]  prefix_ String to append at the start of any output. Not used by default.
  * \return Nothing.
  */
void tqdcScanner::CmdHelp(const std::string &prefix_/*=""*/){
	std::cout << "   tqdc                                      - Integrate all traces in range [low,high].\n";
	std::cout << "   psd                                       - Integrate all traces in range [low,high] and [shortLow,shortHigh]\n";
	std::cout << "   set [chanid]                              - Set the pixie ID signal.\n";
	std::cout << "   clear                                     - Clear all TOF pairs in the deque.\n";
	std::cout << "   size                                      - Print the number of TOF pairs in the deque.\n";
	std::cout << "   length                                    - Print the length of the first event's trace.\n";
	std::cout << "   num [numTraces]                           - Set the minimum number of events.\n";
	std::cout << "   write [filename]                          - Write time differences to an output file.\n";
	std::cout << "   range [low] [high] [shortLow] [shortHigh] - Set the range to use for fits [maxIndex-low, maxIndex+high].\n";
}

/** ArgHelp is used to allow a derived class to add a command line option
  * to the main list of options. This method is called at the end of
  * from the ::Setup method.
  * Does nothing useful by default.
  * \return Nothing.
  */
void tqdcScanner::ArgHelp(){
	AddOption(optionExt("id", required_argument, NULL, 0x0, "<id>", "Set the ID of the channel to analyze."));
	AddOption(optionExt("num-events", required_argument, NULL, 'N', "<num>", "Set the minimum number of events to load."));
}

/** SyntaxStr is used to print a linux style usage message to the screen.
  * \param[in]  name_ The name of the program.
  * \return Nothing.
  */
void tqdcScanner::SyntaxStr(char *name_){ 
	std::cout << " usage: " << std::string(name_) << " [options]\n"; 
}

/** Initialize the map file, the config file, the processor handler, 
  * and add all of the required processors.
  * \param[in]  prefix_ String to append to the beginning of system output.
  * \return True upon successfully initializing and false otherwise.
  */
bool tqdcScanner::Initialize(std::string prefix_){
	// Do some initialization.
	return true;
}

/** Peform any last minute initialization before processing data. 
  * /return Nothing.
  */
void tqdcScanner::FinalInitialization(){
	// Do some last minute initialization before the run starts.
}

/** Receive various status notifications from the scan.
  * \param[in] code_ The notification code passed from ScanInterface methods.
  * \return Nothing.
  */
void tqdcScanner::Notify(const std::string &code_/*=""*/){
	if(code_ == "START_SCAN"){  }
	else if(code_ == "STOP_SCAN"){  }
	else if(code_ == "SCAN_COMPLETE"){ 
		std::cout << msgHeader << "Scan complete.\n"; 
		std::cout << msgHeader << "Loaded " << unsorted.size() << " events from input file.\n";
	}
	else if(code_ == "LOAD_FILE"){ std::cout << msgHeader << "File loaded.\n"; }
	else if(code_ == "REWIND_FILE"){  }
	else{ std::cout << msgHeader << "Unknown notification code '" << code_ << "'!\n"; }
}

/** Return a pointer to the Unpacker object to use for data unpacking.
  * If no object has been initialized, create a new one.
  * \return Pointer to an Unpacker object.
  */
Unpacker *tqdcScanner::GetCore(){ 
	if(!core){ core = (Unpacker*)(new tqdcUnpacker()); }
	return core;
}

/** Add a channel event to the deque of events to send to the processors.
  * This method should only be called from tqdcUnpacker::ProcessRawEvent().
  * \param[in]  event_ The raw XiaData to add to the channel event deque.
  * \return False.
  */
bool tqdcScanner::AddEvent(XiaData *event_){
	if(!event_){ return false; }

	if(event_->getID() == chanid)
		unsorted.push_back(new ChannelEvent(event_));
	
	if(unsorted.size() >= minimumEvents){
		std::cout << msgHeader << "Loaded " << unsorted.size() << " events from input file.\n";
		stop_scan();
	}

	delete event_;

	// Return false so Unpacker will only call ProcessEvents() after finishing the entire rawEvent.
	return false;
}

/** Process all channel events read in from the rawEvent.
  * This method should only be called from tqdcUnpacker::ProcessRawEvent().
  * \return True.
  */
bool tqdcScanner::ProcessEvents(){
	return false;
}

void tqdcScanner::ProcessTQDC(){
	double mean = 0.0;
	double stddev = 0.0;
	unsigned long nGood = 0;
	for(std::vector<double>::iterator iter = tqdcs.begin(); iter != tqdcs.end(); ++iter){
		if(*iter <= 0) continue;
		mean += *iter;
		nGood++;
	}
	mean = mean / nGood;
	for(std::vector<double>::iterator iter = tqdcs.begin(); iter != tqdcs.end(); ++iter){
		if(*iter <= 0) continue;
		stddev += std::pow(*iter-mean, 2.0);
	}
	stddev = std::sqrt(stddev/(nGood-1));
	std::cout << msgHeader << " Mean TQDC = " << mean << "\n";
	std::cout << msgHeader << " Std. Dev. = " << stddev << " (" << fwhmCoeff*stddev << " fwhm)\n";
}

void tqdcScanner::ClearAll(){
	while(!unsorted.empty()){
		delete unsorted.front();
		unsorted.pop_front();
	}
	tqdcs.clear();
}

bool tqdcScanner::Write(const char *fname/*="timing.dat"*/){
	std::ofstream ofile(fname);
	if(!ofile.good()){
		ofile.close();
		return false;
	}

	ofile << "ltqdc\tstqdc\n";
	for(std::deque<ChannelEvent*>::iterator iter = unsorted.begin(); iter != unsorted.end(); ++iter){
		// Write the output.
		ofile << (*iter)->qdc << "\t" << (*iter)->qdc2 << std::endl;
	}
	ofile.close();

	return true;
}

int main(int argc, char *argv[]){
	// Define a new unpacker object.
	tqdcScanner scanner;
	
	// Set the output message prefix.
	scanner.SetProgramName(std::string(PROG_NAME));	
	
	// Initialize the scanner.
	if(!scanner.Setup(argc, argv))
		return 1;

	// Run the main loop.
	int retval = scanner.Execute();
	
	scanner.Close();
	
	return retval;
}
