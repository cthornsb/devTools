#ifndef OPTIONHANDLER_HPP
#define OPTIONHANDLER_HPP

#include <ScanInterface.hpp>

class optionHandler{
  public:
	optionHandler();
	
	~optionHandler(){ }

	/** SyntaxStr is used to print a linux style usage message to the screen.
	  * Prints a standard usage message by default.
	  * \param[in]  name_ The name of the program.
	  * \return Nothing.
	  */
	void syntaxStr(char *name_);

	/** Print a command line argument help dialogue.
	  * \param[in]  name_ The name of the program.
	  * \return Nothing.
	  */  
	void help(char *name_);
	
	/** Add a command line option to the option list.
	  * \param[in]  opt_ The option to add to the list.
	  * \return Nothing.
	  */
	void addOption(optionExt opt_);

	/** Setup user options and initialize all required objects.
	  * \param[in]  argc Number of arguments passed from the command line.
	  * \param[in]  argv Array of strings passed as arguments from the command line.
	  * \return True upon success and false otherwise.
	  */
	bool setup(int argc, char *argv[]);
	
	optionExt *getOption(const size_t &index_);
	
  private:
	std::vector<option> longOpts; /// Vector of all command line options.
	std::vector<optionExt> baseOpts; /// Default command line options.
	std::vector<optionExt> userOpts; /// User added command line options.
	std::string optstr;
};

#endif
