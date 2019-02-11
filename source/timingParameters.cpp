#include <iostream>

#include "TText.h"
#include "TLine.h"
#include "TGraph.h"
#include "TCanvas.h"

#include "XiaData.hpp"

#include "parameter.hpp"
#include "timingParameters.hpp"

const int signalMeans[2][2] = {{50, 50}, {150, 150}};

///////////////////////////////////////////////////////////////////////////////
// class timingParameters
///////////////////////////////////////////////////////////////////////////////

timingParameters::timingParameters(){ 
	this->initialize();
}

timingParameters::~timingParameters(){
	for(size_t i = 0; i < 4; i++){
		delete graphs[i];
	}
	delete line;
}

bool timingParameters::setChannelType(const size_t &mod, const size_t &chan, const std::string &type){
	if(mod >= 2 || chan >= 2) return false;
	chanTypes[mod][chan] = type;
	return true;
}

bool timingParameters::setThreshold(const size_t &mod, const size_t &chan, const int &thresh){
	if(mod >= 2 || chan >= 2) return false;
	thresholds[mod][chan] = thresh;
	return true;
}

bool timingParameters::setParameter(const size_t &mod, const size_t &chan, const std::string &name, const int &val){
	if(mod >= 2 || chan >= 2){
		std::cout << "Error: Invalid mod=" << mod << " or chan=" << chan << " specified!\n";
		return false;
	}
	for(std::vector<parameter>::const_iterator iter = params.begin(); iter != params.end(); iter++){
		if(iter->getName() == name){
			if(iter->checkLimits(val)){
				int retval = iter->execute(this, mod, chan, val);
				if(retval >= 0){
					std::cout << mod << "\t" << chan << "\t" << name << "\t" << retval << "->" << val << std::endl;
					return true;
				}
				else{
					std::cout << "Error: Failed to set parameter \"" << name << "\"!\n";
					return false;
				}
			}
			std::cout << "Error: Specified parameter value (" << val << ") is outside valid range for \"" << name << "\" i.e. [" << iter->getLowLimit() << ", " << iter->getHighLimit() << "]!\n";
			return false;
		}
	}
	std::cout << "Error: No parameter named \"" << name << "\" in parameter list!\n";
	return false;
}

int timingParameters::setAdcBitRange(const int &numBits){
	int oldRange = adcBitRange;
	adcBitRange = std::pow(2, numBits);
	for(size_t i = 0; i < 2; i++){
		for(size_t j = 0; j < 2; j++){
			thresholds[i][j] *= (double)adcBitRange/oldRange;
		}
	}	
	return adcBitRange;
}

int timingParameters::setFastTrigBackLen(const size_t &mod, const size_t &chan, const int &val){
	if(mod >= 2 || chan >= 2) return false;
	int retval = FastTrigBackLen[mod][chan];
	FastTrigBackLen[mod][chan] = val;
	return retval;
}

int timingParameters::setFtrigoutDelay(const size_t &mod, const size_t &chan, const int &val){
	if(mod >= 2 || chan >= 2) return false;
	int retval = FtrigoutDelay[mod][chan];
	FtrigoutDelay[mod][chan] = val;
	return retval;
}

int timingParameters::setExternDelayLen(const size_t &mod, const size_t &chan, const int &val){
	if(mod >= 2 || chan >= 2) return false;
	int retval = ExternDelayLen[mod][chan];
	ExternDelayLen[mod][chan] = val;
	return retval;
}

int timingParameters::setExtTrigStretch(const size_t &mod, const size_t &chan, const int &val){
	if(mod >= 2 || chan >= 2) return false;
	int retval = ExtTrigStretch[mod][chan];
	ExtTrigStretch[mod][chan] = val;
	return retval;
}

int timingParameters::setChanTrigStretch(const size_t &mod, const size_t &chan, const int &val){
	if(mod >= 2 || chan >= 2) return -1;
	int retval = ChanTrigStretch[mod][chan];
	ChanTrigStretch[mod][chan] = val;
	return retval;
}

double timingParameters::eval(const double &x, const double &mean){
	const double sigma = 2;
	if(mean < 0) return 0;
	return (0.5*adcBitRange*std::exp(-0.5*std::pow((x-mean)/sigma, 2)));
}

void timingParameters::drawLabel(const double &x, const double &y, const std::string &label, const int &color/*=1*/){
	if(!label.empty()){
		TText labelText;
		labelText.SetTextAlign(32);
		labelText.SetTextFont(42);
		labelText.SetTextSizePixels(18);
		labelText.SetTextColor(color);
		labelText.DrawText(x, y+0.5, label.c_str());
	}
}

void timingParameters::drawVerticalLine(const int &x, const double &y0){
	line->DrawLine(x, y0, x, y0+1);
}

void timingParameters::drawLogicSignal(const double &y0, const std::string &label/*=""*/){
	line->DrawLine(0, y0, 1000, y0);
	drawLabel(975, y0, label, kRed);
}

void timingParameters::drawLogicSignal(const int *times, const double &y0, const std::string &label/*=""*/, const double &A/*=0.85*/){
	if(times[1] <= 0){
		line->DrawLine(0, y0, 1000, y0);
		drawLabel(975, y0, label, kRed);
		return;
	}
	int start = times[0];
	int stop = times[1];
	line->DrawLine(0, y0, start, y0);
	line->DrawLine(start, y0, start, y0+A);
	line->DrawLine(start, y0+A, stop, y0+A);
	line->DrawLine(stop, y0+A, stop, y0);
	line->DrawLine(stop, y0, 1000, y0);
	drawLabel(975, y0, label, kGreen+2);
}

void timingParameters::drawLogicSignal(const int &start, const double &y0, const std::string &label/*=""*/, const double &A/*=0.85*/){
	int stop = start + LogicLength;
	line->DrawLine(0, y0, start, y0);
	line->DrawLine(start, y0, start, y0+A);
	line->DrawLine(start, y0+A, stop, y0+A);
	line->DrawLine(stop, y0+A, stop, y0);
	line->DrawLine(stop, y0, 1000, y0);
	drawLabel(975, y0, label, kGreen+2);
}

void timingParameters::drawSignal(const int &mod, const int &chan, const double &y0, const std::string &label/*=""*/, const double &scale/*=-1*/){
	if(mod >= 2 || chan >= 2) return;
	for(int i = 0; i < 1000; i++){
		if(scale < 0)
			graphs[2*mod+chan]->SetPoint(i, i, y0+(1.0/adcBitRange)*signals[mod][chan][i]);
		else
			graphs[2*mod+chan]->SetPoint(i, i, y0+scale*signals[mod][chan][i]);
	}
	graphs[2*mod+chan]->Draw("LSAME");
	drawLabel(975, y0, label);	
}

void timingParameters::drawSignal(const int &mod, const int &chan, const double &scale/*=-1*/){
	if(mod >= 2 || chan >= 2) return;
	for(int i = 0; i < 1000; i++){
		if(scale < 0)
			graphs[2*mod+chan]->SetPoint(i, i, (1.0/adcBitRange)*filtered[mod][chan][i]);
		else
			graphs[2*mod+chan]->SetPoint(i, i, scale*filtered[mod][chan][i]);
	}
	graphs[2*mod+chan]->Draw("AL");
	line->DrawLine(triggerTimes[mod][chan], -100, triggerTimes[mod][chan], 100);
}

void timingParameters::fastFilter(const int &mod, const int &chan, double *arr, size_t &maxIndex){
	maxIndex = 0;
	double maximum = -1E10;
	for(int k = 0; k < 1000; k++){
		arr[k] = 0;
		for(int l = k-2*TRIGGER_RISETIME-TRIGGER_FLATTOP+1; l <= k-(TRIGGER_RISETIME+TRIGGER_FLATTOP); l++){
			if(l < 0 || l >= 1000) continue;
			arr[k] += -signals[mod][chan][l];
		}
		for(int l = k-TRIGGER_RISETIME+1; l <= k; l++){
			if(l < 0 || l >= 1000) continue;
			arr[k] += signals[mod][chan][l];
		}
		if(arr[k] > maximum){
			maximum = arr[k];
			maxIndex = k;
		}
	}
	/*for(int k = 0; k < 1000; k++){ // Normalize the filtered pulse.
		if(arr[k] < 0)
			arr[k] = 0;
		else
			arr[k] *= 1/maximum;
	}*/
}

void timingParameters::initialize(){
	line = new TLine();
	for(size_t i = 0; i < 4; i++){
		graphs[i] = new TGraph(1000);
	}
	this->setAdcBitRange(12);
	for(size_t i = 0; i < 2; i++){
		for(size_t j = 0; j < 2; j++){
			FastTrigBackLen[i][j] = InitialValues::FastTrigBackLen;
			FtrigoutDelay[i][j] = InitialValues::FtrigoutDelay;
			ExternDelayLen[i][j] = InitialValues::ExternDelayLen;
			ExtTrigStretch[i][j] = InitialValues::ExtTrigStretch;
			ChanTrigStretch[i][j] = InitialValues::ChanTrigStretch;
			thresholds[i][j] = 5;
		}
	}
	setCoincidence(2);
}

void timingParameters::validateChannels(){
	for(int i = 0; i < 2; i++){
		for(int j = 0; j < 2; j++){
			channels[i][j] = false;
			triggerTimes[i][j] = 0;
			/*for(int k = 0; k < 1000; k++){ // Debug
				signals[i][j][k] = eval(k, signalMeans[i][j]);
			}*/

			// Fast filters.
			size_t maxIndex = 0;
			fastFilter(i, j, filtered[i][j], maxIndex);
			
			if(maxIndex == 0) continue;
			
			// Threshold checks.
			for(int k = maxIndex; k > 2; k--){
				if(filtered[i][j][k-1] < thresholds[i][j] && filtered[i][j][k] >= thresholds[i][j]){ // Trigger.
					triggerTimes[i][j] = k;
					break;
				}
			}
			
			channels[i][j] = (triggerTimes[i][j] > 0);
		}
	}
}

void timingParameters::validateModules(){
	for(int i = 0; i < 2; i++){ // Set the delayed fast trigger for each channel.
		for(int j = 0; j < 2; j++){
			if(channels[i][j]){
				FTRIG_DELAY[i][j][0] = triggerTimes[i][j] + LogicLatency + FtrigoutDelay[i][j];
				FTRIG_DELAY[i][j][1] = FTRIG_DELAY[i][j][0] + FastTrigBackLen[i][j];
			}
			else continue;
		}
	}

	for(int i = 0; i < 2; i++){
		for(int j = 0; j < 2; j++){ // Check for trigger overlaps.
			if(FTRIG_DELAY[i][j][1] <= 0) continue;

			int fastTrigWindow[2];
			fastTrigWindow[0] = std::min(FTRIG_DELAY[i][2*j][0], FTRIG_DELAY[i][2*j+1][0]);
			fastTrigWindow[1] = std::max(FTRIG_DELAY[i][2*j][0], FTRIG_DELAY[i][2*j+1][0]);
			
			// Check for fast trigger overlaps.
			bool validOverlap = false;
			if(fastTrigWindow[1] >= fastTrigWindow[0] && fastTrigWindow[1] <= (fastTrigWindow[0]+FastTrigBackLen[i][j]))
				validOverlap = true;

			if(validOverlap){
				if(chanTypes[i][2*j] == "vandle" || chanTypes[i][2*j] == "neutron"){
					VANDLE_PWA[i][2*j][0] = fastTrigWindow[1];
					VANDLE_PWA[i][2*j][1] = fastTrigWindow[0]+FastTrigBackLen[i][2*j];
					CHANETRIG_CE[i][2*j][0] = VANDLE_PWA[i][2*j][1];
					CHANETRIG_CE[i][2*j][1] = CHANETRIG_CE[i][2*j][0]+ChanTrigStretch[i][2*j];
				}
				else{
					VANDLE_PWA[i][j][0] = 0;
					VANDLE_PWA[i][j][1] = 0;
				}
				if(chanTypes[i][2*j] == "beta_pw" || chanTypes[i][2*j] == "beta"){
					BETA_PWA[i][2*j][0] = fastTrigWindow[1];
					BETA_PWA[i][2*j][1] = fastTrigWindow[0]+FastTrigBackLen[i][2*j];
					CHANETRIG_CE[i][2*j][0] = BETA_PWA[i][2*j][1];
					CHANETRIG_CE[i][2*j][1] = CHANETRIG_CE[i][2*j][0]+ChanTrigStretch[i][2*j];
				}
				else{
					BETA_PWA[i][2*j][0] = 0;
					BETA_PWA[i][2*j][1] = 0;
				}
				
				// Set the odd channels to match the even ones.
				VANDLE_PWA[i][2*j+1][0] = VANDLE_PWA[i][2*j][0];
				VANDLE_PWA[i][2*j+1][1] = VANDLE_PWA[i][2*j][1];
				BETA_PWA[i][2*j+1][0] = BETA_PWA[i][2*j][0];
				BETA_PWA[i][2*j+1][1] = BETA_PWA[i][2*j][1];
				CHANETRIG_CE[i][2*j+1][0] = CHANETRIG_CE[i][2*j][0];
				CHANETRIG_CE[i][2*j+1][1] = CHANETRIG_CE[i][2*j][1];
			}
		}
	}
}

bool timingParameters::validate(){
	validateChannels();
	validateModules();

	BETA_PWA_TRIG_OR = false;
	for(int i = 0; i < 2; i++){
		// Validated, delayed local fast trigger of channel 0.
		for(int j = 0; j < 2; j++){ 
			if(FTRIG_DELAY[i][j][1] <= 0) continue;
		
			FTRIG_DELAY[i][j][0] += ExternDelayLen[i][j];
			FTRIG_DELAY[i][j][1] += ExternDelayLen[i][j];
			if(CHANETRIG_CE[i][j][1] > 0 && (FTRIG_DELAY[i][j][0] >= CHANETRIG_CE[i][j][0] && FTRIG_DELAY[i][j][0] <= CHANETRIG_CE[i][j][1])){
				FTRIG_VAL[i][j][0] = FTRIG_DELAY[i][j][0];
				FTRIG_VAL[i][j][1] = FTRIG_DELAY[i][j][0] + 8; // CRT temporary
			}
			else{
				FTRIG_VAL[i][j][0] = 0;
				FTRIG_VAL[i][j][1] = 0;
			}
			BETA_PWA_TRIG_OR = BETA_PWA_TRIG_OR || (BETA_PWA[0][0][1] > 0);
		}
	}

	// Set the beta validation trigger.
	GLOBAL_VALIDATION_TRIGGER = false;
	BETA_VALIDATION_TRIG[0] = 0;
	BETA_VALIDATION_TRIG[1] = 0;
	if(BETA_PWA_TRIG_OR){
		BETA_VALIDATION_TRIG[0] = std::max(BETA_PWA[0][0][0], VANDLE_PWA[1][0][0]);
		BETA_VALIDATION_TRIG[1] = std::min(BETA_PWA[0][0][1], VANDLE_PWA[1][0][1]);
		if(BETA_VALIDATION_TRIG[1] > BETA_VALIDATION_TRIG[0]){
			GLOBAL_VALIDATION_TRIGGER = true;
		}
		else{
			BETA_VALIDATION_TRIG[0] = 0;
			BETA_VALIDATION_TRIG[1] = 0;
		}
	}
	
	// Set the global trigger.
	if(GLOBAL_VALIDATION_TRIGGER){
		GLBETRIG_CE[0] = BETA_VALIDATION_TRIG[1];
		GLBETRIG_CE[1] = GLBETRIG_CE[0] + ExtTrigStretch[0][0];
	}
	else{
		GLBETRIG_CE[0] = 0;
		GLBETRIG_CE[1] = 0;
	}
	
	return GLOBAL_VALIDATION_TRIGGER;
}

void timingParameters::clear(){
	for(int i = 0; i < 2; i++){ // Clear all graphs.
		for(int j = 0; j < 2; j++){	
			for(size_t k = 0; k < 1000; k++){
				signals[i][j][k] = 0;
				graphs[2*i+j]->SetPoint(k, k, 0);
			}
			FTRIG_DELAY[i][j][0] = 0;
			FTRIG_DELAY[i][j][1] = 0;
			FTRIG_VAL[i][j][0] = 0;
			FTRIG_VAL[i][j][1] = 0;
			VANDLE_PWA[i][j][0] = 0;
			VANDLE_PWA[i][j][1] = 0;
			BETA_PWA[i][j][0] = 0;
			BETA_PWA[i][j][1] = 0;
			CHANETRIG_CE[i][j][0] = 0;
			CHANETRIG_CE[i][j][1] = 0;
		}
	}
	BETA_PWA_TRIG_OR = false;
	GLOBAL_VALIDATION_TRIGGER = false;
	BETA_VALIDATION_TRIG[0] = 0;
	BETA_VALIDATION_TRIG[1] = 0;
}

bool timingParameters::setCoincidence(const int &scheme){
	if(scheme < 0 || scheme >= 4) return false;
	for(size_t i = 0; i < 2; i++){
		for(size_t j = 0; j < 2; j++){
			if(scheme == 0){ // Singles
				chanTypes[0][j] = "beta";
				chanTypes[1][j] = "neutron";				
			}
			else if(scheme == 1){
				chanTypes[0][j] = "beta_pw";
				chanTypes[1][j] = "vandle";
			}
			else if(scheme == 2){
				chanTypes[0][j] = "beta";
				chanTypes[1][j] = "vandle";
			}
			else if(scheme == 3){
				chanTypes[0][j] = "beta_pw";
				chanTypes[1][j] = "vandle";
			}
		}
	}	
}

bool timingParameters::setWaveform(const int &mod, const int &chan, ChannelEvent *evt, const size_t &t0/*=0*/){
	if(!evt || evt->traceLength == 0 || (mod >= 2 || chan >= 2) || t0 > 1000) return false;
	size_t stopIndex = (evt->traceLength < 1000-t0 ? evt->traceLength : 1000-t0);
	for(size_t k = 0; k < stopIndex; k++){
		signals[mod][chan][k+t0] = evt->adcTrace[k] - evt->baseline;
	}
	return true;
}

void timingParameters::paramHelp() const {
	for(std::vector<parameter>::const_iterator iter = params.begin(); iter != params.end(); iter++){
		iter->print();
	}
}

void timingParameters::draw(TCanvas *can){
	can->Clear();

	/*can->Divide(2,2);
	can->cd(1); drawSignal(0, 0);
	can->cd(2); drawSignal(0, 1);
	can->cd(3); drawSignal(1, 0);
	can->cd(4); drawSignal(1, 1);*/

	can->cd()->DrawFrame(0, 0, 1000, 20);

	// Input signals
	drawSignal(0, 0, 19, "BetaL");
	drawSignal(0, 1, 18, "BetaR");
	drawSignal(1, 0, 17, "VandleL");
	drawSignal(1, 1, 16, "VandleR");
	
	for(int i = 0; i < 2; i++){ // Draw vertical lines representing trigger position.
		for(int j = 0; j < 2; j++){	
			if(channels[i][j]) 
				drawVerticalLine(triggerTimes[i][j], 19-2*i-j);
		}
	}
	
	// Beta fast triggers
	drawLogicSignal(FTRIG_DELAY[0][0], 15, "BetaL_FT");
	drawLogicSignal(FTRIG_DELAY[0][1], 14, "BetaR_FT");
	
	// VANDLE fast triggers
	drawLogicSignal(FTRIG_DELAY[1][0], 13, "VandleL_FT");
	drawLogicSignal(FTRIG_DELAY[1][1], 12, "VandleR_FT");

	// Beta validated fast triggers
	drawLogicSignal(FTRIG_VAL[0][0], 11, "BetaL_VAL");
	drawLogicSignal(FTRIG_VAL[0][1], 10, "BetaR_VAL");	
	
	// Beta ChanTrigStretch
	drawLogicSignal(CHANETRIG_CE[0][0], 9, "ChanTrig[0]"); // Channel trigger Betas (doubles).
	
	// Beta PWA
	drawLogicSignal(BETA_PWA[0][0], 8, "Beta_PWA");
	
	// VANDLE validated fast triggers
	drawLogicSignal(FTRIG_VAL[1][0], 7, "VandleL_VAL");
	drawLogicSignal(FTRIG_VAL[1][1], 6, "VandleR_VAL");

	// VANDLE ChanTrigStretch
	drawLogicSignal(CHANETRIG_CE[1][0], 5, "ChanTrig[1]"); // Channel trigger Vandle (doubles).
	
	// VANDLE PWA
	drawLogicSignal(VANDLE_PWA[1][0], 4, "Vandle_PWA"); // VANDLE pairwise OR.		
	
	// Global beta validation trigger.
	drawLogicSignal(BETA_VALIDATION_TRIG, 3, "Beta_Valid");

	// Master trigger.
	drawLogicSignal(GLBETRIG_CE, 2, "GLBETRIG[0]");
	drawLogicSignal(BETA_VALIDATION_TRIG, 1, "Master_Trigger");
	
	can->Update();
}

void timingParameters::update(TCanvas *can){
	this->validate();
	this->draw(can);
}
