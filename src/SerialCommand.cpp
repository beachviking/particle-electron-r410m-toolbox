/*
 * Project: SerialCommand.cpp
 * Description: Class that implements a simple serial command handler
 * Author: Gunnar L. Larsen
 * Date: 22/06/2019
 */

#include "SerialCommand.h"

/**
 * Constructor makes sure some things are set.
 */
SerialCommand::SerialCommand()
  : commandList(NULL),
    commandCount(0),
    defaultHandler(NULL),
    term('\n'),           // default terminator for commands, newline character
    last(NULL)
{
  strcpy(delim, " "); // strtok_r needs a null-terminated string
  clearBuffer();
}

/**
 * Adds a "command" and a handler function to the list of available commands.
 * This is used for matching a found token in the buffer, and gives the pointer
 * to the handler function to deal with it.
 */
void SerialCommand::addCommand(const char *command, void (*function)()) {
  #ifdef SERIALCOMMAND_DEBUG
    serialCLI.print("Adding command (");
    serialCLI.print(commandCount);
    serialCLI.print("): ");
    serialCLI.println(command);
  #endif

  commandList = (SerialCommandCallback *) realloc(commandList, (commandCount + 1) * sizeof(SerialCommandCallback));
  strncpy(commandList[commandCount].command, command, SERIALCOMMAND_MAXCOMMANDLENGTH);
  commandList[commandCount].function = function;
  commandCount++;
}

void SerialCommand::listCommands() {
  for (int i = 0; i < commandCount; i++) {
    serialCLI.println(commandList[i].command);
  }
}

/**
 * This sets up a handler to be called in the event that the receveived command string
 * isn't in the list of commands.
 */
void SerialCommand::setDefaultHandler(void (*function)(const char *)) {
  defaultHandler = function;
}


/**
 * This checks the Serial stream for characters, and assembles them into a buffer.
 * When the terminator character (default '\n') is seen, it starts parsing the
 * buffer for a prefix command, and calls handlers setup by addCommand() member
 */
void SerialCommand::readSerial() {
  while (serialCLI.available() > 0) {
    char inChar = serialCLI.read();   // Read single available character, there may be more waiting
    #ifdef SERIALCOMMAND_DEBUG
      serialCLI.print(inChar);   // Echo back to serial stream
    #endif

    if (inChar == term) {     // Check for the terminator (default '\r') meaning end of command
      #ifdef SERIALCOMMAND_DEBUG
        serialCLI.print("Received: ");
        serialCLI.println(buffer);
      #endif

      char *command = strtok_r(buffer, delim, &last);   // Search for command at start of buffer
      if (command != NULL) {
        boolean matched = false;
        for (int i = 0; i < commandCount; i++) {
          #ifdef SERIALCOMMAND_DEBUG
            serialCLI.print("Comparing [");
            serialCLI.print(command);
            serialCLI.print("] to [");
            serialCLI.print(commandList[i].command);
            serialCLI.println("]");
          #endif

          // Compare the found command against the list of known commands for a match
          if (strncmp(command, commandList[i].command, SERIALCOMMAND_MAXCOMMANDLENGTH) == 0) {
            #ifdef SERIALCOMMAND_DEBUG
              serialCLI.print("Matched Command: ");
              serialCLI.println(command);
            #endif

            // Execute the stored handler function for the command
            (*commandList[i].function)();
            matched = true;
            break;
          }
        }
        if (!matched && (defaultHandler != NULL)) {
          (*defaultHandler)(command);
        }
      }
      clearBuffer();
    }
    else if (isprint(inChar)) {     // Only printable characters into the buffer
      if (bufPos < SERIALCOMMAND_BUFFER) {
        buffer[bufPos++] = inChar;  // Put character into buffer
        buffer[bufPos] = '\0';      // Null terminate
      } else {
        #ifdef SERIALCOMMAND_DEBUG
          serialCLI.println("Line buffer is full - increase SERIALCOMMAND_BUFFER");
        #endif
      }
    }
  }
}

/*
 * Clear the input buffer.
 */
void SerialCommand::clearBuffer() {
  buffer[0] = '\0';
  bufPos = 0;
}

/**
 * Retrieve the next token ("word" or "argument") from the command buffer.
 * Returns NULL if no more tokens exist.
 */
char *SerialCommand::next() {
  return strtok_r(NULL, delim, &last);
}
