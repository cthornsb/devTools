#include <iostream>

#include <unistd.h>
#include <getopt.h>
#include <cstring>

#include "XiaData.hpp"

// Local files
#include "eventReader.hpp"

// Root
#include "TApplication.h"
#include "TCanvas.h"
#include "TGraph.h"
#include "TAxis.h"

// Define the name of the program.
#ifndef PROG_NAME
#define PROG_NAME "EventReader"
#endif

unsigned int rawEventsRead = 0;

void displayBool(const char *msg_, const bool &val_){
	if(val_) std::cout << msg_ << "YES\n";
	else std::cout << msg_ << "NO\n";
}

///////////////////////////////////////////////////////////////////////////////
// class readerUnpacker
///////////////////////////////////////////////////////////////////////////////

/** Process all events in the event list.
  * \param[in]  addr_ Pointer to a location in memory. 
  * \return Nothing.
  */
void readerUnpacker::ProcessRawEvent(ScanInterface *addr_/*=NULL*/){
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

	// Increment the number of raw events processed so far.
	rawEventsRead++;
}

///////////////////////////////////////////////////////////////////////////////
// class readerScanner
///////////////////////////////////////////////////////////////////////////////

/// Default constructor.
readerScanner::readerScanner() : ScanInterface(), init(false), showFlags(false), showTrace(false), showNextEvent(false), numSkip(0), eventsRead(0) {
	canvas = NULL;
	graph = new TGraph(1);
	currentEvent = new XiaData();
}

/// Destructor.
readerScanner::~readerScanner(){
	if(init){ // Handle some cleanup.
	}

	if(canvas){
		canvas->Close();
		delete canvas;
	}
	delete graph;
	delete currentEvent;
}

/** ExtraCommands is used to send command strings to classes derived
  * from ScanInterface. If ScanInterface receives an unrecognized
  * command from the user, it will pass it on to the derived class.
  * \param[in]  cmd_ The command to interpret.
  * \param[out] arg_ Vector or arguments to the user command.
  * \return True if the command was recognized and false otherwise.
  */
bool readerScanner::ExtraCommands(const std::string &cmd_, std::vector<std::string> &args_){
	if(cmd_ == "flags"){
		showFlags = !showFlags;
	}
	else if(cmd_ == "trace"){
		showTrace = !showTrace;
	}
	else if(cmd_ == "draw"){
		draw_current_trace();
	}
	else if(cmd_ == "next" || cmd_ == "n"){
		if(args_.size() >= 1){ // Skip the specified number of events.
			showNextEvent = true;
			numSkip = strtoul(args_.at(0).c_str(), NULL, 0);
			std::cout << msgHeader << "Skipping " << numSkip << " events.\n";
		}
		else{ showNextEvent = true; }
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
void readerScanner::ExtraArguments(){
	if(userOpts.at(0).active){
		numSkip = strtoul(userOpts.at(0).argument.c_str(), NULL, 0);
		std::cout << msgHeader << "Skipping " << numSkip << " events.\n";		
	}
	/*if(userOpts.at(1).active)
		std::cout << msgHeader << "Using option --myarg2 (-y): arg=\"" << userOpts.at(1).argument << "\"\n";
	if(userOpts.at(2).active)
		std::cout << msgHeader << "Using option --myarg3 (-z): arg=\"" << userOpts.at(2).argument << "\"\n";
	if(userOpts.at(3).active)
		std::cout << msgHeader << "Using option --myarg4.\n";*/
}

/** CmdHelp is used to allow a derived class to print a help statement about
  * its own commands. This method is called whenever the user enters 'help'
  * or 'h' into the interactive terminal (if available).
  * \param[in]  prefix_ String to append at the start of any output. Not used by default.
  * \return Nothing.
  */
void readerScanner::CmdHelp(const std::string &prefix_/*=""*/){
	std::cout << "   flags               - Display channel flags.\n";
	std::cout << "   trace               - Print adc trace values.\n";
	std::cout << "   draw                - Draw the adc trace to the screen.\n";
	std::cout << "   next (n) [N=0]      - Skip N events and display the next one.\n";
}

/** ArgHelp is used to allow a derived class to add a command line option
  * to the main list of options. This method is called at the end of
  * from the ::Setup method.
  * Does nothing useful by default.
  * \return Nothing.
  */
void readerScanner::ArgHelp(){
	AddOption(optionExt("skip", required_argument, NULL, 'S', "<N>", "Skip the first N events in the input file."));
	/*AddOption(optionExt("myarg2", required_argument, NULL, 'y', "<arg>", "A useful command line argument with a required argument."));
	AddOption(optionExt("myarg3", optional_argument, NULL, 'z', "[arg]", "A useful command line argument with an optional argument."));
	AddOption(optionExt("myarg4", no_argument, NULL, 0, "", "A long only command line argument."));*/
	
	// Note that the following single character options are reserved by ScanInterface
	//  b, h, i, o, q, s, and v
}

/** SyntaxStr is used to print a linux style usage message to the screen.
  * \param[in]  name_ The name of the program.
  * \return Nothing.
  */
void readerScanner::SyntaxStr(char *name_){ 
	std::cout << " usage: " << std::string(name_) << " [options]\n"; 
}

/** Initialize the map file, the config file, the processor handler, 
  * and add all of the required processors.
  * \param[in]  prefix_ String to append to the beginning of system output.
  * \return True upon successfully initializing and false otherwise.
  */
bool readerScanner::Initialize(std::string prefix_){
	if(init){ return false; }

	// Do some initialization.

	return (init = true);
}

/** Peform any last minute initialization before processing data. 
  * /return Nothing.
  */
void readerScanner::FinalInitialization(){
	// Do some last minute initialization before the run starts.
}

/** Receive various status notifications from the scan.
  * \param[in] code_ The notification code passed from ScanInterface methods.
  * \return Nothing.
  */
void readerScanner::Notify(const std::string &code_/*=""*/){
	if(code_ == "START_SCAN"){  }
	else if(code_ == "STOP_SCAN"){  }
	else if(code_ == "SCAN_COMPLETE"){ std::cout << msgHeader << "Scan complete.\n"; }
	else if(code_ == "LOAD_FILE"){ std::cout << msgHeader << "File loaded.\n"; }
	else if(code_ == "REWIND_FILE"){  }
	else{ std::cout << msgHeader << "Unknown notification code '" << code_ << "'!\n"; }
}

/** Return a pointer to the Unpacker object to use for data unpacking.
  * If no object has been initialized, create a new one.
  * \return Pointer to an Unpacker object.
  */
Unpacker *readerScanner::GetCore(){ 
	if(!core){ core = (Unpacker*)(new readerUnpacker()); }
	return core;
}

/** Add a channel event to the deque of events to send to the processors.
  * This method should only be called from readerUnpacker::ProcessRawEvent().
  * \param[in]  event_ The raw XiaData to add to the channel event deque.
  * \return False.
  */
bool readerScanner::AddEvent(XiaData *event_){
	if(!event_){ return false; }

	//GetEnergySums() - vector<unsigned int>
	//GetQdc() - vector<unsigned int>
	//GetTrace() - vector<unsigned int>

	while(!showNextEvent){
		if(!core->IsRunning()) return false;
		sleep(1);
	}

	if(numSkip == 0){
		std::cout << "*************************************************\n";
		std::cout << "** Channel Event no. " << eventsRead << std::endl;
		std::cout << "**  Raw Event no. " << rawEventsRead << std::endl;
		std::cout << "*************************************************\n";
		
		/*std::cout << " Filter Energy: " << event_->GetEnergy() << std::endl;
		std::cout << " Trigger Time:  " << (unsigned long long)event_->GetTime() << std::endl;
		std::cout << " Crate:         " << event_->GetCrateNumber() << std::endl;
		std::cout << " Slot:          " << event_->GetSlotNumber() << std::endl;
		std::cout << " Module:        " << event_->GetModuleNumber() << std::endl;
		std::cout << " Channel:       " << event_->GetChannelNumber() << std::endl;
		std::cout << " CFD Time:      " << event_->GetCfdFractionalTime() << std::endl;
		std::cout << " Trace Length:  " << event_->GetTrace().size() << std::endl;

		if(showFlags){
			displayBool(" Virtual:       ", event_->IsVirtualChannel());
			displayBool(" Pileup:        ", event_->IsPileup());
			displayBool(" Saturated:     ", event_->IsSaturated());
			displayBool(" CFD Force:     ", event_->GetCfdForcedTriggerBit());
			displayBool(" CFD Trig:      ", event_->GetCfdTriggerSourceBit());
		}

		if(showTrace && !event_->GetTrace().empty()){
			int numLine = 0;
			std::cout << " Trace:\n  ";
			for(size_t i = 0; i < event_->GetTrace().size(); i++){
				std::cout << event_->GetTrace().at(i) << "\t";
				if(++numLine % 10 == 0) std::cout << "\n  ";
			}
			std::cout << std::endl;
		}*/
		
		std::cout << " Filter Energy: " << event_->energy << std::endl;
		std::cout << " Trigger Time:  " << (unsigned long long)event_->time << std::endl;
		std::cout << " Event Time Lo: " << event_->eventTimeLo << std::endl;
		std::cout << " Event Time Hi: " << event_->eventTimeHi << std::endl;
		std::cout << " Crate:         " << event_->crateNum << std::endl;
		std::cout << " Slot:          " << event_->slotNum << std::endl;
		std::cout << " Module:        " << event_->modNum << std::endl;
		std::cout << " Channel:       " << event_->chanNum << std::endl;
		std::cout << " CFD Time:      " << event_->cfdTime << std::endl;
		std::cout << " Trace Length:  " << event_->traceLength << std::endl;

		if(showFlags){
			displayBool(" Virtual:       ", event_->virtualChannel);
			displayBool(" Pileup:        ", event_->pileupBit);
			displayBool(" Saturated:     ", event_->saturatedBit);
			displayBool(" CFD Force:     ", event_->cfdForceTrig);
			displayBool(" CFD Trig:      ", event_->cfdTrigSource);
		}

		if(event_->traceLength != 0 && showTrace){
			int numLine = 0;
			std::cout << " Trace:\n  ";
			for(size_t i = 0; i < event_->traceLength; i++){
				std::cout << event_->adcTrace[i] << "\t";
				if(++numLine % 10 == 0) std::cout << "\n  ";
			}
			std::cout << std::endl;
		}
    	
    		std::cout << std::endl;
    	
		showNextEvent = false;
	}
	else{ numSkip--; }
	
	eventsRead++;
	delete currentEvent;
	currentEvent = event_;
	
	return true;
}

/** Process all channel events read in from the rawEvent.
  * This method should only be called from readerUnpacker::ProcessRawEvent().
  * \return True.
  */
bool readerScanner::ProcessEvents(){
	// Process all of the events added so far.
	return true;
}

void readerScanner::init_graphics(){
	std::cout << msgHeader << "Initializing root graphics.\n";
	canvas = new TCanvas("canvas", "eventReader");
}

void readerScanner::draw_current_trace(){
	if(currentEvent->traceLength == 0) return;
	else if(currentEvent->traceLength != (unsigned int)graph->GetN()){
		std::cout << msgHeader << "Changing trace length from " << graph->GetN() << " to " << currentEvent->traceLength << ".\n";
		delete graph;
		graph = new TGraph(currentEvent->traceLength);
		graph->SetMarkerStyle(kFullDotSmall);
		graph->SetTitle("adcTrace");
		graph->GetXaxis()->SetTitle("Time (ns)");
	}
	for(size_t i = 0; i < currentEvent->traceLength; i++){
		graph->SetPoint(i, i*4, currentEvent->adcTrace[i]);
	}
	if(!canvas) init_graphics();
	canvas->cd();
	graph->Draw("AP");
	canvas->Update();
}

int main(int argc, char *argv[]){
	// Initialize root graphics
	TApplication *rootapp = new TApplication("rootapp", 0, NULL);

	// This is done to keep the compiler from complaining about
	// unused TApplication variable.
	rootapp->SetBit(0, false);

	// Define a new unpacker object.
	readerScanner scanner;
	
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
