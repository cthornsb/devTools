#include <iostream>
#include <fstream>

#include <unistd.h>
#include <getopt.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <algorithm>

#include "TFile.h"
#include "TTree.h"

#include "TF1.h"

#include "CTerminal.h"
#include "XiaData.hpp"

// Local files
#include "timingAnalyzer.hpp"

// Define the name of the program.
#ifndef PROG_NAME
#define PROG_NAME "TimingAnalyzer"
#endif

#define ADC_TIME_STEP 4 // ns

void displayBool(const char *msg_, const bool &val_){
	if(val_) std::cout << msg_ << "YES\n";
	else std::cout << msg_ << "NO\n";
}

typedef std::chrono::high_resolution_clock::time_point hr_time;
typedef std::chrono::high_resolution_clock hr_clock;

const double fwhmCoeff = 2*std::sqrt(2*std::log(2));

bool floatingMode = false;

///////////////////////////////////////////////////////////////////////////////
// class ChanPair
///////////////////////////////////////////////////////////////////////////////

ChanPair::~ChanPair(){
}

bool ChanPair::Analyze(double &tdiff, timingAnalyzer analyzer/*=POLY*/, const float &par1_/*=0.5*/, const float &par2_/*=1*/, const float &par3_/*=1*/, TraceFitter *fitter/*=NULL*/){
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
	else if(analyzer == FIT && fitter != NULL){
		fitter->FitPulse(start);
		rchi2[0] = fitter->GetFunction()->GetChisquare()/fitter->GetFunction()->GetNDF();
		if(floatingMode){
			beta[0] = fitter->GetFunction()->GetParameter(3);
			gamma[0] = fitter->GetFunction()->GetParameter(4);
		}
		fitter->FitPulse(stop);
		rchi2[1] = fitter->GetFunction()->GetChisquare()/fitter->GetFunction()->GetNDF();
		if(floatingMode){
			beta[1] = fitter->GetFunction()->GetParameter(3);
			gamma[1] = fitter->GetFunction()->GetParameter(4);
		}
	}

	// Stop the timer.
	hr_time stop_time = hr_clock::now();

	// Calculate the time taken to analyze the trace.
	std::chrono::duration<double> time_span;
	time_span = std::chrono::duration_cast<std::chrono::duration<double> >(stop_time - start_time); // Time between packets in seconds
	timeTaken = time_span.count();
	
	// Calculate the time difference.
	tdiff = (stop->time*8 + stop->phase*4) - (start->time*8 + start->phase*4);

	// Check the validity of the output.
	if(start->phase < 0 || stop->phase < 0) return false;

	return true;
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
timingScanner::timingScanner() : ScanInterface(), minimumTraces(5000), startID(0), stopID(1), par1(0.5), par2(1), par3(1), fitRangeLow(-5), fitRangeHigh(10), startThresh(0), stopThresh(0), analyzer(POLY), fitter() {
	// Set the fit function x-axis multiplier to the ADC tick size.
	fitter.SetAxisMultiplier(ADC_TIME_STEP);
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
		double tdiff;
		tdiffs.clear();
		if(analyzer == FIT){ // Set fit function beta and gamma.
			fitter.SetFitRange(fitRangeLow, fitRangeHigh);
			fitter.SetBetaGamma(par1, par2);
		}
		for(std::deque<ChanPair>::iterator iter = tofPairs.begin(); iter != tofPairs.end(); ++iter){
			if(iter->Analyze(tdiff, analyzer, par1, par2, par3, &fitter)){
				tdiffs.push_back(tdiff);
				totalTime += iter->timeTaken;
			}
		}
		std::cout << msgHeader << "Total time taken = " << totalTime*(1E6) << " us for " << tdiffs.size() << " traces\n";
		std::cout << msgHeader << " Average time per trace = " << totalTime*(1E6)/(2*tdiffs.size()) << " us\n";
		ProcessTimeDifferences();
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
	else if(cmd_ == "write"){
		std::string ofname = "timing.dat";
		if(args_.size() >= 1) ofname = args_.at(0);
		if(Write(ofname.c_str())) // Write the output file.
			std::cout << msgHeader << "Wrote time differences to file \"" << ofname << "\".\n";
		else
			std::cout << msgHeader << "Error! Failed to open file \"" << ofname << "\" for writing.\n";
	}
	else if(cmd_ == "range"){
		if(args_.size() >= 2){
			fitRangeLow = strtol(args_.at(0).c_str(), NULL, 0);
			fitRangeHigh = strtol(args_.at(1).c_str(), NULL, 0);
		}
		std::cout << msgHeader << "Using fitting range of [maxIndex+" << fitRangeLow << ", maxIndex+" << fitRangeHigh << "].\n";
	}
	else if(cmd_ == "thresh"){
		if(args_.size() >= 1){
			startThresh = strtod(args_.at(0).c_str(), NULL);
			if(args_.size() >= 2)
				stopThresh = strtod(args_.at(1).c_str(), NULL);
			else
				stopThresh = startThresh;
		}
		std::cout << msgHeader << "Using following thresholds, start=" << startThresh << ", stop=" << stopThresh << ".\n";
	}
	else if(cmd_ == "auto"){
		if(analyzer == FIT)
			std::cout << msgHeader << "Error! Unable to perform auto-analysis for fitting analyzer.\n";
		
		double startValue[3] = {0, 0, 0};
		double stopValue[3] = {0, 0, 0};
		double stepSize[3] = {0, 0, 0};
		int numSteps[3] = {1, 1, 1};

		std::string userInput, userArgs;
		std::vector<std::string> userSplitArgs;
		for(int i = 0; i < (analyzer == POLY ? 1 : 3); i++){
			while(true){
				userSplitArgs.clear();
				std::cout << msgHeader << "Enter par" << i+1 << " start stop and step size:\n"; 
				GetTerminal()->flush();
				userInput = GetTerminal()->GetCommand(userArgs);
				split_str(userArgs, userSplitArgs);
				if(userSplitArgs.size() < 2){
					std::cout << "Error! Invalid number of arguments. Expected 3, but received only " << userSplitArgs.size()+1 << ".\n";
					continue;
				}
				startValue[i] = strtod(userInput.c_str(), NULL); 
				stopValue[i] = strtod(userSplitArgs[0].c_str(), NULL); 
				stepSize[i] = strtod(userSplitArgs[1].c_str(), NULL);
				numSteps[i] = ((stopValue[i]-startValue[i])/stepSize[i] + 1);
				if((stopValue[i] != startValue[i]) && numSteps[i] == 1){
					std::cout << msgHeader << "How the hell does this happen???\n";
					numSteps[i]++; // What the hell???
				}
				break;
			}
		}

		std::string outputFilename = "timing.root";
		if(args_.size() >= 1)
			outputFilename = args_.at(0);
	
		std::cout << msgHeader << "Running auto-analysis for F=" << startValue[0] << " to " << stopValue[0] << " (stepSize=" << stepSize[0] << ").\n";
		if(analyzer == CFD){
			std::cout << msgHeader << "Running auto-analysis for D=" << startValue[1] << " to " << stopValue[1] << " (stepSize=" << stepSize[1] << ").\n";
			std::cout << msgHeader << "Running auto-analysis for L=" << startValue[2] << " to " << stopValue[2] << " (stepSize=" << stepSize[2] << ").\n";
		}

		TFile *ofile = new TFile(outputFilename.c_str(), "RECREATE");
		TTree *otree = new TTree("data", "Timing analyzer output tree");

		if(!ofile->IsOpen())
			std::cout << msgHeader << "Error! Failed to open output root file \"" << outputFilename << "\".\n";

		double timeStart, timeStop, tdiff;
		float phaseStart, phaseStop;
		float tqdcStart, tqdcStop;
		int iteration = 0;

		otree->Branch("timeStart", &timeStart);
		otree->Branch("timeStop", &timeStop);
		otree->Branch("tdiff", &tdiff);
		otree->Branch("phaseStart", &phaseStart);
		otree->Branch("phaseStop", &phaseStop);
		otree->Branch("tqdcStart", &tqdcStart);
		otree->Branch("tqdcStop", &tqdcStop);
		otree->Branch("par1", &par1);
		if(analyzer == CFD){
			otree->Branch("par2", &par2);
			otree->Branch("par3", &par3);	
		}
		otree->Branch("iter", &iteration);

		std::cout << msgHeader << "Processing... Please wait.\n";

		ChannelEvent *start, *stop;	
		for(int i = 0; i < numSteps[0]; i++){ // over par1
			for(int j = 0; j < numSteps[1]; j++){ // over par2
				for(int k = 0; k < numSteps[2]; k++){ // over par3
					par1 = startValue[0] + i*stepSize[0];
					if(analyzer == CFD){
						par2 = startValue[1] + j*stepSize[1];
						par3 = startValue[2] + k*stepSize[2];
					}
					for(std::deque<ChanPair>::iterator iter = tofPairs.begin(); iter != tofPairs.end(); ++iter){
						if(!iter->Analyze(tdiff, analyzer, par1, par2, par3)) continue;

						//if(tdiff < 0) tdiff *= -1;

						start = iter->start;
						stop = iter->stop;
						
						timeStart = start->time;
						timeStop = stop->time;
						phaseStart = start->phase;
						phaseStop = stop->phase;
						tqdcStart = start->qdc;
						tqdcStop = stop->qdc;
						
						otree->Fill();
					}
					iteration++;
				}
			}
		}

		ofile->cd();
		otree->Write();
		ofile->Close();
		delete ofile;

		std::cout << msgHeader << "Analysis complete!\n";
	}
	else if(cmd_ == "float"){
		floatingMode = !floatingMode;
		bool retval = fitter.SetFloatingMode(floatingMode);
		if(retval)
			std::cout << msgHeader << "Floating mode ON\n";
		else
			std::cout << msgHeader << "Floating mode OFF\n";
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
	std::cout << "   write [filename]                     - Write time differences to an output file.\n";
	std::cout << "   range [low] [high]                   - Set the range to use for fits [maxIndex-low, maxIndex+high].\n";
	std::cout << "   thresh [start] [stop]                - Set the minimum TQDC threshold to use (default=0).\n";
	std::cout << "   auto [fname]                         - Automatically vary par1 from start to stop.\n";
	std::cout << "   float                                - Toggle beta and gamma floating mode for pulse fitting.\n";
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

	unsigned short idL;
	unsigned short idR;

	std::vector<ChannelEvent*> lefts, rights;

	ChannelEvent *pair1, *pair2;
	
	sort(unsorted.begin(), unsorted.end(), &XiaData::compareTime);

	// Search for pixie channel pairs.	
	while(!unsorted.empty()){
		if(unsorted.size() <= 1){
			unsorted.pop_front();
			break;
		}

		pair1 = unsorted.front();
		idL = pair1->getID();

		// Find this event's neighbor.
		for(std::deque<ChannelEvent*>::iterator iter = unsorted.begin()+1; iter != unsorted.end(); ++iter){
			pair2 = (*iter);
			idR = pair2->getID();
			if(idL == startID){ // start
				if(idR == stopID){
					lefts.push_back(pair1);
					rights.push_back(pair2);
					unsorted.erase(iter);
					break;
				}
			}
			else{ // stop
				if(idL == startID){
					lefts.push_back(pair2);
					rights.push_back(pair1);
					unsorted.erase(iter);
					break;
				}
			}
		}

		unsorted.pop_front();
	}

	ChannelEvent *current_event_L;
	ChannelEvent *current_event_R;

	std::vector<ChannelEvent*>::iterator iterL = lefts.begin();
	std::vector<ChannelEvent*>::iterator iterR = rights.begin();

	// Pick out pairs of channels representing bars.
	for(; iterL != lefts.end() && iterR != rights.end(); ++iterL, ++iterR){
		current_event_L = (*iterL);
		current_event_R = (*iterR);

		// Check that the time and energy values are valid
		//if(!current_event_L->valid_chan || !current_event_R->valid_chan)
			//continue; 

		current_event_L->ComputeBaseline();
		current_event_R->ComputeBaseline();
		if(current_event_L->maximum >= startThresh && current_event_R->maximum >= stopThresh)
			tofPairs.push_back(ChanPair(current_event_L, current_event_R));
		current_event_L->IntegratePulse(current_event_L->max_index + fitRangeLow, current_event_L->max_index + fitRangeHigh);
		current_event_R->IntegratePulse(current_event_R->max_index + fitRangeLow, current_event_R->max_index + fitRangeHigh);
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
	std::cout << msgHeader << " Mean Tdiff = " << mean << " ns\n";
	std::cout << msgHeader << " Std. Dev. = " << stddev << " ns (" << fwhmCoeff*stddev << " ns fwhm)\n";
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

bool timingScanner::Write(const char *fname/*="timing.root"*/){
	TFile *ofile = new TFile(fname, "RECREATE");
	if(!ofile->IsOpen()){
		return false;
	}
	
	unsigned long long time[2];
	double tdiff;
	double chi2[2];
	double beta[2];
	double gamma[2];
	float phase[2];
	unsigned short maximum[2];
	
	TTree *otree = new TTree("data", "timingAnalyzer tree");
	otree->Branch("timeStart", &time[0]);
	otree->Branch("timeStop", &time[1]);
	otree->Branch("phaseStart", &phase[0]);
	otree->Branch("phaseStop", &phase[1]);
	otree->Branch("maxStart", &maximum[0]);
	otree->Branch("maxStop", &maximum[1]);
	if(analyzer == FIT){
		if(floatingMode){
			otree->Branch("betaStart", &beta[0]);
			otree->Branch("betaStop", &beta[1]);
			otree->Branch("gammaStart", &gamma[0]);
			otree->Branch("gammaStop", &gamma[1]);
		}
		otree->Branch("chi2Start", &chi2[0]);
		otree->Branch("chi2Stop", &chi2[1]);
	}
	otree->Branch("tdiff", &tdiff);

	ChannelEvent *start, *stop;
	for(std::deque<ChanPair>::iterator iter = tofPairs.begin(); iter != tofPairs.end(); ++iter){
		start = iter->start;
		stop = iter->stop;
		
		// Check for bad phases.
		if(start->phase < 0 || stop->phase < 0) continue;

		// Set all values.
		time[0] = (unsigned long long)start->time;
		time[1] = (unsigned long long)stop->time;
		phase[0] = start->phase;
		phase[1] = stop->phase;
		maximum[0] = start->max_ADC;
		maximum[1] = stop->max_ADC;
		tdiff = ((stop->time-start->time)*8+(stop->phase-start->phase)*4);
		if(analyzer == FIT){
			for(int i = 0; i < 2; i++){
				if(floatingMode){
					beta[i] = iter->beta[i];
					gamma[i] = iter->gamma[i];
				}
				chi2[i] = iter->rchi2[i];
			}
		}

		// Fill the tree.
		otree->Fill();
	}
	
	ofile->cd();
	otree->Write();
	ofile->Close();
	delete ofile;

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
