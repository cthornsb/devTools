#ifndef PARAMETER_HPP
#define PARAMETER_HPP

class timingParameters;

class parameter{
  public:
	parameter(const std::string &name_, const std::string &description_, const int &lowLimit_, const int &highLimit_);

	parameter(const std::string &name_, const std::string &description_, const int &lowLimit_, const int &highLimit_, int (*ptr_)(timingParameters *, const size_t &, const size_t &, const int &));

	std::string getName() const { return name; }

	int getLowLimit() const { return limits[0]; }
	
	int getHighLimit() const { return limits[1]; }

	bool checkLimits(const int &val) const { return (limits[0] <= val && val <= limits[1]); }

	void setFunctionPointer(int (*ptr_)(timingParameters *ptr, const size_t &, const size_t &, const int &)){ ptr = ptr_; }
	
	int execute(timingParameters *timing, const size_t &mod, const size_t &chan, const int &val) const ;
	
	void print() const ;
	
  private:
	std::string name;
	std::string desc;

	int (*ptr)(timingParameters *, const size_t &, const size_t &, const int &);
	
	int limits[2];
};

int setFastTrigBackLen(timingParameters *ptr, const size_t &mod, const size_t &chan, const int &val);

int setFtrigoutDelay(timingParameters *ptr, const size_t &mod, const size_t &chan, const int &val);

int setExternDelayLen(timingParameters *ptr, const size_t &mod, const size_t &chan, const int &val);

int setExtTrigStretch(timingParameters *ptr, const size_t &mod, const size_t &chan, const int &val);

int setChanTrigStretch(timingParameters *ptr, const size_t &mod, const size_t &chan, const int &val);

int dummy(timingParameters *, const size_t &, const size_t &n, const int &);

const std::string descriptions[5] = {"Stretch the fast trigger before using for coincidence (basically the coincidence window width)",
	                                 "Delay the fast trigger before it is used in coincidence",
	                                 "Delay the local fast trigger to compensate for delayed channel or global validation trigger",
	                                 "Stretch the external global validation trigger (triples)",
	                                 "Stretch the channel validation trigger (doubles)"};

const std::vector<parameter> params = {parameter("FastTrigBackLen", descriptions[0], 8, 32760, setFastTrigBackLen),
	                                   parameter("FtrigoutDelay", descriptions[1], 0, 1016, setFtrigoutDelay),
	                                   parameter("ExternDelayLen", descriptions[2], 0, 2040, setExternDelayLen),
	                                   parameter("ExtTrigStretch", descriptions[3], 8, 32760, setExtTrigStretch),
	                                   parameter("ChanTrigStretch", descriptions[4], 8, 32760, setChanTrigStretch)};

#endif
