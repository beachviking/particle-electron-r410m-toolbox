/*
 * Project: SerialCommand.h
 * Description: Class definition that implements a simple serial command handler
 * Author: Gunnar L. Larsen
 * Date: 22/06/2019
 */

#ifndef SerialCommand_h
#define SerialCommand_h

#include "Particle.h"
#include <string.h>

// Size of the input buffer in bytes (maximum length of one command plus arguments)
#define SERIALCOMMAND_BUFFER 32
// Maximum length of a command excluding the terminating null
#define SERIALCOMMAND_MAXCOMMANDLENGTH 16

#define serialCLI Serial  // Serial port to use

// Uncomment the next line to run the library in debug mode (verbose messages)
//#define SERIALCOMMAND_DEBUG

class SerialCommand {
  public:
    SerialCommand();      // Constructor
    void addCommand(const char *command, void(*function)());  // Add a command to the processing dictionary.
    void listCommands();   // Lists all commands to serial.

    void setDefaultHandler(void (*function)(const char *));   // A handler to call when no valid command received.

    void readSerial();    // Main entry point.
    void clearBuffer();   // Clears the input buffer.

    char *next();         // Returns pointer to next token found in command buffer (for getting arguments to commands).

  private:
    // Command/handler dictionary
    struct SerialCommandCallback {
      char command[SERIALCOMMAND_MAXCOMMANDLENGTH + 1];
      void (*function)();
    };                                    // Data structure to hold Command/Handler function key-value pairs
    SerialCommandCallback *commandList;   // Actual definition for command/handler array
    uint8_t commandCount;

    // Pointer to the default handler function
    void (*defaultHandler)(const char *);

    char delim[2]; // null-terminated list of character to be used as delimeters for tokenizing (default " ")
    char term;     // Character that signals end of command (default '\n')

    char buffer[SERIALCOMMAND_BUFFER + 1]; // Buffer of stored characters while waiting for terminator character
    uint8_t bufPos;                        // Current position in the buffer
    char *last;                         // State variable used by strtok_r during processing
};

#endif //SerialCommand_h
