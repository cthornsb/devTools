#include <iostream>
#include <string>
#include <vector>

#include "parameter.hpp"
#include "timingParameters.hpp"

int setFastTrigBackLen(timingParameters *ptr, const size_t &mod, const size_t &chan, const int &val){ 
	return ptr->setFastTrigBackLen(mod, chan, val);
}

int setFtrigoutDelay(timingParameters *ptr, const size_t &mod, const size_t &chan, const int &val){ 
	return ptr->setFtrigoutDelay(mod, chan, val);
}

int setExternDelayLen(timingParameters *ptr, const size_t &mod, const size_t &chan, const int &val){ 
	return ptr->setExternDelayLen(mod, chan, val);
}

int setExtTrigStretch(timingParameters *ptr, const size_t &mod, const size_t &chan, const int &val){ 
	return ptr->setExtTrigStretch(mod, chan, val);
}

int setChanTrigStretch(timingParameters *ptr, const size_t &mod, const size_t &chan, const int &val){ 
	return ptr->setChanTrigStretch(mod, chan, val);
}

int dummy(timingParameters *, const size_t &, const size_t &n, const int &){ 
	return -1; 
}

///////////////////////////////////////////////////////////////////////////////
// class parameter
///////////////////////////////////////////////////////////////////////////////

parameter::parameter(const std::string &name_, const std::string &description_, const int &lowLimit_, const int &highLimit_) : name(name_), desc(description_), ptr(dummy) {
	limits[0] = lowLimit_;
	limits[1] = highLimit_;
}

parameter::parameter(const std::string &name_, const std::string &description_, const int &lowLimit_, const int &highLimit_, 
                     int (*ptr_)(timingParameters *, const size_t &, const size_t &, const int &)) : name(name_), desc(description_), ptr(ptr_){
	limits[0] = lowLimit_;
	limits[1] = highLimit_;
}

int parameter::execute(timingParameters *timing, const size_t &mod, const size_t &chan, const int &val) const {
	return ptr(timing, mod, chan, val);
}

void parameter::print() const {
	std::cout << name << std::string(20-name.length(), ' ') << desc << std::endl;
}
