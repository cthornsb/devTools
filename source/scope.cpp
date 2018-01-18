#include <iostream>
#include <algorithm> 

// PixieCore libraries
#include "XiaData.hpp"
#include "TraceFitter.hpp"

// Local files
#include "scope.hpp"

// Root files
#include "TApplication.h"
#include "TSystem.h"
#include "TStyle.h"
#include "TMath.h"
#include "TCanvas.h"
#include "TGraph.h"
#include "TH2F.h"
#include "TAxis.h"
#include "TFile.h"
#include "TF1.h"
#include "TLine.h"
#include "TBox.h"
#include "TProfile.h"
#include "TPaveStats.h"

// Define the name of the program.
#ifndef PROG_NAME
#define PROG_NAME "Scope"
#endif

#define ADC_TIME_STEP 4 // ns
#define SLEEP_WAIT 1E4 // When not in shared memory mode, length of time to wait after gSystem->ProcessEvents is called (in us).

const double stdDevCoeff = 2.0 * std::sqrt(2.0 * std::log(2.0));

///////////////////////////////////////////////////////////////////////////////
// class scopeUnpacker
///////////////////////////////////////////////////////////////////////////////

/// Default constructor.
scopeUnpacker::scopeUnpacker(const unsigned int &mod/*=0*/, const unsigned int &chan/*=0*/) : Unpacker(){
}

/** Return a pointer to a new XiaData channel event.
  * \return A pointer to a new XiaData.
  */
XiaData *scopeUnpacker::GetNewEvent(){ 
	return (XiaData*)(new ChannelEvent()); 
}

/** Process all events in the event list.
  * \param[in]  addr_ Pointer to a location in memory. 
  * \return Nothing.
  */
void scopeUnpacker::ProcessRawEvent(ScanInterface *addr_/*=NULL*/){
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
// class scopeScanner
///////////////////////////////////////////////////////////////////////////////

/// Default constructor.
scopeScanner::scopeScanner(int mod /*= 0*/, int chan/*=0*/) : ScanInterface() {
	need_graph_update = false;
	resetGraph_ = false;
	acqRun_ = true;
	singleCapture_ = false;
	init = false;
	running = true;
	performFit_ = false;
	performCfd_ = false;
	tdiffMode_ = false;
	currTraceTime_ = 0;
	prevTraceTime_ = 0;
	numEvents = 20;
	numAvgWaveforms_ = 1;
	cfdF_ = 0.5;
	cfdD_ = 1;
	cfdL_ = 1;
	fitLow_ = 10;
	fitHigh_ = 15;
	delay_ = 2;
	num_displayed = 0;
	just_plotted = 0;
	time(&last_trace);

	mod_ = mod;
	chan_ = chan;
	threshLow_ = 0;
	threshHigh_ = -1;
	
	// Variables for root graphics
	rootapp = new TApplication("scope", 0, NULL);
	gSystem->Load("libTree");
	
	canvas = new TCanvas("scope_canvas", "scopeScanner");
	
	graph = new TGraph();
	cfdLine = new TLine();
	cfdLine->SetLineColor(kRed);
	cfdBox = new TBox();
	cfdBox->SetFillColor(kRed);
	cfdBox->SetFillStyle(3004);
	cfdPol3 = new TF1("cfdPol3", "pol3");
	cfdPol3->SetLineColor(kGreen+1);
	cfdPol2 = new TF1("cfdPol2", "pol2");
	cfdPol2->SetLineColor(kMagenta+1);
	
	hist = new TH2F("hist","",256,0,1,256,0,1);

	gStyle->SetPalette(51);
	
	//Display the stats: Integral
	gStyle->SetOptStat(1000000);
	
	//Display Fit Stats: Fit Values, Errors, and ChiSq.
	gStyle->SetOptFit(111);

	// Set beta and gamma to floating.
	fitter.SetFloatingMode(true);

	// Set the fit function x-axis multiplier to the ADC tick size.
	fitter.SetAxisMultiplier(ADC_TIME_STEP);
}

/// Destructor.
scopeScanner::~scopeScanner(){
	canvas->Close();
	delete canvas;
	delete graph;
	delete cfdLine;
	delete cfdPol3;
	delete cfdPol2;
	delete hist;
}

void scopeScanner::ResetGraph(unsigned int size) {
	delete graph;
		
	graph = new TGraph(size);
	graph->SetMarkerStyle(kFullDotSmall);
	
	if(size != x_vals.size()){
		std::cout << msgHeader << "Changing trace length from " << x_vals.size()*ADC_TIME_STEP << " to " << size*ADC_TIME_STEP << " ns.\n";
		x_vals.resize(size);
		for(size_t index = 0; index < x_vals.size(); index++)
			x_vals[index] = ADC_TIME_STEP * index;
	}
	hist->SetBins(x_vals.size(), x_vals.front(), x_vals.back() + ADC_TIME_STEP, 1, 0, 1);

	std::stringstream stream;
	stream << "M" << mod_ << "C" << chan_;
	graph->SetTitle(stream.str().c_str());
	hist->SetTitle(stream.str().c_str());

	resetGraph_ = false;
}

void scopeScanner::Plot(){
	if(chanEvents_.empty() || chanEvents_.size() < numAvgWaveforms_)
		return;

	bool plotAll = false;
	if(numAvgWaveforms_ == 0){ // Plot whatever we have.
		numAvgWaveforms_ = chanEvents_.size();
		std::cout << msgHeader << "Plotting " << numAvgWaveforms_ << " waveforms.\n";
		plotAll = true;
	}

	///The limits of the vertical axis
	static float axisVals[2][2]; //The max and min values of the graph, first index is the axis, second is the min / max
	static float userZoomVals[2][2];
	static bool userZoom[2];

	//Get the user zoom settings.
	userZoomVals[0][0] = canvas->GetUxmin();
	userZoomVals[0][1] = canvas->GetUxmax();
	userZoomVals[1][0] = canvas->GetUymin();
	userZoomVals[1][1] = canvas->GetUymax();

	if(chanEvents_.front()->traceLength != x_vals.size()){ // The length of the trace has changed.
		resetGraph_ = true;
	}
	if (resetGraph_) {
		ResetGraph(chanEvents_.front()->traceLength);
		for (int i=0;i<2;i++) {
			axisVals[i][0] = 1E9;
			axisVals[i][1] = -1E9;
			userZoomVals[i][0] = 1E9;
			userZoomVals[i][1] = -1E9;
			userZoom[i] = false;
		}
	}

	//Determine if the user had zoomed or unzoomed.
	for (int i=0; i<2; i++) {
		userZoom[i] =  (userZoomVals[i][0] != axisVals[i][0] || userZoomVals[i][1] != axisVals[i][1]);
	}

	//For a waveform pulse we use a graph.
	if (numAvgWaveforms_ == 1) {
		int index = 0;
		for (size_t i = 0; i < chanEvents_.front()->traceLength; ++i) {
			graph->SetPoint(index, x_vals[i], chanEvents_.front()->adcTrace[i]);
			index++;
		}

		//Get and set the updated graph limits.
		if (graph->GetXaxis()->GetXmin() < axisVals[0][0]) axisVals[0][0] = graph->GetXaxis()->GetXmin(); 
		if (graph->GetXaxis()->GetXmax() > axisVals[0][1]) axisVals[0][1] = graph->GetXaxis()->GetXmax(); 
		graph->GetXaxis()->SetLimits(axisVals[0][0], axisVals[0][1]);
		
		if (graph->GetYaxis()->GetXmin() < axisVals[1][0]) axisVals[1][0] = graph->GetYaxis()->GetXmin(); 
		if (graph->GetYaxis()->GetXmax() > axisVals[1][1]) axisVals[1][1] = graph->GetYaxis()->GetXmax(); 
		graph->GetYaxis()->SetLimits(axisVals[1][0], axisVals[1][1]);

		//Set the users zoom window.
		for (int i = 0; i < 2; i++) {
			if (!userZoom[i]) {
				for (int j = 0; j < 2; j++) userZoomVals[i][j] = axisVals[i][j];
			}
		}
		graph->GetXaxis()->SetRangeUser(userZoomVals[0][0], userZoomVals[0][1]);
		graph->GetYaxis()->SetRangeUser(userZoomVals[1][0], userZoomVals[1][1]);

		graph->Draw("AP0");

		if(performCfd_){
			ChannelEvent *evt = chanEvents_.front();

			// Find the zero-crossing of the cfd waveform.
			float cfdCrossing = evt->AnalyzeCFD(cfdF_);
			
			// Draw the cfd crossing line.
			cfdLine->DrawLine(cfdCrossing*ADC_TIME_STEP, userZoomVals[1][0], cfdCrossing*ADC_TIME_STEP, userZoomVals[1][1]);
			
			// Draw the 3rd order polynomial.
			cfdPol3->SetParameter(0, evt->cfdPar[0]);
			cfdPol3->SetParameter(1, evt->cfdPar[1]/ADC_TIME_STEP);
			cfdPol3->SetParameter(2, evt->cfdPar[2]/std::pow(ADC_TIME_STEP, 2.0));
			cfdPol3->SetParameter(3, evt->cfdPar[3]/std::pow(ADC_TIME_STEP, 3.0));
			// Find the pulse maximum by fitting with a third order polynomial.
			if(evt->adcTrace[evt->max_index-1] >= evt->adcTrace[evt->max_index+1]) // Favor the left side of the pulse.
				cfdPol3->SetRange((evt->max_index - 2)*ADC_TIME_STEP, (evt->max_index + 1)*ADC_TIME_STEP);
			else // Favor the right side of the pulse.
				cfdPol3->SetRange((evt->max_index - 1)*ADC_TIME_STEP, (evt->max_index + 2)*ADC_TIME_STEP);
			cfdPol3->Draw("SAME");
			
			// Draw the 2nd order polynomial.
			cfdPol2->SetParameter(0, evt->cfdPar[4]);
			cfdPol2->SetParameter(1, evt->cfdPar[5]/ADC_TIME_STEP);
			cfdPol2->SetParameter(2, evt->cfdPar[6]/std::pow(ADC_TIME_STEP, 2.0));
			cfdPol2->SetRange((evt->cfdIndex - 1)*ADC_TIME_STEP, (evt->cfdIndex + 1)*ADC_TIME_STEP);
			cfdPol2->Draw("SAME");

			if(tdiffMode_){
				currTraceTime_ = evt->time*8 + cfdCrossing*4;
				std::cout << " tdiff = " << currTraceTime_-prevTraceTime_ << " ns.\n";
				prevTraceTime_ = currTraceTime_;
			}
		}

		if(performFit_) fitter.FitPulse(graph, chanEvents_.front(), "QMER");
	}
	else { //For multiple events with make a 2D histogram and plot the profile on top.
		double cfdAvg = 0.0;
		double cfdStdDev = 0.0;
		int numCfdWaveforms = numAvgWaveforms_;

		//Determine the maximum and minimum values of the events.
		for (size_t i = 0; i < numAvgWaveforms_; i++) {
			ChannelEvent* evt = chanEvents_.at(i);
			float evtMin = *std::min_element(evt->adcTrace, evt->adcTrace+evt->traceLength);
			float evtMax = *std::max_element(evt->adcTrace, evt->adcTrace+evt->traceLength);
			evtMin -= fabs(0.1 * evtMax);
			evtMax += fabs(0.1 * evtMax);
			if (evtMin < axisVals[1][0]) axisVals[1][0] = evtMin;
			if (evtMax > axisVals[1][1]) axisVals[1][1] = evtMax;

			if(performCfd_){
				// Find the zero-crossing of the cfd waveform.
				float cfdCrossing = evt->AnalyzeCFD(cfdF_);
				if(cfdCrossing > 0){ // Handle failed CFD.
					cfdAvg += cfdCrossing;
					cfdStdDev += cfdCrossing * cfdCrossing;			
				}
				else{ numCfdWaveforms--; }
			}
		}

		if(performCfd_){
			// Calculate the average phase obtained from CFD.
			cfdAvg = cfdAvg / numCfdWaveforms;

			// Calculate the standard deviation of the phase distribution.
			cfdStdDev = std::sqrt(cfdStdDev / numCfdWaveforms - cfdAvg*cfdAvg);
			cfdStdDev *= stdDevCoeff; // Now FWHM!!!
		}

		//Set the users zoom window.
		for (int i=0; i<2; i++) {
			if (!userZoom[i]) {
				for (int j=0; j<2; j++) 
					userZoomVals[i][j] = axisVals[i][j];
			}
		}

		//Reset the histogram
		hist->Reset();
		
		//Rebin the histogram
		hist->SetBins(x_vals.size(), x_vals.front(), x_vals.back() + ADC_TIME_STEP, axisVals[1][1] - axisVals[1][0], axisVals[1][0], axisVals[1][1]);

		//Fill the histogram
		for (unsigned int i = 0; i < numAvgWaveforms_; i++) {
			ChannelEvent* evt = chanEvents_.at(i);
			for (size_t i=0; i < evt->traceLength; ++i) {
				hist->Fill(x_vals[i], evt->adcTrace[i]);
			}
		}

		prof = hist->ProfileX("AvgPulse");
		prof->SetLineColor(kRed);
		prof->SetMarkerColor(kRed);

		if(performFit_) fitter.FitPulse(prof, chanEvents_.front(), "QMER");

		if(!performCfd_){
			hist->SetStats(false);
			hist->Draw("COLZ");		
			prof->Draw("SAMES");

			hist->GetXaxis()->SetRangeUser(userZoomVals[0][0], userZoomVals[0][1]);
			hist->GetYaxis()->SetRangeUser(userZoomVals[1][0], userZoomVals[1][1]);
		}		
		else{
			prof->SetStats(false);
			prof->Draw("S");

			prof->GetXaxis()->SetRangeUser(userZoomVals[0][0], userZoomVals[0][1]);
			prof->GetYaxis()->SetRangeUser(userZoomVals[1][0], userZoomVals[1][1]);

			// Draw the cfd crossing line.
			cfdLine->DrawLine(cfdAvg*ADC_TIME_STEP, userZoomVals[1][0], cfdAvg*ADC_TIME_STEP, userZoomVals[1][1]);
			
			if(this->IsVerbose())
				std::cout << " CFD PHASE ANALYSIS: meanPhase = " << cfdAvg*ADC_TIME_STEP << " ns, stdDev = " << cfdStdDev*ADC_TIME_STEP << " ns FWHM.\n";

			// Draw a bounding box for the FWHM of the phase.
			cfdBox->SetX1((cfdAvg - cfdStdDev/2)*ADC_TIME_STEP);
			cfdBox->SetY1(userZoomVals[1][0]);
			cfdBox->SetX2((cfdAvg + cfdStdDev/2)*ADC_TIME_STEP);
			cfdBox->SetY2(userZoomVals[1][1]);
			cfdBox->Draw("SAME");
		}

		canvas->Update();	
		TPaveStats* stats = (TPaveStats*) prof->GetListOfFunctions()->FindObject("stats");
		if (stats) {
			stats->SetX1NDC(0.55);
			stats->SetX2NDC(0.9);
		}
	}
	
	// Remove the events from the deque.
	for (unsigned int i = 0; i < numAvgWaveforms_; i++) {
		delete chanEvents_.front();
		chanEvents_.pop_front();
	}

	// Update the number of waveforms currently being displayed.
	just_plotted = numAvgWaveforms_;

	// Update the canvas.
	canvas->Update();

	if(plotAll) // Reset the waveform counter to zero to preserve "avg all" setting.
		numAvgWaveforms_ = 0;

	num_displayed++;
}

/** Initialize the map file, the config file, the processor handler, 
  * and add all of the required processors.
  * \param[in]  prefix_ String to append to the beginning of system output.
  * \return True upon successfully initializing and false otherwise.
  */
bool scopeScanner::Initialize(std::string prefix_){
	if(init){ return false; }

	// Print a small welcome message.
	std::cout << "  Displaying traces for mod = " << mod_ << ", chan = " << chan_ << ".\n";

	return (init = true);
}

/** Receive various status notifications from the scan.
  * \param[in] code_ The notification code passed from ScanInterface methods.
  * \return Nothing.
  */
void scopeScanner::Notify(const std::string &code_/*=""*/){
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
	else if(code_ == "RESTART"){ chanEvents_.clear(); }
	else{ std::cout << msgHeader << "Unknown notification code '" << code_ << "'!\n"; }
}

/** Return a pointer to the Unpacker object to use for data unpacking.
  * If no object has been initialized, create a new one.
  * \return Pointer to an Unpacker object.
  */
Unpacker *scopeScanner::GetCore(){ 
	if(!core){ core = (Unpacker*)(new scopeUnpacker()); }
	return core;
}

/** Add a channel event to the deque of events to send to the processors.
  * This method should only be called from skeletonUnpacker::ProcessRawEvent().
  * \param[in]  event_ The raw XiaData to add to the channel event deque.
  * \return True if events are ready to be processed and false otherwise.
  */
bool scopeScanner::AddEvent(XiaData *event_){
	if(!event_){ return false; }

	//Check for invalid channel.
	if(event_->modNum != mod_ || event_->chanNum != chan_){  
		delete event_;
		return false;
	}

	//Check for empty trace.
	if(event_->traceLength == 0){
		std::cout << msgHeader << "Warning! Trace capture is not enabled for this channel!\n";
		stop_scan();
		delete event_;
		return false;
	}

	//Check threshold.
	int maximum = *std::max_element(event_->adcTrace, event_->adcTrace + event_->traceLength);
	if (maximum < threshLow_) {
		delete event_;
		return false;
	}
	else if (threshHigh_ > threshLow_ && maximum > threshHigh_) {
		delete event_;
		return false;
	}

	//Get the first event int the FIFO.
	ChannelEvent *channel_event = new ChannelEvent(event_);

	//Process the waveform.
	//channel_event->FindLeadingEdge();
	channel_event->ComputeBaseline();
	channel_event->IntegratePulse();
	
	//Push the channel event into the deque.
	chanEvents_.push_back(channel_event);

	// Handle the individual XiaData.
	if(chanEvents_.size() >= numAvgWaveforms_ && numAvgWaveforms_ > 0)
		return true;
		
	return false;
}

/** Process all channel events read in from the rawEvent.
  * This method should only be called from skeletonUnpacker::ProcessRawEvent().
  * \return True if events were processed and false otherwise.
  */
bool scopeScanner::ProcessEvents(){
	//Check if we have delayed the plotting enough
	time_t cur_time;
	time(&cur_time);
	while(difftime(cur_time, last_trace) < delay_) {
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
	Plot(); 
	
	//If this is a single capture we stop the plotting.
	if (singleCapture_) running = false;

	//Update the time.
	time(&last_trace);
	
	return true;
}

void scopeScanner::ClearEvents(){
	while(!chanEvents_.empty()){
		delete chanEvents_.front();
		chanEvents_.pop_front();
	}
}

/** CmdHelp is used to allow a derived class to print a help statement about
  * its own commands. This method is called whenever the user enters 'help'
  * or 'h' into the interactive terminal (if available).
  * \param[in]  prefix_ String to append at the start of any output. Not used by default.
  * \return Nothing.
  */
void scopeScanner::CmdHelp(const std::string &prefix_/*=""*/){
	std::cout << "   set <module> <channel>   - Set the module and channel of signal of interest (default = 0, 0).\n";
	std::cout << "   single                   - Perform a single capture.\n";
	std::cout << "   thresh <low> [high]      - Set the plotting window for trace maximum.\n";
	std::cout << "   fit <low> <high>         - Turn on fitting of waveform. Set <low> to \"off\" to disable.\n";
	std::cout << "   cfd [F=0.5] [D=1] [L=1]  - Turn on cfd analysis of waveform. Set [F] to \"off\" to disable.\n";
	std::cout << "   avg [numWaveforms]       - Set the number of waveforms to average.\n";
	std::cout << "   save <fileName> [suffix] - Save the next trace to the specified file name..\n";
	std::cout << "   delay [time]             - Set the delay between drawing traces (in seconds, default = 1 s).\n";
	std::cout << "   log                      - Toggle log/linear mode on the y-axis.\n";
	std::cout << "   clear                    - Clear all stored traces and start over.\n";
}

/** ArgHelp is used to allow a derived class to add a command line option
  * to the main list of options. This method is called at the end of
  * from the ::Setup method.
  * Does nothing useful by default.
  * \return Nothing.
  */
void scopeScanner::ArgHelp(){
	AddOption(optionExt("mod", required_argument, NULL, 'm', "<module>", "Module of signal of interest (default=0)"));
	AddOption(optionExt("chan", required_argument, NULL, 'c', "<channel>", "Channel of signal of interest (default=0)"));
}

/** SyntaxStr is used to print a linux style usage message to the screen.
  * \param[in]  name_ The name of the program.
  * \return Nothing.
  */
void scopeScanner::SyntaxStr(char *name_){ 
	std::cout << " usage: " << std::string(name_) << " [options]\n"; 
}

/** ExtraArguments is used to send command line arguments to classes derived
  * from ScanInterface. This method should loop over the optionExt elements
  * in the vector userOpts and check for those options which have been flagged
  * as active by ::Setup(). This should be overloaded in the derived class.
  * \return Nothing.
  */
void scopeScanner::ExtraArguments(){
	if(userOpts.at(0).active)
		std::cout << msgHeader << "Set module to (" << (mod_ = atoi(userOpts.at(0).argument.c_str())) << ").\n";
	if(userOpts.at(1).active)
		std::cout << msgHeader << "Set channel to (" << (chan_ = atoi(userOpts.at(1).argument.c_str())) << ").\n";
}

/** ExtraCommands is used to send command strings to classes derived
  * from ScanInterface. If ScanInterface receives an unrecognized
  * command from the user, it will pass it on to the derived class.
  * \param[in]  cmd_ The command to interpret.
  * \param[out] arg_ Vector or arguments to the user command.
  * \return True if the command was recognized and false otherwise.
  */
bool scopeScanner::ExtraCommands(const std::string &cmd_, std::vector<std::string> &args_){
	if(cmd_ == "set"){ // Toggle debug mode
		if(args_.size() == 2){
			// Clear all events from the event deque.
			ClearEvents();
		
			// Set the module and channel.
			mod_ = atoi(args_.at(0).c_str());
			chan_ = atoi(args_.at(1).c_str());

			resetGraph_ = true;
		}
		else{
			std::cout << msgHeader << "Invalid number of parameters to 'set'\n";
			std::cout << msgHeader << " -SYNTAX- set <module> <channel>\n";
		}
	}
	else if(cmd_ == "single") {
		singleCapture_ = !singleCapture_;
	}
	else if (cmd_ == "thresh") {
		if (args_.size() == 1) {
			threshLow_ = atoi(args_.at(0).c_str());
			threshHigh_ = -1;
		}
		else if (args_.size() == 2) {
			threshLow_ = atoi(args_.at(0).c_str());
			threshHigh_ = atoi(args_.at(1).c_str());
		}
		else {
			std::cout << msgHeader << "Invalid number of parameters to 'thresh'\n";
			std::cout << msgHeader << " -SYNTAX- thresh <lowerThresh> [upperThresh]\n";
		}
	}
	else if (cmd_ == "fit") {
		if (args_.size() >= 1 && args_.at(0) == "off") { // Turn root fitting off.
			if(performFit_){
				std::cout << msgHeader << "Disabling root fitting.\n"; 
				delete graph->GetListOfFunctions()->FindObject(fitter.GetFunction()->GetName());
				canvas->Update();
				performFit_ = false;
			}
			else{ std::cout << msgHeader << "Fitting is not enabled.\n"; }
		}
		else if (args_.size() == 2) { // Turn root fitting on.
			fitLow_ = atoi(args_.at(0).c_str());
			fitHigh_ = atoi(args_.at(1).c_str());
			fitter.SetFitRange(fitLow_, fitHigh_);
			std::cout << msgHeader << "Setting root fitting range to [" << fitLow_ << ", " << fitHigh_ << "].\n"; 
			performFit_ = true;
		}
		else {
			std::cout << msgHeader << "Invalid number of parameters to 'fit'\n";
			std::cout << msgHeader << " -SYNTAX- fit <low> <high>\n";
			std::cout << msgHeader << " -SYNTAX- fit off\n";
		}
	}
	else if (cmd_ == "cfd") {
		cfdF_ = 0.5;
		cfdD_ = 1;
		cfdL_ = 1;
		if (args_.empty()) { performCfd_ = true; }
		else if (args_.size() == 1) { 
			if(args_.at(0) == "off"){ // Turn cfd analysis off.
				if(performCfd_){
					std::cout << msgHeader << "Disabling cfd analysis.\n"; 
					performCfd_ = false;
				}
				else{ std::cout << msgHeader << "Cfd is not enabled.\n"; }
			}
			else{
				cfdF_ = atof(args_.at(0).c_str());
				performCfd_ = true;
			}
		}
		else if (args_.size() == 2) {
			cfdF_ = atof(args_.at(0).c_str());
			cfdD_ = atoi(args_.at(1).c_str());
			performCfd_ = true;
		}
		else if (args_.size() == 3) {
			cfdF_ = atof(args_.at(0).c_str());
			cfdD_ = atoi(args_.at(1).c_str());
			cfdL_ = atoi(args_.at(2).c_str());
			performCfd_ = true;
		}
		if(performCfd_)
			std::cout << msgHeader << "Enabling cfd analysis with F=" << cfdF_ << ", D=" << cfdD_ << ", L=" << cfdL_ << std::endl;
	}
	else if (cmd_ == "avg") {
		if (args_.size() == 1) {
			numAvgWaveforms_ = atoi(args_.at(0).c_str());
		}
		else{
			numAvgWaveforms_ = 0;
			restart();
		}
	}
	else if (cmd_ == "tdiff") {
		tdiffMode_ = !tdiffMode_;
		if(tdiffMode_) std::cout << msgHeader << "Enabling time difference mode.\n";
		else std::cout << msgHeader << "Disabling time difference mode.\n";
	}
	else if(cmd_ == "save") {
		if (args_.size() >= 1) {
			std::string saveFile = args_.at(0);
			std::string nameSuffix = "";
			if(args_.size() > 1)
				nameSuffix = args_.at(1);

			if(just_plotted == 1){
				// Save the TGraph to a file.
				TFile f(saveFile.c_str(), "UPDATE");
				graph->Clone(("trace"+nameSuffix).c_str())->Write();
				std::cout << msgHeader << "Wrote \"trace" << nameSuffix << "\" to " << saveFile << std::endl;
				if(performFit_){
					fitter.GetFunction()->Clone(("func"+nameSuffix).c_str())->Write();
					std::cout << msgHeader << "Wrote \"func" << nameSuffix << "\" to " << saveFile << std::endl;				
				}
				if(performCfd_){
					cfdLine->Clone(("cfdLine"+nameSuffix).c_str())->Write();
					std::cout << msgHeader << "Wrote \"cfdLine" << nameSuffix << "\" to " << saveFile << std::endl;
				}
				f.Close();
			}
			else if(just_plotted > 0){
				// Save the TH2F and TProfile to a file.
				TFile f(saveFile.c_str(), "UPDATE");
				hist->Clone(("hist"+nameSuffix).c_str())->Write();
				prof->Clone(("prof"+nameSuffix).c_str())->Write();
				std::cout << msgHeader << "Wrote \"hist" << nameSuffix << "\" and \"prof" << nameSuffix << "\" to " << saveFile << std::endl;
				if(performFit_){
					fitter.GetFunction()->Clone(("func"+nameSuffix).c_str())->Write();
					std::cout << msgHeader << "Wrote \"func" << nameSuffix << "\" to " << saveFile << std::endl;				
				}
				if(performCfd_){
					cfdLine->Clone(("cfdLine"+nameSuffix).c_str())->Write();
					cfdBox->Clone(("cfdBox"+nameSuffix).c_str())->Write();
					std::cout << msgHeader << "Wrote \"cfdLine" << nameSuffix << "\" and \"" << nameSuffix << "\" to " << saveFile << std::endl;
				}
				f.Close();
				
			}
			else{ // Nothing has been drawn yet.
				std::cout << msgHeader << "No waveforms currently displayed.\n";
			}
		}
		else {
			std::cout << msgHeader << "Invalid number of parameters to 'save'\n";
			std::cout << msgHeader << " -SYNTAX- save <fileName> [suffix]\n";
		}
	}
	else if(cmd_ == "delay"){
		if(args_.size() == 1){ delay_ = atoi(args_.at(0).c_str()); }
		else{
			std::cout << msgHeader << "Invalid number of parameters to 'delay'\n";
			std::cout << msgHeader << " -SYNTAX- delay <time>\n";
		}
	}
	else if(cmd_ == "log"){
		if(canvas->GetLogy()){ 
			canvas->SetLogy(0);
			std::cout << msgHeader << "y-axis set to linear.\n"; 
		}
		else{ 
			canvas->SetLogy(1); 
			std::cout << msgHeader << "y-axis set to log.\n"; 
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
void scopeScanner::IdleTask(){
	gSystem->ProcessEvents();
	usleep(SLEEP_WAIT);
}

int main(int argc, char *argv[]){
	// Define a new unpacker object.
	scopeScanner scanner;
	
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
