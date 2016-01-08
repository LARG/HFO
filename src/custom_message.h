
#ifndef CUSTOM_MESSAGE_H
#define CUSTOM_MESSAGE_H

#include <rcsc/player/say_message_builder.h>


class CustomMessage : public rcsc::SayMessage {                                                           

  private:
    std::string msg;                                                                                  

  public:                                                                                             

    CustomMessage(std::string m) : msg(m) {}    
    char header() const { return 'm'; }     // Should be one (unique) character                       
    int length() const {return msg.length(); }
    bool toStr(std::string & to) const {to+=msg; return true;}
    std::ostream & printDebug( std::ostream & os ) const { os << "[MYMSG]"; return os;}        // TODO [SANMIT]       
                                                                                                        
};  

#endif



