#include <iostream>
#include <algorithm> 

// PixieCore libraries
#include "XiaData.hpp"
#include "TraceFitter.hpp"

// Local files
#include "parameter.hpp"
#include "timing.hpp"

// Root files
#include "TApplication.h"
#include "TSystem.h"
#include "TCanvas.h"

// Define the name of the program.
#ifndef PROG_NAME
#define PROG_NAME "Timing"
#endif

#define SLEEP_WAIT 1E4 // When not in shared memory mode, length of time to wait after gSystem->ProcessEvents is called (in us).

///////////////////////////////////////////////////////////////////////////////
// class timingUnpacker
///////////////////////////////////////////////////////////////////////////////

/// Default constructor.
timingUnpacker::timingUnpacker() : Unpacker() {
}

/** Return a pointer to a new XiaData channel event.
  * \return A pointer to a new XiaData.
  */
XiaData *timingUnpacker::GetNewEvent(){ 
	return (XiaData*)(new ChannelEvent()); 
}

/** Process all events in the event list.
  * \param[in]  addr_ Pointer to a location in memory. 
  * \return Nothing.
  */
void timingUnpacker::ProcessRawEvent(ScanInterface *addr_/*=NULL*/){
	if(!addr_){ return; }

	XiaData *current_event = NULL;

	// Fill the processor event deques with events
	while(!rawEvent.empty()){
		if(!running)
			break;
	
		//Get the first event int he FIFO.
		current_event = rawEvent.front();
		rawEvent.pop_front();

		// Safety catches for null event.
		if(!current_event) continue;

		//Store the waveform in the stack of waveforms to be displayed.
		if(addr_->AddEvent(current_event)){
			addr_->ProcessEvents();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// class timingScanner
///////////////////////////////////////////////////////////////////////////////

/// Default constructor.
timingScanner::timingScanner() : ScanInterface() {
	acqRun_ = true;
	singleCapture_ = false;
	init = false;
	running = true;
	
	for(size_t i = 0; i < 4; i++){
		mod[i] = 0;
		chan[i] = i;
		events[i] = NULL;
		tOffsets[i] = 0;
	}
	
	delay = 1; /// The number of seconds to wait between drawing traces.

	// Variables for root graphics
	rootapp = new TApplication("timing", 0, NULL);
	canvas = new TCanvas("timing_canvas", "timingScanner");
	
	coincidenceSelect = 0;
}

/// Destructor.
timingScanner::~timingScanner(){
	canvas->Close();
	delete canvas;
}

/** Initialize the map file, the config file, the processor handler, 
  * and add all of the required processors.
  * \param[in]  prefix_ String to append to the beginning of system output.
  * \return True upon successfully initializing and false otherwise.
  */
bool timingScanner::Initialize(std::string prefix_){
	if(init){ return false; }

	// Print a small welcome message.
	//std::cout << "  Displaying traces for mod = " << mod_ << ", chan = " << chan_ << ".\n";

	return (init = true);
}

/** Receive various status notifications from the scan.
  * \param[in] code_ The notification code passed from ScanInterface methods.
  * \return Nothing.
  */
void timingScanner::Notify(const std::string &code_/*=""*/){
	if(code_ == "START_SCAN"){ 
		acqRun_ = true; 
	}
	else if(code_ == "STOP_SCAN"){ 
		acqRun_ = false; 
	}
	else if(code_ == "SCAN_COMPLETE"){ 
		std::cout << msgHeader << "Scan complete.\n"; 
		ProcessEvents(); // Process whatever is left in the deque.
	}
	else if(code_ == "LOAD_FILE"){ std::cout << msgHeader << "File loaded.\n"; }
	else if(code_ == "REWIND_FILE"){  }
	else if(code_ == "RESTART"){ ClearEvents(); }
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
  * This method should only be called from skeletonUnpacker::ProcessRawEvent().
  * \param[in]  event_ The raw XiaData to add to the channel event deque.
  * \return True if events are ready to be processed and false otherwise.
  */
bool timingScanner::AddEvent(XiaData *event_){
	if(!event_){ return false; }

	//Check for invalid channel.
	bool validChannel = false;
	for(size_t i = 0; i < 4; i++){
		if(event_->modNum == mod[i] && event_->chanNum == chan[i]){  
			validChannel = true;
			
			//Check for empty trace.
			if(event_->traceLength == 0){
				std::cout << msgHeader << "Warning! Trace capture is not enabled for this channel!\n";
				stop_scan();
				delete event_;
				return false;
			}

			//Get the first event int the FIFO.
			ChannelEvent *channel_event = new ChannelEvent(event_);

			//Process the waveform.
			channel_event->ComputeBaseline();
			channel_event->IntegratePulse();
	
			//Push the channel event into the deque.
			//chanEvents[i].push_back(channel_event);
			events[i] = channel_event;
			
			break;
		}
	}

	// Check for invalid channels.
	if(!validChannel){  
		delete event_;
		return false;
	}

	// Check for coincidences.
	bool validTrigger = false;
	if(coincidenceSelect == 0){ // Singles
		validTrigger = (events[0] || events[1] || events[2] || events[3]);
	}
	else if(coincidenceSelect == 1){ // Doubles
		validTrigger = ((events[0] && events[1]) || (events[2] && events[3]));
	}
	else if(coincidenceSelect == 2){ // Triples
		validTrigger = ((events[0] || events[1]) && (events[2] && events[3]));
	}	
	else if(coincidenceSelect == 3){ // Quads
		validTrigger = ((events[0] && events[1]) && (events[2] && events[3]));
	}
	
	return validTrigger;
}

/** Process all channel events read in from the rawEvent.
  * This method should only be called from skeletonUnpacker::ProcessRawEvent().
  * \return True if events were processed and false otherwise.
  */
bool timingScanner::ProcessEvents(){
	// Find the first time.
	double firstTime = std::pow(2, 48);
	bool channels[4] = {false, false, false, false};
	for(size_t i = 0; i < 4; i++){
		//if(chanEvents[i].empty()) continue;
		//events[i] = chanEvents[i].front();
		if(!events[i]) continue;
		channels[i] = true;
		if(events[i]->time < firstTime) 
			firstTime = events[i]->time;
		//chanEvents[i].pop_front();
	}
	
	// Compute time offsets.
	for(size_t i = 0; i < 4; i++){
		if(!channels[i]) continue;
		tOffsets[i] = events[i]->time-firstTime;
		if(tOffsets[i] >= 1000){
			channels[i] = false;
		}
	}

	// Check again for coincidences.
	bool validTrigger = false;
	if(coincidenceSelect == 0){ // Singles
		validTrigger = (channels[0] || channels[1] || channels[2] || channels[3]);
	}
	else if(coincidenceSelect == 1){ // Doubles
		validTrigger = ((channels[0] && channels[1]) || (channels[2] && channels[3]));
	}
	else if(coincidenceSelect == 2){ // Triples
		validTrigger = ((channels[0] || channels[1]) && (channels[2] && channels[3]));
	}	
	else if(coincidenceSelect == 3){ // Quads
		validTrigger = ((channels[0] && channels[1]) && (channels[2] && channels[3]));
	}
	
	if(!validTrigger){
		ClearEvents();
		return false;
	}

	//Check if we have delayed the plotting enough
	time_t cur_time;
	time(&cur_time);
	while(difftime(cur_time, last_trace) < delay) {
		//If in shm mode and the plotting time has not alloted the events are cleared and this function is aborted.
		if (ShmMode()) {
			ClearEvents();
			return false;
		}
		else {
			IdleTask();
			time(&cur_time);
		}
	}

	//When we have the correct number of waveforms we plot them.
	this->Process();
	
	//If this is a single capture we stop the plotting.
	if (singleCapture_) running = false;

	//Update the time.
	time(&last_trace);
	
	// Clear the deque.
	ClearEvents();
	
	return true;
}

void timingScanner::ClearEvents(){
	for(size_t i = 0; i < 4; i++){
		if(events[i])
			delete events[i];
		events[i] = NULL;
		tOffsets[i] = 0;
		while(!chanEvents[i].empty()){
			delete chanEvents[i].front();
			chanEvents[i].pop_front();
		}
	}
}

/** CmdHelp is used to allow a derived class to print a help statement about
  * its own commands. This method is called whenever the user enters 'help'
  * or 'h' into the interactive terminal (if available).
  * \param[in]  prefix_ String to append at the start of any output. Not used by default.
  * \return Nothing.
  */
void timingScanner::CmdHelp(const std::string &prefix_/*=""*/){
	std::cout << "   set <start|stop> <mod1> <chan1> [mod2] [chan2] - Set the module and channel of signals of interest.\n";
	std::cout << "   bitrange <Nbits>                               - Set the dynamic range of the ADC (in bits).\n";
	std::cout << "   trigger [scheme]                               - Set the triggering scheme.\n";
	std::cout << "   single                                         - Perform a single capture.\n";
	std::cout << "   delay [time]                                   - Set the delay between drawing traces (in seconds, default = 1 s).\n";
	std::cout << "   clear                                          - Clear all stored traces and start over.\n";
}

/** ArgHelp is used to allow a derived class to add a command line option
  * to the main list of options. This method is called at the end of
  * from the ::Setup method.
  * Does nothing useful by default.
  * \return Nothing.
  */
void timingScanner::ArgHelp(){
	AddOption(optionExt("mod", required_argument, NULL, 'm', "<module>", "Module of signal of interest (default=0)"));
	AddOption(optionExt("chan", required_argument, NULL, 'c', "<channel>", "Channel of signal of interest (default=0)"));
	AddOption(optionExt("bit-range", required_argument, NULL, 'B', "<Nbits>", "Set the dynamic range of the ADC (default=12)"));
	AddOption(optionExt("trigger", required_argument, NULL, 'T', "<scheme>", "Set the triggering scheme (default=0, singles)"));
}

/** SyntaxStr is used to print a linux style usage message to the screen.
  * \param[in]  name_ The name of the program.
  * \return Nothing.
  */
void timingScanner::SyntaxStr(char *name_){ 
	std::cout << " usage: " << std::string(name_) << " [options]\n"; 
}

/** ExtraArguments is used to send command line arguments to classes derived
  * from ScanInterface. This method should loop over the optionExt elements
  * in the vector userOpts and check for those options which have been flagged
  * as active by ::Setup(). This should be overloaded in the derived class.
  * \return Nothing.
  */
void timingScanner::ExtraArguments(){
	/*if(userOpts.at(0).active)
		std::cout << msgHeader << "Set module to (" << (mod_ = atoi(userOpts.at(0).argument.c_str())) << ").\n";
	if(userOpts.at(1).active)
		std::cout << msgHeader << "Set channel to (" << (chan_ = atoi(userOpts.at(1).argument.c_str())) << ").\n";*/
	if(userOpts.at(2).active)
		timing.setAdcBitRange(strtol(userOpts.at(2).argument.c_str(), NULL, 10));
	if(userOpts.at(3).active)
		coincidenceSelect = strtol(userOpts.at(3).argument.c_str(), NULL, 10);
}

/** ExtraCommands is used to send command strings to classes derived
  * from ScanInterface. If ScanInterface receives an unrecognized
  * command from the user, it will pass it on to the derived class.
  * \param[in]  cmd_ The command to interpret.
  * \param[out] arg_ Vector or arguments to the user command.
  * \return True if the command was recognized and false otherwise.
  */
bool timingScanner::ExtraCommands(const std::string &cmd_, std::vector<std::string> &args_){
	if(cmd_ == "set"){ // Toggle debug mode
		if(args_.size() == 3){
			if(args_.at(0) == "start"){
				// Clear all events from the event deque.
				ClearEvents();

				// Set the module and channel.
				mod[0] = atoi(args_.at(1).c_str());
				chan[0] = atoi(args_.at(2).c_str());
				if(args_.size() >= 5){ // Pairwise channels.
					mod[1] = atoi(args_.at(3).c_str());
					chan[1] = atoi(args_.at(4).c_str());					
				}
			}
			else if(args_.at(0) == "stop"){
				// Clear all events from the event deque.
				ClearEvents();	
				
				// Set the module and channel.
				mod[2] = atoi(args_.at(1).c_str());
				chan[2] = atoi(args_.at(2).c_str());
				if(args_.size() >= 5){ // Pairwise channels.
					mod[3] = atoi(args_.at(3).c_str());
					chan[3] = atoi(args_.at(4).c_str());					
				}		
			}
			else{
				std::cout << msgHeader << "Invalid type specifier (" << args_.at(0) << ")\n";
				std::cout << msgHeader << " -SYNTAX- set <start|stop> <mod1> <chan1> [mod2] [chan2]\n";
			}
		}
		else{
			std::cout << msgHeader << "Invalid number of parameters to 'set'\n";
			std::cout << msgHeader << " -SYNTAX- set <start|stop> <mod1> <chan1> [mod2] [chan2]\n";
		}
	}
	else if(cmd_ == "bitrange") {
		if(args_.size() == 1){ timing.setAdcBitRange(strtol(args_.at(0).c_str(), NULL, 10)); }
		else{
			std::cout << msgHeader << "Invalid number of parameters to 'bitrange'\n";
			std::cout << msgHeader << " -SYNTAX- bitrange <Nbits>\n";
		}
	}
	else if(cmd_ == "trigger") {
		if(args_.size() == 1){ SetCoincidence(strtol(args_.at(0).c_str(), NULL, 10)); }
		else{
			std::cout << msgHeader << "Set the triggering scheme for the system:\n";
			std::cout << msgHeader << " 0 - Singles (requires 1 stop or 1 start)\n";
			std::cout << msgHeader << " 1 - Doubles (requires 2 stops or 2 starts)\n";
			std::cout << msgHeader << " 2 - Triples (requires 2 stops and 1 start)\n";
			std::cout << msgHeader << " 3 - Quads (requires 2 stops and 2 starts)\n";
			std::cout << msgHeader << "Current triggering scheme is (" << coincidenceSelect << ").\n";
		}
	}
	else if(cmd_ == "single") {
		singleCapture_ = !singleCapture_;
	}
	else if(cmd_ == "delay"){
		if(args_.size() == 1){ delay = atoi(args_.at(0).c_str()); }
		else{
			std::cout << msgHeader << "Invalid number of parameters to 'delay'\n";
			std::cout << msgHeader << " -SYNTAX- delay <time>\n";
		}
	}
	else if(cmd_ == "clear"){
		ClearEvents();
		std::cout << msgHeader << "Event deque cleared.\n";
	}
	else{ return false; }

	return true;
}

/** IdleTask is called whenever a scan is running in shared
  * memory mode, and a spill has yet to be received. This method may
  * be used to update things which need to be updated every so often
  * (e.g. a root TCanvas) when working with a low data rate. 
  * \return Nothing.
  */
void timingScanner::IdleTask(){
	gSystem->ProcessEvents();
	usleep(SLEEP_WAIT);
}

/// Process a single event.
void timingScanner::Process(){
	timing.clear();

	for(size_t i = 0; i < 4; i++){
		if(!events[i]) continue;
		timing.setWaveform(i/2, i%2, events[i], tOffsets[i]);
	}
	
	timing.update(canvas);
}

/// Set triggering coincidence scheme.
bool timingScanner::SetCoincidence(const int &scheme){
	if(scheme < 0 || scheme >= 4) return false;
	coincidenceSelect = scheme;
	timing.setCoincidence(scheme);
	return true;
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
