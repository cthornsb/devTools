#ifndef TIMING_PARAMETERS_HPP
#define TIMING_PARAMETERS_HPP

#include <string>

class ChannelEvent;
class TCanvas;

class TLine;
class TGraph;

/*FTRIG_DELAY           Delayed local fast trigger of channel 0
  GND                   
  FTRIG_VAL             Validated, delayed local fast trigger of channel 0 (GLBETRIG_CE && CHANETRIG_CE)
  GLBETRIG_CE           Stretched external global validation trigger of channel 0
  CHANETRIG_CE          Stretched channel validation trigger (doubles) of channel 0
  VANDLE_PWA[0]         VANDLE pairwise coincidence trigger of channels 0 and 1
  GLOBAL_TRIG           Global validation trigger
  FT[0]                 Fast trigger from channel 0
  FT[1]                 Fast trigger from channel 1
  FT[2]                 Fast trigger from channel 2
  VANDLE_PWA_OR         Crate level OR of VANDLE pairwise coincidence triggers
  BETA_PWA_TRIG_OR      OR of beta pairwise coincidence triggers
  BETA_VALIDATION_TRIG  Validation of all beta triggers*/

namespace InitialValues{
	const int FastTrigBackLen = 48; // ns
	const int FtrigoutDelay = 0; // ns
	const int ExternDelayLen = 104; // ns
	const int ExtTrigStretch = 400; // ns
	const int ChanTrigStretch = 200; // ns
}

class timingParameters{
  private:
	const int LogicLatency = 150; // ns
	const int LogicLength = 50; // ns

	const int TRIGGER_RISETIME = 16; // ns, L (length)
	const int TRIGGER_FLATTOP = 0; // ns, G (gap)

	int adcBitRange;

	int FastTrigBackLen[2][2];
	int FtrigoutDelay[2][2];
	int ExternDelayLen[2][2];
	int ExtTrigStretch[2][2];
	int ChanTrigStretch[2][2];

	int GLBETRIG_CE[2];
	int CHANETRIG_CE[2][8][2];
	int VANDLE_PWA[2][8][2];
	int BETA_PWA[2][8][2];
	
	int VANDLE_PWA_SCFT_OR[2];
	int BETA_SINGLES_TRIG_OR[2];
	int SINGLE_CHANNEL_FT_OR[2];
	int BETA_VALIDATION_TRIG[2];

	bool GLOBAL_VALIDATION_TRIGGER;
	bool BETA_PWA_TRIG_OR;

	std::string chanTypes[2][2];

	bool channels[2][2];

	double signals[2][2][1000]; // Signal waveform.
	double filtered[2][2][1000]; // Fast filtered waveform.
	int thresholds[2][2]; // Fast filter thresholds.
	
	int FTRIG_DELAY[2][2][2]; // Delayed fast trigger time.
	int FTRIG_VAL[2][2][2]; // Validated, delayed fast trigger time.

	int triggerTimes[2][2];

	TLine *line;
	
	TGraph *graphs[4];

	double eval(const double &x, const double &mean);

	void drawLabel(const double &x, const double &y, const std::string &label, const int &color=1);

	void drawVerticalLine(const int &x, const double &y0);

	void drawLogicSignal(const double &y0, const std::string &label="");

	void drawLogicSignal(const int *times, const double &y0, const std::string &label="", const double &A=0.85);

	void drawLogicSignal(const int &start, const double &y0, const std::string &label="", const double &A=0.85);

	void drawSignal(const int &mod, const int &chan, const double &y0, const std::string &label="", const double &scale=-1);

	void drawSignal(const int &mod, const int &chan, const double &scale=-1);

	void fastFilter(const int &mod, const int &chan, double *arr, size_t &maxIndex);

	void initialize();

	void validateModules();

	void validateChannels();
	
  public:
	timingParameters();
	
	~timingParameters();

	bool setChannelType(const size_t &mod, const size_t &chan, const std::string &type);

	bool setThreshold(const size_t &mod, const size_t &chan, const int &thresh);

	bool setParameter(const size_t &mod, const size_t &chan, const std::string &name, const int &val);

	int setAdcBitRange(const int &numBits);

	int setFastTrigBackLen(const size_t &mod, const size_t &chan, const int &val);

	int setFtrigoutDelay(const size_t &mod, const size_t &chan, const int &val);

	int setExternDelayLen(const size_t &mod, const size_t &chan, const int &val);

	int setExtTrigStretch(const size_t &mod, const size_t &chan, const int &val);

	int setChanTrigStretch(const size_t &mod, const size_t &chan, const int &val);

	bool setCoincidence(const int &scheme);

	bool setWaveform(const int &mod, const int &chan, ChannelEvent *evt, const size_t &t0=0);

	bool validate();

	void paramHelp() const ;
	
	void draw(TCanvas *can);
	
	void update(TCanvas *can);

	void clear();
};

#endif
