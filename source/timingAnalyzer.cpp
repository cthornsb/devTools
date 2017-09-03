#include <iostream>
#include <fstream>

#include <unistd.h>
#include <getopt.h>
#include <cstring>
#include <cmath>
#include <chrono>

#include "XiaData.hpp"

// Local files
#include "timingAnalyzer.hpp"

// Define the name of the program.
#ifndef PROG_NAME
#define PROG_NAME "TimingAnalyzer"
#endif

void displayBool(const char *msg_, const bool &val_){
	if(val_) std::cout << msg_ << "YES\n";
	else std::cout << msg_ << "NO\n";
}

typedef std::chrono::high_resolution_clock::time_point hr_time;
typedef std::chrono::high_resolution_clock hr_clock;

///////////////////////////////////////////////////////////////////////////////
// class ChanPair
///////////////////////////////////////////////////////////////////////////////

ChanPair::~ChanPair(){
}

double ChanPair::Analyze(timingAnalyzer analyzer/*=POLY*/, const float &par1_/*=0.5*/, const float &par2_/*=1*/, const float &par3_/*=1*/){
	start->ComputeBaseline();
	stop->ComputeBaseline();
	
	// Start the timer.
	hr_time start_time = hr_clock::now();

	if(analyzer == POLY){
		start->AnalyzePolyCFD(par1_);
		stop->AnalyzePolyCFD(par1_);
	}
	else if(analyzer == CFD){
		start->AnalyzeCFD(par1_, (int)par2_, (int)par3_);
		stop->AnalyzeCFD(par1_, (int)par2_, (int)par3_);
	}
	else if(analyzer == FIT){
	}

	// Stop the timer.
	hr_time stop_time = hr_clock::now();

	// Calculate the time taken to analyze the trace.
	std::chrono::duration<double> time_span;
	time_span = std::chrono::duration_cast<std::chrono::duration<double> >(stop_time - start_time); // Time between packets in seconds
	timeTaken = time_span.count();
	
	return (start->time*8 + start->phase*4) - (stop->time*8 + stop->phase*4);
}

///////////////////////////////////////////////////////////////////////////////
// class timingUnpacker
///////////////////////////////////////////////////////////////////////////////

/** Process all events in the event list.
  * \param[in]  addr_ Pointer to a location in memory. 
  * \return Nothing.
  */
void timingUnpacker::ProcessRawEvent(ScanInterface *addr_/*=NULL*/){
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
// class timingScanner
///////////////////////////////////////////////////////////////////////////////

/// Default constructor.
timingScanner::timingScanner() : ScanInterface(), minimumTraces(5000), startID(0), stopID(1), par1(0.5), par2(1), par3(1), analyzer(POLY) {
}

/// Destructor.
timingScanner::~timingScanner(){
	ClearAll();
}

/** ExtraCommands is used to send command strings to classes derived
  * from ScanInterface. If ScanInterface receives an unrecognized
  * command from the user, it will pass it on to the derived class.
  * \param[in]  cmd_ The command to interpret.
  * \param[out] arg_ Vector or arguments to the user command.
  * \return True if the command was recognized and false otherwise.
  */
bool timingScanner::ExtraCommands(const std::string &cmd_, std::vector<std::string> &args_){
	if(cmd_ == "analyze"){
		if(args_.size() >= 1){
			par1 = strtof(args_.at(0).c_str(), NULL);
			if(args_.size() >= 2)
				par2 = strtof(args_.at(1).c_str(), NULL);
			if(args_.size() >= 3)
				par3 = strtof(args_.at(2).c_str(), NULL);
			std::cout << msgHeader << "Set analyzer parameters to (" << par1 << ", " << par2 << ", " << par3 << ")\n";
		}
		double totalTime = 0;
		tdiffs.clear();
		for(std::deque<ChanPair>::iterator iter = tofPairs.begin(); iter != tofPairs.end(); ++iter){
			tdiffs.push_back(iter->Analyze(analyzer, par1, par2, par3));
			totalTime += iter->timeTaken;
		}
		std::cout << msgHeader << "Total time taken = " << totalTime*(1E6) << " us for " << tdiffs.size() << " traces\n";
		std::cout << msgHeader << " Average time per trace = " << totalTime*(1E6)/tdiffs.size() << " us\n";
		ProcessTimeDifferences();
		/*if(args_.size() >= 1){ // Skip the specified number of events.
			showNextEvent = true;
			numSkip = strtoul(args_.at(0).c_str(), NULL, 0);
			std::cout << msgHeader << "Skipping " << numSkip << " events.\n";
		}
		else{ showNextEvent = true; }*/
	}
	else if(cmd_ == "set"){
		if(args_.size() >= 2){ // Set the start and stop IDs.
			startID = strtoul(args_.at(0).c_str(), NULL, 0);
			stopID = strtoul(args_.at(1).c_str(), NULL, 0);
			std::cout << msgHeader << "Set TOF start signal ID to (" << startID << ").\n";
			std::cout << msgHeader << "Set TOF stop signal ID to (" << stopID << ").\n";
		}
		else std::cout << msgHeader << "Current startID=" << startID << ", stopID=" << stopID << std::endl;
	}
	else if(cmd_ == "method"){
		if(args_.size() >= 1){ // Set the high-res timing analysis to use.
			if(args_.at(0) == "POLY")     analyzer = POLY;
			else if(args_.at(0) == "CFD") analyzer = CFD;
			else if(args_.at(0) == "FIT") analyzer = FIT;
			else{
				std::cout << msgHeader << "Unknown timing analyzer specified (" << args_.at(0) << ")\n";
				std::cout << msgHeader << "Valid options are \"POLY\", \"CFD\", and \"FIT\"\n";
			}
		}
		else std::cout << msgHeader << "Current timing analyzer is \"" << analyzer << "\"\n";
	}
	else if(cmd_ == "clear"){
		std::cout << msgHeader << "Clearing TOF pairs\n";
		ClearAll();
	}
	else if(cmd_ == "size"){
		std::cout << msgHeader << "Currently " << tofPairs.size() << " TOF pairs in deque\n";
	}
	else if(cmd_ == "num"){
		if(args_.size() >= 1){
			minimumTraces = strtoul(args_.at(0).c_str(), NULL, 0);
			std::cout << msgHeader << "Set minimum number of traces to " << minimumTraces << "\n";
		}
		else std::cout << msgHeader << "Minimum number of traces is " << minimumTraces << "\n";
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
void timingScanner::ExtraArguments(){
	if(userOpts.at(0).active){
		startID = strtoul(userOpts.at(0).argument.c_str(), NULL, 0);
		std::cout << msgHeader << "Set TOF start signal ID to " << startID << ".\n";
	}
	if(userOpts.at(1).active){
		stopID = strtoul(userOpts.at(1).argument.c_str(), NULL, 0);
		std::cout << msgHeader << "Set TOF stop signal ID to " << stopID << ".\n";
	}
	if(userOpts.at(2).active){
		minimumTraces = strtoul(userOpts.at(2).argument.c_str(), NULL, 0);
		std::cout << msgHeader << "Set minimum number of traces to " << minimumTraces << ".\n";
	}
}

/** CmdHelp is used to allow a derived class to print a help statement about
  * its own commands. This method is called whenever the user enters 'help'
  * or 'h' into the interactive terminal (if available).
  * \param[in]  prefix_ String to append at the start of any output. Not used by default.
  * \return Nothing.
  */
void timingScanner::CmdHelp(const std::string &prefix_/*=""*/){
	std::cout << "   analyze [par1=0.5] [par2=1] [par3=1] - Analyze high-resolution time differences.\n";
	std::cout << "   set [startID] [stopID]               - Set the pixie ID of the TOF start and stop signals.\n";
	std::cout << "   method [analyzer]                    - Set the high-res trace analyzer (\"POLY\", \"CFD\", or \"FIT\").\n";
	std::cout << "   clear                                - Clear all TOF pairs in the deque.\n";
	std::cout << "   size                                 - Print the number of TOF pairs in the deque.\n";
	std::cout << "   num [numTraces]                      - Set the minimum number of traces.\n";
}

/** ArgHelp is used to allow a derived class to add a command line option
  * to the main list of options. This method is called at the end of
  * from the ::Setup method.
  * Does nothing useful by default.
  * \return Nothing.
  */
void timingScanner::ArgHelp(){
	AddOption(optionExt("start-id", required_argument, NULL, 0x0, "<start>", "Set the ID of the TOF start signal."));
	AddOption(optionExt("stop-id", required_argument, NULL, 0x0, "<stop>", "Set the ID of the TOF stop signal."));
	AddOption(optionExt("num-traces", required_argument, NULL, 'N', "<num>", "Set the minimum number of traces to load."));
}

/** SyntaxStr is used to print a linux style usage message to the screen.
  * \param[in]  name_ The name of the program.
  * \return Nothing.
  */
void timingScanner::SyntaxStr(char *name_){ 
	std::cout << " usage: " << std::string(name_) << " [options]\n"; 
}

/** Initialize the map file, the config file, the processor handler, 
  * and add all of the required processors.
  * \param[in]  prefix_ String to append to the beginning of system output.
  * \return True upon successfully initializing and false otherwise.
  */
bool timingScanner::Initialize(std::string prefix_){
	// Do some initialization.
	return true;
}

/** Peform any last minute initialization before processing data. 
  * /return Nothing.
  */
void timingScanner::FinalInitialization(){
	// Do some last minute initialization before the run starts.
}

/** Receive various status notifications from the scan.
  * \param[in] code_ The notification code passed from ScanInterface methods.
  * \return Nothing.
  */
void timingScanner::Notify(const std::string &code_/*=""*/){
	if(code_ == "START_SCAN"){  }
	else if(code_ == "STOP_SCAN"){  }
	else if(code_ == "SCAN_COMPLETE"){ 
		std::cout << msgHeader << "Scan complete.\n"; 
		std::cout << msgHeader << "Loaded " << tofPairs.size() << " TOF pairs from input file.\n";
	}
	else if(code_ == "LOAD_FILE"){ std::cout << msgHeader << "File loaded.\n"; }
	else if(code_ == "REWIND_FILE"){  }
	else{ std::cout << msgHeader << "Unknown notification code '" << code_ << "'!\n"; }
}

/** Return a pointer to the Unpacker object to use for data unpacking.
  * If no object has been initialized, create a new one.
  * \return Pointer to an Unpacker object.
  */
Unpacker *timingScanner::GetCore(){ 
	if(!core){ core = (Unpacker*)(new timingUnpacker()); }
	return core;
}

/** Add a channel event to the deque of events to send to the processors.
  * This method should only be called from timingUnpacker::ProcessRawEvent().
  * \param[in]  event_ The raw XiaData to add to the channel event deque.
  * \return False.
  */
bool timingScanner::AddEvent(XiaData *event_){
	if(!event_){ return false; }

	if(event_->getID() == startID || event_->getID() == stopID)
		unsorted.push_back(new ChannelEvent(event_));
	
	delete event_;

	// Return false so Unpacker will only call ProcessEvents() after finishing the entire rawEvent.
	return false;
}

/** Process all channel events read in from the rawEvent.
  * This method should only be called from timingUnpacker::ProcessRawEvent().
  * \return True.
  */
bool timingScanner::ProcessEvents(){
	if(tofPairs.size() >= minimumTraces){ return false; }

	// Process all of the events added so far.
	ChannelEvent *currentEvent;
	ChannelEvent *nextEvent;
	while(!unsorted.empty()){
		currentEvent = unsorted.front();
		unsorted.pop_front();
	
		// Check for the next event.	
		if(unsorted.empty()){
			delete currentEvent;
			break;
		}

		nextEvent = unsorted.front();

		if(currentEvent->getID() == startID){
			if(nextEvent->getID() == stopID){
				tofPairs.push_back(ChanPair(currentEvent, nextEvent));
				unsorted.pop_front();
			}
			else delete currentEvent;
		}
		else{
			if(nextEvent->getID() == startID){
				tofPairs.push_back(ChanPair(nextEvent, currentEvent));
				unsorted.pop_front();
			}
			else delete currentEvent;
		}
	}

	if(tofPairs.size() >= minimumTraces){
		std::cout << msgHeader << "Loaded " << tofPairs.size() << " TOF pairs from input file.\n";
		stop_scan();
	}

	return true;
}

void timingScanner::ProcessTimeDifferences(){
	double mean = 0.0;
	double stddev = 0.0;
	unsigned long nGood = 0;
	for(std::vector<double>::iterator iter = tdiffs.begin(); iter != tdiffs.end(); ++iter){
		if(std::isnan(*iter)) continue;
		mean += *iter;
		nGood++;
	}
	mean = mean / nGood;
	for(std::vector<double>::iterator iter = tdiffs.begin(); iter != tdiffs.end(); ++iter){
		if(std::isnan(*iter)) continue;
		stddev += std::pow(*iter-mean, 2.0);
	}
	stddev = std::sqrt(stddev/(nGood-1));
	std::cout << msgHeader << " Mean Tdiff = " << mean << std::endl;
	std::cout << msgHeader << " Std. Dev. = " << stddev << std::endl;
}

void timingScanner::ClearAll(){
	while(!unsorted.empty()){
		delete unsorted.front();
		unsorted.pop_front();
	}
	while(!tofPairs.empty()){
		ChanPair *pair = &tofPairs.front();
		delete pair->start;
		delete pair->stop;
		tofPairs.pop_front();
	}
	tdiffs.clear();
}

int main(int argc, char *argv[]){
	// Define a new unpacker object.
	timingScanner scanner;
	
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
