#ifndef TIMING_HPP
#define TIMING_HPP

#include "scope.hpp"
#include "timingParameters.hpp"

class TApplication;
class TCanvas;

///////////////////////////////////////////////////////////////////////////////
// class eventHolder
///////////////////////////////////////////////////////////////////////////////

class eventHolder{
	ChannelEvent *start1;
	ChannelEvent *start2;
	ChannelEvent *stop1;
	ChannelEvent *stop2;
	
	eventHolder() : start1(NULL), start2(NULL), stop1(NULL), stop2(NULL) { }
	
	eventHolder(ChannelEvent *start1_, ChannelEvent *start2_, ChannelEvent *stop1_, ChannelEvent *stop2_) : start1(start1_), start2(start2_), stop1(stop1_), stop2(stop2_) { }
};

///////////////////////////////////////////////////////////////////////////////
// class timingUnpacker
///////////////////////////////////////////////////////////////////////////////

class timingUnpacker : public Unpacker {
  public:
  	/// Default constructor.
	timingUnpacker();
	
	/// Destructor.
	~timingUnpacker(){  }

	/** Return a pointer to a new XiaData channel event.
	  * \return A pointer to a new XiaData.
	  */
	XiaData *GetNewEvent();

  private:
	/** Process all events in the event list.
	  * \param[in]  addr_ Pointer to a ScanInterface object.
	  * \return Nothing.
	  */
	virtual void ProcessRawEvent(ScanInterface *addr_=NULL);
	
	/** Add an event to generic statistics output.
	  * \param[in]  event_ Pointer to the current XIA event.
	  * \param[in]  addr_  Pointer to a ScanInterface object.
	  * \return Nothing.
	  */
	virtual void RawStats(XiaData *event_, ScanInterface *addr_=NULL){  }
};

///////////////////////////////////////////////////////////////////////////////
// class timingScanner
///////////////////////////////////////////////////////////////////////////////

class timingScanner : public ScanInterface {
  public:
	/// Default constructor.
	timingScanner();
	
	/// Destructor.
	~timingScanner();
	
	/// Set the time delay between traces (in seconds).
	int GetDelay(){ return delay; }

	/// Set the number of seconds to wait between drawing of traces.
	void SetDelay(int delay_){ delay = (delay_>1)?delay_:1; }

	/** ExtraCommands is used to send command strings to classes derived
	  * from ScanInterface. If ScanInterface receives an unrecognized
	  * command from the user, it will pass it on to the derived class.
	  * \param[in]  cmd_ The command to interpret.
	  * \param[out] arg_ Vector or arguments to the user command.
	  * \return True if the command was recognized and false otherwise.
	  */
	virtual bool ExtraCommands(const std::string &cmd_, std::vector<std::string> &args_);
	
	/** ExtraArguments is used to send command line arguments to classes derived
	  * from ScanInterface. This method should loop over the optionExt elements
	  * in the vector userOpts and check for those options which have been flagged
	  * as active by ::Setup(). This should be overloaded in the derived class.
	  * \return Nothing.
	  */
	virtual void ExtraArguments();
	
	/** CmdHelp is used to allow a derived class to print a help statement about
	  * its own commands. This method is called whenever the user enters 'help'
	  * or 'h' into the interactive terminal (if available).
	  * \param[in]  prefix_ String to append at the start of any output. Not used by default.
	  * \return Nothing.
	  */
	virtual void CmdHelp(const std::string &prefix_="");
	
	/** ArgHelp is used to allow a derived class to add a command line option
	  * to the main list of options. This method is called at the end of
	  * from the ::Setup method.
	  * Does nothing useful by default.
	  * \return Nothing.
	  */
	virtual void ArgHelp();
	
	/** SyntaxStr is used to print a linux style usage message to the screen.
	  * \param[in]  name_ The name of the program.
	  * \return Nothing.
	  */
	virtual void SyntaxStr(char *name_);

	/** IdleTask is called whenever a scan is running in shared
	  * memory mode, and a spill has yet to be received. This method may
	  * be used to update things which need to be updated every so often
	  * (e.g. a root TCanvas) when working with a low data rate. 
	  * \return Nothing.
	  */
	virtual void IdleTask();

	/** Initialize the map file, the config file, the processor handler, 
	  * and add all of the required processors.
	  * \param[in]  prefix_ String to append to the beginning of system output.
	  * \return True upon successfully initializing and false otherwise.
	  */
	virtual bool Initialize(std::string prefix_="");
	
	/** Peform any last minute initialization before processing data. 
	  * /return Nothing.
	  */
	virtual void FinalInitialization(){  }
	
	/** Initialize the root output. 
	  * \param[in]  fname_     Filename of the output root file. 
	  * \param[in]  overwrite_ Set to true if the user wishes to overwrite the output file. 
	  * \return True upon successfully opening the output file and false otherwise. 
	  */
	virtual bool InitRootOutput(std::string fname_, bool overwrite_=true){ return false; }

	/** Receive various status notifications from the scan.
	  * \param[in] code_ The notification code passed from ScanInterface methods.
	  * \return Nothing.
	  */
	virtual void Notify(const std::string &code_="");

	/** Return a pointer to the Unpacker object to use for data unpacking.
	  * If no object has been initialized, create a new one.
	  * \return Pointer to an Unpacker object.
	  */
	virtual Unpacker *GetCore();

	/** Add a channel event to the deque of events to send to the processors.
	  * This method should only be called from skeletonUnpacker::ProcessRawEvent().
	  * \param[in]  event_ The raw XiaData to add to the channel event deque.
	  * \return True if events are ready to be processed and false otherwise.
	  */
	virtual bool AddEvent(XiaData *event_);
	
	/** Process all channel events read in from the rawEvent.
	  * This method should only be called from skeletonUnpacker::ProcessRawEvent().
	  * \return True if events were processed and false otherwise.
	  */
	virtual bool ProcessEvents();

	/** Clear the event deque.
	  * \return Nothing.
	  */
	void ClearEvents();

  private:
	bool acqRun_;
	bool singleCapture_;
	bool init;	
	bool running;

	unsigned int mod[4]; ///< The module of the signals of interest.
	unsigned int chan[4]; ///< The channel of the signals of interest.
	
	int coincidenceSelect;
	
	timingParameters timing;

	std::deque<ChannelEvent*> chanEvents[4]; ///<The buffer of waveforms to be plotted.
	
	ChannelEvent *events[4];

	unsigned int tOffsets[4];

	TApplication *rootapp; ///< Root application pointer.
	TCanvas *canvas; ///< The main plotting canvas.

	time_t last_trace; ///< The time of the last trace.

	int delay; /// The number of seconds to wait between drawing traces.
	
	/// Process a single event.
	void Process(); 
	
	/// Set triggering coincidence scheme.
	bool SetCoincidence(const int &scheme);
};

#endif
